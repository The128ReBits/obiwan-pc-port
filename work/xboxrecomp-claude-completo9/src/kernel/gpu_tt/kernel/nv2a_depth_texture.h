/* Obi-Wan Windows port: depth texture decoding, independent of colour decoding.
 * MIT, under the same terms as this renderer. Numeric layout checked against
 * xemu 6095054b (pgraph/util.h); no xemu implementation is included here.
 * Surface F16 and xemu's HALF_FLOAT texture upload are deliberately distinct.
 */
#ifndef NV2A_DEPTH_TEXTURE_H
#define NV2A_DEPTH_TEXTURE_H
#include <stdint.h>
#include <stddef.h>
#include <math.h>

static int nvdepth_format(uint32_t f) { return f >= 0x2A && f <= 0x31; }
static int nvdepth_24(uint32_t f) { return ((f - 0x2A) & 3) < 2; }
static int nvdepth_float(uint32_t f) { return (int)(f & 1); }
static float nvdepth_max(uint32_t f)
{
    return nvdepth_24(f) ? (nvdepth_float(f) ? 1.0e30f : 16777215.0f)
                         : (nvdepth_float(f) ? 511.9375f : 65535.0f);
}
static float nvdepth_f16(uint16_t z)
{
    return z ? ldexpf(1.0f + (z & 4095) / 4096.0f, (z >> 12) - 7) : 0.0f;
}
static float nvdepth_f24(uint32_t z)
{
    unsigned e = z >> 16, m = z & 65535;
    if (!e) return ldexpf(m / 65536.0f, -126);
    if (e == 255) return m ? NAN : INFINITY;
    return ldexpf(1.0f + m / 65536.0f, (int)e - 127);
}
/* Clear words contain Z16 in the low half or Z24 above the stencil byte.
 * Convert to the same normalized range used when writing window depth. */
static float nvdepth_clear_value(uint32_t word, int z16, int floating, float scale)
{
    uint32_t raw = z16 ? word & 0xFFFFu : word >> 8;
    float depth = (float)raw;
    if (floating) depth = z16 ? nvdepth_f16((uint16_t)raw) : nvdepth_f24(raw);
    if (!(depth > 0.0f)) return 0.0f;
    if (depth >= scale) return 1.0f;
    return depth / scale;
}

/* Diagnostic only: reproduce xemu's linear Y16_FLOAT GL_HALF_FLOAT upload
 * into a normalized depth texture, then its shader's * 511.9375 rescale. */
static float nvdepth_xemu16(uint16_t z)
{
    unsigned e = (z >> 10) & 31, m = z & 1023;
    float v;
    if (z & 0x8000) return 0.0f;
    if (e == 31) v = m ? 0.0f : 1.0f;
    else v = e ? ldexpf(1.0f + m / 1024.0f, (int)e - 15) : ldexpf((float)m, -24);
    if (v > 1.0f) v = 1.0f;
    return floorf(v * 65535.0f + 0.5f) * (511.9375f / 65535.0f);
}
static float nvdepth_texel(uint32_t f, const uint8_t *p, int xemu16)
{
    uint32_t z = (uint32_t)p[0] | ((uint32_t)p[1] << 8);
    if (nvdepth_24(f)) z = (z | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24)) >> 8;
    if (!nvdepth_float(f)) return (float)z;
    return nvdepth_24(f) ? nvdepth_f24(z) : (xemu16 ? nvdepth_xemu16((uint16_t)z) : nvdepth_f16((uint16_t)z));
}
/* Morton address for rectangular power-of-two textures. */
static size_t nvdepth_morton(uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    size_t a = 0, out = 1;
    uint32_t bit;
    for (bit = 1; bit < w || bit < h; bit <<= 1) {
        if (bit < w) { if (x & bit) a |= out; out <<= 1; }
        if (bit < h) { if (y & bit) a |= out; out <<= 1; }
    }
    return a;
}
static int nvdepth_decode(uint32_t f, const uint8_t *src, size_t bytes,
                          uint32_t w, uint32_t h, uint32_t pitch, int xemu16, float *out)
{
    uint32_t bpp = nvdepth_24(f) ? 4 : 2, x, y;
    size_t need;
    if (!nvdepth_format(f) || !w || !h || w > 2048 || h > 2048) return 0;
    if (f < 0x2E) {
        if ((w & (w - 1)) || (h & (h - 1))) return 0;
        need = (size_t)w * h * bpp;
    } else {
        if (pitch < w * bpp) return 0;
        need = (size_t)(h - 1) * pitch + (size_t)w * bpp;
    }
    if (need > bytes) return 0;
    for (y = 0; y < h; y++) for (x = 0; x < w; x++) {
        size_t a = f < 0x2E ? nvdepth_morton(x, y, w, h) * bpp : (size_t)y * pitch + x * bpp;
        out[(size_t)y * w + x] = nvdepth_texel(f, src + a, xemu16);
    }
    return 1;
}
#endif
