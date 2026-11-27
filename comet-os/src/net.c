/* Comet OS network stack: NE2000 + AMD PCnet drivers, ARP, IPv4, ICMP, UDP, DHCP, DNS, TCP client, HTTP/1.0 */
#include "kernel.h"

#ifdef NETTEST
extern void test_tx(const u8*b,int len);extern int test_rx(u8*b);
#define NETTEST_RX(b) test_rx(b)
#else
#define NETTEST_RX(b) 0
#endif
net_t net; http_t http;
netlog_t netlog[48];int netlog_n;netapp_t netapp[8];int netapp_n;const char*net_owner="System";u32 g_netops;static char http_owner[12],dq_owner[12],pg_owner[12];
static u8 pkt[1600],rxb[1700];
static u16 ne_base,pc_base;static int nic_kind; /* 0 none, 1 ne2k, 2 pcnet, 3 unsupported */
void net_log_as(const char*owner,const char*text,u32 bytes){
    netlog_t*e=&netlog[netlog_n%48];netlog_n++;e->t=ticks;scopy(e->owner,owner,12);scopy(e->text,text,60);e->bytes=bytes;
    int k=-1;for(int i=0;i<netapp_n;i++)if(!strcmp(netapp[i].name,owner))k=i;
    if(k<0&&netapp_n<8){k=netapp_n++;scopy(netapp[k].name,owner,12);netapp[k].reqs=0;netapp[k].bytes=0;}
    if(k>=0){netapp[k].reqs++;netapp[k].bytes+=bytes;}
}
static u16 sw16(u16 v){return (v>>8)|(v<<8);}
static u32 sw32(u32 v){return (v>>24)|((v>>8)&0xFF00)|((v<<8)&0xFF0000)|(v<<24);}
static u16 csum(const u8*d,int n,u32 s){for(;n>1;n-=2,d+=2)s+=(d[0]<<8)|d[1];if(n)s+=d[0]<<8;while(s>>16)s=(s&0xFFFF)+(s>>16);return (u16)~s;}
void ip_str(u32 ip,char*o){ksnprintf(o,20,"%d.%d.%d.%d",ip>>24,(ip>>16)&255,(ip>>8)&255,ip&255);}
static u32 mk(int a,int b,int c,int d){return ((u32)a<<24)|(b<<16)|(c<<8)|d;}

/* ---------------- NE2000 ---------------- */
#define NE(r) (ne_base+(r))
static int ne_wait(u8 bit){for(int t=0;t<100000;t++)if(inb(NE(7))&bit)return 1;return 0;}
static void ne_rd(u16 addr,u8*buf,int len){
    outb(NE(0),0x22);outb(NE(0x0A),len&255);outb(NE(0x0B),len>>8);outb(NE(8),addr&255);outb(NE(9),addr>>8);outb(NE(0),0x0A);
    insw(NE(0x10),buf,(len+1)/2);ne_wait(0x40);outb(NE(7),0x40);
}
static void ne_wr(u16 addr,const u8*buf,int len){
    outb(NE(0),0x22);outb(NE(7),0x40);outb(NE(0x0A),len&255);outb(NE(0x0B),len>>8);outb(NE(8),addr&255);outb(NE(9),addr>>8);outb(NE(0),0x12);
    outsw(NE(0x10),buf,(len+1)/2);ne_wait(0x40);outb(NE(7),0x40);
}
static int ne_init(void){
    u8 r=inb(NE(0x1F));outb(NE(0x1F),r);for(int t=0;t<100000&&!(inb(NE(7))&0x80);t++);outb(NE(7),0xFF);
    outb(NE(0),0x21);outb(NE(0x0E),0x49);outb(NE(0x0A),0);outb(NE(0x0B),0);outb(NE(0x0C),0x20);outb(NE(0x0D),0x02);
    outb(NE(4),0x40);outb(NE(1),0x46);outb(NE(2),0x80);outb(NE(3),0x46);
    u8 prom[32];ne_rd(0,prom,32);for(int i=0;i<6;i++)net.mac[i]=prom[i*2];
    outb(NE(0),0x61);for(int i=0;i<6;i++)outb(NE(1+i),net.mac[i]);outb(NE(7),0x47);for(int i=0;i<8;i++)outb(NE(8+i),0);
    outb(NE(0),0x21);outb(NE(7),0xFF);outb(NE(0x0F),0);outb(NE(0),0x22);outb(NE(0x0D),0x00);outb(NE(0x0C),0x04);
    return 1;
}
static void ne_send(const u8*b,int len){
    static u8 pad[64];if(len<60){memset(pad,0,64);memcpy(pad,b,len);b=pad;len=60;}
    ne_wr(0x4000,b,len);outb(NE(0),0x22);outb(NE(4),0x40);outb(NE(5),len&255);outb(NE(6),len>>8);outb(NE(0),0x26);
    for(int t=0;t<100000&&(inb(NE(0))&4);t++);
}
static int ne_recv(u8*buf){
    outb(NE(0),0x62);u8 curr=inb(NE(7));outb(NE(0),0x22);u8 bn=inb(NE(3));u8 next=bn+1;if(next>=0x80)next=0x46;
    if(next==curr)return 0;
    u8 h[4];ne_rd(next<<8,h,4);int len=(h[2]|(h[3]<<8))-4;
    if(len<14||len>1600||h[1]<0x46||h[1]>=0x80){outb(NE(3),curr==0x46?0x7F:curr-1);return 0;}
    u16 a=(next<<8)+4;if(a+len>0x8000){int f=0x8000-a;ne_rd(a,buf,f);ne_rd(0x4600,buf+f,len-f);}else ne_rd(a,buf,len);
    u8 nb=h[1]-1;if(nb<0x46)nb=0x7F;outb(NE(3),nb);return len;
}

/* ---------------- AMD PCnet (VirtualBox "Am79C973") ---------------- */
#define PC_RDP (pc_base+0x10)
#define PC_RAP (pc_base+0x14)
#define PC_RST (pc_base+0x18)
#define PC_BDP (pc_base+0x1C)
typedef struct{u32 base;u16 len,status;u32 msg,res;}__attribute__((packed)) pcd_t;
typedef struct{u16 mode,tlen_rlen;u8 mac[6];u16 res;u32 filt[2];u32 rx,tx;}__attribute__((packed)) pci_blk_t;
static u32 csr_r(int r){outl(PC_RAP,r);return inl(PC_RDP);}
static void csr_w(int r,u32 v){outl(PC_RAP,r);outl(PC_RDP,v);}
static void bcr_w(int r,u32 v){outl(PC_RAP,r);outl(PC_BDP,v);}
static pcd_t rxd[8] __attribute__((aligned(16))),txd[4] __attribute__((aligned(16)));
static u8 rxbuf[8][1560] __attribute__((aligned(16))),txbuf[4][1560] __attribute__((aligned(16)));
static pci_blk_t pcb __attribute__((aligned(16)));static int rxi,txi;
static int pc_init(void){
    inl(PC_RST);inw(pc_base+0x14);for(int t=0;t<100000;t++);outl(PC_RDP,0);
    for(int i=0;i<6;i++)net.mac[i]=inb(pc_base+i);
    bcr_w(20,2);csr_w(58,(csr_r(58)&0xFFFC)|2);
    for(int i=0;i<8;i++){rxd[i].base=(u32)rxbuf[i];rxd[i].len=(u16)(-1536)|0xF000;rxd[i].status=0x8000;rxd[i].msg=0;}
    for(int i=0;i<4;i++){txd[i].base=(u32)txbuf[i];txd[i].len=0xF000;txd[i].status=0;}
    memset(&pcb,0,sizeof pcb);pcb.mode=0;pcb.tlen_rlen=(2<<12)|(3<<4);memcpy(pcb.mac,net.mac,6);pcb.filt[0]=pcb.filt[1]=0;pcb.rx=(u32)rxd;pcb.tx=(u32)txd;
    csr_w(1,(u32)&pcb&0xFFFF);csr_w(2,((u32)&pcb)>>16);csr_w(3,0x5F00);csr_w(4,csr_r(4)|0x0915);csr_w(0,1);
    for(int t=0;t<200000&&!(csr_r(0)&0x100);t++);csr_w(0,0x102);return 1;
}
static void pc_send(const u8*b,int len){
    pcd_t*d=&txd[txi];for(int t=0;t<100000&&(d->status&0x8000);t++);
    if(len>1536)len=1536;memcpy(txbuf[txi],b,len);d->len=(u16)(-len)|0xF000;d->msg=0;d->status=0x8300;csr_w(0,0x48);txi=(txi+1)&3;
}
static int pc_recv(u8*buf){
    pcd_t*d=&rxd[rxi];if(d->status&0x8000)return 0;int len=(d->msg&0xFFF)-4;
    if(!(d->status&0x4000)&&len>14&&len<=1536)memcpy(buf,rxbuf[rxi],len);else len=0;
    d->len=(u16)(-1536)|0xF000;d->msg=0;d->status=0x8000;rxi=(rxi+1)&7;return len?len:-1;
}
static void nic_send(const u8*b,int len){
    g_netops++;
#ifdef NETTEST
    if(nic_kind==9){test_tx(b,len);net.tx++;return;}
#endif
    if(nic_kind==1)ne_send(b,len);else if(nic_kind==2)pc_send(b,len);net.tx++;}

/* ---------------- ARP / IP ---------------- */
static struct{u32 ip;u8 mac[6];}arpc[8];static int arpn;
static const u8 bcast[6]={255,255,255,255,255,255};
static int arp_get(u32 ip,u8*mac){for(int i=0;i<8;i++)if(arpc[i].ip==ip){memcpy(mac,arpc[i].mac,6);return 1;}return 0;}
static void arp_put(u32 ip,const u8*mac){for(int i=0;i<8;i++)if(arpc[i].ip==ip){memcpy(arpc[i].mac,mac,6);return;}arpc[arpn&7].ip=ip;memcpy(arpc[arpn&7].mac,mac,6);arpn++;}
static void arp_send(int op,u32 tip,const u8*tmac){
    u8 f[60];memset(f,0,60);memcpy(f,op==1?bcast:tmac,6);memcpy(f+6,net.mac,6);f[12]=8;f[13]=6;
    u8*a=f+14;a[0]=0;a[1]=1;a[2]=8;a[3]=0;a[4]=6;a[5]=4;a[6]=0;a[7]=op;memcpy(a+8,net.mac,6);u32 s=sw32(net.ip);memcpy(a+14,&s,4);
    if(op==2)memcpy(a+18,tmac,6);u32 t=sw32(tip);memcpy(a+24,&t,4);nic_send(f,60);
}
static int ip_out(u32 dst,u8 proto,const u8*pl,int len){
    if(len>1480)return -1;u8 mac[6];
    if(dst==0xFFFFFFFF)memcpy(mac,bcast,6);
    else{u32 nh=((dst^net.ip)&net.mask)==0?dst:net.gw;if(!arp_get(nh,mac)){arp_send(1,nh,0);return -1;}}
    static u16 id;u8*f=pkt;memcpy(f,mac,6);memcpy(f+6,net.mac,6);f[12]=8;f[13]=0;u8*p=f+14;
    p[0]=0x45;p[1]=0;p[2]=(20+len)>>8;p[3]=20+len;id++;p[4]=id>>8;p[5]=id;p[6]=0;p[7]=0;p[8]=64;p[9]=proto;p[10]=p[11]=0;
    u32 s=sw32(net.ip),d=sw32(dst);memcpy(p+12,&s,4);memcpy(p+16,&d,4);u16 c=csum(p,20,0);p[10]=c>>8;p[11]=c;
    memcpy(p+20,pl,len);nic_send(f,14+20+len);return 0;
}
static int udp_out(u32 dst,u16 sp,u16 dp,const u8*d,int len){
    static u8 u[1500];u[0]=sp>>8;u[1]=sp;u[2]=dp>>8;u[3]=dp;u[4]=(8+len)>>8;u[5]=8+len;u[6]=u[7]=0;memcpy(u+8,d,len);
    u32 ps=(net.ip>>16)+(net.ip&0xFFFF)+(dst>>16)+(dst&0xFFFF)+17+8+len;u16 c=csum(u,8+len,ps);if(!c)c=0xFFFF;u[6]=c>>8;u[7]=c;
    return ip_out(dst,17,u,8+len);
}

/* ---------------- DHCP ---------------- */
static u32 dh_xid,dh_t0,dh_tries;static u32 dh_offer,dh_server;
static void dhcp_send(int type){
    static u8 b[300];memset(b,0,300);b[0]=1;b[1]=1;b[2]=6;b[4]=dh_xid>>24;b[5]=dh_xid>>16;b[6]=dh_xid>>8;b[7]=dh_xid;b[10]=0x80;memcpy(b+28,net.mac,6);
    b[236]=99;b[237]=130;b[238]=83;b[239]=99;int o=240;b[o++]=53;b[o++]=1;b[o++]=type;
    if(type==3){b[o++]=50;b[o++]=4;u32 v=sw32(dh_offer);memcpy(b+o,&v,4);o+=4;b[o++]=54;b[o++]=4;v=sw32(dh_server);memcpy(b+o,&v,4);o+=4;}
    b[o++]=55;b[o++]=3;b[o++]=1;b[o++]=3;b[o++]=6;b[o++]=255;u32 sv=net.ip;net.ip=0;udp_out(0xFFFFFFFF,68,67,b,o);net.ip=sv;
}
void net_dhcp_start(void){if(!nic_kind||nic_kind==3)return;net.cfg=0;net.ip=0;net.mask=0;net.dhcp=1;dh_xid=rnd();dh_t0=ticks;dh_tries=0;dhcp_send(1);}
static void dhcp_in(const u8*d,int len){
    if(len<240||d[0]!=2||((d[4]<<24)|(d[5]<<16)|(d[6]<<8)|d[7])!=dh_xid)return;
    u32 yi=mk(d[16],d[17],d[18],d[19]);int type=0;u32 mask=0,gw=0,dns=0,srv=0;
    for(int i=240;i+1<len&&d[i]!=255;){if(d[i]==0){i++;continue;}int c=d[i],l=d[i+1];if(i+2+l>len)break;const u8*v=d+i+2;
        if(c==53)type=v[0];else if(c==1&&l>=4)mask=mk(v[0],v[1],v[2],v[3]);else if(c==3&&l>=4)gw=mk(v[0],v[1],v[2],v[3]);else if(c==6&&l>=4)dns=mk(v[0],v[1],v[2],v[3]);else if(c==54&&l>=4)srv=mk(v[0],v[1],v[2],v[3]);i+=2+l;}
    if(type==2&&net.dhcp==1){dh_offer=yi;dh_server=srv;net.dhcp=2;dhcp_send(3);}
    else if(type==5){net.ip=yi;net.mask=mask?mask:0xFFFFFF00;net.gw=gw;net.dns=dns?dns:gw;net.cfg=1;net.dhcp=3;arp_send(1,net.gw,0);}
}

/* ---------------- DNS ---------------- */
static struct{char name[48];u32 ip;}dnsc[8];static int dnsn;
static char dq_name[64];static int dq_state;static u32 dq_ip,dq_t0,dq_tries;static u16 dq_id;
static void dns_send(void){
    static u8 b[300];int o=12;memset(b,0,12);b[0]=dq_id>>8;b[1]=dq_id;b[2]=1;b[5]=1;
    const char*p=dq_name;while(*p){const char*e=p;while(*e&&*e!='.')e++;int l=e-p;if(l>63||o+l+6>290)return;b[o++]=l;memcpy(b+o,p,l);o+=l;p=*e?e+1:e;}
    b[o++]=0;b[o++]=0;b[o++]=1;b[o++]=0;b[o++]=1;udp_out(net.dns,40000,53,b,o);
}
static void dns_in(const u8*d,int len){
    if(dq_state!=1||len<12||((d[0]<<8)|d[1])!=dq_id)return;if((d[3]&15)!=0){dq_state=3;return;}
    int an=(d[6]<<8)|d[7],o=12;while(o<len&&d[o]){if((d[o]&0xC0)==0xC0){o++;break;}o+=d[o]+1;}o+=1+4;
    for(int i=0;i<an&&o+10<=len;i++){
        if((d[o]&0xC0)==0xC0)o+=2;else{while(o<len&&d[o])o+=d[o]+1;o++;}
        int t=(d[o]<<8)|d[o+1],rl=(d[o+8]<<8)|d[o+9];o+=10;
        if(t==1&&rl==4&&o+4<=len){dq_ip=mk(d[o],d[o+1],d[o+2],d[o+3]);dq_state=2;{char ib[20],lb[60];ip_str(dq_ip,ib);ksnprintf(lb,60,"DNS %s -> %s",dq_name,ib);net_log_as(dq_owner,lb,0);}scopy(dnsc[dnsn&7].name,dq_name,48);dnsc[dnsn&7].ip=dq_ip;dnsn++;return;}
        o+=rl;
    }
    dq_state=3;
}
static int parse_ip(const char*s,u32*o){int v[4],n=0;const char*p=s;while(n<4){if(*p<'0'||*p>'9')return 0;int x=0;while(*p>='0'&&*p<='9')x=x*10+*p++-'0';if(x>255)return 0;v[n++]=x;if(n<4){if(*p!='.')return 0;p++;}}if(*p)return 0;*o=mk(v[0],v[1],v[2],v[3]);return 1;}
int net_resolve(const char*name,u32*ip){
    if(parse_ip(name,ip))return 1;
    for(int i=0;i<8;i++)if(dnsc[i].ip&&!strcmp(dnsc[i].name,name)){*ip=dnsc[i].ip;return 1;}
    if(!net.cfg)return -1;
    if(dq_state==0||strcmp(dq_name,name)){if(dq_state==1&&strcmp(dq_name,name))return 0;scopy(dq_name,name,64);scopy(dq_owner,net_owner,12);dq_state=1;dq_t0=ticks;dq_tries=0;dq_id=rnd();dns_send();return 0;}
    if(dq_state==2){*ip=dq_ip;dq_state=0;return 1;}if(dq_state==3){dq_state=0;return -1;}return 0;
}

/* ---------------- ICMP ping ---------------- */
static int pg_state;static u32 pg_ip,pg_t0,pg_rtt;static char pg_host[64];static u16 pg_seq;
int net_ping_start(const char*h){if(!net.cfg)return -1;scopy(pg_owner,net_owner,12);{char lb[60];ksnprintf(lb,60,"ping %s",h);net_log_as(pg_owner,lb,0);}scopy(pg_host,h,64);pg_state=1;pg_seq++;return 0;}
int net_ping_state(u32*rtt,u32*ip){*rtt=pg_rtt;*ip=pg_ip;return pg_state;}
static int pg_resent;
static void ping_send(void){u8 b[40];memset(b,0,40);b[0]=8;b[4]=0x43;b[5]=0x4F;b[6]=pg_seq>>8;b[7]=pg_seq;memcpy(b+8,"cometos-ping-data",17);u16 c=csum(b,32,0);b[2]=c>>8;b[3]=c;ip_out(pg_ip,1,b,32);}
static void ping_step(void){
    if(pg_state==1){int r=net_resolve(pg_host,&pg_ip);if(r<0)pg_state=5;else if(r>0){pg_t0=ticks;pg_resent=0;ping_send();pg_state=2;}}
    else if(pg_state==2){if(!pg_resent&&ticks-pg_t0>30){pg_resent=1;ping_send();}if(ticks-pg_t0>300)pg_state=4;}
}

/* ---------------- TCP + HTTP ---------------- */
static struct{int st;u32 rip;u16 rport,lport;u32 snd,rcv;u32 t0;int tries;}tcp;
static int hreq_sent;
static void tcp_out(u8 fl,const u8*d,int len){
    static u8 s[1500];int hl=(fl&2)?24:20;memset(s,0,hl);s[0]=tcp.lport>>8;s[1]=tcp.lport;s[2]=tcp.rport>>8;s[3]=tcp.rport;
    u32 sq=sw32(tcp.snd),ak=sw32(tcp.rcv);memcpy(s+4,&sq,4);memcpy(s+8,&ak,4);s[12]=(hl/4)<<4;s[13]=fl;s[14]=0x20;s[15]=0;
    if(fl&2){s[20]=2;s[21]=4;s[22]=5;s[23]=0xB4;}if(len)memcpy(s+hl,d,len);
    u32 ps=(net.ip>>16)+(net.ip&0xFFFF)+(tcp.rip>>16)+(tcp.rip&0xFFFF)+6+hl+len;u16 c=csum(s,hl+len,ps);s[16]=c>>8;s[17]=c;ip_out(tcp.rip,6,s,hl+len);
}
static void http_fail(const char*m){scopy(http.err,m,56);http.state=6;tcp.st=0;}
static void http_finish(void);
int http_start(const char*url){
    if(http.body){kfree(http.body);http.body=0;}memset(&http,0,sizeof http);
    const char*p=url;if(!strncmp(p,"http://",7))p+=7;else if(!strncmp(p,"https://",8)){http.state=6;scopy(http.err,"HTTPS is not supported (plain http:// sites only)",56);return -1;}
    int n=0;while(*p&&*p!='/'&&*p!=':'&&n<63)http.host[n++]=*p++;http.host[n]=0;http.port=80;
    if(*p==':'){p++;int v=0;while(*p>='0'&&*p<='9')v=v*10+*p++-'0';http.port=v?v:80;}
    scopy(http.path,*p?p:"/",160);if(!n){http.state=6;scopy(http.err,"Enter an address",56);return -1;}
    if(!net.cfg){http.state=6;scopy(http.err,nic_kind?"Not connected yet (waiting for DHCP)":"No supported network card",56);return -1;}
    http.cap=1<<19;http.body=kmalloc(http.cap);if(!http.body){http.state=6;scopy(http.err,"Out of memory",56);return -1;}
    scopy(http_owner,net_owner,12);{char lb[60];ksnprintf(lb,60,"GET %s%s",http.host,http.path);net_log_as(http_owner,lb,0);}
    http.state=1;http.t0=http.tlast=ticks;return 0;
}
void http_cancel(void){if(http.state>=1&&http.state<=4){http.state=6;scopy(http.err,"Cancelled",56);tcp.st=0;}}
static void http_send_req(void){
    char r[400];int n=ksnprintf(r,400,"GET %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: CometOS/1.0\r\nAccept: text/html,text/plain\r\nConnection: close\r\n\r\n",http.path,http.host);
    tcp_out(0x18,(u8*)r,n);tcp.snd+=n;hreq_sent=1;http.tlast=ticks;
}
static void tcp_in(const u8*s,int len,u32 src){
    if(!tcp.st||src!=tcp.rip||len<20)return;u16 sp=(s[0]<<8)|s[1],dp=(s[2]<<8)|s[3];if(sp!=tcp.rport||dp!=tcp.lport)return;
    u32 seq=(s[4]<<24)|(s[5]<<16)|(s[6]<<8)|s[7],ack=(s[8]<<24)|(s[9]<<16)|(s[10]<<8)|s[11];u8 fl=s[13];int hl=(s[12]>>4)*4;if(hl>len)return;int dl=len-hl;const u8*d=s+hl;
    if(fl&4){http_fail("Connection refused/reset");return;}
    if(tcp.st==1){if((fl&0x12)==0x12&&ack==tcp.snd+1){tcp.snd++;tcp.rcv=seq+1;tcp.st=2;tcp_out(0x10,0,0);http_send_req();http.state=3;}return;}
    if(tcp.st==2){
        if(dl>0){if(seq==tcp.rcv){if(http.len+dl<=http.cap){memcpy(http.body+http.len,d,dl);http.len+=dl;}tcp.rcv+=dl;http.tlast=ticks;http.state=4;}tcp_out(0x10,0,0);}
        if((fl&1)&&seq+dl==tcp.rcv){tcp.rcv++;tcp_out(0x11,0,0);tcp.snd++;tcp.st=0;http_finish();}
    }
}
static void http_finish(void){
    http.body[http.len<http.cap?http.len:http.cap-1]=0;u32 i=0;int ok=0;for(;i+3<http.len;i++)if(http.body[i]=='\r'&&http.body[i+1]=='\n'&&http.body[i+2]=='\r'&&http.body[i+3]=='\n'){ok=1;break;}
    if(!ok||http.len<12){http_fail("Bad response");return;}
    http.status=atoi_((char*)http.body+9);http.hdr_end=i+4;
    if((http.status==301||http.status==302||http.status==303||http.status==307||http.status==308)&&http.redirects<4){
        for(u32 k=0;k<i;k++)if((k==0||http.body[k-1]=='\n')&&!memcmp(http.body+k,"Location:",9)){char loc[200];int n=0;u32 j=k+9;while(http.body[j]==' ')j++;while(j<i&&http.body[j]!='\r'&&n<199)loc[n++]=http.body[j++];loc[n]=0;
            char url[260];if(!strncmp(loc,"http",4))scopy(url,loc,260);else ksnprintf(url,260,"http://%s%s",http.host,loc);int rd=http.redirects+1;char keep[260];scopy(keep,url,260);
            http_start(keep);http.redirects=rd;return;}
    }
    {char lb[60];ksnprintf(lb,60,"HTTP %d from %s",http.status,http.host);net_log_as(http_owner,lb,http.len);}
    http.state=5;
}
static void http_step(void){
    if(http.state==1){u32 ip;int r=net_resolve(http.host,&ip);
        if(r<0)http_fail("Could not resolve that name");
        else if(r>0){http.ip=ip;tcp.rip=ip;tcp.rport=http.port;tcp.lport=49152+(rnd()%1000);tcp.snd=rnd();tcp.rcv=0;tcp.st=1;tcp.t0=ticks;tcp.tries=0;hreq_sent=0;http.state=2;tcp_out(2,0,0);}
        else if(ticks-http.t0>800)http_fail("DNS timed out");}
    else if(http.state==2){if(ticks-tcp.t0>(tcp.tries?150u:30u)){if(++tcp.tries>4){http_fail("Connection timed out");return;}tcp.t0=ticks;tcp.snd=tcp.snd;tcp_out(2,0,0);}}
    else if(http.state>=3&&http.state<=4){if(ticks-http.tlast>1500){if(http.len>0){tcp.st=0;http_finish();}else http_fail("No response (timed out)");}}
}

/* ---------------- receive path ---------------- */
static void ip_in(const u8*p,int len){
    if(len<20||(p[0]>>4)!=4)return;int hl=(p[0]&15)*4,tl=(p[2]<<8)|p[3];if(tl>len||hl>tl)return;
    u32 src=mk(p[12],p[13],p[14],p[15]),dst=mk(p[16],p[17],p[18],p[19]);if(dst!=net.ip&&dst!=0xFFFFFFFF&&net.cfg)return;
    const u8*d=p+hl;int dl=tl-hl;
    if(p[9]==1&&dl>=8){if(d[0]==0&&((d[4]<<8)|d[5])==0x434F&&pg_state==2&&((d[6]<<8)|d[7])==pg_seq){pg_rtt=(ticks-pg_t0)*10;pg_state=3;}
        else if(d[0]==8){u8 r[1500];memcpy(r,d,dl);r[0]=0;r[2]=r[3]=0;u16 c=csum(r,dl,0);r[2]=c>>8;r[3]=c;ip_out(src,1,r,dl);}}
    else if(p[9]==17&&dl>=8){u16 dp=(d[2]<<8)|d[3];if(dp==68)dhcp_in(d+8,dl-8);else if(dp==40000)dns_in(d+8,dl-8);}
    else if(p[9]==6)tcp_in(d,dl,src);
}
static void rx_frame(const u8*f,int len){
    net.rx++;u16 t=(f[12]<<8)|f[13];
    if(t==0x0806&&len>=42){const u8*a=f+14;u32 sip=mk(a[14],a[15],a[16],a[17]),tip=mk(a[24],a[25],a[26],a[27]);arp_put(sip,a+8);if(a[7]==1&&tip==net.ip&&net.cfg)arp_send(2,sip,a+8);}
    else if(t==0x0800)ip_in(f+14,len-14);
}
void net_poll(void){
    if(nic_kind!=1&&nic_kind!=2&&nic_kind!=9)return;
    for(int i=0;i<6;i++){int n=nic_kind==1?ne_recv(rxb):(nic_kind==2?pc_recv(rxb):NETTEST_RX(rxb));if(n==0)break;if(n>0)rx_frame(rxb,n);}
    if(net.dhcp==1||net.dhcp==2){if(ticks-dh_t0>200){dh_t0=ticks;if(++dh_tries>3){net.ip=mk(10,0,2,15);net.mask=0xFFFFFF00;net.gw=mk(10,0,2,2);net.dns=mk(10,0,2,3);net.cfg=1;net.dhcp=4;arp_send(1,net.gw,0);}else{if(net.dhcp==1)dhcp_send(1);else dhcp_send(3);}}}
    if(dq_state==1&&ticks-dq_t0>(dq_tries?100u:25u)){dq_t0=ticks;if(++dq_tries>5)dq_state=3;else dns_send();}
    ping_step();http_step();
}
const char*net_status(void){
    static char b[40];if(!nic_kind)return "No adapter";if(nic_kind==3)return "No driver";if(!net.cfg)return "Connecting...";ip_str(net.ip,b);return b;
}

/* ---------------- init ---------------- */
static void pci_enable(pci_dev_t*p){outl(0xCF8,0x80000000u|((u32)p->bus<<16)|((u32)p->dev<<11)|((u32)p->fn<<8)|4);u32 c=inl(0xCFC);outl(0xCF8,0x80000000u|((u32)p->bus<<16)|((u32)p->dev<<11)|((u32)p->fn<<8)|4);outl(0xCFC,c|7);}
void net_init(void){
    nic_kind=0;scopy(net.nicname,"none",32);
    for(int i=0;i<pci_count;i++){pci_dev_t*p=&pci_devs[i];
        if(p->vendor==0x10EC&&p->device==0x8029){nic_kind=1;ne_base=p->bar0&0xFFFC;pci_enable(p);scopy(net.nicname,"NE2000 (RTL8029)",32);break;}
        if(p->vendor==0x1022&&p->device==0x2000){nic_kind=2;pc_base=p->bar0&0xFFFC;pci_enable(p);scopy(net.nicname,"AMD PCnet-FAST III",32);break;}
        if(p->cls==2&&!nic_kind){nic_kind=3;scopy(net.nicname,p->vendor==0x8086?"Intel PRO/1000 (no driver)":"Unsupported NIC",32);}}
    if(nic_kind==1)ne_init();else if(nic_kind==2)pc_init();
    if(nic_kind==1||nic_kind==2)net_dhcp_start();
}
