#ifndef _WIN32
#define _GNU_SOURCE
#endif
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
/* Windows host: VEH instead of signals, CNG instead of OpenSSL, and the
 * framework's own framebuffer window thread instead of the SDL main loop. */
#include <windows.h>
#include <bcrypt.h>
#include <dbghelp.h>
#include <io.h>
#include <direct.h>
#include <sys/stat.h>
#else
#include <signal.h>
#include <ucontext.h>
#include <execinfo.h>
#include <unistd.h>
#include <sys/stat.h>
#include <openssl/evp.h>
#endif
#include "kernel.h"
#include "xbox_memory_layout.h"
#include "video_player.h"
#include "apu.h"
#include "recomp_types.h"
#ifdef _WIN32
#define PATH_MAX MAX_PATH
#define STDERR_FILENO 2
#define write(fd,buf,len) _write((fd),(buf),(unsigned)(len))
#endif

/* (Claude completo, multidioma) The title-layer addresses per edition, found
 * by code signature in each XBE (tools/anclas_claude_completo.py, report in
 * reports/regional/anclas_claude_completo.json). The generated C is made from
 * ONE XBE, so a build runs only the edition it was generated from
 * (OBIWAN_EDICION_GENERADA, "usa" unless the build says otherwise); another
 * known XBE is named in the log instead of "Unsupported XBE". */
#include "obiwan_ediciones.h"
#ifndef OBIWAN_EDICION_GENERADA
#define OBIWAN_EDICION_GENERADA "usa"
#endif
static const obiwan_edicion_t *g_ed;
#define ENTRY (g_ed->entry)
static void *image;
static size_t image_size;
static char game_dir[PATH_MAX],save_dir[PATH_MAX];
static int init_only;
extern MCPXAPUState *g_apu_state;
extern void xbox_path_init(const char *,const char *);
extern void nv2a_pb_set_semaphore_target(uint32_t guest_va);
extern void nv2a_pb_set_semaphore_pointer(uint32_t ptr_va);
extern void xbox_OhciInit(void);
extern void xbox_D3dGpuWaitEvent(uint32_t device_ptr_va, uint32_t event_off,
                                 uint32_t method);
extern int xbox_Nv2aMirrorCounter(uint32_t device_ptr_va,
                                  uint32_t src_off, uint32_t dst_off);

/* write() and _exit() keep the fatal path independent of stdio locks/allocators. */
static void fault_hex(const char *label, size_t len, uint64_t value) {
    char digits[17]; const char *hex="0123456789abcdef";
    for(int i=0;i<16;i++)digits[i]=hex[(value >> (60-4*i))&15];digits[16]='\n';
    (void)write(STDERR_FILENO,label,len);(void)write(STDERR_FILENO,digits,sizeof digits);
}
#ifdef _WIN32
/* Trapped hardware registers (USB OHCI, APU) are decoded and stepped over;
 * every other exception is left to the next handler, as on POSIX where only
 * SIGSEGV/SIGBUS reach the MMIO decoder. */
static LONG CALLBACK mmio_veh(PEXCEPTION_POINTERS ep) {
    if(ep->ExceptionRecord->ExceptionCode==EXCEPTION_ACCESS_VIOLATION && xbox_VehMmioFault(ep))
        return EXCEPTION_CONTINUE_EXECUTION;
    return EXCEPTION_CONTINUE_SEARCH;
}
/* Unhandled exceptions end the process with the same report as the POSIX path. */
static LONG WINAPI fault(PEXCEPTION_POINTERS ep) {
    char buf[sizeof(SYMBOL_INFO)+256];SYMBOL_INFO *sym=(SYMBOL_INFO *)buf;DWORD64 disp=0;
    const char msg[]="[FATAL] host exception; guest execution stopped\n";
    (void)write(STDERR_FILENO,msg,sizeof(msg)-1);
    fault_hex("exception_code=0x",17,ep->ExceptionRecord->ExceptionCode);
    fault_hex("fault_address=0x",16,ep->ExceptionRecord->NumberParameters>1 ?
              (uint64_t)ep->ExceptionRecord->ExceptionInformation[1] : 0);
    fault_hex("host_pc=0x",10,(uint64_t)ep->ContextRecord->Rip);
    fault_hex("guest_esp=0x",12,g_esp);
    fault_hex("guest_eax=0x",12,g_eax);
    memset(buf,0,sizeof buf);sym->SizeOfStruct=sizeof(SYMBOL_INFO);sym->MaxNameLen=255;
    if(SymFromAddr(GetCurrentProcess(),(DWORD64)ep->ContextRecord->Rip,&disp,sym))
        fprintf(stderr,"host_symbol=%s+0x%llx\n",sym->Name,(unsigned long long)disp);
    fflush(NULL);
    TerminateProcess(GetCurrentProcess(),128+11);
    return EXCEPTION_EXECUTE_HANDLER;
}
static int install_handlers(void) {
    SymSetOptions(SYMOPT_DEFERRED_LOADS|SYMOPT_UNDNAME);
    SymInitialize(GetCurrentProcess(),NULL,TRUE);
    SetUnhandledExceptionFilter(fault);
    return AddVectoredExceptionHandler(1,mmio_veh)!=NULL;
}
static int sha256_hex(const void *data,size_t size,char text[65]) {
    unsigned char md[32];
    if(size>ULONG_MAX || BCryptHash(BCRYPT_SHA256_ALG_HANDLE,NULL,0,(PUCHAR)data,(ULONG)size,md,sizeof md)!=0)return 0;
    for(unsigned i=0;i<32;i++)snprintf(text+2*i,3,"%02x",md[i]);
    return 1;
}
static char *full_directory(const char *path,char *out) {
    struct _stat st;
    if(!_fullpath(out,path,PATH_MAX)){errno=ENAMETOOLONG;return NULL;}
    if(_stat(out,&st)!=0)return NULL;
    if(!(st.st_mode&_S_IFDIR)){errno=ENOTDIR;return NULL;}
    return out;
}
#define realpath full_directory
#else
static void fault(int sig,siginfo_t *info,void *ctx) {
    void *frames[24];
    int frame_count;
    if(xbox_PosixMmioFault(ctx,(uintptr_t)info->si_addr))return;
    const char msg[]="[FATAL] host signal; guest execution stopped\n";
    (void)write(STDERR_FILENO,msg,sizeof(msg)-1);
    fault_hex("fault_address=0x",16,(uintptr_t)info->si_addr);
    ucontext_t *uc=ctx;
    fault_hex("host_pc=0x",10,(uint64_t)uc->uc_mcontext.gregs[REG_RIP]);
    fault_hex("guest_esp=0x",12,g_esp);
    fault_hex("guest_eax=0x",12,g_eax);
    frame_count = backtrace(frames, (int)(sizeof frames / sizeof frames[0]));
    (void)write(STDERR_FILENO, "native_backtrace:\n", 18);
    backtrace_symbols_fd(frames, frame_count, STDERR_FILENO);
    _exit(128+sig);
}
static int install_handlers(void) {
    struct sigaction sa={0};sa.sa_sigaction=fault;sa.sa_flags=SA_SIGINFO;sigemptyset(&sa.sa_mask);
    return sigaction(SIGSEGV,&sa,NULL)==0 && sigaction(SIGBUS,&sa,NULL)==0;
}
static int sha256_hex(const void *data,size_t size,char text[65]) {
    unsigned char md[EVP_MAX_MD_SIZE];unsigned int count=0;
    if(!EVP_Digest(data,size,md,&count,EVP_sha256(),NULL) || count!=32)return 0;
    for(unsigned i=0;i<32;i++)snprintf(text+2*i,3,"%02x",md[i]);
    return 1;
}
#endif
static int load_and_check(void) {
    char path[PATH_MAX];
    if(snprintf(path,sizeof path,"%s/default.xbe",game_dir)>=(int)sizeof path)return 0;
    FILE *f=fopen(path,"rb");if(!f){perror(path);return 0;}
    if(fseek(f,0,SEEK_END)!=0){fclose(f);return 0;}
    long length=ftell(f);
    if(length<=0 || length>(32L<<20) || fseek(f,0,SEEK_SET)!=0){fprintf(stderr,"Unsupported XBE size\n");fclose(f);return 0;}
    image_size=(size_t)length;image=malloc(image_size);
    if(!image){fclose(f);return 0;}
    size_t got=fread(image,1,image_size,f);fclose(f);if(got!=image_size)return 0;
    char text[65];
    if(!sha256_hex(image,image_size,text))return 0;
    g_ed=NULL;
    for(size_t i=0;i<sizeof obiwan_ediciones/sizeof obiwan_ediciones[0];i++)
        if(!strcmp(text,obiwan_ediciones[i].sha256) && image_size==obiwan_ediciones[i].size)g_ed=&obiwan_ediciones[i];
    if(!g_ed){fprintf(stderr,"Unsupported XBE SHA-256: %s\n",text);return 0;}
    if(strcmp(g_ed->edicion,OBIWAN_EDICION_GENERADA)){
        fprintf(stderr,"[BOOT] XBE of the %s edition recognized, but this build's code was generated from the %s XBE;"
                " the %s edition needs its own build (anchors %s)\n",g_ed->edicion,OBIWAN_EDICION_GENERADA,
                g_ed->edicion,g_ed->resuelta?"already mapped":"not mapped");
        return 0;
    }
    fprintf(stderr,"[BOOT] exact Obi-Wan XBE verified (%zu bytes, edition %s)\n",image_size,g_ed->edicion);return 1;
}
/* RECOMP_MEDIR=1 (diagnostic, off by default): one switch for the frame-time
 * and audio measures that already exist, because the launcher clears each of
 * them by name ("Jugar Obi-Wan 3.exe medir" sets it).
 *   FPS, per second: [PORTPERF] (frames, draws, render-thread ms in the
 *   pushbuffer walk and in Present); per [IRQ] report: [GATE] (how long the
 *   one-CPU DISPATCH gate was held and waited for); every two seconds:
 *   [TEXSTAT] (texture binds from the memo or checked again) and the
 *   KeStallExecutionProcessor sites. Render-bound if walk_ms is near the
 *   wall-clock ms; otherwise the game thread or the gate limits.
 *   Audio, per second: [AUDIO-HOST] (samples handed to XAudio2 against
 *   samples played, starves, front-end trap spells), [APU-RING] (music rings
 *   the title wrote too late, replayed laps); every 5 s: [APU-IRQ]
 *   se_frames, which must rise 7500 (1500 frames of 32 samples a second). */
static void medir_env(void) {
    static const char *const vars[]={"RECOMP_GPU_TIMING","RECOMP_GATE_STATS",
        "RECOMP_TEX_STATS","RECOMP_STALL_SITES","RECOMP_AUDIO_HOST","RECOMP_APU_RING",
        "RECOMP_START_TRACE"};
    const char *v=getenv("RECOMP_MEDIR");
    size_t i;
    if(!v || *v!='1') return;
    for(i=0;i<sizeof vars/sizeof vars[0];i++){
#ifdef _WIN32
        char s[64];snprintf(s,sizeof s,"%s=1",vars[i]);_putenv(s);
#else
        setenv(vars[i],"1",1);
#endif
    }
    fprintf(stderr,"[MEDIR] on: GPU_TIMING GATE_STATS TEX_STATS STALL_SITES AUDIO_HOST APU_RING START_TRACE\n");
}
static int guest_main(void) {
    medir_env();
    /* The menu makes and frees small vertex buffers every frame (sub_002E1110);
     * kept forever, they fill the 64 MB contiguous arena in under half a
     * minute at the Direct3D 11 renderer's frame rate. RECOMP_CONTIG_RECLAIM=0
     * keeps them, as before. */
#ifdef _WIN32
    if(!getenv("RECOMP_CONTIG_RECLAIM"))_putenv("RECOMP_CONTIG_RECLAIM=1");
#else
    setenv("RECOMP_CONTIG_RECLAIM","1",0);
#endif
    if(!xbox_MemoryLayoutInit(image,image_size)){fprintf(stderr,"[BOOT] memory initialization failed\n");return 3;}
    fprintf(stderr,"[BOOT] memory layout initialized\n");
    /* The same switch arms the codec and APU MMIO traps in the runtime.
     * Both halves must be present before the guest starts DirectSound. */
    if (getenv("RECOMP_AC97_READY")) {
        g_apu_state = mcpx_apu_init_standalone((uint8_t *)xbox_GetMemoryBase());
        if (!g_apu_state) {
            fprintf(stderr,"[BOOT] emulated APU initialization failed\n");
            xbox_MemoryLayoutShutdown();
            return 3;
        }
        fprintf(stderr,"[BOOT] emulated APU initialized\n");
    }
    /* D3D device 0x003770A0 points its GPU-read sequence word at this guest
     * address (device+0x3F0 -> 0x8001D000). Its make-space path waits for
     * BACK_END_WRITE_SEMAPHORE_RELEASE to advance this word. */
    nv2a_pb_set_semaphore_target(0x8001D000u);
    /* The pointer itself, read at each release: with freed contiguous
     * memory reused (RECOMP_CONTIG_RECLAIM) the word is elsewhere. */
    nv2a_pb_set_semaphore_pointer(g_ed->d3d_device + 0x3F0u);
    xbox_kernel_init();xbox_path_init(game_dir,save_dir);xbox_kernel_bridge_init();
    /* D3D's two-entry swap throttle compares submitted (+0x2B60) against
     * GPU-completed (+0x2518) on the device pointer at 0x00379C30. The
     * software executor consumes commands synchronously, so mirror that
     * completion field rather than letting the title wait for a GPU write
     * that this host does not produce. */
    if (xbox_Nv2aMirrorCounter(g_ed->d3d_device_ptr, 0x2B60u, 0x2518u) != 0)
        fprintf(stderr, "[BOOT] could not register D3D completion counter\n");
    /* D3D's GPU wait (sub_003705B0) patches a NOP with software method
     * 0x310 and sleeps on device+0x2500 with no timeout; on the console the
     * GPU interrupt handler (sub_00372190, switch at 0x372398 -> 0x3722A2)
     * sets that event. USA addresses, like the counter above. */
    xbox_D3dGpuWaitEvent(g_ed->d3d_device_ptr, 0x2500u, 0x310u);
    /* Bring up the optional Xbox USB controller model after guest memory,
     * kernel bridges, and fault handlers are ready. */
    xbox_OhciInit();
    g_esp=XBOX_STACK_TOP;
    if(!recomp_dispatch_init())fprintf(stderr,"[BOOT] flat dispatch unavailable; using binary search\n");
    recomp_func_t entry=recomp_lookup(ENTRY);
    if(!entry){fprintf(stderr,"[BOOT] missing generated entry 0x%08x\n",ENTRY);return 4;}
    fprintf(stderr,"[BOOT] runtime initialized; entry=0x%08x esp=0x%08x\n",ENTRY,g_esp);
    if(init_only){
        xbox_kernel_shutdown();
        if(g_apu_state){mcpx_apu_shutdown(g_apu_state);g_apu_state=NULL;}
        xbox_MemoryLayoutShutdown();return 0;
    }
    if (getenv("RECOMP_OBIWAN_D3D_SOFT_COMPLETE") &&
        !strcmp(getenv("RECOMP_OBIWAN_D3D_SOFT_COMPLETE"), "1")) {
        volatile uint32_t *complete = (volatile uint32_t *)(uintptr_t)
                (g_ed->d3d_soft_complete + xbox_GetMemoryOffset());
        *complete = 1;
        fprintf(stderr, "[DIAGNOSTIC] software D3D queue completion enabled\n");
    }
    xbox_WatchdogStart();
    fprintf(stderr,"[BOOT] entering recompiled game\n");entry();
    fprintf(stderr,"[BOOT] entry returned; stopping diagnostic process\n");
    /* Xbox startup may create worker threads. Do not unmap their memory while alive. */
    fflush(NULL);_exit(0);
}
/* OBIWAN_NIVEL=<level>: start at that level, for testing.
 *
 * The title's developer hook is T:\Options.script, which it runs on every
 * boot; "NextLevel <level>" in it plays the level-change sequence. That
 * sequence ends in a quick reboot (the runtime relaunches this executable,
 * see title_relaunch in kernel_bridge.c), and the title loads the level on
 * the far side of it from its launch data. Left in place, the hook runs
 * again on that second boot, pauses the game for a new level change while
 * the first one is loading, and the load never resumes -- the "stuck on the
 * loading screen" hang. So the file is written for the first boot only and
 * removed on the relaunched one, the way a developer would have deleted it.
 * The title itself never writes Options.script. */
void profile_start(void); /* perfil.c, RECOMP_PROFILE */

static void level_shortcut(void) {
    const char *level=getenv("OBIWAN_NIVEL");
    const char *kept=getenv("RECOMP_LAUNCH_PAGE");
    char path[PATH_MAX+64];
    size_t i,n;
    FILE *f;

    if(!level || !*level) return;
    n=strlen(level);
    for(i=0;i<n;i++)
        if(!(level[i]=='_' || (level[i]>='0'&&level[i]<='9') ||
             (level[i]>='A'&&level[i]<='Z') || (level[i]>='a'&&level[i]<='z'))) break;
    if(i<n || n>31){fprintf(stderr,"[NIVEL] ignored: '%s' is not a level name\n",level);return;}
    snprintf(path,sizeof path,"%s/TDATA/4c410001/Options.script",save_dir);
    if(kept && *kept){
        if(remove(path)==0) fprintf(stderr,"[NIVEL] relaunched for %s: start script removed\n",level);
        return;
    }
    /* The title creates T: on its first boot; this runs before that one. */
    snprintf(path,sizeof path,"%s/TDATA",save_dir);
#ifdef _WIN32
    _mkdir(path);
#else
    mkdir(path,0755);
#endif
    snprintf(path,sizeof path,"%s/TDATA/4c410001",save_dir);
#ifdef _WIN32
    _mkdir(path);
#else
    mkdir(path,0755);
#endif
    snprintf(path,sizeof path,"%s/TDATA/4c410001/Options.script",save_dir);
    f=fopen(path,"wb");
    if(!f){fprintf(stderr,"[NIVEL] cannot write %s\n",path);return;}
    fprintf(f,"NextLevel %s\r\n",level);fclose(f);
    fprintf(stderr,"[NIVEL] first boot: NextLevel %s\n",level);
}
int main(int argc,char **argv) {
    setvbuf(stderr,NULL,_IONBF,0);
    if(argc<3 || argc>4 || (argc==4 && strcmp(argv[3],"--check-input") && strcmp(argv[3],"--init-only"))){
        fprintf(stderr,"Usage: %s GAME_DIRECTORY SAVE_DIRECTORY [--check-input|--init-only]\n",argv[0]);return 2;}
    if(!realpath(argv[1],game_dir)){perror("game directory");return 2;}
    /* Require an explicit, existing save root; do not default to HOME during bring-up. */
    if(!realpath(argv[2],save_dir)){perror("save directory");return 2;}
    /* Upstream path storage is MAX_PATH-sized; leave room for its subdirectories. */
    if(strlen(game_dir)>180 || strlen(save_dir)>180){fprintf(stderr,"Runtime paths too long\n");return 2;}
    if(!strcmp(game_dir,save_dir)){fprintf(stderr,"Game and save directories must differ\n");return 2;}
    if(!load_and_check()){free(image);return 2;}
    if(argc==4 && !strcmp(argv[3],"--check-input")){free(image);return 0;}
    init_only=argc==4;
    if(!install_handlers()){perror("fault handlers");free(image);return 3;}
    if(!init_only){level_shortcut();profile_start();}
#ifdef _WIN32
    /* fb_present.c opens its window on its own thread once the title sets a
     * display mode (RECOMP_FB_WINDOW=1); the guest runs on this thread. */
    int result=guest_main();
#else
    int result=init_only ? guest_main() : xbox_HostWindowMain(guest_main);
#endif
    free(image);return result;
}
