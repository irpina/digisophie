/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../sophie.h"

static uint64_t energy(const int32_t *x, unsigned n)
{
    unsigned i; uint64_t total = 0;
    for (i = 0; i < n; ++i) total += (uint32_t)(x[i] < 0 ? -(x[i] >> 16) : x[i] >> 16);
    return total;
}

static uint64_t power(const int32_t *x, unsigned n)
{
    unsigned i; uint64_t total = 0;
    for (i = 0; i < n; ++i) {
        int64_t s = x[i] / 65536;
        total += (uint64_t)(s * s);
    }
    return total;
}

static uint64_t difference(const int32_t *a, const int32_t *b, unsigned n)
{
    unsigned i; uint64_t total = 0;
    for (i = 0; i < n; ++i) {
        int32_t d = (a[i] >> 16) - (b[i] >> 16);
        total += (uint32_t)(d < 0 ? -d : d);
    }
    return total;
}

int main(void)
{
    enum { N = 48000 };
    static int32_t a[N], b[N];
    struct ds_voice va, vb;
    struct ds_params p = { 180, 0, 64, 90, 30, 30, 127, 0 };
    unsigned model, control, positive = 0, negative = 0, nonzero_late = 0;
    for (control = 0; control < 128; ++control)
        assert(ds_u7_q15((uint8_t)control) == (int32_t)(control * 32767u / 127u));
    ds_voice_init(&va);
    ds_voice_render(&va, &p, 1, a, N);
    assert(energy(a, 12000) > 50000);
    assert(energy(a + 36000, 12000) > 50000);
    for (control = 0; control < 4096; ++control) {
        if (a[control] > 0) ++positive;
        if (a[control] < 0) ++negative;
    }
    for (control = 4000; control < 8000; ++control)
        if (a[control]) ++nonzero_late;
    assert(positive > 500 && negative > 500); /* a genuine bipolar oscillator */
    assert(nonzero_late > 3000);              /* not a one-block impulse */
    ds_voice_init(&vb);
    ds_voice_render(&vb, &p, 1, b, N);
    assert(!memcmp(a, b, sizeof a));
    /* FOLD stays in Sophie's source path, with an exact zero bypass. */
    ds_fold_block(b, N, 0);
    assert(!memcmp(a, b, sizeof a));
    ds_fold_block(b, N, 127);
    assert(difference(a, b, N) > 100000);
    for (control = 0; control < N; ++control)
        assert(b[control] <= 0x7fff0000 && b[control] >= (int32_t)0x80000000);
    /* The common controls must produce four genuinely different voices;
     * BOOM keeps a pitched body while FUSE, PIPE and SHARD emphasize their
     * different metallic structures. */
    {
        static int32_t voices[4][4096];
        unsigned other;
        p = (struct ds_params){180, 0, 91, 98, 30, 60, 127, 0};
        for (model = 0; model < 4; ++model) {
            p.model = (uint8_t)model;
            ds_voice_init(&va);
            ds_voice_render(&va, &p, 1, voices[model], 4096);
            assert(va.amp > 0 && va.amp < 20000);
            for (other = 0; other < model; ++other)
                assert(difference(voices[model], voices[other], 4096) > 100000);
        }
    }
    /* PIPE's ring interaction keeps COLOR and FBK useful even with METAL
     * completely down, unlike the deliberately clean body of FUSE. */
    p = (struct ds_params){180, 2, 20, 0, 0, 0, 127, 0};
    ds_voice_init(&va); ds_voice_render(&va, &p, 1, a, 4096);
    p.color = 100; p.feedback = 100;
    ds_voice_init(&vb); ds_voice_render(&vb, &p, 1, b, 4096);
    assert(difference(a, b, 4096) > 100000);
    p.sweep = -64;
    ds_voice_init(&va); ds_voice_render(&va, &p, 1, a, 4096);
    p.sweep = 63;
    ds_voice_init(&vb); ds_voice_render(&vb, &p, 1, b, 4096);
    assert(difference(a, b, 4096) > 1000000); /* both sweep directions matter */
    for (model = 0; model < 4; ++model) for (control = 0; control < 128; control += 17) {
        unsigned i;
        p.model = (uint8_t)model; p.color = (uint8_t)control; p.metal = (uint8_t)(127 - control);
        p.sweep = (int8_t)control - 64; p.feedback = (uint8_t)control;
        ds_voice_init(&vb); ds_voice_render(&vb, &p, 1, b, DS_BLOCK_SIZE);
        for (i = 0; i < DS_BLOCK_SIZE; ++i) assert(b[i] <= 0x7fff0000 && b[i] >= (int32_t)0x80010000);
    }
    p.model = 3; p.color = 127; p.metal = 127;
    p.sweep = 63; p.feedback = 127;
    ds_voice_init(&vb); ds_voice_render(&vb, &p, 1, b, N);
    assert(energy(b, N) > 100000); /* worst-case controls remain live/bounded */
    /* The audited factory-like Fuse patch must not return to its former
     * 8.7 ms feedback burst. Bound normalized slope as well as absolute
     * jump, so simply reducing output gain cannot satisfy this check. */
    p = (struct ds_params){68, 0, 64, 40, 0, 32, 127, 0};
    ds_voice_init(&va); ds_voice_render(&va, &p, 1, a, N);
    {
        int32_t peak = 0, jump = 0;
        unsigned i;
        for (i = 1; i < N; ++i) {
            int32_t s = a[i] / 65536, d = s - a[i-1] / 65536;
            if (s < 0) s = -s;
            if (d < 0) d = -d;
            if (s > peak) peak = s;
            if (d > jump) jump = d;
        }
        assert(peak > 6000 && peak < 10000);
        assert(jump < 2000 && jump * 4 < peak);
        assert(va.pitch == 0); /* the pitch contour must settle exactly */
    }
    /* Audio must be invariant to render block size. */
    ds_voice_init(&vb);
    for (control = 0; control < N; control += DS_BLOCK_SIZE)
        ds_voice_render(&vb, &p, control == 0, b + control, DS_BLOCK_SIZE);
    assert(!memcmp(a, b, sizeof a));
    /* Retrigger and model changes start at the previous output, even when
     * velocity or oscillator parameters change sharply. */
    {
        int32_t last = b[N-1];
        p.velocity = 40; p.metal = 127;
        ds_voice_render(&vb, &p, 1, b, DS_BLOCK_SIZE);
        assert(b[0] == last);
        last = b[DS_BLOCK_SIZE-1];
        p.model = 2;
        ds_voice_render(&vb, &p, 0, b, DS_BLOCK_SIZE);
        assert(b[0] == last);
    }
    /* A quiet stock AMP envelope sleeps the oscillator. A held envelope
     * above the threshold never sleeps, and the next trig replaces the
     * sleeping voice from silence rather than replaying a stale sample. */
    p = (struct ds_params){68, 0, 64, 40, 0, 32, 127, 0};
    ds_voice_init(&vb);
    ds_voice_render(&vb, &p, 1, b, DS_BLOCK_SIZE);
    for (control = 0; control < 64; ++control)
        ds_voice_gate(&vb, 1 << 23, 3);
    assert(!vb.sleeping && vb.quiet_blocks == 0);
    /* No hidden source-duration timer: a held stock AMP envelope leaves the
     * oscillator alive for far longer than the brightness contour. */
    for (control = 0; control < 3000; ++control) {
        ds_voice_gate(&vb, 1 << 23, 3);
        ds_voice_render(&vb, &p, 0, b, DS_BLOCK_SIZE);
    }
    assert(!vb.sleeping && energy(b, DS_BLOCK_SIZE) > 100);
    for (control = 0; control < 64; ++control)
        ds_voice_gate(&vb, 0, 3);
    assert(!vb.sleeping && vb.quiet_blocks == 0);
    for (control = 0; control < 31; ++control)
        ds_voice_gate(&vb, 0, 0);
    assert(!vb.sleeping);
    ds_voice_gate(&vb, 0, 0);
    assert(!vb.sleeping && vb.fade_left == 128);
    for (control = 0; control < 8; ++control)
        ds_voice_render(&vb, &p, 0, b, DS_BLOCK_SIZE);
    assert(vb.sleeping && vb.active && vb.last == 0);
    ds_voice_gate(&vb, 1 << 23, 3);
    assert(!vb.sleeping && vb.transition == 64);
    ds_voice_gate(&vb, 0, 0);
    for (control = 0; control < 31; ++control) ds_voice_gate(&vb, 0, 0);
    assert(vb.fade_left == 128);
    for (control = 0; control < 8; ++control)
        ds_voice_render(&vb, &p, 0, b, DS_BLOCK_SIZE);
    assert(vb.sleeping);
    ds_voice_render(&vb, &p, 1, b, DS_BLOCK_SIZE);
    assert(!vb.sleeping && b[0] == 0 && energy(b, DS_BLOCK_SIZE) > 0);
    /* Sustained endpoints, all models, several pitches and both sweeps.
     * The output conditioner must leave headroom without muting the voice. */
    for (model = 0; model < 4; ++model) for (control = 0; control < 16; ++control) {
        unsigned i;
        p = (struct ds_params){ (control & 1) ? 24576 : 68, (uint8_t)model,
            (control & 2) ? 127 : 0, 127, (control & 4) ? 63 : -64,
            (control & 8) ? 127 : 0, 127, 0 };
        ds_voice_init(&vb); ds_voice_render(&vb, &p, 1, b, 4096);
        assert(energy(b, 4096) > 10000);
        for (i = 0; i < 4096; ++i)
            assert(b[i] / 65536 < 12000 && b[i] / 65536 > -12000);
    }
    /* The one-knob compensation keeps sustained level near the unfurled
     * source across all topologies, including the strongest fold setting. */
    for (model = 0; model < 4; ++model) {
        uint64_t plain, folded;
        p = (struct ds_params){68, (uint8_t)model, 64, 40, 0, 32, 127, 0};
        ds_voice_init(&va);
        ds_voice_render(&va, &p, 1, a, N);
        plain = power(a + 2400, N - 2400);
        memcpy(b, a, sizeof a);
        ds_fold_block(b, N, 127);
        folded = power(b + 2400, N - 2400);
        assert(folded * 3 > plain * 2 && folded * 2 < plain * 3);
    }
    puts("ok: Sophie fixed-point engine");
    return 0;
}
