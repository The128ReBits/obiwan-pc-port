/* Synthetic data only; no game assets. Compile with cl /O2 /W4. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/kernel/gpu_tt/kernel/nv2a_depth_texture.h"

int main(void)
{
    unsigned z, x, y, f;
    uint8_t data[64] = {0};
    float result[16];
    for (z = 0; z <= 65535; z++) {
        uint32_t bits = z ? (z << 11) + 0x3C000000u : 0;
        float reference;
        memcpy(&reference, &bits, 4);
        assert(nvdepth_f16((uint16_t)z) == reference);
    }
    assert(nvdepth_f16(0) == 0 && nvdepth_f16(65535) == 511.9375f);
    assert(nvdepth_f16(0x7000) == 1 && nvdepth_f16(0x8000) == 2);
    assert(nvdepth_f24(0x7F0000) == 1 && nvdepth_f24(0x800000) == 2);
    assert(nvdepth_xemu16(0x3C00) == 511.9375f);
    assert(nvdepth_f16(0x3C00) != nvdepth_xemu16(0x3C00));
    /* Clear tests use the same encoding and range as window depth. */
    assert(nvdepth_clear_value(0xABCD8000u, 1, 0, 65536.0f) == 0.5f);
    assert(nvdepth_clear_value(0x008000ABu, 0, 0, 65536.0f) == 0.5f);
    assert(nvdepth_clear_value(0xABCD8000u, 1, 1, 4.0f) == 0.5f);
    assert(nvdepth_clear_value(0x800000ABu, 0, 1, 4.0f) == 0.5f);
    assert(nvdepth_clear_value(0xFFFFu, 1, 1, nvdepth_max(0x2D)) == 1.0f);
    assert(nvdepth_clear_value(0xFE000000u, 0, 1, nvdepth_max(0x2B)) == 1.0f);
    assert(nvdepth_clear_value(0xFF000000u, 0, 1, 4.0f) == 1.0f);
    assert(nvdepth_clear_value(0xFF000100u, 0, 1, 4.0f) == 0.0f);
    assert(nvdepth_clear_value(0x000000ABu, 0, 1, 4.0f) == 0.0f);
    /* Linear rows have padding; the last row needs no trailing padding. */
    data[0] = 1; data[2] = 2; data[8] = 3; data[10] = 4;
    assert(nvdepth_decode(0x30, data, 12, 2, 2, 8, 0, result));
    assert(result[0] == 1 && result[1] == 2 && result[2] == 3 && result[3] == 4);
    assert(!nvdepth_decode(0x30, data, 11, 2, 2, 8, 0, result));
    assert(!nvdepth_decode(0x30, data, 64, 2, 2, 3, 0, result));
    /* Non-square Morton layout and all eight format families. */
    for (f = 0x2A; f <= 0x31; f++) {
        unsigned bpp = nvdepth_24(f) ? 4 : 2;
        memset(data, 0, sizeof data);
        for (y = 0; y < 2; y++) for (x = 0; x < 4; x++) {
            unsigned a = (f < 0x2E ? (unsigned)nvdepth_morton(x, y, 4, 2) : y * 4 + x) * bpp;
            uint32_t raw = nvdepth_float(f) ? (nvdepth_24(f) ? 0x7F0000 : 0x7000) : y * 4 + x;
            if (bpp == 4) raw = (raw << 8) | 0xAB; /* stencil excluded */
            memcpy(data + a, &raw, bpp);
        }
        assert(nvdepth_decode(f, data, sizeof data, 4, 2, 4 * bpp, 0, result));
        for (z = 0; z < 8; z++) assert(result[z] == (nvdepth_float(f) ? 1.0f : (float)z));
    }
    assert(!nvdepth_decode(0x2C, data, 64, 3, 2, 0, 0, result));
    puts("PASS: 65536 F16 values, F24, HALF distinction, clear values, 8 formats, pitch, Morton and bounds");
    return 0;
}
