/* The sound engine. It has three voices, and each one is a patch: a chain of up to six blocks, in the order its
 * player gave them, then the voice's fade, send and level. A changed value glides to its new place over one block.
 * The blocks:
 *   VCO   adds its wave to what comes in, so the first one starts the sound and a second one is a second oscillator
 *   VCF   one of the four filters
 *   VCA   a level
 *   FX    the reverb, with its own decay and tone, and a mix of the sound as it came and the reverb's
 * There is one reverb. While a patch has an FX block, the reverb is that block's: the first FX block of the first
 * such patch. The voices' sends then go nowhere, and the reverb is in the chain, one sound and not left and right.
 * A second FX block lets the sound through as it is.
 * A patch's three LFOs have a speed and a wave, and a cable to each value they move: an amount, -100 .. 100, for
 * each LFO on each value of each block and on the voice's level and send. A negative amount is the LFO upside down;
 * 0 is no cable. The cables are the block's own, so they go with it when it moves along the chain. An LFO also has
 * a depth, which all its cables share, and a cable to the speed and to the depth of each LFO.
 *
 * A patch has one feedback cable: the sound after one block goes back in before another one, through a level of
 * its own (up to twice: with enough of it the loop sings by itself) and bent softly to a full sample, so a loop
 * does not grow without end. What goes back is late by the cable's time, 16 to 271 frames (0.4 to 6.1 ms), as the
 * chain is done a piece at a time and a piece gets what the pieces before it left. So a loop also rings at
 * 1 / time and its multiples. An LFO's cable can move the feedback's level.
 * The scope (drone_scope_t) takes, when it is asked, a short picture of the two sounds that meet where the cable
 * goes back in: what comes down the chain and what the cable brings. It starts where the first one comes up
 * through zero, so that pictures of a steady sound are the same.
 *
 * After the voices comes the end of the chain. Each voice sends to it from after its fade and before its level, so
 * a voice with its level down can sound in the effects alone. The sends go into the delay (one line, up to a second,
 * with repeats and a tone), and the sends with the delay's echoes go into the one reverb. The echoes and the
 * reverb's sound (through RETURN, not the same left and right) come on top of the voices' own sum, and all of it
 * goes through the compressor, then the master's level.
 *
 * The filters are in the style of four old ones, not copies of their circuits:
 *   ladder    four poles (24 dB an octave), soft distortion at its input, resonance that sings by itself
 *   MS-20     two poles, the resonance's path cut off flat (a hard clip)
 *   Polivoks  two poles, each of its two stages distorts softly
 *   Wasp      two poles, a soft distortion that is not the same up and down
 *
 * Whole numbers only, and no product above 32 bits: felucca-bsp's docs/CPU.md reports wrong results from 64-bit
 * multiplies on this processor. It has no hardware code in it, so tests/drone_test.c runs it on a computer.
 *
 *   drone_init(&engine);
 *   drone_render(&engine, &controls, out, frames);    each audio block; out: left, right, 24-bit in int32_t
 */
#pragma once
#include <stdint.h>
#include "params.h"

#define DRONE_VOICES 3u
#define DRONE_LFOS 3u
#define DRONE_BLOCK_FRAMES 256u         /* the most frames a call renders */
#define DRONE_MASTER_MAX 256u
enum { DRONE_LEVEL, DRONE_FADE, DRONE_NPARAM };     /* a voice's own, after its chain */
enum { DRONE_SAW, DRONE_SQUARE, DRONE_TRIANGLE, DRONE_NWAVE };
enum { DRONE_LADDER, DRONE_MS20, DRONE_POLIVOKS, DRONE_WASP, DRONE_NFILTER };
enum { DRONE_LFO_SPEED, DRONE_LFO_WAVE, DRONE_LFO_DEPTH, DRONE_LFO_NPARAM };
enum { DRONE_LFO_SINE, DRONE_LFO_TRIANGLE, DRONE_LFO_SAW, DRONE_LFO_SQUARE, DRONE_LFO_NWAVE };
enum { DRONE_REVERB_DECAY, DRONE_REVERB_TONE, DRONE_REVERB_RETURN, DRONE_REVERB_NPARAM };
#define DRONE_SLOTS 6u                  /* the most blocks in a patch */
enum { DRONE_BLOCK_NONE, DRONE_BLOCK_VCO, DRONE_BLOCK_VCF, DRONE_BLOCK_VCA, DRONE_BLOCK_FX, DRONE_NBLOCK };
#define DRONE_BLOCK_NPARAM 3u
enum { DRONE_VCO_PITCH, DRONE_VCO_FINE };
enum { DRONE_VCF_CUTOFF, DRONE_VCF_RESONANCE };
enum { DRONE_VCA_LEVEL };
enum { DRONE_FX_MIX, DRONE_FX_DECAY, DRONE_FX_TONE };
enum { DRONE_OUT_LEVEL, DRONE_OUT_SEND, DRONE_OUT_NCABLED };   /* what of a patch's end the LFOs can move */
#define DRONE_AMOUNT_TOP 100
enum { DRONE_LFO_TO_SPEED, DRONE_LFO_TO_DEPTH, DRONE_LFO_NCABLED };     /* what of an LFO the LFOs can move */
enum { DRONE_FB_FROM, DRONE_FB_TO, DRONE_FB_LEVEL, DRONE_FB_TIME, DRONE_FB_NPARAM };
#define DRONE_FB_FRAMES(time) (16u + (time))     /* the feedback's time in frames */
#define DRONE_FB_PIECE 32u              /* with the feedback on, the chain is done this many frames at a time, or the time if that is less */
#define DRONE_FB_RING 512u              /* what the cable remembers: more than the longest time and a piece */
#define DRONE_SCOPE_POINTS 104u
#define DRONE_SCOPE_EVERY 4u            /* a point every 4 frames: the picture is 9.4 ms */
#define DRONE_REVERB_LINES 8u
#define DRONE_REVERB_FRAMES 23012u      /* all the lines together: 45 KiB */
/* the end of the chain, after the voices: the sends go into the delay, the delay into the reverb, and the voices with
 * the two into the compressor */
enum { DRONE_DELAY_TIME, DRONE_DELAY_REPEATS, DRONE_DELAY_TONE, DRONE_DELAY_LEVEL, DRONE_DELAY_NPARAM };
enum { DRONE_COMP_THRESHOLD, DRONE_COMP_RATIO, DRONE_COMP_RELEASE, DRONE_COMP_GAIN, DRONE_COMP_NPARAM };
#define DRONE_DELAY_FRAMES 44118u       /* what the delay remembers: a second, 86 KiB */
#define DRONE_COMP_PIECE 32u            /* the compressor finds its gain this many frames at a time: 0.7 ms */

static const param_spec_t drone_param_spec[DRONE_NPARAM] = {
    [DRONE_LEVEL] = { 127, 4, 32 },             /* a quarter: all the voices at once stay below the sum's flat top */
    [DRONE_FADE] = { 127, 4, 96 },              /* on and off in 5 ms .. 6 s */
};
static const param_spec_t drone_lfo_spec[DRONE_LFO_NPARAM] = {
    [DRONE_LFO_SPEED] = { 127, 2, 40 },         /* one cycle in 63 s .. 1/24 s, 12 steps to twice as fast; 40: 6 s */
    [DRONE_LFO_WAVE] = { DRONE_LFO_NWAVE - 1, 1, DRONE_LFO_SINE },
    [DRONE_LFO_DEPTH] = { 127, 4, 127 },        /* all of it: its cables say how far it moves a value */
};
/* a block's parameters; top 0: the block has no such parameter. The pitch in semitones, as MIDI numbers them: 42 is
 * F sharp 2, 92.5 Hz; the fine tuning half a semitone down .. up, 64 is in tune; the cutoff 20 Hz .. 7 kHz, 15 steps
 * an octave */
static const param_spec_t drone_block_spec[DRONE_NBLOCK][DRONE_BLOCK_NPARAM] = {
    [DRONE_BLOCK_VCO] = { [DRONE_VCO_PITCH] = { 127, 1, 42 }, [DRONE_VCO_FINE] = { 127, 1, 64 } },
    [DRONE_BLOCK_VCF] = { [DRONE_VCF_CUTOFF] = { 127, 2, 90 }, [DRONE_VCF_RESONANCE] = { 127, 4, 0 } },
    [DRONE_BLOCK_VCA] = { [DRONE_VCA_LEVEL] = { 127, 4, 127 } },
    /* the mix: 0 the sound as it came, 127 the reverb alone; the decay and the tone as the shared reverb's */
    [DRONE_BLOCK_FX] = { [DRONE_FX_MIX] = { 127, 4, 64 }, [DRONE_FX_DECAY] = { 127, 4, 64 }, [DRONE_FX_TONE] = { 127, 4, 64 } },
};
/* a block's kind: the VCO's wave, the VCF's filter; top 0: the block is of one kind only */
static const param_spec_t drone_block_kind_spec[DRONE_NBLOCK] = {
    [DRONE_BLOCK_VCO] = { DRONE_NWAVE - 1, 1, DRONE_SAW }, [DRONE_BLOCK_VCF] = { DRONE_NFILTER - 1, 1, DRONE_LADDER },
};
static const param_spec_t drone_block_type_spec = { DRONE_NBLOCK - 1, 1, DRONE_BLOCK_NONE };
/* how far an LFO with its amount at the top moves a block's value each way, in the value's own steps: an octave of
 * pitch, four octaves of cutoff, the whole resonance, the whole level. 0: the LFOs do not move it */
static const uint8_t drone_block_reach[DRONE_NBLOCK][DRONE_BLOCK_NPARAM] = {
    [DRONE_BLOCK_VCO] = { [DRONE_VCO_PITCH] = 12 },
    [DRONE_BLOCK_VCF] = { [DRONE_VCF_CUTOFF] = 64, [DRONE_VCF_RESONANCE] = 127 },
    [DRONE_BLOCK_VCA] = { [DRONE_VCA_LEVEL] = 127 },
    [DRONE_BLOCK_FX] = { [DRONE_FX_MIX] = 127 },
};
#define DRONE_OUT_REACH 127
/* an LFO's speed: two octaves each way (12 steps are twice as fast); its depth: all of it */
static const uint8_t drone_lfo_cable_reach[DRONE_LFO_NCABLED] = { [DRONE_LFO_TO_SPEED] = 24, [DRONE_LFO_TO_DEPTH] = 127 };
static const param_spec_t drone_fb_spec[DRONE_FB_NPARAM] = {
    [DRONE_FB_FROM] = { DRONE_SLOTS - 1, 1, 0 },    /* after this place's block */
    [DRONE_FB_TO] = { DRONE_SLOTS - 1, 1, 0 },      /* before this place's block */
    [DRONE_FB_LEVEL] = { 127, 4, 0 },               /* 0: no feedback; 64: as loud as it came; 127: twice */
    [DRONE_FB_TIME] = { 255, 4, 16 },               /* 16 frames and this many more: 0.36 .. 6.1 ms; 16: 0.7 ms */
};
static const param_spec_t drone_send_spec = { 127, 4, 32 };     /* a voice's send to the reverb */
enum { DRONE_LINK_FOLLOW, DRONE_LINK_THROUGH, DRONE_NLINK };
static const param_spec_t drone_link_kind_spec = { DRONE_NLINK - 1, 1, DRONE_LINK_FOLLOW };
static const param_spec_t drone_link_spec = { 127, 4, 0 };      /* a voice's link from the voice before it; 0: it is alone */
#define DRONE_LINK_REACH 48             /* a link all open, a voice at a full sample: the cutoff this many steps up, 3 octaves */
static const param_spec_t drone_reverb_spec[DRONE_REVERB_NPARAM] = {
    [DRONE_REVERB_DECAY] = { 127, 4, 64 },      /* down to a thousandth in 0.3 s .. 30 s; 64: 3 s */
    [DRONE_REVERB_TONE] = { 127, 4, 64 },       /* the tail's highs: lost above 1 kHz .. all kept */
    [DRONE_REVERB_RETURN] = { 127, 4, 127 },
};

static const param_spec_t drone_delay_spec[DRONE_DELAY_NPARAM] = {
    [DRONE_DELAY_TIME] = { 255, 4, 152 },       /* 1 ms .. 0.99 s, small steps at the short end; 152: 0.35 s */
    [DRONE_DELAY_REPEATS] = { 127, 4, 64 },     /* what goes back in: none .. a little more than all, so it goes on */
    [DRONE_DELAY_TONE] = { 127, 4, 100 },       /* the echoes' highs, as the reverb's tone */
    [DRONE_DELAY_LEVEL] = { 127, 4, 0 },        /* the echoes heard, and into the reverb; 0: no delay */
};
static const param_spec_t drone_comp_spec[DRONE_COMP_NPARAM] = {
    [DRONE_COMP_THRESHOLD] = { 127, 4, 95 },    /* 48 dB below a full sample .. a full sample, 3/8 dB a step; 95: -12 dB */
    [DRONE_COMP_RATIO] = { 127, 4, 0 },         /* how much of what is above the threshold it takes away: 0 none (1 to 1),
                                                 * 64 half (2 to 1), 127 all (a limiter) */
    [DRONE_COMP_RELEASE] = { 127, 4, 64 },      /* 20 ms .. 2 s for the gain to come back up 20 dB; 64: 0.2 s */
    [DRONE_COMP_GAIN] = { 127, 4, 0 },          /* louder after it: 0 .. 24 dB */
};

typedef struct {
    uint8_t type;                       /* DRONE_BLOCK_NONE: the chain ends here */
    uint8_t kind;
    uint8_t param[DRONE_BLOCK_NPARAM];
    int8_t amount[DRONE_BLOCK_NPARAM][DRONE_LFOS];      /* the cables: how far each LFO moves each value, -100 .. 100 */
} drone_block_controls_t;

typedef struct {
    uint8_t on;
    drone_block_controls_t block[DRONE_SLOTS];
    int8_t out_amount[DRONE_OUT_NCABLED][DRONE_LFOS];   /* a patch's cables to its level and its send */
    int8_t lfo_amount[DRONE_LFOS][DRONE_LFO_NCABLED][DRONE_LFOS];   /* and to an LFO's speed and depth: [to][what][from] */
    uint8_t fb[DRONE_FB_NPARAM];        /* a patch's feedback cable */
    int8_t fb_amount[DRONE_LFOS];       /* the LFOs' cables to the feedback's level */
    uint8_t param[DRONE_NPARAM];
    uint8_t lfo[DRONE_LFOS][DRONE_LFO_NPARAM];
    uint8_t send;                       /* to the delay (whose echoes go into the reverb) */
    uint8_t send_reverb;                /* to the reverb */
    uint8_t post;                       /* the two sends are taken after the level; 0: before it */
    int8_t pan;                         /* -64: all to the left; 0: the centre, all of it at both sides; 64: all to the right */
    uint8_t level_log;                  /* the level is a fader's place: drone_fader_q8 gives how loud */
    uint8_t link;                       /* how much it takes from the voice before it (the last one, for the first) */
    uint8_t link_kind;                  /* DRONE_LINK_FOLLOW: that voice's loudness opens this one's filters; DRONE_LINK_THROUGH:
                                         * that voice's sound goes down this one's chain, with its own oscillators' */
} drone_voice_controls_t;

typedef struct {
    drone_voice_controls_t voice[DRONE_VOICES];
    uint8_t reverb[DRONE_REVERB_NPARAM];
    uint8_t delay[DRONE_DELAY_NPARAM];
    uint8_t comp[DRONE_COMP_NPARAM];
    uint16_t master;                    /* 0..DRONE_MASTER_MAX */
} drone_controls_t;

typedef struct {                        /* a value on its way to a new one, in 1/32768: values 0..65535 */
    int32_t now, step;
} drone_glide_t;

typedef struct {                        /* what one block keeps from a render to the next: an oscillator's, a filter's, a level's */
    uint32_t phase, phase_step;
    int32_t state[4];                   /* the filter's states, 256 times a 16-bit sample */
    drone_glide_t cutoff, resonance;    /* the filter's two coefficients */
    drone_glide_t level;                /* a VCA's, 256 a step */
    uint8_t type, kind;                 /* what the states are of */
    uint8_t moving;                     /* it ran in the last block: its values glide from where they are */
} drone_block_t;

typedef struct {
    drone_block_t block[DRONE_SLOTS];   /* a patch's blocks */
    uint32_t lfo_phase[DRONE_LFOS];
    int32_t lfo_was[DRONE_LFOS];        /* a patch's LFOs in the block before: what their cables to each other carry */
    int16_t fb[DRONE_FB_RING];          /* a patch's feedback: the last frames from after the cable's block, in a ring */
    uint16_t fb_at;                     /* where the next frame goes in the ring */
    drone_glide_t fb_level;
    drone_glide_t level, send, send_reverb;     /* 256 a step */
    drone_glide_t left, right;          /* the pan: how much of it at each side, 32768 is all */
    uint32_t level_q16;                 /* the fade: 0 .. DRONE_LEVEL_TOP */
    int16_t sent[DRONE_BLOCK_FRAMES];   /* its sound after its fade, as the last call rendered it: what a THROUGH link takes */
    int32_t loud;                       /* how loud it is after its fade, 0 .. 32767: up at once, down slowly. The next voice's link reads it */
    uint8_t moving;                     /* it sounded in the last block: its values glide from where they are */
} drone_voice_t;

typedef struct {
    int16_t line[DRONE_REVERB_FRAMES];  /* the eight delay lines, one after the other */
    uint16_t at[DRONE_REVERB_LINES];    /* where each line is read, then written: its oldest sample */
    int32_t dark[DRONE_REVERB_LINES];   /* each line's tone filter, 16 times a sample */
    int32_t work[(DRONE_REVERB_LINES + 1u) * DRONE_BLOCK_FRAMES];      /* a block of each line, then of what goes in */
    drone_glide_t back;                 /* RETURN, 256 a step */
    uint8_t moving;                     /* it has run: RETURN glides from where it is */
} drone_reverb_t;

typedef struct {
    int16_t line[DRONE_DELAY_FRAMES];   /* the last second of what went in, in a ring */
    uint32_t at;                        /* where the next frame goes */
    uint32_t length;                    /* how far back it reads now, in frames: on its way to the time's */
    int32_t dark;                       /* the tone filter, 16 times a sample */
    drone_glide_t level;                /* 256 a step */
    uint8_t moving;                     /* it has run: the length and the level go from where they are */
} drone_delay_t;

typedef struct {
    int32_t loud_q16;                   /* how loud the sound is now: the log of its peak, falling at the release's speed */
    drone_glide_t gain;                 /* 256: as it is */
    uint8_t moving;
} drone_comp_t;

typedef struct {                        /* the main loop sets `voice` and `wanted`; the render fills it and clears `wanted` */
    volatile uint8_t wanted;
    volatile uint8_t voice;
    volatile uint8_t out;               /* the main loop sets it: the picture is of the voice's sound after its chain, not of its feedback */
    uint8_t running, skip;
    uint16_t at, waited;
    int32_t before;
    int16_t in[DRONE_SCOPE_POINTS], back[DRONE_SCOPE_POINTS];   /* down the chain; from the cable */
} drone_scope_t;

typedef struct {
    drone_voice_t voice[DRONE_VOICES];
    drone_scope_t scope;
    drone_reverb_t reverb;
    drone_delay_t delay;
    drone_comp_t comp;
    int32_t echo[DRONE_BLOCK_FRAMES];           /* out of the delay, at its level */
    int32_t one_voice[DRONE_BLOCK_FRAMES];
    int32_t mix[2u * DRONE_BLOCK_FRAMES];       /* the voices together, each where its pan puts it: left, right */
    int32_t send[DRONE_BLOCK_FRAMES];           /* the voices' sends to the delay together */
    int32_t send_reverb[DRONE_BLOCK_FRAMES];    /* and to the reverb; the delay's echoes join them */
    int32_t wet[2u * DRONE_BLOCK_FRAMES];       /* out of the reverb: left, right */
} drone_t;

#define DRONE_LEVEL_TOP 0x7FFF0000u
#define DRONE_STATE_TOP (1 << 26)       /* the filter's states stop here: 8 times a full sample, for the resonance */
#define DRONE_MS20_CLIP (1 << 22)       /* the MS-20's resonance path stops here: half a full sample */
#define DRONE_DRIVE 128                 /* the wave goes into a filter at half a full sample (128 of 256), so that
                                         * the resonance has room above it; drone_chain gives the half back */

/* the filter's coefficient for each cutoff value: 2 sin(pi f / rate) * 16384, f = 20 Hz * 2^(value / 15) */
static const uint16_t drone_cutoff_q14[128] = {
        47, 49, 51, 54, 56, 59, 62, 64, 68, 71, 74, 78, 81, 85, 89, 93,
        98, 102, 107, 112, 118, 123, 129, 135, 141, 148, 155, 163, 170, 178, 187, 195,
        205, 214, 225, 235, 246, 258, 270, 283, 296, 310, 325, 340, 356, 373, 391, 409,
        429, 449, 470, 493, 516, 540, 566, 593, 621, 650, 681, 713, 747, 782, 819, 858,
        898, 941, 985, 1032, 1080, 1132, 1185, 1241, 1300, 1361, 1425, 1493, 1563, 1637, 1715, 1796,
        1880, 1969, 2062, 2160, 2262, 2369, 2480, 2597, 2720, 2848, 2983, 3123, 3270, 3425, 3586, 3755,
        3932, 4117, 4310, 4513, 4725, 4946, 5178, 5421, 5675, 5940, 6218, 6508, 6812, 7129, 7460, 7806,
        8168, 8545, 8939, 9351, 9779, 10227, 10693, 11178, 11684, 12210, 12757, 13326, 13916, 14528, 15163, 15820
};

/* the same for a one-pole stage (the ladder, the MS-20): (1 - exp(-2 pi f / rate)) * 16384 */
static const uint16_t drone_pole_q14[128] = {
        47, 49, 51, 54, 56, 59, 61, 64, 67, 71, 74, 77, 81, 85, 89, 93,
        97, 102, 107, 112, 117, 123, 128, 135, 141, 147, 154, 162, 169, 177, 186, 194,
        203, 213, 223, 234, 244, 256, 268, 281, 294, 307, 322, 337, 353, 369, 386, 404,
        423, 443, 464, 485, 508, 532, 556, 582, 609, 637, 667, 698, 730, 764, 799, 836,
        874, 914, 956, 1000, 1046, 1094, 1143, 1195, 1250, 1307, 1366, 1427, 1492, 1559, 1629, 1702,
        1778, 1857, 1939, 2025, 2114, 2207, 2304, 2404, 2509, 2617, 2730, 2848, 2969, 3095, 3226, 3362,
        3503, 3649, 3799, 3955, 4117, 4284, 4456, 4633, 4817, 5006, 5200, 5400, 5606, 5817, 6034, 6257,
        6485, 6718, 6956, 7199, 7447, 7700, 7957, 8217, 8482, 8750, 9021, 9294, 9569, 9846, 10124, 10403,
};

/* a fader's curve: how loud for each place, as a level in 1/256 of a step (127 * 256 is all). 50 dB from the top
 * to place 1, each place 0.39 dB; place 0 is silent */
static const uint16_t drone_fader_q8[128] = {
    0, 108, 113, 118, 123, 129, 135, 141, 148, 155, 162, 169, 177, 185, 194, 203,
    212, 222, 232, 243, 255, 266, 279, 292, 305, 319, 334, 350, 366, 383, 400, 419,
    438, 459, 480, 502, 526, 550, 576, 602, 630, 659, 690, 722, 755, 790, 827, 865,
    906, 948, 992, 1037, 1086, 1136, 1189, 1244, 1301, 1362, 1425, 1491, 1560, 1632, 1708, 1787,
    1870, 1957, 2048, 2143, 2242, 2346, 2455, 2569, 2688, 2812, 2943, 3079, 3222, 3371, 3528, 3691,
    3862, 4041, 4229, 4425, 4630, 4845, 5069, 5304, 5550, 5808, 6077, 6359, 6654, 6962, 7285, 7623,
    7976, 8346, 8733, 9138, 9562, 10005, 10469, 10955, 11463, 11994, 12550, 13132, 13741, 14378, 15045, 15743,
    16473, 17237, 18036, 18872, 19747, 20663, 21621, 22624, 23673, 24770, 25919, 27121, 28378, 29694, 31071, 32512,
};

/* the phase step each sample, of 2^32 a cycle, at the FM-1's sample rate 24000000 / 544 Hz, for a pitch in 1/256
 * semitone (it stops at pitch 0 and 127) */
static uint32_t drone_step_q8(int32_t pitch_q8)
{
    static const uint32_t f3_to_f4[13] = {      /* 174.614 Hz * 2^(n / 12) * 2^32 * 544 / 24000000 */
        16999137u, 18009958u, 19080886u, 20215494u, 21417570u, 22691125u,
        24040410u, 25469927u, 26984448u, 28589026u, 30289018u, 32090097u, 33998274u,
    };
    uint32_t pitch = pitch_q8 < 0 ? 0 : pitch_q8 > 127 * 256 ? 127u * 256u : (uint32_t)pitch_q8;
    uint32_t from_f = (pitch >> 8) + 7u, octave = from_f / 12u;     /* F3 is pitch 53: 53 + 7 = 5 octaves */
    uint32_t low = f3_to_f4[from_f % 12u], high = f3_to_f4[from_f % 12u + 1u];
    uint32_t step = low + (((high - low) >> 8) * (pitch & 255u));
    return octave < 5u ? step >> (5u - octave) : step << (octave - 5u);
}

static uint32_t drone_pitch_step(uint32_t pitch, uint32_t fine)     /* fine: 0..127, half a semitone down .. up */
{
    return drone_step_q8((int32_t)(pitch * 256u + fine * 2u) - 128);
}

/* a * b / 16384 without a 64-bit product: |a| below 2^28, b 0..32768 */
static inline int32_t drone_times_q14(int32_t a, int32_t b)
{
    return (((a >> 12) * b) >> 2) + (((a & 0xFFF) * b) >> 14);
}

static inline int32_t drone_limited(int32_t value, int32_t top)
{
    return value > top ? top : value < -top ? -top : value;
}

/* a table's value at a place between two entries: the place in 1/256 of an entry, 0 .. 127 * 256 */
static int32_t drone_between(const uint16_t *table, int32_t place_q8)
{
    int32_t at = place_q8 >> 8, low = table[at], high = table[at < 127 ? at + 1 : 127];
    return low + (((high - low) * (place_q8 & 255)) >> 8);
}

/* soft distortion: a small signal as it is, a big one bent over to stop at a full sample (32767), the same up
 * and down. x - 4/27 x^3, which is flat at 1.5 times a full sample */
static int32_t drone_soft(int32_t x)
{
    uint32_t size = (uint32_t)(x < 0 ? -x : x), cube;
    if (size >= 49152u)
        return x < 0 ? -32767 : 32767;
    cube = (((size * size) >> 15) * size) >> 15;        /* x^3 / 32768^2: the products stay below 2^32 */
    size -= (cube * 4855u) >> 15;
    size = size > 32767u ? 32767u : size;
    return x < 0 ? -(int32_t)size : (int32_t)size;
}

/* how far a filter's state (256 times a sample) is above its softly distorted self: nothing for a small one */
static inline int32_t drone_excess(int32_t state_q8)
{
    return ((state_q8 >> 8) - drone_soft(state_q8 >> 8)) * 256;
}

/* the Wasp's distortion: soft, and the top half more than the bottom half */
static int32_t drone_uneven(int32_t x)
{
    uint32_t size = (uint32_t)(x < 0 ? -x : x);
    size = size > 65535u ? 65535u : size;
    return drone_soft((x < 0 ? -(int32_t)size : (int32_t)size) - (int32_t)((size * size) >> 17));
}

/* an LFO's value where it is in its cycle (2^32 a cycle): -32767..32767 */
static int32_t drone_lfo_wave(uint32_t wave, uint32_t phase)
{
    int32_t ramp = (int32_t)phase >> 16, size = ramp < 0 ? -ramp : ramp, value;
    if (wave == DRONE_LFO_SQUARE)
        value = ramp < 0 ? -32767 : 32767;
    else if (wave == DRONE_LFO_SAW)
        value = ramp;
    else if (wave == DRONE_LFO_TRIANGLE)
        value = ramp < 0 ? -(size < 16384 ? 2 * size : 65535 - 2 * size) : size < 16384 ? 2 * size : 65535 - 2 * size;
    else
        value = (ramp * (32768 - size)) >> 13;          /* two parabolas: close to a sine */
    return drone_limited(value, 32767);
}

static void drone_glide_to(drone_glide_t *glide, int32_t value, uint32_t frames, int at_once)
{
    if (at_once)
        glide->now = value << 15;
    glide->step = ((value << 15) - glide->now) / (int32_t)frames;
}
#define DRONE_GLIDE(glide) (((glide).now += (glide).step) >> 15)

/* What to add to a wave near a fall of its full height, to make the edge smooth (PolyBLEP): `since` is the phase
 * gone since the fall, so just below 2^32 before it. Only the sample before the fall and the one after get some. */
static inline int32_t drone_edge(uint32_t since, uint32_t step)
{
    int32_t gap;
    if (since < step) {
        gap = 32768 - (int32_t)(since / (step >> 15));
        return gap > 0 ? (gap * gap) >> 15 : 0;
    }
    if (0u - since <= step) {
        gap = 32768 - (int32_t)((0u - since) / (step >> 15));
        return gap > 0 ? -((gap * gap) >> 15) : 0;
    }
    return 0;
}

/* the wave, -32768..32767, added to what `out` has; its pitch on the way to step_to. The block's phase_step is
 * never below 2^15 (pitch 0 is 24 times that) */
static void drone_oscillator(drone_block_t *v, uint32_t wave, uint32_t step_to, int32_t *out, uint32_t frames)
{
    int32_t step_change = ((int32_t)step_to - (int32_t)v->phase_step) / (int32_t)frames;
    uint32_t i;
    for (i = 0; i < frames; i++) {
        uint32_t step = v->phase_step += (uint32_t)step_change, phase = v->phase += step;
        if (wave == DRONE_SQUARE) {                     /* it falls at the half cycle and rises at the start */
            out[i] += (phase & 0x80000000u ? -32767 : 32767) + drone_edge(phase - 0x80000000u, step) - drone_edge(phase, step);
        } else if (wave == DRONE_TRIANGLE) {
            int32_t up = (int32_t)(phase >> 15);        /* 0..131071 a cycle */
            out[i] += up < 65536 ? up - 32768 : 98303 - up;
        } else {                                        /* the saw falls at the half cycle */
            out[i] += ((int32_t)phase >> 16) + drone_edge(phase - 0x80000000u, step);
        }
    }
}

/* ---- the filters: each takes a block of the wave and leaves what it lets through, -32767..32767.
 * The block's cutoff is the coefficient of one stage; its resonance is the filter's own (see drone_filter) ---- */

/* four one-pole stages in a row; the last one's output, times up to 4.2, is taken from the input */
static void drone_ladder(drone_block_t *v, int32_t *wave, uint32_t frames)
{
    int32_t *stage = v->state;
    uint32_t i;
    for (i = 0; i < frames; i++) {
        int32_t pole = DRONE_GLIDE(v->cutoff), back_quarter = DRONE_GLIDE(v->resonance);
        int32_t in = drone_soft(((wave[i] * DRONE_DRIVE) >> 8) - (drone_times_q14(stage[3], back_quarter) >> 6)) * 256;
        stage[0] += drone_times_q14(in - stage[0], pole);
        stage[1] += drone_times_q14(stage[0] - stage[1], pole);
        stage[2] += drone_times_q14(stage[1] - stage[2], pole);
        stage[3] += drone_times_q14(stage[2] - stage[3], pole);
        wave[i] = drone_limited(stage[3] >> 8, 32767);
    }
}

/* two one-pole stages; what is between them, times up to 2.1 and cut off flat, goes back to the input */
static void drone_ms20(drone_block_t *v, int32_t *wave, uint32_t frames)
{
    int32_t *stage = v->state;
    uint32_t i;
    for (i = 0; i < frames; i++) {
        int32_t pole = DRONE_GLIDE(v->cutoff), back_half = DRONE_GLIDE(v->resonance);
        int32_t back = drone_limited(2 * drone_times_q14(stage[0] - stage[1], back_half), DRONE_MS20_CLIP);
        stage[0] += drone_times_q14(wave[i] * DRONE_DRIVE - stage[0] + back, pole);
        stage[1] += drone_times_q14(stage[0] - stage[1], pole);
        wave[i] = drone_limited(stage[1] >> 8, 32767);
    }
}

/* a state-variable filter (low is the output, band feeds the resonance); each of its two stages gives way softly
 * when it is loud, which also keeps its resonance near a full sample */
static void drone_polivoks(drone_block_t *v, int32_t *wave, uint32_t frames)
{
    int32_t *low = &v->state[0], *band = &v->state[1];
    uint32_t i;
    for (i = 0; i < frames; i++) {
        int32_t cutoff = DRONE_GLIDE(v->cutoff), damping = DRONE_GLIDE(v->resonance), high;
        *low = drone_limited(*low + drone_times_q14(*band - drone_excess(*low), cutoff), DRONE_STATE_TOP);
        high = wave[i] * DRONE_DRIVE - *low - drone_times_q14(*band, damping);
        *band = drone_limited(*band + drone_times_q14(high - drone_excess(*band), cutoff), DRONE_STATE_TOP);
        wave[i] = drone_limited(*low >> 8, 32767);
    }
}

/* a state-variable filter with the uneven distortion where the input and the two stages meet. The distortion
 * moves the output off zero, so the third state follows that slowly (3 Hz) and it is taken away */
static void drone_wasp(drone_block_t *v, int32_t *wave, uint32_t frames)
{
    int32_t *low = &v->state[0], *band = &v->state[1], *off_zero = &v->state[2];
    uint32_t i;
    for (i = 0; i < frames; i++) {
        int32_t cutoff = DRONE_GLIDE(v->cutoff), damping = DRONE_GLIDE(v->resonance), high;
        *low = drone_limited(*low + drone_times_q14(*band, cutoff), DRONE_STATE_TOP);
        high = drone_uneven((wave[i] * DRONE_DRIVE - *low - drone_times_q14(*band, damping)) >> 8) * 256;
        *band = drone_limited(*band + drone_times_q14(high, cutoff), DRONE_STATE_TOP);
        *off_zero += (*low - *off_zero) >> 11;
        wave[i] = drone_limited((*low - *off_zero) >> 8, 32767);
    }
}

/* The reverb: eight delay lines of different lengths (prime numbers of frames, 32 ms .. 98 ms). What comes out
 * of the lines is mixed, each into all (a Hadamard matrix: sums and differences only, and it keeps the loudness),
 * and goes back in a little quieter: that is the decay. A low-pass filter on each line makes the highs go first.
 * Everything in the loop rounds toward zero, so a tail ends in true silence. */
static const uint16_t drone_reverb_length[DRONE_REVERB_LINES] = { 1423, 1871, 2269, 2647, 3067, 3491, 3907, 4337 };
static const uint16_t drone_reverb_start[DRONE_REVERB_LINES] = { 0, 1423, 3294, 5563, 8210, 11277, 14768, 18675 };
_Static_assert(18675 + 4337 == DRONE_REVERB_FRAMES, "the lines fill the memory");
/* what goes back, for each decay value: 8192 / sqrt(8) (the mixing's own gain) times 10^(-3 d / t), where d is
 * the lines' mean length, 65.2 ms, and t = 0.3 s * 100^(value / 127) is the time down to a thousandth */
static const uint16_t drone_reverb_back_q13[128] = {
    645, 681, 717, 753, 790, 828, 866, 904, 942, 980, 1019, 1058, 1096, 1135, 1173, 1212,
    1250, 1288, 1326, 1363, 1400, 1437, 1473, 1509, 1544, 1579, 1614, 1648, 1681, 1714, 1746, 1778,
    1809, 1840, 1870, 1899, 1928, 1956, 1984, 2011, 2037, 2063, 2088, 2112, 2136, 2159, 2182, 2204,
    2226, 2247, 2267, 2287, 2306, 2325, 2343, 2361, 2378, 2395, 2411, 2427, 2442, 2457, 2472, 2486,
    2499, 2512, 2525, 2538, 2550, 2561, 2572, 2583, 2594, 2604, 2614, 2623, 2633, 2642, 2650, 2659,
    2667, 2675, 2682, 2690, 2697, 2704, 2710, 2717, 2723, 2729, 2735, 2740, 2746, 2751, 2756, 2761,
    2766, 2770, 2774, 2779, 2783, 2787, 2791, 2794, 2798, 2801, 2805, 2808, 2811, 2814, 2817, 2820,
    2822, 2825, 2827, 2830, 2832, 2834, 2837, 2839, 2841, 2843, 2845, 2846, 2848, 2850, 2852, 2853,
};

/* value / 2^bits, rounded toward zero. Not written as a division: the compiler makes a hardware divide of one,
 * and 27 of them a frame cost 7 % of the processor's time (measured on the FM-1: V0.7 against V0.8) */
static inline int32_t drone_toward_zero(int32_t value, uint32_t bits)
{
    return (value + ((value >> 31) & ((1 << bits) - 1))) >> bits;
}
#define DRONE_SUM_AND_DIFFERENCE(a, b) do { int32_t sum = (a) + (b); (b) = (a) - (b); (a) = sum; } while (0)

/* in: the sends' sum; wet: left, right, each -65534..65534. It does a whole block of one line at a time, in
 * small loops, to cost less than frame by frame; the result is the same, as a block is shorter than the shortest
 * line, so nothing a block writes is read in that block */
_Static_assert(DRONE_BLOCK_FRAMES < 1423, "a block is shorter than the shortest line");
static void drone_reverb(drone_reverb_t *r, const uint8_t *param, const int32_t *in, int32_t *wet, uint32_t frames)
{
    int32_t back = drone_reverb_back_q13[param[DRONE_REVERB_DECAY] & 127u];
    int32_t tone = 32 + (param[DRONE_REVERB_TONE] & 127) * 224 / 127;   /* of 256: the filter open at the top */
    int32_t *feed = r->work + DRONE_REVERB_LINES * DRONE_BLOCK_FRAMES;
    uint32_t i, k;
    for (i = 0; i < frames; i++) {
        feed[i] = drone_soft(in[i]) >> 2;       /* a quarter: a long decay piles the sound up */
        wet[2u * i] = wet[2u * i + 1u] = 0;
    }
    for (k = 0; k < DRONE_REVERB_LINES; k++) {  /* out of each line: to the left or the right, and through its tone filter */
        const int16_t *begin = r->line + drone_reverb_start[k], *end = begin + drone_reverb_length[k], *from = begin + r->at[k];
        int32_t dark = r->dark[k], down = k & 2u ? -1 : 0, *x = r->work + k * DRONE_BLOCK_FRAMES, *side = wet + (k & 1u);
        for (i = 0; i < frames; i++) {
            int32_t sample = *from;
            if (++from == end)
                from = begin;
            side[2u * i] += (sample ^ down) - down;     /* lines 2, 3, 6 and 7 upside down */
            dark += drone_toward_zero((sample * 16 - dark) * tone, 8);
            x[i] = drone_toward_zero(dark, 4);
        }
        r->dark[k] = dark;
    }
    for (i = 0; i < frames; i++) {              /* each into all: sums and differences */
        int32_t *x = r->work + i;
        int32_t x0 = x[0], x1 = x[DRONE_BLOCK_FRAMES], x2 = x[2u * DRONE_BLOCK_FRAMES], x3 = x[3u * DRONE_BLOCK_FRAMES];
        int32_t x4 = x[4u * DRONE_BLOCK_FRAMES], x5 = x[5u * DRONE_BLOCK_FRAMES], x6 = x[6u * DRONE_BLOCK_FRAMES];
        int32_t x7 = x[7u * DRONE_BLOCK_FRAMES];
        DRONE_SUM_AND_DIFFERENCE(x0, x1);
        DRONE_SUM_AND_DIFFERENCE(x2, x3);
        DRONE_SUM_AND_DIFFERENCE(x4, x5);
        DRONE_SUM_AND_DIFFERENCE(x6, x7);
        DRONE_SUM_AND_DIFFERENCE(x0, x2);
        DRONE_SUM_AND_DIFFERENCE(x1, x3);
        DRONE_SUM_AND_DIFFERENCE(x4, x6);
        DRONE_SUM_AND_DIFFERENCE(x5, x7);
        DRONE_SUM_AND_DIFFERENCE(x0, x4);
        DRONE_SUM_AND_DIFFERENCE(x1, x5);
        DRONE_SUM_AND_DIFFERENCE(x2, x6);
        DRONE_SUM_AND_DIFFERENCE(x3, x7);
        x[0] = x0, x[DRONE_BLOCK_FRAMES] = x1, x[2u * DRONE_BLOCK_FRAMES] = x2, x[3u * DRONE_BLOCK_FRAMES] = x3;
        x[4u * DRONE_BLOCK_FRAMES] = x4, x[5u * DRONE_BLOCK_FRAMES] = x5, x[6u * DRONE_BLOCK_FRAMES] = x6;
        x[7u * DRONE_BLOCK_FRAMES] = x7;
        wet[2u * i] >>= 1;
        wet[2u * i + 1u] >>= 1;
    }
    for (k = 0; k < DRONE_REVERB_LINES; k++) {  /* back into each line, a little quieter, with what goes in */
        int16_t *begin = r->line + drone_reverb_start[k], *end = begin + drone_reverb_length[k], *to = begin + r->at[k];
        const int32_t *x = r->work + k * DRONE_BLOCK_FRAMES;
        for (i = 0; i < frames; i++) {          /* 8 samples' sum times 2896: below 2^31 */
            *to = (int16_t)drone_limited(feed[i] + drone_toward_zero(x[i] * back, 13), 32767);
            if (++to == end)
                to = begin;
        }
        r->at[k] = (uint16_t)(to - begin);
    }
}

/* ---- the delay: one line, a second long. What comes out of it goes through the tone filter, is heard at the delay's
 * level, and goes back in by the repeats. A new time is not a jump: the place it reads from moves there, a frame
 * every two frames, so the echoes bend down or up for a moment, as a tape's ---- */
static uint32_t drone_delay_length(uint32_t time)       /* in frames: 44 .. 43773, the square of the knob */
{
    time = time > 255u ? 255u : time;
    return 44u + ((time * time * 44073u) >> 16);
}
_Static_assert(44u + ((255u * 255u * 44073u) >> 16) < DRONE_DELAY_FRAMES, "the longest time is in the line");

/* in: the sends' sum; echo: what is heard of the delay, -32767..32767 */
static void drone_delay(drone_delay_t *d, const uint8_t *param, const int32_t *in, int32_t *echo, uint32_t frames)
{
    uint32_t to = drone_delay_length(param[DRONE_DELAY_TIME]), at = d->at, length = d->moving ? d->length : to, i;
    int32_t repeats = param[DRONE_DELAY_REPEATS] & 127, dark = d->dark;
    int32_t tone = 32 + (param[DRONE_DELAY_TONE] & 127) * 224 / 127;    /* of 256: the filter open at the top */
    repeats += repeats >> 5;                    /* of 128: 130 at the top */
    drone_glide_to(&d->level, (int32_t)(param[DRONE_DELAY_LEVEL] & 127u) * 256, frames, !d->moving);
    for (i = 0; i < frames; i++) {
        int32_t heard;
        if (length != to && (i & 1u))
            length += length < to ? 1u : 0u - 1u;
        dark += drone_toward_zero((d->line[at >= length ? at - length : at + DRONE_DELAY_FRAMES - length] * 16 - dark) * tone, 8);
        heard = drone_toward_zero(dark, 4);
        d->line[at] = (int16_t)drone_soft(drone_soft(in[i]) + drone_toward_zero(heard * repeats, 7));
        if (++at == DRONE_DELAY_FRAMES)
            at = 0;
        echo[i] = drone_toward_zero(heard * DRONE_GLIDE(d->level), 15);
    }
    d->at = at;
    d->length = length;
    d->dark = dark;
    d->moving = 1;
}

/* ---- the compressor: above the threshold, the sound gets quieter by the ratio's part of what it is above (in dB);
 * then the gain makes all of it louder. It works in logs: 4096 is twice as loud (6 dB) ---- */
/* log2(x) * 4096, x from 1 to 2^19: the place of the top bit, and a curve for what is after it (0.5 % near) */
static int32_t drone_log_q12(uint32_t x)
{
    int32_t top = 0, part;
    x = x < 1u ? 1u : x > 0x7FFFFu ? 0x7FFFFu : x;
    while (x >> (top + 1))
        top++;
    part = (int32_t)((x << 12) >> top) - 4096;
    return top * 4096 + part + ((((part * (4096 - part)) >> 12) * 1408) >> 12);
}
/* 2^(log / 4096), the way back; 8 * 4096 gives 256 to the bit: the gain of one */
static int32_t drone_exp_q12(int32_t log)
{
    int32_t whole, part, value;
    log = log < 0 ? 0 : log > (20 << 12) ? 20 << 12 : log;
    whole = log >> 12;
    part = log & 4095;
    value = 4096 + part - ((((part * (4096 - part)) >> 12) * 1408) >> 12);
    return whole >= 12 ? value << (whole - 12) : value >> (12 - whole);
}
/* the release: the time for the gain to come back up 20 dB, in ms, for each value: 20 * 100^(value / 127) */
static const uint16_t drone_release_ms[128] = {
    20, 21, 22, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34,
    36, 37, 38, 40, 41, 43, 44, 46, 48, 50, 51, 53, 55, 57, 59, 62,
    64, 66, 69, 71, 74, 77, 79, 82, 85, 88, 92, 95, 99, 102, 106, 110,
    114, 118, 123, 127, 132, 137, 142, 147, 152, 158, 164, 170, 176, 183, 189, 196,
    204, 211, 219, 227, 235, 244, 253, 263, 272, 282, 293, 303, 315, 326, 338, 351,
    364, 377, 391, 406, 421, 436, 452, 469, 486, 504, 523, 542, 562, 583, 604, 627,
    650, 674, 699, 725, 751, 779, 808, 838, 869, 901, 934, 968, 1004, 1041, 1080, 1120,
    1161, 1204, 1248, 1294, 1342, 1392, 1443, 1496, 1552, 1609, 1668, 1730, 1794, 1860, 1929, 2000,
};

/* out: the voices and the effects together, left and right, before the limit; then the same through the
 * compressor, cut at a full sample and at the master's level: the 24-bit range. A piece at a time: the loudest
 * frame of the piece sets where the gain is at the piece's end */
static void drone_compress(drone_comp_t *k, const uint8_t *param, int32_t *out, uint32_t frames, int32_t master)
{
    int32_t threshold = (15 << 12) - (127 - (param[DRONE_COMP_THRESHOLD] & 127)) * 256;
    int32_t ratio = (param[DRONE_COMP_RATIO] & 127) * 129, louder = (param[DRONE_COMP_GAIN] & 127) * 129;   /* 16383 at the top */
    /* 20 dB in a log of 65536 for each 6 dB is 217700; a piece is 0.7253 ms */
    int32_t fall = (int32_t)(157900u * DRONE_COMP_PIECE / 32u / drone_release_ms[param[DRONE_COMP_RELEASE] & 127u]);
    uint32_t at, i;
    for (at = 0; at < frames; at += DRONE_COMP_PIECE) {
        uint32_t piece = frames - at < DRONE_COMP_PIECE ? frames - at : DRONE_COMP_PIECE;
        int32_t *sound = out + 2u * at, peak = 0, loud, above;
        for (i = 0; i < 2u * piece; i++) {
            int32_t size = sound[i] < 0 ? -sound[i] : sound[i];
            peak = size > peak ? size : peak;
        }
        loud = drone_log_q12((uint32_t)peak) << 4;
        k->loud_q16 = loud > k->loud_q16 - fall ? loud : k->loud_q16 - fall;
        above = (k->loud_q16 >> 4) - threshold;
        above = above > 0 ? (above * ratio) >> 14 : 0;          /* 19 * 4096 * 16383: below 2^31 */
        drone_glide_to(&k->gain, drone_exp_q12((8 << 12) + louder - above), piece, !k->moving);
        k->moving = 1;
        for (i = 0; i < piece; i++) {           /* a sum of 2^18 times a gain of 2^12: below 2^31 */
            int32_t gain = DRONE_GLIDE(k->gain);
            sound[2u * i] = drone_limited((sound[2u * i] * gain) >> 8, 32767) * master;
            sound[2u * i + 1u] = drone_limited((sound[2u * i + 1u] * gain) >> 8, 32767) * master;
        }
    }
}

static void drone_block_rest(drone_block_t *b)      /* the next start is from rest */
{
    b->state[0] = b->state[1] = b->state[2] = b->state[3] = 0;
    b->moving = 0;
}

static void drone_voice_rest(drone_voice_t *v)
{
    uint32_t s;
    for (s = 0; s < DRONE_SLOTS; s++)
        drone_block_rest(&v->block[s]);
    for (s = 0; s < DRONE_FB_RING; s++)
        v->fb[s] = 0;
    for (s = 0; s < DRONE_BLOCK_FRAMES; s++)
        v->sent[s] = 0;
    v->loud = 0;
    v->moving = 0;
}

static void drone_reverb_clear(drone_reverb_t *r)    /* no tail left in it */
{
    uint32_t n;
    for (n = 0; n < DRONE_REVERB_FRAMES; n++)
        r->line[n] = 0;
    for (n = 0; n < DRONE_REVERB_LINES; n++)
        r->at[n] = 0, r->dark[n] = 0;
}

/* the place of a chain's first FX block, or -1 */
static int32_t drone_fx_place(const drone_block_controls_t *blocks)
{
    int32_t s;
    for (s = 0; s < (int32_t)DRONE_SLOTS; s++)
        if (blocks[s].type == DRONE_BLOCK_FX)
            return s;
    return -1;
}

/* the voice whose chain has the reverb: the first patch with an FX block, or -1 when the reverb is the shared one */
static int32_t drone_reverb_holder(const drone_controls_t *c)
{
    int32_t n;
    for (n = 0; n < (int32_t)DRONE_VOICES; n++)
        if (drone_fx_place(c->voice[n].block) >= 0)
            return n;
    return -1;
}

static void drone_init(drone_t *d)
{
    uint32_t n, l;
    for (n = 0; n < DRONE_VOICES; n++) {
        for (l = 0; l < DRONE_SLOTS; l++)
            d->voice[n].block[l].phase = 0, d->voice[n].block[l].type = DRONE_BLOCK_NONE;
        d->voice[n].level_q16 = 0;
        for (l = 0; l < DRONE_LFOS; l++)
            d->voice[n].lfo_phase[l] = 0, d->voice[n].lfo_was[l] = 0;
        drone_voice_rest(&d->voice[n]);
    }
    d->scope.wanted = d->scope.running = 0;
    d->scope.waited = 0;
    d->scope.before = 0;
    drone_reverb_clear(&d->reverb);
    d->reverb.moving = 0;
    for (n = 0; n < DRONE_DELAY_FRAMES; n++)
        d->delay.line[n] = 0;
    d->delay.at = 0;
    d->delay.dark = 0;
    d->delay.moving = 0;
    d->comp.loud_q16 = 0;
    d->comp.moving = 0;
}

static uint32_t drone_voices_sounding(const drone_t *d)    /* after the last render: on, or still fading out */
{
    uint32_t n, count = 0;
    for (n = 0; n < DRONE_VOICES; n++)
        count += d->voice[n].level_q16 != 0;
    return count;
}

/* a parameter with what the LFOs add, in 1/256 of its step, kept in its range */
static int32_t drone_moved_q8(const param_spec_t *spec, uint32_t value, int32_t moved_q8)
{
    int32_t now = (int32_t)value * 256 + moved_q8;
    return now < 0 ? 0 : now > spec->top * 256 ? spec->top * 256 : now;
}

/* a filter over a block of the wave: its cutoff and resonance in 1/256 of a step, on their way there */
static void drone_filter(drone_block_t *b, uint32_t filter, int32_t cutoff_q8, int32_t resonance_q8, int32_t *wave,
                         uint32_t frames, int at_once)
{
    if (filter == DRONE_LADDER || filter == DRONE_MS20) {
        /* a quarter of the ladder's way back, 0..1.05, or half of the MS-20's: both sing by themselves at the top */
        drone_glide_to(&b->cutoff, drone_between(drone_pole_q14, cutoff_q8), frames, at_once);
        drone_glide_to(&b->resonance, (resonance_q8 * 135) >> 8, frames, at_once);
    } else {
        /* the damping: 1.41 (flat) down to 0.02 (the Polivoks: it sings) or 0.13 (the Wasp: a peak only) */
        drone_glide_to(&b->cutoff, drone_between(drone_cutoff_q14, cutoff_q8), frames, at_once);
        drone_glide_to(&b->resonance, 23170 - ((resonance_q8 * (filter == DRONE_POLIVOKS ? 180 : 166)) >> 8), frames, at_once);
    }
    if (filter == DRONE_LADDER)
        drone_ladder(b, wave, frames);
    else if (filter == DRONE_MS20)
        drone_ms20(b, wave, frames);
    else if (filter == DRONE_POLIVOKS)
        drone_polivoks(b, wave, frames);
    else
        drone_wasp(b, wave, frames);
}

/* what the LFOs (each -32767 .. 32767 now) add to a value through their cables, in 1/256 of the value's step */
static int32_t drone_cabled_q8(const int32_t *lfo_now, const int8_t *amount, int32_t reach)
{
    int32_t moved = 0;
    uint32_t l;
    for (l = 0; l < DRONE_LFOS; l++)
        if (amount[l])
            moved += ((lfo_now[l] * drone_limited(amount[l], DRONE_AMOUNT_TOP) / DRONE_AMOUNT_TOP) * reach) >> 7;
    return moved;
}

/* one frame for the scope's picture, when one is wanted. It waits for the sound to come up through zero (not
 * longer than a tenth of a second), then takes a point every few frames until the picture is full */
static inline void drone_scope_point(drone_scope_t *scope, int32_t in, int32_t back)
{
    if (!scope->wanted)
        return;
    if (!scope->running) {
        int up = scope->before < 0 && in >= 0;
        scope->before = in;
        if (!up && ++scope->waited < 4096u)
            return;
        scope->running = 1;
        scope->at = 0;
        scope->skip = 0;
    }
    if (scope->skip) {
        scope->skip--;
        return;
    }
    scope->skip = DRONE_SCOPE_EVERY - 1u;
    scope->in[scope->at] = (int16_t)drone_limited(in, 32767);
    scope->back[scope->at] = (int16_t)drone_limited(back, 32767);
    if (++scope->at == DRONE_SCOPE_POINTS) {
        scope->running = 0;
        scope->waited = 0;
        scope->wanted = 0;
    }
}

/* the main loop asks for a picture of that voice: of its sound after its chain (`out`), or of the two sounds where
 * its feedback cable goes back in. A picture asked before that has not come (its voice is silent) is given up */
static void drone_scope_ask(drone_scope_t *scope, uint32_t voice, int out)
{
    scope->wanted = 0;
    scope->running = 0;
    scope->waited = 0;
    scope->voice = (uint8_t)voice;
    scope->out = out != 0;
    scope->wanted = 1;
}

static int drone_fb_on(const drone_voice_controls_t *c)     /* the feedback cable has a level, or an LFO that gives it one */
{
    uint32_t l, on = c->fb[DRONE_FB_LEVEL];
    for (l = 0; l < DRONE_LFOS; l++)
        on |= (uint32_t)(c->fb_amount[l] != 0);
    return on != 0;
}

/* a patch: each block in its turn over a piece of sound, which starts silent. A block that is new in its place
 * starts from rest. With the feedback on, the piece is not longer than the cable's time: before the cable's second
 * block it gets what was after the first one that time ago. `scope`: 0, or it gets the two sounds that meet there.
 * `reverb`: 0, or the reverb for the chain's first FX block, with `wet` for its sound, left and right.
 * `link_q8`: what the voice before this one adds to every filter's cutoff, in 1/256 of a step.
 * With six oscillators and the feedback the sum is 8 full samples: every product here has room for that */
static void drone_chain(drone_voice_t *v, const drone_voice_controls_t *controls, const int32_t *lfo_now, int32_t *wave, uint32_t frames,
                        drone_scope_t *scope, drone_reverb_t *reverb, int32_t *wet, int32_t link_q8)
{
    const drone_block_controls_t *blocks = controls->block;
    uint32_t s, i, fb_on = (uint32_t)drone_fb_on(controls);
    uint32_t fb_from = controls->fb[DRONE_FB_FROM] < DRONE_SLOTS ? controls->fb[DRONE_FB_FROM] : DRONE_SLOTS - 1u;
    uint32_t fb_to = controls->fb[DRONE_FB_TO] < DRONE_SLOTS ? controls->fb[DRONE_FB_TO] : DRONE_SLOTS - 1u;
    uint32_t fb_from_at = v->fb_at + DRONE_FB_RING - DRONE_FB_FRAMES(controls->fb[DRONE_FB_TIME]);
    if (fb_on)
        drone_glide_to(&v->fb_level, drone_moved_q8(&drone_fb_spec[DRONE_FB_LEVEL], controls->fb[DRONE_FB_LEVEL],
                                                    drone_cabled_q8(lfo_now, controls->fb_amount, 127)), frames, !v->moving);
    for (s = 0; s < DRONE_SLOTS; s++) {
        drone_block_t *b = &v->block[s];
        const uint8_t *param = blocks[s].param;
        const param_spec_t *spec;
        uint32_t type = blocks[s].type < DRONE_NBLOCK ? blocks[s].type : DRONE_BLOCK_NONE;
        uint32_t kind = blocks[s].kind <= drone_block_kind_spec[type].top ? blocks[s].kind : 0u;
        int32_t value_q8[DRONE_BLOCK_NPARAM];
        uint32_t p;
        int at_once;
        if ((fb_on || scope) && s == fb_to)
            for (i = 0; i < frames; i++) {      /* 127 * 256 of 16384: twice */
                int32_t back = fb_on ? (v->fb[(fb_from_at + i) % DRONE_FB_RING] * DRONE_GLIDE(v->fb_level)) >> 14 : 0;
                if (scope)
                    drone_scope_point(scope, wave[i], back);
                wave[i] += back;
            }
        if (b->type != type || (type == DRONE_BLOCK_VCF && b->kind != kind))
            drone_block_rest(b);
        b->type = (uint8_t)type;
        b->kind = (uint8_t)kind;
        at_once = !b->moving;
        spec = drone_block_spec[type];
        for (p = 0; p < DRONE_BLOCK_NPARAM; p++)        /* each value where its knob and its cables put it */
            value_q8[p] = drone_moved_q8(&spec[p], param[p], drone_cabled_q8(lfo_now, blocks[s].amount[p], drone_block_reach[type][p]));
        if (type == DRONE_BLOCK_VCO) {
            uint32_t step = drone_step_q8(value_q8[DRONE_VCO_PITCH] + ((int32_t)(param[DRONE_VCO_FINE] & 127) - 64) * 2);
            if (at_once)
                b->phase_step = step;
            drone_oscillator(b, kind, step, wave, frames);
        } else if (type == DRONE_BLOCK_VCF) {
            drone_filter(b, kind, drone_moved_q8(&spec[DRONE_VCF_CUTOFF], 0, value_q8[DRONE_VCF_CUTOFF] + link_q8),
                         value_q8[DRONE_VCF_RESONANCE], wave, frames, at_once);
            for (i = 0; i < frames; i++)        /* back to a full sample, softly: a resonance above that is bent, not cut */
                wave[i] = drone_soft(wave[i] * (256 / DRONE_DRIVE));
        } else if (type == DRONE_BLOCK_VCA) {
            drone_glide_to(&b->level, value_q8[DRONE_VCA_LEVEL], frames, at_once);
            for (i = 0; i < frames; i++)        /* 127 * 256 is nearly all; 12 bits of it, so that 6 full samples fit */
                wave[i] = (wave[i] * (DRONE_GLIDE(b->level) >> 3)) >> 12;
        } else if (type == DRONE_BLOCK_FX && reverb) {
            const uint8_t of_reverb[DRONE_REVERB_NPARAM] = { [DRONE_REVERB_DECAY] = param[DRONE_FX_DECAY] & 127u,
                                                             [DRONE_REVERB_TONE] = param[DRONE_FX_TONE] & 127u };
            drone_reverb(reverb, of_reverb, wave, wet, frames);
            reverb = 0;                         /* a second FX block has none */
            drone_glide_to(&b->level, value_q8[DRONE_FX_MIX], frames, at_once);
            for (i = 0; i < frames; i++) {      /* the mix in 128 parts: 127 is all of the reverb, its left and right together */
                int32_t mix = DRONE_GLIDE(b->level) >> 8;
                mix += mix >> 6;
                wave[i] = (wave[i] * (128 - mix) + ((wet[2u * i] + wet[2u * i + 1u]) >> 1) * mix) >> 7;
            }
        }
        b->moving = type != DRONE_BLOCK_NONE;
        if (fb_on && s == fb_from)
            for (i = 0; i < frames; i++)
                v->fb[(v->fb_at + i) % DRONE_FB_RING] = (int16_t)drone_soft(wave[i]);
    }
    v->fb_at = (uint16_t)((v->fb_at + frames) % DRONE_FB_RING);
}

/* the end of every voice: `wave` through the fade; from there into the mix by the level, at the side its pan says,
 * and into the delay and the reverb by its two sends, from before the level or after it (all in 1/256 of a step). The wave's filters have bent it already: here it is only cut off at a full
 * sample, so a patch with oscillators that add up to more wants a level or a filter after them */
static void drone_voice_out(drone_t *d, drone_voice_t *v, const drone_voice_controls_t *controls, const int32_t *wave,
                            int32_t level_q8, int32_t send_q8, int32_t send_reverb_q8, uint32_t frames)
{
    int32_t pan = drone_limited(controls->pan, 64);
    uint32_t slow = 127u - (controls->param[DRONE_FADE] & 127u), i;
    uint32_t fade_step = 8000u + slow * slow * slow * 5u;                   /* 6 s .. 5 ms for the whole fade */
    int32_t peak = 0;
    drone_glide_to(&v->level, controls->level_log ? drone_between(drone_fader_q8, level_q8) : level_q8, frames, !v->moving);
    drone_glide_to(&v->send, send_q8, frames, !v->moving);
    drone_glide_to(&v->send_reverb, send_reverb_q8, frames, !v->moving);
    /* in the centre both sides have all of it; to one side, the other side goes down to nothing */
    drone_glide_to(&v->left, pan <= 0 ? 32768 : (64 - pan) * 512, frames, !v->moving);
    drone_glide_to(&v->right, pan >= 0 ? 32768 : (64 + pan) * 512, frames, !v->moving);
    for (i = 0; i < frames; i++) {
        int32_t sample, heard, sent;
        if (controls->on)
            v->level_q16 = DRONE_LEVEL_TOP - v->level_q16 > fade_step ? v->level_q16 + fade_step : DRONE_LEVEL_TOP;
        else
            v->level_q16 = v->level_q16 > fade_step ? v->level_q16 - fade_step : 0;
        sample = (drone_limited(wave[i], 32767) * (int32_t)(v->level_q16 >> 16)) >> 15;
        peak = sample > peak ? sample : -sample > peak ? -sample : peak;
        v->sent[i] = (int16_t)sample;
        heard = (sample * DRONE_GLIDE(v->level)) >> 15;             /* 127 * 256 is nearly all */
        d->mix[2u * i] += (heard * DRONE_GLIDE(v->left)) >> 15;
        d->mix[2u * i + 1u] += (heard * DRONE_GLIDE(v->right)) >> 15;
        sent = controls->post ? heard : sample;
        d->send[i] += (sent * DRONE_GLIDE(v->send)) >> 15;
        d->send_reverb[i] += (sent * DRONE_GLIDE(v->send_reverb)) >> 15;
    }
    v->loud = peak > v->loud ? peak : v->loud - ((v->loud - peak) >> 3);       /* down by an eighth of the way each call */
    v->moving = 1;
    if (!v->level_q16)                          /* faded out */
        drone_voice_rest(v);
}

static void drone_render(drone_t *d, const drone_controls_t *c, int32_t *out, uint32_t frames)
{
    uint32_t i, n, l;
    int32_t holder = drone_reverb_holder(c);
    int32_t master = c->master > DRONE_MASTER_MAX ? (int32_t)DRONE_MASTER_MAX : (int32_t)c->master;
    int32_t loud_before[DRONE_VOICES];          /* each voice at the last call: every link is that much late, in both ways round the ring */

    for (i = 0; i < frames; i++)
        d->mix[2u * i] = d->mix[2u * i + 1u] = d->send[i] = d->send_reverb[i] = 0;
    for (n = 0; n < DRONE_VOICES; n++)
        loud_before[n] = d->voice[n].loud;

    for (n = 0; n < DRONE_VOICES; n++) {      /* its LFOs move one time a block; the glides make that smooth */
        drone_voice_t *v = &d->voice[n];
        const drone_voice_controls_t *controls = &c->voice[n];
        int32_t lfo_now[DRONE_LFOS], *wave = d->one_voice;
        uint32_t time = DRONE_FB_FRAMES(controls->fb[DRONE_FB_TIME]), at;
        uint32_t piece = !drone_fb_on(controls) ? frames : time < DRONE_FB_PIECE ? time : DRONE_FB_PIECE;
        drone_scope_t *scope = d->scope.voice == n ? &d->scope : 0;
        /* the link from the voice before: FOLLOW is its loudness at the last call, 0 .. 32767 by link 0 .. 127, to
         * 0 .. DRONE_LINK_REACH steps (the products stay in 32 bits). THROUGH is its sound: this call's for the voices
         * after the first one, the last call's for the first one, which closes the ring */
        uint32_t before = n ? n - 1u : DRONE_VOICES - 1u;
        int32_t link = controls->link & 127, through = controls->link_kind == DRONE_LINK_THROUGH;
        int32_t link_q8 = through ? 0 : (((loud_before[before] * link) >> 7) * (DRONE_LINK_REACH * 256)) >> 15;
        if (!controls->on && !v->level_q16)
            continue;                           /* silent: it costs nothing */
        for (i = 0; i < frames; i++)            /* 127 of 128: nearly all of it */
            wave[i] = through ? (d->voice[before].sent[i % DRONE_BLOCK_FRAMES] * link) >> 7 : 0;
        for (l = 0; l < DRONE_LFOS; l++) {      /* each LFO's speed and depth, with what the LFOs' cables add: they carry the block before */
            const uint8_t *lfo = controls->lfo[l];
            int32_t speed_q8 = drone_moved_q8(&drone_lfo_spec[DRONE_LFO_SPEED], lfo[DRONE_LFO_SPEED],
                                              drone_cabled_q8(v->lfo_was, controls->lfo_amount[l][DRONE_LFO_TO_SPEED],
                                                              drone_lfo_cable_reach[DRONE_LFO_TO_SPEED]));
            int32_t depth_q8 = drone_moved_q8(&drone_lfo_spec[DRONE_LFO_DEPTH], lfo[DRONE_LFO_DEPTH],
                                              drone_cabled_q8(v->lfo_was, controls->lfo_amount[l][DRONE_LFO_TO_DEPTH],
                                                              drone_lfo_cable_reach[DRONE_LFO_TO_DEPTH]));
            lfo_now[l] = drone_lfo_wave(lfo[DRONE_LFO_WAVE], v->lfo_phase[l]);
            if (depth_q8 < 127 * 256)
                lfo_now[l] = (lfo_now[l] * (depth_q8 >> 1)) >> 14;
            v->lfo_phase[l] += (drone_step_q8(speed_q8) >> 9) * frames;
        }
        for (l = 0; l < DRONE_LFOS; l++)
            v->lfo_was[l] = lfo_now[l];
        for (at = 0; at < frames; at += piece)
            drone_chain(v, controls, lfo_now, wave + at, frames - at < piece ? frames - at : piece, scope && !scope->out ? scope : 0,
                        (int32_t)n == holder ? &d->reverb : 0, d->wet, link_q8);
        for (i = 0; scope && scope->out && i < frames; i++)
            drone_scope_point(scope, wave[i], 0);
        drone_voice_out(d, v, controls, wave,
                        drone_moved_q8(&drone_param_spec[DRONE_LEVEL], controls->param[DRONE_LEVEL],
                                       drone_cabled_q8(lfo_now, controls->out_amount[DRONE_OUT_LEVEL], DRONE_OUT_REACH)),
                        drone_moved_q8(&drone_send_spec, controls->send,
                                       drone_cabled_q8(lfo_now, controls->out_amount[DRONE_OUT_SEND], DRONE_OUT_REACH)),
                        drone_moved_q8(&drone_send_spec, controls->send_reverb,     /* an LFO's cable to the send moves the two */
                                       drone_cabled_q8(lfo_now, controls->out_amount[DRONE_OUT_SEND], DRONE_OUT_REACH)), frames);
        if ((int32_t)n == holder && !v->level_q16)      /* the voice with the reverb is silent now: its next start has no old tail */
            drone_reverb_clear(&d->reverb);
    }

    drone_delay(&d->delay, c->delay, d->send, d->echo, frames);
    for (i = 0; i < frames; i++)                /* the delay first, the reverb second: the sends and their echoes go in */
        d->send_reverb[i] += d->echo[i];
    if (holder < 0)
        drone_reverb(&d->reverb, c->reverb, d->send_reverb, d->wet, frames);
    else                                        /* the reverb is in a patch's chain: the sends go into the delay only */
        for (i = 0; i < 2u * frames; i++)
            d->wet[i] = 0;
    drone_glide_to(&d->reverb.back, (int32_t)c->reverb[DRONE_REVERB_RETURN] * 256, frames, !d->reverb.moving);
    d->reverb.moving = 1;
    for (i = 0; i < frames; i++) {
        /* the voices together go above a full sample: bent, not cut. 16 bits times 0..256: the 24-bit range */
        int32_t back = DRONE_GLIDE(d->reverb.back);
        out[2u * i] = drone_soft(d->mix[2u * i]) + d->echo[i] + ((d->wet[2u * i] * back) >> 15);
        out[2u * i + 1u] = drone_soft(d->mix[2u * i + 1u]) + d->echo[i] + ((d->wet[2u * i + 1u] * back) >> 15);
    }
    drone_compress(&d->comp, c->comp, out, frames, master);
}
