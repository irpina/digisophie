/* SPDX-License-Identifier: MIT */
/* Standalone continuity diagnostic for the fixed-point Sophie renderer. */
#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include "../sophie.h"

#define N 24000
static int32_t pcm[N];

static void reference_fuse(struct ds_params p)
{
    double c = 0, m1p = 0, m2p = 0, m3p = 0, fbz = 0, pitch = 1, attack = 0;
    double hz = p.phase_inc * 48000.0 / 65536.0;
    double color = p.color / 127.0, metal = p.metal / 127.0;
    double feedback = p.feedback / 127.0;
    uint32_t i;
    for (i = 0; i < N; ++i) {
        double r1 = .47 + color * color * 7.6;
        double r2 = 1.31 + color * 11.17;
        double r3 = 2.07 + (1 - color) * 15.73;
        double inc = 2 * 3.14159265358979323846 * hz / 48000.0;
        double fb, m1, m2, out, index;
        pitch *= exp(-1.0 / (.028 * 48000));
        attack += (1 - attack) * .085;
        c = fmod(c + inc, 2 * 3.14159265358979323846);
        m1p = fmod(m1p + inc * r1, 2 * 3.14159265358979323846);
        m2p = fmod(m2p + inc * r2, 2 * 3.14159265358979323846);
        m3p = fmod(m3p + inc * r3, 2 * 3.14159265358979323846);
        fb = fbz * feedback * (2 + metal * 10);
        m1 = sin(m1p + fb);
        m2 = sin(m2p + m1 * metal * 2.5);
        index = metal * metal * (2 + 24 * (.25 + pitch * .75));
        out = sin(c + index * (m2 * .72 + m1 * .28));
        fbz = (out * 1.4) / (1 + fabs(out * 1.4));
        pcm[i] = (int32_t)(out * attack * (p.velocity / 127.0) * 32767) * 65536;
    }
}

static void reference_probe(const char *name, struct ds_params p)
{
    uint32_t i, large = 0, scaled_large = 0, clipped = 0;
    int32_t prev = 0, prev_scaled = 0, peak = 0, max_jump = 0, max_scaled_jump = 0;
    uint64_t sum_jump = 0;
    reference_fuse(p);
    for (i = 0; i < N; ++i) {
        int32_t s = pcm[i] / 65536;
        int32_t a = s < 0 ? -s : s;
        int32_t jump = s - prev;
        int32_t scaled = (int32_t)((int64_t)s * 7200 / 32767);
        int32_t scaled_jump = scaled - prev_scaled;
        if (jump < 0) jump = -jump;
        if (scaled_jump < 0) scaled_jump = -scaled_jump;
        if (a > peak) peak = a;
        if (a >= 32766) ++clipped;
        if (i && jump > 8192) ++large;
        if (i && scaled_jump > 8192) ++scaled_large;
        if (jump > max_jump) max_jump = jump;
        if (scaled_jump > max_scaled_jump) max_scaled_jump = scaled_jump;
        sum_jump += (uint32_t)jump;
        prev = s;
        prev_scaled = scaled;
    }
    printf("%s: peak=%d mean_jump=%llu max_jump=%d jumps>8192=%u clipped=%u; source mix-only max_jump=%d jumps>8192=%u\n",
           name, peak, (unsigned long long)(sum_jump / N), max_jump, large, clipped,
           max_scaled_jump, scaled_large);
}

static void probe(const char *name, struct ds_params p)
{
    struct ds_voice v;
    uint32_t i, large = 0, at_blocks = 0, clipped = 0;
    int32_t prev = 0, max_jump = 0, peak = 0;
    uint64_t sum_jump = 0, sum_square = 0;
    ds_voice_init(&v);
    for (i = 0; i < N; i += DS_BLOCK_SIZE)
        ds_voice_render(&v, &p, i == 0, pcm + i, DS_BLOCK_SIZE);
    for (i = 0; i < N; ++i) {
        int32_t s = pcm[i] / 65536;
        int32_t a = s < 0 ? -s : s;
        int32_t jump = s - prev;
        if (jump < 0) jump = -jump;
        if (a > peak) peak = a;
        if (a >= 32766) ++clipped;
        if (i && jump > 8192) {
            ++large;
            if (!(i % DS_BLOCK_SIZE)) ++at_blocks;
            if (large <= 8)
                printf("  jump sample=%u time_ms=%.2f from=%d to=%d delta=%d\n",
                       i, i / 48.0, prev, s, jump);
        }
        if (jump > max_jump) max_jump = jump;
        sum_jump += (uint32_t)jump;
        sum_square += (uint64_t)(s * (int64_t)s);
        prev = s;
    }
    printf("%s: peak=%d rms2=%llu mean_jump=%llu max_jump=%d jumps>8192=%u block_jumps=%u clipped=%u\n",
           name, peak, (unsigned long long)(sum_square / N),
           (unsigned long long)(sum_jump / N), max_jump, large, at_blocks, clipped);
}

int main(void)
{
    struct ds_params p = {180, 0, 64, 90, 0, 32, 127, 0};
    uint8_t model;
    p.metal = 0; p.feedback = 0;
    probe("plain sine", p);
    p.metal = 32;
    probe("light FM", p);
    p.metal = 90; p.feedback = 32;
    p.phase_inc = 68; p.metal = 40; p.color = 64;
    probe("factory-ish low note", p);
    reference_probe("original floating-point low note", p);
    for (model = 1; model < 4; ++model) {
        p.model = model;
        probe("factory-ish alternate model", p);
    }
    p.model = 0;
    p.phase_inc = 180; p.metal = 90;
    reference_probe("original floating-point high-metal note", p);
    for (model = 0; model < 4; ++model) {
        p.model = model;
        probe("default", p);
        p.feedback = 127; p.metal = 127; p.color = 127;
        probe("hot", p);
        p.feedback = 32; p.metal = 90; p.color = 64;
    }
    return 0;
}
