#include "kernel.h"
#include "font.h"

int SW,SH,bga_ok;int g_glass,g_blur=7,g_trail,g_hour=12;u32 g_prims; u32 *bb,*wall,*wallb,*tmpb;
static u32 *fb; static int fbp;
static int clx0,cly0,clx1,cly1;

/* ------------------------------------------------ mode / buffers ------- */
static void bga_w(u16 i,u16 v){outw(0x1CE,i);outw(0x1CF,v);}
static void bga_set(int w,int h){bga_w(4,0);bga_w(1,w);bga_w(2,h);bga_w(3,32);bga_w(4,0x41);}
static void alloc_buffers(void){
    u32 n=SW*SH*4;
    if(bb){kfree(bb);kfree(wall);kfree(wallb);kfree(tmpb);}
    bb=kmalloc(n);wall=kmalloc(n);wallb=kmalloc(n);tmpb=kmalloc(n);
    if(!bb||!wall||!wallb||!tmpb){for(;;)hlt();}
    clip_reset();
}
int gfx_init(u32 addr,int w,int h,int pitch,int bpp){
    outw(0x1CE,0);u16 id=inw(0x1CF);bga_ok=(id>=0xB0C0&&id<=0xB0C6);
    if(addr&&bpp==32){fb=(u32*)addr;fbp=pitch/4;SW=w;SH=h;}
    else{
        u8 b,d,f;if(!bga_ok||!pci_find_class(3,&b,&d,&f))return 0;
        fb=(u32*)(pci_cfg(b,d,f,0x10)&~0xFu);SW=1280;SH=800;bga_set(SW,SH);fbp=SW;
    }
    alloc_buffers();return 1;
}
int gfx_setmode(int w,int h){if(!bga_ok)return 0;bga_set(w,h);SW=w;SH=h;fbp=w;alloc_buffers();return 1;}
void gfx_present(void){for(int y=0;y<SH;y++)memcpy32(fb+y*fbp,bb+y*SW,SW);}
void gfx_panic(const char*a,const char*b){
    if(!fb)return;
    u32*sv=bb;bb=fb;int s=SW;SW=fbp;int sh=SH;if(!SH)SH=600;
    clx0=0;cly0=0;clx1=SW;cly1=SH;
    fill_rect(0,0,SW,SH,0x0A3D91);
    draw_text(60,60,":(",0xFFFFFF,8);
    draw_text(60,160,"Comet OS ran into a problem and has to stop.",0xFFFFFF,2);
    draw_text(60,200,a,0xFFFFFF,2);draw_text(60,240,b,0xCFE3FF,1);
    draw_text(60,280,"Restart the virtual machine to continue.",0xCFE3FF,1);
    bb=sv;SW=s;SH=sh;
}

/* ------------------------------------------------ clipping ------------- */
void clip_reset(void){clx0=0;cly0=0;clx1=SW;cly1=SH;}
cliprect clip_push(int x,int y,int w,int h){
    cliprect o={clx0,cly0,clx1,cly1};
    clx0=MAX(clx0,x);cly0=MAX(cly0,y);clx1=MIN(clx1,x+w);cly1=MIN(cly1,y+h);
    if(clx1<clx0)clx1=clx0;if(cly1<cly0)cly1=cly0;return o;
}
void clip_pop(cliprect c){clx0=c.x0;cly0=c.y0;clx1=c.x1;cly1=c.y1;}

/* ------------------------------------------------ pixels / rects ------- */
u32 blend(u32 d,u32 s,int a){
    if(a<=0)return d;if(a>=255)return s;
    a+=a>>7;int ia=256-a;
    u32 rb=(((d&0xFF00FF)*ia+(s&0xFF00FF)*a)>>8)&0xFF00FF;
    u32 g=(((d&0x00FF00)*ia+(s&0x00FF00)*a)>>8)&0x00FF00;
    return rb|g;
}
static inline void putpx(int x,int y,u32 c){if(x>=clx0&&x<clx1&&y>=cly0&&y<cly1)bb[y*SW+x]=c;}
void fill_rect(int x,int y,int w,int h,u32 c){
    g_prims++;
    int x0=MAX(x,clx0),y0=MAX(y,cly0),x1=MIN(x+w,clx1),y1=MIN(y+h,cly1);
    if(x1<=x0)return;for(int j=y0;j<y1;j++)memset32(bb+j*SW+x0,c,x1-x0);
}
void fill_rect_a(int x,int y,int w,int h,u32 c,int a){
    int x0=MAX(x,clx0),y0=MAX(y,cly0),x1=MIN(x+w,clx1),y1=MIN(y+h,cly1);
    for(int j=y0;j<y1;j++){u32*p=bb+j*SW;for(int i=x0;i<x1;i++)p[i]=blend(p[i],c,a);}
}
void fill_grad(int x,int y,int w,int h,u32 c1,u32 c2){
    for(int j=0;j<h;j++)fill_rect(x,y+j,w,1,blend(c1,c2,h>1?j*255/(h-1):0));
}
static inline int corner_cov(int r,int dx2,int dy2){
    u32 d2=dx2*dx2+dy2*dy2;int d16=(int)isqrt(d2<<8);int cv=(r*32+16-d16)*8;
    return cv<0?0:(cv>255?255:cv);
}
static inline void spx(int x,int y,u32 c,int a,int cov,int mode){
    if(x<clx0||x>=clx1||y<cly0||y>=cly1)return;int i=y*SW+x;
    if(mode){u32 s=blend(wallb[i],c,a);bb[i]=cov>=255?s:blend(bb[i],s,cov);}
    else{int aa=cov>=255?a:a*cov/255;if(aa>=255)bb[i]=c;else if(aa>0)bb[i]=blend(bb[i],c,aa);}
}
void rrect_ex(int x,int y,int w,int h,int r,u32 c,int a,int mask,int mode){
    g_prims++;
    if(w<=0||h<=0)return;if(r*2>w)r=w/2;if(r*2>h)r=h/2;
    for(int j=0;j<h;j++){
        int py=y+j;if(py<cly0||py>=cly1)continue;
        int top=j<r,bot=j>=h-r,lc=0,rc=0,dy2=0;
        if(top){dy2=2*(r-j)-1;lc=mask&1;rc=mask&2;}else if(bot){dy2=2*(j-(h-r))+1;lc=mask&4;rc=mask&8;}
        int sx0=x,sx1=x+w;
        if(lc){for(int i=0;i<r;i++)spx(x+i,py,c,a,corner_cov(r,2*(r-i)-1,dy2),mode);sx0=x+r;}
        if(rc){for(int i=w-r;i<w;i++)spx(x+i,py,c,a,corner_cov(r,2*(i-(w-r))+1,dy2),mode);sx1=x+w-r;}
        int a0=MAX(sx0,clx0),a1=MIN(sx1,clx1);
        if(!mode&&a>=255){if(a1>a0)memset32(bb+py*SW+a0,c,a1-a0);}
        else for(int px=a0;px<a1;px++)spx(px,py,c,a,255,mode);
    }
}
void rframe(int x,int y,int w,int h,int r,u32 c,int a){
    if(w<4||h<4)return;if(r*2>w)r=w/2;if(r*2>h)r=h/2;
    fill_rect_a(x+r,y,w-2*r,1,c,a);fill_rect_a(x+r,y+h-1,w-2*r,1,c,a);
    fill_rect_a(x,y+r,1,h-2*r,c,a);fill_rect_a(x+w-1,y+r,1,h-2*r,c,a);
    for(int j=0;j<r;j++)for(int i=0;i<r;i++){
        int dx2=2*(r-i)-1,dy2=2*(r-j)-1;int cv=corner_cov(r,dx2,dy2)-corner_cov(r-1,dx2,dy2);if(cv<=0)continue;int aa=a*cv/255;
        int xs[2]={x+i,x+w-1-i},ys[2]={y+j,y+h-1-j};
        for(int k=0;k<4;k++){int px=xs[k&1],py=ys[k>>1];if(px>=clx0&&px<clx1&&py>=cly0&&py<cly1)bb[py*SW+px]=blend(bb[py*SW+px],c,aa);}
    }
}
void circle_ex(int cx,int cy,int r,int th,u32 c,int a){
    g_prims++;
    for(int py=cy-r-1;py<=cy+r;py++){
        if(py<cly0||py>=cly1)continue;
        for(int px=cx-r-1;px<=cx+r;px++){
            if(px<clx0||px>=clx1)continue;
            int dx2=2*px+1-2*cx,dy2=2*py+1-2*cy;int cv=corner_cov(r,dx2,dy2);
            if(th>0)cv-=corner_cov(r-th,dx2,dy2);
            if(cv<=0)continue;int aa=a*cv/255;bb[py*SW+px]=blend(bb[py*SW+px],c,aa);
        }
    }
}
void line(int x0,int y0,int x1,int y1,u32 c,int t){
    int minx=MIN(x0,x1)-t-1,maxx=MAX(x0,x1)+t+1,miny=MIN(y0,y1)-t-1,maxy=MAX(y0,y1)+t+1;
    minx=MAX(minx,clx0);maxx=MIN(maxx,clx1-1);miny=MAX(miny,cly0);maxy=MIN(maxy,cly1-1);
    int dx=x1-x0,dy=y1-y0;int l2=dx*dx+dy*dy;int h16=t*8;
    for(int py=miny;py<=maxy;py++)for(int px=minx;px<=maxx;px++){
        int rx=px-x0,ry=py-y0,tf=0;if(l2){tf=((rx*dx+ry*dy)*256)/l2;tf=tf<0?0:(tf>256?256:tf);}
        int ex=(rx*256-tf*dx)>>4,ey=(ry*256-tf*dy)>>4;int d16=(int)isqrt((u32)(ex*ex+ey*ey));
        int cv=(h16-d16+8)*255/16;if(cv<=0)continue;if(cv>255)cv=255;bb[py*SW+px]=blend(bb[py*SW+px],c,cv);
    }
}
void tri_up(int cx,int y,int h,int base,u32 c){for(int j=0;j<h;j++){int w=(j+1)*base/h;fill_rect(cx-w/2,y+j,w,1,c);}}
static void tri_dn(int cx,int y,int h,int base,u32 c){for(int j=0;j<h;j++){int w=(h-j)*base/h;fill_rect(cx-w/2,y+j,w,1,c);}}

/* ------------------------------------------------ text ----------------- */
static u8 g1[95][64],g2[95][256];static u8 gbuilt[95];
static void glyph_build(int ci){
    const unsigned char*g=font8x8[ci];
#define BIT(x,y) (((x)<0||(x)>7||(y)<0||(y)>7)?0:((g[y]>>(x))&1))
    for(int y=0;y<8;y++)for(int x=0;x<8;x++){
        int P=BIT(x,y),A=BIT(x,y-1),B=BIT(x+1,y),C=BIT(x-1,y),D=BIT(x,y+1);
        int e0=(C==A&&C!=D&&A!=B)?A:P,e1=(A==B&&A!=C&&B!=D)?B:P,e2=(D==C&&D!=B&&C!=A)?C:P,e3=(B==D&&B!=A&&D!=C)?D:P;
        g2[ci][(2*y)*16+2*x]=e0?255:0;g2[ci][(2*y)*16+2*x+1]=e1?255:0;g2[ci][(2*y+1)*16+2*x]=e2?255:0;g2[ci][(2*y+1)*16+2*x+1]=e3?255:0;
        g1[ci][y*8+x]=(e0+e1+e2+e3)*255/4;
    }
    gbuilt[ci]=1;
}
void draw_char(int x,int y,int ch,u32 c,int sc){
    if(ch<32||ch>126)return;int ci=ch-32;
    if(x>=clx1||x+8*sc<=clx0||y>=cly1||y+8*sc<=cly0)return;
    if(!gbuilt[ci])glyph_build(ci);
    if(sc==1){
        for(int j=0;j<8;j++){int py=y+j;if(py<cly0||py>=cly1)continue;for(int i=0;i<8;i++){int cv=g1[ci][j*8+i];if(!cv)continue;int px=x+i;if(px<clx0||px>=clx1)continue;bb[py*SW+px]=blend(bb[py*SW+px],c,cv);}}
    }else if(sc==2){
        for(int j=0;j<16;j++){int py=y+j;if(py<cly0||py>=cly1)continue;for(int i=0;i<16;i++)if(g2[ci][j*16+i]){int px=x+i;if(px>=clx0&&px<clx1)bb[py*SW+px]=c;}}
    }else{
        int s2=sc/2;if(s2<1)s2=1;int ex=(sc*8-16*s2)/2;
        for(int j=0;j<16;j++)for(int i=0;i<16;i++)if(g2[ci][j*16+i])fill_rect(x+ex+i*s2,y+ex+j*s2,s2,s2,c);
    }
}
void draw_text(int x,int y,const char*s,u32 c,int sc){g_prims++;for(;*s;s++){draw_char(x,y,*s,c,sc);x+=8*sc;}}
void draw_text_b(int x,int y,const char*s,u32 c,int sc){draw_text(x,y,s,c,sc);draw_text(x+1,y,s,c,sc);}
int text_w(const char*s,int sc){return strlen(s)*8*sc;}

/* ------------------------------------------------ blur / acrylic ------- */
void blur_rect(u32*buf,int x,int y,int w,int h,int r){
    if(x<0){w+=x;x=0;}if(y<0){h+=y;y=0;}if(x+w>SW)w=SW-x;if(y+h>SH)h=SH-y;
    if(w<3||h<3)return;if(r>w/2-1)r=w/2-1;if(r>h/2-1)r=h/2-1;if(r<1)return;
    int win=2*r+1;u32 inv=65536/win;
    for(int j=0;j<h;j++){
        u32*src=buf+(y+j)*SW+x,*dst=tmpb+(y+j)*SW+x;int sr=0,sg=0,sb=0;
        for(int k=-r;k<=r;k++){u32 p=src[k<0?0:k];sr+=(p>>16)&255;sg+=(p>>8)&255;sb+=p&255;}
        for(int i=0;i<w;i++){
            dst[i]=(((sr*inv)>>16)<<16)|(((sg*inv)>>16)<<8)|((sb*inv)>>16);
            int a=i-r;if(a<0)a=0;int b=i+r+1;if(b>w-1)b=w-1;u32 pa=src[a],pb=src[b];
            sr+=((pb>>16)&255)-((pa>>16)&255);sg+=((pb>>8)&255)-((pa>>8)&255);sb+=(pb&255)-(pa&255);
        }
    }
    for(int i=0;i<w;i++){
        int sr=0,sg=0,sb=0;
        for(int k=-r;k<=r;k++){u32 p=tmpb[(y+(k<0?0:k))*SW+x+i];sr+=(p>>16)&255;sg+=(p>>8)&255;sb+=p&255;}
        for(int j=0;j<h;j++){
            buf[(y+j)*SW+x+i]=(((sr*inv)>>16)<<16)|(((sg*inv)>>16)<<8)|((sb*inv)>>16);
            int a=j-r;if(a<0)a=0;int b=j+r+1;if(b>h-1)b=h-1;u32 pa=tmpb[(y+a)*SW+x+i],pb=tmpb[(y+b)*SW+x+i];
            sr+=((pb>>16)&255)-((pa>>16)&255);sg+=((pb>>8)&255)-((pa>>8)&255);sb+=(pb&255)-(pa&255);
        }
    }
}
void blur_region(int x,int y,int w,int h,int r){blur_rect(bb,x,y,w,h,r);}
void shadow(int x,int y,int w,int h,int r,int spread){
    for(int k=spread;k>=1;k--){int a=34*(spread-k+1)*(spread-k+1)/(spread*spread)+1;rframe(x-k,y-k+spread/3,w+2*k,h+2*k,r+k,0x000000,a/2+1);}
}
void acrylic(int x,int y,int w,int h,int rad,u32 tint,int ta){
    shadow(x,y,w,h,rad,10);
    u32 sv[4][256];int cs=MIN(rad,16);int xs[4]={x,x+w-cs,x,x+w-cs},ys[4]={y,y,y+h-cs,y+h-cs};
    for(int k=0;k<4;k++)for(int j=0;j<cs;j++)for(int i=0;i<cs;i++){int px=xs[k]+i,py=ys[k]+j;sv[k][j*16+i]=(px>=0&&px<SW&&py>=0&&py<SH)?bb[py*SW+px]:0;}
    blur_region(x,y,w,h,g_blur);
    for(int k=0;k<4;k++)for(int j=0;j<cs;j++)for(int i=0;i<cs;i++){int px=xs[k]+i,py=ys[k]+j;if(px>=0&&px<SW&&py>=0&&py<SH)bb[py*SW+px]=sv[k][j*16+i];}
    ta+=g_glass;if(ta<40)ta=40;if(ta>250)ta=250;rrect(x,y,w,h,rad,tint,ta);rframe(x,y,w,h,rad,0xFFFFFF,38);
}

/* ------------------------------------------------ wallpaper ------------ */
static void glow(int cx,int cy,int rad,u32 col,int str){
    int x0=MAX(cx-rad,0),x1=MIN(cx+rad,SW),y0=MAX(cy-rad,0),y1=MIN(cy+rad,SH);
    u32 r2=rad*rad,inv=(1u<<24)/r2;
    for(int y=y0;y<y1;y++){int dy=y-cy;u32*p=bb+y*SW;
        for(int x=x0;x<x1;x++){int dx=x-cx;u32 d2=dx*dx+dy*dy;if(d2>=r2)continue;
            int u=256-(int)((d2*inv)>>16);u=(u*u)>>8;int a=(u*str)>>8;if(a>0)p[x]=blend(p[x],col,a);}}
}
void wall_generate(int dark,u32 acc){
    clip_reset();
    u32 c1=dark?0x070B18:0xCFDDFB,c2=dark?0x1A1038:0xF6E7FF,pinkc=0xEC4899;
    if(dark){int h=g_hour;
        if(h>=5&&h<8){c1=0x1B1030;c2=0x5A2A55;pinkc=0xFB923C;}else if(h>=8&&h<17){c1=0x0B1B3A;c2=0x1D3F7A;pinkc=0x22D3EE;}else if(h>=17&&h<21){c1=0x1A0F2E;c2=0x6E2A4A;pinkc=0xF472B6;}else{c1=0x050813;c2=0x140C30;pinkc=0x8B5CF6;}}
    for(int y=0;y<SH;y++){u32*p=bb+y*SW;for(int x=0;x<SW;x++){int t=(x*256/SW+y*256/SH)>>1;p[x]=blend(c1,c2,t);}}
    glow(SW*3/4,SH*35/100,SH*80/100,acc,dark?150:120);
    glow(SW/5,SH*85/100,SH*65/100,0x7C3AED,dark?140:90);
    glow(SW/2,-SH/10,SH*60/100,0x06B6D4,dark?90:70);
    glow(SW*9/10,SH*95/100,SH*40/100,pinkc,dark?70:60);
    if(dark){u32 s=777;for(int i=0;i<SW*SH/2200;i++){s=s*1664525+1013904223;int x=(s>>8)%SW;s=s*1664525+1013904223;int y=(s>>8)%SH;s=s*1664525+1013904223;int a=60+((s>>16)&127);
        putpx(x,y,blend(bb[y*SW+x],0xFFFFFF,a));if(a>150){putpx(x+1,y,blend(bb[y*SW+x+1],0xFFFFFF,a/3));putpx(x,y+1,blend(bb[(y+1)*SW+x],0xFFFFFF,a/3));putpx(x-1,y,blend(bb[y*SW+x-1],0xFFFFFF,a/3));putpx(x,y-1,blend(bb[(y-1)*SW+x],0xFFFFFF,a/3));}}}
    /* the comet */
    int hx=SW*70/100,hy=SH*24/100,L=SH*45/100;
    for(int k=L;k>=0;k--){int x=hx-k*5/5,y=hy-k*2/5;int f=(L-k)*256/L;int rr=2+f*7/256,a=(f*f>>8)*(dark?200:150)>>8;if(a<6)continue;
        circle_ex(x,y,rr,0,0xBFE6FF,a/3);circle_ex(x,y,rr/2+1,0,0xFFFFFF,a);}
    circle_ex(hx,hy,34,0,0x9FD8FF,40);circle_ex(hx,hy,18,0,0xDDF2FF,90);circle_ex(hx,hy,9,0,0xFFFFFF,255);
    memcpy32(wall,bb,SW*SH);memcpy32(wallb,bb,SW*SH);
    blur_rect(wallb,0,0,SW,SH,14);blur_rect(wallb,0,0,SW,SH,10);
}

void wall_from_image(const u32*src,int iw,int ih){
    if(!src||iw<2||ih<2)return;clip_reset();int sw,sh;
    if((long)iw*SH>=(long)ih*SW){sh=SH;sw=iw*SH/ih;}else{sw=SW;sh=ih*SW/iw;}
    int ox=(sw-SW)/2,oy=(sh-SH)/2;int*xs=kmalloc(SW*sizeof(int));if(!xs)return;
    int fxs=((iw-1)*4096)/MAX(sw-1,1),fys=((ih-1)*4096)/MAX(sh-1,1);
    for(int x=0;x<SW;x++)xs[x]=((x+ox)*fxs)>>4;
    for(int y=0;y<SH;y++){int sy=((y+oy)*fys)>>4;int y0=sy>>8,fy=sy&255,y1=MIN(y0+1,ih-1);const u32*r0=src+y0*iw,*r1=src+y1*iw;u32*d=bb+y*SW;
        for(int x=0;x<SW;x++){int x0=xs[x]>>8,fx=xs[x]&255,x1=MIN(x0+1,iw-1);d[x]=blend(blend(r0[x0],r0[x1],fx),blend(r1[x0],r1[x1],fx),fy);}}
    kfree(xs);memcpy32(wall,bb,SW*SH);memcpy32(wallb,bb,SW*SH);blur_rect(wallb,0,0,SW,SH,14);blur_rect(wallb,0,0,SW,SH,10);
}
void wall_live(u32 tick){
    for(int i=0;i<3;i++){u32 per=520+i*190,ph=(tick+i*233)%per;if(ph>=70)continue;u32 seed=(tick+i*233)/per;
        int sx=(int)((seed*977+i*313)%(u32)SW)+SW/4,sy=(int)((seed*613+i*97)%(u32)(SH/2));int x=sx-(int)ph*10,y=sy+(int)ph*5;
        for(int k=0;k<16;k++){int px=x+k*6,py=y-k*3;int a=230-k*14;if(a<10)break;circle_ex(px,py,k<3?2:1,0,k<3?0xFFFFFF:0xBFE6FF,a);}
    }
}
void post_fx(int bright,int night){
    int f=bright*256/100;u32 n=SW*SH;
    for(u32 i=0;i<n;i++){u32 p=bb[i];int r=(p>>16)&255,g=(p>>8)&255,b=p&255;
        r=(r*f)>>8;g=(g*f)>>8;b=(b*f)>>8;if(night){g=g*92/100;b=b*62/100;}bb[i]=(r<<16)|(g<<8)|b;}
}

/* ------------------------------------------------ cursor --------------- */
static u32 spr[24*24];static int hx_,hy_,cur_drawn,cur_ox,cur_oy,cur_kind=-1;
static const char*arrow[19]={"X           ","XX          ","X.X         ","X..X        ","X...X       ","X....X      ","X.....X     ","X......X    ","X.......X   ","X........X  ","X.....XXXXX ","X..X..X     ","X.X X..X    ","XX  X..X    ","X    X..X   ","     X..X   ","      X..X  ","      X..X  ","       XX   "};
static void sline(int x0,int y0,int x1,int y1,u32 c,int t){
    int dx=ABS(x1-x0),sx=x0<x1?1:-1,dy=-ABS(y1-y0),sy=y0<y1?1:-1,err=dx+dy;
    for(;;){for(int j=0;j<t;j++)for(int i=0;i<t;i++){int px=x0-t/2+i,py=y0-t/2+j;if(px>=0&&px<24&&py>=0&&py<24)spr[py*24+px]=c;}
        if(x0==x1&&y0==y1)break;int e2=2*err;if(e2>=dy){err+=dy;x0+=sx;}if(e2<=dx){err+=dx;y0+=sy;}}
}
static void shape(int k,u32 c,int t){
    switch(k){
    case 1:sline(4,12,20,12,c,t);sline(4,12,9,7,c,t);sline(4,12,9,17,c,t);sline(20,12,15,7,c,t);sline(20,12,15,17,c,t);break;
    case 2:sline(12,4,12,20,c,t);sline(12,4,7,9,c,t);sline(12,4,17,9,c,t);sline(12,20,7,15,c,t);sline(12,20,17,15,c,t);break;
    case 3:sline(5,5,19,19,c,t);sline(5,5,5,12,c,t);sline(5,5,12,5,c,t);sline(19,19,19,12,c,t);sline(19,19,12,19,c,t);break;
    case 4:sline(19,5,5,19,c,t);sline(19,5,19,12,c,t);sline(19,5,12,5,c,t);sline(5,19,5,12,c,t);sline(5,19,12,19,c,t);break;
    case 5:sline(12,4,12,20,c,t);sline(8,4,16,4,c,t);sline(8,20,16,20,c,t);break;
    }
}
void cursor_set(int k){
    if(k==cur_kind)return;cur_kind=k;memset(spr,0,sizeof spr);
    if(k==0){hx_=0;hy_=0;for(int j=0;j<19;j++)for(int i=0;i<12;i++){char ch=arrow[j][i];if(ch=='X')spr[j*24+i]=0x01000000;else if(ch=='.')spr[j*24+i]=0x01FFFFFF;}}
    else{hx_=12;hy_=12;shape(k,0x01000000,4);shape(k,0x01FFFFFF,2);}
}
static int tr_x[8],tr_y[8],tr_n,td_x[8],td_y[8],td_n;
static void trail_restore(void){for(int k=0;k<td_n;k++)for(int j=-7;j<=7;j++){int y=td_y[k]+j;if(y<0||y>=SH)continue;for(int i=-7;i<=7;i++){int x=td_x[k]+i;if(x<0||x>=SW)continue;fb[y*fbp+x]=bb[y*SW+x];}}td_n=0;}
static void trail_draw(int x,int y){
    if(!g_trail){tr_n=0;return;}
    if(!tr_n||ABS(x-tr_x[0])+ABS(y-tr_y[0])>=4){for(int k=7;k>0;k--){tr_x[k]=tr_x[k-1];tr_y[k]=tr_y[k-1];}tr_x[0]=x;tr_y[0]=y;if(tr_n<8)tr_n++;}
    for(int k=1;k<tr_n;k++){int r=5-k/2,a=170-k*20;if(r<1||a<10)continue;int cx=tr_x[k],cy=tr_y[k];td_x[td_n]=cx;td_y[td_n]=cy;td_n++;
        for(int j=-r;j<=r;j++)for(int i=-r;i<=r;i++){if(i*i+j*j>r*r)continue;int px=cx+i,py=cy+j;if(px<0||px>=SW||py<0||py>=SH)continue;fb[py*fbp+px]=blend(fb[py*fbp+px],0xBFE6FF,a);}}
}
void cursor_hide(void){
    trail_restore();
    if(!cur_drawn)return;cur_drawn=0;
    for(int j=0;j<24;j++){int y=cur_oy+j;if(y<0||y>=SH)continue;for(int i=0;i<24;i++){int x=cur_ox+i;if(x<0||x>=SW)continue;fb[y*fbp+x]=bb[y*SW+x];}}
}
void cursor_show(int x,int y){
    td_n=0;trail_draw(x,y);if(cur_kind<0)cursor_set(0);cur_ox=x-hx_;cur_oy=y-hy_;cur_drawn=1;
    for(int j=0;j<24;j++){int py=cur_oy+j;if(py<0||py>=SH)continue;for(int i=0;i<24;i++){int px=cur_ox+i;if(px<0||px>=SW)continue;u32 s=spr[j*24+i];if(s)fb[py*fbp+px]=s&0xFFFFFF;}}
}

/* ------------------------------------------------ icons ---------------- */
static u32 ic_fg=0xFFFFFF,ic_bg=0x000000;
void icon_color(u32 fg,u32 bg){ic_fg=fg;ic_bg=bg;}
static void tile(int x,int y,int s,u32 c){rrect(x,y,s,s,s/4,c,255);rrect_ex(x,y,s,s/2,s/4,0xFFFFFF,34,3,0);rframe(x,y,s,s,s/4,0xFFFFFF,40);}
void draw_logo(int cx,int cy,int s,u32 c){
    for(int i=0;i<3;i++){int o=(i-1)*s/4;line(cx-s/2+i,cy-s/3+o+s/6,cx+s/8,cy+s/10+o/2,blend(c,0xFFFFFF,i*20),MAX(2,s/10-i));}
    circle_ex(cx+s/5,cy+s/5,s/4,0,0xFFFFFF,255);circle_ex(cx+s/5,cy+s/5,s/4+2,2,c,200);
}
static void dots(int cx,int cy,int r,int a0,int a1,int step,int dr,u32 c){
    for(int a=a0;a<=a1;a+=step)circle_ex(cx+r*icos(a)/1024,cy-r*isin(a)/1024,dr,0,c,255);
}
void draw_icon(int id,int x,int y,int s){
    int cx=x+s/2,cy=y+s/2;u32 W=0xFFFFFF;
    switch(id){
    case APP_TERM:tile(x,y,s,0x1A2138);{int sc=s>=36?2:1;draw_text(x+(s-16*sc)/2,y+(s-8*sc)/2,">_",0x7CF2C8,sc);}break;
    case APP_FILES:tile(x,y,s,0xF59E0B);rrect(x+s*16/100,y+s*27/100,s*30/100,s*14/100,3,0xFFE08A,255);rrect(x+s*16/100,y+s*35/100,s*68/100,s*42/100,s/12,0xFFD35C,255);rrect(x+s*16/100,y+s*44/100,s*68/100,s*33/100,s/12,0xFFF0B8,255);break;
    case APP_NOTES:tile(x,y,s,0xEAB308);rrect(x+s*26/100,y+s*16/100,s*48/100,s*68/100,s/14,W,255);for(int i=0;i<4;i++)fill_rect(x+s*34/100,y+s*(30+i*13)/100,s*32/100-(i==3?s/8:0),MAX(1,s/20),0x9CA3AF);break;
    case APP_MON:tile(x,y,s,0x10B981);for(int i=0;i<4;i++){int h=s*(25+i*13+(i==2?-10:0))/100;rrect(x+s*(20+i*16)/100,y+s*78/100-h,s*11/100+1,h,2,W,255);}break;
    case APP_TASK:tile(x,y,s,0x6366F1);for(int i=0;i<3;i++){rrect(x+s*20/100,y+s*(24+i*18)/100,s*11/100+1,s*11/100+1,2,W,255);fill_rect(x+s*38/100,y+s*(28+i*18)/100,s*40/100,MAX(2,s/14),W);}break;
    case APP_STORE:tile(x,y,s,0x3B82F6);rrect(x+s*24/100,y+s*40/100,s*52/100,s*40/100,s/10,W,255);rframe(x+s*36/100,y+s*20/100,s*28/100,s*30/100,s/6,W,255);rframe(x+s*37/100,y+s*21/100,s*26/100,s*28/100,s/6,W,255);break;
    case APP_VIEW:tile(x,y,s,0xEC4899);rrect(x+s*18/100,y+s*24/100,s*64/100,s*52/100,s/12,0xFFE4F1,255);circle_ex(x+s*64/100,y+s*40/100,MAX(2,s/10),0,0xFBBF24,255);tri_up(x+s*40/100,y+s*44/100,s*26/100,s*34/100,0xDB2777);tri_up(x+s*62/100,y+s*53/100,s*17/100,s*24/100,0xBE185D);break;
    case APP_GAME:tile(x,y,s,0xF97316);rrect(x+s*14/100,y+s*32/100,s*72/100,s*38/100,s/6,W,255);fill_rect(x+s*26/100,y+s*47/100,s*16/100,MAX(2,s/12),0x7C2D12);fill_rect(x+s*32/100,y+s*41/100,MAX(2,s/12),s*16/100,0x7C2D12);circle_ex(x+s*66/100,y+s*45/100,MAX(2,s/14),0,0x7C2D12,255);circle_ex(x+s*74/100,y+s*53/100,MAX(2,s/14),0,0x7C2D12,255);break;
    case APP_SET:tile(x,y,s,0x64748B);for(int a=0;a<360;a+=45)line(cx,cy,cx+s*30/100*icos(a)/1024,cy-s*30/100*isin(a)/1024,W,MAX(3,s/9));circle_ex(cx,cy,s*24/100,0,W,255);circle_ex(cx,cy,s*10/100,0,0x64748B,255);break;
    case APP_SETUP:tile(x,y,s,0x06B6D4);fill_rect(x+s*43/100,y+s*18/100,s*14/100,s*26/100,W);tri_dn(cx,y+s*42/100,s*20/100,s*40/100,W);rrect(x+s*20/100,y+s*68/100,s*60/100,s*12/100,2,W,255);break;
    case APP_CALC:tile(x,y,s,0x14B8A6);rrect(x+s*22/100,y+s*18/100,s*56/100,s*16/100,2,W,255);for(int j=0;j<3;j++)for(int i=0;i<3;i++)rrect(x+s*(22+i*20)/100,y+s*(40+j*16)/100,s*14/100,s*11/100,2,W,j==2&&i==2?255:200);break;
    case APP_PAINT:tile(x,y,s,0xA855F7);circle_ex(cx,cy,s*30/100,0,W,255);{u32 cc[4]={0xEF4444,0x3B82F6,0x22C55E,0xFACC15};for(int i=0;i<4;i++)circle_ex(cx+s*17/100*icos(200+i*50)/1024,cy-s*17/100*isin(200+i*50)/1024,MAX(2,s/11),0,cc[i],255);}break;
    case APP_SNAKE:tile(x,y,s,0x22C55E);for(int i=0;i<4;i++)rrect(x+s*(18+i*14)/100,y+s*(i&1?58:44)/100,s*14/100+1,s*14/100+1,2,W,255);rrect(x+s*74/100,y+s*30/100,s*12/100,s*14/100,2,0xFEF08A,255);break;
    case APP_METEORITE:tile(x,y,s,0x241838);for(int i=0;i<4;i++)line(x+s*18/100+i*s/40,y+s*22/100+i*s/14,x+s*58/100,y+s*58/100,blend(0xFB923C,0x241838,i*55),MAX(2,s/10-i));circle_ex(x+s*62/100,y+s*62/100,s*20/100,0,0xFB923C,255);circle_ex(x+s*58/100,y+s*57/100,s*7/100,0,0xFDBA74,255);circle_ex(x+s*68/100,y+s*68/100,MAX(2,s*5/100),0,0xC2410C,255);break;
    case APP_BROWSER:tile(x,y,s,0x0EA5E9);circle_ex(cx,cy,s*30/100,MAX(2,s/14),0xFFFFFF,255);fill_rect(cx-s*30/100,cy-1,s*60/100,MAX(2,s/16),0xFFFFFF);circle_ex(cx,cy,s*14/100,MAX(1,s/20),0xFFFFFF,255);line(cx,y+s*20/100,cx,y+s*80/100,0xFFFFFF,MAX(1,s/20));break;
    case APP_MUSIC:tile(x,y,s,0xF43F5E);circle_ex(x+s*38/100,y+s*68/100,s*13/100,0,0xFFFFFF,255);circle_ex(x+s*66/100,y+s*62/100,s*13/100,0,0xFFFFFF,255);fill_rect(x+s*47/100,y+s*26/100,MAX(2,s/14),s*42/100,0xFFFFFF);fill_rect(x+s*75/100,y+s*20/100,MAX(2,s/14),s*42/100,0xFFFFFF);line(x+s*48/100,y+s*27/100,x+s*76/100,y+s*21/100,0xFFFFFF,MAX(3,s/9));break;
    case APP_SHIELD:tile(x,y,s,0x16A34A);rrect_ex(x+s*24/100,y+s*20/100,s*52/100,s*38/100,s/10,0xFFFFFF,255,3,0);tri_dn(cx,y+s*50/100,s*30/100,s*52/100,0xFFFFFF);line(cx-s*10/100,cy-s*2/100,cx-s*2/100,cy+s*8/100,0x16A34A,MAX(2,s/10));line(cx-s*2/100,cy+s*8/100,cx+s*12/100,cy-s*10/100,0x16A34A,MAX(2,s/10));break;
    case APP_SANDBOX:tile(x,y,s,0xB45309);rframe(x+s*18/100,y+s*18/100,s*64/100,s*64/100,s/8,0xFDE68A,255);rframe(x+s*19/100,y+s*19/100,s*62/100,s*62/100,s/8,0xFDE68A,255);rrect_ex(x+s*34/100,y+s*30/100,s*32/100,s*24/100,s/10,0xFFFFFF,255,3,0);tri_dn(cx,y+s*50/100,s*20/100,s*32/100,0xFFFFFF);break;
    case APP_NETMON:tile(x,y,s,0x0F766E);for(int i=0;i<4;i++)rrect(x+s*(20+i*16)/100,y+s*(70-i*14)/100-s/20,s*10/100+1,s*(14+i*14)/100,2,0xFFFFFF,255);circle_ex(x+s*80/100,y+s*24/100,MAX(2,s/12),0,0x5EEAD4,255);break;
    case APP_UILAB:tile(x,y,s,0x7C3AED);for(int i=0;i<3;i++){int yy=y+s*(28+i*22)/100;fill_rect(x+s*20/100,yy,s*60/100,MAX(2,s/16),0xDDD6FE);circle_ex(x+s*(30+i*18)/100,yy+1,MAX(3,s/9),0,0xFFFFFF,255);}break;
    case APP_SCRIPT:tile(x,y,s,0x475569);{int sc=s>=36?2:1;draw_text(x+(s-24*sc)/2,y+(s-8*sc)/2,"</>",0xA5F3FC,sc);}break;
    case 100:rrect(x,y+s*20/100,s*40/100,s*18/100,3,0xE0A526,255);rrect(x,y+s*28/100,s,s*60/100,s/8,0xF7C948,255);break;
    case 101:rrect(x+s*15/100,y,s*70/100,s,s/9,ic_fg,255);fill_rect(x+s*28/100,y+s*40/100,s*44/100,MAX(1,s/12),ic_bg);fill_rect(x+s*28/100,y+s*58/100,s*44/100,MAX(1,s/12),ic_bg);fill_rect(x+s*28/100,y+s*74/100,s*28/100,MAX(1,s/12),ic_bg);break;
    case 102:rrect(x,y+s*12/100,s,s*76/100,s/9,0xEC4899,255);circle_ex(x+s*70/100,y+s*35/100,MAX(2,s/9),0,0xFEF3C7,255);tri_up(x+s*35/100,y+s*40/100,s*40/100,s*50/100,0xFFE4F1);break;
    case 103:rrect(x+s*10/100,y+s*8/100,s*80/100,s*84/100,s/9,0x8B5CF6,255);for(int i=0;i<5;i++)fill_rect(x+s*44/100,y+s*(12+i*16)/100,s*12/100,s*8/100,i&1?0xDDD6FE:0xFFFFFF);break;
    case 104:dots(cx,y+s*72/100,s*18/100,40,140,8,MAX(1,s/16),ic_fg);dots(cx,y+s*72/100,s*36/100,40,140,6,MAX(1,s/16),ic_fg);dots(cx,y+s*72/100,s*54/100,40,140,5,MAX(1,s/16),ic_fg);circle_ex(cx,y+s*72/100,MAX(2,s/10),0,ic_fg,255);break;
    case 105:rrect(x+s*10/100,y+s*36/100,s*20/100,s*28/100,2,ic_fg,255);for(int j=0;j<s*40/100;j++){int hh=s*28/100+j*3/2;fill_rect(x+s*30/100+j/2,cy-hh/2,1+(j>0),hh,ic_fg);}dots(x+s*40/100,cy,s*22/100,-40,40,10,MAX(1,s/16),ic_fg);dots(x+s*40/100,cy,s*36/100,-40,40,8,MAX(1,s/16),ic_fg);break;
    case 106:rframe(x+s*8/100,y+s*28/100,s*76/100,s*44/100,4,ic_fg,255);rrect(x+s*14/100,y+s*35/100,s*54/100,s*30/100,2,ic_fg,255);rrect(x+s*86/100,y+s*40/100,s*8/100,s*20/100,2,ic_fg,255);break;
    case 107:circle_ex(x+s*42/100,y+s*42/100,s*30/100,MAX(2,s/9),ic_fg,255);line(x+s*64/100,y+s*64/100,x+s*88/100,y+s*88/100,ic_fg,MAX(2,s/8));break;
    case 108:rframe(x+s*10/100,y+s*22/100,s*58/100,s*46/100,4,ic_fg,255);rframe(x+s*32/100,y+s*36/100,s*58/100,s*46/100,4,ic_fg,255);break;
    case 109:for(int i=0;i<4;i++)rrect(x+s*(14+(i&1)*40)/100,y+s*(14+(i>>1)*40)/100,s*32/100,s*32/100,s/10,ic_fg,i==0?255:190);break;
    case 110:dots(cx,cy,s*34/100,130,410,12,MAX(1,s/14),ic_fg);line(cx,y+s*10/100,cx,cy,ic_fg,MAX(2,s/10));break;
    case 111:circle_ex(cx,cy,s*20/100,0,ic_fg,255);for(int a=0;a<360;a+=45)line(cx+s*30/100*icos(a)/1024,cy-s*30/100*isin(a)/1024,cx+s*44/100*icos(a)/1024,cy-s*44/100*isin(a)/1024,ic_fg,MAX(2,s/12));break;
    case 112:circle_ex(cx,cy,s*36/100,0,ic_fg,255);circle_ex(cx+s*16/100,cy-s*10/100,s*32/100,0,ic_bg,255);break;
    case 113:tri_up(cx,y+s*10/100,s*50/100,s*50/100,ic_fg);fill_rect(cx-s*4/100-1,y+s*55/100,s*8/100+2,s*30/100,ic_fg);fill_rect(x+s*10/100,y+s*44/100,s*80/100,s*12/100,ic_fg);break;
    case 114:for(int a=0;a<360;a+=45)line(cx,cy,cx+s*40/100*icos(a)/1024,cy-s*40/100*isin(a)/1024,ic_fg,MAX(2,s/12));circle_ex(cx,cy,s*12/100,0,ic_fg,255);break;
    case 115:circle_ex(cx,cy,s*34/100,MAX(2,s/10),ic_fg,255);circle_ex(cx,cy,s*12/100,0,ic_fg,255);break;
    case 116:line(x+s*20/100,y+s*20/100,x+s*80/100,y+s*80/100,ic_fg,MAX(2,s/9));line(x+s*80/100,y+s*20/100,x+s*20/100,y+s*80/100,ic_fg,MAX(2,s/9));break;
    case 117:fill_rect(x+s*14/100,cy-s/16,s*72/100,MAX(2,s/9),ic_fg);break;
    case 118:rframe(x+s*16/100,y+s*16/100,s*68/100,s*68/100,2,ic_fg,255);rframe(x+s*17/100,y+s*17/100,s*66/100,s*66/100,2,ic_fg,255);break;
    }
}
void gauge(int cx,int cy,int r,int pct,u32 col,u32 track){
    circle_ex(cx,cy,r+3,6,track,90);
    int end=pct*360/100;for(int a=0;a<=end;a+=3)circle_ex(cx+r*icos(a-90)/1024,cy+r*isin(a-90)/1024,3,0,col,255);
}
void blit_scaled(const u32*src,int sw,int sh,int dx,int dy,int dw,int dh){
    if(dw<=0||dh<=0||!src)return;u32 fx=((u32)sw<<16)/dw,fy=((u32)sh<<16)/dh;
    for(int j=0;j<dh;j++){int y=dy+j;if(y<cly0||y>=cly1)continue;const u32*row=src+((j*fy)>>16)*sw;u32*d=bb+y*SW;
        for(int i=0;i<dw;i++){int x=dx+i;if(x<clx0||x>=clx1)continue;d[x]=row[(i*fx)>>16];}}
}
