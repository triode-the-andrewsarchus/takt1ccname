/* takt1ccname: Custom MIDI CC names per track for Digitakt I
 *
 * On MIDI tracks (TRK 9 to TRK 16), the parameter pages (FLTR CC VALUE and AMP CC SELECT)
 * normally show generic names "VAL1".."VAL8" and "SEL1".."SEL8".
 * This mod allows assigning custom 4-character names to each MIDI CC knob per track:
 *   - Each knob independently selects from a build-time 64-word library (words.txt)
 *   - 6 bits per knob * 8 knobs = 48 bits = 6 bytes per track
 *   - Durable persistence across project saves/reloads and power cycles
 *   - Fast, authentic Elektron scrolling menu/picker (LEVEL knob or arrows to scroll, [YES] to assign)
 *   - Direct [FUNC] + Knob A..H shortcut from parameter pages to assign CC names instantly
 *   - Overview and settings page at SETTINGS > MIDI CC NAMES
 *   - Full compatibility with all digi1_mods under ColdFire RAM budget
 */

#ifdef OS154
#define DESC_TABLE_BASE 0x401aa09c
#define F_ACTIVE_TRACK  0x4197c6b4
#define F_INVALIDATE    0x400c9a3a
#define F_FILLRECT      0x400c1bce
#define F_TEXT          0x400c27a4
#define F_FONT5         0x40200ebc
#define F_UI_KIT        0x4199ec44
#else
#define DESC_TABLE_BASE 0x401a9d9c
#define F_ACTIVE_TRACK  0x4197b6b4
#define F_INVALIDATE    0x400c9812
#define F_FILLRECT      0x400c19a6
#define F_TEXT          0x400c257c
#define F_FONT5         0x40200b0c
#define F_UI_KIT        0x4199dc44
#endif

#define ACTIVE_TRACK (*(volatile int *)F_ACTIVE_TRACK)
#define INVALIDATE   ((void (*)(void *))F_INVALIDATE)
#define FILLRECT     ((void (*)(void *, int, int, int, int, int))F_FILLRECT)
#define TEXT         ((void (*)(void *, const void *, int, int, int, const char *, ...))F_TEXT)
#define FONT5        ((const void *)F_FONT5)
#define UI_KIT       (*(unsigned char *volatile *)F_UI_KIT)

#include "kitstore.h"
#include "words.h"

/* Access parameter descriptor fields in firmware RAM (52 bytes per entry) */
#define DESC_PTR(id, offset) (*(const char *volatile *)(DESC_TABLE_BASE + (id) * 52 + (offset)))
#define DESC_LONG(id)        DESC_PTR(id, 40)
#define DESC_SHORT(id)       DESC_PTR(id, 48)

#define NUM_MIDI_TRACKS   8
#define NUM_CCS_PER_TRACK 8

/* MIDI Track record in kit: starts at kit + 0x596, stride 0x70 bytes */
#define MIDI_TRK_BASE(kit, t) ((unsigned char *)(kit) + 0x596 + (t) * 0x70)
#define MIDI_TRK_DATA(kit, t) (MIDI_TRK_BASE(kit, t) + 0x66)

struct __attribute__((packed)) cc_slot {
    char name[5];           /* 4 characters + '\0' */
    unsigned char custom;   /* 1 if custom name active, 0 for default VAL# */
    unsigned char word_idx; /* 0..63 index into WORDS */
};

struct __attribute__((packed)) midi_track {
    struct cc_slot slots[NUM_CCS_PER_TRACK];
};

static struct midi_track tracks[NUM_MIDI_TRACKS];

/* Original stock pointers saved at startup */
static const char *stock_short[16];
static const char *stock_long[16];
static int stock_saved = 0;

/* UI State */
int ccname_open = 0;
static int cur_track = 0;     /* 0..7 for MIDI tracks 9..16 */
static int cur_slot = 0;      /* 0..7 for CC 1..8 */
static int edit_mode = 0;     /* 0 = overview mode, 1 = word picker menu */
static int picker_idx = 0;    /* 0..63: active word in scrolling menu */
static int held_knob = -1;    /* 0..7 for knob A..H held with FUNC, -1 = none */
static int held_ticks = 0;    /* tick count for press-and-hold detection */
static int from_shortcut = 0; /* 1 if opened directly via [FUNC]+knob */
static int clip_track = -1;   /* Clipboard (track index, -1 = empty) */

static struct kstore kst;

/* Starter word indices for fresh unconfigured kits */
static const unsigned char STARTER_WORDS[NUM_MIDI_TRACKS][8] = {
    {1, 2, 3, 4, 5, 6, 7, 13},     /* T09: CUTO, RESO, ENV , ATK , DEC , SUS , REL , LFOR */
    {1, 2, 3, 4, 5, 13, 14, 31},    /* T10: CUTO, RESO, ENV , ATK , DEC , LFOR, LFOD, SHAP */
    {1, 2, 3, 5, 15, 63, 63, 23},    /* T11: CUTO, RESO, ENV , DEC , WAVE, USER, USER, MIX  */
    {1, 2, 17, 18, 4, 5, 12, 26},    /* T12: CUTO, RESO, SUB , DIST, ATK , DEC , GLID, MOD  */
    {11, 5, 10, 63, 8, 9, 18, 48},   /* T13: PICH, DEC , TUNE, USER, VOL , PAN , DIST, CRSH */
    {19, 20, 21, 22, 23, 24, 25, 31},/* T14: TIME, FEED, RATE, DPTH, MIX , DRY , WET , SHAP */
    {26, 27, 8, 9, 28, 63, 29, 30},  /* T15: MOD , BREA, VOL , PAN , EXPR, USER, BRIG, REVB */
    {0, 0, 0, 0, 0, 0, 0, 0}         /* T16: stock default VAL1..VAL8 */
};

/* Memory copy helper */
void *memcpy(void *dst, const void *src, unsigned long n)
{
    char *d = (char *)dst;
    const char *s = (const char *)src;
    while (n--)
        *d++ = *s++;
    return dst;
}

/* String copy helper without stdlib */
static void str_copy(char *dst, const char *src, int maxlen)
{
    int i = 0;
    while (src[i] && i < maxlen - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

/* Pack 8 knobs (6 bits each) into 6 bytes */
static void pack_track(int t, unsigned char *out6)
{
    unsigned char w[8];
    int i;
    for (i = 0; i < 8; i++)
        w[i] = tracks[t].slots[i].word_idx & 0x3f;

    out6[0] = (w[0] & 0x3f) | ((w[1] & 0x03) << 6);
    out6[1] = ((w[1] >> 2) & 0x0f) | ((w[2] & 0x0f) << 4);
    out6[2] = ((w[2] >> 4) & 0x03) | ((w[3] & 0x3f) << 2);
    out6[3] = (w[4] & 0x3f) | ((w[5] & 0x03) << 6);
    out6[4] = ((w[5] >> 2) & 0x0f) | ((w[6] & 0x0f) << 4);
    out6[5] = ((w[6] >> 4) & 0x03) | ((w[7] & 0x3f) << 2);
}

/* Apply a word index to a slot */
static void apply_slot_word(int t, int s, int word_idx)
{
    if ((unsigned)t >= NUM_MIDI_TRACKS || (unsigned)s >= NUM_CCS_PER_TRACK)
        return;
    if (word_idx < 0) word_idx = 0;
    if (word_idx >= NUM_WORDS) word_idx = NUM_WORDS - 1;

    tracks[t].slots[s].word_idx = (unsigned char)word_idx;
    if (word_idx == 0) {
        tracks[t].slots[s].custom = 0;
        tracks[t].slots[s].name[0] = 'V';
        tracks[t].slots[s].name[1] = 'A';
        tracks[t].slots[s].name[2] = 'L';
        tracks[t].slots[s].name[3] = '1' + s;
        tracks[t].slots[s].name[4] = '\0';
    } else {
        tracks[t].slots[s].custom = 1;
        str_copy(tracks[t].slots[s].name, WORDS[word_idx], 5);
    }
}

/* Unpack 6 bytes into 8 knobs */
static void unpack_track(int t, const unsigned char *in6)
{
    unsigned char w[8];
    int i;

    w[0] = in6[0] & 0x3f;
    w[1] = ((in6[0] >> 6) & 0x03) | ((in6[1] & 0x0f) << 2);
    w[2] = ((in6[1] >> 4) & 0x0f) | ((in6[2] & 0x03) << 4);
    w[3] = (in6[2] >> 2) & 0x3f;
    w[4] = in6[3] & 0x3f;
    w[5] = ((in6[3] >> 6) & 0x03) | ((in6[4] & 0x0f) << 2);
    w[6] = ((in6[4] >> 4) & 0x0f) | ((in6[5] & 0x03) << 4);
    w[7] = (in6[5] >> 2) & 0x3f;

    for (i = 0; i < 8; i++)
        apply_slot_word(t, i, w[i]);
}

/* Synchronize track's packed 6 bytes into kit storage */
static void ccname_sync_kit_track(unsigned char *kit, int t)
{
    unsigned char packed[6];
    if (!kit || (unsigned)t >= NUM_MIDI_TRACKS)
        return;

    pack_track(t, packed);
    memcpy(MIDI_TRK_DATA(kit, t), packed, 6);

    /* Mirror fallback & trigger sound persistence */
    XB(kit, MIDI_TRK_XB[t]) = (unsigned char)(tracks[t].slots[0].word_idx + 1);
    ks_persist(kit, 4 + (t >> 1));
}

static void ccname_mark_dirty(int t)
{
    ccname_sync_kit_track(UI_KIT, t);
}

/* Active track descriptor updater */
static void ccname_update_descriptors(void)
{
    int track = ACTIVE_TRACK;
    int is_midi = (track >= 8 && track <= 15);
    int t = track - 8;
    int i;

    /* Save original stock pointers on first tick */
    if (!stock_saved) {
        for (i = 0; i < 16; i++) {
            stock_short[i] = DESC_SHORT(148 + i);
            stock_long[i] = DESC_LONG(148 + i);
        }
        stock_saved = 1;
        if (tracks[0].slots[0].name[0] == '\0') {
            for (t = 0; t < NUM_MIDI_TRACKS; t++) {
                for (i = 0; i < 8; i++)
                    apply_slot_word(t, i, STARTER_WORDS[t][i]);
            }
        }
    }

    for (i = 0; i < 8; i++) {
        const char *s = stock_short[i];
        const char *l = stock_long[i];
        const char *ss = stock_short[8 + i];
        const char *sl = stock_long[8 + i];
        if (is_midi && tracks[t].slots[i].custom) {
            s = ss = l = sl = tracks[t].slots[i].name;
        }
        DESC_SHORT(148 + i) = s;
        DESC_LONG(148 + i) = l;
        DESC_SHORT(156 + i) = ss;
        DESC_LONG(156 + i) = sl;
    }
}

/* Open word picker for a specific track and knob slot */
static void open_picker(int trk, int slot, int shortcut)
{
    ccname_open = 1;
    edit_mode = 1;
    cur_track = trk;
    cur_slot = slot;
    from_shortcut = shortcut;
    picker_idx = tracks[trk].slots[slot].word_idx;
}

/* ---- SETTINGS menu callbacks ---- */

void ccname_select(void **payload)
{
    ccname_open = 1;
    edit_mode = 0;
    from_shortcut = 0;
    cur_slot = 0;
    INVALIDATE((char *)*payload + 0x38);
}

void ccname_change(void **payload, int unused, int delta)
{
    (void)unused;
    (void)delta;
    INVALIDATE((char *)*payload + 0x38);
}

void ccname_rowdraw(void *a, void *b, void *bmp, int x, int y)
{
    (void)a;
    (void)b;
    (void)bmp;
    (void)x;
    (void)y;
}

/* ---- UI drawing & event handling ---- */

#define ROWY(i) (48 - (i) * 6)

void ccname_draw(void *bmp, void *ctrl)
{
    int i;
    struct midi_track *tr;
    (void)ctrl;

    /* Always ensure descriptor pointers match active track */
    ccname_update_descriptors();

    if (!ccname_open)
        return;

    tr = &tracks[cur_track];

    FILLRECT(bmp, 0, 0, 127, 63, 0);

    if (edit_mode == 1) {
        /* ==== SCROLLING WORD PICKER MENU ==== */
        int d;
        int thumb_y;

        /* Top Header */
        TEXT(bmp, FONT5, 2, 57, -1, "CC%d NAME [T%02d]", cur_slot + 1, cur_track + 9);
        TEXT(bmp, FONT5, 88, 57, -1, "[%02d/64]", picker_idx + 1);

        /* Separator line */
        FILLRECT(bmp, 0, 54, 127, 54, 1);

        /* 5 visible rows centered on picker_idx */
        for (d = -2; d <= 2; d++) {
            int idx = (picker_idx + d) % NUM_WORDS;
            int y;
            if (idx < 0)
                idx += NUM_WORDS;

            y = 23 - d * 10; /* d=-2: 43, d=-1: 33, d=0: 23, d=1: 13, d=2: 3 */

            if (d == 0) {
                if (idx == 0) {
                    TEXT(bmp, FONT5, 10, y, -1, "> %02d: ---- [DEFAULT]", idx + 1);
                } else {
                    TEXT(bmp, FONT5, 10, y, -1, "> %02d: %-4s", idx + 1, WORDS[idx]);
                }
                /* Invert row to create crisp black-on-yellow highlight bar */
                FILLRECT(bmp, 6, y - 1, 118, y + 5, -1);
            } else {
                /* Inactive rows: standard text */
                if (idx == 0) {
                    TEXT(bmp, FONT5, 14, y, -1, "  %02d: ----", idx + 1);
                } else {
                    TEXT(bmp, FONT5, 14, y, -1, "  %02d: %-4s", idx + 1, WORDS[idx]);
                }
            }
        }

        /* Right scrollbar indicator */
        FILLRECT(bmp, 123, 3, 124, 51, 1);
        thumb_y = 45 - (picker_idx * 42) / (NUM_WORDS - 1);
        if (thumb_y < 3) thumb_y = 3;
        if (thumb_y > 45) thumb_y = 45;
        FILLRECT(bmp, 122, thumb_y, 125, thumb_y + 6, 1);

    } else {
        /* ==== CC SLOTS OVERVIEW MODE ==== */
        /* Header */
        TEXT(bmp, FONT5, 1, 57, -1, "MIDI CC NAMES");
        TEXT(bmp, FONT5, 84, 57, -1, "TRK %02d", cur_track + 9);

        /* 8 CC slot rows */
        for (i = 0; i < 8; i++) {
            int y = ROWY(i);
            struct cc_slot *slot = &tr->slots[i];

            TEXT(bmp, FONT5, 2, y, -1, "%d:", i + 1);

            if (slot->custom) {
                TEXT(bmp, FONT5, 16, y, -1, "[%-4s]", slot->name);
            } else {
                TEXT(bmp, FONT5, 16, y, -1, "VAL%d", i + 1);
            }

            if (i == cur_slot) {
                /* Highlight cursor row */
                FILLRECT(bmp, 0, y - 1, 127, y + 5, -1);
            }
        }

        /* Footer */
        TEXT(bmp, FONT5, 1, 1, -1, "A:TRK B:SLOT C:WORD [YES]:PICK");
    }
}

void ccname_tick(void *ctrl)
{
    unsigned char *kit = UI_KIT;

    /* Kit / Project Storage synchronization */
    if (kit) {
        int r = ccname_kstore_tick(&kst, kit);
        if (r == KS_LOADED || r == KS_OTHER) {
            int t;
            for (t = 0; t < NUM_MIDI_TRACKS; t++) {
                unsigned char *src = MIDI_TRK_DATA(kit, t);
                int non_zero = 0, k;
                for (k = 0; k < 6; k++) {
                    if (src[k] != 0) {
                        non_zero = 1;
                        break;
                    }
                }
                if (non_zero) {
                    unpack_track(t, src);
                } else {
                    /* Kit has no setting: initialize with starter words and sync */
                    for (k = 0; k < 8; k++)
                        apply_slot_word(t, k, STARTER_WORDS[t][k]);
                    ccname_sync_kit_track(kit, t);
                }
            }
            ccname_kstore_tick(&kst, kit);
        }
    }

    ccname_update_descriptors();

    /* Press-and-hold [FUNC] + DATA ENTRY knob A-H (Digitakt II style shortcut) */
    if (held_knob >= 0) {
        held_ticks++;
        if (held_ticks >= 6) {
            int trk = ACTIVE_TRACK;
            if (trk >= 8 && trk <= 15) {
                open_picker(trk - 8, held_knob, 1);
            }
            held_knob = -1;
            held_ticks = 0;
        }
    }

    if (ccname_open)
        *((volatile char *)ctrl + 0x20) = 1;
}

#define KEYID(ev)    (*(int *)((char *)(ev) + 12))
#define KEYFLAGS(ev) (*(int *)((char *)(ev) + 16))
#define KEYDOWN(ev)  (KEYFLAGS(ev) & 1)
#define KEYFUNC(ev)  (KEYFLAGS(ev) & 2)
#define KEYUP(ev)    (KEYFLAGS(ev) & 0x10)
#define KEYLONG(ev)  (KEYFLAGS(ev) & 0x20)

int ccname_key(void *brain, void *ev)
{
    int id = KEYID(ev);
    int func = KEYFUNC(ev) || (id == 1 && !KEYUP(ev));
    (void)brain;

    ccname_update_descriptors();

    /* Never consume the FUNC key itself (id 1) so stock UI always receives press and release */
    if (id == 1)
        return 0;

    if (!ccname_open) {
        /* When on MIDI tracks (TRK 9..16), monitor [FUNC] + Knob A..H (ids 40..47) */
        int trk = ACTIVE_TRACK;
        if (trk >= 8 && trk <= 15 && func && id >= 40 && id <= 47) {
            int knob_idx = id - 40;
            if (KEYLONG(ev)) {
                /* Hardware panel driver native long-press (~0.5s) */
                open_picker(trk - 8, knob_idx, 1);
                held_knob = -1;
                held_ticks = 0;
                return 1;
            }
            if (KEYUP(ev)) {
                held_knob = -1;
                held_ticks = 0;
                return 1;
            }
            if (KEYDOWN(ev)) {
                held_knob = knob_idx;
                held_ticks = 0;
                return 1;
            }
            return 1;
        }
        return 0;
    }

    /* Modal consumes all non-KEYDOWN events */
    if (!KEYDOWN(ev))
        return 1;

    if (edit_mode == 1) {
        /* ==== WORD PICKER MENU NAVIGATION ==== */
        switch (id) {
        case 12: /* YES: confirm and apply word */
        case 32:
            apply_slot_word(cur_track, cur_slot, picker_idx);
            ccname_mark_dirty(cur_track);
            edit_mode = 0;
            if (from_shortcut) {
                ccname_open = 0;
                from_shortcut = 0;
            }
            break;
        case 13: /* NO: cancel */
        case 33:
            edit_mode = 0;
            if (from_shortcut) {
                ccname_open = 0;
                from_shortcut = 0;
            }
            break;
        case 14: /* UP: previous word */
        case 35:
            if (picker_idx > 0)
                picker_idx--;
            else
                picker_idx = NUM_WORDS - 1;
            break;
        case 15: /* DOWN: next word */
        case 36:
            if (picker_idx < NUM_WORDS - 1)
                picker_idx++;
            else
                picker_idx = 0;
            break;
        case 16: /* LEFT: page up (-8) */
        case 34:
            picker_idx -= 8;
            if (picker_idx < 0)
                picker_idx += NUM_WORDS;
            break;
        case 17: /* RIGHT: page down (+8) */
        case 37:
            picker_idx += 8;
            if (picker_idx >= NUM_WORDS)
                picker_idx -= NUM_WORDS;
            break;
        default:
            break;
        }
        return 1;
    }

    /* ==== OVERVIEW NAVIGATION MODE ==== */
    /* Transport keys (REC, PLAY, STOP) pass through when not holding FUNC */
    if (!func && (id == 9 || id == 10 || id == 11))
        return 0;

    /* Clipboard shortcuts */
    if (func) {
        if (id == 9 || id == 24) {   /* FUNC+REC: Copy current track CC names */
            clip_track = cur_track;
            return 1;
        }
        if (id == 11 || id == 26) {  /* FUNC+STOP: Paste CC names */
            if (clip_track >= 0 && clip_track != cur_track) {
                int s;
                for (s = 0; s < 8; s++)
                    tracks[cur_track].slots[s] = tracks[clip_track].slots[s];
                ccname_mark_dirty(cur_track);
            }
            return 1;
        }
        if (id == 10 || id == 25) {  /* FUNC+PLAY: Reset to default */
            int s;
            for (s = 0; s < 8; s++)
                apply_slot_word(cur_track, s, 0);
            ccname_mark_dirty(cur_track);
            return 1;
        }
    }

    switch (id) {
    case 13: /* NO: exit page */
    case 33:
        ccname_open = 0;
        break;
    case 14: /* UP: previous CC slot */
    case 35:
        if (cur_slot > 0)
            cur_slot--;
        else
            cur_slot = 7;
        break;
    case 15: /* DOWN: next CC slot */
    case 36:
        if (cur_slot < 7)
            cur_slot++;
        else
            cur_slot = 0;
        break;
    case 16: /* LEFT: previous MIDI track */
    case 34:
        if (cur_track > 0)
            cur_track--;
        else
            cur_track = NUM_MIDI_TRACKS - 1;
        break;
    case 17: /* RIGHT: next MIDI track */
    case 37:
        if (cur_track < NUM_MIDI_TRACKS - 1)
            cur_track++;
        else
            cur_track = 0;
        break;
    case 12: /* YES: open Word Picker menu */
    case 32:
        open_picker(cur_track, cur_slot, 0);
        break;
    default:
        break;
    }
    return 1;
}

int ccname_enc(void *brain, void *ev)
{
    int id = *(int *)((char *)ev + 12);
    int delta = *(int *)((char *)ev + 16);
    int s;
    (void)brain;

    if (!ccname_open)
        return 0;
    if (!delta)
        return 1;

    s = delta > 0 ? 1 : -1;

    if (edit_mode == 1) {
        /* LEVEL knob (id 9) or Knobs E/F cycles through the 64 words */
        if (id == 9 || id == 5 || id == 6) {
            picker_idx = (picker_idx + delta) % NUM_WORDS;
            if (picker_idx < 0)
                picker_idx += NUM_WORDS;
            return 1;
        }
        return 1;
    }

    /* In Overview Mode */
    if (id == 9) { /* LEVEL knob: navigate CC slots */
        if (s > 0) {
            if (cur_slot < 7) cur_slot++;
            else cur_slot = 0;
        } else {
            if (cur_slot > 0) cur_slot--;
            else cur_slot = 7;
        }
        return 1;
    }

    switch (id) {
    case 1: { /* Knob A: select MIDI Track (0..7) */
        cur_track += s;
        if (cur_track < 0) cur_track = NUM_MIDI_TRACKS - 1;
        if (cur_track >= NUM_MIDI_TRACKS) cur_track = 0;
        break;
    }
    case 2: { /* Knob B: select CC Slot (0..7) */
        cur_slot += s;
        if (cur_slot < 0) cur_slot = 7;
        if (cur_slot > 7) cur_slot = 0;
        break;
    }
    case 3: { /* Knob C: cycle word directly (+1 / -1) */
        int cur_w = tracks[cur_track].slots[cur_slot].word_idx;
        int next_w = (cur_w + s) % NUM_WORDS;
        if (next_w < 0) next_w += NUM_WORDS;
        apply_slot_word(cur_track, cur_slot, next_w);
        ccname_mark_dirty(cur_track);
        break;
    }
    case 4: { /* Knob D: page jump word (+/- 8) */
        int cur_w = tracks[cur_track].slots[cur_slot].word_idx;
        int next_w = (cur_w + s * 8) % NUM_WORDS;
        if (next_w < 0) next_w += NUM_WORDS;
        apply_slot_word(cur_track, cur_slot, next_w);
        ccname_mark_dirty(cur_track);
        break;
    }
    default:
        break;
    }

    return 1;
}
