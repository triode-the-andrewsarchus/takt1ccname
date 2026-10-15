/* takt1ccname: Per-pattern kit persistence and non-volatile mirror
 * (Based on kitstore.h architecture for audio tracks 5..8 / sounds 4..7)
 */
#ifndef KITSTORE_H
#define KITSTORE_H

#define XB_OFF(n)  (0x20 + (n) / 6 * 0xa2 + ((n) % 6 < 4 ? (n) % 6 : 0x10 + (n) % 6))
#define XB(kit, n) (*(volatile unsigned char *)(void *)((char *)(kit) + XB_OFF(n)))

/* RAM-only marker slots */
#define KS_SLOT_CCNAME 49
#define KS_MARK        0x4b53
#define KS_MARKW(kit, t, slot) \
    (*(volatile unsigned short *)(void *)((char *)(kit) + 0x20 + (t) * 0xa2 + 0x14 + 2 * (slot)))

/* the non-volatile mirror of the kits */
#ifdef OS154
#define KS_KIT0        0x4191f018u
#define KS_MIRROR0     0x4095941cu
#else
#define KS_KIT0        0x4191e018u
#define KS_MIRROR0     0x4095841cu
#endif
#define KS_KIT_STEP    0x91eu
#define KS_MIRROR_STEP 0xa00u
#define KS_PATTERNS    128u
#define KS_SERIALIZE   ((int (*)(void *, void *, int))0x4007a5a0)

static void __attribute__((noinline)) ks_persist(unsigned char *kit, int t)
{
    unsigned int off = (unsigned int)kit - KS_KIT0, p = off / KS_KIT_STEP;
    volatile unsigned int *m;
    if (off % KS_KIT_STEP || p >= KS_PATTERNS || (unsigned)t > 7)
        return;
    m = (volatile unsigned int *)(KS_MIRROR0 + p * KS_MIRROR_STEP + (unsigned)t * 0xa0u);
    if (m[0] != 0xbeefbaceu || m[1] != 3)
        return;
    KS_SERIALIZE((void *)m, kit + 0x20 + t * 0xa2, 0);
}

static const unsigned char MIDI_TRK_XB[8] = {
    28, 29, 34, 35, 40, 41, 46, 47
};

struct kstore {
    unsigned char *kit;
    unsigned char copy[8];
};

enum { KS_SAME, KS_LOADED, KS_OTHER };

static int ccname_kstore_tick(struct kstore *s, unsigned char *kit)
{
    int t, lost = 0, r = KS_SAME, dirty = 0;
    if (!kit) {
        s->kit = 0;
        return KS_SAME;
    }
    for (t = 4; t < 8; t++) {
        if (KS_MARKW(kit, t, KS_SLOT_CCNAME) != KS_MARK)
            lost |= 1 << t;
    }
    if (kit != s->kit) {
        r = KS_OTHER;
    } else if ((lost & 0xf0) == 0xf0) {
        r = KS_LOADED;
    } else {
        if (lost) {
            for (t = 0; t < 8; t++) {
                int snd = 4 + (t >> 1);
                if ((lost >> snd) & 1) {
                    XB(kit, MIDI_TRK_XB[t]) = s->copy[t];
                    dirty |= 1 << snd;
                }
            }
        }
        for (t = 0; t < 8; t++) {
            int snd = 4 + (t >> 1);
            if (XB(kit, MIDI_TRK_XB[t]) != s->copy[t])
                dirty |= 1 << snd;
        }
    }
    for (t = 4; t < 8; t++) {
        if ((lost >> t) & 1)
            KS_MARKW(kit, t, KS_SLOT_CCNAME) = KS_MARK;
    }
    for (t = 4; t < 8; t++) {
        if ((dirty >> t) & 1)
            ks_persist(kit, t);
    }
    s->kit = kit;
    for (t = 0; t < 8; t++)
        s->copy[t] = XB(kit, MIDI_TRK_XB[t]);
    return r;
}

#endif
