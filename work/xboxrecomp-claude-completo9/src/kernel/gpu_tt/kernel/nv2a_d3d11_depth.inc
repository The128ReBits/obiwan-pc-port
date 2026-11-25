/* Depth-only uploads. Included after GTex in nv2a_d3d11.inc.
 * MIT, same licence and credits as gpu_tt. Never enters the ARGB cache.
 */
#include "nv2a_depth_texture.h"
typedef struct {
    GTex gpu;
    uint32_t epoch, writes, checksum;
} GDepthTex;
static GDepthTex g_depth_tex[16];

static ID3D11ShaderResourceView *gtex_depth_get(const Texture *t)
{
    GDepthTex *e = NULL, *victim = &g_depth_tex[0];
    uint32_t bpp = nvdepth_24(t->color) ? 4 : 2, sum = 2166136261u;
    size_t bytes, i, physical, ram;
    const uint8_t *src;
    float *values;
    D3D11_TEXTURE2D_DESC td;
    static int xemu16 = -1, dump = -1;
    extern size_t xbox_GetMemorySize(void);
    if (xemu16 < 0) { const char *v = getenv("RECOMP_GPU_DEPTH_XEMU16"); xemu16 = v && v[0] == '1'; }
    if (dump < 0) dump = getenv("RECOMP_GPU_DEPTH_LOG") != NULL;
    if (!t->valid || !t->width || !t->height || t->width > 2048 || t->height > 2048) return NULL;
    if (t->color < 0x2E) {
        if ((t->width & (t->width - 1)) || (t->height & (t->height - 1))) return NULL;
        bytes = (size_t)t->width * t->height * bpp;
    } else {
        if (t->pitch < t->width * bpp) return NULL;
        bytes = (size_t)(t->height - 1) * t->pitch + t->width * bpp;
    }
    /* dma_resolve may have selected the contiguous physical mirror. */
    physical = t->offset >= XBOX_CONTIG_BASE ? t->offset - XBOX_CONTIG_BASE : t->offset;
    ram = t->offset >= XBOX_CONTIG_BASE ? XBOX_CONTIG_SIZE : xbox_GetMemorySize();
    if (physical >= ram || bytes > ram - physical) return NULL;
    src = (const uint8_t *)xbox_GetMemoryOffset() + t->offset;
    for (i = 0; i < 16; i++) {
        GDepthTex *c = &g_depth_tex[i];
        if (c->gpu.tex && c->gpu.off == t->offset && c->gpu.color == t->color && c->gpu.w == t->width
                && c->gpu.h == t->height && c->gpu.pitch == t->pitch) { e = c; break; }
        if (!c->gpu.tex || (victim->gpu.tex && c->gpu.used < victim->gpu.used)) victim = c;
    }
    if (e && e->epoch == g_pb_runs && e->writes == s_wgen) { e->gpu.used = ++g_clock; return e->gpu.srv; }
    /* Full content check, including all depth bits. No colour hash shortcuts. */
    {
        uint32_t h1 = 0x01000193u, h2 = 0x9E3779B9u, h3 = 0x85EBCA6Bu;
        for (i = 0; i + 16 <= bytes; i += 16) {
            uint32_t v[4]; memcpy(v, src + i, sizeof v);
            sum = (sum ^ v[0]) * 16777619u; h1 = (h1 ^ v[1]) * 16777619u;
            h2 = (h2 ^ v[2]) * 16777619u; h3 = (h3 ^ v[3]) * 16777619u;
        }
        sum ^= h1 * 3u ^ h2 * 5u ^ h3 * 7u;
        for (; i < bytes; i++) sum = (sum ^ src[i]) * 16777619u;
    }
    if (e && e->checksum == sum) goto current;
    values = (float *)malloc((size_t)t->width * t->height * sizeof(float));
    if (!values) return NULL;
    if (!nvdepth_decode(t->color, src, bytes, t->width, t->height, t->pitch, xemu16, values)) { free(values); return NULL; }
    if (!e) {
        e = victim;
        if (e->gpu.srv) ID3D11ShaderResourceView_Release(e->gpu.srv);
        if (e->gpu.tex) ID3D11Texture2D_Release(e->gpu.tex);
        memset(e, 0, sizeof *e);
        memset(&td, 0, sizeof td);
        td.Width = t->width; td.Height = t->height; td.MipLevels = 1; td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R32_FLOAT; td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        if (FAILED(ID3D11Device_CreateTexture2D(g_dev, &td, NULL, &e->gpu.tex))
                || FAILED(ID3D11Device_CreateShaderResourceView(g_dev, (ID3D11Resource *)e->gpu.tex, NULL, &e->gpu.srv))) {
            if (e->gpu.tex) ID3D11Texture2D_Release(e->gpu.tex);
            memset(e, 0, sizeof *e); free(values); return NULL;
        }
        e->gpu.off = t->offset; e->gpu.color = t->color; e->gpu.w = t->width; e->gpu.h = t->height; e->gpu.pitch = t->pitch;
    }
    ID3D11DeviceContext_UpdateSubresource(g_ctx, (ID3D11Resource *)e->gpu.tex, 0, NULL, values, t->width * 4, 0);
    if (dump) {
        static unsigned n;
        if (n++ < 8) {
            float lo = values[0], hi = values[0]; size_t j;
            for (j = 1; j < (size_t)t->width * t->height; j++) { if (values[j] < lo) lo = values[j]; if (values[j] > hi) hi = values[j]; }
            fprintf(stderr, "  [DEPTHDATA] fmt %02X min %.9g max %.9g first %04X xemu16 %d hash %08X\n",
                    t->color, lo, hi, (unsigned)(src[0] | (src[1] << 8)), xemu16, sum);
        }
    }
    free(values);
    e->checksum = sum;
current:
    e->epoch = g_pb_runs; e->writes = s_wgen; e->gpu.used = ++g_clock;
    return e->gpu.srv;
}
