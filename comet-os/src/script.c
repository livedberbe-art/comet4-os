/* CometScript: a tiny interpreted app format. Save a .cs file, press Run in Notes (or open it) and it becomes a window.
   Sections: lines before the first "on" run once; "on draw", "on tick", "on click", "on key". */
#include "kernel.h"
#define SLINES 400
typedef struct{char*src;int n,hd[4],he[4],init_end,node;int vars[26];int mx,my,key,cw,ch,steps;char err[60];u32 last;win_t*win;}sc_t;
static sc_t*S;static const char*P;
enum{H_DRAW,H_TICK,H_CLICK,H_KEY};
static char*lines[1];  /* placeholder to keep struct small */
typedef struct{char*l[SLINES];}lt_t;static lt_t*LT;

static void skipws(void){while(*P==' '||*P=='\t')P++;}
static int expr(void);
static int is_kw(const char*l,const char*k){int n=strlen(k);return !strncmp(l,k,n)&&(l[n]==0||l[n]==' '||l[n]=='\t');}
static int color_name(const char*id,int*v){
    static const struct{const char*n;int c;}t[]={{"black",0x000000},{"white",0xFFFFFF},{"red",0xEF4444},{"green",0x22C55E},{"blue",0x3B82F6},{"yellow",0xFACC15},{"orange",0xF97316},{"pink",0xEC4899},{"purple",0x8B5CF6},{"gray",0x6B7280},{"cyan",0x22D3EE}};
    for(unsigned i=0;i<sizeof t/sizeof t[0];i++)if(!strcmp(id,t[i].n)){*v=t[i].c;return 1;}
    if(!strcmp(id,"accent")){*v=T.accent;return 1;}if(!strcmp(id,"text")){*v=T.text;return 1;}if(!strcmp(id,"bg")){*v=T.win;return 1;}return 0;
}
static int primary(void){
    skipws();
    if(*P=='('){P++;int v=expr();skipws();if(*P==')')P++;return v;}
    if(*P=='-'){P++;return -primary();}
    if(*P=='!'){P++;return !primary();}
    if(*P>='0'&&*P<='9'){int v=0;if(P[0]=='0'&&P[1]=='x'){P+=2;for(;;){char c=*P;int d=c>='0'&&c<='9'?c-'0':(c>='a'&&c<='f'?c-'a'+10:(c>='A'&&c<='F'?c-'A'+10:-1));if(d<0)break;v=v*16+d;P++;}}else while(*P>='0'&&*P<='9')v=v*10+*P++-'0';return v;}
    if(*P>='a'&&*P<='z'){char id[12];int n=0;while(*P>='a'&&*P<='z'&&n<11)id[n++]=*P++;id[n]=0;
        if(n==1)return S->vars[id[0]-'a'];
        if(!strcmp(id,"mx"))return S->mx;if(!strcmp(id,"my"))return S->my;if(!strcmp(id,"cw"))return S->cw;if(!strcmp(id,"ch"))return S->ch;
        if(!strcmp(id,"key"))return S->key;if(!strcmp(id,"ticks"))return ticks;
        if(!strcmp(id,"rand")||!strcmp(id,"abs")||!strcmp(id,"min")||!strcmp(id,"max")){skipws();if(*P=='(')P++;int a=expr(),b=0;skipws();if(*P==','){P++;b=expr();}skipws();if(*P==')')P++;
            if(id[0]=='r')return a>0?(int)(rnd()%(u32)a):0;if(id[1]=='b')return a<0?-a:a;return id[1]=='i'?(a<b?a:b):(a>b?a:b);}
        int c;if(color_name(id,&c))return c;
        ksnprintf(S->err,60,"unknown name '%s'",id);return 0;}
    ksnprintf(S->err,60,"bad expression");if(*P)P++;return 0;
}
static int mul(void){int v=primary();for(;;){skipws();if(*P=='*'){P++;v*=primary();}else if(*P=='/'){P++;int d=primary();v=d?v/d:0;}else if(*P=='%'){P++;int d=primary();v=d?v%d:0;}else return v;}}
static int add(void){int v=mul();for(;;){skipws();if(*P=='+'){P++;v+=mul();}else if(*P=='-'){P++;v-=mul();}else return v;}}
static int cmp(void){int v=add();for(;;){skipws();
    if(P[0]=='='&&P[1]=='='){P+=2;v=v==add();}else if(P[0]=='!'&&P[1]=='='){P+=2;v=v!=add();}else if(P[0]=='<'&&P[1]=='='){P+=2;v=v<=add();}else if(P[0]=='>'&&P[1]=='='){P+=2;v=v>=add();}
    else if(P[0]=='<'){P++;v=v<add();}else if(P[0]=='>'){P++;v=v>add();}else return v;}}
static int land(void){int v=cmp();for(;;){skipws();if(P[0]=='&'&&P[1]=='&'){P+=2;int r=cmp();v=v&&r;}else return v;}}
static int expr(void){int v=land();for(;;){skipws();if(P[0]=='|'&&P[1]=='|'){P+=2;int r=land();v=v||r;}else return v;}}
static int str_arg(char*o,int max){
    skipws();if(*P!='"'){ksnprintf(S->err,60,"expected \"text\"");return 0;}P++;int n=0;
    while(*P&&*P!='"'&&n<max-12){if(*P=='$'&&P[1]>='a'&&P[1]<='z'){n+=ksnprintf(o+n,max-n,"%d",S->vars[P[1]-'a']);P+=2;}else o[n++]=*P++;}
    o[n]=0;if(*P=='"')P++;return 1;
}
static int text_or_rest(char*o,int max){skipws();if(*P=='"')return str_arg(o,max);int n=0;while(*P&&n<max-1)o[n++]=*P++;while(n>0&&o[n-1]==' ')n--;o[n]=0;return n>0;}
static int find_close(int pc,int stop,int want_else){
    int depth=0;for(int i=pc+1;i<stop;i++){const char*l=LT->l[i];if(is_kw(l,"if")||is_kw(l,"while"))depth++;else if(is_kw(l,"end")){if(depth==0)return i;depth--;}else if(want_else&&depth==0&&is_kw(l,"else"))return i;}
    return stop;
}
static int run(int pc,int stop){
    win_t*w=S->win;
    while(pc<stop){
        if(++S->steps>60000){ksnprintf(S->err,60,"step limit reached (infinite loop?)");return stop;}if(S->err[0])return stop;
        const char*l=LT->l[pc];if(!l[0]||l[0]=='#'){pc++;continue;}
        if(is_kw(l,"end")||is_kw(l,"else"))return pc;
        if(is_kw(l,"if")){P=l+2;int c=expr();int e=find_close(pc,stop,1);
            if(c){run(pc+1,e);if(e<stop&&is_kw(LT->l[e],"else"))e=find_close(e,stop,0);}
            else if(e<stop&&is_kw(LT->l[e],"else")){int e2=find_close(e,stop,0);run(e+1,e2);e=e2;}
            pc=e+1;continue;}
        if(is_kw(l,"while")){int e=find_close(pc,stop,0);for(;;){P=l+5;if(!expr()||S->err[0])break;run(pc+1,e);if(++S->steps>60000){ksnprintf(S->err,60,"step limit reached (infinite loop?)");break;}}pc=e+1;continue;}
        if(is_kw(l,"let")){P=l+3;skipws();if(*P>='a'&&*P<='z'&&!(P[1]>='a'&&P[1]<='z')){int v=*P-'a';P++;skipws();if(*P=='=')P++;S->vars[v]=expr();}else ksnprintf(S->err,60,"let needs a letter a-z");}
        else if(is_kw(l,"fill")){P=l+4;int c=expr();if(w)ui_bg(w,c);}
        else if(is_kw(l,"rect")){P=l+4;int x=expr(),y=expr(),ww=expr(),hh=expr(),c=expr();if(w)fill_rect(w->cx+x,w->cy+y,ww,hh,c);}
        else if(is_kw(l,"rrect")){P=l+5;int x=expr(),y=expr(),ww=expr(),hh=expr(),r=expr(),c=expr();if(w)rrect(w->cx+x,w->cy+y,ww,hh,r,c,255);}
        else if(is_kw(l,"circle")){P=l+6;int x=expr(),y=expr(),r=expr(),c=expr();if(w&&r>0&&r<400)circle_ex(w->cx+x,w->cy+y,r,0,c,255);}
        else if(is_kw(l,"line")){P=l+4;int a=expr(),b=expr(),c=expr(),d=expr(),col=expr();if(w)line(w->cx+a,w->cy+b,w->cx+c,w->cy+d,col,2);}
        else if(is_kw(l,"text")){P=l+4;int x=expr(),y=expr();char b[96];if(str_arg(b,96)){int c=expr(),sc=1;skipws();if(*P){int sv=expr();sc=CLAMP(sv,1,8);}if(w)draw_text(w->cx+x,w->cy+y,b,c,sc);}}
        else if(is_kw(l,"say")){P=l+3;char b[96];if(text_or_rest(b,96))toast(b);}
        else if(is_kw(l,"title")){P=l+5;char b[48];if(text_or_rest(b,48)&&w)ui_title(w,b);}
        else if(is_kw(l,"beep")){P=l+4;speaker(expr());}
        else if(is_kw(l,"size")||is_kw(l,"on")){}
        else ksnprintf(S->err,60,"unknown command: %s",l);
        pc++;
    }
    return pc;
}
static int sc_load(win_t*w,sc_t*s,int node){
    if(s->src)kfree(s->src);if(!LT)LT=kmalloc(sizeof(lt_t));
    s->src=kmalloc(fsn[node].size+2);if(!s->src||!LT)return 0;memcpy(s->src,fsn[node].data,fsn[node].size);s->src[fsn[node].size]=0;
    s->n=0;memset(s->hd,-1,sizeof s->hd);s->init_end=-1;memset(s->vars,0,sizeof s->vars);s->err[0]=0;
    S=s;char*p=s->src;
    while(*p&&s->n<SLINES){char*e=p;while(*e&&*e!='\n')e++;char*nx=*e?e+1:e;*e=0;if(e>p&&e[-1]=='\r')e[-1]=0;while(*p==' '||*p=='\t')p++;LT->l[s->n++]=p;p=nx;}
    int cur=-1;for(int i=0;i<s->n;i++){const char*l=LT->l[i];
        if(is_kw(l,"on")){if(s->init_end<0)s->init_end=i;if(cur>=0)s->he[cur]=i;cur=-1;
            if(!strcmp(l,"on draw"))cur=H_DRAW;else if(!strcmp(l,"on tick"))cur=H_TICK;else if(!strcmp(l,"on click"))cur=H_CLICK;else if(!strcmp(l,"on key"))cur=H_KEY;
            if(cur>=0)s->hd[cur]=i+1;}}
    if(cur>=0)s->he[cur]=s->n;if(s->init_end<0)s->init_end=s->n;
    char t[48];ksnprintf(t,48,"%s",fsn[node].name);ui_title(w,t);s->win=w;s->cw=w->cw;s->ch=w->ch;s->steps=0;
    run(0,s->init_end);return 1;
}
static void call(win_t*w,int h){
    sc_t*s=(sc_t*)w->d;if(s->hd[h]<0||s->err[0])return;S=s;s->win=w;s->steps=0;s->cw=w->cw;s->ch=w->ch;s->mx=mouse_x-w->cx;s->my=mouse_y-w->cy;
    run(s->hd[h],s->he[h]);
}
static void sc_init(win_t*w){sc_t*s=kzalloc(sizeof(sc_t));w->d=s;w->mem=64;int n=g_arg;if(n>0&&fsn[n].used&&!fsn[n].dir&&fsn[n].data)s->node=n;else{scopy(s->err,"no script file",60);return;}sc_load(w,s,n);}
static void sc_close(win_t*w){sc_t*s=(sc_t*)w->d;if(s&&s->src)kfree(s->src);speaker(0);}
static void sc_draw(win_t*w){
    sc_t*s=(sc_t*)w->d;ui_bg(w,T.win);call(w,H_DRAW);
    if(s->err[0]){ui_fill(w,0,w->ch-44,w->cw,44,0x3B0D12,0);ui_text(w,10,w->ch-34,"Script error:",0xFCA5A5,1);ui_text(w,10,w->ch-20,s->err,0xFFFFFF,1);}
}
static void sc_click(win_t*w,int x,int y,int b,int d){sc_t*s=(sc_t*)w->d;(void)x;(void)y;(void)d;if(b!=1)return;s->key=0;call(w,H_CLICK);}
static void sc_key(win_t*w,int k,int m){sc_t*s=(sc_t*)w->d;(void)m;s->key=k;call(w,H_KEY);s->key=0;}
static void sc_tick(win_t*w){sc_t*s=(sc_t*)w->d;if(w->minimized||s->hd[H_TICK]<0)return;if(ticks-s->last<3)return;s->last=ticks;call(w,H_TICK);wm_redraw();}
void script_register(void){
    apps[APP_SCRIPT]=(app_t){.name="Script",.desc="Runs CometScript (.cs) apps",.dw=640,.dh=480,.minw=240,.minh=180,.installed=1,.init=sc_init,.draw=sc_draw,.key=sc_key,.click=sc_click,.tick=sc_tick,.close=sc_close};
}
