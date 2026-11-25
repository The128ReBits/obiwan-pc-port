/**
 * NV2A register combiners. See nv2a_combiner.h.
 *
 * Register numbers (an input or output byte's low nibble):
 *   0 zero/discard  1 c0  2 c1  3 fog  4 v0  5 v1  8-11 t0-t3  12 r0  13 r1
 *   14 v1+r0 sum and 15 E*F product (final combiner only)
 * An input byte is reg | alpha<<4 | mapping<<5. In the RGB portion the alpha
 * bit replicates .a; in the alpha portion it picks .a over .b.
 *
 * nv2a_rc_compile decodes those words once; nv2a_rc_run is the per-pixel
 * part. Every arithmetic expression is the one the evaluator always used, in
 * the same order, so a compiled program gives bit-identical pixels. What the
 * split saves is the decoding, the constant unpacking, and the work whose
 * result a program never reads (dot products it does not ask for, outputs
 * sent to the discard register).
 */
#include "nv2a_combiner.h"

#include <string.h>

/* The run loop is the software rasteriser's hottest code. */
#if defined(_MSC_VER)
#define NV_RC_INLINE static __forceinline
#else
#define NV_RC_INLINE static inline __attribute__((always_inline))
#endif

enum { R_ZERO = 0, R_C0 = 1, R_C1 = 2, R_FOG = 3, R_V0 = 4, R_V1 = 5,
       R_T0 = 8, R_R0 = 12, R_R1 = 13, R_SUM = 14, R_EF = 15 };

/* n / 255.0f for every byte, folded by the compiler: the same correctly
 * rounded quotients the division gives, without dividing per channel --
 * bilinear filtering unpacks four texels for every pixel of every stage. */
#define U8F(n)   ((float)(n) / 255.0f)
#define U8F4(n)  U8F(n), U8F((n) + 1), U8F((n) + 2), U8F((n) + 3)
#define U8F16(n) U8F4(n), U8F4((n) + 4), U8F4((n) + 8), U8F4((n) + 12)
#define U8F64(n) U8F16(n), U8F16((n) + 16), U8F16((n) + 32), U8F16((n) + 48)
static const float k_u8f[256] = { U8F64(0), U8F64(64), U8F64(128), U8F64(192) };

void nv2a_rc_unpack(uint32_t c, float o[4])
{
    o[0] = k_u8f[(c >> 16) & 0xFF];
    o[1] = k_u8f[(c >>  8) & 0xFF];
    o[2] = k_u8f[ c        & 0xFF];
    o[3] = k_u8f[ c >> 24        ];
}

static Nv2aRcInput decode_input(uint32_t byte)
{
    Nv2aRcInput in;
    in.reg = (uint8_t)(byte & 0xF);
    in.alpha = (uint8_t)((byte >> 4) & 1);
    in.map = (uint8_t)((byte >> 5) & 7);
    return in;
}

void nv2a_rc_compile(const Nv2aCombiner *rc, Nv2aRcProgram *p)
{
    uint32_t flags = rc->control >> 8, s;
    int k;

    memset(p, 0, sizeof(*p));
    p->stages = rc->control & 0xFF;
    if (p->stages > 8)
        p->stages = 8;
    p->mux_msb = flags & 1;
    /* r0.a starts as texture 0's alpha, or 1 with stage 0 off (psh.c). */
    p->r0a_t0 = (rc->stage_program & 0x1F) != 0;

    for (s = 0; s < p->stages; s++) {
        Nv2aRcStage *st = &p->st[s];
        uint32_t icw = rc->color_icw[s], ocw = rc->color_ocw[s];
        uint32_t aicw = rc->alpha_icw[s], aocw = rc->alpha_ocw[s];

        for (k = 0; k < 4; k++) {
            st->in[k] = decode_input(icw >> (24 - 8 * k));
            st->in[4 + k] = decode_input(aicw >> (24 - 8 * k));
        }
        st->fl = ocw >> 12;
        st->afl = aocw >> 12;
        st->ab = (uint8_t)((ocw >> 4) & 0xF);
        st->cd = (uint8_t)(ocw & 0xF);
        st->sum = (uint8_t)((ocw >> 8) & 0xF);
        st->a_ab = (uint8_t)((aocw >> 4) & 0xF);
        st->a_cd = (uint8_t)(aocw & 0xF);
        st->a_sum = (uint8_t)((aocw >> 8) & 0xF);
        /* One c0/c1 for every stage, or one per stage (COMBINERCOUNT). */
        nv2a_rc_unpack(rc->factor0[(flags & 0x010) ? s : 0], st->c0);
        nv2a_rc_unpack(rc->factor1[(flags & 0x100) ? s : 0], st->c1);
    }

    p->final_on = rc->final0 || rc->final1;
    if (p->final_on) {
        uint32_t f0 = rc->final0, f1 = rc->final1;
        p->fflags = f1 & 0xFF;
        nv2a_rc_unpack(rc->final_c0, p->fc0);
        nv2a_rc_unpack(rc->final_c1, p->fc1);
        p->fin[0] = decode_input(f1 >> 24);         /* E */
        p->fin[1] = decode_input(f1 >> 16);         /* F */
        p->fin[2] = decode_input(f0 >> 24);         /* A */
        p->fin[3] = decode_input(f0 >> 16);         /* B */
        p->fin[4] = decode_input(f0 >> 8);          /* C */
        p->fin[5] = decode_input(f0);               /* D */
        p->fin[6] = decode_input(f1 >> 8);          /* G */
    }
}

#if defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#include <emmintrin.h>

/* One register is one vector, r g b a in lanes 0-3. Each lane does exactly
 * the scalar arithmetic of the portable version below: the same IEEE
 * operations in the same order, never fused. Where the scalar code branches,
 * min/max take their operands in the order that keeps -0 and NaN the same:
 * _mm_max_ps(a, b) is a > b ? a : b, _mm_min_ps(a, b) is a < b ? a : b. */

NV_RC_INLINE __m128 v_clamp(__m128 x, __m128 lo, __m128 hi)
{
    return _mm_max_ps(lo, _mm_min_ps(hi, x));   /* x<lo ? lo : x>hi ? hi : x */
}

NV_RC_INLINE __m128 v_blend(__m128 a, __m128 b, __m128 mask)  /* mask ? b : a */
{
    return _mm_or_ps(_mm_andnot_ps(mask, a), _mm_and_ps(mask, b));
}

/* PS_INPUTMAPPING, psh.c get_input_var. */
NV_RC_INLINE __m128 v_map_in(__m128 x, unsigned map)
{
    const __m128 zero = _mm_setzero_ps(), one = _mm_set1_ps(1.0f);
    const __m128 sign = _mm_set1_ps(-0.0f);
    __m128 p = _mm_max_ps(zero, x);                         /* x < 0 ? 0 : x */
    switch (map) {
    case 0:  return p;                                      /* unsigned identity */
    case 1:  return _mm_sub_ps(one, v_clamp(x, zero, one)); /* unsigned invert   */
    case 2:  return _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(2.0f), p), one);
    case 3:  return _mm_add_ps(_mm_mul_ps(_mm_set1_ps(-2.0f), p), one);
    case 4:  return _mm_sub_ps(p, _mm_set1_ps(0.5f));       /* half-bias normal  */
    case 5:  return _mm_add_ps(_mm_xor_ps(p, sign), _mm_set1_ps(0.5f));
    case 6:  return x;                                      /* signed identity   */
    default: return _mm_xor_ps(x, sign);                    /* signed negate     */
    }
}

/* PS_COMBINEROUTPUT scale/bias, psh.c get_output, then the [-1,1] clamp. */
NV_RC_INLINE __m128 v_out(__m128 x, uint32_t mapping)
{
    const __m128 half = _mm_set1_ps(0.5f);
    switch (mapping) {
    case 0x08: x = _mm_sub_ps(x, half); break;
    case 0x10: x = _mm_mul_ps(x, _mm_set1_ps(2.0f)); break;
    case 0x18: x = _mm_mul_ps(_mm_sub_ps(x, half), _mm_set1_ps(2.0f)); break;
    case 0x20: x = _mm_mul_ps(x, _mm_set1_ps(4.0f)); break;
    case 0x30: x = _mm_mul_ps(x, half); break;
    default:   break;
    }
    return v_clamp(x, _mm_set1_ps(-1.0f), _mm_set1_ps(1.0f));
}

NV_RC_INLINE __m128 v_read_rgb(const __m128 *R, Nv2aRcInput in)
{
    __m128 r = R[in.reg];
    if (in.alpha)
        r = _mm_shuffle_ps(r, r, _MM_SHUFFLE(3, 3, 3, 3));
    return v_map_in(r, in.map);
}

/* The alpha portion's input, in every lane. */
NV_RC_INLINE __m128 v_read_alpha(const __m128 *R, Nv2aRcInput in)
{
    __m128 r = R[in.reg];
    r = in.alpha ? _mm_shuffle_ps(r, r, _MM_SHUFFLE(3, 3, 3, 3))
                 : _mm_shuffle_ps(r, r, _MM_SHUFFLE(2, 2, 2, 2));
    return v_map_in(r, in.map);
}

NV_RC_INLINE __m128 v_dot3(__m128 a, __m128 b)
{
    __m128 m = _mm_mul_ps(a, b);
    float d = _mm_cvtss_f32(m)
            + _mm_cvtss_f32(_mm_shuffle_ps(m, m, _MM_SHUFFLE(1, 1, 1, 1)));
    d = d + _mm_cvtss_f32(_mm_shuffle_ps(m, m, _MM_SHUFFLE(2, 2, 2, 2)));
    return _mm_set1_ps(d);
}

void nv2a_rc_run(const Nv2aRcProgram *p, const float v0[4],
                 const float v1[4], const float fog[4],
                 const float t[4][4], float out[4])
{
    const __m128 zero = _mm_setzero_ps(), one = _mm_set1_ps(1.0f);
    const __m128 lane_a = _mm_castsi128_ps(_mm_set_epi32(-1, 0, 0, 0));
    __m128 R[16], o;
    uint32_t s;

    /* Only the registers a program can read before writing need a value:
     * zero, r0, r1 and the final-combiner specials. */
    R[0] = R[6] = R[7] = R[R_R1] = R[R_SUM] = R[R_EF] = zero;
    R[R_FOG] = _mm_loadu_ps(fog);
    R[R_V0] = _mm_loadu_ps(v0);
    R[R_V1] = _mm_loadu_ps(v1);
    R[R_T0 + 0] = _mm_loadu_ps(t[0]);
    R[R_T0 + 1] = _mm_loadu_ps(t[1]);
    R[R_T0 + 2] = _mm_loadu_ps(t[2]);
    R[R_T0 + 3] = _mm_loadu_ps(t[3]);
    R[R_R0] = _mm_set_ps(p->r0a_t0 ? t[0][3] : 1.0f, 0.0f, 0.0f, 0.0f);

    for (s = 0; s < p->stages; s++) {
        const Nv2aRcStage *st = &p->st[s];
        uint32_t fl = st->fl, afl = st->afl;
        uint32_t m = fl & 0x38, am = afl & 0x38;
        __m128 A, B, C, D, aA, aB, aC, aD, pab, pcd, apab, apcd;
        int mux_cd = 0;

        R[R_C0] = _mm_loadu_ps(st->c0);
        R[R_C1] = _mm_loadu_ps(st->c1);
        if ((fl | afl) & 4) {
            float r0a = _mm_cvtss_f32(_mm_shuffle_ps(R[R_R0], R[R_R0],
                                                     _MM_SHUFFLE(3, 3, 3, 3)));
            mux_cd = p->mux_msb ? r0a >= 0.5f : ((int)(r0a * 255.0f) & 1);
        }

        /* Both portions read before either writes (psh.c emits all the
         * arithmetic, then the assignments). */
        A = v_read_rgb(R, st->in[0]); B = v_read_rgb(R, st->in[1]);
        C = v_read_rgb(R, st->in[2]); D = v_read_rgb(R, st->in[3]);
        aA = v_read_alpha(R, st->in[4]); aB = v_read_alpha(R, st->in[5]);
        aC = v_read_alpha(R, st->in[6]); aD = v_read_alpha(R, st->in[7]);

        pab = (fl & 2) ? v_dot3(A, B) : _mm_mul_ps(A, B);
        pcd = (fl & 1) ? v_dot3(C, D) : _mm_mul_ps(C, D);
        apab = _mm_mul_ps(aA, aB);
        apcd = _mm_mul_ps(aC, aD);

        /* Outputs, in the evaluator's write order: RGB ab, cd, sum, then the
         * alpha ones. Blue-to-alpha also writes the destination's .a. A
         * destination of 0 is the discard register, so nothing is computed
         * for it. */
        if (st->ab) {
            __m128 v = v_out(pab, m);
            R[st->ab] = (fl & 0x80) ? _mm_shuffle_ps(v, v, _MM_SHUFFLE(2, 2, 1, 0))
                                    : v_blend(v, R[st->ab], lane_a);
        }
        if (st->cd) {
            __m128 v = v_out(pcd, m);
            R[st->cd] = (fl & 0x40) ? _mm_shuffle_ps(v, v, _MM_SHUFFLE(2, 2, 1, 0))
                                    : v_blend(v, R[st->cd], lane_a);
        }
        if (st->sum) {
            __m128 sum = (fl & 4) ? (mux_cd ? pcd : pab) : _mm_add_ps(pab, pcd);
            R[st->sum] = v_blend(v_out(sum, m), R[st->sum], lane_a);
        }
        if (st->a_ab)
            R[st->a_ab] = v_blend(R[st->a_ab], v_out(apab, am), lane_a);
        if (st->a_cd)
            R[st->a_cd] = v_blend(R[st->a_cd], v_out(apcd, am), lane_a);
        if (st->a_sum) {
            __m128 sum = (afl & 4) ? (mux_cd ? apcd : apab) : _mm_add_ps(apab, apcd);
            R[st->a_sum] = v_blend(R[st->a_sum], v_out(sum, am), lane_a);
        }
    }

    if (p->final_on) {
        uint32_t fflags = p->fflags;
        __m128 a, b, sum, A, B, C, D, E, F;

        R[R_C0] = _mm_loadu_ps(p->fc0);
        R[R_C1] = _mm_loadu_ps(p->fc1);
        /* V1R0 sum: optional complements (0x40 v1, 0x20 r0), clamp (0x80). */
        a = (fflags & 0x40) ? _mm_sub_ps(one, R[R_V1]) : R[R_V1];
        b = (fflags & 0x20) ? _mm_sub_ps(one, R[R_R0]) : R[R_R0];
        sum = _mm_add_ps(a, b);
        if (fflags & 0x80)
            sum = v_clamp(sum, zero, one);
        R[R_SUM] = _mm_andnot_ps(lane_a, sum);
        E = v_read_rgb(R, p->fin[0]);
        F = v_read_rgb(R, p->fin[1]);
        R[R_EF] = _mm_andnot_ps(lane_a, _mm_mul_ps(E, F));
        A = v_read_rgb(R, p->fin[2]); B = v_read_rgb(R, p->fin[3]);
        C = v_read_rgb(R, p->fin[4]); D = v_read_rgb(R, p->fin[5]);
        /* D + mix(C, B, A), alpha from G. */
        o = _mm_add_ps(_mm_add_ps(D, _mm_mul_ps(C, _mm_sub_ps(one, A))),
                       _mm_mul_ps(B, A));
        o = v_blend(o, v_read_alpha(R, p->fin[6]), lane_a);
    } else {
        o = R[R_R0];
    }
    _mm_storeu_ps(out, v_clamp(o, zero, one));
}

#else /* portable */

NV_RC_INLINE float clampf(float x, float lo, float hi)
{
    return x < lo ? lo : (x > hi ? hi : x);
}

/* PS_INPUTMAPPING, psh.c get_input_var. */
NV_RC_INLINE float map_in(float x, unsigned map)
{
    float p = x < 0.0f ? 0.0f : x;
    switch (map) {
    case 0:  return p;                               /* unsigned identity */
    case 1:  return 1.0f - clampf(x, 0.0f, 1.0f);    /* unsigned invert   */
    case 2:  return 2.0f * p - 1.0f;                 /* expand normal     */
    case 3:  return -2.0f * p + 1.0f;                /* expand negate     */
    case 4:  return p - 0.5f;                        /* half-bias normal  */
    case 5:  return -p + 0.5f;                       /* half-bias negate  */
    case 6:  return x;                               /* signed identity   */
    default: return -x;                              /* signed negate     */
    }
}

/* PS_COMBINEROUTPUT scale/bias, psh.c get_output. */
NV_RC_INLINE float map_out(float x, uint32_t mapping)
{
    switch (mapping) {
    case 0x08: return x - 0.5f;
    case 0x10: return x * 2.0f;
    case 0x18: return (x - 0.5f) * 2.0f;
    case 0x20: return x * 4.0f;
    case 0x30: return x * 0.5f;
    default:   return x;
    }
}

NV_RC_INLINE float out_clamp(float x, uint32_t mapping)
{
    return clampf(map_out(x, mapping), -1.0f, 1.0f);
}

NV_RC_INLINE void read_rgb(float R[16][4], Nv2aRcInput in, float v[3])
{
    const float *r = R[in.reg];
    if (in.alpha) {
        v[0] = v[1] = v[2] = map_in(r[3], in.map);
    } else {
        v[0] = map_in(r[0], in.map);
        v[1] = map_in(r[1], in.map);
        v[2] = map_in(r[2], in.map);
    }
}

NV_RC_INLINE float read_alpha(float R[16][4], Nv2aRcInput in)
{
    return map_in(R[in.reg][in.alpha ? 3 : 2], in.map);
}

void nv2a_rc_run(const Nv2aRcProgram *p, const float v0[4],
                 const float v1[4], const float fog[4],
                 const float t[4][4], float out[4])
{
    float R[16][4];
    uint32_t s;
    int i;

    /* Only the registers a program can read before writing need a value:
     * zero, r0, r1 and the final-combiner specials. */
    for (i = 0; i < 4; i++) {
        R[0][i] = R[6][i] = R[7][i] = 0.0f;
        R[12][i] = R[13][i] = R[14][i] = R[15][i] = 0.0f;
    }
    for (i = 0; i < 4; i++) {
        R[R_FOG][i] = fog[i];
        R[R_V0][i] = v0[i];
        R[R_V1][i] = v1[i];
        R[R_T0 + 0][i] = t[0][i];
        R[R_T0 + 1][i] = t[1][i];
        R[R_T0 + 2][i] = t[2][i];
        R[R_T0 + 3][i] = t[3][i];
    }
    R[R_R0][3] = p->r0a_t0 ? t[0][3] : 1.0f;

    for (s = 0; s < p->stages; s++) {
        const Nv2aRcStage *st = &p->st[s];
        uint32_t fl = st->fl, afl = st->afl;
        uint32_t m = fl & 0x38, am = afl & 0x38;
        float A[3], B[3], C[3], D[3], pab[3], pcd[3];
        float aA, aB, aC, aD, apab, apcd;
        int mux_cd = 0;

        memcpy(R[R_C0], st->c0, 16);
        memcpy(R[R_C1], st->c1, 16);
        if ((fl | afl) & 4)
            mux_cd = p->mux_msb ? R[R_R0][3] >= 0.5f
                                : ((int)(R[R_R0][3] * 255.0f) & 1);

        /* Both portions read before either writes (psh.c emits all the
         * arithmetic, then the assignments). */
        read_rgb(R, st->in[0], A); read_rgb(R, st->in[1], B);
        read_rgb(R, st->in[2], C); read_rgb(R, st->in[3], D);
        aA = read_alpha(R, st->in[4]); aB = read_alpha(R, st->in[5]);
        aC = read_alpha(R, st->in[6]); aD = read_alpha(R, st->in[7]);

        if (fl & 2) {
            float dab = A[0] * B[0] + A[1] * B[1] + A[2] * B[2];
            pab[0] = pab[1] = pab[2] = dab;
        } else {
            for (i = 0; i < 3; i++)
                pab[i] = A[i] * B[i];
        }
        if (fl & 1) {
            float dcd = C[0] * D[0] + C[1] * D[1] + C[2] * D[2];
            pcd[0] = pcd[1] = pcd[2] = dcd;
        } else {
            for (i = 0; i < 3; i++)
                pcd[i] = C[i] * D[i];
        }
        apab = aA * aB;
        apcd = aC * aD;

        /* Outputs, in the evaluator's write order: RGB ab, cd, sum, then the
         * alpha ones. Blue-to-alpha also writes the destination's .a. A
         * destination of 0 is the discard register, so nothing is computed
         * for it. */
        if (st->ab) {
            float *d = R[st->ab];
            float ab2 = out_clamp(pab[2], m);
            d[0] = out_clamp(pab[0], m);
            d[1] = out_clamp(pab[1], m);
            d[2] = ab2;
            if (fl & 0x80) d[3] = ab2;
        }
        if (st->cd) {
            float *d = R[st->cd];
            float cd2 = out_clamp(pcd[2], m);
            d[0] = out_clamp(pcd[0], m);
            d[1] = out_clamp(pcd[1], m);
            d[2] = cd2;
            if (fl & 0x40) d[3] = cd2;
        }
        if (st->sum) {
            float *d = R[st->sum];
            for (i = 0; i < 3; i++) {
                float sum = (fl & 4) ? (mux_cd ? pcd[i] : pab[i])
                                     : pab[i] + pcd[i];
                d[i] = out_clamp(sum, m);
            }
        }
        if (st->a_ab)
            R[st->a_ab][3] = out_clamp(apab, am);
        if (st->a_cd)
            R[st->a_cd][3] = out_clamp(apcd, am);
        if (st->a_sum) {
            float sum = (afl & 4) ? (mux_cd ? apcd : apab) : apab + apcd;
            R[st->a_sum][3] = out_clamp(sum, am);
        }
    }

    if (p->final_on) {
        uint32_t fflags = p->fflags;
        float A[3], B[3], C[3], D[3], E[3], F[3];

        memcpy(R[R_C0], p->fc0, 16);
        memcpy(R[R_C1], p->fc1, 16);
        /* V1R0 sum: optional complements (0x40 v1, 0x20 r0), clamp (0x80). */
        for (i = 0; i < 3; i++) {
            float a = (fflags & 0x40) ? 1.0f - R[R_V1][i] : R[R_V1][i];
            float b = (fflags & 0x20) ? 1.0f - R[R_R0][i] : R[R_R0][i];
            R[R_SUM][i] = (fflags & 0x80) ? clampf(a + b, 0.0f, 1.0f) : a + b;
        }
        R[R_SUM][3] = 0.0f;
        read_rgb(R, p->fin[0], E);
        read_rgb(R, p->fin[1], F);
        for (i = 0; i < 3; i++)
            R[R_EF][i] = E[i] * F[i];
        R[R_EF][3] = 0.0f;
        read_rgb(R, p->fin[2], A); read_rgb(R, p->fin[3], B);
        read_rgb(R, p->fin[4], C); read_rgb(R, p->fin[5], D);
        /* D + mix(C, B, A), alpha from G. */
        for (i = 0; i < 3; i++)
            out[i] = D[i] + C[i] * (1.0f - A[i]) + B[i] * A[i];
        out[3] = read_alpha(R, p->fin[6]);
    } else {
        for (i = 0; i < 4; i++)
            out[i] = R[R_R0][i];
    }
    for (i = 0; i < 4; i++)
        out[i] = clampf(out[i], 0.0f, 1.0f);
}

#endif

void nv2a_rc_eval(const Nv2aCombiner *rc, const float v0[4],
                  const float v1[4], const float fog[4],
                  const float t[4][4], float out[4])
{
    Nv2aRcProgram p;
    nv2a_rc_compile(rc, &p);
    nv2a_rc_run(&p, v0, v1, fog, t, out);
}
