# takt1ccname: Custom MIDI CC Names for Elektron Digitakt I

An unofficial, community-made firmware mod for the **Elektron Digitakt MKI** (OS 1.53 and OS 1.54) that adds **custom parameter names for each MIDI CC per track**.

Built as an [elekloader](https://github.com/irpina/elekloader) linkable mod, designed for full compatibility with [digi1_mods](https://github.com/gdeo607/digi1_mods) (Digi Poly, Digi Mono, Digi Matrix, Digi EQ, Digi Utilities, and digichain).

---

## The Problem

On stock Digitakt firmware, sequencing external synthesizers and MIDI devices on MIDI tracks (TRK 9–16) means interacting with 8 generic parameters:
- On the **FLTR page** (CC VALUE): Knobs A–H are labeled `VAL1` through `VAL8`.
- On the **AMP page** (CC SELECT): Knobs A–H are labeled `SEL1` through `SEL8`.

Musicians sequencing multiple external synths must memorize or write down which CC controls which parameter on each track.

## The Solution

**takt1ccname** allows setting individual custom names for each of the 8 MIDI CCs per MIDI track:
- **On-Screen Display:** Whenever a MIDI track (TRK 9–16) is active, the FLTR and AMP pages display your custom 4-character names (e.g. `CUTO`, `RESO`, `ATK `, `DEC `) above encoders A–H instead of `VAL1`..`VAL8`.
- **Parameter Popups & Lists:** Turning or holding encoders shows your custom 4-character names cleanly without truncation.
- **LFO Modulation Destinations:** The LFO DEST page displays the custom parameter names for the track.
- **Independent Per Track:** Track 9 can control a subtractive poly synth (`CUTO`, `RESO`, `ENV `...), Track 10 can control an FM synth (`WAVE`, `TIMB`, `MORP`...), and Track 11 can control a drum machine (`PICH`, `SNAP`, `TONE`...).
- **Kit & Project Persistence:** Presets selected per track are stored into the pattern's non-volatile kit mirror, surviving power cycles and project saves.

---

## Features

- **8 MIDI Tracks x 8 CC Slots:** Up to 64 custom parameter names.
- **Built-in Presets:**
  - `DEFAULT`: Standard stock `VAL1`..`VAL8`
  - `SYNTH`: Standard subtractive synthesizer (`CUTO`, `RESO`, `ENV `, `ATK `, `DEC `, `SUS `, `REL `, `LFOR`)
  - `MINILOGUE`: Korg Minilogue / XD mapping (`CUTO`, `RESO`, `EG  `, `ATK `, `DEC `, `LFOR`, `LFOD`, `SHAP`)
  - `MICROFREAK`: Arturia MicroFreak mapping (`CUTO`, `RESO`, `CYCL`, `ENV `, `DECA`, `WAVE`, `TIMB`, `MORP`)
  - `BASS SYNTH`: Acid / Bass synthesizer (`CUTO`, `RESO`, `SUB `, `DIST`, `ATK `, `DEC `, `ACC `, `GLID`)
  - `DRUM/SAMP`: External drum machines and samplers (`PICH`, `DECA`, `TUNE`, `SNAP`, `TONE`, `SWNG`, `VOL `, `PAN `)
  - `MOD/FX`: FX processors and pedals (`TIME`, `FEED`, `RATE`, `DPTH`, `MIX `, `DRY `, `WET `, `SHAP`)
  - `GEN. MIDI`: General MIDI & DAW controls (`MOD `, `BREA`, `VOL `, `PAN `, `EXPR`, `SUST`, `BRIG`, `REVB`)
- **Quick Name Library:** 32 standard synth parameter abbreviations (`CUTO`, `RESO`, `ENV `, `ATK `, `DEC `, `SUS `, `REL `, `VOL `, `PAN `, `TUNE`, `PICH`, `GLID`, `LFOR`, `LFOD`, `WAVE`, `PWM `, `SUB `, `DIST`, `TIME`, `FEED`, `RATE`, `DPTH`, `MIX `, `DRY `, `WET `, `MOD `, `BREA`, `EXPR`, `BRIG`, `REVB`, `SHAP`) selectable via Knob D in seconds.
- **Character Editor:** Edit any custom 4-character name directly from the hardware front panel.
- **Clipboard Shortcuts:** Fast copy/paste between MIDI tracks.
- **Zero Audio Impact:** Zero CPU overhead during playback and render; zero impact on audio tracks.
- **100% Non-Colliding:** Uses the core event bus and dynamic descriptor redirection rather than patching shared code sites.

---

## How to Name CCs (Digitakt II Style Shortcut)

Just like on the Digitakt II:
1. Select any MIDI track (**TRK 9–16**).
2. On the **FLTR** (CC VALUE) or **AMP** (CC SELECT) page, **press and hold `[FUNC]` and press & hold DATA ENTRY knob A–H** (~300 ms).
3. The on-screen **Authentic Elektron Naming Screen** opens immediately for that exact CC slot!
4. Edit the 4-character name and press **`[YES]`** to save. The naming modal closes directly back to your parameter page with your new custom name active!

---

## User Guide: SETTINGS > MIDI CC NAMES

You can also navigate to **SETTINGS > MIDI CC NAMES** on the Digitakt to open the full overview and management screen.

### Navigation Mode
- **Knob A (or [LEFT] / [RIGHT]):** Select active MIDI track (TRK 09 to TRK 16).
- **Knob B / LEVEL (or [UP] / [DOWN]):** Select CC Slot (1 to 8).
- **Knob C:** Cycle track preset (`DEFAULT`, `SYNTH`, `MINILOGUE`, `MICROFREAK`, etc.).
- **Knob D:** Quick library select for the highlighted slot (instantly assigns common parameters).
- **[YES]:** Enter character editing mode for the selected slot.
- **[NO]:** Exit and return to the SETTINGS menu.

### Authentic Elektron Pop-Up Naming Screen
Pressing **[YES]** on any CC slot opens the dedicated Naming Screen:
- **Hold `[FUNC]`:** Opens the 4-row character selection matrix.
  - **LEVEL/DATA Knob or Arrow Keys:** Move cursor across the character matrix.
  - **Release `[FUNC]`:** Inserts the highlighted character and advances cursor to next position.
- **`[LEFT]` / `[RIGHT]`:** Move character cursor position (0..3).
- **`[UP]` / `[DOWN]` or LEVEL/DATA Knob:** Step character at cursor up/down without opening grid.
- **`[FUNC]` + `[NO]`:** Erase character / Backspace.
- **`[FUNC]` + `[YES]`:** Insert Space.
- **`[YES]`:** Confirm name and save.
- **`[NO]`:** Cancel and return to overview list.

### Track Clipboard Shortcuts
- **[FUNC] + [RECORD]:** Copy current track's CC names to clipboard.
- **[FUNC] + [STOP]:** Paste copied CC names to current track.
- **[FUNC] + [PLAY]:** Reset current track to default (`VAL1`..`VAL8`).

---

## Building and Flashing

### Requirements
- Linux or macOS with `python3`, `make`.
- Official Digitakt OS 1.54 or 1.53 file (`Digitakt_OS1.54.syx`).

### Build the Mod
```bash
make
```
Builds `out/takt1ccname-1.0-os1.54.elemod`.

### Run Automated Tests
```bash
make test
```
Executes the Unicorn CPU emulation test suite verifying descriptor redirection, track switching, presets, and UI handling.

### Build Combined Flashable Firmware
```bash
make syx
```
Creates `out/Digitakt_OS1.54_ccname.syx`.

### Flashing to Hardware
1. Connect Digitakt to your computer via USB.
2. Launch **Elektron Transfer**.
3. Drag and drop `out/Digitakt_OS1.54_ccname.syx` into Transfer.
4. Follow on-screen instructions on the Digitakt to complete update.

---

## Compatibility with digi1_mods

`takt1ccname` is fully compatible with the `digi1_mods` ecosystem:
- **Digi Poly:** Coexists without conflicts.
- **Digi Mono & Digi Macro:** Preserves all custom synth machine pages.
- **Digi Matrix & Digi EQ:** Preserves all LFO matrix routings and Master EQ settings.
- **Digi Utilities:** Preserves scope, tuner, and spectrum analyser views on the `...` key.
- **digichain:** Combines cleanly in elekloader builds.

To build a full firmware image containing `takt1ccname` alongside all `digi1_mods`, simply include both `.elemod` packages in elekloader.

---

## License

GPL-2.0-or-later
