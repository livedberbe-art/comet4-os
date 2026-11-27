/* Comet OS built-in applications */
#include "kernel.h"
#define ST(type) type*s=(type*)w->d
#define TPF(t,...) do{char _b[200];ksnprintf(_b,200,__VA_ARGS__);tp(t,_b);}while(0)
static int lumi(u32 c){return (((c>>16)&255)*299+((c>>8)&255)*587+(c&255)*114)/1000;}
static u32 fgon(u32 c){return lumi(c)>150?0x0B1020:0xFFFFFF;}
static u32*make_thumb(const u32*src,int w,int h,int tw,int th){
    if(tw<2||th<2||w<2||h<2)return 0;u32*o=kmalloc(tw*th*4);if(!o)return 0;int sw,sh;
    if(w*th>h*tw){sh=h;sw=h*tw/th;}else{sw=w;sh=w*th/tw;}int ox=(w-sw)/2,oy=(h-sh)/2;
    for(int y=0;y<th;y++){int sy=oy+y*sh/th;for(int x=0;x<tw;x++)o[y*tw+x]=src[sy*w+ox+x*sw/tw];}return o;
}
static int inb_(int px,int py,int x,int y,int w,int h){return px>=x&&px<x+w&&py>=y&&py<y+h;}

/* ================================================== Terminal =========== */
#define TL 200
#define TC 110
typedef struct{char l[TL][TC+1];int n;char in[120];int il,cwd,off,nh,hi;char hist[16][120];char*rb;int rl;int job;u32 jt;char jarg[64],jfile[32];int jail;}term_t;
static int in_tree(int n,int root){while(n>0){if(n==root)return 1;n=fsn[n].parent;}return n==root;}
static int jres_(term_t*t,const char*p){int n=fs_resolve(t->cwd,p);if(t->jail&&(n<0||!in_tree(n,t->jail)))return -1;return n;}
static void tp(term_t*t,const char*s){
    if(t->rb){int len=strlen(s);if(t->rl+len<4000){memcpy(t->rb+t->rl,s,len);t->rl+=len;t->rb[t->rl]=0;}return;}
    for(;*s;s++){
        int len=strlen(t->l[t->n-1]);
        if(*s=='\n'||len>=TC){if(t->n==TL){memmove(t->l[0],t->l[1],(TL-1)*(TC+1));t->n--;}t->n++;t->l[t->n-1][0]=0;if(*s=='\n')continue;len=0;}
        char*c=t->l[t->n-1];c[len]=*s;c[len+1]=0;
    }
}
static int split(char*s,char**av,int max){
    int n=0;while(*s&&n<max){while(*s==' ')s++;if(!*s)break;av[n++]=s;if(*s=='"'){av[n-1]=++s;while(*s&&*s!='"')s++;}else while(*s&&*s!=' ')s++;if(*s)*s++=0;}
    return n;
}
static void prompt_str(term_t*t,char*o){char p[64];fs_path(t->cwd,p,64);int hl=fs_home?1:0;(void)hl;ksnprintf(o,100,"guest@comet:%s$ ",p);}
static void term_exec(win_t*w,term_t*t,char*line){
    char buf[120];scopy(buf,line,120);char*av[10];int ac=split(buf,av,10);if(!ac)return;
    int red=0;char*rf=0;for(int i=0;i<ac;i++)if(!strcmp(av[i],">")||!strcmp(av[i],">>")){red=av[i][1]?2:1;rf=i+1<ac?av[i+1]:0;ac=i;break;}
    if(!ac)return;if(red&&rf){t->rb=kzalloc(4096);t->rl=0;}
    const char*c=av[0];
#define IS(x) (!strcmp(c,x))
    if(t->jail){static const char*blk[]={"open","install","theme","accent","resolution","reboot","shutdown","poweroff","lock","kill","dhcp","lspci","lsblk","lscpu","df"};
        for(unsigned bi=0;bi<sizeof blk/sizeof blk[0];bi++)if(IS(blk[bi])){TPF(t,"%s: not available inside Safe Space\n",c);goto done_cmd;}
        if((IS("wget")||IS("ping")||IS("nslookup"))&&!g_sbx_net){tp(t,"Internet is off in Safe Space. Turn it on with the switch at the top.\n");goto done_cmd;}}
    if(IS("help")){tp(t,"Commands: help clear ls cd pwd cat echo mkdir touch rm cp mv grep tar\n  open edit theme accent resolution install\n  uname whoami hostname date uptime free df ps kill lscpu lspci lsblk neofetch\n  history reboot shutdown lock\n  ifconfig dhcp ping nslookup wget run  (use > and >> to redirect output)\n");}
    else if(IS("clear")){t->n=1;t->l[0][0]=0;}
    else if(IS("pwd")){char p[64];fs_path(t->cwd,p,64);tp(t,p);tp(t,"\n");}
    else if(IS("ls")){int d=ac>1&&av[1][0]!='-'?jres_(t,av[1]):t->cwd;int lng=ac>1&&!strcmp(av[1],"-l");if(d<0){tp(t,"ls: no such directory\n");}
        else if(!fsn[d].dir){TPF(t,"%s\n",fsn[d].name);}else{int k[64],n=fs_children(d,k,64);for(int i=0;i<n;i++){if(lng)TPF(t,"%s %6d  %s%s\n",fsn[k[i]].dir?"d":"-",fsn[k[i]].size,fsn[k[i]].name,fsn[k[i]].dir?"/":"");else TPF(t,"%s%s  ",fsn[k[i]].name,fsn[k[i]].dir?"/":"");}if(!lng)tp(t,"\n");}}
    else if(IS("cd")){int d=ac>1?jres_(t,av[1]):(t->jail?t->jail:fs_home);if(d>=0&&fsn[d].dir)t->cwd=d;else tp(t,"cd: no such directory\n");}
    else if(IS("cat")){for(int i=1;i<ac;i++){int n=jres_(t,av[i]);if(n<=0||fsn[n].dir){TPF(t,"cat: %s: no such file\n",av[i]);continue;}
        for(u32 k=0;k<fsn[n].size&&k<8000;k++){char ch=fsn[n].data[k];char s[2]={(ch==10||(ch>=32&&ch<127))?ch:'.',0};tp(t,s);}tp(t,"\n");}}
    else if(IS("echo")){for(int i=1;i<ac;i++){tp(t,av[i]);tp(t," ");}tp(t,"\n");}
    else if(IS("mkdir")){for(int i=1;i<ac;i++)if(fs_create(t->cwd,av[i],1)<0)TPF(t,"mkdir: cannot create %s\n",av[i]);}
    else if(IS("touch")){for(int i=1;i<ac;i++)if(fs_find(t->cwd,av[i])<0&&fs_create(t->cwd,av[i],0)<0)TPF(t,"touch: cannot create %s\n",av[i]);}
    else if(IS("rm")){for(int i=1;i<ac;i++){if(av[i][0]=='-')continue;int n=jres_(t,av[i]);if(n<=0)TPF(t,"rm: %s: no such file\n",av[i]);else if(fsn[n].dir&&!(ac>2&&av[1][0]=='-'))TPF(t,"rm: %s is a directory (use rm -r)\n",av[i]);else fs_remove(n);}}
    else if(IS("cp")&&ac>2){int a=jres_(t,av[1]);if(a<=0||fsn[a].dir)tp(t,"cp: bad source\n");else{int d=jres_(t,av[2]);int dir=t->cwd;const char*nm=av[2];if(d>0&&fsn[d].dir){dir=d;nm=fsn[a].name;d=-1;}int n=d>0?d:fs_create(dir,nm,0);if(n>0)fs_write(n,fsn[a].data,fsn[a].size);else tp(t,"cp: failed\n");}}
    else if(IS("mv")&&ac>2){int a=jres_(t,av[1]);int d=jres_(t,av[2]);if(a<=0)tp(t,"mv: no such file\n");else if(d>0&&fsn[d].dir)fsn[a].parent=d;else if(fs_rename(a,av[2]))tp(t,"mv: failed\n");}
    else if(IS("grep")&&ac>2){int n=jres_(t,av[2]);if(n<=0||fsn[n].dir)tp(t,"grep: bad file\n");else{char ln[120];int li=0;for(u32 k=0;k<=fsn[n].size;k++){char ch=k<fsn[n].size?fsn[n].data[k]:'\n';if(ch=='\n'||li>=118){ln[li]=0;if(stricontains(ln,av[1])){tp(t,ln);tp(t,"\n");}li=0;}else ln[li++]=ch;}}}
    else if(IS("tar")&&ac>2){
        if(!strcmp(av[1],"c")){int nodes[8],nn=0;for(int i=3;i<ac&&nn<8;i++){int n=jres_(t,av[i]);if(n>0)nodes[nn++]=n;}int o=fs_find(t->cwd,av[2]);if(o<0)o=fs_create(t->cwd,av[2],0);if(o>0&&nn&&!tar_create(o,nodes,nn))TPF(t,"created %s (%d items)\n",av[2],nn);else tp(t,"tar: failed\n");}
        else{int n=jres_(t,av[2]);if(n<=0||fsn[n].dir)tp(t,"tar: no such archive\n");
            else if(!strcmp(av[1],"t")){tar_ent*e=kmalloc(sizeof(tar_ent)*64);int k=tar_list(fsn[n].data,fsn[n].size,e,64);for(int i=0;i<k;i++)TPF(t,"%8d  %s\n",e[i].size,e[i].name);kfree(e);}
            else if(!strcmp(av[1],"x")){int k=tar_extract(n,t->cwd);TPF(t,"extracted %d files\n",k);}}}
    else if(IS("open")&&ac>1){int found=0;for(int i=0;i<APP_COUNT;i++)if(stricontains(apps[i].name,av[1])){win_open(i);found=1;break;}
        if(!found){int n=jres_(t,av[1]);if(n>0)open_node(n);else tp(t,"open: not found\n");}}
    else if((IS("edit")||IS("nano"))&&ac>1){int n=jres_(t,av[1]);if(n<0)n=fs_create(t->cwd,av[1],0);if(n>0){if(t->jail)win_open_sbx(APP_NOTES,n);else win_open_arg(APP_NOTES,n);}}
    else if(IS("theme")&&ac>1){g_dark=!strcmp(av[1],"dark");theme_apply();cfg_theme=g_dark;cfg_save();}
    else if(IS("accent")&&ac>1){int ok=0;for(int i=0;i<8;i++)if(stricontains(accent_names[i],av[1])){g_accent=accents[i];theme_apply();cfg_accent=i;cfg_save();ok=1;break;}if(!ok)tp(t,"accents: blue violet pink coral amber gold mint cyan\n");}
    else if(IS("resolution")&&ac>1){int rw=0,rh=0;char*p=av[1];while(*p>='0'&&*p<='9')rw=rw*10+*p++-'0';if(*p=='x')p++;while(*p>='0'&&*p<='9')rh=rh*10+*p++-'0';
        if(rw<640||rh<480||rw>1920||rh>1080||!bga_ok||!ui_res_set(rw,rh))tp(t,"resolution: unsupported (640x480 .. 1920x1080, needs VBE/BGA)\n");}
    else if(IS("install"))win_open(APP_SETUP);
    else if(IS("uname"))tp(t,"Comet 1.0-perseid i686 Comet OS\n");
    else if(IS("whoami"))tp(t,"guest\n");else if(IS("hostname"))tp(t,"comet\n");
    else if(IS("date")){int h,m,s,d,mo,y;rtc_read(&h,&m,&s,&d,&mo,&y);TPF(t,"%d-%02d-%02d %02d:%02d:%02d\n",y,mo,d,h,m,s);}
    else if(IS("uptime")){u32 u=ticks/100;TPF(t,"up %dh %dm %ds\n",u/3600,(u/60)%60,u%60);}
    else if(IS("free")){TPF(t,"RAM total %d MB\nkernel heap used %d KB of %d KB\n",mem_total_kb/1024,heap_used()/1024,heap_total()/1024);}
    else if(IS("df")){TPF(t,"ramfs  %d KB used  %d nodes\n",fs_used()/1024,fs_nodes());for(int i=0;i<4;i++)if(ata[i].present&&!ata[i].atapi)TPF(t,"hd%d     %d MB  %s\n",i,ata[i].sectors/2048,ata[i].model);}
    else if(IS("ps")){int pids[40];char nm[40][24];int n=proc_list(pids,nm,40);tp(t,"  PID  NAME             CPU%   MEM(KB)\n");for(int i=0;i<n;i++){u32 cp,me;proc_info(pids[i],&cp,&me);TPF(t,"%5d  %s",pids[i],nm[i]);for(int k=strlen(nm[i]);k<17;k++)tp(t," ");TPF(t,"%d.%d   %d\n",cp/10,cp%10,me);}}
    else if(IS("kill")&&ac>1){int r=proc_kill(atoi_(av[1]));tp(t,r==1?"terminated\n":r==-1?"kill: operation not permitted\n":"kill: no such process\n");}
    else if(IS("lscpu")){TPF(t,"Model: %s\nVendor: %s\n",cpu_brand,cpu_vendor);}
    else if(IS("lspci")){for(int i=0;i<pci_count;i++)TPF(t,"%02x:%02x.%d  %04x:%04x  class %02x%02x\n",pci_devs[i].bus,pci_devs[i].dev,pci_devs[i].fn,pci_devs[i].vendor,pci_devs[i].device,pci_devs[i].cls,pci_devs[i].sub);}
    else if(IS("lsblk")){for(int i=0;i<4;i++)if(ata[i].present)TPF(t,"ata%d  %s  %d MB  %s\n",i,ata[i].atapi?"cdrom":"disk ",ata[i].sectors/2048,ata[i].model);}
    else if(IS("neofetch")){u32 u=ticks/100;
        TPF(t,"      .  *           guest@comet\n   *    .---.   .      -----------\n  .   ./  o  \\.        OS: Comet OS 1.0 (Perseid)\n     /   ___   \\  *    Kernel: comet 1.0 i686\n *   '--'   '--'        Uptime: %dm %ds\n   .   `  .   `         Resolution: %dx%d\n                        CPU: %s\n                        Memory: %d / %d KB\n",u/60,u%60,SW,SH,cpu_brand,heap_used()/1024,heap_total()/1024);}
    else if(IS("history")){for(int i=0;i<t->nh;i++)TPF(t,"%3d  %s\n",i+1,t->hist[i]);}
    else if(IS("reboot"))wm_power(2);else if(IS("shutdown")||IS("poweroff"))wm_power(1);else if(IS("lock"))wm_power(3);
    else if(IS("ifconfig")){char ia[20],ig[20];ip_str(net.ip,ia);ip_str(net.gw,ig);TPF(t,"%s\n  mac %02x:%02x:%02x:%02x:%02x:%02x\n  inet %s   gateway %s\n  rx %d packets   tx %d packets\n",net.nicname,net.mac[0],net.mac[1],net.mac[2],net.mac[3],net.mac[4],net.mac[5],ia,ig,net.rx,net.tx);}
    else if(IS("dhcp")){net_dhcp_start();tp(t,"requesting an address...\n");t->job=4;t->jt=ticks;}
    else if(IS("ping")&&ac>1){net_owner=t->jail?"Safe Space":"Terminal";if(net_ping_start(av[1])<0)tp(t,"ping: network not ready (try 'dhcp')\n");else{tp(t,"pinging...\n");t->job=1;t->jt=ticks;scopy(t->jarg,av[1],64);}}
    else if(IS("nslookup")&&ac>1){scopy(t->jarg,av[1],64);t->job=2;t->jt=ticks;}
    else if(IS("wget")&&ac>1){net_owner=t->jail?"Safe Space":"Terminal";if(http_start(av[1])<0)TPF(t,"wget: %s\n",http.err);else{t->job=3;t->jt=ticks;scopy(t->jfile,ac>2?av[2]:"download.html",32);tp(t,"downloading...\n");}}
    else if(IS("run")&&ac>1){int n=jres_(t,av[1]);if(n>0){if(t->jail)win_open_sbx(APP_SCRIPT,n);else win_open_arg(APP_SCRIPT,n);}else tp(t,"run: no such file\n");}
    else TPF(t,"%s: command not found (try 'help')\n",c);
    done_cmd:;
    if(t->rb){char*rb=t->rb;int rl=t->rl;t->rb=0;if(rf){int n=fs_find(t->cwd,rf);if(n<0)n=fs_create(t->cwd,rf,0);if(n>0){if(red==2)fs_append(n,rb,rl);else fs_write(n,rb,rl);}}kfree(rb);}
}
static void term_render(win_t*w,term_t*s,int x0,int y0,int ww,int hh){
    ui_fill(w,x0,y0,ww,hh,0x0D1017,8);int rows=(hh-12)/11;int first=MAX(0,s->n-rows-s->off);
    cliprect c=clip_push(w->cx+x0,w->cy+y0,ww,hh);
    for(int i=first;i<s->n&&i<first+rows;i++){int y=y0+8+(i-first)*11;
        if(i==s->n-1&&s->off==0){char p[100];prompt_str(s,p);ui_text(w,x0+8,y+2,s->l[i],0xD6DEEB,1);int x=x0+8+strlen(s->l[i])*8;ui_text(w,x,y+2,p,T.accent2,1);x+=strlen(p)*8;ui_text(w,x,y+2,s->in,0xFFFFFF,1);x+=s->il*8;fill_rect(w->cx+x,w->cy+y,7,10,((ticks/50)&1)?T.accent2:blend(0x0D1017,T.accent2,60));}
        else ui_text(w,x0+8,y+2,s->l[i],0xD6DEEB,1);}
    clip_pop(c);
}
static void term_init(win_t*w){term_t*t=kzalloc(sizeof(term_t));w->d=t;t->n=1;t->cwd=fs_home;w->mem=24;tp(t,"Comet OS 1.0 (Perseid) - type 'help' for commands.\n");}
static void term_draw(win_t*w){
    ST(term_t);ui_bg(w,0x0D1017);int rows=(w->ch-10)/11;int first=MAX(0,s->n-rows-s->off);
    for(int i=first;i<s->n&&i<first+rows;i++){int y=6+(i-first)*11;
        if(i==s->n-1&&s->off==0){char p[100];prompt_str(s,p);ui_text(w,8,y+2,s->l[i],0xD6DEEB,1);int x=8+strlen(s->l[i])*8;
            ui_text(w,x,y+2,p,T.accent2,1);x+=strlen(p)*8;ui_text(w,x,y+2,s->in,0xFFFFFF,1);x+=s->il*8;
            if(((ticks/50)&1)||1)fill_rect(w->cx+x,w->cy+y,7,10,((ticks/50)&1)?T.accent2:blend(0x0D1017,T.accent2,60));}
        else ui_text(w,8,y+2,s->l[i],0xD6DEEB,1);}
}
static void term_key2(win_t*w,term_t*s,int k,int m){
    if(k=='\n'){char p[100];prompt_str(s,p);tp(s,p);tp(s,s->in);tp(s,"\n");if(s->il){if(s->nh==16){memmove(s->hist[0],s->hist[1],15*120);s->nh--;}scopy(s->hist[s->nh++],s->in,120);}s->hi=s->nh;
        char cmd[120];scopy(cmd,s->in,120);s->il=0;s->in[0]=0;s->off=0;term_exec(w,s,cmd);return;}
    if(k=='\b'){if(s->il)s->in[--s->il]=0;return;}
    if((m&M_CTRL)&&k=='v'){const char*c=cb_get(0);while(*c&&s->il<110)s->in[s->il++]=*c++;s->in[s->il]=0;return;}
    if((m&M_CTRL)&&k=='l'){s->n=1;s->l[0][0]=0;return;}
    if((m&M_CTRL)&&k=='c'){tp(s,"^C\n");s->il=0;s->in[0]=0;return;}
    if(k==K_UP&&s->hi>0){s->hi--;scopy(s->in,s->hist[s->hi],120);s->il=strlen(s->in);return;}
    if(k==K_DOWN){if(s->hi<s->nh-1){s->hi++;scopy(s->in,s->hist[s->hi],120);}else{s->hi=s->nh;s->in[0]=0;}s->il=strlen(s->in);return;}
    if(k==K_PGUP){s->off=MIN(s->off+8,s->n);return;}if(k==K_PGDN){s->off=MAX(s->off-8,0);return;}
    if(k=='\t'){int st=s->il;while(st>0&&s->in[st-1]!=' ')st--;char*pre=s->in+st;int ks[64],n=fs_children(s->cwd,ks,64),hit=-1,cnt=0;
        for(int i=0;i<n;i++)if(!strncmp(fsn[ks[i]].name,pre,strlen(pre))){hit=ks[i];cnt++;}
        if(cnt==1&&st+(int)strlen(fsn[hit].name)<118){scopy(pre,fsn[hit].name,118-st);s->il=strlen(s->in);}return;}
    if(k>=32&&k<127&&s->il<110){s->in[s->il++]=k;s->in[s->il]=0;}
}
static void term_key(win_t*w,int k,int m){term_key2(w,(term_t*)w->d,k,m);}
static void term_scroll(win_t*w,int dz){ST(term_t);s->off=CLAMP(s->off-dz*3,0,s->n);}
static void term_tick2(term_t*s){
    if(!s->job)return;u32 r,ip;char b[24];
    if(ticks-s->jt>1500){tp(s,"timed out\n");s->job=0;return;}
    if(s->job==1){int st=net_ping_state(&r,&ip);if(st==3){ip_str(ip,b);TPF(s,"reply from %s: time=%d ms\n",b,r);s->job=0;}else if(st==4){tp(s,"request timed out\n");s->job=0;}else if(st==5){tp(s,"unknown host\n");s->job=0;}}
    else if(s->job==2){net_owner=s->jail?"Safe Space":"Terminal";int k=net_resolve(s->jarg,&ip);if(k>0){ip_str(ip,b);TPF(s,"%s has address %s\n",s->jarg,b);s->job=0;}else if(k<0){tp(s,"could not resolve\n");s->job=0;}}
    else if(s->job==3){if(http.state==5){u32 off=http.hdr_end;int n=fs_find(s->cwd,s->jfile);if(n<0)n=fs_create(s->cwd,s->jfile,0);if(n>0)fs_write(n,http.body+off,http.len-off);TPF(s,"saved %s (%d bytes, HTTP %d)\n",s->jfile,http.len-off,http.status);s->job=0;}else if(http.state==6){TPF(s,"wget: %s\n",http.err);s->job=0;}}
    else if(s->job==4){if(net.cfg){ip_str(net.ip,b);TPF(s,"connected: %s\n",b);s->job=0;}}
    wm_redraw();
}
static void term_tick(win_t*w){if(ticks%50==0)wm_redraw();term_tick2((term_t*)w->d);}

/* ================================================== Files ============== */
int sbx_test_file(int node);
typedef struct{int cur,sel,scroll,ren,psel,nk,nte,kids[96];char rb[32];int rl;tar_ent*te;u32*pimg;int pw_,ph_;}files_t;
static void files_init(win_t*w){
    files_t*s=kzalloc(sizeof(files_t));w->d=s;s->te=kmalloc(sizeof(tar_ent)*48);s->cur=fs_home;s->psel=-1;s->sel=-1;w->mem=32;
    if(g_arg>0&&fsn[g_arg].used){if(fsn[g_arg].dir)s->cur=g_arg;else{s->cur=fsn[g_arg].parent;s->sel=g_arg;}}
}
static void files_close(win_t*w){ST(files_t);if(s&&s->te)kfree(s->te);if(s&&s->pimg)kfree(s->pimg);}
static int is_img(const char*n){return is_img_name(n);}
static const char*kind_of(int n){if(fsn[n].dir)return "Folder";if(is_img(fsn[n].name))return "Image";if(ends_with(fsn[n].name,".tar"))return "Archive";return "Text file";}
static void size_str(char*b,u32 v){if(v<1024)ksnprintf(b,16,"%d B",v);else ksnprintf(b,16,"%d KB",v/1024);}
static void files_draw(win_t*w){
    ST(files_t);ui_bg(w,T.win);char p[64],b[64];fs_path(s->cur,p,64);ksnprintf(b,64,"Files - %s",p);ui_title(w,b);
    s->nk=fs_children(s->cur,s->kids,96);if(s->sel>0&&!fsn[s->sel].used)s->sel=-1;
    int cw=w->cw,ch=w->ch,pv=cw>760?260:0,side=150;
    ui_button(w,1,10,8,34,28,"Up",0);ui_fill(w,52,8,cw-62,28,T.field,6);ui_text(w,62,18,p,T.text,1);
    ui_button(w,2,10,44,92,28,"New folder",0);ui_button(w,3,108,44,80,28,"New file",0);ui_button(w,6,194,44,70,28,"Rename",0);ui_button(w,4,270,44,66,28,"Delete",0);
    if(s->cur==fs_trash&&s->sel>0)ui_button(w,7,342,44,80,28,"Restore",1);
    if(s->sel>0&&fsn[s->sel].prev)ui_button(w,8,428,44,130,28,"Previous version",0);
    if(s->sel>0&&is_img(fsn[s->sel].name))ui_button(w,9,564,44,120,28,"Set wallpaper",1);
    if(s->sel>0&&s->cur!=fs_trash)ui_button(w,20,690,44,160,28,"Test in Safe Space",0);
    rrect_ex(w->cx,w->cy+80,side,ch-80,10,T.win2,255,4,0);
    const char*pn[6]={"Home","Desktop","Documents","Pictures","Downloads","Trash"};int pd[6]={fs_home,fs_desk,fs_docs,fs_pics,fs_dl,fs_trash};
    for(int i=0;i<6;i++){int y=92+i*34,on=s->cur==pd[i];if(on||w_hover(w,6,y,side-12,30))ui_fill(w,6,y,side-12,30,on?T.accent:T.hover,6);
        icon_color(on?fgon(T.accent):T.text,0);draw_icon(IC_FOLDER,w->cx+14,w->cy+y+6,18);ui_text(w,40,y+11,pn[i],on?fgon(T.accent):T.text,1);w_hit(w,6,y,side-12,30,10+i);}
    int lx=side+8,lw=cw-side-pv-16;ui_text(w,lx+34,88,"Name",T.dim,1);ui_text(w,lx+lw-190,88,"Size",T.dim,1);ui_text(w,lx+lw-110,88,"Type",T.dim,1);
    int rows=(ch-112)/26;
    for(int i=s->scroll;i<s->nk&&i<s->scroll+rows;i++){int n=s->kids[i],y=108+(i-s->scroll)*26,on=n==s->sel;
        if(on||w_hover(w,lx,y,lw,26))ui_fill(w,lx,y,lw,26,on?T.accent:T.hover,6);u32 fg=on?fgon(T.accent):T.text;
        icon_color(fg,T.win);draw_icon(fsn[n].dir?IC_FOLDER:(is_img(fsn[n].name)?IC_IMAGE:(ends_with(fsn[n].name,".tar")?IC_ARCHIVE:IC_FILE)),w->cx+lx+8,w->cy+y+4,18);
        if(s->ren==n){ui_fill(w,lx+32,y+2,lw-220,22,T.field,4);ui_text(w,lx+38,y+9,s->rb,T.text,1);fill_rect(w->cx+lx+38+s->rl*8,w->cy+y+6,1,14,T.accent);}
        else ui_text(w,lx+34,y+9,fsn[n].name,fg,1);
        if(!fsn[n].dir){size_str(b,fsn[n].size);ui_text(w,lx+lw-190,y+9,b,on?fg:T.dim,1);}ui_text(w,lx+lw-110,y+9,kind_of(n),on?fg:T.dim,1);
        w_hit(w,lx,y,lw,26,1000+i);}
    if(!s->nk)ui_text(w,lx+10,116,"This folder is empty.",T.dim,1);
    if(pv){int px=cw-pv;rrect_ex(w->cx+px,w->cy+80,pv,ch-80,10,T.win2,255,8,0);int n=s->sel;
        if(n>0&&fsn[n].used){
            if(s->psel!=n){s->psel=n;s->nte=0;if(ends_with(fsn[n].name,".tar"))s->nte=tar_list(fsn[n].data,fsn[n].size,s->te,48);
                if(s->pimg){kfree(s->pimg);s->pimg=0;}if(is_img(fsn[n].name)){int iw,ih,own;u32*src=img_load(n,&iw,&ih,&own);if(src){int aw=pv-28,ah=150,dw=aw,dh=ih*aw/iw;if(dh>ah){dh=ah;dw=iw*ah/ih;}s->pimg=make_thumb(src,iw,ih,MAX(dw,2),MAX(dh,2));s->pw_=MAX(dw,2);s->ph_=MAX(dh,2);s->pw_=s->pimg?s->pw_:0;ksnprintf(s->rb,32,"%dx%d",iw,ih);if(own)kfree(src);}}}
            ui_text(w,px+14,92,fsn[n].name,T.text,1);ksnprintf(b,64,"%s  -  %d bytes",kind_of(n),fsn[n].size);ui_text(w,px+14,108,b,T.dim,1);
            int y=130;
            if(fsn[n].dir){int k[96];ksnprintf(b,64,"%d items",fs_children(n,k,96));ui_text(w,px+14,y,b,T.dim,1);}
            else if(ends_with(fsn[n].name,".tar")){ui_text(w,px+14,y,"Archive contents",T.text,1);y+=18;
                for(int i=0;i<s->nte&&y<ch-52;i++){icon_color(T.text,T.win2);draw_icon(s->te[i].dir?IC_FOLDER:IC_FILE,w->cx+px+14,w->cy+y,14);char nm[26];scopy(nm,s->te[i].name,26);ui_text(w,px+34,y+3,nm,T.text,1);y+=18;}
                ui_button(w,5,px+14,ch-44,pv-28,30,"Extract here",1);}
            else if(is_img(fsn[n].name)){if(s->pimg){cliprect c=clip_push(w->cx,w->cy,w->cw,w->ch);blit_scaled(s->pimg,s->pw_,s->ph_,w->cx+px+14,w->cy+y,s->pw_,s->ph_);clip_pop(c);ui_text(w,px+14,y+s->ph_+10,s->rb,T.dim,1);}else ui_text(w,px+14,y,"Cannot preview this image",T.dim,1);}
            else{int ln=0,col=0;for(u32 k=0;k<fsn[n].size&&ln<14;k++){char c=fsn[n].data[k];if(c=='\n'||col>=(pv-28)/8){ln++;col=0;if(c=='\n')continue;}char t[2]={c>=32&&c<127?c:'.',0};ui_text(w,px+14+col*8,y+ln*12,t,T.dim,1);col++;}}
        }else ui_text(w,px+14,92,"Select a file to preview",T.dim,1);}
}
static void files_action(win_t*w,int id,int dbl){
    ST(files_t);char nm[32];
    if(id==1&&s->cur!=0){s->sel=s->cur;s->cur=fsn[s->cur].parent;s->scroll=0;}
    else if(id==2){fs_unique(s->cur,"New folder",nm);fs_create(s->cur,nm,1);}
    else if(id==3){fs_unique(s->cur,"New file.txt",nm);fs_create(s->cur,nm,0);}
    else if(id==4&&s->sel>0){fs_trash_node(s->sel);s->sel=-1;toast(s->cur==fs_trash?"Deleted permanently":"Moved to Trash");}
    else if(id==7&&s->sel>0){if(!fs_untrash(s->sel))toast("Restored");s->sel=-1;}
    else if(id==8&&s->sel>0){if(!fs_restore_prev(s->sel))toast("Restored the previous version");s->psel=-1;}
    else if(id==9&&s->sel>0)wallpaper_set_node(s->sel);
    else if(id==20&&s->sel>0)sbx_test_file(s->sel);
    else if(id==5&&s->sel>0){int k=tar_extract(s->sel,s->cur);char b[48];ksnprintf(b,48,"Extracted %d files",k);toast(b);}
    else if(id==6&&s->sel>0){s->ren=s->sel;scopy(s->rb,fsn[s->sel].name,32);s->rl=strlen(s->rb);}
    else if(id>=10&&id<16){int pd[6]={fs_home,fs_desk,fs_docs,fs_pics,fs_dl,fs_trash};s->cur=pd[id-10];s->scroll=0;s->sel=-1;}
    else if(id>=1000){int i=id-1000;if(i<s->nk){int n=s->kids[i];s->sel=n;s->ren=0;
        if(dbl){if(fsn[n].dir){s->cur=n;s->sel=-1;s->scroll=0;}else if(!ends_with(fsn[n].name,".tar"))open_node(n);}}}
}
static void files_key(win_t*w,int k,int m){
    ST(files_t);
    if(s->ren>0){if(k=='\n'){fs_rename(s->ren,s->rb);s->ren=0;}else if(k==27)s->ren=0;else if(k=='\b'){if(s->rl)s->rb[--s->rl]=0;}else if(k>=32&&k<127&&s->rl<30){s->rb[s->rl++]=k;s->rb[s->rl]=0;}return;}
    int idx=-1;for(int i=0;i<s->nk;i++)if(s->kids[i]==s->sel)idx=i;
    if(k==K_DOWN&&idx<s->nk-1)s->sel=s->kids[idx+1];else if(k==K_UP&&idx>0)s->sel=s->kids[idx-1];
    else if(k==K_DEL&&s->sel>0){fs_trash_node(s->sel);s->sel=-1;}else if(k==K_F2)files_action(w,6,0);
    else if(k=='\b')files_action(w,1,0);else if(k=='\n'&&s->sel>0){if(fsn[s->sel].dir){s->cur=s->sel;s->sel=-1;}else if(!ends_with(fsn[s->sel].name,".tar"))open_node(s->sel);}
    (void)m;
}
static void files_click(win_t*w,int x,int y,int b,int d){
    ST(files_t);(void)d;if(b!=2)return;int rows=(w->ch-112)/26;int i=s->scroll+(y-108)/26;if(y>=108&&(y-108)/26<rows&&i<s->nk&&x>150){s->sel=s->kids[i];sbx_test_file(s->sel);}
}
static void files_scroll(win_t*w,int dz){ST(files_t);s->scroll=CLAMP(s->scroll+dz,0,MAX(0,s->nk-3));}

/* ================================================== Notes ============== */
typedef struct{char*buf;int cap,len,cur,top,left,node,dirty;}ed_t;
static void ed_load(win_t*w,int n){
    ST(ed_t);s->node=n;s->len=0;s->cur=0;s->top=0;s->left=0;s->dirty=0;
    if(n>0){u32 l=MIN(fsn[n].size,(u32)s->cap-1);memcpy(s->buf,fsn[n].data,l);s->len=l;}
    char t[48];ksnprintf(t,48,"Notes - %s",n>0?fsn[n].name:"Untitled");ui_title(w,t);
}
static void notes_init(win_t*w){ed_t*s=kzalloc(sizeof(ed_t));w->d=s;s->cap=32768;s->buf=kzalloc(s->cap);w->mem=40;
    int n=g_arg>0?g_arg:fs_find(fs_docs,"notes.txt");ed_load(w,n);}
static void notes_close(win_t*w){ST(ed_t);if(s&&s->buf)kfree(s->buf);}
static int ed_ls(ed_t*e,int i){while(i>0&&e->buf[i-1]!='\n')i--;return i;}
static int ed_le(ed_t*e,int i){while(i<e->len&&e->buf[i]!='\n')i++;return i;}
static void ed_save(win_t*w){ST(ed_t);if(s->node<=0){char nm[32];fs_unique(w->sbx?sbx_dir():fs_docs,"Untitled.txt",nm);s->node=fs_create(w->sbx?sbx_dir():fs_docs,nm,0);}if(s->node>0&&!fs_write(s->node,s->buf,s->len)){s->dirty=0;toast("Saved");ed_load(w,s->node);s->cur=s->cur;}}
static void ed_view(win_t*w){
    ST(ed_t);int rows=(w->ch-44-22)/12,cols=(w->cw-52)/8;if(rows<1)rows=1;if(cols<1)cols=1;
    int line=0;for(int i=0;i<s->cur;i++)if(s->buf[i]=='\n')line++;int col=s->cur-ed_ls(s,s->cur);
    if(line<s->top)s->top=line;if(line>=s->top+rows)s->top=line-rows+1;if(col<s->left)s->left=col;if(col>=s->left+cols)s->left=col-cols+1;
}
static void notes_draw(win_t*w){
    ST(ed_t);ui_bg(w,T.win);ed_view(w);
    ui_button(w,1,10,6,66,28,"Save",1);ui_button(w,2,82,6,60,28,"New",0);if(s->node>0&&ends_with(fsn[s->node].name,".cs"))ui_button(w,3,148,6,60,28,"Run",1);
    fill_rect_a(w->cx,w->cy+40,w->cw,1,T.border,120);
    int rows=(w->ch-44-22)/12,cols=(w->cw-52)/8,ln=0,idx=0;
    while(ln<s->top&&idx<s->len){if(s->buf[idx]=='\n')ln++;idx++;}
    for(int r=0;r<rows;r++){
        int le=ed_le(s,idx),y=48+r*12;char nb[8];ksnprintf(nb,8,"%d",ln+1);ui_text(w,36-strlen(nb)*8,y+2,nb,T.dim,1);
        for(int c=0;c<cols&&idx+s->left+c<le;c++)draw_char(w->cx+44+c*8,w->cy+y+2,(u8)s->buf[idx+s->left+c],T.text,1);
        if(s->cur>=idx&&s->cur<=le&&((ticks/50)&1||1))fill_rect(w->cx+44+(s->cur-idx-s->left)*8,w->cy+y,2,11,((ticks/50)&1)?T.accent:blend(T.win,T.accent,70));
        idx=le+1;ln++;if(idx>s->len+0&&le>=s->len)break;
    }
    int line=0;for(int i=0;i<s->cur;i++)if(s->buf[i]=='\n')line++;char b[96];
    ksnprintf(b,96,"Ln %d, Col %d    %d chars    %s    Ctrl+S saves",line+1,s->cur-ed_ls(s,s->cur)+1,s->len,s->dirty?"Modified":"Saved");
    fill_rect_a(w->cx,w->cy+w->ch-22,w->cw,22,T.win2,255);ui_text(w,10,w->ch-15,b,T.dim,1);
}
static void ed_ins(ed_t*s,char c){if(s->len>=s->cap-1)return;memmove(s->buf+s->cur+1,s->buf+s->cur,s->len-s->cur);s->buf[s->cur++]=c;s->len++;s->dirty=1;}
static void notes_key(win_t*w,int k,int m){
    ST(ed_t);int rows=(w->ch-44-22)/12;
    if((m&M_CTRL)&&k=='s'){ed_save(w);return;}
    if((m&M_CTRL)&&(k=='c'||k=='x')){int ls=ed_ls(s,s->cur),le=ed_le(s,s->cur);cb_push(s->buf+ls,le-ls);toast(k=='c'?"Line copied":"Line cut");
        if(k=='x'){int e=le<s->len?le+1:le;memmove(s->buf+ls,s->buf+e,s->len-e);s->len-=e-ls;s->cur=ls;s->dirty=1;}return;}
    if((m&M_CTRL)&&k=='v'){const char*c=cb_get(0);for(;*c;c++)ed_ins(s,*c);return;}
    if(k=='\n')ed_ins(s,'\n');else if(k=='\t'){for(int i=0;i<4;i++)ed_ins(s,' ');}
    else if(k=='\b'){if(s->cur>0){memmove(s->buf+s->cur-1,s->buf+s->cur,s->len-s->cur);s->cur--;s->len--;s->dirty=1;}}
    else if(k==K_DEL){if(s->cur<s->len){memmove(s->buf+s->cur,s->buf+s->cur+1,s->len-s->cur-1);s->len--;s->dirty=1;}}
    else if(k==K_LEFT){if(s->cur>0)s->cur--;}else if(k==K_RIGHT){if(s->cur<s->len)s->cur++;}
    else if(k==K_HOME)s->cur=ed_ls(s,s->cur);else if(k==K_END)s->cur=ed_le(s,s->cur);
    else if(k==K_UP||k==K_PGUP||k==K_DOWN||k==K_PGDN){int n=(k==K_UP||k==K_DOWN)?1:rows,col=s->cur-ed_ls(s,s->cur);
        for(int i=0;i<n;i++){if(k==K_UP||k==K_PGUP){int ls=ed_ls(s,s->cur);if(ls==0){s->cur=0;break;}int ps=ed_ls(s,ls-1);s->cur=MIN(ps+col,ls-1);}
            else{int le=ed_le(s,s->cur);if(le>=s->len){s->cur=s->len;break;}int ne=ed_le(s,le+1);s->cur=MIN(le+1+col,ne);}}}
    else if(k>=32&&k<127&&!(m&M_CTRL))ed_ins(s,k);
}
static void notes_action(win_t*w,int id,int dbl){ST(ed_t);(void)dbl;if(id==1)ed_save(w);else if(id==3){ed_save(w);if(s->node>0)win_open_arg(APP_SCRIPT,s->node);}else if(id==2){s->len=0;s->cur=0;s->node=0;s->dirty=0;ui_title(w,"Notes - Untitled");}}
static void notes_click(win_t*w,int x,int y,int b,int d){
    ST(ed_t);(void)b;(void)d;if(y<44||y>w->ch-22)return;int row=(y-44)/12+s->top,col=(x-44)/8+s->left;if(col<0)col=0;
    int ln=0,idx=0;while(ln<row&&idx<s->len){if(s->buf[idx]=='\n')ln++;idx++;}if(ln<row){s->cur=s->len;return;}
    int le=ed_le(s,idx);s->cur=MIN(idx+col,le);
}
static void notes_scroll(win_t*w,int dz){ST(ed_t);s->top=MAX(0,s->top+dz*3);int lines=0;for(int i=0;i<s->len;i++)if(s->buf[i]=='\n')lines++;if(s->top>lines)s->top=lines;
    int line=0;for(int i=0;i<s->cur;i++)if(s->buf[i]=='\n')line++;(void)line;}

/* ================================================== System monitor ===== */
static void mon_draw(win_t*w){
    ui_bg(w,T.win);int pad=16,cw=w->cw,gw=(cw-3*pad)*62/100,rw=cw-3*pad-gw;char b[96];
    ui_fill(w,pad,pad,gw,210,T.win2,12);ui_text(w,pad+16,pad+14,"CPU",T.dim,1);ksnprintf(b,96,"%d%%",cpu_pct);ui_text(w,pad+16,pad+30,b,T.accent2,3);
    char br[40];scopy(br,cpu_brand,(gw-140)/8);ui_text(w,pad+110,pad+36,br,T.text,1);ksnprintf(b,96,"vendor %s",cpu_vendor);ui_text(w,pad+110,pad+50,b,T.dim,1);
    int gx=pad+16,gy=pad+80,gwid=gw-32,gh=112;ui_fill(w,gx,gy,gwid,gh,T.field,8);
    cliprect c=clip_push(w->cx+gx,w->cy+gy,gwid,gh);
    for(int i=0;i<60;i++){int bh=cpu_hist[i]*gh/100;int x=w->cx+gx+i*gwid/60;fill_rect_a(x,w->cy+gy+gh-bh,gwid/60+1,bh,T.accent,150);if(bh)fill_rect(x,w->cy+gy+gh-bh,gwid/60+1,2,T.accent2);}
    clip_pop(c);
    int mp=pct(heap_used(),heap_total());
    ui_fill(w,2*pad+gw,pad,rw,210,T.win2,12);ui_text(w,2*pad+gw+16,pad+14,"Memory",T.dim,1);
    gauge(w->cx+2*pad+gw+rw/2,w->cy+pad+110,52,mp,0xF472B6,T.dim);ksnprintf(b,96,"%d%%",mp);ui_text(w,2*pad+gw+rw/2-text_w(b,2)/2,pad+102,b,T.text,2);
    ksnprintf(b,96,"%d / %d MB heap",heap_used()>>20,heap_total()>>20);ui_text(w,2*pad+gw+rw/2-text_w(b,1)/2,pad+182,b,T.dim,1);
    int y=2*pad+210,bh=MAX(110,w->ch-y-pad),hw=(cw-3*pad)/2;
    ui_fill(w,pad,y,hw,bh,T.win2,12);ui_text(w,pad+16,y+14,"Storage",T.dim,1);
    ksnprintf(b,96,"RAM disk  %d KB  (%d items)",fs_used()/1024,fs_nodes());ui_text(w,pad+16,y+36,b,T.text,1);ui_bar(w,pad+16,y+54,hw-32,8,MIN(100,(int)(fs_used()/(1024*1024)*100/64)),T.accent);
    int ly=y+78;for(int i=0;i<4;i++)if(ata[i].present&&ly<y+bh-14){ksnprintf(b,96,"%s: %d MB",ata[i].atapi?"CD-ROM":"Disk",ata[i].sectors/2048);ui_text(w,pad+16,ly,b,T.text,1);char m[30];scopy(m,ata[i].model,(hw-32)/8);ui_text(w,pad+16,ly+12,m,T.dim,1);ly+=30;}
    ui_fill(w,2*pad+hw,y,hw,bh,T.win2,12);ui_text(w,2*pad+hw+16,y+14,"System",T.dim,1);u32 u=ticks/100;
    ksnprintf(b,96,"Uptime      %dh %02dm %02ds",u/3600,(u/60)%60,u%60);ui_text(w,2*pad+hw+16,y+36,b,T.text,1);
    int pids[40];char nm[40][24];ksnprintf(b,96,"Processes   %d",proc_list(pids,nm,40));ui_text(w,2*pad+hw+16,y+52,b,T.text,1);
    ksnprintf(b,96,"Display     %dx%d",SW,SH);ui_text(w,2*pad+hw+16,y+68,b,T.text,1);
    ksnprintf(b,96,"PCI devices %d",pci_count);ui_text(w,2*pad+hw+16,y+84,b,T.text,1);
    ksnprintf(b,96,"RAM         %d MB",mem_total_kb/1024);ui_text(w,2*pad+hw+16,y+100,b,T.text,1);
}

/* ================================================== Task manager ======= */
typedef struct{int sel,scroll;}task_t;
static void task_init(win_t*w){w->d=kzalloc(sizeof(task_t));((task_t*)w->d)->sel=-1;}
static void task_draw(win_t*w){
    ST(task_t);ui_bg(w,T.win);int pids[40];char nm[40][24];int n=proc_list(pids,nm,40),cw=w->cw;char b[64];
    int mp=pct(heap_used(),heap_total());int cx[4]={16,16+(cw-48)/4+8,16+2*((cw-48)/4+8),16+3*((cw-48)/4+8)},cwid=(cw-48)/4;
    const char*lab[4]={"CPU","Memory","Processes","Uptime"};u32 up=ticks/100;
    for(int i=0;i<4;i++){ui_fill(w,cx[i],12,cwid,62,T.win2,10);ui_text(w,cx[i]+12,22,lab[i],T.dim,1);
        if(i==0)ksnprintf(b,64,"%d%%",cpu_pct);else if(i==1)ksnprintf(b,64,"%d%%",mp);else if(i==2)ksnprintf(b,64,"%d",n);else ksnprintf(b,64,"%d:%02d:%02d",up/3600,(up/60)%60,up%60);
        ui_text(w,cx[i]+12,40,b,i==0?T.accent2:T.text,2);}
    int y0=88;ui_text(w,20,y0,"Name",T.dim,1);ui_text(w,cw-340,y0,"PID",T.dim,1);ui_text(w,cw-270,y0,"CPU",T.dim,1);ui_text(w,cw-200,y0,"Memory",T.dim,1);ui_text(w,cw-100,y0,"Type",T.dim,1);
    int rows=(w->ch-y0-62)/26;
    for(int i=s->scroll;i<n&&i<s->scroll+rows;i++){int y=y0+16+(i-s->scroll)*26,on=pids[i]==s->sel,sys=pids[i]<=5;u32 cp,me;proc_info(pids[i],&cp,&me);
        if(on||w_hover(w,12,y,cw-24,26))ui_fill(w,12,y,cw-24,26,on?T.accent:T.hover,6);u32 fg=on?fgon(T.accent):T.text;
        ui_text(w,20,y+9,nm[i],fg,1);ksnprintf(b,64,"%d",pids[i]);ui_text(w,cw-340,y+9,b,fg,1);ksnprintf(b,64,"%d.%d%%",cp/10,cp%10);ui_text(w,cw-270,y+9,b,fg,1);
        ksnprintf(b,64,"%d KB",me);ui_text(w,cw-200,y+9,b,fg,1);ui_text(w,cw-100,y+9,sys?"System":"App",on?fg:T.dim,1);w_hit(w,12,y,cw-24,26,1000+pids[i]);}
    ui_button(w,1,cw-130,w->ch-46,114,32,"End task",s->sel>5?2:0);
}
static void task_action(win_t*w,int id,int dbl){ST(task_t);(void)dbl;
    if(id>=1000)s->sel=id-1000;else if(id==1&&s->sel>0){int r=proc_kill(s->sel);if(r==-1)toast("System processes can't be ended");else if(r==1)s->sel=-1;}}
static void task_scroll(win_t*w,int dz){ST(task_t);s->scroll=MAX(0,s->scroll+dz);}
static void task_key(win_t*w,int k,int m){ST(task_t);(void)m;if(k==K_DEL&&s->sel>5)task_action(w,1,0);}

/* ================================================== App Store ========== */
typedef struct{int prog[APP_COUNT],scroll,ul,req,mode,ngal,galreq;char url[120],msg[80];struct{char name[24],url[110],desc[50];}gal[12];}store_t;
static void store_init(win_t*w){store_t*s=kzalloc(sizeof(store_t));w->d=s;scopy(s->msg,"Paste the address of a CometScript (.cs) file to add an app from the web (http only).",80);}
static void store_draw(win_t*w){
    ST(store_t);ui_bg(w,T.win);fill_grad(w->cx,w->cy,w->cw,96,blend(T.accent,0x000000,60),0x6D28D9);
    ui_text(w,24,22,"Comet Store",0xFFFFFF,3);ui_text(w,24,58,"Add apps, remove the ones you don't use, or install scripts from a web address.",0xE6E1FF,1);
    ui_fill(w,16,106,w->cw-170,32,T.field,10);ui_text(w,28,118,s->ul?s->url:"http://example.com/app.cs  (or a gallery list)",s->ul?T.text:T.dim,1);if(((ticks/50)&1))fill_rect(w->cx+28+s->ul*8,w->cy+112,1,18,T.accent);
    ui_button(w,90,w->cw-146,106,130,32,s->mode?"Load list":"Install script",1);
    ui_button(w,92,16,146,90,28,"Apps",s->mode==0);ui_button(w,93,112,146,90,28,"Gallery",s->mode==1);ui_text(w,216,157,s->msg,T.dim,1);
    cliprect c=clip_push(w->cx,w->cy+182,w->cw,w->ch-182);int y=186-s->scroll;
    if(s->mode==1){
        if(!s->ngal)ui_text(w,24,y+10,"Gallery lists are plain text: one app per line as  name|address|description.",T.dim,1);
        for(int i=0;i<s->ngal;i++){ui_fill(w,16,y,w->cw-32,62,T.win2,10);draw_icon(APP_SCRIPT,w->cx+28,w->cy+y+9,44);ui_text(w,84,y+14,s->gal[i].name,T.text,1);ui_text(w,84,y+32,s->gal[i].desc,T.dim,1);ui_button(w,400+i,w->cw-126,y+16,110,30,"Install",1);y+=70;}
    }else for(int i=0;i<APP_COUNT;i++){if(i==APP_STORE||i==APP_SCRIPT)continue;
        if(y>-70&&y<w->ch){ui_fill(w,16,y,w->cw-32,62,T.win2,10);draw_icon(i,w->cx+28,w->cy+y+9,44);
            ui_text(w,84,y+14,apps[i].name,T.text,1);ui_text(w,84,y+32,apps[i].desc,T.dim,1);
            if(s->prog[i]>0&&s->prog[i]<100)ui_bar(w,w->cw-150,y+26,110,10,s->prog[i],T.accent);
            else if(apps[i].installed){ui_button(w,100+i,w->cw-150,y+16,64,30,"Open",0);if(app_removable(i))ui_button(w,300+i,w->cw-80,y+16,56,30,"Remove",2);}
            else ui_button(w,100+i,w->cw-150,y+16,110,30,"Get",1);}
        y+=70;}
    clip_pop(c);
}
static void store_action(win_t*w,int id,int d){
    ST(store_t);(void)d;
    if(id==92||id==93){s->mode=id-92;s->scroll=0;}
    else if(id>=400&&id<412){int i=id-400;if(i<s->ngal){scopy(s->url,s->gal[i].url,120);s->ul=strlen(s->url);store_action(w,90,0);}}
    else if(id==90){if(!s->ul)return;net_owner="Store";if(http_start(s->url)<0)scopy(s->msg,http.err,80);else{if(s->mode==1)s->galreq=1;else s->req=1;scopy(s->msg,"Downloading...",80);}}
    else if(id>=300&&id<300+APP_COUNT)app_uninstall(id-300);
    else if(id>=100&&id<100+APP_COUNT){int i=id-100;if(apps[i].installed)win_open(i);else if(!s->prog[i])s->prog[i]=1;}
}
static void store_key(win_t*w,int k,int m){ST(store_t);(void)m;if(k=='\n')store_action(w,90,0);else if(k=='\b'){if(s->ul)s->url[--s->ul]=0;}else if(k>=32&&k<127&&s->ul<110){s->url[s->ul++]=k;s->url[s->ul]=0;}}
static void store_tick(win_t*w){
    ST(store_t);
    if(s->req){if(http.state>=1&&http.state<=4)wm_redraw();
        if(http.state==5){s->req=0;const char*p=s->url;const char*last=p;for(const char*q=p;*q;q++)if(*q=='/')last=q+1;char nm[32];int m=0;while(last[m]&&last[m]!='?'&&m<26){nm[m]=last[m];m++;}nm[m]=0;if(!m)scopy(nm,"app",8);if(!ends_with(nm,".cs"))scopy(nm+strlen(nm),".cs",4);
            char u[32];fs_unique(fs_apps,nm,u);int n=fs_create(fs_apps,u,0);if(n>0&&http.len>(u32)http.hdr_end){fs_write(n,http.body+http.hdr_end,http.len-http.hdr_end);ksnprintf(s->msg,80,"Installed %s - find it in Start and in /home/guest/Apps",u);toast("App installed");}else scopy(s->msg,"Download failed",80);wm_redraw();}
        else if(http.state==6){s->req=0;scopy(s->msg,http.err,80);wm_redraw();}}
    if(s->galreq){if(http.state==5){s->galreq=0;s->ngal=0;const char*p=(const char*)http.body+http.hdr_end,*e=(const char*)http.body+http.len;
            while(p<e&&s->ngal<12){const char*ln=p;while(p<e&&*p!='\n')p++;int len=p-ln;if(p<e)p++;if(len>1&&len<200&&ln[0]!='#'){char t[200];memcpy(t,ln,len);t[len]=0;if(t[len-1]=='\r')t[len-1]=0;char*f[3]={t,0,0};int k=0;for(char*q=t;*q&&k<2;q++)if(*q=='|'){*q=0;f[++k]=q+1;}
                if(k>=1){scopy(s->gal[s->ngal].name,f[0],24);scopy(s->gal[s->ngal].url,f[1],110);scopy(s->gal[s->ngal].desc,f[2]?f[2]:"",50);s->ngal++;}}}
            ksnprintf(s->msg,80,"%d app%s in this gallery",s->ngal,s->ngal==1?"":"s");wm_redraw();}
        else if(http.state==6){s->galreq=0;scopy(s->msg,http.err,80);wm_redraw();}else wm_redraw();}
    if(ticks%3)return;
    for(int i=0;i<APP_COUNT;i++)if(s->prog[i]>0&&s->prog[i]<100){s->prog[i]+=4;wm_redraw();if(s->prog[i]>=100){s->prog[i]=100;app_install(i);char b[64];ksnprintf(b,64,"%s installed",apps[i].name);toast(b);}}
}
static void store_scroll(win_t*w,int dz){ST(store_t);s->scroll=CLAMP(s->scroll+dz*24,0,MAX(0,(s->mode?s->ngal:APP_COUNT)*70-(w->ch-190)));}

/* ================================================== Photos ============= */
typedef struct{int list[64],n,idx,iw,ih,own,slide;u32 last;u32*px;u32*th[16];}view_t;
static void view_load(win_t*w){
    ST(view_t);if(s->own&&s->px)kfree(s->px);s->px=0;s->own=0;if(!s->n)return;
    s->px=img_load(s->list[s->idx],&s->iw,&s->ih,&s->own);char t[48];ksnprintf(t,48,"Photos - %s",fsn[s->list[s->idx]].name);ui_title(w,t);
}
static void view_refresh(win_t*w,int arg){
    ST(view_t);int k[96],n=fs_children(fs_pics,k,96);s->n=0;
    if(arg>0&&fsn[arg].parent!=fs_pics&&fsn[arg].parent!=fs_dl)s->list[s->n++]=arg;
    if(w->sbx)n=0;
    for(int i=0;i<n&&s->n<64;i++)if(is_img(fsn[k[i]].name))s->list[s->n++]=k[i];
    if(!w->sbx){n=fs_children(fs_dl,k,96);for(int i=0;i<n&&s->n<64;i++)if(is_img(fsn[k[i]].name))s->list[s->n++]=k[i];}
    for(int i=0;i<16;i++){if(s->th[i]){kfree(s->th[i]);s->th[i]=0;}if(i<s->n){int iw,ih,own;u32*src=img_load(s->list[i],&iw,&ih,&own);if(src){s->th[i]=make_thumb(src,iw,ih,96,60);if(own)kfree(src);}}}
    s->idx=0;for(int i=0;i<s->n;i++)if(s->list[i]==arg)s->idx=i;view_load(w);
}
static void view_init(win_t*w){view_t*s=kzalloc(sizeof(view_t));w->d=s;view_refresh(w,g_arg);}
static void view_close(win_t*w){ST(view_t);if(s&&s->own&&s->px)kfree(s->px);if(s)for(int i=0;i<16;i++)if(s->th[i])kfree(s->th[i]);}
static void view_draw(win_t*w){
    ST(view_t);if(g_arg>0&&fsn[g_arg].used&&is_img(fsn[g_arg].name)){view_refresh(w,g_arg);g_arg=0;}
    ui_bg(w,g_dark?0x10121A:0xE6E9F2);ui_button(w,1,10,8,60,28,"Prev",0);ui_button(w,2,76,8,60,28,"Next",0);ui_button(w,3,142,8,s->slide?110:100,28,s->slide?"Stop show":"Slideshow",s->slide?1:0);ui_button(w,4,258,8,100,28,"Wallpaper",0);
    char b[80];if(s->n&&s->px){ksnprintf(b,80,"%s   %dx%d   (%d/%d)",fsn[s->list[s->idx]].name,s->iw,s->ih,s->idx+1,s->n);ui_text(w,370,18,b,T.dim,1);}
    int ax=16,ay=48,aw=w->cw-32,ah=w->ch-48-90;
    if(s->px){int dw=aw,dh=s->ih*aw/s->iw;if(dh>ah){dh=ah;dw=s->iw*ah/s->ih;}int x=w->cx+ax+(aw-dw)/2,y=w->cy+ay+(ah-dh)/2;
        shadow(x,y,dw,dh,6,10);blit_scaled(s->px,s->iw,s->ih,x,y,dw,dh);rframe(x,y,dw,dh,0,0xFFFFFF,40);}
    else ui_text(w,ax+20,ay+20,"No pictures. Put .cim or .bmp files in /home/guest/Pictures",T.dim,1);
    int tx=16;for(int i=0;i<s->n&&tx<w->cw-100;i++){int ty=w->ch-78,u32on=i==s->idx;
        ui_fill(w,tx-2,ty-2,100,64,u32on?T.accent:T.win2,6);if(i<16&&s->th[i]){cliprect c=clip_push(w->cx+tx,w->cy+ty,96,60);blit_scaled(s->th[i],96,60,w->cx+tx,w->cy+ty,96,60);clip_pop(c);}
        w_hit(w,tx,ty,96,60,2000+i);tx+=108;}
}
static void view_step(win_t*w,int d){ST(view_t);if(!s->n)return;s->idx=(s->idx+d+s->n)%s->n;view_load(w);}
static void view_action(win_t*w,int id,int dbl){ST(view_t);(void)dbl;if(id==1)view_step(w,-1);else if(id==2)view_step(w,1);else if(id==3){s->slide=!s->slide;s->last=ticks;}else if(id==4&&s->n)wallpaper_set_node(s->list[s->idx]);else if(id>=2000){s->idx=id-2000;view_load(w);}}
static void view_key(win_t*w,int k,int m){(void)m;if(k==K_LEFT)view_step(w,-1);else if(k==K_RIGHT||k==' ')view_step(w,1);}
static void view_tick(win_t*w){ST(view_t);if(s->slide&&ticks-s->last>300){s->last=ticks;view_step(w,1);wm_redraw();}}

/* ================================================== Comet Rush (game) == */
typedef struct{int px,state,score,best,lives,spawn,age,mx;u32 last;struct{int x,y16,vy,kind,r;}o[20];struct{int x,y,sp;}st[50];int inited;}game_t;
static void game_reset(game_t*s,int cw){s->px=cw/2;s->score=0;s->lives=3;s->age=0;s->spawn=30;for(int i=0;i<20;i++)s->o[i].r=0;s->state=1;}
static void game_init(win_t*w){game_t*s=kzalloc(sizeof(game_t));w->d=s;s->last=ticks;for(int i=0;i<50;i++){s->st[i].x=rnd()%700;s->st[i].y=rnd()%500;s->st[i].sp=1+rnd()%3;}s->best=0;}
static void game_tick(win_t*w){
    ST(game_t);if(w->minimized)return;int ch=w->ch;
    int n=0;while(s->last<ticks&&n<4){s->last++;n++;
        for(int i=0;i<50;i++){s->st[i].y+=s->st[i].sp;if(s->st[i].y>ch+4){s->st[i].y=0;s->st[i].x=rnd()%MAX(w->cw,1);}}
        if(s->state!=1)continue;s->age++;if(s->age%20==0)s->score++;
        if(mouse_x!=s->mx&&mouse_x>=w->cx&&mouse_x<w->cx+w->cw){s->px=CLAMP(mouse_x-w->cx,14,w->cw-14);}s->mx=mouse_x;
        if(--s->spawn<=0){s->spawn=MAX(18,60-s->age/50);for(int i=0;i<20;i++)if(!s->o[i].r){int st=(rnd()%5)==0;s->o[i].kind=st;s->o[i].r=st?9:(10+rnd()%14);s->o[i].x=s->o[i].r+rnd()%MAX(1,w->cw-2*s->o[i].r);s->o[i].y16=-32*16;s->o[i].vy=24+rnd()%24+s->age/150;break;}}
        for(int i=0;i<20;i++){if(!s->o[i].r)continue;s->o[i].y16+=s->o[i].vy;int y=s->o[i].y16/16;if(y>ch+30){s->o[i].r=0;continue;}
            int dx=s->o[i].x-s->px,dy=y-(ch-50),rr=s->o[i].r+11;if(dx*dx+dy*dy<rr*rr){if(s->o[i].kind){s->score+=10;}else{if(--s->lives<=0){s->state=2;if(s->score>s->best)s->best=s->score;}}s->o[i].r=0;}}
    }
    wm_redraw();
}
static void game_draw(win_t*w){
    ST(game_t);fill_grad(w->cx,w->cy,w->cw,w->ch,0x050818,0x1B1040);rrect_ex(w->cx,w->cy+w->ch-12,w->cw,12,10,0x1B1040,255,12,0);
    for(int i=0;i<50;i++)fill_rect(w->cx+s->st[i].x%MAX(w->cw,1),w->cy+s->st[i].y,s->st[i].sp>2?2:1,s->st[i].sp>2?2:1,s->st[i].sp>1?0xFFFFFF:0x8892B8);
    for(int i=0;i<20;i++)if(s->o[i].r){int x=w->cx+s->o[i].x,y=w->cy+s->o[i].y16/16,r=s->o[i].r;
        if(s->o[i].kind){circle_ex(x,y,r+5,0,0xFACC15,50);circle_ex(x,y,r,0,0xFDE047,255);circle_ex(x,y,r/2,0,0xFFFFFF,200);}
        else{circle_ex(x,y,r,0,0x6B7280,255);circle_ex(x-r/3,y-r/3,r/3,0,0x9CA3AF,255);circle_ex(x+r/3,y+r/4,r/4,0,0x4B5563,255);}}
    if(s->state==1||s->state==0){int x=w->cx+s->px,y=w->cy+w->ch-50;circle_ex(x,y+16,10+((ticks/4)&3),0,0xFB923C,170);tri_up(x,y-14,30,26,0xE5E7EB);tri_up(x,y-14,14,10,T.accent);fill_rect(x-14,y+8,6,6,0x9CA3AF);fill_rect(x+8,y+8,6,6,0x9CA3AF);}
    char b[64];ksnprintf(b,64,"Score %d",s->score);ui_text(w,14,12,b,0xFFFFFF,2);ksnprintf(b,64,"Best %d",s->best);ui_text(w,14,34,b,0xA5B4FC,1);
    for(int i=0;i<3;i++)circle_ex(w->cx+w->cw-24-i*24,w->cy+22,8,i<s->lives?0:2,i<s->lives?0xF472B6:0x6B7280,255);
    if(s->state!=1){int bx=w->cw/2-170,by=w->ch/2-50;ui_fill(w,bx,by,340,100,0x0B0F24,14);rframe(w->cx+bx,w->cy+by,340,100,14,T.accent,200);
        ui_text(w,bx+(340-text_w(s->state==2?"GAME OVER":"COMET RUSH",2))/2,by+18,s->state==2?"GAME OVER":"COMET RUSH",0xFFFFFF,2);
        const char*m=s->state==2?"Click or press Space to retry":"Dodge rocks, grab stars. Click to start";ui_text(w,bx+(340-text_w(m,1))/2,by+56,m,0xA5B4FC,1);ui_text(w,bx+(340-text_w("Mouse or A / D to steer",1))/2,by+74,"Mouse or A / D to steer",0x8892B8,1);}
}
static void game_click(win_t*w,int x,int y,int b,int d){ST(game_t);(void)x;(void)y;(void)b;(void)d;if(s->state!=1){game_reset(s,w->cw);s->last=ticks;}}
static void game_key(win_t*w,int k,int m){ST(game_t);(void)m;if(k==' '&&s->state!=1){game_reset(s,w->cw);s->last=ticks;}
    if(k==K_LEFT||k=='a')s->px=MAX(14,s->px-22);if(k==K_RIGHT||k=='d')s->px=MIN(w->cw-14,s->px+22);}

/* ================================================== Settings =========== */
typedef struct{int page,scroll,nth,tn[12];u32*th[12];char pw[32];int pl;int focus,ul;char nm[12],upw[32];}set_t;
static void set_thumbs(win_t*w){
    ST(set_t);for(int i=0;i<12;i++){if(s->th[i]){kfree(s->th[i]);s->th[i]=0;}}s->nth=0;int k[96],n;int dirs[2]={fs_pics,fs_dl};
    for(int d=0;d<2;d++){n=fs_children(dirs[d],k,96);for(int i=0;i<n&&s->nth<12;i++)if(is_img(fsn[k[i]].name)){int iw,ih,own;u32*src=img_load(k[i],&iw,&ih,&own);if(src){s->th[s->nth]=make_thumb(src,iw,ih,150,94);s->tn[s->nth]=k[i];s->nth++;if(own)kfree(src);}}}
}
static void set_init(win_t*w){w->d=kzalloc(sizeof(set_t));}
static void set_close(win_t*w){ST(set_t);if(s)for(int i=0;i<12;i++)if(s->th[i])kfree(s->th[i]);}
static void sw_(win_t*w,int id,int x,int y,int on){ui_fill(w,x,y,44,22,on?T.accent:T.hover,11);circle_ex(w->cx+x+(on?33:11),w->cy+y+11,8,0,on?fgon(T.accent):T.dim,255);w_hit(w,x,y,44,22,id);}
static void set_draw(win_t*w){
    ST(set_t);ui_bg(w,T.win);rrect_ex(w->cx,w->cy,180,w->ch,10,T.win2,255,4,0);
    static const char*nav[6]={"Personalization","Wallpaper","Display","Accounts","Apps","System"};
    for(int i=0;i<6;i++){int y=16+i*38,on=s->page==i;if(on||w_hover(w,8,y,164,34))ui_fill(w,8,y,164,34,on?T.accent:T.hover,8);ui_text(w,22,y+13,nav[i],on?fgon(T.accent):T.text,1);w_hit(w,8,y,164,34,1+i);}
    int x=204;char b[96];int cw=w->cw-x-16;
    if(s->page==0){
        ui_text(w,x,20,"Personalization",T.text,2);ui_text(w,x,52,"Theme",T.dim,1);
        for(int i=0;i<2;i++){int cx=x+i*170,dk=i==0;int on=g_dark==dk;ui_fill(w,cx,70,156,96,dk?0x1C1F2B:0xFAFBFE,10);ui_fill(w,cx+14,84,100,10,dk?0x2A3044:0xE9EDF8,3);ui_fill(w,cx+14,102,70,10,T.accent,3);
            ui_fill(w,cx+14,122,128,28,dk?0x2A3044:0xE9EDF8,6);if(on){rframe(w->cx+cx-2,w->cy+68,160,100,12,T.accent,255);rframe(w->cx+cx-1,w->cy+69,158,98,11,T.accent,255);}
            ui_text(w,cx+4,176,dk?"Dark":"Light",T.text,1);w_hit(w,cx,70,156,96,10+i);}
        ui_text(w,x,206,"Accent color",T.dim,1);
        for(int i=0;i<8;i++){int cx=x+16+i*44;circle_ex(w->cx+cx,w->cy+240,16,0,accents[i],255);if(accents[i]==g_accent)circle_ex(w->cx+cx,w->cy+240,20,2,T.text,255);w_hit(w,cx-18,222,36,36,20+i);}
        ui_button(w,33,x+370,224,220,32,"Match accent to wallpaper",0);
        ui_text(w,x,286,"Window animations",T.text,1);sw_(w,30,x+300,280,g_anim);
        ui_text(w,x,322,"Night light (warm colors)",T.text,1);sw_(w,31,x+300,316,g_night);
        ui_text(w,x,358,"Night light schedule (8 PM - 7 AM)",T.text,1);sw_(w,32,x+300,352,g_nightsched);
    }else if(s->page==1){
        ui_text(w,x,20,"Wallpaper",T.text,2);ksnprintf(b,96,"Current: %s",g_wallnode>0?fsn[g_wallnode].name:"Comet default");ui_text(w,x,52,b,T.dim,1);
        ui_button(w,60,x,72,150,30,"Comet default",g_wallnode?0:1);
        for(int i=0;i<s->nth;i++){int tx=x+(i%4)*(cw/4),ty=118+(i/4)*118,tw=cw/4-10;
            if(s->th[i]){cliprect c=clip_push(w->cx+tx,w->cy+ty,150,94);blit_scaled(s->th[i],150,94,w->cx+tx,w->cy+ty,150,94);clip_pop(c);}
            if(s->tn[i]==g_wallnode)rframe(w->cx+tx-2,w->cy+ty-2,154,98,6,T.accent,255);else if(w_hover(w,tx,ty,150,94))rframe(w->cx+tx-1,w->cy+ty-1,152,96,4,T.text,160);
            char nm[20];scopy(nm,fsn[s->tn[i]].name,19);ui_text(w,tx,ty+100,nm,T.dim,1);w_hit(w,tx,ty,150,94,100+i);(void)tw;}
        if(!s->nth)ui_text(w,x,130,"No images yet. Save one from the Browser (Downloads), or draw one in Paint.",T.dim,1);
        ui_text(w,x,w->ch-34,"Supported: PNG, baseline JPEG, BMP and CIM. Open Photos for more.",T.dim,1);
    }else if(s->page==2){
        static const int rw[4]={1024,1280,1600,1920},rh[4]={768,800,900,1080};
        ui_text(w,x,20,"Display",T.text,2);ksnprintf(b,96,"Current resolution: %d x %d",SW,SH);ui_text(w,x,56,b,T.text,1);
        ui_text(w,x,80,bga_ok?"Display driver: VBE/BGA - live mode switching available":"Display driver: GRUB framebuffer (fixed mode)",T.dim,1);
        for(int i=0;i<4;i++){ksnprintf(b,96,"%d x %d",rw[i],rh[i]);ui_button(w,40+i,x+(i%2)*170,112+(i/2)*44,156,34,b,(SW==rw[i]&&SH==rh[i])?1:0);}
        ui_text(w,x,214,"Every layout in Comet OS is computed from the screen size,",T.dim,1);ui_text(w,x,230,"so any mode works. In VirtualBox use VBoxVGA as the",T.dim,1);ui_text(w,x,246,"graphics controller for live resolution switching.",T.dim,1);
    }else if(s->page==3){
        char nm[12];prof_display(cur_prof,nm);ui_text(w,x,20,"Accounts",T.text,2);ksnprintf(b,96,"Signed in as %s - %s",nm,cfg_haspw?"password protected":"no password");ui_text(w,x,52,b,T.text,1);
        ui_text(w,x,80,"Your password:",T.dim,1);ui_fill(w,x,98,240,30,T.field,10);if(s->focus==0)rframe(w->cx+x,w->cy+98,240,30,10,T.accent,255);w_hit(w,x,98,240,30,80);
        for(int i=0;i<s->pl;i++)circle_ex(w->cx+x+16+i*13,w->cy+113,4,0,T.text,255);if(s->focus==0&&((ticks/50)&1))fill_rect(w->cx+x+10+s->pl*13,w->cy+103,1,20,T.accent);
        ui_button(w,70,x+250,98,100,30,"Set",1);if(cfg_haspw)ui_button(w,71,x+356,98,100,30,"Remove",0);
        ui_text(w,x,150,"Users on this computer",T.dim,1);int yy=170;
        for(int k=0;k<4;k++)if(prof[k].used){prof_display(k,nm);ui_fill(w,x,yy,cw,34,T.win2,8);circle_ex(w->cx+x+20,w->cy+yy+17,10,0,accents[prof[k].accent&7],255);
            ksnprintf(b,96,"%s%s%s",nm,k==cur_prof?"  (you)":"",prof[k].has?"  - password":"");ui_text(w,x+40,yy+13,b,T.text,1);if(k>0&&k!=cur_prof)ui_button(w,90+k,x+cw-90,yy+3,80,28,"Remove",2);yy+=40;}
        ui_text(w,x,yy+10,"Add a user (switch users from the lock screen):",T.dim,1);
        ui_fill(w,x,yy+30,150,30,T.field,10);if(s->focus==1)rframe(w->cx+x,w->cy+yy+30,150,30,10,T.accent,255);ui_text(w,x+10,yy+41,s->nm[0]?s->nm:"name",s->nm[0]?T.text:T.dim,1);w_hit(w,x,yy+30,150,30,81);
        ui_fill(w,x+160,yy+30,150,30,T.field,10);if(s->focus==2)rframe(w->cx+x+160,w->cy+yy+30,150,30,10,T.accent,255);for(int i=0;i<s->ul;i++)circle_ex(w->cx+x+174+i*12,w->cy+yy+45,4,0,T.text,255);if(!s->ul)ui_text(w,x+170,yy+41,"password (optional)",T.dim,1);w_hit(w,x+160,yy+30,150,30,82);
        ui_button(w,72,x+320,yy+30,110,30,"Add user",1);
        ui_text(w,x,yy+76,"The lock screen appears at startup and with Win+L. Passwords are stored as hashes.",T.dim,1);
    }else if(s->page==4){
        ui_text(w,x,20,"Apps",T.text,2);ui_text(w,x,50,"Remove what you don't use. Settings, Files, Store and Task Manager are required.",T.dim,1);
        cliprect c=clip_push(w->cx+x-8,w->cy+76,cw+16,w->ch-80);int y=80-s->scroll;
        for(int i=0;i<APP_COUNT;i++){if(i==APP_SCRIPT)continue;
            if(y>40&&y<w->ch){ui_fill(w,x,y,cw,38,T.win2,8);draw_icon(i,w->cx+x+8,w->cy+y+7,24);ui_text(w,x+44,y+8,apps[i].name,apps[i].installed?T.text:T.dim,1);
                ksnprintf(b,96,"%s",apps[i].installed?"":"removed - ");ksnprintf(b+strlen(b),40,"~%d KB when open",app_mem_kb(i));ui_text(w,x+44,y+22,b,T.dim,1);
                if(!app_removable(i))ui_text(w,x+cw-90,y+15,"Required",T.dim,1);
                else if(apps[i].installed)ui_button(w,200+i,x+cw-96,y+5,86,28,"Remove",2);else ui_button(w,300+i,x+cw-96,y+5,86,28,"Reinstall",1);}
            y+=44;}
        clip_pop(c);
    }else{
        ui_text(w,x,20,"System",T.text,2);int y=60;
        ksnprintf(b,96,"Comet OS 1.0 (Perseid), i686");ui_text(w,x,y,b,T.text,1);y+=20;ksnprintf(b,96,"CPU: %s",cpu_brand);ui_text(w,x,y,b,T.dim,1);y+=16;
        ksnprintf(b,96,"Memory: %d MB    Heap in use: %d KB",mem_total_kb/1024,heap_used()/1024);ui_text(w,x,y,b,T.dim,1);y+=16;
        ksnprintf(b,96,"Network: %s - %s",net.nicname,net_status());ui_text(w,x,y,b,T.dim,1);y+=16;
        ksnprintf(b,96,"Install state: %s",g_installed?"installed on disk":"live session");ui_text(w,x,y,b,T.dim,1);y+=16;
        for(int i=0;i<4;i++)if(ata[i].present){ksnprintf(b,96,"%s: %s",ata[i].atapi?"CD":"HD",ata[i].model);ui_text(w,x,y,b,T.dim,1);y+=16;}
        y+=10;ui_text(w,x,y,"Input: PS/2 keyboard and mouse (set VirtualBox pointing device to PS/2 Mouse).",T.dim,1);y+=16;
        ui_text(w,x,y,"Audio: PC speaker only (Music app). No sound card driver yet.",T.dim,1);y+=16;
        ui_text(w,x,y,"Guest Additions: not supported by this kernel (see README).",T.dim,1);y+=30;
        ui_button(w,50,x,y,200,32,"Open Comet Setup",1);
    }
}
static void set_action(win_t*w,int id,int d){
    ST(set_t);(void)d;static const int rw[4]={1024,1280,1600,1920},rh[4]={768,800,900,1080};
    if(id>=1&&id<=6){s->page=id-1;s->scroll=0;if(s->page==1)set_thumbs(w);}
    else if(id==10||id==11){g_dark=id==10;theme_apply();cfg_theme=g_dark;cfg_save();}
    else if(id>=20&&id<28){g_accent=accents[id-20];theme_apply();cfg_accent=id-20;cfg_save();}
    else if(id==30)g_anim=!g_anim;else if(id==31)g_night=!g_night;else if(id==32){g_nightsched=!g_nightsched;cfg_nightsched=g_nightsched;cfg_save();}
    else if(id==33)wallpaper_accent();
    else if(id>=40&&id<44){if(!ui_res_set(rw[id-40],rh[id-40]))toast("Resolution switching needs the VBoxVGA controller");}
    else if(id==50)win_open(APP_SETUP);
    else if(id==60){g_wallnode=0;theme_apply();toast("Default wallpaper");}
    else if(id>=100&&id<112){int i=id-100;if(i<s->nth)wallpaper_set_node(s->tn[i]);}
    else if(id==70){pw_set(s->pw);toast(s->pl?"Password set":"Enter a password first");s->pl=0;s->pw[0]=0;}
    else if(id==71){pw_set("");toast("Password removed");}
    else if(id>=80&&id<=82)s->focus=id-80;
    else if(id==72){int r=prof_add(s->nm,s->upw);toast(r>0?"User added":(r==-3?"That name is taken":(r==-2?"Use letters and numbers (max 10)":"No free user slots")));if(r>0){s->nm[0]=0;s->upw[0]=0;s->ul=0;}}
    else if(id>=91&&id<=93){prof_remove(id-90);toast("User removed");}
    else if(id>=200&&id<200+APP_COUNT)app_uninstall(id-200);
    else if(id>=300&&id<300+APP_COUNT){app_install(id-300);toast("Reinstalled");}
}
static void set_key(win_t*w,int k,int m){
    ST(set_t);(void)m;if(s->page!=3)return;
    if(k=='\t'){s->focus=(s->focus+1)%3;return;}
    if(s->focus==0){if(k=='\b'){if(s->pl)s->pw[--s->pl]=0;}else if(k=='\n')set_action(w,70,0);else if(k>=32&&k<127&&s->pl<28){s->pw[s->pl++]=k;s->pw[s->pl]=0;}}
    else if(s->focus==1){int l=strlen(s->nm);if(k=='\b'){if(l)s->nm[l-1]=0;}else if(k=='\n')s->focus=2;else if(k>=32&&k<127&&l<10){s->nm[l]=k;s->nm[l+1]=0;}}
    else{if(k=='\b'){if(s->ul)s->upw[--s->ul]=0;}else if(k=='\n')set_action(w,72,0);else if(k>=32&&k<127&&s->ul<28){s->upw[s->ul++]=k;s->upw[s->ul]=0;}}
}
static void set_scroll(win_t*w,int dz){ST(set_t);if(s->page==4)s->scroll=CLAMP(s->scroll+dz*30,0,MAX(0,APP_COUNT*44-(w->ch-90)));}

/* ================================================== Setup (installer) == */
typedef struct{int step;}setup_t;
static void setup_init(win_t*w){w->d=kzalloc(sizeof(setup_t));inst_detect();}
static void setup_draw(win_t*w){
    ST(setup_t);ui_bg(w,T.win);fill_grad(w->cx,w->cy,w->cw,86,blend(T.accent,0x000000,70),0x0E7490);
    ui_text(w,24,18,"Install Comet OS",0xFFFFFF,3);ui_text(w,24,54,"Copy this live system to a hard disk so it boots on its own.",0xDFF7FF,1);
    char b[96];int y=108;
    ui_fill(w,20,y,w->cw-40,76,T.win2,10);ui_text(w,36,y+14,"Source",T.dim,1);ui_text(w,36,y+30,inst.src>=0?ata[inst.src].model:"(none)",T.text,1);
    ui_text(w,36,y+48,inst.dst>=0?"Target disk":"",T.dim,1);if(inst.dst>=0){ksnprintf(b,96,"%s  -  %d MB",ata[inst.dst].model,ata[inst.dst].sectors/2048);ui_text(w,130,y+48,b,T.text,1);}
    y+=96;
    if(inst.state==1){ui_text(w,24,y,"Installing...",T.text,2);int pc=pct(inst.done,inst.total);ui_bar(w,24,y+34,w->cw-48,16,pc,T.accent);ksnprintf(b,96,"%d%%  (%d / %d MB)",pc,inst.done/512,inst.total/512);ui_text(w,24,y+60,b,T.dim,1);ui_text(w,24,y+80,"Do not close this window or power off the VM.",T.dim,1);}
    else if(inst.state==2){ui_text(w,24,y,"All done!",T.good,2);ui_text(w,24,y+30,inst.msg,T.text,1);ui_text(w,24,y+50,"Eject the ISO in VirtualBox (Devices > Optical Drives), then restart.",T.dim,1);ui_button(w,4,24,y+80,160,34,"Restart now",1);}
    else if(inst.state==3){ui_text(w,24,y,"Something went wrong",T.bad,2);ui_text(w,24,y+30,inst.msg,T.text,1);ui_button(w,5,24,y+60,120,32,"Try again",0);}
    else if(s->step==0){ui_text(w,24,y,inst.msg,inst.src>=0&&inst.dst>=0?T.text:T.bad,1);ui_text(w,24,y+22,"The installer writes the live image to the whole target disk.",T.dim,1);
        if(inst.src>=0&&inst.dst>=0)ui_button(w,1,24,y+52,180,34,"Install Comet OS",1);}
    else{ui_text(w,24,y,"Erase the disk and install?",T.bad,2);ksnprintf(b,96,"Everything on \"%s\" will be permanently erased.",ata[inst.dst].model);ui_text(w,24,y+30,b,T.text,1);
        ui_button(w,2,24,y+60,200,34,"Erase disk and install",2);ui_button(w,3,232,y+60,100,34,"Cancel",0);}
}
static void setup_action(win_t*w,int id,int d){ST(setup_t);(void)d;
    if(id==1)s->step=1;else if(id==2){s->step=0;inst_start();}else if(id==3)s->step=0;else if(id==4)wm_power(2);else if(id==5){inst.state=0;s->step=0;inst_detect();}}

/* ================================================== Calculator ========= */
typedef struct{int acc,op,fresh,err,el,dot,decs;char ent[16];}calc_t;
static int cparse(const char*e){int neg=0,i=0,v=0,dec=0,dd=0,seen=0;if(e[0]=='-'){neg=1;i=1;}for(;e[i];i++){if(e[i]=='.'){seen=1;continue;}if(seen){if(dd<2){dec=dec*10+(e[i]-'0');dd++;}}else v=v*10+(e[i]-'0');}while(dd<2){dec*=10;dd++;}v=v*100+dec;return neg?-v:v;}
static void cfmt(int v,char*o){int neg=v<0;if(neg)v=-v;int ip=v/100,fp=v%100;char t[16];if(fp==0)ksnprintf(t,16,"%d",ip);else if(fp%10==0)ksnprintf(t,16,"%d.%d",ip,fp/10);else ksnprintf(t,16,"%d.%02d",ip,fp);ksnprintf(o,16,"%s%s",neg?"-":"",t);}
static int cmul(int a,int b){int neg=(a<0)^(b<0);if(a<0)a=-a;if(b<0)b=-b;int r=(a/100)*b+(a%100)*b/100;return neg?-r:r;}
static int cdiv(int a,int b){int neg=(a<0)^(b<0);if(a<0)a=-a;if(b<0)b=-b;int r=(a/b)*100+(a%b)*100/b;return neg?-r:r;}
static void ceval(calc_t*s){int b=cparse(s->ent),r=b;
    if(s->op=='+')r=s->acc+b;else if(s->op=='-')r=s->acc-b;else if(s->op=='*')r=cmul(s->acc,b);else if(s->op=='/'){if(!b){s->err=1;r=0;}else r=cdiv(s->acc,b);}
    if(s->op){cfmt(r,s->ent);s->el=strlen(s->ent);s->acc=r;}}
static void cpress(calc_t*s,int c){
    if(s->err&&c!='C'){return;}
    if((c>='0'&&c<='9')||c=='.'){if(s->fresh){s->ent[0]=0;s->el=0;s->dot=0;s->decs=0;s->fresh=0;}if(c=='.'){if(s->dot)return;s->dot=1;if(!s->el){s->ent[s->el++]='0';}}else if(s->dot){if(s->decs>=2)return;s->decs++;}if(s->el<10){s->ent[s->el++]=c;s->ent[s->el]=0;}}
    else if(c=='+'||c=='-'||c=='*'||c=='/'){if(s->op&&!s->fresh)ceval(s);s->acc=cparse(s->ent);s->op=c;s->fresh=1;}
    else if(c=='='){ceval(s);s->op=0;s->fresh=1;}
    else if(c=='C'){memset(s,0,sizeof *s);s->ent[0]='0';s->el=1;}
    else if(c=='N'){int v=-cparse(s->ent);cfmt(v,s->ent);s->el=strlen(s->ent);}
    else if(c=='%'){int v=cparse(s->ent)/100;cfmt(v,s->ent);s->el=strlen(s->ent);s->fresh=1;}
    else if(c=='\b'){if(s->el>1)s->ent[--s->el]=0;else{s->ent[0]='0';s->el=1;}}
}
static void calc_init(win_t*w){calc_t*s=kzalloc(sizeof(calc_t));w->d=s;s->ent[0]='0';s->el=1;}
static void calc_draw(win_t*w){
    ST(calc_t);ui_bg(w,T.win);ui_fill(w,16,14,w->cw-32,76,T.field,12);
    char b[24];if(s->op){cfmt(s->acc,b);char t[28];ksnprintf(t,28,"%s %c",b,s->op=='*'?'x':s->op);ui_text(w,w->cw-28-text_w(t,1),24,t,T.dim,1);}
    const char*d=s->err?"Error":s->ent;ui_text(w,w->cw-28-text_w(d,3),50,d,T.text,3);
    static const char*lab[]={"C","+/-","%","/","7","8","9","x","4","5","6","-","1","2","3","+","0",".","=",""};static const int code[]={'C','N','%','/','7','8','9','*','4','5','6','-','1','2','3','+','0','.','=',0};
    int bw=(w->cw-32-18)/4,bh=(w->ch-110-16-24)/5;
    for(int i=0;i<20;i++){if(!code[i])continue;int x=16+(i%4)*(bw+6),y=104+(i/4)*(bh+6);int op=(i%4==3)||code[i]=='=';ui_button(w,code[i],x,y,bw,bh,lab[i],code[i]=='='?1:(op?0:0));}
}
static void calc_action(win_t*w,int id,int d){ST(calc_t);(void)d;cpress(s,id);}
static void calc_key(win_t*w,int k,int m){ST(calc_t);(void)m;if(k=='\n')k='=';if(k=='c')k='C';if((k>='0'&&k<='9')||k=='.'||k=='+'||k=='-'||k=='*'||k=='/'||k=='='||k=='C'||k=='\b'||k=='%')cpress(s,k);}

/* ================================================== Paint ============== */
#define PW 640
#define PH 420
typedef struct{u32*cv;int col,size,lx,ly,draw,eraser;}paint_t;
static const u32 pal[12]={0x000000,0xFFFFFF,0xEF4444,0xF97316,0xFACC15,0x22C55E,0x14B8A6,0x3B82F6,0x8B5CF6,0xEC4899,0x92400E,0x6B7280};
static void paint_init(win_t*w){paint_t*s=kzalloc(sizeof(paint_t));w->d=s;s->cv=kmalloc(PW*PH*4);memset32(s->cv,0xFFFFFF,PW*PH);s->col=0;s->size=3;w->mem=1100;}
static void paint_close(win_t*w){ST(paint_t);if(s&&s->cv)kfree(s->cv);}
static void pdot(paint_t*s,int x,int y){u32 c=s->eraser?0xFFFFFF:pal[s->col];int r=s->size;for(int j=-r;j<=r;j++)for(int i=-r;i<=r;i++)if(i*i+j*j<=r*r){int px=x+i,py=y+j;if(px>=0&&px<PW&&py>=0&&py<PH)s->cv[py*PW+px]=c;}}
static void pline(paint_t*s,int x0,int y0,int x1,int y1){int n=MAX(ABS(x1-x0),ABS(y1-y0));if(!n)n=1;for(int i=0;i<=n;i++)pdot(s,x0+(x1-x0)*i/n,y0+(y1-y0)*i/n);}
static void paint_draw(win_t*w){
    ST(paint_t);ui_bg(w,T.win);
    for(int i=0;i<12;i++){int x=14+i*28;ui_fill(w,x,10,22,22,pal[i],11);if(i==s->col&&!s->eraser)rframe(w->cx+x-3,w->cy+7,28,28,14,T.text,255);w_hit(w,x,10,22,22,10+i);}
    ui_button(w,30,360,8,30,26,"S",s->size==1);ui_button(w,31,394,8,30,26,"M",s->size==3);ui_button(w,32,428,8,30,26,"L",s->size==8);
    ui_button(w,40,468,8,70,26,"Eraser",s->eraser?1:0);ui_button(w,41,544,8,56,26,"Clear",0);ui_button(w,42,606,8,56,26,"Save",1);
    cliprect c=clip_push(w->cx+16,w->cy+48,w->cw-32,w->ch-60);blit_scaled(s->cv,PW,PH,w->cx+16,w->cy+48,PW,PH);clip_pop(c);
}
static void paint_action(win_t*w,int id,int d){ST(paint_t);(void)d;
    if(id>=10&&id<22){s->col=id-10;s->eraser=0;}else if(id>=30&&id<=32){s->size=id==30?1:(id==31?3:8);}
    else if(id==40)s->eraser=!s->eraser;else if(id==41)memset32(s->cv,0xFFFFFF,PW*PH);
    else if(id==42){if(!img_save_cim(fs_pics,"painting.cim",s->cv,PW,PH))toast("Saved to Pictures");else toast("Could not save");}}
static void paint_click(win_t*w,int x,int y,int b,int d){ST(paint_t);(void)w;(void)d;if(b!=1)return;if(x<16||y<48)return;s->draw=1;s->lx=x-16;s->ly=y-48;pdot(s,s->lx,s->ly);}
static void paint_drag(win_t*w,int x,int y){ST(paint_t);if(!s->draw)return;int nx=x-16,ny=y-48;pline(s,s->lx,s->ly,nx,ny);s->lx=nx;s->ly=ny;}
static void paint_release(win_t*w,int x,int y){ST(paint_t);(void)x;(void)y;s->draw=0;}

/* ================================================== Snake ============== */
#define SGW 28
#define SGH 18
typedef struct{int bx[SGW*SGH],by[SGW*SGH],len,dir,ndir,fx,fy,state,score,best;u32 last;}snake_t;
static void snake_food(snake_t*s){for(int t=0;t<200;t++){s->fx=rnd()%SGW;s->fy=rnd()%SGH;int ok=1;for(int i=0;i<s->len;i++)if(s->bx[i]==s->fx&&s->by[i]==s->fy)ok=0;if(ok)return;}}
static void snake_reset(snake_t*s){s->len=4;for(int i=0;i<4;i++){s->bx[i]=8-i;s->by[i]=9;}s->dir=s->ndir=0;s->score=0;s->state=1;s->last=ticks;snake_food(s);}
static void snake_init(win_t*w){w->d=kzalloc(sizeof(snake_t));}
static void snake_tick(win_t*w){
    ST(snake_t);if(s->state!=1)return;int speed=MAX(5,11-s->score/60);if(ticks-s->last<(u32)speed)return;s->last=ticks;
    s->dir=s->ndir;static const int dx[4]={1,0,-1,0},dy[4]={0,1,0,-1};int nx=(s->bx[0]+dx[s->dir]+SGW)%SGW,ny=(s->by[0]+dy[s->dir]+SGH)%SGH;
    for(int i=0;i<s->len-1;i++)if(s->bx[i]==nx&&s->by[i]==ny){s->state=2;if(s->score>s->best)s->best=s->score;wm_redraw();return;}
    int eat=(nx==s->fx&&ny==s->fy);if(eat&&s->len<SGW*SGH-1)s->len++;
    for(int i=s->len-1;i>0;i--){s->bx[i]=s->bx[i-1];s->by[i]=s->by[i-1];}s->bx[0]=nx;s->by[0]=ny;
    if(eat){s->score+=10;snake_food(s);}wm_redraw();
}
static void snake_draw(win_t*w){
    ST(snake_t);ui_bg(w,T.win);int ox=(w->cw-SGW*20)/2,oy=MAX(40,(w->ch-SGH*20)/2+14);char b[48];
    ksnprintf(b,48,"Score %d    Best %d",s->score,s->best);ui_text(w,ox,14,b,T.text,2);
    ui_fill(w,ox-6,oy-6,SGW*20+12,SGH*20+12,T.win2,10);
    for(int i=s->len-1;i>=0;i--){u32 c=blend(T.accent,T.win2,i*120/MAX(s->len,1));ui_fill(w,ox+s->bx[i]*20+1,oy+s->by[i]*20+1,18,18,c,i==0?8:5);}
    circle_ex(w->cx+ox+s->fx*20+10,w->cy+oy+s->fy*20+10,8,0,0xF87171,255);circle_ex(w->cx+ox+s->fx*20+8,w->cy+oy+s->fy*20+7,2,0,0xFFFFFF,200);
    if(s->state!=1){const char*m=s->state==2?"Game over - click or Space to retry":"Arrow keys / WASD - click or Space to start";ui_fill(w,ox+40,oy+SGH*10-24,SGW*20-80,48,0x0B0F24,12);ui_text(w,ox+(SGW*20-text_w(m,1))/2,oy+SGH*10-4,m,0xFFFFFF,1);}
}
static void snake_key(win_t*w,int k,int m){
    ST(snake_t);(void)m;if(k==' '&&s->state!=1){snake_reset(s);return;}
    int d=-1;if(k==K_RIGHT||k=='d')d=0;else if(k==K_DOWN||k=='s')d=1;else if(k==K_LEFT||k=='a')d=2;else if(k==K_UP||k=='w')d=3;
    if(d>=0&&((d+2)%4!=s->dir||s->len<2)){s->ndir=d;if(s->state==0)snake_reset(s),s->ndir=d;}
}
static void snake_click(win_t*w,int x,int y,int b,int d){ST(snake_t);(void)x;(void)y;(void)b;(void)d;if(s->state!=1)snake_reset(s);}


/* ================================================== Meteorite ========== */
/* Native Comet OS edition of the Meteorite Kilo dashboard: overview, system, productivity, search, utilities, settings */
#define MT 24
typedef struct{int page,ntodo,nrem,il,ql,nl;char todo[MT][48];u8 done[MT];char in[44],q[32];struct{char text[40];u32 due;int fired;}rem[8];char num[16];char pw[20];int sres[10],nsres;}met_t;
static int met_file(void){int n=fs_find(fs_docs,"meteorite-tasks.txt");if(n<0)n=fs_create(fs_docs,"meteorite-tasks.txt",0);return n;}
static void met_save(met_t*s){char b[MT*52+8];int o=0;for(int i=0;i<s->ntodo;i++)o+=ksnprintf(b+o,sizeof(b)-o,"%c %s\n",s->done[i]?'x':'-',s->todo[i]);fs_write(met_file(),b,o);}
static void met_load(met_t*s){
    int n=met_file();s->ntodo=0;
    if(n>0&&fsn[n].size){const char*d=(const char*)fsn[n].data;u32 sz=fsn[n].size,i=0;
        while(i<sz&&s->ntodo<MT){char ln[48];int l=0;u8 dn=d[i]=='x';i+=2;while(i<sz&&d[i]!='\n'&&l<47)ln[l++]=d[i++];while(i<sz&&d[i]!='\n')i++;i++;ln[l]=0;if(l){scopy(s->todo[s->ntodo],ln,48);s->done[s->ntodo++]=dn;}}}
    else{const char*seed[3]={"Try snap layouts (hover maximize)","Open the Store and install Paint","Install Comet OS to disk"};for(int i=0;i<3;i++){scopy(s->todo[i],seed[i],48);s->done[i]=0;}s->ntodo=3;met_save(s);}
}
static void met_init(win_t*w){met_t*s=kzalloc(sizeof(met_t));w->d=s;met_load(s);scopy(s->num,"255",16);w->mem=48;}
static void mcard(win_t*w,int x,int y,int cw,int ch){ui_fill(w,x,y,cw,ch,T.win2,12);}
static void met_add(met_t*s){if(s->il&&s->ntodo<MT){scopy(s->todo[s->ntodo],s->in,48);s->done[s->ntodo++]=0;s->il=0;s->in[0]=0;met_save(s);}}
static void met_rem(met_t*s,int secs){if(s->nrem>=8)return;scopy(s->rem[s->nrem].text,s->il?s->in:"Reminder",40);s->rem[s->nrem].due=ticks+secs*100;s->rem[s->nrem].fired=0;s->nrem++;if(s->il){s->il=0;s->in[0]=0;}toast("Reminder set");}
static void met_draw(win_t*w){
    ST(met_t);ui_bg(w,T.win);rrect_ex(w->cx,w->cy,190,w->ch,10,T.win2,255,4,0);
    ui_text(w,22,20,"Meteorite",T.text,2);ui_text(w,22,42,"Comet edition",T.dim,1);
    static const char*nav[6]={"Overview","System","Productivity","Search","Utilities","Settings"};
    for(int i=0;i<6;i++){int y=72+i*38,on=s->page==i;if(on||w_hover(w,10,y,170,34))ui_fill(w,10,y,170,34,on?T.accent:T.hover,8);
        ui_text(w,26,y+13,nav[i],on?fgon(T.accent):T.text,1);w_hit(w,10,y,170,34,1+i);}
    int x=210,cw=w->cw-x-18,h,m,sec,d,mo,y;rtc_read(&h,&m,&sec,&d,&mo,&y);char b[96];
    int mp=pct(heap_used(),heap_total());
    if(s->page==0){
        ksnprintf(b,96,"Good %s, Guest",h<12?"morning":(h<18?"afternoon":"evening"));ui_text(w,x,20,b,T.text,2);ui_text(w,x,46,"System, tasks and everyday tools at a glance.",T.dim,1);
        int c3=(cw-24)/3;
        mcard(w,x,72,c3,160);ui_text(w,x+14,86,"CPU",T.dim,1);gauge(w->cx+x+c3/2,w->cy+158,42,cpu_pct,T.accent,T.dim);ksnprintf(b,96,"%d%%",cpu_pct);ui_text(w,x+c3/2-text_w(b,2)/2,152,b,T.text,2);
        mcard(w,x+c3+12,72,c3,160);ui_text(w,x+c3+26,86,"Memory",T.dim,1);gauge(w->cx+x+c3+12+c3/2,w->cy+158,42,mp,0xF472B6,T.dim);ksnprintf(b,96,"%d%%",mp);ui_text(w,x+c3+12+c3/2-text_w(b,2)/2,152,b,T.text,2);
        int open=0;for(int i=0;i<s->ntodo;i++)open+=!s->done[i];
        mcard(w,x+2*(c3+12),72,c3,160);ui_text(w,x+2*(c3+12)+14,86,"Tasks",T.dim,1);ksnprintf(b,96,"%d",open);ui_text(w,x+2*(c3+12)+14,116,b,T.accent2,4);ksnprintf(b,96,"open of %d",s->ntodo);ui_text(w,x+2*(c3+12)+14,160,b,T.dim,1);
        ksnprintf(b,96,"%d:%02d %s",h%12?h%12:12,m,h<12?"AM":"PM");ui_text(w,x+2*(c3+12)+14,184,b,T.text,2);
        mcard(w,x,246,cw,132);ui_text(w,x+14,260,"Quick launch",T.dim,1);
        static const int ql[6]={APP_FILES,APP_TERM,APP_NOTES,APP_VIEW,APP_STORE,APP_TASK};int tw=(cw-28)/6;
        for(int i=0;i<6;i++){int tx=x+14+i*tw;if(w_hover(w,tx,278,tw-6,84))ui_fill(w,tx,278,tw-6,84,T.hover,10);draw_icon(ql[i],w->cx+tx+(tw-6-40)/2,w->cy+288,40);ui_text(w,tx+(tw-6-text_w(apps[ql[i]].name,1))/2,336,apps[ql[i]].name,T.text,1);w_hit(w,tx,278,tw-6,84,400+ql[i]);}
        mcard(w,x,392,cw,110);ui_text(w,x+14,406,"Up next",T.dim,1);const char*nx="Nothing scheduled";for(int i=0;i<s->ntodo;i++)if(!s->done[i]){nx=s->todo[i];break;}
        for(int i=0;i<s->nrem;i++)if(!s->rem[i].fired){ksnprintf(b,96,"Reminder in %ds: %s",(s->rem[i].due>ticks?(s->rem[i].due-ticks)/100:0),s->rem[i].text);nx=b;break;}
        ui_text(w,x+14,434,nx,T.text,2);u32 up=ticks/100;char ub[48];ksnprintf(ub,48,"Uptime %dh %dm %ds    %dx%d",up/3600,(up/60)%60,up%60,SW,SH);ui_text(w,x+14,470,ub,T.dim,1);ksnprintf(b,96,"Network: %s (%s)  rx %d tx %d",net_status(),net.nicname,net.rx,net.tx);ui_text(w,x+14,486,b,T.dim,1);
        ui_text(w,x,520,"Workspaces",T.dim,1);ui_button(w,820,x+110,512,90,28,"Study",0);ui_button(w,821,x+206,512,90,28,"Coding",0);ui_button(w,822,x+302,512,90,28,"Focus",0);
    }else if(s->page==1){
        ui_text(w,x,20,"System",T.text,2);ui_text(w,x,46,"Live performance, processes and quick actions.",T.dim,1);
        mcard(w,x,72,cw,190);ksnprintf(b,96,"CPU  %d%%   -   %s",cpu_pct,cpu_brand);ui_text(w,x+14,86,b,T.text,1);
        int gx=x+14,gy=108,gw=cw-28,gh=140;ui_fill(w,gx,gy,gw,gh,T.field,8);cliprect c=clip_push(w->cx+gx,w->cy+gy,gw,gh);
        for(int i=0;i<60;i++){int bh=cpu_hist[i]*gh/100,bx=w->cx+gx+i*gw/60;fill_rect_a(bx,w->cy+gy+gh-bh,gw/60+1,bh,T.accent,150);if(bh)fill_rect(bx,w->cy+gy+gh-bh,gw/60+1,2,T.accent2);}clip_pop(c);
        mcard(w,x,276,cw,w->ch-276-18);ui_text(w,x+14,290,"Top processes",T.dim,1);int pids[40];char nm[40][24];int n=proc_list(pids,nm,40);
        for(int i=0;i<n&&i<8;i++){u32 cp,me;proc_info(pids[i],&cp,&me);int ry=314+i*24;if(ry>w->ch-60)break;ui_text(w,x+14,ry,nm[i],T.text,1);ksnprintf(b,96,"%d.%d%%",cp/10,cp%10);ui_text(w,x+cw-200,ry,b,T.dim,1);ksnprintf(b,96,"%d KB",me);ui_text(w,x+cw-120,ry,b,T.dim,1);}
        ui_button(w,700,x+cw-150,w->ch-62,136,30,"Task Manager",0);
    }else if(s->page==2){
        ui_text(w,x,20,"Productivity",T.text,2);ui_text(w,x,46,"Tasks and reminders. Saved to Documents/meteorite-tasks.txt.",T.dim,1);
        ui_fill(w,x,72,cw-100,34,T.field,8);ui_text(w,x+12,85,s->il?s->in:"Type a task and press Enter...",s->il?T.text:T.dim,1);if(s->il||((ticks/50)&1))fill_rect(w->cx+x+12+s->il*8,w->cy+82,1,14,T.accent);
        ui_button(w,90,x+cw-92,72,92,34,"Add",1);
        for(int i=0;i<s->ntodo;i++){int ry=120+i*28;if(ry>w->ch-170)break;if(w_hover(w,x,ry,cw,26))ui_fill(w,x,ry,cw,26,T.hover,6);
            circle_ex(w->cx+x+14,w->cy+ry+13,8,s->done[i]?0:2,s->done[i]?T.accent:T.dim,255);w_hit(w,x+4,ry,20,26,100+i);
            ui_text(w,x+32,ry+9,s->todo[i],s->done[i]?T.dim:T.text,1);if(s->done[i])fill_rect(w->cx+x+32,w->cy+ry+13,strlen(s->todo[i])*8,1,T.dim);
            ui_text(w,x+cw-22,ry+9,"x",T.dim,1);w_hit(w,x+cw-30,ry,28,26,200+i);}
        int ry=w->ch-160;fill_rect_a(w->cx+x,w->cy+ry-8,cw,1,T.dim,60);ui_text(w,x,ry,"Reminders",T.dim,1);
        ui_button(w,91,x+100,ry-8,80,26,"in 30s",0);ui_button(w,92,x+186,ry-8,80,26,"in 1m",0);ui_button(w,93,x+272,ry-8,80,26,"in 5m",0);
        for(int i=0;i<s->nrem&&i<4;i++){int yy=ry+24+i*22;if(s->rem[i].fired)ksnprintf(b,96,"done   %s",s->rem[i].text);else ksnprintf(b,96,"%4ds   %s",(s->rem[i].due>ticks?(s->rem[i].due-ticks)/100:0),s->rem[i].text);ui_text(w,x,yy,b,s->rem[i].fired?T.dim:T.text,1);}
    }else if(s->page==3){
        ui_text(w,x,20,"Search",T.text,2);ui_text(w,x,46,"Apps and files.",T.dim,1);
        ui_fill(w,x,72,cw,38,T.field,10);ui_text(w,x+14,87,s->ql?s->q:"Search apps and files...",s->ql?T.text:T.dim,1);if(s->ql||((ticks/50)&1))fill_rect(w->cx+x+14+s->ql*8,w->cy+84,1,14,T.accent);
        s->nsres=0;if(s->ql){for(int i=0;i<APP_COUNT&&s->nsres<10;i++)if(apps[i].installed&&i!=APP_SCRIPT&&stricontains(apps[i].name,s->q))s->sres[s->nsres++]=i;int f[8];int n=fs_search(s->q,f,8);for(int i=0;i<n&&s->nsres<10;i++)s->sres[s->nsres++]=1000+f[i];}
        for(int i=0;i<s->nsres;i++){int ry=124+i*44;if(w_hover(w,x,ry,cw,40))ui_fill(w,x,ry,cw,40,T.hover,8);
            if(s->sres[i]<1000){draw_icon(s->sres[i],w->cx+x+8,w->cy+ry+6,28);ui_text(w,x+48,ry+10,apps[s->sres[i]].name,T.text,1);ui_text(w,x+48,ry+24,"App",T.dim,1);}
            else{int fn=s->sres[i]-1000;char p[64];fs_path(fn,p,64);icon_color(T.text,T.win);draw_icon(IC_FILE,w->cx+x+14,w->cy+ry+8,22);ui_text(w,x+48,ry+10,fsn[fn].name,T.text,1);ui_text(w,x+48,ry+24,p,T.dim,1);}
            w_hit(w,x,ry,cw,40,300+i);}
        if(s->ql&&!s->nsres)ui_text(w,x,128,"No matches.",T.dim,1);
    }else if(s->page==4){
        ui_text(w,x,20,"Utilities",T.text,2);ui_text(w,x,46,"Launchers, number converter and password generator.",T.dim,1);
        mcard(w,x,72,cw,150);ui_text(w,x+14,86,"Launcher",T.dim,1);int tw=(cw-28)/7,k=0;
        for(int i=0;i<APP_COUNT;i++){if(!apps[i].installed||i==APP_METEORITE||i==APP_SCRIPT)continue;int tx=x+14+(k%7)*tw,ty=106+(k/7)*58;if(w_hover(w,tx,ty,tw-4,54))ui_fill(w,tx,ty,tw-4,54,T.hover,8);
            draw_icon(i,w->cx+tx+(tw-4-28)/2,w->cy+ty+4,28);char nm[10];scopy(nm,apps[i].name,(tw-4)/8+1);ui_text(w,tx+(tw-4-text_w(nm,1))/2,ty+38,nm,T.text,1);w_hit(w,tx,ty,tw-4,54,400+i);k++;}
        mcard(w,x,236,cw,120);ui_text(w,x+14,250,"Number converter",T.dim,1);int v=atoi_(s->num);
        ksnprintf(b,96,"dec %d    hex %x    bin ",v,(u32)v);{char t[40];int o=0;for(int i=15;i>=0;i--)if((v>>i)&1||o||i==0)t[o++]=((v>>i)&1)?'1':'0';t[o]=0;char z[140];ksnprintf(z,140,"%s%s",b,t);
            ui_fill(w,x+14,272,200,30,T.field,8);ui_text(w,x+24,283,s->num,T.text,1);ui_text(w,x+14,314,z,T.text,1);}
        ui_button(w,810,x+224,272,70,30,"Clear",0);
        mcard(w,x,370,cw,90);ui_text(w,x+14,384,"Password generator",T.dim,1);ui_button(w,800,x+14,404,110,30,"Generate",1);ui_text(w,x+136,415,s->pw[0]?s->pw:"Click Generate",T.text,1);
        ui_text(w,x,w->ch-34,"Type digits on this page to use the converter.",T.dim,1);
    }else{
        ui_text(w,x,20,"Settings",T.text,2);ui_text(w,x,46,"Startup apps, theme and accent.",T.dim,1);
        mcard(w,x,72,cw,60+((APP_COUNT+1)/2)*34);ui_text(w,x+14,86,"Open at startup",T.dim,1);int k=0;
        for(int i=0;i<APP_COUNT;i++){if(!apps[i].installed||i==APP_SCRIPT)continue;int cx=x+14+(k%2)*((cw-28)/2),cy=108+(k/2)*34;draw_icon(i,w->cx+cx,w->cy+cy,22);ui_text(w,cx+32,cy+7,apps[i].name,T.text,1);sw_(w,500+i,cx+(cw-28)/2-60,cy,(cfg_startup>>i)&1);k++;}
        int by=72+60+((APP_COUNT+1)/2)*34+14;ui_text(w,x,by,"Accent",T.dim,1);
        for(int i=0;i<8;i++){int cx=x+16+i*40;circle_ex(w->cx+cx,w->cy+by+34,14,0,accents[i],255);if(accents[i]==g_accent)circle_ex(w->cx+cx,w->cy+by+34,18,2,T.text,255);w_hit(w,cx-16,by+18,32,32,600+i);}
        ui_button(w,10,x,by+64,110,30,"Dark theme",g_dark?1:0);ui_button(w,11,x+120,by+64,110,30,"Light theme",g_dark?0:1);
    }
}
static void met_action(win_t*w,int id,int dbl){
    ST(met_t);(void)dbl;
    if(id>=1&&id<=6){s->page=id-1;return;}
    if(id==90)met_add(s);else if(id==91)met_rem(s,30);else if(id==92)met_rem(s,60);else if(id==93)met_rem(s,300);
    else if(id>=100&&id<100+MT){int i=id-100;if(i<s->ntodo){s->done[i]=!s->done[i];met_save(s);}}
    else if(id>=200&&id<200+MT){int i=id-200;if(i<s->ntodo){memmove(s->todo[i],s->todo[i+1],(s->ntodo-i-1)*48);memmove(s->done+i,s->done+i+1,s->ntodo-i-1);s->ntodo--;met_save(s);}}
    else if(id>=300&&id<310){int i=id-300;if(i<s->nsres){int c=s->sres[i];if(c<1000)win_open(c);else open_node(c-1000);}}
    else if(id>=400&&id<400+APP_COUNT)win_open(id-400);
    else if(id>=500&&id<500+APP_COUNT){cfg_startup^=1<<(id-500);cfg_save();}
    else if(id>=600&&id<608){g_accent=accents[id-600];theme_apply();cfg_accent=id-600;cfg_save();}
    else if(id==10||id==11){g_dark=id==10;theme_apply();cfg_theme=g_dark;cfg_save();}
    else if(id==700)win_open(APP_TASK);else if(id>=820&&id<=822)wm_workspace(id-820);
    else if(id==800){static const char cs[]="abcdefghijkmnpqrstuvwxyzABCDEFGHJKLMNPQRSTUVWXYZ23456789";for(int i=0;i<16;i++)s->pw[i]=cs[rnd()%(sizeof cs-1)];s->pw[16]=0;}
    else if(id==810)scopy(s->num,"0",16);
}
static void met_key(win_t*w,int k,int m){
    ST(met_t);(void)m;
    if(s->page==2){if(k=='\n')met_add(s);else if(k=='\b'){if(s->il)s->in[--s->il]=0;}else if(k>=32&&k<127&&s->il<42){s->in[s->il++]=k;s->in[s->il]=0;}}
    else if(s->page==3){if(k=='\b'){if(s->ql)s->q[--s->ql]=0;}else if(k=='\n'){if(s->nsres){int c=s->sres[0];if(c<1000)win_open(c);else open_node(c-1000);}}else if(k>=32&&k<127&&s->ql<30){s->q[s->ql++]=k;s->q[s->ql]=0;}}
    else if(s->page==4){int l=strlen(s->num);if(k=='\b'){if(l>1)s->num[l-1]=0;else scopy(s->num,"0",16);}else if(k>='0'&&k<='9'&&l<9){if(!strcmp(s->num,"0"))l=0;s->num[l]=k;s->num[l+1]=0;}}
}
static void met_tick(win_t*w){
    ST(met_t);if(ticks%100==0)wm_redraw();
    for(int i=0;i<s->nrem;i++)if(!s->rem[i].fired&&ticks>=s->rem[i].due){s->rem[i].fired=1;char b[64];ksnprintf(b,64,"Reminder: %s",s->rem[i].text);toast(b);wm_redraw();}
}


/* ================================================== Safe Space (sandbox) = */
typedef struct{term_t*t;int sel,drawer,newc,shared;}sbx_t;
static void sbx_init(win_t*w){
    sbx_t*s=kzalloc(sizeof(sbx_t));w->d=s;w->sbx=1;if(w->y<30)w->y=30;s->t=kzalloc(sizeof(term_t));s->t->n=1;s->t->jail=sbx_dir();s->t->cwd=s->t->jail;s->sel=-1;w->mem=48;
    tp(s->t,"Safe Space - a sealed test box. Files here cannot touch your real folders.\nType 'help'. Use 'run name.cs' to start a script inside the box.\n");
}
static void sbx_close(win_t*w){sbx_t*s=(sbx_t*)w->d;if(s&&s->t)kfree(s->t);}
static int sbx_list(int*o,int max){int d=sbx_dir(),n=0;for(int i=1;i<FS_MAX&&n<max;i++)if(fsn[i].used&&fsn[i].parent==d)o[n++]=i;return n;}
static void sbx_open_item(int n){
    if(n<=0||!fsn[n].used)return;
    if(fsn[n].dir){toast("Folders stay closed inside the box - use the terminal (ls, cd)");return;}
    if(ends_with(fsn[n].name,".cs"))win_open_sbx(APP_SCRIPT,n);else if(is_img(fsn[n].name))win_open_sbx(APP_VIEW,n);
    else if(ends_with(fsn[n].name,".tar")){int k=tar_extract(n,sbx_dir());char b[48];ksnprintf(b,48,"Unpacked %d files inside the box",k);toast(b);}
    else win_open_sbx(APP_NOTES,n);
}
int sbx_test_file(int node){
    int c=sbx_import(node);if(c==-2){toast("Safe Space is full - raise the storage limit in Pro settings");return -1;}if(c<0){toast("Could not copy that into Safe Space");return -1;}
    win_t*w=0;for(int i=0;i<24;i++)if(wins[i].used&&wins[i].app==APP_SANDBOX&&!wins[i].closing)w=&wins[i];
    if(!w)w=win_open_sbx(APP_SANDBOX,0);else win_focus(w);if(!w)return -1;
    sbx_t*s=(sbx_t*)w->d;char why[40];int bad=shield_scan_node(c,why);s->newc++;s->sel=c;
    char b[160];if(bad){ksnprintf(b,160,"Imported %s. Shield: %s. Not opened - review it first.\n",fsn[c].name,why);tp(s->t,b);toast("Shield flagged this file");}
    else{ksnprintf(b,160,"Imported %s. Shield: no known threats. Opening it inside the box.\n",fsn[c].name);tp(s->t,b);sbx_open_item(c);}
    return 0;
}
static void sbx_draw(win_t*w){
    sbx_t*s=(sbx_t*)w->d;ui_bg(w,T.win);ui_fill(w,0,0,w->cw,44,T.win2,0);
    ui_text(w,14,18,"Internet",T.text,1);sw_(w,1,76,11,g_sbx_net);ui_text(w,128,18,g_sbx_net?"ON":"OFF",g_sbx_net?T.good:T.bad,1);
    ui_button(w,2,170,8,80,28,"Reset",2);ui_button(w,4,258,8,160,28,"Import last download",0);ui_button(w,3,w->cw-86,8,74,28,s->drawer?"Close":"Pro",0);
    int it[32],n=sbx_list(it,32);int by=52,bh=w->ch-52-48;
    ui_fill(w,12,by,222,bh,T.win2,8);
    for(int i=0;i<n&&i<(bh-40)/26;i++){int y=by+6+i*26,on=it[i]==s->sel;if(on||w_hover(w,16,y,214,24))ui_fill(w,16,y,214,24,on?T.accent:T.hover,6);u32 fg=on?(((T.accent>>16)&255)*299+((T.accent>>8)&255)*587+(T.accent&255)*114)/1000>150?0x0B1020:0xFFFFFF:T.text;
        icon_color(fg,0);draw_icon(fsn[it[i]].dir?IC_FOLDER:(is_img(fsn[it[i]].name)?IC_IMAGE:IC_FILE),w->cx+22,w->cy+y+4,16);char nm[22];scopy(nm,fsn[it[i]].name,22);ui_text(w,46,y+8,nm,fg,1);w_hit(w,16,y,214,24,100+i);}
    if(!n)ui_text(w,24,by+14,"Empty. Use Files >",T.dim,1),ui_text(w,24,by+28,"\"Test in Safe Space\".",T.dim,1);
    ui_button(w,5,20,by+bh-34,92,28,"Open",0);ui_button(w,6,118,by+bh-34,92,28,"Delete",0);
    term_render(w,s->t,244,by,w->cw-256,bh);
    ui_fill(w,0,w->ch-40,w->cw,40,T.win2,0);char b[96];
    if(s->newc){ksnprintf(b,96,"%d new item%s inside Safe Mode",s->newc,s->newc==1?"":"s");ui_text(w,14,w->ch-26,b,0xFBBF24,1);ui_button(w,7,w->cw-110,w->ch-34,96,26,"Review",1);}
    else{ksnprintf(b,96,"Box usage %d KB of %d KB",sbx_usage()/1024,g_sbx_quota_kb);ui_text(w,14,w->ch-26,b,T.dim,1);}
    if(s->drawer){int dx=w->cw-300,dw=300;ui_fill(w,dx,44,dw,w->ch-44-40,T.win2,0);rframe(w->cx+dx,w->cy+44,dw,w->ch-84,0,0xF59E0B,200);
        ui_text(w,dx+16,58,"Pro settings",T.text,2);ksnprintf(b,96,"Storage limit: %d MB",g_sbx_quota_kb/1024);ui_text(w,dx+16,90,b,T.dim,1);
        ui_fill(w,dx+16,112,268,6,T.field,3);ui_fill(w,dx+16,112,MAX(6,268*(g_sbx_quota_kb/1024-1)/15),6,0xF59E0B,3);circle_ex(w->cx+dx+16+268*(g_sbx_quota_kb/1024-1)/15,w->cy+115,8,0,0xFFFFFF,255);
        ui_text(w,dx+16,146,"Shared folder",T.text,1);sw_(w,8,dx+232,139,s->shared);
        if(s->shared){ui_button(w,9,dx+16,176,120,28,"Import from Shared",0);ui_button(w,10,dx+146,176,138,28,"Export to Shared",0);}
        else ui_text(w,dx+16,180,"Turn on to copy files in or out.",T.dim,1);
        ui_text(w,dx+16,226,"Snapshots",T.text,1);ui_button(w,11,dx+16,246,120,28,"Save snapshot",0);ui_button(w,12,dx+146,246,138,28,"Restore snapshot",0);
        ui_text(w,dx+16,296,"CPU and memory are shared with the",T.dim,1);ui_text(w,dx+16,310,"system; the box limits files and internet.",T.dim,1);}
}
static void sbx_pick(win_t*w,int x){sbx_t*s=(sbx_t*)w->d;if(!s->drawer)return;int dx=w->cw-300;int mb=1+(x-(dx+16))*15/268;g_sbx_quota_kb=CLAMP(mb,1,16)*1024;}
static void sbx_click(win_t*w,int x,int y,int b,int d){sbx_t*s=(sbx_t*)w->d;(void)b;(void)d;if(s->drawer&&y>=104&&y<=126&&x>=w->cw-300)sbx_pick(w,x);}
static void sbx_drag(win_t*w,int x,int y){sbx_t*s=(sbx_t*)w->d;if(s->drawer&&y>=94&&y<=140)sbx_pick(w,x);}
static void sbx_action(win_t*w,int id,int d){
    sbx_t*s=(sbx_t*)w->d;int it[32],n=sbx_list(it,32);
    if(id==1){g_sbx_net=!g_sbx_net;net_log_as("Safe Space",g_sbx_net?"internet turned on":"internet turned off",0);}
    else if(id==2){sbx_reset();s->sel=-1;s->newc=0;s->t->cwd=s->t->jail;tp(s->t,"Reset to a clean slate.\n");}
    else if(id==3)s->drawer=!s->drawer;
    else if(id==4){int last=0;for(int i=1;i<FS_MAX;i++)if(fsn[i].used&&fsn[i].parent==fs_dl&&!fsn[i].dir)last=i;if(last)sbx_test_file(last);else toast("Downloads is empty");}
    else if(id==5)sbx_open_item(s->sel);
    else if(id==6&&s->sel>0&&fsn[s->sel].used){fs_remove(s->sel);s->sel=-1;}
    else if(id==7){s->newc=0;}
    else if(id==8)s->shared=!s->shared;
    else if(id==9){int sh=fs_find(fs_home,"Shared"),k=0;for(int i=1;i<FS_MAX;i++)if(sh>0&&fsn[i].used&&fsn[i].parent==sh){if(sbx_import(i)>=0)k++;}char b[48];ksnprintf(b,48,"Imported %d item%s from Shared",k,k==1?"":"s");toast(b);s->newc+=k;}
    else if(id==10){int sh=fs_find(fs_home,"Shared");if(sh>0){for(int i=0;i<n;i++)fs_copy_tree(it[i],sh);toast("Copied to Shared");}}
    else if(id==11){sbx_snapshot_save();toast("Snapshot saved");}
    else if(id==12){toast(sbx_snapshot_restore()?"No snapshot yet":"Snapshot restored");}
    else if(id>=100&&id<132){int i=id-100;if(i<n){s->sel=it[i];if(d)sbx_open_item(s->sel);}}
}
static void sbx_key(win_t*w,int k,int m){sbx_t*s=(sbx_t*)w->d;term_key2(w,s->t,k,m);}
static void sbx_tick(win_t*w){sbx_t*s=(sbx_t*)w->d;if(ticks%50==0)wm_redraw();term_tick2(s->t);}
static void sbx_scroll(win_t*w,int dz){sbx_t*s=(sbx_t*)w->d;s->t->off=CLAMP(s->t->off-dz*3,0,s->t->n);}

/* ================================================== Network Monitor ===== */
static void nm_draw(win_t*w){
    ui_bg(w,T.win);char b[120];ui_text(w,20,16,"Network Monitor",T.text,2);ui_text(w,20,42,"Every connection Comet OS makes, and which app made it.",T.dim,1);
    ui_fill(w,16,64,w->cw-32,70,T.win2,12);char ia[20],ig[20],id_[20];ip_str(net.ip,ia);ip_str(net.gw,ig);ip_str(net.dns,id_);
    ksnprintf(b,120,"%s   -   %s",net.nicname,net_status());ui_text(w,30,76,b,T.text,1);ksnprintf(b,120,"address %s   gateway %s   dns %s",ia,ig,id_);ui_text(w,30,94,b,T.dim,1);
    ksnprintf(b,120,"packets in %d   out %d      Safe Space internet: %s",net.rx,net.tx,g_sbx_net?"ON":"off");ui_text(w,30,112,b,T.dim,1);
    ui_text(w,20,146,"By app",T.dim,1);int y=164;
    for(int i=0;i<netapp_n;i++){ksnprintf(b,120,"%s",netapp[i].name);ui_text(w,24,y,b,T.text,1);ksnprintf(b,120,"%d request%s   %d KB",netapp[i].reqs,netapp[i].reqs==1?"":"s",netapp[i].bytes/1024);ui_text(w,w->cw-24-text_w(b,1),y,b,T.dim,1);y+=16;}
    if(!netapp_n){ui_text(w,24,y,"Nothing yet. Open the Browser or run 'ping' in the Terminal.",T.dim,1);y+=16;}
    y+=14;ui_text(w,20,y,"Recent activity",T.dim,1);y+=18;
    for(int k=0;k<netlog_n&&k<48&&y<w->ch-16;k++){netlog_t*e=&netlog[(netlog_n-1-k)%48];ksnprintf(b,120,"%d:%02d",e->t/6000,(e->t/100)%60);ui_text(w,24,y,b,T.dim,1);
        ui_text(w,76,y,e->owner,T.accent2,1);ui_text(w,170,y,e->text,T.text,1);if(e->bytes){ksnprintf(b,120,"%d KB",(e->bytes+1023)/1024);ui_text(w,w->cw-24-text_w(b,1),y,b,T.dim,1);}y+=16;}
}
static void nm_tick(win_t*w){if(ticks%100==0)wm_redraw();}

/* ================================================== UI Lab ============== */
typedef struct{char name[16];int nl;int act;}lab_t;
static void lab_init(win_t*w){w->d=kzalloc(sizeof(lab_t));((lab_t*)w->d)->act=-1;}
typedef struct{const char*n;int*p;int lo,hi;const char*l,*r;}lslider_t;
static lslider_t lsl[5];
static void lab_sl(void){static int init;if(init)return;init=1;
    lsl[0]=(lslider_t){"Window corner radius",&g_radius,0,16,"square","round"};lsl[1]=(lslider_t){"Glass transparency",&g_glass,-60,40,"solid","clear"};
    lsl[2]=(lslider_t){"Blur strength",&g_blur,3,12,"light","heavy"};lsl[3]=(lslider_t){"Taskbar height",&g_tbh,44,64,"slim","tall"};lsl[4]=(lslider_t){"Animation duration",&g_animdur,4,40,"quick","slow"};}
static int lab_theme_dir(void){int t=fs_find(fs_home,"Themes");if(t<0)t=fs_create(fs_home,"Themes",1);return t;}
static void lab_apply_text(const char*d,u32 n){
    char ln[60];u32 i=0;
    while(i<n){int l=0;while(i<n&&d[i]!='\n'&&l<59)ln[l++]=d[i++];ln[l]=0;while(i<n&&d[i]!='\n')i++;i++;char*eq=strchr_(ln,'=');if(!eq)continue;*eq=0;int v=atoi_(eq+1);
        if(!strcmp(ln,"radius"))g_radius=CLAMP(v,0,16);else if(!strcmp(ln,"glass"))g_glass=CLAMP(v,-60,40);else if(!strcmp(ln,"blur"))g_blur=CLAMP(v,3,12);else if(!strcmp(ln,"tbh"))g_tbh=CLAMP(v,44,64);
        else if(!strcmp(ln,"anim"))g_animdur=CLAMP(v,4,40);else if(!strcmp(ln,"dark"))g_dark=v&1;else if(!strcmp(ln,"accent"))g_accent=accents[v&7];else if(!strcmp(ln,"live"))g_live=v&1;else if(!strcmp(ln,"trail"))g_trail=v&1;else if(!strcmp(ln,"hot"))g_hotcorners=v&1;}
    theme_apply();ui_settings_save();
}
static void lab_draw(win_t*w){
    lab_t*s=(lab_t*)w->d;lab_sl();ui_bg(w,T.win);ui_text(w,20,16,"UI Lab",T.text,2);ui_text(w,20,42,"Change how Comet OS looks and moves. Everything updates live.",T.dim,1);
    for(int i=0;i<5;i++){int y=70+i*46;char b[40];ksnprintf(b,40,"%s: %d",lsl[i].n,*lsl[i].p);ui_text(w,24,y,b,T.text,1);int tx=24,tw=w->cw/2-60;
        ui_fill(w,tx,y+20,tw,6,T.field,3);int pos=tw*(*lsl[i].p-lsl[i].lo)/(lsl[i].hi-lsl[i].lo);ui_fill(w,tx,y+20,MAX(6,pos),6,T.accent,3);circle_ex(w->cx+tx+pos,w->cy+y+23,8,0,0xFFFFFF,255);circle_ex(w->cx+tx+pos,w->cy+y+23,8,2,T.accent,255);
        ui_text(w,tx,y+32,lsl[i].l,T.dim,1);ui_text(w,tx+tw-text_w(lsl[i].r,1),y+32,lsl[i].r,T.dim,1);}
    int x=w->cw/2+10;ui_text(w,x,70,"Effects",T.dim,1);
    ui_text(w,x,98,"Living wallpaper (time of day + meteors)",T.text,1);sw_(w,10,w->cw-70,92,g_live);
    ui_text(w,x,134,"Comet cursor trail",T.text,1);sw_(w,11,w->cw-70,128,g_trail);
    ui_text(w,x,170,"Hot corners (top-left: Task View, top-right: Quick settings)",T.text,1);sw_(w,12,w->cw-70,164,g_hotcorners);
    ui_text(w,x,206,"X-ray mode (also Win+X)",T.text,1);sw_(w,13,w->cw-70,200,g_xray);
    ui_text(w,24,316,"Themes",T.dim,1);ui_fill(w,24,336,220,30,T.field,10);ui_text(w,34,347,s->nl?s->name:"theme name",s->nl?T.text:T.dim,1);if(s->nl&&((ticks/50)&1))fill_rect(w->cx+34+s->nl*8,w->cy+341,1,16,T.accent);
    ui_button(w,20,254,336,110,30,"Save theme",1);
    int td=lab_theme_dir(),k[16],n=fs_children(td,k,16),yy=380;
    for(int i=0;i<n&&yy<w->ch-30;i++){if(!ends_with(fsn[k[i]].name,".cstheme"))continue;ui_fill(w,24,yy,w->cw-48,30,T.win2,8);char nm[28];scopy(nm,fsn[k[i]].name,28);ui_text(w,36,yy+11,nm,T.text,1);ui_button(w,300+i,w->cw-176,yy+2,80,26,"Apply",1);ui_button(w,400+i,w->cw-90,yy+2,70,26,"Delete",2);yy+=36;}
}
static void lab_set(win_t*w,int x,int y){
    lab_t*s=(lab_t*)w->d;lab_sl();int tw=w->cw/2-60;
    for(int i=0;i<5;i++){int ty=70+i*46+20;if(s->act==i||(y>=ty-12&&y<=ty+18&&x>=14&&x<=24+tw+14)){s->act=i;int v=lsl[i].lo+(x-24)*(lsl[i].hi-lsl[i].lo)/tw;*lsl[i].p=CLAMP(v,lsl[i].lo,lsl[i].hi);wm_redraw();return;}}
}
static void lab_click(win_t*w,int x,int y,int b,int d){(void)b;(void)d;lab_t*s=(lab_t*)w->d;s->act=-1;lab_set(w,x,y);}
static void lab_drag(win_t*w,int x,int y){lab_set(w,x,y);}
static void lab_release(win_t*w,int x,int y){lab_t*s=(lab_t*)w->d;(void)x;(void)y;if(s->act>=0){s->act=-1;ui_settings_save();}}
static void lab_action(win_t*w,int id,int d){
    lab_t*s=(lab_t*)w->d;(void)d;
    if(id==10){g_live=!g_live;ui_settings_save();}else if(id==11){g_trail=!g_trail;ui_settings_save();}else if(id==12){g_hotcorners=!g_hotcorners;ui_settings_save();}else if(id==13)g_xray=!g_xray;
    else if(id==20&&s->nl){char nm[32],t[200];ksnprintf(nm,32,"%s.cstheme",s->name);int td=lab_theme_dir();int n=fs_find(td,nm);if(n<0)n=fs_create(td,nm,0);
        int len=ksnprintf(t,200,"radius=%d\nglass=%d\nblur=%d\ntbh=%d\nanim=%d\ndark=%d\naccent=%d\nlive=%d\ntrail=%d\nhot=%d\n",g_radius,g_glass,g_blur,g_tbh,g_animdur,g_dark,prof[cur_prof].accent,g_live,g_trail,g_hotcorners);
        if(n>0){fs_write(n,t,len);toast("Theme saved to Themes");}}
    else if(id>=300&&id<316){int td=lab_theme_dir(),k[16],n=fs_children(td,k,16);if(id-300<n&&fsn[k[id-300]].data){lab_apply_text((const char*)fsn[k[id-300]].data,fsn[k[id-300]].size);toast("Theme applied");}}
    else if(id>=400&&id<416){int td=lab_theme_dir(),k[16],n=fs_children(td,k,16);if(id-400<n)fs_remove(k[id-400]);}
}
static void lab_key(win_t*w,int k,int m){lab_t*s=(lab_t*)w->d;(void)m;if(k=='\b'){if(s->nl)s->name[--s->nl]=0;}else if(k=='\n')lab_action(w,20,0);else if(((k>='a'&&k<='z')||(k>='A'&&k<='Z')||(k>='0'&&k<='9')||k=='-'||k=='_')&&s->nl<14){s->name[s->nl++]=k;s->name[s->nl]=0;}}

/* ================================================== table ============== */
app_t apps[APP_COUNT];
void apps_init(void){
    apps2_init();
    apps[APP_TERM]=(app_t){.name="Terminal",.desc="Command line with 30+ commands",.dw=760,.dh=470,.minw=360,.minh=220,.installed=1,.init=term_init,.draw=term_draw,.key=term_key,.scroll=term_scroll,.tick=term_tick};
    apps[APP_FILES]=(app_t){.name="Files",.desc="Browse folders, preview archives",.dw=900,.dh=560,.minw=520,.minh=340,.installed=1,.init=files_init,.draw=files_draw,.key=files_key,.action=files_action,.click=files_click,.scroll=files_scroll,.close=files_close};
    apps[APP_NOTES]=(app_t){.name="Notes",.desc="Text editor",.dw=720,.dh=520,.minw=360,.minh=240,.installed=1,.init=notes_init,.draw=notes_draw,.key=notes_key,.action=notes_action,.click=notes_click,.scroll=notes_scroll,.close=notes_close};
    apps[APP_MON]=(app_t){.name="System Monitor",.desc="CPU, memory, disk and uptime",.dw=820,.dh=580,.minw=560,.minh=480,.installed=1,.draw=mon_draw};
    apps[APP_TASK]=(app_t){.name="Task Manager",.desc="Processes and end task",.dw=700,.dh=500,.minw=520,.minh=320,.installed=1,.init=task_init,.draw=task_draw,.key=task_key,.action=task_action,.scroll=task_scroll};
    apps[APP_STORE]=(app_t){.name="Store",.desc="Get more apps",.dw=720,.dh=560,.minw=520,.minh=360,.installed=1,.init=store_init,.draw=store_draw,.key=store_key,.action=store_action,.scroll=store_scroll,.tick=store_tick};
    apps[APP_VIEW]=(app_t){.name="Photos",.desc="Image viewer with slideshow (.cim, .bmp)",.dw=820,.dh=580,.minw=520,.minh=380,.installed=1,.init=view_init,.draw=view_draw,.key=view_key,.action=view_action,.tick=view_tick,.close=view_close};
    apps[APP_GAME]=(app_t){.name="Comet Rush",.desc="Arcade: dodge rocks, catch stars",.dw=640,.dh=520,.minw=360,.minh=300,.installed=1,.init=game_init,.draw=game_draw,.key=game_key,.click=game_click,.tick=game_tick};
    apps[APP_SET]=(app_t){.name="Settings",.desc="Theme, accent, display, system",.dw=860,.dh=520,.minw=640,.minh=420,.installed=1,.init=set_init,.draw=set_draw,.action=set_action,.key=set_key,.scroll=set_scroll,.close=set_close};
    apps[APP_SETUP]=(app_t){.name="Comet Setup",.desc="Install Comet OS to disk",.dw=640,.dh=460,.minw=520,.minh=380,.installed=1,.init=setup_init,.draw=setup_draw,.action=setup_action};
    apps[APP_CALC]=(app_t){.name="Calculator",.desc="Simple fixed-point calculator",.dw=340,.dh=480,.minw=300,.minh=420,.store=1,.init=calc_init,.draw=calc_draw,.key=calc_key,.action=calc_action};
    apps[APP_PAINT]=(app_t){.name="Paint",.desc="Draw and save to Pictures",.dw=720,.dh=560,.minw=420,.minh=300,.store=1,.init=paint_init,.draw=paint_draw,.action=paint_action,.click=paint_click,.drag=paint_drag,.release=paint_release,.close=paint_close};
    apps[APP_METEORITE]=(app_t){.name="Meteorite",.desc="Dashboard: tasks, reminders, search, startup",.dw=1000,.dh=640,.minw=760,.minh=520,.installed=1,.init=met_init,.draw=met_draw,.key=met_key,.action=met_action,.tick=met_tick};
    apps[APP_SANDBOX]=(app_t){.name="Safe Space",.desc="Sealed test box: try files with no risk",.dw=900,.dh=560,.minw=700,.minh=420,.installed=1,.init=sbx_init,.draw=sbx_draw,.key=sbx_key,.action=sbx_action,.click=sbx_click,.drag=sbx_drag,.scroll=sbx_scroll,.tick=sbx_tick,.close=sbx_close};
    apps[APP_NETMON]=(app_t){.name="Network Monitor",.desc="See what every app sends and receives",.dw=760,.dh=520,.minw=520,.minh=360,.installed=1,.draw=nm_draw,.tick=nm_tick};
    apps[APP_UILAB]=(app_t){.name="UI Lab",.desc="Tune corners, glass, taskbar and effects live",.dw=820,.dh=560,.minw=640,.minh=420,.installed=1,.init=lab_init,.draw=lab_draw,.key=lab_key,.action=lab_action,.click=lab_click,.drag=lab_drag,.release=lab_release};
    apps[APP_SNAKE]=(app_t){.name="Snake",.desc="Classic arcade snake",.dw=640,.dh=470,.minw=600,.minh=440,.store=1,.init=snake_init,.draw=snake_draw,.key=snake_key,.click=snake_click,.tick=snake_tick};
}
