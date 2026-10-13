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
static int edit_mode = 0;   /* 0 = select slot/preset, 1 = edit char */
static int char_pos = 0;    /* 0..3 for short name, 4..18 for long name */

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
static const char CHAR_SET[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789+-/*._#";
#define NUM_CHARS (int)(sizeof(CHAR_SET) - 1)

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

/* Count how many MIDI tracks have custom CC names configured */
static int active_custom_count(void)
{
    int t, s, count = 0;
    for (t = 0; t < NUM_MIDI_TRACKS; t++) {
        for (s = 0; s < NUM_CCS_PER_TRACK; s++) {
            if (tracks[t].slots[s].custom) {
                count++;
                break;
            }
        }
    }
    return count;
}

/* ---- SETTINGS menu callbacks ---- */

void ccname_select(void **payload)
{
    ccname_open = 1;
    edit_mode = 0;
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
    TEXT(bmp, FONT5, x + 54, y, -1, "%d/8 TRK", active_custom_count());
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

            /* In character edit mode, show underline under character */
            if (edit_mode == 1) {
                int cx = (char_pos < 4) ? (14 + char_pos * 6) : (42 + (char_pos - 4) * 6);
                FILLRECT(bmp, cx, y - 1, cx + 4, y, 1);
            }
        }
    }

    /* Footer help text */
    if (edit_mode == 1) {
        TEXT(bmp, FONT5, 1, 1, -1, "EDIT: D=POS E=CHAR YES=OK");
    } else {
        TEXT(bmp, FONT5, 1, 1, -1, "A:TRK B:SLOT C:PRE D:LIB");
    }
}

void ccname_tick(void *ctrl)
{
    ccname_update_descriptors();
    if (ccname_open)
        *((volatile char *)ctrl + 0x20) = 1;
}

#define KEYID(ev)    (*(int *)((char *)(ev) + 12))
#define KEYDOWN(ev)  (*(int *)((char *)(ev) + 16) & 1)
#define KEYFUNC(ev)  (*(int *)((char *)(ev) + 16) & 2)

int ccname_key(void *brain, void *ev)
{
    int id = KEYID(ev);
    int func = KEYFUNC(ev);
    (void)brain;

    ccname_update_descriptors();

    if (!ccname_open)
        return 0;

    /* Transport keys (REC, PLAY, STOP) */
    if (!func && (id == 9 || id == 10 || id == 11))
        return 0;

    if (!KEYDOWN(ev))
        return 1;

    /* FUNC combinations for copy/paste/clear */
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

    if (edit_mode == 1) {
        /* In character edit mode */
        switch (id) {
        case 12: /* YES: save and exit edit mode */
            edit_mode = 0;
            break;
        case 13: /* NO: cancel and exit edit mode */
            edit_mode = 0;
            break;
        case 14: /* UP: character up */
        case 15: /* DOWN: character down */
            break;
        case 34: /* LEFT */
            if (char_pos > 0)
                char_pos--;
            break;
        case 37: /* RIGHT */
            if (char_pos < 18)
                char_pos++;
            break;
        default:
            break;
        }
        return 1;
    }

    /* Normal Navigation Mode */
    switch (id) {
    case 13: /* NO: exit page */
        ccname_open = 0;
        break;
    case 14: /* UP: previous CC slot */
        if (cur_slot > 0)
            cur_slot--;
        else
            cur_slot = 7;
        break;
    case 15: /* DOWN: next CC slot */
        if (cur_slot < 7)
            cur_slot++;
        else
            cur_slot = 0;
        break;
    case 34: /* LEFT: previous MIDI track */
        if (cur_track > 0)
            cur_track--;
        else
            cur_track = NUM_MIDI_TRACKS - 1;
        break;
    case 37: /* RIGHT: next MIDI track */
        if (cur_track < NUM_MIDI_TRACKS - 1)
            cur_track++;
        else
            cur_track = 0;
        break;
    case 12: /* YES: toggle edit mode */
        edit_mode = 1;
        char_pos = 0;
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

    if (edit_mode == 1) {
        /* In Character Edit Mode */
        struct cc_slot *slot = &tracks[cur_track].slots[cur_slot];
        char *target_str;
        int target_idx, max_len;

        if (id == 4) { /* Knob D: move char cursor */
            char_pos += s;
            if (char_pos < 0) char_pos = 0;
            if (char_pos > 18) char_pos = 18;
            return 1;
        }

        if (id == 5 || id == 9) { /* Knob E: scroll character */
            int ci = 0;
            char current_ch;

            if (char_pos < 4) {
                target_str = slot->short_name;
                target_idx = char_pos;
                max_len = 4;
            } else {
                target_str = slot->long_name;
                target_idx = char_pos - 4;
                max_len = 15;
            }

            current_ch = target_str[target_idx];
            /* Find current char in CHAR_SET */
            for (ci = 0; ci < NUM_CHARS; ci++) {
                if (CHAR_SET[ci] == current_ch)
                    break;
            }
            if (ci >= NUM_CHARS) ci = 0;

            ci = (ci + s) % NUM_CHARS;
            if (ci < 0) ci += NUM_CHARS;

            target_str[target_idx] = CHAR_SET[ci];
            /* Ensure string termination */
            if (target_str[max_len] != '\0')
                target_str[max_len] = '\0';
            return 1;
        }
        return 1;
    }

    /* In Normal Navigation Mode */
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
            /* Default */
            tracks[cur_track].slots[cur_slot].custom = 0;
        } else {
            str_copy(tracks[cur_track].slots[cur_slot].short_name, CC_LIBRARY[lib_idx].s, 5);
            str_copy(tracks[cur_track].slots[cur_slot].long_name, CC_LIBRARY[lib_idx].l, 16);
            tracks[cur_track].slots[cur_slot].custom = 1;
            tracks[cur_track].preset_idx = -1; /* modified */
        }
        break;
    }
    default:
        break;
    }

    return 1;
}
