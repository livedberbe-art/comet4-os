#include "kernel.h"

ata_dev ata[4]; inst_t inst; int g_installed; int cfg_theme=1,cfg_accent=0,cfg_res=1,cfg_startup=1<<13,cfg_removed,cfg_nightsched,cfg_haspw; u32 cfg_pwhash;
profile_t prof[4]={{"guest",1,0,0,1,0}};int cur_prof,sel_prof;unsigned char cfg_ui[8]={10,100,7,52,18,3,0,0};
static const u16 bases[2]={0x1F0,0x170},ctrls[2]={0x3F6,0x376};
static u8 iobuf[16*2048];
static u32 cfg_lba; static int cfg_disk=-1;

static void d400(u32 c){inb(c);inb(c);inb(c);inb(c);}
static int busy_wait(u32 b,int need_drq){
    for(int t=0;t<400000;t++){u8 s=inb(b+7);if(s==0xFF)return -3;if(s&0x21)return -1;if(!(s&0x80)){if(!need_drq||(s&8))return 0;}}
    return -2;
}
static void probe(int i){
    int ch=i>>1,dr=i&1;u32 b=bases[ch];ata_dev*d=&ata[i];memset(d,0,sizeof *d);
    d->base=b;d->ctrl=ctrls[ch];d->drive=dr;outb(d->ctrl,2);
    outb(b+6,0xA0|(dr<<4));d400(d->ctrl);outb(b+2,0);outb(b+3,0);outb(b+4,0);outb(b+5,0);outb(b+7,0xEC);
    u8 s=inb(b+7);if(s==0||s==0xFF)return;
    for(int t=0;t<400000&&(inb(b+7)&0x80);t++);
    u8 m=inb(b+4),h=inb(b+5);int atapi=0;
    if(m==0x14&&h==0xEB){atapi=1;outb(b+7,0xA1);d400(d->ctrl);}else if(m||h)return;
    if(busy_wait(b,1)!=0)return;
    u16 id[256];insw(b,id,256);
    for(int k=0;k<20;k++){d->model[k*2]=id[27+k]>>8;d->model[k*2+1]=id[27+k]&255;}d->model[40]=0;
    for(int k=39;k>=0&&d->model[k]==' ';k--)d->model[k]=0;
    d->atapi=atapi;d->present=1;d->sectors=atapi?0:(id[60]|((u32)id[61]<<16));
}
static int ata_rw(ata_dev*d,u32 lba,u32 cnt,u8*buf,int wr){
    u32 b=d->base;outb(b+6,0xE0|(d->drive<<4)|((lba>>24)&15));d400(d->ctrl);
    outb(b+2,cnt);outb(b+3,lba);outb(b+4,lba>>8);outb(b+5,lba>>16);outb(b+7,wr?0x30:0x20);
    for(u32 i=0;i<cnt;i++){
        if(busy_wait(b,1))return -1;
        if(wr)outsw(b,buf+i*512,256);else insw(b,buf+i*512,256);
        d400(d->ctrl);
    }
    if(wr){outb(b+7,0xE7);busy_wait(b,0);}
    return 0;
}
static int atapi_read(ata_dev*d,u32 lba,u32 blocks,u8*buf){
    u32 b=d->base;outb(b+6,0xA0|(d->drive<<4));d400(d->ctrl);
    outb(b+1,0);outb(b+4,0x00);outb(b+5,0x80);outb(b+7,0xA0);
    if(busy_wait(b,1))return -1;
    u8 cdb[12]={0x28,0,lba>>24,lba>>16,lba>>8,lba,0,blocks>>8,blocks,0,0,0};
    outsw(b,cdb,6);
    u32 got=0,want=blocks*2048;
    while(got<want){
        if(busy_wait(b,1))return -1;
        u32 len=inb(b+4)|((u32)inb(b+5)<<8);if(!len||got+len>want)break;
        insw(b,buf+got,len/2);got+=len;d400(d->ctrl);
    }
    busy_wait(b,0);return got==want?0:-1;
}
void disk_init(void){for(int i=0;i<4;i++)probe(i);inst_detect();}

void inst_detect(void){
    inst.src=inst.dst=-1;
    for(int i=0;i<4;i++)if(ata[i].present&&ata[i].atapi&&inst.src<0)inst.src=i;
    for(int i=0;i<4;i++)if(ata[i].present&&!ata[i].atapi&&inst.dst<0)inst.dst=i;
    if(inst.src<0)scopy(inst.msg,"No CD/DVD drive with the Comet OS ISO was found.",80);
    else if(inst.dst<0)scopy(inst.msg,"No hard disk found. Add an IDE disk in VirtualBox.",80);
    else scopy(inst.msg,"Ready to install.",80);
}
void inst_start(void){
    if(inst.src<0||inst.dst<0)return;
    if(atapi_read(&ata[inst.src],16,1,iobuf)||memcmp(iobuf+1,"CD001",5)){inst.state=3;scopy(inst.msg,"Could not read the installation medium.",80);return;}
    inst.total=*(u32*)(iobuf+80);inst.done=0;
    if(!inst.total||ata[inst.dst].sectors<inst.total*4+16){inst.state=3;scopy(inst.msg,"The disk is too small (need the ISO size + 8 KB).",80);return;}
    inst.state=1;scopy(inst.msg,"Copying system files...",80);
}
static void cfg_pack(u8*s){memset(s,0,512);memcpy(s,"COMETCFG",8);s[8]=cfg_theme;s[9]=cfg_accent;s[10]=cfg_res;s[11]=cfg_startup&255;s[12]=(cfg_startup>>8)&255;s[14]=(cfg_startup>>16)&255;s[15]=(cfg_startup>>24)&255;s[13]=1;s[16]=cfg_removed&255;s[17]=(cfg_removed>>8)&255;s[18]=(cfg_removed>>16)&255;s[19]=(cfg_removed>>24)&255;s[20]=cfg_nightsched;s[21]=cfg_haspw;memcpy(s+22,&cfg_pwhash,4);memcpy(s+40,cfg_ui,8);s[63]=0xA5;for(int i=0;i<4;i++){u8*p=s+64+i*20;memcpy(p,prof[i].name,12);p[12]=prof[i].used;p[13]=prof[i].has;p[14]=prof[i].accent;p[15]=prof[i].dark;memcpy(p+16,&prof[i].hash,4);}}
void inst_step(void){
    if(inst.state!=1)return;
    for(int k=0;k<4&&inst.done<inst.total;k++){
        u32 n=MIN(16u,inst.total-inst.done);
        if(atapi_read(&ata[inst.src],inst.done,n,iobuf)){inst.state=3;scopy(inst.msg,"Read error from the installation medium.",80);return;}
        if(ata_rw(&ata[inst.dst],inst.done*4,n*4,iobuf,1)){inst.state=3;scopy(inst.msg,"Write error on the target disk.",80);return;}
        inst.done+=n;
    }
    if(inst.done>=inst.total){
        cfg_pack(iobuf);ata_rw(&ata[inst.dst],inst.total*4,1,iobuf,1);
        cfg_lba=inst.total*4;cfg_disk=inst.dst;g_installed=1;
        inst.state=2;scopy(inst.msg,"Comet OS is installed. Remove the ISO and restart.",80);
    }
}
u32 pw_hash(const char*pw){u32 h=2166136261u;const char*pre="comet:";for(;*pre;pre++){h^=(u8)*pre;h*=16777619u;}for(;*pw;pw++){h^=(u8)*pw;h*=16777619u;}return h;}
void cfg_load(void){
    for(int i=0;i<4;i++)if(ata[i].present&&!ata[i].atapi){
        if(ata_rw(&ata[i],64,1,iobuf,0))continue;
        if(memcmp(iobuf+1,"CD001",5))continue;
        u32 total=*(u32*)(iobuf+80);if(!total||total>0x100000)continue;
        if(ata_rw(&ata[i],total*4,1,iobuf,0))continue;
        if(memcmp(iobuf,"COMETCFG",8))continue;
        cfg_theme=iobuf[8]&1;cfg_accent=iobuf[9]&7;cfg_res=iobuf[10]&3;if(iobuf[13]==1){cfg_removed=iobuf[16]|(iobuf[17]<<8)|(iobuf[18]<<16)|((u32)iobuf[19]<<24);cfg_nightsched=iobuf[20];cfg_haspw=iobuf[21];memcpy(&cfg_pwhash,iobuf+22,4);memcpy(cfg_ui,iobuf+40,8);
            if(iobuf[63]==0xA5){for(int k=0;k<4;k++){const u8*p=iobuf+64+k*20;memcpy(prof[k].name,p,12);prof[k].name[11]=0;prof[k].used=p[12];prof[k].has=p[13];prof[k].accent=p[14];prof[k].dark=p[15];memcpy(&prof[k].hash,p+16,4);}}
            else{prof[0].has=cfg_haspw;prof[0].hash=cfg_pwhash;}}if(iobuf[13]==1)cfg_startup=iobuf[11]|(iobuf[12]<<8)|(iobuf[14]<<16)|(iobuf[15]<<24);cfg_lba=total*4;cfg_disk=i;g_installed=1;return;
    }
}
void cfg_save(void){if(cfg_disk<0)return;cfg_pack(iobuf);ata_rw(&ata[cfg_disk],cfg_lba,1,iobuf,1);}

/* ---- filesystem image lives right after the config sector on the installed disk ---- */
static int pio_rw_big(int disk,u32 lba,u8*buf,u32 sectors,int wr){
    while(sectors){u32 n=sectors>128?128:sectors;if(ata_rw(&ata[disk],lba,n,buf,wr))return -1;lba+=n;buf+=n*512;sectors-=n;}return 0;
}
int disk_persist_write(const u8*buf,u32 len){
    if(cfg_disk<0)return -1;u32 base=cfg_lba+8,secs=len/512;if(base+secs>ata[cfg_disk].sectors)return -1;
    /* data first, header sector last: a torn write leaves the old header, whose checksum then fails safely */
    if(secs>1&&pio_rw_big(cfg_disk,base+1,(u8*)buf+512,secs-1,1))return -1;
    return pio_rw_big(cfg_disk,base,(u8*)buf,1,1);
}
int disk_persist_read(u8**out,u32*len){
    if(cfg_disk<0)return -1;u32 base=cfg_lba+8;static u8 hdr[512];if(ata_rw(&ata[cfg_disk],base,1,hdr,0))return -1;
    if(memcmp(hdr,"CFS1",4))return -1;u32 total;memcpy(&total,hdr+8,4);if(total<512||total>(32u<<20)||base+total/512>ata[cfg_disk].sectors)return -1;
    u8*b=kmalloc(total);if(!b)return -1;memcpy(b,hdr,512);if(total>512&&pio_rw_big(cfg_disk,base+1,b+512,total/512-1,0)){kfree(b);return -1;}
    *out=b;*len=total;return 0;
}
