#ifndef KERNEL_H
#define KERNEL_H
typedef unsigned char u8; typedef unsigned short u16; typedef unsigned int u32; typedef unsigned long long u64;
typedef signed char i8; typedef signed short i16; typedef signed int i32; typedef signed long long i64;
typedef u32 size_t;
#define NULL ((void*)0)
#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))
#define ABS(a) ((a)<0?-(a):(a))
#define CLAMP(v,lo,hi) ((v)<(lo)?(lo):((v)>(hi)?(hi):(v)))
typedef __builtin_va_list va_list;
#define va_start(v,l) __builtin_va_start(v,l)
#define va_arg(v,t) __builtin_va_arg(v,t)
#define va_end(v) __builtin_va_end(v)

static inline void outb(u16 p,u8 v){__asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p));}
static inline u8 inb(u16 p){u8 v;__asm__ volatile("inb %1,%0":"=a"(v):"Nd"(p));return v;}
static inline void outw(u16 p,u16 v){__asm__ volatile("outw %0,%1"::"a"(v),"Nd"(p));}
static inline u16 inw(u16 p){u16 v;__asm__ volatile("inw %1,%0":"=a"(v):"Nd"(p));return v;}
static inline void outl(u16 p,u32 v){__asm__ volatile("outl %0,%1"::"a"(v),"Nd"(p));}
static inline u32 inl(u16 p){u32 v;__asm__ volatile("inl %1,%0":"=a"(v):"Nd"(p));return v;}
static inline void insw(u16 p,void*b,u32 n){__asm__ volatile("rep insw":"+D"(b),"+c"(n):"d"(p):"memory");}
static inline void outsw(u16 p,const void*b,u32 n){__asm__ volatile("rep outsw":"+S"(b),"+c"(n):"d"(p));}
static inline u64 rdtsc(void){u32 lo,hi;__asm__ volatile("rdtsc":"=a"(lo),"=d"(hi));return ((u64)hi<<32)|lo;}
static inline void cli(void){__asm__ volatile("cli");}
static inline void sti(void){__asm__ volatile("sti");}
static inline void hlt(void){__asm__ volatile("hlt");}

/* ---------- shared settings / counters ---------- */
extern int g_radius,g_glass,g_blur,g_tbh,g_animdur,g_xray,g_live,g_trail,g_hotcorners,g_hour;
extern u32 g_prims,fs_gen,g_fsops,g_netops,irq_timer,irq_kbd,irq_mouse,frames_sec;
extern const char*net_owner; extern int g_sbx_net,g_sbx_quota_kb;
typedef struct{u32 t;char owner[12];char text[60];u32 bytes;}netlog_t; extern netlog_t netlog[48]; extern int netlog_n;
typedef struct{char name[12];u32 reqs,bytes;}netapp_t; extern netapp_t netapp[8]; extern int netapp_n;
void net_log_as(const char*owner,const char*text,u32 bytes);
typedef struct{char name[12];int used,has,accent,dark;u32 hash;}profile_t; extern profile_t prof[4]; extern int cur_prof,sel_prof;
extern unsigned char cfg_ui[8];
/* ---------- kernel.c ---------- */
extern volatile u32 ticks;            /* 100 Hz */
extern u32 mem_total_kb;
extern char cpu_brand[64], cpu_vendor[16];
typedef struct { u32 gs,fs,es,ds,edi,esi,ebp,esp,ebx,edx,ecx,eax,int_no,err,eip,cs,eflags; } regs_t;
typedef struct { u16 vendor,device; u8 cls,sub,bus,dev,fn; u32 bar0; } pci_dev_t;
extern pci_dev_t pci_devs[64]; extern int pci_count;
void *kmalloc(u32 n); void *kzalloc(u32 n); void kfree(void *p);
u32 heap_used(void); u32 heap_total(void);
void *memset(void*,int,size_t); void *memcpy(void*,const void*,size_t);
void *memmove(void*,const void*,size_t); int memcmp(const void*,const void*,size_t);
void memcpy32(u32*d,const u32*s,u32 n); void memset32(u32*d,u32 v,u32 n);
size_t strlen(const char*); char *strchr_(const char*s,int c); int strcmp(const char*,const char*); int strncmp(const char*,const char*,size_t);
char *strcpy(char*,const char*); void scopy(char*d,const char*s,int n);
int ksnprintf(char*buf,int n,const char*fmt,...);
int stricontains(const char*hay,const char*needle);
int ends_with(const char*s,const char*suf);
int atoi_(const char*s);
void kpanic(const char*msg,u32 code,u32 eip);
void rtc_read(int*h,int*m,int*s,int*d,int*mo,int*y);
void poweroff(void); void reboot(void);
int pct(u32 a,u32 b); u32 isqrt(u32 v); int isin(int deg); int icos(int deg); u32 rnd(void);
u32 pci_cfg(u8 b,u8 d,u8 f,u8 off); int pci_find_class(u8 cls,u8*b,u8*d,u8*f);

/* ---------- gfx.c ---------- */
extern int SW,SH,bga_ok; extern u32 *bb,*wall,*wallb,*tmpb;
typedef struct { int x0,y0,x1,y1; } cliprect;
int gfx_init(u32 addr,int w,int h,int pitch,int bpp); int gfx_setmode(int w,int h);
void gfx_present(void); void gfx_panic(const char*a,const char*b);
cliprect clip_push(int x,int y,int w,int h); void clip_pop(cliprect c); void clip_reset(void);
u32 blend(u32 d,u32 s,int a);
void fill_rect(int x,int y,int w,int h,u32 c); void fill_rect_a(int x,int y,int w,int h,u32 c,int a);
void fill_grad(int x,int y,int w,int h,u32 c1,u32 c2);
void rrect_ex(int x,int y,int w,int h,int r,u32 c,int a,int mask,int mode);
#define rrect(x,y,w,h,r,c,a) rrect_ex(x,y,w,h,r,c,a,15,0)
#define rmica(x,y,w,h,r,c,a) rrect_ex(x,y,w,h,r,c,a,15,1)
void rframe(int x,int y,int w,int h,int r,u32 c,int a);
void circle_ex(int cx,int cy,int r,int th,u32 c,int a);
void line(int x0,int y0,int x1,int y1,u32 c,int t);
void tri_up(int cx,int y,int h,int base,u32 c);
void draw_char(int x,int y,int ch,u32 c,int sc);
void draw_text(int x,int y,const char*s,u32 c,int sc);
void draw_text_b(int x,int y,const char*s,u32 c,int sc);
int text_w(const char*s,int sc);
void blur_rect(u32*buf,int x,int y,int w,int h,int r); void blur_region(int x,int y,int w,int h,int r);
void acrylic(int x,int y,int w,int h,int r,u32 tint,int ta);
void shadow(int x,int y,int w,int h,int r,int spread);
void wall_generate(int dark,u32 accent); void wall_live(u32 tick);
void cursor_set(int kind); void cursor_hide(void); void cursor_show(int x,int y);
void post_fx(int bright,int night);
void draw_icon(int app,int x,int y,int s); void icon_color(u32 fg,u32 bg);
enum { IC_FOLDER=100,IC_FILE,IC_IMAGE,IC_ARCHIVE,IC_WIFI,IC_SPEAKER,IC_BATT,IC_SEARCH,IC_TASKVIEW,IC_WIDGETS,IC_POWER,IC_SUN,IC_MOON,IC_AIR,IC_NIGHT,IC_FOCUS,IC_CLOSE,IC_MIN,IC_MAX };
void draw_logo(int cx,int cy,int s,u32 c);
void gauge(int cx,int cy,int r,int pct,u32 col,u32 track);
void blit_scaled(const u32*src,int sw,int sh,int dx,int dy,int dw,int dh);

/* ---------- input.c ---------- */
enum { K_UP=0x100,K_DOWN,K_LEFT,K_RIGHT,K_HOME,K_END,K_PGUP,K_PGDN,K_DEL,K_INS,
       K_F1,K_F2,K_F3,K_F4,K_F5,K_F6,K_F7,K_F8,K_F9,K_F10,K_F11,K_F12 };
enum { M_SHIFT=1,M_CTRL=2,M_ALT=4,M_WIN=8 };
enum { EV_KEY=1,EV_MBTN,EV_WHEEL,EV_WINTAP,EV_ALTUP,EV_WINUP };
typedef struct { int type,a,b,c; } event_t;
extern volatile int mouse_x,mouse_y,mouse_b,mouse_moved,mods_now; extern int mouse_wheel_ok;
void input_init(void); int ev_pop(event_t*e); void kbd_irq(void); void mouse_irq(void);
void mouse_clamp(void);

/* ---------- fs.c ---------- */
typedef struct { char name[32]; int parent,dir,used; u8*data; u32 size,cap; u8*prev; u32 prevsz; int oparent; } fnode;
#define FS_MAX 320
extern fnode fsn[FS_MAX];
void fs_init(void); int fs_find(int dir,const char*name); int fs_create(int dir,const char*name,int isdir);
int fs_resolve(int cwd,const char*path); int fs_write(int n,const void*d,u32 len); int fs_append(int n,const void*d,u32 len);
int fs_remove(int n); void fs_path(int n,char*out,int max); int fs_children(int dir,int*out,int max);
u32 fs_used(void); int fs_nodes(void); int fs_rename(int n,const char*name);
int fs_unique(int dir,const char*base,char*out);
int fs_search(const char*q,int*out,int max);
typedef struct { char name[100]; u32 size; int dir; } tar_ent;
int tar_list(const u8*d,u32 len,tar_ent*out,int max);
int tar_extract(int tarnode,int destdir);
int tar_create(int outnode,int*nodes,int n);
extern int fs_home,fs_pics,fs_docs,fs_desk,fs_dl,fs_apps,fs_trash;
u8*fs_pack(u32*outlen); int fs_unpack(const u8*b,u32 len); void fs_set_user(const char*name); int fs_make_home(const char*name);
int fs_copy_tree(int src,int dstdir); int sbx_dir(void); void sbx_reset(void); u32 sbx_usage(void); int sbx_import(int node); int sbx_snapshot_save(void); int sbx_snapshot_restore(void); int sbx_template_save(const char*name);
int fs_persist_save(void); int fs_persist_load(void); extern u32 fs_saved_gen; extern int fs_sbx;
int fs_trash_node(int n); int fs_untrash(int n); int fs_restore_prev(int n);
u32 *img_load(int node,int*w,int*h,int*owned);
int img_save_cim(int dir,const char*name,const u32*px,int w,int h);

/* ---------- disk.c ---------- */
typedef struct { int present,atapi; u32 base,ctrl,drive; u32 sectors; char model[44]; } ata_dev;
extern ata_dev ata[4];
typedef struct { int state; u32 total,done; char msg[80]; int src,dst; } inst_t;
int disk_persist_write(const u8*buf,u32 len); int disk_persist_read(u8**buf,u32*len);
extern inst_t inst; extern int g_installed;
void disk_init(void); void inst_detect(void); void inst_start(void); void inst_step(void);
void cfg_load(void); void cfg_save(void);
extern int cfg_theme,cfg_accent,cfg_res,cfg_startup,cfg_removed,cfg_nightsched,cfg_haspw; extern u32 cfg_pwhash;
u32 pw_hash(const char*pw);

/* ---------- wm.c ---------- */
enum { APP_TERM,APP_FILES,APP_NOTES,APP_MON,APP_TASK,APP_STORE,APP_VIEW,APP_GAME,APP_SET,APP_SETUP,APP_CALC,APP_PAINT,APP_SNAKE,APP_METEORITE,APP_BROWSER,APP_MUSIC,APP_SHIELD,APP_SCRIPT,APP_SANDBOX,APP_NETMON,APP_UILAB,APP_COUNT };
typedef struct { int x,y,w,h,id; } hit_t;
typedef struct win {
    int used,pid,app,x,y,w,h,rx,ry,rw,rh,max,minimized,desktop,closing;
    int anim_kind,anim_t0,fx,fy,fw,fh,tx,ty,tw,th;
    char title[48]; void*d; int cx,cy,cw,ch; int nhits; hit_t hits[100];
    u64 cycles; u32 cpu; u32 mem; u32 born;
    int sbx,pin,pip,prx,pry,prw,prh; u32 prims,events,fsops,netpk,frames;
} win_t;
typedef struct { u32 text,dim,win,win2,tint,hover,border,field,accent,accent2,good,bad; } theme_t;
typedef struct app {
    const char*name,*desc; int dw,dh,minw,minh,installed,store;
    void(*init)(win_t*); void(*draw)(win_t*); void(*key)(win_t*,int,int);
    void(*action)(win_t*,int,int); void(*click)(win_t*,int,int,int,int);
    void(*drag)(win_t*,int,int); void(*release)(win_t*,int,int); void(*scroll)(win_t*,int);
    void(*tick)(win_t*); void(*close)(win_t*);
} app_t;
extern app_t apps[APP_COUNT]; extern theme_t T; extern int g_dark,g_anim,g_night,g_bright,g_vol,g_air,g_focus;
extern u32 g_accent; extern int g_arg; extern win_t wins[]; extern int cpu_pct,cpu_hist[60],comp_pct;
extern const u32 accents[8]; extern const char*accent_names[8];
void wm_init(void); void wm_run(void); void theme_apply(void);
win_t*win_open(int app); win_t*win_open_arg(int app,int arg); void win_close(win_t*w); void win_focus(win_t*w);
void toast(const char*msg); void ui_text(win_t*w,int x,int y,const char*s,u32 c,int sc);
void ui_fill(win_t*w,int x,int y,int bw,int bh,u32 c,int r); void ui_bg(win_t*w,u32 c);
int w_hover(win_t*w,int x,int y,int bw,int bh); void w_hit(win_t*w,int x,int y,int bw,int bh,int id);
void ui_button(win_t*w,int id,int x,int y,int bw,int bh,const char*label,int style);
void ui_bar(win_t*w,int x,int y,int bw,int bh,int pct,u32 col);
void ui_title(win_t*w,const char*t);
int proc_list(int*pids,char names[][24],int max); int proc_kill(int pid);
int proc_info(int pid,u32*cpu,u32*mem);
extern void splash(int step);
extern int ui_res_set(int w,int h);

/* ---------- net.c ---------- */
typedef struct { int nic; char nicname[32]; u8 mac[6]; u32 ip,mask,gw,dns; int cfg,dhcp,rx,tx; } net_t;
extern net_t net;
typedef struct { int state; char host[64],path[160]; u16 port; u32 ip,t0,tlast,len,cap; u8*body; int status,redirects; char err[56]; int hdr_end; } http_t;
extern http_t http;
void net_init(void); void net_poll(void); void net_dhcp_start(void);
int net_resolve(const char*name,u32*ip);
int net_ping_start(const char*host); int net_ping_state(u32*rtt_ticks,u32*ip);
int http_start(const char*url); void http_cancel(void);
void ip_str(u32 ip,char*o);
const char*net_status(void);
void speaker(int hz);
/* ---------- script.c ---------- */
int script_new_app(void);
/* ---------- apps.c ---------- */
win_t*win_open_sbx(int app,int arg); void ui_settings_save(void); int prof_add(const char*name,const char*pw); int prof_remove(int k); void prof_display(int k,char*o);
void wm_power(int kind); void wallpaper_set_node(int node); void wallpaper_accent(void); void wm_workspace(int id);
extern int g_wallnode,g_nightsched; int pw_set(const char*pw); void cb_push(const char*s,int len); const char*cb_get(int i); extern int cb_n;
void app_uninstall(int app); void app_install(int app); int app_removable(int app); int app_mem_kb(int app);
void wall_from_image(const u32*px,int w,int h); u32 *png_decode(const unsigned char*d,u32 n,int*ow,int*oh); u32 *jpg_decode(const unsigned char*d,u32 n,int*ow,int*oh);
int is_img_name(const char*n);
int shield_scan_node(int n,char*why); int sbx_test_file(int node);
void apps2_init(void); void script_register(void);
void wm_redraw(void); void open_node(int n);
void apps_init(void); void term_exec_public(win_t*w,const char*cmd);
#endif
