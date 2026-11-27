/* Comet OS apps part 2: Browser (HTTP text browser + downloads), Music (PC speaker), Shield (file scanner) */
#include "kernel.h"
#define ST(type) type*s=(type*)w->d
static int lcc(int c){return (c>='A'&&c<='Z')?c+32:c;}
static u32 fgon2(u32 c){int l=(((c>>16)&255)*299+((c>>8)&255)*587+(c&255)*114)/1000;return l>150?0x0B1020:0xFFFFFF;}

/* ================================================== Browser ============ */
typedef struct{char url[200];int ul,scroll,shown,nl,req,hn,bin;char*text;int tl;char links[40][120];char hist[8][120];char status[90];char base[120];}br_t;
static void br_init(win_t*w){br_t*s=kzalloc(sizeof(br_t));w->d=s;s->text=kzalloc(65536);w->mem=70;scopy(s->url,"http://example.com",200);s->ul=strlen(s->url);scopy(s->status,"Type an http:// address and press Enter. (HTTPS sites are not supported yet.)",90);}
static void br_close(win_t*w){ST(br_t);if(s&&s->text)kfree(s->text);http_cancel();}
static void br_go(br_t*s,const char*u){
    char b[200];if(!strncmp(u,"http",4))scopy(b,u,200);else ksnprintf(b,200,"http://%s",u);
    if(s->hn<8)scopy(s->hist[s->hn++],s->url,120);scopy(s->url,b,200);s->ul=strlen(s->url);s->shown=0;s->req=1;s->scroll=0;s->bin=0;s->tl=0;s->text[0]=0;s->nl=0;
    http_start(b);
}
static int hdr_is_text(void){
    for(u32 k=0;k<(u32)http.hdr_end;k++)if((k==0||http.body[k-1]=='\n')&&!strncmp((char*)http.body+k,"Content-Type:",13)){
        char t[60];int n=0;u32 j=k+13;while(j<(u32)http.hdr_end&&http.body[j]!='\r'&&n<59)t[n++]=lcc(http.body[j++]);t[n]=0;
        return stricontains(t,"text")||stricontains(t,"html")||stricontains(t,"xml")||stricontains(t,"json")||stricontains(t,"javascript");}
    return 1;
}
static void html2text(br_t*s){
    const u8*b=http.body+http.hdr_end;u32 n=http.len>(u32)http.hdr_end?http.len-http.hdr_end:0;if(n>300000)n=300000;
    char*o=s->text;int oi=0,cap=65000,skip=0,lastsp=1,cur=-1;s->nl=0;
#define NL(k) do{while(oi>0&&o[oi-1]==' ')oi--;if(oi>0&&o[oi-1]!='\n'&&oi<cap){o[oi++]='\n';}if((k)&&oi>1&&o[oi-2]!='\n'&&oi<cap)o[oi++]='\n';lastsp=1;}while(0)
    for(u32 i=0;i<n&&oi<cap-8;i++){
        char c=b[i];
        if(c=='<'){
            if(i+3<n&&b[i+1]=='!'&&b[i+2]=='-'&&b[i+3]=='-'){u32 j=i+4;while(j+2<n&&!(b[j]=='-'&&b[j+1]=='-'&&b[j+2]=='>'))j++;i=j+2;continue;}
            char tag[12];int t=0,cl=0;u32 j=i+1;if(j<n&&b[j]=='/'){cl=1;j++;}
            while(j<n&&t<11&&((b[j]>='a'&&b[j]<='z')||(b[j]>='A'&&b[j]<='Z')||(b[j]>='0'&&b[j]<='9')))tag[t++]=lcc(b[j++]);tag[t]=0;
            u32 e=j;while(e<n&&b[e]!='>')e++;
            if(!strcmp(tag,"script")||!strcmp(tag,"style"))skip=!cl;
            else if(!skip){
                if(!strcmp(tag,"br")||!strcmp(tag,"tr")||!strcmp(tag,"div")||!strcmp(tag,"ul")||!strcmp(tag,"ol"))NL(0);
                else if(!strcmp(tag,"p")||(tag[0]=='h'&&tag[1]>='1'&&tag[1]<='6'&&!tag[2])||!strcmp(tag,"title"))NL(1);
                else if(!strcmp(tag,"li")&&!cl){NL(0);o[oi++]='-';o[oi++]=' ';}
                else if(!strcmp(tag,"a")&&!cl&&s->nl<40){for(u32 k=j;k+5<e;k++)if(lcc(b[k])=='h'&&lcc(b[k+1])=='r'&&lcc(b[k+2])=='e'&&lcc(b[k+3])=='f'&&b[k+4]=='='){u32 q=k+5;char qc=(b[q]=='"'||b[q]=='\'')?b[q++]:' ';int m=0;while(q<n&&b[q]!=qc&&b[q]!='>'&&m<119)s->links[s->nl][m++]=b[q++];s->links[s->nl][m]=0;cur=s->nl++;break;}}
                else if(!strcmp(tag,"a")&&cl&&cur>=0){oi+=ksnprintf(o+oi,cap-oi," [%d]",cur+1);cur=-1;lastsp=0;}
            }
            i=e;continue;
        }
        if(skip)continue;
        if(c=='&'){const char*ent[]={"&amp;","&lt;","&gt;","&quot;","&nbsp;","&#39;"};const char rep[]={'&','<','>','"',' ','\''};int hit=0;
            for(int k=0;k<6;k++){int l=strlen(ent[k]);if(i+l<=n&&!memcmp(b+i,ent[k],l)){c=rep[k];i+=l-1;hit=1;break;}}if(!hit){while(i<n&&b[i]!=';'&&b[i]!=' '&&b[i]!='<')i++;if(i<n&&b[i]=='<')i--;c=' ';}}
        if(c=='\r'||c=='\n'||c=='\t'||c==' '){if(!lastsp){o[oi++]=' ';lastsp=1;}continue;}
        if(c<32||c>126)c='?';o[oi++]=c;lastsp=0;
    }
    o[oi]=0;s->tl=oi;
}
static void br_save(win_t*w,br_t*s){
    if(http.state!=5||!http.body){toast("Nothing to save yet");return;}
    char nm[32];const char*p=s->url;if(!strncmp(p,"http://",7))p+=7;const char*sl=strchr_(p,'/');const char*last=sl?sl:0;
    const char*q=last;while(q&&*q){if(*q=='/'&&q[1])last=q;q++;}char base[32];
    if(last&&last[1]&&last[1]!='?'){int m=0;const char*r=last+1;while(*r&&*r!='?'&&*r!='#'&&m<28)base[m++]=*r++;base[m]=0;}else{int m=0;while(p[m]&&p[m]!='/'&&p[m]!=':'&&m<24){base[m]=p[m];m++;}base[m]=0;scopy(base+m,".html",8);}
    if(!strchr_(base,'.'))scopy(base+strlen(base),".html",8);
    fs_unique(fs_dl,base,nm);int n=fs_create(fs_dl,nm,0);if(n>0&&!fs_write(n,http.body+http.hdr_end,http.len-http.hdr_end)){char t[64];ksnprintf(t,64,"Saved to Downloads/%s",nm);toast(t);}else toast("Could not save");
}
static void br_draw(win_t*w){
    ST(br_t);ui_bg(w,T.win);int cw=w->cw,pan=cw>780?230:0;
    ui_button(w,1,8,8,34,28,"<",0);ui_button(w,2,46,8,34,28,"R",0);ui_fill(w,86,8,cw-86-136,28,T.field,14);ui_text(w,100,18,s->url,T.text,1);
    if(((ticks/50)&1))fill_rect(w->cx+100+s->ul*8,w->cy+14,1,16,T.accent);ui_button(w,3,cw-44,8,36,28,"Go",1);ui_button(w,4,cw-90,8,40,28,"Save",0);
    int rows=(w->ch-44-26)/12,cols=(cw-pan-30)/8;int ln=0,i=0;
    const char*st=s->status;if(http.state==1)st="Looking up host...";else if(http.state==2)st="Connecting...";else if(http.state==3)st="Waiting for the server...";else if(http.state==4){static char b[60];ksnprintf(b,60,"Receiving... %d bytes",http.len);st=b;}
    else if(http.state==6&&s->req)st=http.err;
    if(s->shown&&!s->bin){
        while(i<s->tl){int j=i,ls=-1,col=0;while(j<s->tl&&s->text[j]!='\n'&&col<cols){if(s->text[j]==' ')ls=j;j++;col++;}if(j<s->tl&&s->text[j]!='\n'&&ls>i)j=ls;
            if(ln>=s->scroll&&ln<s->scroll+rows){char t[160];int m=j-i>159?159:j-i;memcpy(t,s->text+i,m);t[m]=0;ui_text(w,14,50+(ln-s->scroll)*12,t,T.text,1);}
            ln++;i=j;if(i<s->tl&&(s->text[i]==' '||s->text[i]=='\n'))i++;}
    }else if(s->shown&&s->bin){char b[96];ksnprintf(b,96,"Binary file (%d bytes). Press Save to put it in Downloads.",http.len-http.hdr_end);ui_text(w,14,60,b,T.text,1);}
    else ui_text(w,14,60,"Comet Browser - text mode. Plain http:// pages only.",T.dim,1);
    if(pan){rrect_ex(w->cx+cw-pan,w->cy+44,pan,w->ch-44-26,0,T.win2,255,0,0);ui_text(w,cw-pan+12,54,"Links",T.dim,1);
        for(int k=0;k<s->nl&&k<(w->ch-120)/22;k++){char t[40];char l[28];scopy(l,s->links[k],26);ksnprintf(t,40,"[%d] %s",k+1,l);int y=74+k*22;if(w_hover(w,cw-pan+4,y,pan-8,20))ui_fill(w,cw-pan+4,y,pan-8,20,T.hover,6);ui_text(w,cw-pan+12,y+6,t,T.text,1);w_hit(w,cw-pan+4,y,pan-8,20,100+k);}}
    fill_rect(w->cx,w->cy+w->ch-26,cw,26,T.win2);ui_text(w,10,w->ch-17,st,T.dim,1);
}
static void br_link(br_t*s,int k){
    char u[200];const char*l=s->links[k];if(!strncmp(l,"http",4))scopy(u,l,200);
    else{char host[64];const char*p=s->url;if(!strncmp(p,"http://",7))p+=7;int m=0;while(p[m]&&p[m]!='/'&&m<63){host[m]=p[m];m++;}host[m]=0;
        if(l[0]=='/')ksnprintf(u,200,"http://%s%s",host,l);else{char dir[120];scopy(dir,p,120);char*sl=0;for(char*c=dir;*c;c++)if(*c=='/')sl=c;if(sl)sl[1]=0;else{scopy(dir+strlen(dir),"/",4);}ksnprintf(u,200,"http://%s%s",dir,l);}}
    br_go(s,u);
}
static void br_action(win_t*w,int id,int d){
    ST(br_t);(void)d;
    if(id==1&&s->hn>0){char u[120];scopy(u,s->hist[--s->hn],120);int hn=s->hn;br_go(s,u);s->hn=hn;}
    else if(id==2||id==3)br_go(s,s->url);else if(id==4)br_save(w,s);else if(id>=100&&id<140)br_link(s,id-100);
}
static void br_key(win_t*w,int k,int m){
    ST(br_t);(void)m;
    if(k=='\n')br_go(s,s->url);else if(k=='\b'){if(s->ul)s->url[--s->ul]=0;}
    else if(k==K_DOWN)s->scroll+=3;else if(k==K_UP)s->scroll=s->scroll>3?s->scroll-3:0;else if(k==K_PGDN)s->scroll+=20;else if(k==K_PGUP)s->scroll=s->scroll>20?s->scroll-20:0;
    else if(k>=32&&k<127&&s->ul<190){s->url[s->ul++]=k;s->url[s->ul]=0;}
}
static void br_scroll(win_t*w,int dz){ST(br_t);s->scroll=CLAMP(s->scroll+dz*3,0,100000);}
static void br_tick(win_t*w){
    ST(br_t);if(!s->req)return;if(http.state>=1&&http.state<=4)wm_redraw();
    if(http.state==5&&!s->shown){s->shown=1;s->bin=!hdr_is_text();if(!s->bin)html2text(s);ksnprintf(s->status,90,"Done - HTTP %d, %d bytes",http.status,http.len-http.hdr_end);wm_redraw();}
    if(http.state==6&&s->shown==0){s->shown=1;s->bin=0;s->tl=0;scopy(s->status,http.err,90);wm_redraw();}
}

/* ================================================== Music (PC speaker) == */
#define N(h,d) h,d
static const u16 ode[]={330,25,330,25,349,25,392,25,392,25,349,25,330,25,294,25,262,25,262,25,294,25,330,25,330,37,294,12,294,50,0xFFFF};
static const u16 twinkle[]={262,30,262,30,392,30,392,30,440,30,440,30,392,60,349,30,349,30,330,30,330,30,294,30,294,30,262,60,0xFFFF};
static const u16 comet[]={262,12,330,12,392,12,523,12,659,12,784,24,659,12,523,12,392,12,330,12,262,24,0,12,294,12,370,12,440,12,587,12,740,12,880,36,0xFFFF};
static const u16 march[]={392,20,392,20,392,20,311,15,466,5,392,20,311,15,466,5,392,40,587,20,587,20,587,20,622,15,466,5,370,20,311,15,466,5,392,40,0xFFFF};
static const struct{const char*n;const u16*d;}songs[]={{"Comet Theme (original)",comet},{"Ode to Joy",ode},{"Twinkle Twinkle",twinkle},{"Imperial-style March",march}};
typedef struct{int cur,pos,playing;u32 end;}mu_t;
static void mu_init(win_t*w){w->d=kzalloc(sizeof(mu_t));}
static void mu_stop(mu_t*s){s->playing=0;speaker(0);}
static void mu_close(win_t*w){speaker(0);}
static void mu_draw(win_t*w){
    ST(mu_t);ui_bg(w,T.win);fill_grad(w->cx,w->cy,w->cw,110,blend(T.accent,0,80),0x6D28D9);ui_text(w,20,20,"Music",0xFFFFFF,3);
    ui_text(w,22,60,s->playing?songs[s->cur].n:"Choose a track",0xFFFFFF,1);ui_text(w,22,80,"Plays through the PC speaker (square-wave notes). Needs a VM with speaker emulation.",0xDDD6FE,1);
    for(int i=0;i<4;i++){int y=124+i*40,on=i==s->cur&&s->playing;if(on||w_hover(w,16,y,w->cw-32,36))ui_fill(w,16,y,w->cw-32,36,on?T.accent:T.hover,8);
        ui_text(w,32,y+14,songs[i].n,on?fgon2(T.accent):T.text,1);ui_text(w,w->cw-60,y+14,on?"[||]":"[>]",on?fgon2(T.accent):T.dim,1);w_hit(w,16,y,w->cw-32,36,10+i);}
    int bw=(w->cw-40)/24;for(int i=0;i<24;i++){int h=s->playing?20+(isin(ticks*7+i*37)+1024)*50/2048+(isin(ticks*13+i*91)+1024)*20/2048:6;rrect(w->cx+20+i*bw,w->cy+w->ch-30-h,bw-4,h,3,T.accent,s->playing?220:90);}
    ui_button(w,1,w->cw-112,w->ch-46,92,30,"Stop",0);
}
static void mu_action(win_t*w,int id,int d){ST(mu_t);(void)d;if(id>=10&&id<14){s->cur=id-10;s->pos=0;s->playing=1;s->end=0;}else if(id==1)mu_stop(s);}
static void mu_tick(win_t*w){
    ST(mu_t);if(!s->playing)return;wm_redraw();if(ticks<s->end)return;const u16*d=songs[s->cur].d;
    if(d[s->pos*2]==0xFFFF){mu_stop(s);return;}speaker(d[s->pos*2]);s->end=ticks+d[s->pos*2+1]*2;s->pos++;
}

/* ================================================== Shield (scanner) === */
typedef struct{int idx,scanning,scanned,nfound;struct{int node;char why[40];}f[12];u32 t0;int done;}sh_t;
static const struct{const char*sig;const char*why;}sigs[]={{"EICAR-STANDARD-ANTIVIRUS-TEST-FILE","EICAR test signature"},{"rm -rf /","Destructive shell command"},{"powershell -enc","Encoded PowerShell"},{"format c:","Disk format command"},{"cmd.exe /c del","Windows delete command"}};
static int memfind(const u8*h,u32 hl,const char*n){u32 nl=strlen(n);if(nl>hl)return 0;for(u32 i=0;i+nl<=hl;i++)if(!memcmp(h+i,n,nl))return 1;return 0;}
static void sh_init(win_t*w){w->d=kzalloc(sizeof(sh_t));}
static void sh_tick(win_t*w){
    ST(sh_t);if(!s->scanning)return;wm_redraw();
    for(int k=0;k<12&&s->idx<FS_MAX;k++,s->idx++){int n=s->idx;if(n<1||!fsn[n].used||fsn[n].dir||!fsn[n].data)continue;s->scanned++;
        for(unsigned g=0;g<sizeof sigs/sizeof sigs[0];g++)if(memfind(fsn[n].data,fsn[n].size,sigs[g].sig)&&s->nfound<12){s->f[s->nfound].node=n;scopy(s->f[s->nfound].why,sigs[g].why,40);s->nfound++;break;}}
    if(s->idx>=FS_MAX){s->scanning=0;s->done=1;}
}
static void sh_draw(win_t*w){
    ST(sh_t);ui_bg(w,T.win);int bad=s->nfound>0;u32 col=bad?T.bad:T.good;
    ui_fill(w,16,16,w->cw-32,110,T.win2,14);draw_icon(APP_SHIELD,w->cx+34,w->cy+34,64);
    ui_text(w,116,34,s->scanning?"Scanning...":(bad?"Threats found":(s->done?"No threats found":"Ready to scan")),col,2);
    char b[80];ksnprintf(b,80,"%d files scanned    %d threat%s",s->scanned,s->nfound,s->nfound==1?"":"s");ui_text(w,116,66,b,T.dim,1);
    ui_text(w,116,86,"Basic signature scanner for files inside Comet OS.",T.dim,1);ui_button(w,1,w->cw-130,30,100,32,"Scan now",1);ui_button(w,2,w->cw-130,70,100,28,"Test file",0);
    for(int i=0;i<s->nfound;i++){int y=140+i*40;ui_fill(w,16,y,w->cw-32,36,T.win2,8);char p[64];fs_path(s->f[i].node,p,64);ui_text(w,28,y+8,p,T.text,1);ui_text(w,28,y+22,s->f[i].why,T.bad,1);
        ui_button(w,20+i,w->cw-230,y+4,100,28,"Quarantine",2);ui_button(w,40+i,w->cw-124,y+4,96,28,"Delete",0);}
    ui_text(w,16,w->ch-24,"Not a replacement for a real antivirus: it knows only a few test and command patterns.",T.dim,1);
}
static void sh_action(win_t*w,int id,int d){
    ST(sh_t);(void)d;
    if(id==1){s->idx=1;s->scanned=0;s->nfound=0;s->scanning=1;s->done=0;}
    else if(id==2){static const char e[]="X5O!P%@AP[4\\PZX54(P^)7CC)7}$EICAR-STANDARD-ANTIVIRUS-TEST-FILE!$H+H*\n";char nm[32];fs_unique(fs_dl,"eicar-test.txt",nm);int n=fs_create(fs_dl,nm,0);if(n>0)fs_write(n,e,sizeof e-1);toast("Created Downloads/eicar-test.txt (harmless test file)");}
    else if(id>=20&&id<32){int n=s->f[id-20].node;int q=fs_find(0,"quarantine");if(q<0)q=fs_create(0,"quarantine",1);if(q>0&&fsn[n].used){char nm[32];fs_unique(q,fsn[n].name,nm);scopy(fsn[n].name,nm,32);fsn[n].parent=q;toast("Moved to /quarantine");}
        for(int k=id-20;k<s->nfound-1;k++)s->f[k]=s->f[k+1];s->nfound--;}
    else if(id>=40&&id<52){int n=s->f[id-40].node;fs_remove(n);for(int k=id-40;k<s->nfound-1;k++)s->f[k]=s->f[k+1];s->nfound--;}
}
int shield_scan_node(int n,char*why){
    if(n<=0||!fsn[n].used)return 0;
    if(fsn[n].dir){for(int i=1;i<FS_MAX;i++)if(fsn[i].used&&fsn[i].parent==n&&shield_scan_node(i,why))return 1;return 0;}
    if(!fsn[n].data)return 0;
    for(unsigned g=0;g<sizeof sigs/sizeof sigs[0];g++)if(memfind(fsn[n].data,fsn[n].size,sigs[g].sig)){scopy(why,sigs[g].why,40);return 1;}
    if(fsn[n].size>2&&fsn[n].data[0]=='M'&&fsn[n].data[1]=='Z'){scopy(why,"Windows program (cannot run here)",40);return 1;}
    return 0;
}
void apps2_init(void){
    apps[APP_BROWSER]=(app_t){.name="Browser",.desc="Text browser for plain http pages, with downloads",.dw=960,.dh=620,.minw=520,.minh=360,.installed=1,.init=br_init,.draw=br_draw,.key=br_key,.action=br_action,.scroll=br_scroll,.tick=br_tick,.close=br_close};
    apps[APP_MUSIC]=(app_t){.name="Music",.desc="Melody player (PC speaker)",.dw=560,.dh=460,.minw=420,.minh=380,.installed=1,.init=mu_init,.draw=mu_draw,.action=mu_action,.tick=mu_tick,.close=mu_close};
    apps[APP_SHIELD]=(app_t){.name="Shield",.desc="Scan files for threats",.dw=640,.dh=520,.minw=520,.minh=360,.installed=1,.init=sh_init,.draw=sh_draw,.action=sh_action,.tick=sh_tick};
    script_register();
}
