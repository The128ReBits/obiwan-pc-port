/* (Claude completo-9) Opt-in fixed function on D3D11. Keep the CPU formulae,
 * including their existing lighting limitations, divide/rebuild and TEXGEN
 * order. Numeric values never enter the shader key. Pending/failed variants
 * use the CPU; the HWVP worker compiles each discrete variant only once.
 * Shadow repair reads stencil, not g_verts. HUD adjustment is program-only;
 * SHAPE/BIGTRI/CPU-address hooks and debug marking stay on the CPU. */
typedef struct {
    uint32_t light, mask, colmat, d0, d1, vc, tg[16], tm[4];
} FfKey;
typedef struct {
    FfKey key;
    volatile LONG state, claimed; /* 0 pending, 1 ready, 2 failed */
    ID3D11VertexShader *vs;
    ID3D11InputLayout *il;
    ID3D11GeometryShader *so;
    char *src;
} FfShader;
#define FF_GPU_MAX 128
static FfShader *g_ff_shader[FF_GPU_MAX];
static volatile LONG g_ff_n;
static int g_ff_on = -1, g_ff_ab = -1;
static uint32_t g_ff_ab_done;
static DWORD g_ff_ab_start, g_ff_ab_after;
static ID3D11Buffer *g_ff_cb, *g_ff_vb, *g_ff_so, *g_ff_read;
static UINT g_ff_vb_cap, g_ff_vb_pos, g_ff_so_cap;
static float *g_ff_inputs;
static uint32_t g_ff_inputs_cap;
static uint64_t g_ff_cb_gen;
enum { FF_OFF, FF_MODE, FF_SKIN, FF_HOOK, FF_STATE, FF_PENDING, FF_FAILED,
       FF_NUMERIC, FF_RESOURCE, FF_AB, FF_REASONS };
static const char *g_ff_reason[FF_REASONS] = {
    "off", "mode", "skinning", "cpu_hook", "state", "shader_pending",
    "shader_failed", "nonfinite_or_extreme", "resource", "ab_cpu"
};
static uint32_t g_ff_gpu, g_ff_fallback[FF_REASONS];

static int ffgpu_enabled(void)
{
    if (g_ff_on < 0) {
        const char *e = getenv("RECOMP_FF_GPU_AB");
        g_ff_ab = e ? atoi(e) : 0;
        if (g_ff_ab < 0) g_ff_ab = 0;
        /* (Claude completo-9) Optional delay lets a validation pass sample
         * gameplay rather than spend its N readbacks on the loading quads.
         * Unset: exactly the first N eligible draws, as requested. */
        e = getenv("RECOMP_FF_GPU_AB_AFTER_MS");
        g_ff_ab_after = e ? (DWORD)strtoul(e,NULL,10) : 0;
        g_ff_ab_start = GetTickCount();
        g_ff_on = xbox_RecompFix("FF_GPU");
    }
    return g_ff_on;
}
static int ffgpu_fallback(int why)
{
    g_ff_fallback[why]++;
    return 0;
}
static void ffgpu_perf(void)
{
    static DWORD last;
    DWORD now;
    uint32_t cpu = 0;
    int i;
    if (!PT_ON()) return;
    now = GetTickCount();
    if (!last) { last = now; return; }
    if (now - last < 1000) return;
    for (i = 0; i < FF_REASONS; i++) cpu += g_ff_fallback[i];
    fprintf(stderr, "  [FFGPU] ms %lu ff_gpu %u ff_cpu_fallback %u", (unsigned long)(now-last), g_ff_gpu, cpu);
    for (i = 0; i < FF_REASONS; i++) if (g_ff_fallback[i])
        fprintf(stderr, " %s=%u", g_ff_reason[i], g_ff_fallback[i]);
    fprintf(stderr, "\n");
    g_ff_gpu = 0; memset(g_ff_fallback, 0, sizeof g_ff_fallback); last = now;
}

/* Scalar expressions and precise intermediates prohibit MAD/reassociation;
 * dot()/mul()/normalize() would obscure the CPU's rounding order. */
static const char g_ff_hlsl[] =
"cbuffer VSC:register(b0){float4 vp;};\n"
"cbuffer FFC:register(b3){float4 rr[512];float4 cm[4];float4 vo;};\n"
"float f(uint b){return rr[b/16][(b/4)%4];}\n"
"precise float row(float4 a,float4 b){precise float x=a.x*b.x+a.y*b.y; x=x+a.z*b.z; return x+a.w*b.w;}\n"
"precise float nr(float4 a,float4 b){precise float x=a.x*b.x+a.y*b.y;return x+a.z*b.z;}\n"
"float4 mv(float4 p){return float4(row(rr[72],p),row(rr[73],p),row(rr[74],p),row(rr[75],p));}\n"
"float3 normal(float4 n){return float3(nr(rr[72],n),nr(rr[73],n),nr(rr[74],n));}\n"
"precise float len3(float3 n){precise float a=n.x*n.x+n.y*n.y;return sqrt(a+n.z*n.z);}\n"
"struct VI{float4 p:TEXCOORD0;float4 n:TEXCOORD1;float4 d0:TEXCOORD2;float4 d1:TEXCOORD3;float4 fog:TEXCOORD4;float4 t0:TEXCOORD5;float4 t1:TEXCOORD6;float4 t2:TEXCOORD7;float4 t3:TEXCOORD8;};\n"
"struct VO{float4 pos:SV_Position;float4 d0:COLOR0;float4 d1:COLOR1;float4 fog:TEXCOORD4;float4 t0:TEXCOORD0;float4 t1:TEXCOORD1;float4 t2:TEXCOORD2;float4 t3:TEXCOORD3;float4 win:TEXCOORD5;};\n"
"VO vs_ff(VI v){VO o;precise float4 c=float4(row(cm[0],v.p),row(cm[1],v.p),row(cm[2],v.p),row(cm[3],v.p));\n"
"precise float w=c.w;if(w==0)w=1;precise float3 s=c.xyz/w+vo.xyz;o.win=float4(s*w,w);\n"
"o.d0=D0?v.d0:float4(1,1,1,1);o.d1=D1?v.d1:float4(0,0,0,0);o.fog=float4(v.fog.x,0,0,0);\n"
"if(LIT){precise float3 e=float3(row(rr[72],float4(v.p.xyz,1)),row(rr[73],float4(v.p.xyz,1)),row(rr[74],float4(v.p.xyz,1)));precise float3 n=normal(v.n);precise float nl=len3(n);if(nl>1e-8)n=n/nl;precise float3 col=float3(f(0xA10),f(0xA14),f(0xA18));\n"
"[unroll]for(int j=0;j<8;j++){uint kind=(MASK>>(j*2))&3;uint b=0x1000+j*128;if(kind){precise float3 l;precise float att=1;bool range=true;\n"
"if(kind==1)l=float3(f(b+0x34),f(b+0x38),f(b+0x3c));else{l=float3(f(b+0x5c),f(b+0x60),f(b+0x64))-e;precise float d=len3(l);if(d>f(b+0x24)&&f(b+0x24)>0)range=false;if(d>1e-8)l=l/d;att=f(b+0x68)+f(b+0x6c)*d;att=att+(f(b+0x70)*d)*d;att=att>1e-8?1/att:1;}\n"
"if(range){precise float ndl=n.x*l.x+n.y*l.y;ndl=ndl+n.z*l.z;if(ndl<0)ndl=0;[unroll]for(int k=0;k<3;k++){precise float amb=f(b+4*k),dif=f(b+12+4*k);if(VC&&((COLMAT>>2)&3)==1)amb=amb*v.d0[k];if(VC&&((COLMAT>>4)&3)==1)dif=dif*v.d0[k];col[k]=col[k]+att*(amb+dif*ndl);}}}}\n"
"o.d0=float4(col,(VC&&((COLMAT>>4)&3)==1)?v.d0.w:f(0x3b4));}\n"
"precise float4 eye=mv(v.p);float4 tc[4]={v.t0,v.t1,v.t2,v.t3};\n"
"[unroll]for(int a=0;a<4;a++){precise float3 n=normal(v.n);precise float nl=len3(n);if(nl>1e-12)n=n/nl;precise float el=len3(eye.xyz);precise float3 u=el>1e-12?eye.xyz/el:float3(0,0,0);precise float dd=n.x*u.x+n.y*u.y;dd=2*(dd+n.z*u.z);precise float3 r=u-dd*n;\n"
"[unroll]for(int k=0;k<4;k++){uint g=TG[a*4+k];float4 pl=rr[(0x840+a*64+k*16)/16];if(g==0x2400)tc[a][k]=row(pl,eye);else if(g==0x2401)tc[a][k]=row(pl,v.p);else if(g==0x8511)tc[a][k]=k<3?n[k]:1;else if(g==0x8512)tc[a][k]=k<3?r[k]:1;else if(g==0x2402&&k<2){precise float z=r.z+1;precise float m=r.x*r.x+r.y*r.y;m=2*sqrt(m+z*z);tc[a][k]=m>1e-12?r[k]/m+0.5:0.5;}}\n"
"if(TM[a]){float4 t=tc[a];[unroll]for(int k=0;k<4;k++)tc[a][k]=row(rr[(0x6c0+a*64+k*16)/16],t);}}\n"
"[unroll]for(int k=0;k<4;k++){if(o.d0[k]<0)o.d0[k]=0;if(o.d0[k]>1)o.d0[k]=1;if(o.d1[k]<0)o.d1[k]=0;if(o.d1[k]>1)o.d1[k]=1;}\n"
"o.t0=tc[0];o.t1=tc[1];o.t2=tc[2];o.t3=tc[3];o.pos=float4(o.win.x*vp.x-w,w-o.win.y*vp.y,o.win.z*vp.z,w);return o;}\n"
"[maxvertexcount(1)]void gs_so(point VO i[1],inout PointStream<VO> s){s.Append(i[0]);}\n";

static char *ffgpu_source(const FfKey *k)
{
    size_t cap = 16384, len = 0;
    char *s = (char *)malloc(cap);
    int i;
    if (!s) return NULL;
    s[0] = 0;
    hwvp_cat(&s,&cap,&len,"#define LIT %u\n#define MASK %u\n#define COLMAT %u\n#define D0 %u\n#define D1 %u\n#define VC %u\nstatic const uint TG[16]={", k->light,k->mask,k->colmat,k->d0,k->d1,k->vc);
    for(i=0;i<16;i++) hwvp_cat(&s,&cap,&len,"%s%u",i?",":"",k->tg[i]);
    hwvp_cat(&s,&cap,&len,"};\nstatic const uint TM[4]={%u,%u,%u,%u};\n%s",k->tm[0],k->tm[1],k->tm[2],k->tm[3],g_ff_hlsl);
    return s;
}
static int ffgpu_compile_pending(void)
{
    LONG i;
    int did = 0;
    for (i=0;i<g_ff_n;i++) {
        FfShader *e=g_ff_shader[i];
        ID3DBlob *b=NULL,*err=NULL,*gs=NULL;
        D3D11_INPUT_ELEMENT_DESC il[9];
        HRESULT hr;
        UINT j;
        if (!e || e->state || InterlockedExchange(&e->claimed,1)) continue;
        did=1;
        hr=D3DCompile(e->src,strlen(e->src),"nv2a_ff",NULL,NULL,"vs_ff","vs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL1|D3DCOMPILE_IEEE_STRICTNESS,0,&b,&err);
        if (FAILED(hr)) {
            fprintf(stderr,"  [FFGPU] compile failed: %s\n",err?(char*)ID3D10Blob_GetBufferPointer(err):"?");
        } else {
            hr=ID3D11Device_CreateVertexShader(g_dev,ID3D10Blob_GetBufferPointer(b),ID3D10Blob_GetBufferSize(b),NULL,&e->vs);
            memset(il,0,sizeof il);
            for(j=0;j<9;j++){il[j].SemanticName="TEXCOORD";il[j].SemanticIndex=j;il[j].Format=DXGI_FORMAT_R32G32B32A32_FLOAT;il[j].AlignedByteOffset=16*j;il[j].InputSlotClass=D3D11_INPUT_PER_VERTEX_DATA;}
            if(SUCCEEDED(hr)) hr=ID3D11Device_CreateInputLayout(g_dev,il,9,ID3D10Blob_GetBufferPointer(b),ID3D10Blob_GetBufferSize(b),&e->il);
            if(SUCCEEDED(hr) && g_ff_ab>0) {
                D3D11_SO_DECLARATION_ENTRY so[8];
                const char *sem[8]={"TEXCOORD","COLOR","COLOR","TEXCOORD","TEXCOORD","TEXCOORD","TEXCOORD","TEXCOORD"};
                UINT idx[8]={5,0,1,4,0,1,2,3}, stride=sizeof(GVert);
                ID3DBlob *ge=NULL;
                hr=D3DCompile(e->src,strlen(e->src),"nv2a_ff_so",NULL,NULL,"gs_so","gs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL1|D3DCOMPILE_IEEE_STRICTNESS,0,&gs,&ge);
                if(FAILED(hr)) fprintf(stderr,"  [FFGPU] SO compile failed: %s\n",ge?(char*)ID3D10Blob_GetBufferPointer(ge):"?");
                if(ge)ID3D10Blob_Release(ge);
                memset(so,0,sizeof so);
                for(j=0;j<8;j++){so[j].SemanticName=sem[j];so[j].SemanticIndex=idx[j];so[j].ComponentCount=4;}
                if(SUCCEEDED(hr))hr=ID3D11Device_CreateGeometryShaderWithStreamOutput(g_dev,ID3D10Blob_GetBufferPointer(gs),ID3D10Blob_GetBufferSize(gs),so,8,&stride,1,D3D11_SO_NO_RASTERIZED_STREAM,NULL,&e->so);
            }
        }
        if(err)ID3D10Blob_Release(err);
        if(b)ID3D10Blob_Release(b);
        if(gs)ID3D10Blob_Release(gs);
        /* (Claude completo-9) Release partial creations before publishing failure. */
        if (FAILED(hr)) {
            if(e->so){ID3D11GeometryShader_Release(e->so);e->so=NULL;}
            if(e->il){ID3D11InputLayout_Release(e->il);e->il=NULL;}
            if(e->vs){ID3D11VertexShader_Release(e->vs);e->vs=NULL;}
        }
        free(e->src);e->src=NULL;
        MemoryBarrier();InterlockedExchange(&e->state,SUCCEEDED(hr)?1:2);
    }
    return did;
}
static FfShader *ffgpu_lookup(const FfKey *k, int *reason)
{
    LONG i;
    FfShader *e;
    for(i=0;i<g_ff_n;i++) if(!memcmp(k,&g_ff_shader[i]->key,sizeof *k)) {
        e=g_ff_shader[i];
        if(e->state==1){MemoryBarrier();return e;}
        *reason=e->state==2?FF_FAILED:FF_PENDING;return NULL;
    }
    if(g_ff_n==FF_GPU_MAX || !hwvp_enabled()){*reason=FF_RESOURCE;return NULL;}
    e=(FfShader*)calloc(1,sizeof *e);
    if(!e){*reason=FF_RESOURCE;return NULL;}
    e->key=*k;e->src=ffgpu_source(k);
    if(!e->src)e->state=2;
    g_ff_shader[g_ff_n]=e;MemoryBarrier();InterlockedIncrement(&g_ff_n);
    SetEvent(g_hwvp_ev);*reason=FF_PENDING;return NULL;
}
static int ffgpu_constants(void)
{
    struct { uint32_t r[0x800]; float m[16],v[4]; } cb;
    if(!g_ff_cb){g_ff_cb=gpu_cbuf(sizeof cb);g_ff_cb_gen=0;}
    if(!g_ff_cb)return 0;
    if(g_ff_cb_gen!=s_ff_gpu_gen){
        memcpy(cb.r,s_ffr,sizeof cb.r);memcpy(cb.m,s_3d.composite,sizeof cb.m);memcpy(cb.v,s_3d.vp_off,sizeof cb.v);
        ID3D11DeviceContext_UpdateSubresource(g_ctx,(ID3D11Resource*)g_ff_cb,0,NULL,&cb,0,0);
        g_ff_cb_gen=s_ff_gpu_gen;
    }
    ID3D11DeviceContext_VSSetConstantBuffers(g_ctx,3,1,&g_ff_cb);
    return 1;
}

/* (Claude completo-9) True GPU readback, point per original slot (not
 * expanded triangles), before the normal CPU draw. No render target writes.
 * Compare window-times-W, colours+fog, and all sixteen texture components. */
static int ffgpu_ab(FfShader *e, uint32_t nverts, UINT vb_at)
{
    UINT bytes=nverts*sizeof(GVert),zero=0,k,j;
    ID3D11Buffer *nullb=NULL;
    D3D11_MAPPED_SUBRESOURCE ms;
    double ep=0,ec=0,et=0;
    double pc[4]={0}, pr=0;
    float worst_cpu[4]={0}, worst_gpu[4]={0};
    uint32_t nonfinite=0;
    if(!e->so)return 0;
    if(bytes>g_ff_so_cap){
        D3D11_BUFFER_DESC d;
        if(g_ff_so)ID3D11Buffer_Release(g_ff_so);
        if(g_ff_read)ID3D11Buffer_Release(g_ff_read);
        g_ff_so=g_ff_read=NULL;g_ff_so_cap=0;
        memset(&d,0,sizeof d);d.ByteWidth=bytes;d.Usage=D3D11_USAGE_DEFAULT;d.BindFlags=D3D11_BIND_STREAM_OUTPUT;
        if(FAILED(ID3D11Device_CreateBuffer(g_dev,&d,NULL,&g_ff_so)))return 0;
        d.Usage=D3D11_USAGE_STAGING;d.BindFlags=0;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        if(FAILED(ID3D11Device_CreateBuffer(g_dev,&d,NULL,&g_ff_read)))return 0;
        g_ff_so_cap=bytes;
    }
    ID3D11DeviceContext_SOSetTargets(g_ctx,1,&g_ff_so,&zero);
    ID3D11DeviceContext_GSSetShader(g_ctx,e->so,NULL,0);
    ID3D11DeviceContext_IASetPrimitiveTopology(g_ctx,D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
    ID3D11DeviceContext_Draw(g_ctx,nverts,vb_at/(9*16));
    ID3D11DeviceContext_SOSetTargets(g_ctx,1,&nullb,&zero);
    ID3D11DeviceContext_GSSetShader(g_ctx,NULL,NULL,0);
    gpu_sh_reset();
    ID3D11DeviceContext_CopyResource(g_ctx,(ID3D11Resource*)g_ff_read,(ID3D11Resource*)g_ff_so);
    if(FAILED(ID3D11DeviceContext_Map(g_ctx,(ID3D11Resource*)g_ff_read,0,D3D11_MAP_READ,0,&ms)))return 0;
    for(k=0;k<nverts;k++) {
        float in[16][4]={{0}},cpu[32];
        const float *gpu=(const float*)ms.pData+32*k,*raw=g_ff_inputs+36*k;
        SwVert v;
        static const uint32_t attrs[9]={0,2,3,4,5,9,10,11,12};
        for(j=0;j<16;j++)in[j][3]=1;
        for(j=0;j<9;j++)memcpy(in[attrs[j]],raw+4*j,16);
        if(!sw3d_transform_loaded(in,&v)){nonfinite++;continue;}
        memcpy(cpu,v.p,16);memcpy(cpu+4,v.a+SA_D0,32);
        cpu[12]=v.a[SA_FOG];cpu[13]=cpu[14]=cpu[15]=0;
        memcpy(cpu+16,v.a+SA_T0,64);
        for(j=0;j<32;j++){
            double d=fabs((double)cpu[j]-gpu[j]);
            if(!isfinite(d)){nonfinite++;continue;}
            if(j<4){
                double den=fabs((double)cpu[j]), rel;
                if(d>pc[j])pc[j]=d;
                rel=d/(den>1e-30?den:1e-30);if(rel>pr)pr=rel;
                if(d>ep){ep=d;memcpy(worst_cpu,cpu,16);memcpy(worst_gpu,gpu,16);}
            }else if(j<16){if(d>ec)ec=d;}else if(d>et)et=d;
        }
    }
    ID3D11DeviceContext_Unmap(g_ctx,(ID3D11Resource*)g_ff_read,0);
    fprintf(stderr,"  [FFAB] draw %u max_err pos=%.9g col=%.9g tex=%.9g nonfinite=%u verts=%u batch=%u light=%u texgen=%X/%X/%X/%X texmat=%u%u%u%u\n",++g_ff_ab_done,ep,ec,et,nonfinite,nverts,s_3d.batches,e->key.light,e->key.tg[0],e->key.tg[4],e->key.tg[8],e->key.tg[12],e->key.tm[0],e->key.tm[1],e->key.tm[2],e->key.tm[3]);
    /* (Claude completo-9) Window depth can be 24-bit, then multiplied by W:
     * absolute position error alone cannot locate or interpret rounding. */
    if(ep)fprintf(stderr,"  [FFAB-POS] draw %u xyzw=%.9g/%.9g/%.9g/%.9g relative=%.9g cpu=%.9g/%.9g/%.9g/%.9g gpu=%.9g/%.9g/%.9g/%.9g\n",g_ff_ab_done,pc[0],pc[1],pc[2],pc[3],pr,worst_cpu[0],worst_cpu[1],worst_cpu[2],worst_cpu[3],worst_gpu[0],worst_gpu[1],worst_gpu[2],worst_gpu[3]);
    return 1;
}

static void gpu_ps_log(void);
static int gpu_batch_ff(uint32_t nverts)
{
    FfKey key;
    FfShader *e;
    uint32_t i,j,k,va0=s_gpu.attr[0].offset|0x80000000u;
    UINT vb_at,ib_at;
    int why=FF_STATE;
    static int hook=-1;
    static const uint32_t attrs[9]={0,2,3,4,5,9,10,11,12};
    ffgpu_perf();
    if(s_3d.mode==2)return 0; /* program draws are not FF fallbacks */
    if(!ffgpu_enabled())return ffgpu_fallback(FF_OFF);
    if(s_3d.mode!=0)return ffgpu_fallback(FF_MODE);
    if(s_ffr[0x0328/4])return ffgpu_fallback(FF_SKIN);
    if(hook<0)hook=getenv("RECOMP_BIGTRI")||getenv("RECOMP_GPU_MARK")||getenv("RECOMP_GPU_NULL")||getenv("RECOMP_REPLAY");
    if(hook||gpu_cached_option(10)||(xbox_gpu_cpu_hi&&va0>=xbox_gpu_cpu_lo&&va0<xbox_gpu_cpu_hi))return ffgpu_fallback(FF_HOOK);
    if(s_fix_texgen<0)sw3d_texgen_init();
    memset(&key,0,sizeof key);
    key.light=s_ffr[0x0314/4]&&s_gpu.attr[2].size&&s_gpu.attr[2].stride;
    key.mask=key.light?s_ffr[0x03BC/4]:0;key.colmat=key.light?s_ffr[0x0298/4]:0;
    key.d0=!!s_gpu.attr[3].size;key.d1=!!s_gpu.attr[4].size;key.vc=key.d0&&s_gpu.attr[3].stride;
    for(i=0;i<4;i++){
        key.tm[i]=!!s_ffr[0x0420/4+i];
        for(j=0;j<4;j++){
            uint32_t g=s_fix_texgen?s_ffr[(0x03C0+i*16)/4+j]:0;
            if(g&&g!=0x2400&&g!=0x2401&&g!=0x2402&&g!=0x8511&&g!=0x8512)return ffgpu_fallback(FF_STATE);
            key.tg[4*i+j]=g;
        }
    }
    e=ffgpu_lookup(&key,&why);
    if(!e)return ffgpu_fallback(why);
    if(nverts>g_ff_inputs_cap){
        float *p=(float*)realloc(g_ff_inputs,(size_t)nverts*36*sizeof(float));
        if(!p)return ffgpu_fallback(FF_RESOURCE);
        g_ff_inputs=p;g_ff_inputs_cap=nverts;
    }
    for(k=0;k<nverts;k++){
        float *dst=g_ff_inputs+36*k,c[4],w;
        for(i=0;i<9;i++)fetch_attr_any(attrs[i],g_vidx[k],dst+4*i);
        for(i=0;i<36;i++)if(!isfinite(dst[i]))return ffgpu_fallback(FF_NUMERIC);
        /* Only the position safety test stays on CPU. A shader cannot remove
         * all triangles using a rejected slot; keep gpu_drop_bad_tris exact.
         * Tiny W and extreme inputs conservatively fall back as well. */
        for(i=0;i<4;i++){
            const float *m=s_3d.composite+4*i;
            c[i]=m[0]*dst[0]+m[1]*dst[1]+m[2]*dst[2]+m[3]*dst[3];
            if(!isfinite(c[i])||fabsf(c[i])>1e30f)return ffgpu_fallback(FF_NUMERIC);
        }
        w=c[3]==0?1:c[3];
        if(fabsf(w)<1e-18f)return ffgpu_fallback(FF_NUMERIC);
        for(i=0;i<3;i++){
            float s=c[i]/w+s_3d.vp_off[i];
            if(!isfinite(s)||!isfinite(s*w)||fabsf(s*w)>1e30f)return ffgpu_fallback(FF_NUMERIC);
        }
    }
    vb_at=gpu_upload(&g_ff_vb,&g_ff_vb_cap,&g_ff_vb_pos,D3D11_BIND_VERTEX_BUFFER,g_ff_inputs,nverts*36*sizeof(float),36*sizeof(float));
    if(vb_at==0xFFFFFFFFu||!ffgpu_constants())return ffgpu_fallback(FF_RESOURCE);
    if(g_ff_ab_done<(uint32_t)g_ff_ab && GetTickCount()-g_ff_ab_start>=g_ff_ab_after){
        gpu_bind_pipe(e->il,g_ff_vb,36*sizeof(float),e->vs,g_ps_cur);
        if(!ffgpu_ab(e,nverts,vb_at)){
            static int shown;if(!shown++)fprintf(stderr,"  [FFAB] readback failed; CPU draw retained\n");
            return ffgpu_fallback(FF_RESOURCE);
        }
        return ffgpu_fallback(FF_AB);
    }
    ib_at=gpu_upload(&g_ib,&g_ib_cap,&g_ib_pos,D3D11_BIND_INDEX_BUFFER,g_idx,g_nidx*4,4);
    if(ib_at==0xFFFFFFFFu)return ffgpu_fallback(FF_RESOURCE);
    if(shadow_hook())return 1;
    gpu_bind_stages();gpu_set_blend(0);gpu_set_depth(g_cur_ds!=NULL);
    g_ps_cur=hwps_lookup(&g_pskey);
    gpu_bind_pipe(e->il,g_ff_vb,36*sizeof(float),e->vs,g_ps_cur);
    gpu_ps_log();
    ID3D11DeviceContext_DrawIndexed(g_ctx,g_nidx,ib_at/4,(INT)(vb_at/(36*sizeof(float))));
    gpu_record_draw_writes();s_gpu.tris_drawn+=g_nidx/3;s_gpu.drawn_offset=s_gpu.color_offset;s_3d.verts+=nverts;
    g_ff_gpu++;return 1;
}
