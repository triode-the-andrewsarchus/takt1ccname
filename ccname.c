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

/* Access parameter descriptor fields in firmware RAM (52 bytes per entry) */
#define DESC_PTR(id, offset) (*(const char *volatile *)(DESC_TABLE_BASE + (id) * 52 + (offset)))
#define DESC_LONG(id)        DESC_PTR(id, 40)
#define DESC_SHORT(id)       DESC_PTR(id, 48)

#define NUM_MIDI_TRACKS 8
#define NUM_CCS_PER_TRACK 8

struct cc_slot {
    char short_name[5];   /* 4 characters + '\0' */
    char long_name[16];   /* up to 15 characters + '\0' */
    unsigned char custom; /* 1 if custom name is active, 0 for default */
};

struct midi_track {
    int preset_idx;
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
static int char_pos = 0;    /* 0..14 cursor position */
static int edit_field = 0;  /* 0 = long name (up to 15), 1 = short name (up to 4) */
static int grid_active = 0; /* 1 when holding [FUNC] to show pop-up grid */
static int grid_row = 0;    /* 0..3 grid row */
static int grid_col = 0;    /* 0..12 grid column */
static int func_action = 0; /* 1 if FUNC+NO (erase) or FUNC+YES (space) occurred */
static int held_knob = -1;  /* 0..7 for knob A..H held with FUNC, -1 = none */
static int held_ticks = 0;  /* tick count for press-and-hold detection */
static int from_shortcut = 0; /* 1 if opened directly via [FUNC]+knob */

/* Clipboard */
static struct cc_slot clip_slots[NUM_CCS_PER_TRACK];
static int clip_valid = 0;

/* Presets */
struct cc_preset {
    const char *name;
    struct {
        const char *s;
        const char *l;
    } slots[8];
};

static const struct cc_preset PRESETS[] = {
    {
        "DEFAULT",
        {
            {"VAL1", "CC1 Value"},
            {"VAL2", "CC2 Value"},
            {"VAL3", "CC3 Value"},
            {"VAL4", "CC4 Value"},
            {"VAL5", "CC5 Value"},
            {"VAL6", "CC6 Value"},
            {"VAL7", "CC7 Value"},
            {"VAL8", "CC8 Value"}
        }
    },
    {
        "SYNTH",
        {
            {"CUTO", "Cutoff"},
            {"RESO", "Resonance"},
            {"ENV ", "Filter Env"},
            {"ATK ", "Attack Time"},
            {"DEC ", "Decay Time"},
            {"SUS ", "Sustain Level"},
            {"REL ", "Release Time"},
            {"LFOR", "LFO Rate"}
        }
    },
    {
        "MINILOGUE",
        {
            {"CUTO", "Cutoff Freq"},
            {"RESO", "Resonance"},
            {"EG  ", "EG Intensity"},
            {"ATK ", "Attack Time"},
            {"DEC ", "Decay Time"},
            {"LFOR", "LFO Rate"},
            {"LFOD", "LFO Depth"},
            {"SHAP", "Osc Shape"}
        }
    },
    {
        "MICROFREAK",
        {
            {"CUTO", "Cutoff Freq"},
            {"RESO", "Resonance"},
            {"CYCL", "Cycling Env"},
            {"ENV ", "Env Amount"},
            {"DECA", "Env Decay"},
            {"WAVE", "Waveform"},
            {"TIMB", "Timbre"},
            {"MORP", "Morph"}
        }
    },
    {
        "BASS SYNTH",
        {
            {"CUTO", "Filter Cutoff"},
            {"RESO", "Resonance"},
            {"SUB ", "Sub Osc Level"},
            {"DIST", "Distortion"},
            {"ATK ", "Amp Attack"},
            {"DEC ", "Amp Decay"},
            {"ACC ", "Accent Level"},
            {"GLID", "Glide Time"}
        }
    },
    {
        "DRUM/SAMP",
        {
            {"PICH", "Pitch Tune"},
            {"DECA", "Decay Length"},
            {"TUNE", "Sample Tune"},
            {"SNAP", "Snappy/Bite"},
            {"TONE", "Tone Color"},
            {"SWNG", "Track Swing"},
            {"VOL ", "Track Volume"},
            {"PAN ", "Track Pan"}
        }
    },
    {
        "MOD/FX",
        {
            {"TIME", "FX Delay Time"},
            {"FEED", "FX Feedback"},
            {"RATE", "Chorus Rate"},
            {"DPTH", "Chorus Depth"},
            {"MIX ", "FX Wet Mix"},
            {"DRY ", "Dry Level"},
            {"WET ", "Wet Level"},
            {"SHAP", "Mod Waveshape"}
        }
    },
    {
        "GEN. MIDI",
        {
            {"MOD ", "Mod Wheel"},
            {"BREA", "Breath Ctrl"},
            {"VOL ", "Main Volume"},
            {"PAN ", "Stereo Pan"},
            {"EXPR", "Expression"},
            {"SUST", "Sustain Pedal"},
            {"BRIG", "Brightness"},
            {"REVB", "Reverb Send"}
        }
    }
};
#define NUM_PRESETS (int)(sizeof(PRESETS) / sizeof(PRESETS[0]))

/* Library of standard parameters for single-slot assignment */
struct cc_lib_item {
    const char *s;
    const char *l;
};

static const struct cc_lib_item CC_LIBRARY[] = {
    {"----", "Default"},
    {"CUTO", "Cutoff"},
    {"RESO", "Resonance"},
    {"ENV ", "Filter Env"},
    {"ATK ", "Attack"},
    {"DEC ", "Decay"},
    {"SUS ", "Sustain"},
    {"REL ", "Release"},
    {"VOL ", "Volume"},
    {"PAN ", "Panorama"},
    {"LEV ", "Level"},
    {"TUNE", "Tune"},
    {"FINE", "Fine Tune"},
    {"PICH", "Pitch"},
    {"GLID", "Glide"},
    {"PORT", "Portamento"},
    {"LFOR", "LFO Rate"},
    {"LFOD", "LFO Depth"},
    {"LFOW", "LFO Wave"},
    {"WAVE", "Waveform"},
    {"SHAP", "Waveshape"},
    {"PWM ", "Pulse Width"},
    {"SYNC", "Osc Sync"},
    {"SUB ", "Sub Osc"},
    {"NOIS", "Noise Level"},
    {"DIST", "Distortion"},
    {"DRIV", "Overdrive"},
    {"CRUS", "Bitcrush"},
    {"FLTR", "Filter Type"},
    {"VEL ", "Velocity"},
    {"AFT ", "Aftertouch"},
    {"MOD ", "Mod Wheel"},
    {"BREA", "Breath Ctrl"},
    {"EXPR", "Expression"},
    {"TIME", "Delay Time"},
    {"FEED", "Feedback"},
    {"REVT", "Reverb Time"},
    {"REVD", "Reverb Damp"},
    {"MIX ", "Effect Mix"},
    {"DRY ", "Dry Level"},
    {"WET ", "Wet Level"}
};
#define NUM_LIB_ITEMS (int)(sizeof(CC_LIBRARY) / sizeof(CC_LIBRARY[0]))

/* Characters for text editing */
static const char CHAR_SET[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_+!/?#%*.:=";
#define NUM_CHARS (int)(sizeof(CHAR_SET) - 1)

static const char GRID_CHARS[4][13] = {
    {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M'},
    {'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z'},
    {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '-', '_', '+'},
    {'!', '?', '/', '#', '%', '*', '.', '-', ':', '=', '(', ')', ' '}
};

static void grid_locate(char c)
{
    int r, col;
    if (c >= 'a' && c <= 'z') c -= 32;
    for (r = 0; r < 4; r++) {
        for (col = 0; col < 13; col++) {
            if (GRID_CHARS[r][col] == c) {
                grid_row = r;
                grid_col = col;
                return;
            }
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

/* Auto-derive a 4-character short name from the long name */
static void auto_derive_short(struct cc_slot *slot)
{
    int i = 0, j = 0;
    while (slot->long_name[i] == ' ' && i < 15)
        i++;
    for (; j < 4 && i < 15 && slot->long_name[i] != '\0'; i++) {
        char c = slot->long_name[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        slot->short_name[j++] = c;
    }
    for (; j < 4; j++)
        slot->short_name[j] = ' ';
    slot->short_name[4] = '\0';
}

static void apply_preset(int track_idx, int preset_idx)
{
    int s;
    if (track_idx < 0 || track_idx >= NUM_MIDI_TRACKS)
        return;
    if (preset_idx < 0 || preset_idx >= NUM_PRESETS)
        return;

    tracks[track_idx].preset_idx = preset_idx;
    for (s = 0; s < 8; s++) {
        str_copy(tracks[track_idx].slots[s].short_name, PRESETS[preset_idx].slots[s].s, 5);
        str_copy(tracks[track_idx].slots[s].long_name, PRESETS[preset_idx].slots[s].l, 16);
        tracks[track_idx].slots[s].custom = (preset_idx != 0);
    }
}

/* Initialize all tracks with defaults or useful starter templates */
static void init_defaults(void)
{
    int t;
    /* Track 9 (index 0): Standard Synth */
    apply_preset(0, 1);
    /* Track 10 (index 1): Minilogue */
    apply_preset(1, 2);
    /* Track 11 (index 2): MicroFreak */
    apply_preset(2, 3);
    /* Track 12 (index 3): Bass Synth */
    apply_preset(3, 4);
    /* Track 13 (index 4): Drum/Sampler */
    apply_preset(4, 5);
    /* Track 14 (index 5): Mod/FX */
    apply_preset(5, 6);
    /* Track 15 (index 6): General MIDI */
    apply_preset(6, 7);
    /* Track 16 (index 7): Default VAL1-8 */
    apply_preset(7, 0);

    for (t = 0; t < NUM_MIDI_TRACKS; t++) {
        tracks[t].preset_idx = (t == 7) ? 0 : (t + 1);
    }
}

/* Active track descriptor updater */
static void ccname_update_descriptors(void)
{
    int track = ACTIVE_TRACK;
    int i;

    /* Save original stock pointers on first tick */
    if (!stock_saved) {
        for (i = 0; i < 16; i++) {
            stock_short[i] = DESC_SHORT(148 + i);
            stock_long[i] = DESC_LONG(148 + i);
        }
        stock_saved = 1;
        init_defaults();
    }

    if (track >= 8 && track <= 15) {
        int t = track - 8;
        for (i = 0; i < 8; i++) {
            struct cc_slot *slot = &tracks[t].slots[i];
            if (slot->custom) {
                DESC_SHORT(148 + i) = slot->short_name;
                DESC_LONG(148 + i) = slot->long_name;
                DESC_SHORT(156 + i) = slot->short_name;
                DESC_LONG(156 + i) = slot->long_name;
            } else {
                DESC_SHORT(148 + i) = stock_short[i];
                DESC_LONG(148 + i) = stock_long[i];
                DESC_SHORT(156 + i) = stock_short[8 + i];
                DESC_LONG(156 + i) = stock_long[8 + i];
            }
        }
    } else {
        /* On audio tracks (0..7), restore stock pointers */
        for (i = 0; i < 8; i++) {
            DESC_SHORT(148 + i) = stock_short[i];
            DESC_LONG(148 + i) = stock_long[i];
            DESC_SHORT(156 + i) = stock_short[8 + i];
            DESC_LONG(156 + i) = stock_long[8 + i];
        }
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
        TEXT(bmp, FONT5, 86, 57, -1, "[%s]", edit_field == 0 ? "LONG" : "SHRT");

        /* Long Name Edit Box */
        FILLRECT(bmp, 2, 43, 125, 53, 0);
        FILLRECT(bmp, 2, 42, 125, 42, 1);
        FILLRECT(bmp, 2, 54, 125, 54, 1);
        FILLRECT(bmp, 2, 42, 2, 54, 1);
        FILLRECT(bmp, 125, 42, 125, 54, 1);

        /* Active Field Text inside Box */
        if (edit_field == 0) {
            TEXT(bmp, FONT5, 5, 45, -1, "%-15s", slot->long_name);
            int cx = 5 + char_pos * 6;
            FILLRECT(bmp, cx - 1, 44, cx + 5, 52, -1);
        } else {
            TEXT(bmp, FONT5, 5, 45, -1, "SHORT: %-4s", slot->short_name);
            int cx = 47 + char_pos * 6;
            FILLRECT(bmp, cx - 1, 44, cx + 5, 52, -1);
        }

        if (grid_active) {
            /* Pop-Up Naming Grid (shown while holding [FUNC]) */
            int c;
            FILLRECT(bmp, 1, 0, 126, 39, 0);
            FILLRECT(bmp, 1, 0, 126, 0, 1);
            FILLRECT(bmp, 1, 39, 126, 39, 1);
            FILLRECT(bmp, 1, 0, 1, 39, 1);
            FILLRECT(bmp, 126, 0, 126, 39, 1);

            TEXT(bmp, FONT5, 6, 32, -1, "SELECT CHAR [RELEASE FUNC]");
            for (r = 0; r < 4; r++) {
                int ry = 24 - r * 7;
                for (c = 0; c < 13; c++) {
                    int cx = 6 + c * 9;
                    if (r == 3 && c == 12) {
                        TEXT(bmp, FONT5, cx, ry, -1, "SP");
                    } else {
                        char ch_str[2];
                        ch_str[0] = GRID_CHARS[r][c];
                        ch_str[1] = '\0';
                        TEXT(bmp, FONT5, cx, ry, -1, "%s", ch_str);
                    }
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
            if (edit_field == 0)
                TEXT(bmp, FONT5, 5, 32, -1, "SHORT: [%-4s]", slot->short_name);
            else
                TEXT(bmp, FONT5, 5, 32, -1, "LONG:  [%-15s]", slot->long_name);

            TEXT(bmp, FONT5, 5, 22, -1, "HOLD [FUNC]: POP-UP GRID");
            TEXT(bmp, FONT5, 5, 12, -1, "ARROWS/LEVEL: EDIT CHAR");
            TEXT(bmp, FONT5, 5, 2,  -1, "[PAGE]:FIELD  [YES]:SAVE");
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
                TEXT(bmp, FONT5, 14, y, -1, "%-4s", slot->short_name);
                TEXT(bmp, FONT5, 42, y, -1, "%s", slot->long_name);
            } else {
                TEXT(bmp, FONT5, 14, y, -1, "VAL%d", i + 1);
                TEXT(bmp, FONT5, 42, y, -1, "--");
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

void ccname_tick(void *ctrl)
{
    ccname_update_descriptors();

    /* Press-and-hold [FUNC] + DATA ENTRY knob A-H (Digitakt II style shortcut) */
    if (held_knob >= 0) {
        held_ticks++;
        if (held_ticks >= 6) {
            int trk = ACTIVE_TRACK;
            if (trk >= 8 && trk <= 15) {
                ccname_open = 1;
                edit_mode = 1;
                cur_track = trk - 8;
                cur_slot = held_knob;
                char_pos = 0;
                grid_active = 0;
                func_action = 0;
                edit_field = 0;
                from_shortcut = 1;
                tracks[cur_track].slots[cur_slot].custom = 1;
                tracks[cur_track].preset_idx = -1;
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
                ccname_open = 1;
                edit_mode = 1;
                cur_track = trk - 8;
                cur_slot = knob_idx;
                char_pos = 0;
                grid_active = 0;
                func_action = 0;
                edit_field = 0;
                from_shortcut = 1;
                tracks[cur_track].slots[cur_slot].custom = 1;
                tracks[cur_track].preset_idx = -1;
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
                char ch = GRID_CHARS[grid_row][grid_col];
                if (edit_field == 0) {
                    slot->long_name[char_pos] = ch;
                    slot->long_name[15] = '\0';
                    if (char_pos < 14)
                        char_pos++;
                    auto_derive_short(slot);
                } else {
                    slot->short_name[char_pos] = ch;
                    slot->short_name[4] = '\0';
                    if (char_pos < 3)
                        char_pos++;
                }
                tracks[cur_track].slots[cur_slot].custom = 1;
                tracks[cur_track].preset_idx = -1;
            }
            grid_active = 0;
            func_action = 0;
            return 1;
        }

        /* Detect FUNC press: open grid and center on current char */
        if (func && !grid_active) {
            char cur_ch = (edit_field == 0) ? slot->long_name[char_pos] : slot->short_name[char_pos];
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
            if (edit_field == 0) {
                if (char_pos > 0) {
                    int k;
                    for (k = char_pos - 1; k < 14; k++)
                        slot->long_name[k] = slot->long_name[k + 1];
                    slot->long_name[14] = ' ';
                    slot->long_name[15] = '\0';
                    char_pos--;
                    auto_derive_short(slot);
                } else {
                    slot->long_name[0] = ' ';
                    auto_derive_short(slot);
                }
            } else {
                if (char_pos > 0) {
                    int k;
                    for (k = char_pos - 1; k < 3; k++)
                        slot->short_name[k] = slot->short_name[k + 1];
                    slot->short_name[3] = ' ';
                    slot->short_name[4] = '\0';
                    char_pos--;
                }
            }
            return 1;
        }

        /* [FUNC] + [YES]: Insert Space */
        if (func && (id == 12 || id == 32)) {
            func_action = 1;
            if (edit_field == 0) {
                int k;
                for (k = 14; k > char_pos; k--)
                    slot->long_name[k] = slot->long_name[k - 1];
                slot->long_name[char_pos] = ' ';
                slot->long_name[15] = '\0';
                if (char_pos < 14)
                    char_pos++;
                auto_derive_short(slot);
            }
            return 1;
        }

        /* PAGE key: Toggle Long Name vs Short Name field */
        if (id == 18 || id == 38) {
            edit_field = 1 - edit_field;
            char_pos = 0;
            if (func)
                func_action = 1;
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
        case 35: {
            char *target_str = (edit_field == 0) ? slot->long_name : slot->short_name;
            int ci;
            for (ci = 0; ci < NUM_CHARS; ci++) {
                if (CHAR_SET[ci] == target_str[char_pos]) break;
            }
            if (ci >= NUM_CHARS) ci = 0;
            ci = (ci + 1) % NUM_CHARS;
            target_str[char_pos] = CHAR_SET[ci];
            tracks[cur_track].slots[cur_slot].custom = 1;
            tracks[cur_track].preset_idx = -1;
            if (edit_field == 0) auto_derive_short(slot);
            break;
        }
        case 15: /* DOWN: character down */
        case 36: {
            char *target_str = (edit_field == 0) ? slot->long_name : slot->short_name;
            int ci;
            for (ci = 0; ci < NUM_CHARS; ci++) {
                if (CHAR_SET[ci] == target_str[char_pos]) break;
            }
            if (ci >= NUM_CHARS) ci = 0;
            ci = (ci - 1 + NUM_CHARS) % NUM_CHARS;
            target_str[char_pos] = CHAR_SET[ci];
            tracks[cur_track].slots[cur_slot].custom = 1;
            tracks[cur_track].preset_idx = -1;
            if (edit_field == 0) auto_derive_short(slot);
            break;
        }
        case 16: /* LEFT */
        case 34:
            if (char_pos > 0)
                char_pos--;
            break;
        case 17: /* RIGHT */
        case 37:
            if (char_pos < (edit_field ? 3 : 14))
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
            int s;
            for (s = 0; s < 8; s++)
                clip_slots[s] = tracks[cur_track].slots[s];
            clip_valid = 1;
            return 1;
        }
        if (id == 11 || id == 26) {  /* FUNC+STOP: Paste CC names */
            if (clip_valid) {
                int s;
                for (s = 0; s < 8; s++)
                    tracks[cur_track].slots[s] = clip_slots[s];
                tracks[cur_track].preset_idx = -1; /* mark as custom */
            }
            return 1;
        }
        if (id == 10 || id == 25) {  /* FUNC+PLAY: Reset to default */
            apply_preset(cur_track, 0);
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
        edit_field = 0;
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
            struct cc_slot *slot = &tracks[cur_track].slots[cur_slot];
            char *target_str = (edit_field == 0) ? slot->long_name : slot->short_name;
            int ci;

            for (ci = 0; ci < NUM_CHARS; ci++) {
                if (CHAR_SET[ci] == target_str[char_pos])
                    break;
            }
            if (ci >= NUM_CHARS) ci = 0;

            ci = (ci + delta) % NUM_CHARS;
            if (ci < 0) ci += NUM_CHARS;
            target_str[char_pos] = CHAR_SET[ci];
            tracks[cur_track].slots[cur_slot].custom = 1;
            tracks[cur_track].preset_idx = -1;
            if (edit_field == 0)
                auto_derive_short(slot);
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
            str_copy(tracks[cur_track].slots[cur_slot].short_name, CC_LIBRARY[lib_idx].s, 5);
            str_copy(tracks[cur_track].slots[cur_slot].long_name, CC_LIBRARY[lib_idx].l, 16);
            tracks[cur_track].slots[cur_slot].custom = 1;
            tracks[cur_track].preset_idx = -1;
        }
        break;
    }
    default:
        break;
    }

    return 1;
}
