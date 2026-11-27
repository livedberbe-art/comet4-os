#include "kernel.h"

volatile u32 ticks; u32 mem_total_kb; u32 irq_timer,irq_kbd,irq_mouse;
char cpu_brand[64], cpu_vendor[16];
pci_dev_t pci_devs[64]; int pci_count;
extern u32 _kernel_end; extern u32 isr_table[48]; extern void gdt_flush(u32);

/* ------------------------------------------------ libc-ish ------------- */
void *memset(void*d,int v,size_t n){u8*p=d;while(n--)*p++=(u8)v;return d;}
void *memcpy(void*d,const void*s,size_t n){u8*a=d;const u8*b=s;while(n--)*a++=*b++;return d;}
void *memmove(void*d,const void*s,size_t n){u8*a=d;const u8*b=s;if(a<b){while(n--)*a++=*b++;}else{a+=n;b+=n;while(n--)*--a=*--b;}return d;}
int memcmp(const void*a,const void*b,size_t n){const u8*x=a,*y=b;for(;n;n--,x++,y++)if(*x!=*y)return *x-*y;return 0;}
void memcpy32(u32*d,const u32*s,u32 n){__asm__ volatile("rep movsl":"+D"(d),"+S"(s),"+c"(n)::"memory");}
void memset32(u32*d,u32 v,u32 n){__asm__ volatile("rep stosl":"+D"(d),"+c"(n):"a"(v):"memory");}
size_t strlen(const char*s){size_t n=0;while(s[n])n++;return n;}
int strcmp(const char*a,const char*b){while(*a&&*a==*b){a++;b++;}return (u8)*a-(u8)*b;}
int strncmp(const char*a,const char*b,size_t n){for(;n;n--,a++,b++){if(*a!=*b)return (u8)*a-(u8)*b;if(!*a)return 0;}return 0;}
char *strchr_(const char*s,int c){for(;*s;s++)if(*s==(char)c)return (char*)s;return 0;}
char *strcpy(char*d,const char*s){char*r=d;while((*d++=*s++));return r;}
void scopy(char*d,const char*s,int n){int i=0;if(n<=0)return;for(;i<n-1&&s[i];i++)d[i]=s[i];d[i]=0;}
static int lc(int c){return (c>='A'&&c<='Z')?c+32:c;}
int stricontains(const char*h,const char*n){
    if(!*n)return 1;
    for(;*h;h++){const char*a=h,*b=n;while(*a&&*b&&lc(*a)==lc(*b)){a++;b++;}if(!*b)return 1;}
    return 0;
}
int ends_with(const char*s,const char*suf){int a=strlen(s),b=strlen(suf);if(b>a)return 0;for(int i=0;i<b;i++)if(lc(s[a-b+i])!=lc(suf[i]))return 0;return 1;}
int atoi_(const char*s){int v=0,neg=0;if(*s=='-'){neg=1;s++;}while(*s>='0'&&*s<='9')v=v*10+(*s++-'0');return neg?-v:v;}

int ksnprintf(char*buf,int n,const char*f,...){
    va_list ap;va_start(ap,f);int o=0;
#define PUT(ch) do{if(o<n-1)buf[o]=(ch);o++;}while(0)
    for(;*f;f++){
        if(*f!='%'){PUT(*f);continue;}
        f++;int zero=0,wd=0;if(*f=='0'){zero=1;f++;}
        while(*f>='0'&&*f<='9'){wd=wd*10+(*f-'0');f++;}
        char t[16];int tl=0;const char*s=0;
        if(*f=='s'){s=va_arg(ap,const char*);if(!s)s="(null)";}
        else if(*f=='c'){t[0]=(char)va_arg(ap,int);tl=1;}
        else if(*f=='d'||*f=='u'){
            int v=va_arg(ap,int);u32 u;int neg=0;
            if(*f=='d'&&v<0){neg=1;u=(u32)(-v);}else u=(u32)v;
            char r[12];int rl=0;do{r[rl++]='0'+u%10;u/=10;}while(u);
            if(neg)t[tl++]='-';while(rl)t[tl++]=r[--rl];
        }else if(*f=='x'){u32 u=va_arg(ap,u32);char r[10];int rl=0;do{r[rl++]="0123456789abcdef"[u&15];u>>=4;}while(u);while(rl)t[tl++]=r[--rl];}
        else if(*f=='%'){t[0]='%';tl=1;}
        else{PUT('%');if(*f)PUT(*f);else break;continue;}
        int len=s?(int)strlen(s):tl;
        for(int i=len;i<wd;i++)PUT(zero&&!s?'0':' ');
        if(s)for(int i=0;s[i];i++)PUT(s[i]);else for(int i=0;i<tl;i++)PUT(t[i]);
    }
    va_end(ap);if(n>0)buf[o<n?o:n-1]=0;return o;
#undef PUT
}

/* ------------------------------------------------ math ----------------- */
u32 isqrt(u32 v){u32 r=0,b=1u<<30;while(b>v)b>>=2;while(b){if(v>=r+b){v-=r+b;r=(r>>1)+b;}else r>>=1;b>>=2;}return r;}
static int bh(int x){int p=x*(180-x);return 4096*p/(40500-p);} /* sin(x deg)*1024, 0..180 */
int isin(int d){d%=360;if(d<0)d+=360;return d<=180?bh(d):-bh(d-180);}
int icos(int d){return isin(d+90);}
static u32 rs=0x1234567;
u32 rnd(void){rs^=rs<<13;rs^=rs>>17;rs^=rs<<5;return rs;}

int pct(u32 a,u32 b){if(!b)return 0;while(a>40000000u){a>>=1;b>>=1;}if(!b)b=1;int r=a*100/b;return r>100?100:r;}

/* ------------------------------------------------ heap ----------------- */
typedef struct blk{u32 size;u32 free;struct blk*next;u32 pad;}blk;
static blk*head;static u32 hsz,hused;
static void heap_init(void*s,u32 sz){head=s;head->size=sz-sizeof(blk);head->free=1;head->next=0;hsz=sz;hused=0;}
void *kmalloc(u32 n){
    n=(n+15)&~15u;
    for(blk*b=head;b;b=b->next)if(b->free&&b->size>=n){
        if(b->size>=n+sizeof(blk)+32){blk*nb=(blk*)((u8*)(b+1)+n);nb->size=b->size-n-sizeof(blk);nb->free=1;nb->next=b->next;b->next=nb;b->size=n;}
        b->free=0;hused+=b->size;return b+1;
    }
    return 0;
}
void *kzalloc(u32 n){void*p=kmalloc(n);if(p)memset(p,0,n);return p;}
void kfree(void*p){
    if(!p)return;blk*b=(blk*)p-1;if(b->free)return;b->free=1;hused-=b->size;
    for(blk*c=head;c;c=c->next)while(c->free&&c->next&&c->next->free){c->size+=sizeof(blk)+c->next->size;c->next=c->next->next;}
}
u32 heap_used(void){return hused;} u32 heap_total(void){return hsz;}

/* ------------------------------------------------ descriptor tables ---- */
struct idt_e{u16 lo,sel;u8 zero,flags;u16 hi;}__attribute__((packed));
struct dtr{u16 limit;u32 base;}__attribute__((packed));
static u64 gdt[3];static struct idt_e idt[256];static struct dtr gdtr;
static void gdt_init(void){gdt[0]=0;gdt[1]=0x00CF9A000000FFFFULL;gdt[2]=0x00CF92000000FFFFULL;gdtr.limit=sizeof(gdt)-1;gdtr.base=(u32)gdt;gdt_flush((u32)&gdtr);}
static void idt_init(void){
    for(int i=0;i<48;i++){u32 a=isr_table[i];idt[i].lo=a&0xFFFF;idt[i].hi=a>>16;idt[i].sel=8;idt[i].zero=0;idt[i].flags=0x8E;}
    struct dtr d={sizeof(idt)-1,(u32)idt};__asm__ volatile("lidt %0"::"m"(d));
}
static void pic_init(void){
    outb(0x20,0x11);outb(0xA0,0x11);outb(0x21,32);outb(0xA1,40);outb(0x21,4);outb(0xA1,2);outb(0x21,1);outb(0xA1,1);
    outb(0x21,0xF8);outb(0xA1,0xEF);
}
static void pit_init(void){u32 d=1193182/100;outb(0x43,0x36);outb(0x40,d&255);outb(0x40,d>>8);}

static const char*exn[]={"Divide error","Debug","NMI","Breakpoint","Overflow","Bound range","Invalid opcode","Device N/A","Double fault","Coproc","Invalid TSS","Segment not present","Stack fault","General protection","Page fault","Reserved","x87 FPU","Alignment","Machine check","SIMD"};
void kpanic(const char*msg,u32 code,u32 eip){
    char a[96],b[96];ksnprintf(a,96,"%s",msg);ksnprintf(b,96,"code=%x  eip=%x",code,eip);
    cli();gfx_panic(a,b);for(;;)hlt();
}
void isr_handler(regs_t*r){
    if(r->int_no<32){kpanic(r->int_no<20?exn[r->int_no]:"CPU exception",r->err,r->eip);}
    u32 n=r->int_no-32;
    if(n==0){ticks++;irq_timer++;}else if(n==1){irq_kbd++;kbd_irq();}else if(n==12){irq_mouse++;mouse_irq();}
    if(n>=8)outb(0xA0,0x20);outb(0x20,0x20);
}

/* ------------------------------------------------ rtc / power ---------- */
static u8 cmos(u8 r){outb(0x70,r);return inb(0x71);}
void rtc_read(int*h,int*m,int*s,int*d,int*mo,int*y){
    int t=100000;while((cmos(0x0A)&0x80)&&--t);
    u8 S=cmos(0),M=cmos(2),H=cmos(4),D=cmos(7),Mo=cmos(8),Y=cmos(9),B=cmos(0x0B);
    if(!(B&4)){
#define BCD(x) (((x)&15)+((x)>>4)*10)
        int pm=H&0x80;S=BCD(S);M=BCD(M);H=BCD(H&0x7F);D=BCD(D);Mo=BCD(Mo);Y=BCD(Y);if(pm)H|=0x80;
    }
    if(!(B&2)&&(H&0x80)){H=((H&0x7F)%12)+12;}
    *h=H%24;*m=M;*s=S;*d=D?D:1;*mo=Mo?Mo:1;*y=2000+Y;
}
void poweroff(void){outw(0x4004,0x3400);outw(0x604,0x2000);outw(0xB004,0x2000);}
void reboot(void){cli();int t=100000;while((inb(0x64)&2)&&--t);outb(0x64,0xFE);for(volatile int i=0;i<2000000;i++);static const struct{u16 l;u32 b;}__attribute__((packed)) nul={0,0};__asm__ volatile("lidt %0; int3"::"m"(nul));for(;;)hlt();}

/* ------------------------------------------------ pci / cpu ------------ */
u32 pci_cfg(u8 b,u8 d,u8 f,u8 off){outl(0xCF8,0x80000000u|((u32)b<<16)|((u32)d<<11)|((u32)f<<8)|(off&0xFC));return inl(0xCFC);}
static void pci_scan(void){
    for(int b=0;b<2;b++)for(int d=0;d<32;d++)for(int f=0;f<8;f++){
        u32 id=pci_cfg(b,d,f,0);if((id&0xFFFF)==0xFFFF){if(f==0)break;continue;}
        if(pci_count<64){pci_dev_t*p=&pci_devs[pci_count++];p->vendor=id&0xFFFF;p->device=id>>16;
            u32 c=pci_cfg(b,d,f,8);p->cls=c>>24;p->sub=(c>>16)&255;p->bus=b;p->dev=d;p->fn=f;p->bar0=pci_cfg(b,d,f,0x10);}
        if(f==0&&!(pci_cfg(b,d,0,0x0C)&0x800000))break;
    }
}
int pci_find_class(u8 cls,u8*b,u8*d,u8*f){for(int i=0;i<pci_count;i++)if(pci_devs[i].cls==cls){*b=pci_devs[i].bus;*d=pci_devs[i].dev;*f=pci_devs[i].fn;return 1;}return 0;}
static void cpuid(u32 l,u32*a,u32*b,u32*c,u32*d){__asm__ volatile("cpuid":"=a"(*a),"=b"(*b),"=c"(*c),"=d"(*d):"a"(l),"c"(0));}
static void cpu_detect(void){
    u32 a,b,c,d;cpuid(0,&a,&b,&c,&d);memcpy(cpu_vendor,&b,4);memcpy(cpu_vendor+4,&d,4);memcpy(cpu_vendor+8,&c,4);cpu_vendor[12]=0;
    cpuid(0x80000000,&a,&b,&c,&d);scopy(cpu_brand,"Generic x86 CPU",64);
    if(a>=0x80000004){char t[52];for(u32 i=0;i<3;i++){cpuid(0x80000002+i,&a,&b,&c,&d);memcpy(t+i*16,&a,4);memcpy(t+i*16+4,&b,4);memcpy(t+i*16+8,&c,4);memcpy(t+i*16+12,&d,4);}
        t[48]=0;char*p=t;while(*p==' ')p++;scopy(cpu_brand,p,64);}
}

/* ------------------------------------------------ entry ---------------- */
typedef struct{u32 flags,mem_lower,mem_upper,boot_device,cmdline,mods_count,mods_addr,syms[4],mmap_length,mmap_addr,drives_length,drives_addr,config_table,boot_loader_name,apm_table,vbe_control_info,vbe_mode_info;
    u16 vbe_mode,vbe_seg,vbe_off,vbe_len;u64 fb_addr;u32 fb_pitch,fb_width,fb_height;u8 fb_bpp,fb_type;u8 color[6];}__attribute__((packed)) mbi_t;

void kmain(u32 magic,mbi_t*mp){
    mbi_t mbi;memset(&mbi,0,sizeof mbi);if(magic==0x2BADB002&&mp)memcpy(&mbi,mp,sizeof mbi);
    gdt_init();idt_init();pic_init();pit_init();
    u32 top=(mbi.flags&1)?0x100000u+mbi.mem_upper*1024u:0x8000000u;
    u32 ke=((u32)&_kernel_end+0xFFFF)&~0xFFFFu;u32 hs=top>0x3000000u?0x1000000u:ke;if(hs<ke)hs=ke;
    u32 hz=top-hs-0x100000u;if(hz>0x10000000u)hz=0x10000000u;
    heap_init((void*)hs,hz);mem_total_kb=top/1024;
    cpu_detect();pci_scan();
    int ok=0;
    if((mbi.flags&(1u<<12))&&mbi.fb_type==1&&mbi.fb_bpp==32&&mbi.fb_addr)ok=gfx_init((u32)mbi.fb_addr,mbi.fb_width,mbi.fb_height,mbi.fb_pitch,32);
    else ok=gfx_init(0,0,0,0,0);
    if(!ok)for(;;)hlt();
    splash(0);input_init();sti();
    fs_init();splash(1);disk_init();cfg_load();fs_persist_load();fs_set_user(prof[cur_prof].name);net_init();splash(2);
    apps_init();wm_init();splash(3);
    wm_run();
}

void speaker(int hz){
    if(hz<=0){outb(0x61,inb(0x61)&0xFC);return;}
    u32 d=1193182/hz;outb(0x43,0xB6);outb(0x42,d&255);outb(0x42,d>>8);outb(0x61,inb(0x61)|3);
}
int g_sbx_net,g_sbx_quota_kb=4096;
