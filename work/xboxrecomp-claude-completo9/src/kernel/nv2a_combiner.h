/**
 * NV2A register combiners, evaluated per pixel for the software rasteriser.
 *
 * Every pixel the NV2A writes goes through its register combiners: up to
 * eight general stages and a final stage that mix the vertex colours, the
 * four texture results, two constants per stage and fog. D3D's fixed-function
 * texture stages compile to them too, so a title never draws without them.
 * The executor used to stand in with "texel * diffuse" from stage 0, which is
 * right for a menu quad and wrong for anything that mixes two textures --
 * Burnout 3 composites its whole 3D scene into the frame that way.
 *
 * The encoding (ICW/OCW bit layout, input mappings, output scale/bias, mux,
 * final combiner) follows xemu's pixel-shader generator
 * (hw/xbox/nv2a/pgraph/glsl/psh.c); the comments in nv2a_combiner.c say where
 * each rule comes from. Portable C, no graphics API.
 */
#ifndef XBOXRECOMP_NV2A_COMBINER_H
#define XBOXRECOMP_NV2A_COMBINER_H

#include <stdint.h>

typedef struct {
    uint32_t color_icw[8], alpha_icw[8];    /* SET_COMBINER_COLOR/ALPHA_ICW */
    uint32_t color_ocw[8], alpha_ocw[8];    /* SET_COMBINER_COLOR/ALPHA_OCW */
    uint32_t factor0[8], factor1[8];        /* SET_COMBINER_FACTOR0/1 (ARGB) */
    uint32_t final0, final1;                /* SET_COMBINER_SPECULAR_FOG_CW0/1 */
    uint32_t final_c0, final_c1;            /* SET_SPECULAR_FOG_FACTOR */
    uint32_t control;                       /* SET_COMBINER_CONTROL */
    uint32_t stage_program;                 /* SET_SHADER_STAGE_PROGRAM */
} Nv2aCombiner;

/* One pixel. Colours are r,g,b,a in [0,1]; `fog` is (fog colour, factor).
 * t[i] is what texture stage i produced. The result is clamped to [0,1]. */
void nv2a_rc_eval(const Nv2aCombiner *rc, const float v0[4],
                  const float v1[4], const float fog[4],
                  const float t[4][4], float out[4]);

/* The same evaluation split in two, for the rasteriser: decode the
 * combiner words once when they change (they cannot change inside a batch),
 * then run the decoded program per pixel. nv2a_rc_run gives exactly what
 * nv2a_rc_eval gives for the Nv2aCombiner the program was compiled from. */
typedef struct {
    uint8_t reg, alpha, map;    /* register, .a select/replicate, mapping */
} Nv2aRcInput;

typedef struct {
    Nv2aRcInput in[8];          /* RGB A B C D, then alpha A B C D */
    uint32_t fl, afl;           /* OCW >> 12: scale/bias, dot, mux, blue->a */
    uint8_t ab, cd, sum;        /* RGB destinations (0: discard) */
    uint8_t a_ab, a_cd, a_sum;  /* alpha destinations */
    float c0[4], c1[4];
} Nv2aRcStage;

typedef struct {
    uint32_t stages;
    int mux_msb;                /* COMBINER_CONTROL mux select */
    int r0a_t0;                 /* r0.a starts as t0.a (stage 0 on) */
    Nv2aRcStage st[8];
    int final_on;
    uint32_t fflags;
    float fc0[4], fc1[4];
    Nv2aRcInput fin[7];         /* E F A B C D G */
} Nv2aRcProgram;

void nv2a_rc_compile(const Nv2aCombiner *rc, Nv2aRcProgram *p);
void nv2a_rc_run(const Nv2aRcProgram *p, const float v0[4],
                 const float v1[4], const float fog[4],
                 const float t[4][4], float out[4]);

/* D3DCOLOR (0xAARRGGBB) to r,g,b,a floats. */
void nv2a_rc_unpack(uint32_t argb, float out[4]);

#endif
