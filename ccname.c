/* takt1ccname: Custom MIDI CC names per track for Digitakt I
 *
 * On MIDI tracks (TRK 9 to TRK 16), the parameter pages (FLTR CC VALUE and AMP CC SELECT)
 * normally show generic names "VAL1".."VAL8" and "SEL1".."SEL8".
 * This mod allows assigning custom names to each MIDI CC per track, with:
 *   - Per-track custom short name (up to 4 chars) displayed above each knob on FLTR/AMP pages
 *   - Per-track custom long name (up to 15 chars) displayed in popups and lists
 *   - Quick presets for popular synths (Standard Synth, Minilogue, MicroFreak, Bass Synth, Drum, FX, General MIDI)
 *   - Library of standard synth parameter names
 *   - In-device character-by-character editing
 *   - Settings menu page at SETTINGS > MIDI CC NAMES
 *   - Full compatibility with digi1_mods (Digi Poly, Digi Mono, Digi Matrix, Digi EQ, Digi Utilities)
 */

#ifdef OS154
#define DESC_TABLE_BASE 0x401aa09c
#define F_ACTIVE_TRACK  0x4197c6b4
#define F_INVALIDATE    0x400c9a3a
#define F_FILLRECT      0x400c1bce
#define F_TEXT          0x400c27a4
#define F_FONT5         0x40200ebc
#define F_UI_KIT        0x4199ec44
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

/* Access parameter descriptor fields in firmware RAM (52 bytes per entry) */
#define DESC_PTR(id, offset) (*(const char *volatile *)(DESC_TABLE_BASE + (id) * 52 + (offset)))
#define DESC_LONG(id)        DESC_PTR(id, 40)
#define DESC_SHORT(id)       DESC_PTR(id, 48)

#define NUM_MIDI_TRACKS 8
#define NUM_CCS_PER_TRACK 8

struct __attribute__((packed)) cc_slot {
    char name[5];         /* 4 characters + '\0' */
    unsigned char custom; /* 1 if custom name is active, 0 for default */
};

struct __attribute__((packed)) midi_track {
    short preset_idx;
    struct cc_slot slots[NUM_CCS_PER_TRACK];
};

static struct midi_track tracks[NUM_MIDI_TRACKS];

/* Original stock pointers saved at startup */
static const char *stock_short[16];
static const char *stock_long[16];
static int stock_saved = 0;

/* UI State */
int ccname_open = 0;
static int cur_track = 0;   /* 0..7 for MIDI tracks 9..16 */
static int cur_slot = 0;    /* 0..7 for CC 1..8 */
static int edit_mode = 0;   /* 0 = select slot/preset, 1 = naming mode */
static int char_pos = 0;    /* 0..3 cursor position */
static int grid_active = 0; /* 1 when holding [FUNC] to show pop-up grid */
static int grid_row = 0;    /* 0..3 grid row */
static int grid_col = 0;    /* 0..12 grid column */
static int func_action = 0; /* 1 if FUNC+NO (erase) or FUNC+YES (space) occurred */
static int held_knob = -1;  /* 0..7 for knob A..H held with FUNC, -1 = none */
static int held_ticks = 0;  /* tick count for press-and-hold detection */
static int from_shortcut = 0; /* 1 if opened directly via [FUNC]+knob */

/* Clipboard (track index, -1 = empty) */
static int clip_track = -1;

static struct kstore kst;

/* Presets */
struct cc_preset {
    const char *name;
    char s[8][5];
};

static const struct cc_preset PRESETS[] = {
    {
        "DEFAULT",
        {}
    },
    {
        "SYNTH",
        {"CUTO", "RESO", "ENV ", "ATK ", "DEC ", "SUS ", "REL ", "LFOR"}
    },
    {
        "MINILOGUE",
        {"CUTO", "RESO", "EG  ", "ATK ", "DEC ", "LFOR", "LFOD", "SHAP"}
    },
    {
        "MICROFREAK",
        {"CUTO", "RESO", "CYCL", "ENV ", "DECA", "WAVE", "TIMB", "MORP"}
    },
    {
        "BASS SYNTH",
        {"CUTO", "RESO", "SUB ", "DIST", "ATK ", "DEC ", "ACC ", "GLID"}
    },
    {
        "DRUM/SAMP",
        {"PICH", "DECA", "TUNE", "SNAP", "TONE", "SWNG", "VOL ", "PAN "}
    },
    {
        "MOD/FX",
        {"TIME", "FEED", "RATE", "DPTH", "MIX ", "DRY ", "WET ", "SHAP"}
    },
    {
        "GEN. MIDI",
        {"MOD ", "BREA", "VOL ", "PAN ", "EXPR", "SUST", "BRIG", "REVB"}
    }
};
#define NUM_PRESETS (int)(sizeof(PRESETS) / sizeof(PRESETS[0]))

/* Library of standard parameter short-names for single-slot assignment */
static const char CC_LIBRARY[][5] = {
    "----", "CUTO", "RESO", "ENV ", "ATK ", "DEC ", "SUS ", "REL ",
    "VOL ", "PAN ", "TUNE", "PICH", "GLID", "LFOR", "LFOD", "WAVE",
    "PWM ", "SUB ", "DIST", "TIME", "FEED", "RATE", "DPTH", "MIX ",
    "DRY ", "WET ", "MOD ", "BREA", "EXPR", "BRIG", "REVB", "SHAP"
};
#define NUM_LIB_ITEMS (int)(sizeof(CC_LIBRARY) / sizeof(CC_LIBRARY[0]))

static const char GRID_CHARS[53] =
    "ABCDEFGHIJKLM"
    "NOPQRSTUVWXYZ"
    "0123456789-_+"
    "!?/#%*.-:=() ";

static void grid_locate(char c)
{
    int i;
    if (c >= 'a' && c <= 'z') c -= 32;
    for (i = 0; i < 52; i++) {
        if (GRID_CHARS[i] == c) {
            grid_row = i / 13;
            grid_col = i % 13;
            return;
        }
    }
    grid_row = 0;
    grid_col = 0;
}

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

static void apply_preset(int track_idx, int preset_idx)
{
    int s;
    if (track_idx < 0 || track_idx >= NUM_MIDI_TRACKS)
        return;
    if (preset_idx < 0 || preset_idx >= NUM_PRESETS)
        return;

    tracks[track_idx].preset_idx = preset_idx;
    if (preset_idx == 0) {
        for (s = 0; s < 8; s++) {
            tracks[track_idx].slots[s].name[0] = 'V';
            tracks[track_idx].slots[s].name[1] = 'A';
            tracks[track_idx].slots[s].name[2] = 'L';
            tracks[track_idx].slots[s].name[3] = '1' + s;
            tracks[track_idx].slots[s].name[4] = '\0';
            tracks[track_idx].slots[s].custom = 0;
        }
        return;
    }

    for (s = 0; s < 8; s++) {
        str_copy(tracks[track_idx].slots[s].name, PRESETS[preset_idx].s[s], 5);
        tracks[track_idx].slots[s].custom = 1;
    }
}

static void ccname_sync_kit_track(unsigned char *kit, int t)
{
    int p;
    if (!kit || (unsigned)t >= NUM_MIDI_TRACKS)
        return;
    p = tracks[t].preset_idx;
    XB(kit, MIDI_TRK_XB[t]) = (unsigned char)(p >= 0 ? (p + 1) : 0);
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
            for (i = 0; i < NUM_MIDI_TRACKS; i++)
                apply_preset(i, i == 7 ? 0 : i + 1);
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

static void draw_box(void *bmp, int x0, int y0, int x1, int y1)
{
    FILLRECT(bmp, x0, y0, x1, y1, 0);
    FILLRECT(bmp, x0, y0, x1, y0, 1);
    FILLRECT(bmp, x0, y1, x1, y1, 1);
    FILLRECT(bmp, x0, y0, x0, y1, 1);
    FILLRECT(bmp, x1, y0, x1, y1, 1);
}

static void draw_char(void *bmp, int x, int y, char c)
{
    char buf[2];
    buf[0] = c;
    buf[1] = '\0';
    TEXT(bmp, FONT5, x, y, -1, "%s", buf);
}

static void char_step(struct cc_slot *slot, int delta)
{
    int ci;
    for (ci = 0; ci < 52; ci++) {
        if (GRID_CHARS[ci] == slot->name[char_pos])
            break;
    }
    if (ci >= 52) ci = 51;
    ci = (ci + delta) % 52;
    if (ci < 0) ci += 52;
    slot->name[char_pos] = GRID_CHARS[ci];
    tracks[cur_track].slots[cur_slot].custom = 1;
    tracks[cur_track].preset_idx = -1;
}

#define ROWY(i) (48 - (i) * 6)

void ccname_draw(void *bmp, void *ctrl)
{
    int i;
    const char *preset_name;
    struct midi_track *tr;
    (void)ctrl;

    /* Always ensure descriptor pointers match active track */
    ccname_update_descriptors();

    if (!ccname_open)
        return;

    tr = &tracks[cur_track];
    preset_name = (tr->preset_idx >= 0 && tr->preset_idx < NUM_PRESETS)
                      ? PRESETS[tr->preset_idx].name
                      : "CUSTOM";

    FILLRECT(bmp, 0, 0, 127, 63, 0);

    if (edit_mode == 1) {
        /* ==== ELEKTRON AUTHENTIC NAMING SCREEN ==== */
        struct cc_slot *slot = &tr->slots[cur_slot];
        int r;

        /* Top Header */
        TEXT(bmp, FONT5, 1, 57, -1, "NAME CC%d [T%02d]", cur_slot + 1, cur_track + 9);

        /* 4-Char Name Edit Box (centered) */
        draw_box(bmp, 44, 42, 83, 54);

        /* Draw 4 characters inside Edit Box */
        for (i = 0; i < 4; i++) {
            char ch = slot->name[i];
            if (ch != '\0' && ch != ' ')
                draw_char(bmp, 49 + i * 8, 45, ch);
        }
        /* Inverted cursor highlight on active character */
        {
            int hx = 48 + char_pos * 8;
            FILLRECT(bmp, hx, 44, hx + 6, 51, -1);
        }

        if (grid_active) {
            /* Pop-Up Naming Grid (shown while holding [FUNC]) */
            int c;
            draw_box(bmp, 1, 0, 126, 39);

            TEXT(bmp, FONT5, 6, 32, -1, "SELECT CHAR [RELEASE FUNC]");
            for (r = 0; r < 4; r++) {
                int ry = 24 - r * 7;
                for (c = 0; c < 13; c++) {
                    int cx = 6 + c * 9;
                    if (r == 3 && c == 12)
                        TEXT(bmp, FONT5, cx, ry, -1, "SP");
                    else
                        draw_char(bmp, cx, ry, GRID_CHARS[r * 13 + c]);
                }
            }
            /* Highlight selected character in grid */
            {
                int hx = 6 + grid_col * 9;
                int hy = 24 - grid_row * 7;
                int hw = (grid_row == 3 && grid_col == 12) ? 10 : 6;
                FILLRECT(bmp, hx - 1, hy - 1, hx + hw - 1, hy + 5, -1);
            }
        } else {
            /* Standard editing view hints */
            TEXT(bmp, FONT5, 5, 26, -1, "HOLD [FUNC]: POP-UP GRID");
            TEXT(bmp, FONT5, 5, 16, -1, "ARROWS/LEVEL: EDIT CHAR");
            TEXT(bmp, FONT5, 5, 6,  -1, "[YES]:SAVE  [NO]:CANCEL");
        }
    } else {
        /* ==== CC SLOTS OVERVIEW MODE ==== */
        /* Header */
        TEXT(bmp, FONT5, 1, 57, -1, "MIDI CC NAMES");
        TEXT(bmp, FONT5, 78, 57, -1, "T%02d [%s]", cur_track + 9, preset_name);

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
        TEXT(bmp, FONT5, 1, 1, -1, "A:TRK B:SLOT C:PRE D:LIB [YES]");
    }
}

static void open_edit_mode(int trk, int slot, int shortcut)
{
    ccname_open = 1;
    edit_mode = 1;
    cur_track = trk;
    cur_slot = slot;
    char_pos = 0;
    grid_active = 0;
    func_action = 0;
    from_shortcut = shortcut;
    tracks[trk].slots[slot].custom = 1;
    tracks[trk].preset_idx = -1;
}

void ccname_tick(void *ctrl)
{
    unsigned char *kit = UI_KIT;

    /* Kit / Project Storage */
    if (kit) {
        int r = ccname_kstore_tick(&kst, kit);
        if (r == KS_LOADED || r == KS_OTHER) {
            int t;
            for (t = 0; t < NUM_MIDI_TRACKS; t++) {
                unsigned char b = XB(kit, MIDI_TRK_XB[t]);
                if (b != 0) {
                    int p = b - 1;
                    if (p >= 0 && p < NUM_PRESETS) {
                        apply_preset(t, p);
                    }
                } else {
                    /* Kit has no setting: initialize with starter preset and sync */
                    apply_preset(t, t == 7 ? 0 : t + 1);
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
                open_edit_mode(trk - 8, held_knob, 1);
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

    if (!ccname_open) {
        /* When on MIDI tracks (TRK 9..16), monitor [FUNC] + Knob A..H (ids 40..47) */
        int trk = ACTIVE_TRACK;
        if (trk >= 8 && trk <= 15 && func && id >= 40 && id <= 47) {
            int knob_idx = id - 40;
            if (KEYLONG(ev)) {
                /* Hardware panel driver native long-press (~0.5s) */
                open_edit_mode(trk - 8, knob_idx, 1);
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
            /* Consume repeat/intermediate events while held so stock doesn't toggle CC */
            return 1;
        }
        return 0;
    }

    if (edit_mode == 1) {
        struct cc_slot *slot = &tracks[cur_track].slots[cur_slot];

        /* Detect FUNC release: insert character if user was navigating grid */
        if (!func && grid_active) {
            if (!func_action) {
                char ch = GRID_CHARS[grid_row * 13 + grid_col];
                slot->name[char_pos] = ch;
                slot->name[4] = '\0';
                if (char_pos < 3)
                    char_pos++;
                tracks[cur_track].slots[cur_slot].custom = 1;
                tracks[cur_track].preset_idx = -1;
            }
            grid_active = 0;
            func_action = 0;
            return 1;
        }

        /* Detect FUNC press: open grid and center on current char */
        if (func && !grid_active) {
            char cur_ch = slot->name[char_pos];
            grid_locate(cur_ch);
            grid_active = 1;
            func_action = 0;
        }

        /* If key is not down edge, consume while in modal naming */
        if (!KEYDOWN(ev))
            return 1;

        /* [FUNC] + [NO]: Erase / Backspace */
        if (func && (id == 13 || id == 33)) {
            func_action = 1;
            if (char_pos > 0) {
                int k;
                for (k = char_pos - 1; k < 3; k++)
                    slot->name[k] = slot->name[k + 1];
                slot->name[3] = ' ';
                slot->name[4] = '\0';
                char_pos--;
            } else {
                slot->name[0] = ' ';
            }
            return 1;
        }

        /* [FUNC] + [YES]: Insert Space */
        if (func && (id == 12 || id == 32)) {
            int k;
            func_action = 1;
            for (k = 3; k > char_pos; k--)
                slot->name[k] = slot->name[k - 1];
            slot->name[char_pos] = ' ';
            slot->name[4] = '\0';
            if (char_pos < 3)
                char_pos++;
            return 1;
        }

        /* Grid Navigation while holding [FUNC] */
        if (grid_active) {
            switch (id) {
            case 14: /* UP */
            case 35:
                if (grid_row > 0) grid_row--; else grid_row = 3;
                break;
            case 15: /* DOWN */
            case 36:
                if (grid_row < 3) grid_row++; else grid_row = 0;
                break;
            case 16: /* LEFT */
            case 34:
                if (grid_col > 0) grid_col--; else grid_col = 12;
                break;
            case 17: /* RIGHT */
            case 37:
                if (grid_col < 12) grid_col++; else grid_col = 0;
                break;
            default:
                break;
            }
            return 1;
        }

        /* Normal Editing Navigation & Confirmation (when [FUNC] not held) */
        switch (id) {
        case 12: /* YES: save and exit edit mode */
        case 32:
            edit_mode = 0;
            ccname_mark_dirty(cur_track);
            if (from_shortcut) {
                ccname_open = 0;
                from_shortcut = 0;
            }
            break;
        case 13: /* NO: cancel and exit edit mode */
        case 33:
            edit_mode = 0;
            if (from_shortcut) {
                ccname_open = 0;
                from_shortcut = 0;
            }
            break;
        case 14: /* UP: character up */
        case 35:
            char_step(slot, 1);
            break;
        case 15: /* DOWN: character down */
        case 36:
            char_step(slot, -1);
            break;
        case 16: /* LEFT */
        case 34:
            if (char_pos > 0)
                char_pos--;
            break;
        case 17: /* RIGHT */
        case 37:
            if (char_pos < 3)
                char_pos++;
            break;
        default:
            break;
        }
        return 1;
    }

    /* ==== NORMAL CC OVERVIEW NAVIGATION MODE ==== */
    /* Transport keys (REC, PLAY, STOP) */
    if (!func && (id == 9 || id == 10 || id == 11))
        return 0;

    if (!KEYDOWN(ev))
        return 1;

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
                tracks[cur_track].preset_idx = -1; /* mark as custom */
                ccname_mark_dirty(cur_track);
            }
            return 1;
        }
        if (id == 10 || id == 25) {  /* FUNC+PLAY: Reset to default */
            apply_preset(cur_track, 0);
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
    case 12: /* YES: enter naming mode */
    case 32:
        edit_mode = 1;
        char_pos = 0;
        grid_active = 0;
        func_action = 0;
        from_shortcut = 0;
        tracks[cur_track].slots[cur_slot].custom = 1;
        tracks[cur_track].preset_idx = -1;
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
        if (grid_active) {
            /* While [FUNC] is held, LEVEL/DATA knob (id 9) navigates pop-up grid */
            if (id == 9) {
                int pos = (grid_row * 13 + grid_col + delta) % 52;
                if (pos < 0) pos += 52;
                grid_row = pos / 13;
                grid_col = pos % 13;
            }
            return 1;
        }

        /* In standard Naming Mode:
         * LEVEL/DATA knob (id 9) or Knobs E/F cycles character at cursor position
         */
        if (id == 9 || id == 5 || id == 6) {
            char_step(&tracks[cur_track].slots[cur_slot], delta);
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
    case 3: { /* Knob C: cycle Preset for track */
        int p = tracks[cur_track].preset_idx + s;
        if (p < 0) p = NUM_PRESETS - 1;
        if (p >= NUM_PRESETS) p = 0;
        apply_preset(cur_track, p);
        ccname_mark_dirty(cur_track);
        break;
    }
    case 4: { /* Knob D: quick Library Select for current slot */
        static int lib_idx = 0;
        lib_idx += s;
        if (lib_idx < 0) lib_idx = NUM_LIB_ITEMS - 1;
        if (lib_idx >= NUM_LIB_ITEMS) lib_idx = 0;

        if (lib_idx == 0) {
            tracks[cur_track].slots[cur_slot].custom = 0;
        } else {
            str_copy(tracks[cur_track].slots[cur_slot].name, CC_LIBRARY[lib_idx], 5);
            tracks[cur_track].slots[cur_slot].custom = 1;
            tracks[cur_track].preset_idx = -1;
        }
        ccname_mark_dirty(cur_track);
        break;
    }
    default:
        break;
    }

    return 1;
}
