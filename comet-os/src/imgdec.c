/* PNG (all non-interlaced color types) and baseline JPEG decoders. Pure C so it can be tested on a host. */
#ifdef HOST
#include <stdlib.h>
#include <string.h>
typedef unsigned char u8;typedef unsigned short u16;typedef unsigned int u32;typedef short i16;
#define kmalloc malloc
#define kfree free
#else
#include "kernel.h"
#endif
u32 *png_decode(const u8*d,u32 n,int*ow,int*oh);
u32 *jpg_decode(const u8*d,u32 n,int*ow,int*oh);

/* ------------------------------------------------ inflate (puff-style) - */
typedef struct{const u8*src;u32 n,pos;u32 bb;int bc;u8*dst;u32 cap,len;int err;}inf_t;
typedef struct{u16 count[16];u16 sym[288];}huff_t;
static int gbit(inf_t*s){if(!s->bc){if(s->pos>=s->n){s->err=1;return 0;}s->bb=s->src[s->pos++];s->bc=8;}int b=s->bb&1;s->bb>>=1;s->bc--;return b;}
static u32 gbits(inf_t*s,int n){u32 v=0;for(int i=0;i<n;i++)v|=(u32)gbit(s)<<i;return v;}
static void hbuild(huff_t*h,const u8*len,int n){u16 offs[16];memset(h->count,0,sizeof h->count);for(int i=0;i<n;i++)h->count[len[i]]++;h->count[0]=0;offs[1]=0;for(int i=1;i<15;i++)offs[i+1]=offs[i]+h->count[i];for(int i=0;i<n;i++)if(len[i])h->sym[offs[len[i]]++]=i;}
static int hdec(inf_t*s,const huff_t*h){int code=0,first=0,index=0;for(int l=1;l<=15;l++){code|=gbit(s);int c=h->count[l];if(code-c<first)return h->sym[index+(code-first)];index+=c;first+=c;first<<=1;code<<=1;if(s->err)return -1;}return -1;}
static const u16 lbase[29]={3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258},lext[29]={0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
static const u16 dbase[30]={1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577},dext[30]={0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};
static int codes(inf_t*s,const huff_t*lh,const huff_t*dh){
    for(;;){int sym=hdec(s,lh);if(sym<0)return -1;
        if(sym<256){if(s->len>=s->cap)return -1;s->dst[s->len++]=sym;}
        else if(sym==256)return 0;
        else{sym-=257;if(sym>=29)return -1;u32 len=lbase[sym]+gbits(s,lext[sym]);int ds=hdec(s,dh);if(ds<0||ds>=30)return -1;u32 dist=dbase[ds]+gbits(s,dext[ds]);
            if(dist>s->len||s->len+len>s->cap)return -1;for(u32 i=0;i<len;i++){s->dst[s->len]=s->dst[s->len-dist];s->len++;}}
        if(s->err)return -1;}
}
static int inflate_(const u8*src,u32 n,u8*dst,u32 cap){
    inf_t s;memset(&s,0,sizeof s);s.src=src;s.n=n;s.dst=dst;s.cap=cap;int last;
    do{last=gbit(&s);int t=gbits(&s,2);
        if(t==0){s.bc=0;if(s.pos+4>s.n)return -1;u32 l=s.src[s.pos]|(s.src[s.pos+1]<<8);s.pos+=4;if(s.pos+l>s.n||s.len+l>s.cap)return -1;memcpy(s.dst+s.len,s.src+s.pos,l);s.len+=l;s.pos+=l;}
        else if(t==1){static huff_t fl,fd;static int built;if(!built){u8 ln[288];int i=0;for(;i<144;i++)ln[i]=8;for(;i<256;i++)ln[i]=9;for(;i<280;i++)ln[i]=7;for(;i<288;i++)ln[i]=8;hbuild(&fl,ln,288);for(i=0;i<30;i++)ln[i]=5;hbuild(&fd,ln,30);built=1;}if(codes(&s,&fl,&fd))return -1;}
        else if(t==2){huff_t lh,dh,ch;u8 ln[320];int nl=gbits(&s,5)+257,nd=gbits(&s,5)+1,nc=gbits(&s,4)+4;static const u8 ord[19]={16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
            if(nl>286||nd>30)return -1;for(int i=0;i<19;i++)ln[i]=0;for(int i=0;i<nc;i++)ln[ord[i]]=gbits(&s,3);hbuild(&ch,ln,19);
            int idx=0;while(idx<nl+nd){int sym=hdec(&s,&ch);if(sym<0)return -1;if(sym<16)ln[idx++]=sym;else{int rep,v=0;if(sym==16){if(!idx)return -1;v=ln[idx-1];rep=3+gbits(&s,2);}else if(sym==17)rep=3+gbits(&s,3);else rep=11+gbits(&s,7);if(idx+rep>nl+nd)return -1;while(rep--)ln[idx++]=v;}}
            hbuild(&lh,ln,nl);hbuild(&dh,ln+nl,nd);if(codes(&s,&lh,&dh))return -1;}
        else return -1;
        if(s.err)return -1;}while(!last);
    return s.len;
}

/* ------------------------------------------------ PNG ------------------- */
static u32 be32(const u8*p){return ((u32)p[0]<<24)|(p[1]<<16)|(p[2]<<8)|p[3];}
u32 *png_decode(const u8*d,u32 n,int*ow,int*oh){
    static const u8 sig[8]={137,80,78,71,13,10,26,10};if(n<33||memcmp(d,sig,8))return 0;
    u32 w=0,h=0;int depth=0,ct=0,il=0;u8 pal[768];memset(pal,0,sizeof pal);u32 zl=0;
    for(u32 p=8;p+12<=n;){u32 l=be32(d+p);const u8*c=d+p+4;if(l>n||p+12+l>n)break;
        if(!memcmp(c,"IHDR",4)&&l>=13){w=be32(c+4);h=be32(c+8);depth=c[12];ct=c[13];il=c[16];}
        else if(!memcmp(c,"PLTE",4)&&l<=768)memcpy(pal,c+4,l);else if(!memcmp(c,"IDAT",4))zl+=l;else if(!memcmp(c,"IEND",4))break;p+=12+l;}
    if(!w||!h||w>8192||h>8192||(u32)(w*h)>6000000u||il||!zl)return 0;
    int ch=ct==0?1:ct==2?3:ct==3?1:ct==4?2:ct==6?4:0;if(!ch||(depth!=1&&depth!=2&&depth!=4&&depth!=8&&depth!=16))return 0;
    if((ct==2||ct==4||ct==6)&&depth<8)return 0;if(ct==3&&depth==16)return 0;
    u32 bits=ch*depth,stride=(w*bits+7)/8,bpp=bits<8?1:bits/8,raw=h*(stride+1);
    u8*z=kmalloc(zl),*rawb=kmalloc(raw);u32*out=kmalloc(w*h*4);if(!z||!rawb||!out){kfree(z);kfree(rawb);kfree(out);return 0;}
    u32 zo=0;for(u32 p=8;p+12<=n;){u32 l=be32(d+p);const u8*c=d+p+4;if(l>n||p+12+l>n)break;if(!memcmp(c,"IDAT",4)){memcpy(z+zo,c+4,l);zo+=l;}p+=12+l;}
    int got=zo>2?inflate_(z+2,zo-2,rawb,raw):-1;kfree(z);
    if(got<(int)raw){kfree(rawb);kfree(out);return 0;}
    u8*prev=0;
    for(u32 y=0;y<h;y++){u8*row=rawb+y*(stride+1)+1;int f=rawb[y*(stride+1)];
        for(u32 i=0;i<stride;i++){int a=i>=bpp?row[i-bpp]:0,b=prev?prev[i]:0,c=(prev&&i>=bpp)?prev[i-bpp]:0,x=row[i];
            if(f==1)x+=a;else if(f==2)x+=b;else if(f==3)x+=(a+b)/2;else if(f==4){int pp=a+b-c,pa=pp>a?pp-a:a-pp,pb=pp>b?pp-b:b-pp,pc=pp>c?pp-c:c-pp;x+=(pa<=pb&&pa<=pc)?a:(pb<=pc?b:c);}else if(f>4){kfree(rawb);kfree(out);return 0;}
            row[i]=x;}
        for(u32 x=0;x<w;x++){u32 r,g,b2,al=255;
            if(depth==8||depth==16){int st=depth/8;const u8*px=row+x*ch*st;
                if(ct==0){r=g=b2=px[0];}else if(ct==2){r=px[0];g=px[st];b2=px[2*st];}else if(ct==3){r=pal[px[0]*3];g=pal[px[0]*3+1];b2=pal[px[0]*3+2];}
                else if(ct==4){r=g=b2=px[0];al=px[st];}else{r=px[0];g=px[st];b2=px[2*st];al=px[3*st];}}
            else{u32 bi=x*depth;int v=(row[bi/8]>>(8-depth-(bi%8)))&((1<<depth)-1);if(ct==3){r=pal[v*3];g=pal[v*3+1];b2=pal[v*3+2];}else{r=g=b2=v*255/((1<<depth)-1);}}
            if(al<255){r=r*al/255;g=g*al/255;b2=b2*al/255;}
            out[y*w+x]=(r<<16)|(g<<8)|b2;}
        prev=row;}
    kfree(rawb);*ow=w;*oh=h;return out;
}

/* ------------------------------------------------ JPEG (baseline) ------- */
static const u8 zz[64]={0,1,8,16,9,2,3,10,17,24,32,25,18,11,4,5,12,19,26,33,40,48,41,34,27,20,13,6,7,14,21,28,35,42,49,56,57,50,43,36,29,22,15,23,30,37,44,51,58,59,52,45,38,31,39,46,53,60,61,54,47,55,62,63};
static const short IT[8][8]={{1448,2009,1892,1703,1448,1138,784,400},{1448,1703,784,-400,-1448,-2009,-1892,-1138},{1448,1138,-784,-2009,-1448,400,1892,1703},{1448,400,-1892,-1138,1448,1703,-784,-2009},{1448,-400,-1892,1138,1448,-1703,-784,2009},{1448,-1138,-784,2009,-1448,-400,1892,-1703},{1448,-1703,784,400,-1448,2009,-1892,1138},{1448,-2009,1892,-1703,1448,-1138,784,-400}};
typedef struct{u8 bits[17],val[256];int mincode[17],maxcode[18],valptr[17];int ok;}jh_t;
typedef struct{const u8*d;u32 n,pos;u32 bb;int bc;int err;}jb_t;
static int jbit(jb_t*b){if(!b->bc){u32 c=b->pos<b->n?b->d[b->pos]:0xFF;if(b->pos<b->n)b->pos++;
        if(c==0xFF){u32 c2=b->pos<b->n?b->d[b->pos]:0;if(c2==0)b->pos++;else{c=0;b->pos--;}}b->bb=c;b->bc=8;}
    b->bc--;return (b->bb>>b->bc)&1;}
static int jrecv(jb_t*b,int n){int v=0;while(n--)v=(v<<1)|jbit(b);return v;}
static int jext(int v,int n){return n&&v<(1<<(n-1))?v-(1<<n)+1:v;}
static void jhbuild(jh_t*h){int code=0,k=0;for(int l=1;l<=16;l++){h->valptr[l]=k;h->mincode[l]=code;code+=h->bits[l];k+=h->bits[l];h->maxcode[l]=h->bits[l]?code-1:-1;code<<=1;}h->maxcode[17]=0x7FFFFFFF;h->ok=1;}
static int jdec(jb_t*b,const jh_t*h){int code=0;for(int l=1;l<=16;l++){code=(code<<1)|jbit(b);if(h->maxcode[l]>=0&&code<=h->maxcode[l])return h->val[h->valptr[l]+code-h->mincode[l]];}b->err=1;return 0;}
typedef struct{int id,hs,vs,tq,dc,ac,pred;u8*pl;int pw,ph;}jc_t;
static void idct(const int*in,u8*out,int stride){
    int tmp[64];
    for(int y=0;y<8;y++)for(int x=0;x<8;x++){int s=0;for(int u=0;u<8;u++)s+=IT[x][u]*in[y*8+u];tmp[y*8+x]=s>>13;}
    for(int x=0;x<8;x++)for(int y=0;y<8;y++){int s=0;for(int v=0;v<8;v++)s+=IT[y][v]*tmp[v*8+x];s=(s>>11)+128;out[y*stride+x]=s<0?0:(s>255?255:s);}
}
u32 *jpg_decode(const u8*d,u32 n,int*ow,int*oh){
    if(n<4||d[0]!=0xFF||d[1]!=0xD8)return 0;
    int qt[4][64];jh_t ht[2][4];jc_t cs[3];int nc=0,w=0,h=0,ri=0,hmax=1,vmax=1;u32 p=2,scan=0;memset(ht,0,sizeof ht);memset(qt,0,sizeof qt);
    while(p+4<=n){
        if(d[p]!=0xFF){p++;continue;}int m=d[p+1];if(m==0xFF){p++;continue;}if(m==0xD8||m==0x01||(m>=0xD0&&m<=0xD7)){p+=2;continue;}
        int len=(d[p+2]<<8)|d[p+3];const u8*s=d+p+4;u32 end=p+2+len;if(end>n)return 0;
        if(m==0xC2||m==0xC3||(m>=0xC5&&m<=0xCF&&m!=0xC8&&m!=0xCC&&m!=0xC4))return 0;
        if(m==0xDB){const u8*q=s;while(q<d+end){int pq=q[0]>>4,tq=q[0]&3;q++;for(int i=0;i<64;i++){int v=pq?((q[0]<<8)|q[1]):q[0];q+=pq?2:1;qt[tq][zz[i]]=v;}}}
        else if(m==0xC0||m==0xC1){if(s[0]!=8)return 0;h=(s[1]<<8)|s[2];w=(s[3]<<8)|s[4];nc=s[5];if(nc!=1&&nc!=3)return 0;
            for(int i=0;i<nc;i++){cs[i].id=s[6+i*3];cs[i].hs=s[7+i*3]>>4;cs[i].vs=s[7+i*3]&15;cs[i].tq=s[8+i*3]&3;if(cs[i].hs<1||cs[i].hs>4||cs[i].vs<1||cs[i].vs>4)return 0;if(cs[i].hs>hmax)hmax=cs[i].hs;if(cs[i].vs>vmax)vmax=cs[i].vs;}}
        else if(m==0xC4){const u8*q=s;while(q<d+end){int tc=q[0]>>4,th=q[0]&3;q++;jh_t*t=&ht[tc&1][th];memset(t,0,sizeof *t);int tot=0;for(int i=1;i<=16;i++){t->bits[i]=q[i-1];tot+=q[i-1];}q+=16;if(tot>256)return 0;memcpy(t->val,q,tot);q+=tot;jhbuild(t);}}
        else if(m==0xDD)ri=(s[0]<<8)|s[1];
        else if(m==0xDA){int ns=s[0];if(ns!=nc)return 0;for(int i=0;i<ns;i++){int cid=s[1+i*2];for(int k=0;k<nc;k++)if(cs[k].id==cid){cs[k].dc=s[2+i*2]>>4;cs[k].ac=s[2+i*2]&15;}}scan=end;break;}
        p=end;
    }
    if(!scan||!w||!h||w>8192||h>8192||(u32)(w*h)>6000000u)return 0;
    int mcux=(w+8*hmax-1)/(8*hmax),mcuy=(h+8*vmax-1)/(8*vmax);
    for(int i=0;i<nc;i++){cs[i].pw=mcux*8*cs[i].hs;cs[i].ph=mcuy*8*cs[i].vs;cs[i].pl=kmalloc(cs[i].pw*cs[i].ph);cs[i].pred=0;if(!cs[i].pl){for(int k=0;k<i;k++)kfree(cs[k].pl);return 0;}}
    jb_t b;b.d=d;b.n=n;b.pos=scan;b.bb=0;b.bc=0;b.err=0;int cnt=0,fail=0;
    for(int my=0;my<mcuy&&!fail;my++)for(int mx=0;mx<mcux&&!fail;mx++){
        if(ri&&cnt&&cnt%ri==0){b.bc=0;while(b.pos+1<n&&!(d[b.pos]==0xFF&&d[b.pos+1]>=0xD0&&d[b.pos+1]<=0xD7))b.pos++;b.pos+=2;for(int i=0;i<nc;i++)cs[i].pred=0;}
        cnt++;
        for(int c=0;c<nc;c++)for(int by=0;by<cs[c].vs;by++)for(int bx=0;bx<cs[c].hs;bx++){
            int co[64];memset(co,0,sizeof co);const jh_t*dc=&ht[0][cs[c].dc&3],*ac=&ht[1][cs[c].ac&3];if(!dc->ok||!ac->ok){fail=1;break;}
            int t=jdec(&b,dc);int diff=t?jext(jrecv(&b,t),t):0;cs[c].pred+=diff;co[0]=cs[c].pred*qt[cs[c].tq][0];
            for(int k=1;k<64;){int rs=jdec(&b,ac),r=rs>>4,sz=rs&15;if(!sz){if(r==15){k+=16;continue;}break;}k+=r;if(k>63)break;co[zz[k]]=jext(jrecv(&b,sz),sz)*qt[cs[c].tq][zz[k]];k++;}
            idct(co,cs[c].pl+(my*cs[c].vs+by)*8*cs[c].pw+(mx*cs[c].hs+bx)*8,cs[c].pw);
            if(b.err){fail=1;break;}}
    }
    u32*out=fail?0:kmalloc(w*h*4);
    if(out)for(int y=0;y<h;y++)for(int x=0;x<w;x++){
        int Y=cs[0].pl[(y*cs[0].vs/vmax)*cs[0].pw+x*cs[0].hs/hmax],r=Y,g=Y,bl=Y;
        if(nc==3){int cb=cs[1].pl[(y*cs[1].vs/vmax)*cs[1].pw+x*cs[1].hs/hmax]-128,cr=cs[2].pl[(y*cs[2].vs/vmax)*cs[2].pw+x*cs[2].hs/hmax]-128;
            r=Y+((91881*cr)>>16);g=Y-((22554*cb+46802*cr)>>16);bl=Y+((116130*cb)>>16);}
        r=r<0?0:(r>255?255:r);g=g<0?0:(g>255?255:g);bl=bl<0?0:(bl>255?255:bl);out[y*w+x]=(r<<16)|(g<<8)|bl;}
    for(int i=0;i<nc;i++)kfree(cs[i].pl);
    if(out){*ow=w;*oh=h;}return out;
}
