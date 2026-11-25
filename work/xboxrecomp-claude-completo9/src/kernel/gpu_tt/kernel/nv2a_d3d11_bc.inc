/* Native BC textures for the Obi-Wan D3D11 adapter. MIT, under the same
 * terms as this renderer. Guest DXT2D levels are contiguous 4x4 blocks. */
typedef struct {
    UINT offset, pitch, bytes;
} GBCLevel;

static UINT gpu_bc_layout(UINT w, UINT h, UINT levels, UINT block, GBCLevel layout[12])
{
    UINT total = 0;
    if (w < 4 || h < 4 || w > 2048 || h > 2048 || (w & 3) || (h & 3)
            || !levels || levels > 12 || (block != 8 && block != 16))
        return 0;
    for (UINT i = 0; i < levels; ++i) {
        UINT pitch = ((w + 3) / 4) * block;
        UINT bytes = pitch * ((h + 3) / 4);
        layout[i] = (GBCLevel){ total, pitch, bytes };
        total += bytes;
        if (w == 1 && h == 1 && i + 1 < levels)
            return 0;
        w = w > 1 ? w / 2 : 1;
        h = h > 1 ? h / 2 : 1;
    }
    return total;
}

static DXGI_FORMAT gpu_bc_format(uint32_t format)
{
    switch (format) {
    case 0x0C: return DXGI_FORMAT_BC1_UNORM;
    case 0x0E: return DXGI_FORMAT_BC2_UNORM;
    case 0x0F: return DXGI_FORMAT_BC3_UNORM;
    default: return DXGI_FORMAT_UNKNOWN;
    }
}

static uint64_t gpu_bc_hash(const uint8_t *src, UINT bytes)
{
    /* Independent lanes keep the chain fast to check, and cover even a
     * change to the last mip. The layout always ends on an 8-byte block. */
    uint64_t a = 0xcbf29ce484222325ull, b = 0x9e3779b97f4a7c15ull;
    UINT i = 0;
    for (; i + 16 <= bytes; i += 16) {
        uint64_t u, v;
        memcpy(&u, src + i, sizeof u);
        memcpy(&v, src + i + 8, sizeof v);
        a = (a ^ u) * 0x100000001b3ull;
        b = (b ^ v) * 0x100000001b3ull;
    }
    if (i < bytes) {
        uint64_t tail;
        memcpy(&tail, src + i, sizeof tail);
        a = (a ^ tail) * 0x100000001b3ull;
    }
    return a ^ (b * 0x9e3779b97f4a7c15ull);
}

typedef struct {
    GTex gpu;
    uint32_t epoch, writes;
    uint64_t checksum;
} GBCTexture;

/* Four ways keep all stages of one draw live even when their keys collide. */
static GBCTexture g_bc[256][4];

static int gpu_bc_enabled(void)
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("RECOMP_GPU_NATIVE_BC");
        on = e && e[0] == '1' && !getenv("RECOMP_TEX_VERIFY")
             && !getenv("RECOMP_TEX_SLOWDEC") && !getenv("RECOMP_TEX_DUMP");
    }
    /* The optional TT hooks expect decoded colour pixels and their hashes. */
    return on && !(s_texrec_on && s_texrec_n) && !xbox_tex_observer && !ultra_on();
}

static ID3D11ShaderResourceView *gpu_bc_get(const Texture *t, uint32_t format, unsigned levels)
{
    DXGI_FORMAT dxgi = gpu_bc_format(t->color);
    /* 2D, no cubemap and a constant border (no extra texels in RAM). */
    if (dxgi == DXGI_FORMAT_UNKNOWN || (format & 0xFCu) != 0x28u || !gpu_bc_enabled())
        return NULL;
    uint32_t epoch = texc_epoch(t);
    unsigned slot = ((t->offset >> 6) ^ (t->offset >> 16) ^ (t->width << 1)
                     ^ (t->height << 3) ^ t->color ^ levels) & 255u;
    GBCTexture *entry = NULL, *victim = &g_bc[slot][0];
    for (unsigned i = 0; i < 4; ++i) {
        GBCTexture *e = &g_bc[slot][i];
        if (e->gpu.srv && e->gpu.off == t->offset && e->gpu.color == t->color
                && e->gpu.w == t->width && e->gpu.h == t->height && e->gpu.levels == levels) {
            entry = e;
            break;
        }
        if (!e->gpu.srv || (victim->gpu.srv && e->gpu.used < victim->gpu.used)) victim = e;
    }
    if (entry && entry->epoch == epoch && entry->writes == s_wgen) {
        entry->gpu.used = ++g_clock;
        return entry->gpu.srv;
    }

    GBCLevel layout[12];
    UINT bytes = gpu_bc_layout(t->width, t->height, levels, d3d8_format_dxt_block_bytes(t->color), layout);
    uint32_t physical = t->offset;
    size_t ram = xbox_GetMemorySize();
    if (physical >= XBOX_CONTIG_BASE) {
        physical -= XBOX_CONTIG_BASE;
        ram = XBOX_CONTIG_SIZE;
    } else if (physical < 0x1000u) {
        return NULL;
    }
    if (!bytes || physical >= ram || bytes > ram - physical) return NULL;
    const uint8_t *src = (const uint8_t *)xbox_GetMemoryOffset() + t->offset;
    uint64_t checksum = gpu_bc_hash(src, bytes);
    if (!entry) {
        ID3D11Texture2D *tex;
        ID3D11ShaderResourceView *srv;
        D3D11_TEXTURE2D_DESC d = {0};
        D3D11_SUBRESOURCE_DATA initial[12];
        for (unsigned i = 0; i < levels; ++i)
            initial[i] = (D3D11_SUBRESOURCE_DATA){ src + layout[i].offset, layout[i].pitch, layout[i].bytes };
        d.Width = t->width; d.Height = t->height; d.MipLevels = levels; d.ArraySize = 1;
        d.Format = dxgi; d.SampleDesc.Count = 1;
        d.Usage = D3D11_USAGE_DEFAULT; d.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        if (FAILED(ID3D11Device_CreateTexture2D(g_dev, &d, initial, &tex))) return NULL;
        if (FAILED(ID3D11Device_CreateShaderResourceView(g_dev, (ID3D11Resource *)tex, NULL, &srv))) {
            ID3D11Texture2D_Release(tex);
            return NULL;
        }
        entry = victim;
        if (entry->gpu.srv) ID3D11ShaderResourceView_Release(entry->gpu.srv);
        if (entry->gpu.tex) ID3D11Texture2D_Release(entry->gpu.tex);
        entry->gpu = (GTex){ .off = t->offset, .color = t->color, .w = t->width, .h = t->height,
                             .levels = levels, .tex = tex, .srv = srv };
    } else if (checksum != entry->checksum) {
        for (unsigned i = 0; i < levels; ++i)
            ID3D11DeviceContext_UpdateSubresource(g_ctx, (ID3D11Resource *)entry->gpu.tex,
                i, NULL, src + layout[i].offset, layout[i].pitch, layout[i].bytes);
        s_tex_dyn[(t->offset >> 12) & 16383u] = 1;
        epoch = texc_epoch(t);
    }
    entry->epoch = epoch;
    entry->writes = s_wgen;
    entry->checksum = checksum;
    entry->gpu.used = ++g_clock;
    return entry->gpu.srv;
}
