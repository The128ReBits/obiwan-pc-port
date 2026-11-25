/* RECOMP_PROFILE=<seconds>: diagnostic sampling profiler, off by default.
 *
 * Every millisecond a helper thread suspends each other thread of the
 * process, reads its instruction pointer and resumes it. Every <seconds> it
 * prints, per thread, the functions the samples fell in (generated guest
 * functions keep their sub_XXXXXXXX names in the PDB) and starts a new
 * window. Nothing is allocated and no lock is taken while a thread is
 * suspended, so a thread stopped inside malloc or stdio cannot deadlock it.
 * It reports to stderr periodically rather than at exit because test runs
 * end with the process being killed. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
void profile_start(void);
#ifdef _WIN32
#include <windows.h>
#include <tlhelp32.h>
#include <dbghelp.h>
#include <mmsystem.h>
#pragma comment(lib,"winmm.lib")

#define MAX_THREADS 64
#define SLOTS (1u<<16)
#define MAX_NAMES 4096

static struct { uint64_t key; uint32_t count; } table[SLOTS];
static uint32_t lost;
static struct { DWORD tid; HANDLE h; uint32_t samples; } threads[MAX_THREADS];
static int nthreads;
struct name_count { int thread; char name[96]; uint32_t count; };
static struct name_count names[MAX_NAMES];
static int nnames;

static void refresh_threads(void) {
    DWORD self=GetCurrentThreadId(),pid=GetCurrentProcessId();
    HANDLE snap=CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD,0);
    THREADENTRY32 te;
    int i;
    if(snap==INVALID_HANDLE_VALUE) return;
    te.dwSize=sizeof te;
    for(BOOL ok=Thread32First(snap,&te);ok;ok=Thread32Next(snap,&te)) {
        if(te.th32OwnerProcessID!=pid || te.th32ThreadID==self) continue;
        for(i=0;i<nthreads && threads[i].tid!=te.th32ThreadID;i++);
        if(i<nthreads || nthreads==MAX_THREADS) continue;
        threads[i].h=OpenThread(THREAD_SUSPEND_RESUME|THREAD_GET_CONTEXT|THREAD_QUERY_INFORMATION,
                                FALSE,te.th32ThreadID);
        if(!threads[i].h) continue;
        threads[i].tid=te.th32ThreadID; threads[i].samples=0; nthreads++;
    }
    CloseHandle(snap);
}

static void record(int thread,uint64_t rip) {
    uint64_t key=((uint64_t)thread<<56)|(rip&0x00FFFFFFFFFFFFFFull);
    uint32_t i=(uint32_t)((key*0x9E3779B97F4A7C15ull)>>48)&(SLOTS-1),n;
    for(n=0;n<SLOTS;n++,i=(i+1)&(SLOTS-1)) {
        if(table[i].count && table[i].key!=key) continue;
        table[i].key=key; table[i].count++; return;
    }
    lost++;
}

static void add_name(int thread,const char *name,uint32_t count) {
    int i;
    for(i=0;i<nnames;i++)
        if(names[i].thread==thread && !strcmp(names[i].name,name)){names[i].count+=count;return;}
    if(nnames==MAX_NAMES){lost+=count;return;}
    names[nnames].thread=thread; names[nnames].count=count;
    snprintf(names[nnames].name,sizeof names[nnames].name,"%s",name); nnames++;
}

static int by_count(const void *a,const void *b) {
    uint32_t x=((const struct name_count *)a)->count,y=((const struct name_count *)b)->count;
    return x<y ? 1 : x>y ? -1 : 0;
}

static void report(unsigned window,double seconds) {
    HANDLE proc=GetCurrentProcess();
    char buf[sizeof(SYMBOL_INFO)+256];
    SYMBOL_INFO *sym=(SYMBOL_INFO *)buf;
    uint32_t i;
    int t,k;
    nnames=0;
    for(i=0;i<SLOTS;i++) {
        DWORD64 disp=0;
        char name[96];
        if(!table[i].count) continue;
        memset(buf,0,sizeof buf); sym->SizeOfStruct=sizeof(SYMBOL_INFO); sym->MaxNameLen=255;
        if(SymFromAddr(proc,(DWORD64)(table[i].key&0x00FFFFFFFFFFFFFFull),&disp,sym))
            snprintf(name,sizeof name,"%s",sym->Name);
        else snprintf(name,sizeof name,"?");
        add_name((int)(table[i].key>>56),name,table[i].count);
    }
    qsort(names,(size_t)nnames,sizeof names[0],by_count);
    fprintf(stderr,"[PERFIL] window %u: %.1f s%s\n",window,seconds,lost?" (some samples lost)":"");
    for(t=0;t<nthreads;t++) {
        int shown=0;
        if(!threads[t].samples) continue;
        fprintf(stderr,"[PERFIL]  thread %lu: %u samples\n",(unsigned long)threads[t].tid,threads[t].samples);
        for(k=0;k<nnames && shown<12;k++) {
            if(names[k].thread!=t) continue;
            fprintf(stderr,"[PERFIL]    %5.1f%%  %s\n",100.0*names[k].count/threads[t].samples,names[k].name);
            shown++;
        }
    }
    fflush(stderr);
}

static DWORD WINAPI sampler(LPVOID arg) {
    unsigned seconds=(unsigned)(uintptr_t)arg,window=0,tick=0;
    ULONGLONG start=GetTickCount64();
    CONTEXT ctx;
    int t;
    timeBeginPeriod(1);
    refresh_threads();
    for(;;) {
        Sleep(1);
        if(++tick%250==0) refresh_threads(); /* threads the title starts later */
        for(t=0;t<nthreads;t++) {
            if(SuspendThread(threads[t].h)==(DWORD)-1) continue;
            memset(&ctx,0,sizeof ctx); ctx.ContextFlags=CONTEXT_CONTROL;
            if(GetThreadContext(threads[t].h,&ctx)){record(t,ctx.Rip);threads[t].samples++;}
            ResumeThread(threads[t].h);
        }
        if(GetTickCount64()-start>=seconds*1000ull) {
            report(++window,(GetTickCount64()-start)/1000.0);
            memset(table,0,sizeof table); lost=0;
            for(t=0;t<nthreads;t++) threads[t].samples=0;
            refresh_threads();
            start=GetTickCount64();
        }
    }
}

void profile_start(void) {
    const char *v=getenv("RECOMP_PROFILE");
    int seconds=v ? atoi(v) : 0;
    HANDLE h;
    if(seconds<=0) return;
    h=CreateThread(NULL,0,sampler,(LPVOID)(uintptr_t)seconds,0,NULL);
    if(h){CloseHandle(h);fprintf(stderr,"[PERFIL] sampling every 1 ms, report every %d s\n",seconds);}
}
#else
void profile_start(void) {}
#endif
