#include "kernel.h"

volatile int mouse_x,mouse_y,mouse_b,mouse_moved,mods_now; int mouse_wheel_ok;
static event_t q[256]; static volatile int qh,qt;
static void push(int t,int a,int b,int c){int n=(qt+1)&255;if(n==qh)return;q[qt].type=t;q[qt].a=a;q[qt].b=b;q[qt].c=c;qt=n;}
int ev_pop(event_t*e){cli();if(qh==qt){sti();return 0;}*e=q[qh];qh=(qh+1)&255;sti();return 1;}
void mouse_clamp(void){if(mouse_x>=SW)mouse_x=SW-1;if(mouse_y>=SH)mouse_y=SH-1;if(mouse_x<0)mouse_x=0;if(mouse_y<0)mouse_y=0;}

/* ------------------------------------------------ keyboard ------------- */
static int shift,ctrl,alt,win,caps,ext,win_used;
static const char km[]="\0" "\033" "1234567890-=" "\b\t" "qwertyuiop[]" "\n\0" "asdfghjkl;'`" "\0\\" "zxcvbnm,./" "\0*\0 ";
static const char ks[]="\0" "\033" "!@#$%^&*()_+" "\b\t" "QWERTYUIOP{}" "\n\0" "ASDFGHJKL:\"~" "\0|" "ZXCVBNM<>?" "\0*\0 ";
static void setmods(void){mods_now=(shift?M_SHIFT:0)|(ctrl?M_CTRL:0)|(alt?M_ALT:0)|(win?M_WIN:0);}

void kbd_irq(void){
    u8 sc=inb(0x60);
    if(sc==0xE0){ext=1;return;}
    if(sc==0xE1){return;}
    int up=sc&0x80;sc&=0x7F;int e=ext;ext=0;int key=0;
    if(sc==0x2A||sc==0x36){shift=!up;setmods();return;}
    if(sc==0x1D){ctrl=!up;setmods();return;}
    if(sc==0x38){int was=alt;alt=!up;setmods();if(was&&up)push(EV_ALTUP,0,0,0);return;}
    if(e&&(sc==0x5B||sc==0x5C)){
        if(!up){if(!win){win=1;win_used=0;}}
        else{if(win){win=0;if(!win_used)push(EV_WINTAP,0,0,0);push(EV_WINUP,0,0,0);}}
        setmods();return;
    }
    if(sc==0x3A&&!up&&!e){caps=!caps;return;}
    if(up)return;
    if(e){
        switch(sc){case 0x48:key=K_UP;break;case 0x50:key=K_DOWN;break;case 0x4B:key=K_LEFT;break;case 0x4D:key=K_RIGHT;break;
            case 0x47:key=K_HOME;break;case 0x4F:key=K_END;break;case 0x49:key=K_PGUP;break;case 0x51:key=K_PGDN;break;
            case 0x53:key=K_DEL;break;case 0x52:key=K_INS;break;case 0x1C:key='\n';break;case 0x35:key='/';break;}
    }else if(sc<0x3A){
        char c=(shift?ks:km)[sc];
        if(caps&&c>='a'&&c<='z')c-=32;else if(caps&&c>='A'&&c<='Z')c+=32;
        key=(u8)c;
    }else if(sc>=0x3B&&sc<=0x44)key=K_F1+(sc-0x3B);
    else if(sc==0x57)key=K_F11;else if(sc==0x58)key=K_F12;
    else{static const char pad[]="789-456+1230.";if(sc>=0x47&&sc<=0x53)key=(u8)pad[sc-0x47];}
    if(!key)return;
    if(win)win_used=1;
    push(EV_KEY,key,mods_now,0);
}

/* ------------------------------------------------ mouse ---------------- */
static u8 mpk[4];static int mi,msz=3;
void mouse_irq(void){
    u8 d=inb(0x60);
    if(mi==0&&!(d&8))return;
    mpk[mi++]=d;if(mi<msz)return;mi=0;
    if(mpk[0]&0xC0)return;
    int dx=mpk[1],dy=mpk[2];if(mpk[0]&0x10)dx-=256;if(mpk[0]&0x20)dy-=256;
    int dz=0;if(msz==4){dz=mpk[3]&15;if(dz&8)dz-=16;}
    if(ABS(dx)+ABS(dy)>14){dx=dx*3/2;dy=dy*3/2;}
    mouse_x=CLAMP(mouse_x+dx,0,SW-1);mouse_y=CLAMP(mouse_y-dy,0,SH-1);if(dx||dy)mouse_moved=1;
    int b=mpk[0]&7,ch=b^mouse_b;
    for(int i=0;i<3;i++)if(ch&(1<<i))push(EV_MBTN,i+1,(b>>i)&1,(mouse_x<<16)|mouse_y);
    mouse_b=b;if(dz)push(EV_WHEEL,dz,0,0);
}
static int wait_in(void){for(int t=200000;t;t--)if(!(inb(0x64)&2))return 1;return 0;}
static int wait_out(void){for(int t=200000;t;t--)if(inb(0x64)&1)return 1;return 0;}
static u8 mw(u8 v){wait_in();outb(0x64,0xD4);wait_in();outb(0x60,v);if(wait_out())return inb(0x60);return 0;}
void input_init(void){
    for(int i=0;i<64&&(inb(0x64)&1);i++)inb(0x60);
    wait_in();outb(0x64,0xA8);
    wait_in();outb(0x64,0x20);wait_out();u8 s=inb(0x60);s|=3;s&=~0x20;
    wait_in();outb(0x64,0x60);wait_in();outb(0x60,s);
    mw(0xF6);mw(0xF3);mw(200);mw(0xF3);mw(100);mw(0xF3);mw(80);
    mw(0xF2);u8 id=wait_out()?inb(0x60):0;
    if(id==3){msz=4;mouse_wheel_ok=1;}
    mw(0xF4);
    mouse_x=SW/2;mouse_y=SH/2;
}
