# takt1ccname: Custom MIDI CC Names for Elektron Digitakt I

An unofficial, community-made firmware mod for the **Elektron Digitakt MKI** (OS 1.53 and OS 1.54) that adds **independent custom parameter names for each MIDI CC knob per track**.

Built as an [elekloader](https://github.com/irpina/elekloader) linkable mod, designed for full compatibility with [digi1_mods](https://github.com/gdeo607/digi1_mods) (Digi Poly, Digi Mono, Digi Matrix, Digi EQ, Digi Utilities, and digichain).

---

## The Problem

On stock Digitakt firmware, sequencing external synthesizers and MIDI devices on MIDI tracks (TRK 9–16) means interacting with generic parameter labels:
- On the **FLTR page** (CC VALUE): Knobs A–H are labeled `VAL1` through `VAL8`.
- On the **AMP page** (CC SELECT): Knobs A–H are labeled `SEL1` through `SEL8`.

Musicians sequencing multiple external synths must memorize or write down which CC controls which parameter on each track.

---

## The Solution

**takt1ccname** allows setting individual custom names for every MIDI CC knob across all 8 MIDI tracks:
- **On-Screen Display:** When on a MIDI track (TRK 9–16), the FLTR and AMP pages display your custom 4-character names (e.g. `CUTO`, `RESO`, `ATK `, `DEC `) above encoders A–H instead of `VAL1`..`VAL8`.
- **Parameter Popups & Lists:** Turning or holding encoders shows your custom 4-character names cleanly without truncation.
- **LFO Modulation Destinations:** The LFO DEST page displays the custom parameter names for the track.
- **Completely Independent Per Knob:** Every knob on every MIDI track independently selects from a 64-word library. Track 9 can control a subtractive poly synth (`CUTO`, `RESO`, `ATK `, `DEC `...), Track 10 an FM synth (`WAVE`, `TIMB`, `FM  `, `MIX `...), and Track 11 a drum machine (`PICH`, `SNAP`, `TONE`, `DIST`...).
- **Durable Project & Reboot Persistence:** Each knob's 6-bit selection is packed directly into the kit's non-volatile storage, surviving power cycles, reboots, and project saves (`[SETTINGS] > PROJECT > SAVE`).
- **Customizable Build-Time Wordlist:** The 64 words are read directly from `words.txt`, making it trivial to add or change words for your exact synth setup.

---

## Features

- **8 MIDI Tracks x 8 CC Knobs:** All 64 knobs independently configurable.
- **64-Word Library (`words.txt`):** Includes standard synth, filter, envelope, modulation, FX, and mixer parameters (`CUTO`, `RESO`, `ENV `, `ATK `, `DEC `, `SUS `, `REL `, `VOL `, `PAN `, `TUNE`, `PICH`, `GLID`, `LFOR`, `LFOD`, `WAVE`, `PWM `, `SUB `, `DIST`, `TIME`, `FEED`, `RATE`, `DPTH`, `MIX `, `MOD `, `HPF `, `BPF `, `DRIV`, `CRSH`, `COMP`, `REVB`, etc.).
- **Fast Scrolling Word Picker Menu:** Browse and assign words in seconds using the LEVEL knob or arrow keys with an authentic inverted OLED highlight bar.
- **Digitakt II Style Shortcut:** Hold `[FUNC]` + hold Knob A–H on any MIDI parameter page to open the Word Picker menu directly for that knob.
- **Full Settings Overview Page:** Access via `[SETTINGS] > MIDI CC NAMES` to view and configure all 8 CC knobs for any track.
- **Clipboard Shortcuts:** Fast copy/paste of all CC names between MIDI tracks.
- **Zero Audio Impact:** Zero CPU overhead during audio playback; 0 impact on audio tracks.
- **100% Non-Colliding:** Uses the core event bus and dynamic descriptor redirection rather than patching shared code sites. Fits within ColdFire RAM budget with all 10 `digi1_mods` active (over 4.5 KB RAM headroom).

---

## Quick Start: Assigning CC Names

### Method 1: Direct Shortcut (Digitakt II Style)
1. Select any MIDI track (**TRK 9–16**).
2. On the **FLTR** (CC VALUE) or **AMP** (CC SELECT) page, **press and hold `[FUNC]` and press & hold DATA ENTRY knob A–H** (~300 ms).
3. The **Word Picker Menu** opens immediately for that exact knob.
4. Turn the **LEVEL knob** or press **`[UP]` / `[DOWN]`** to scroll through the 64 words (use **`[LEFT]` / `[RIGHT]`** to jump 8 at a time).
5. Press **`[YES]`** to confirm. The menu closes directly back to your parameter page with your new name active!

### Method 2: Overview Page (`[SETTINGS] > MIDI CC NAMES`)
Navigate to **`[SETTINGS] > MIDI CC NAMES`** to see an overview of all 8 CC slots on the track:
- **Knob A (or `[LEFT]` / `[RIGHT]`):** Select MIDI Track (TRK 09 to TRK 16).
- **Knob B / LEVEL (or `[UP]` / `[DOWN]`):** Select CC Slot (1 to 8).
- **Knob C:** Directly cycle word for current slot (+1 / -1).
- **Knob D:** Jump words in steps of 8.
- **`[YES]`:** Open full Word Picker Menu for the highlighted slot.
- **`[NO]`:** Exit back to the SETTINGS menu.

### Track Clipboard Shortcuts
- **`[FUNC]` + `[RECORD]`:** Copy current track's CC names to clipboard.
- **`[FUNC]` + `[STOP]`:** Paste copied CC names to current track.
- **`[FUNC]` + `[PLAY]`:** Reset current track to default stock names (`VAL1`..`VAL8`).

---

## Customizing the Wordlist (`words.txt`)

You can customize the 64 available words for your own synths:
1. Open [`words.txt`](words.txt) in any text editor.
2. Edit any line (1 to 4 characters per word, 64 lines total).
   - Line 1 (`----`) represents the stock default (`VAL1`..`VAL8`).
   - Lines 2–64 are your custom parameter names.
3. Run `make syx` to regenerate `words.h` and rebuild the firmware in seconds.

---

## Building and Flashing

### Requirements
- Linux or macOS with `make`, `python3` (with `unicorn` and `capstone` in venv).
- Official Digitakt OS 1.54 SysEx file (`Digitakt_OS1.54.syx`).

### Build the Mod
```bash
make
```
Builds `words.h` and `out/takt1ccname-1.0-os1.54.elemod`.

### Run Automated Tests
```bash
make test
```
Executes the Unicorn CPU emulation test suite verifying descriptor redirection, track switching, word picker scrolling, and 6-bit kitstore project & reboot persistence.

### Build Combined Flashable Firmware
```bash
make syx
```
Creates `out/Digitakt_OS1.54_ccname.syx` (Version `DCCN`).

To build the full bundle with all 10 `digi1_mods`:
```bash
PYTHONPATH=elekloader digiemu/.venv/bin/python -m elekloader.patch \
    --stock ~/Downloads/Digitakt_OS1.54.syx \
    --mod elekloader/mods/core/out/core-3.0-os1.54.elemod \
    --mod digi1_mods/elemods/digichain-1.6-os1.54.elemod \
    --mod digi1_mods/elemods/digimono-0.22-os1.54.elemod \
    --mod digi1_mods/elemods/digipoly-2.0a-os1.54.elemod \
    --mod digi1_mods/elemods/digimatrix-1.0c-os1.54.elemod \
    --mod digi1_mods/elemods/digieq-1.0c-os1.54.elemod \
    --mod digi1_mods/elemods/digiutils-1.9a-os1.54.elemod \
    --mod digi1_mods/elemods/digihealth-1.0-os1.54.elemod \
    --mod digi1_mods/elemods/digifilter-1.0i-os1.54.elemod \
    --mod digi1_mods/elemods/digineighbor-0.6-chain-os1.54.elemod \
    --mod out/takt1ccname-1.0-os1.54.elemod \
    --version DGAL \
    --out out/Digitakt_OS1.54_full_digi1mods_ccname.syx
```

### Flashing to Hardware
1. Connect your Digitakt to your computer via USB.
2. On the Digitakt, navigate to **`[SETTINGS] > SYSTEM > OS UPGRADE`** ("Waiting for SysEx").
3. Send the `.syx` file using `amidi` (Linux) or **Elektron Transfer**:
   ```bash
   amidi -p hw:7,0,0 -s out/Digitakt_OS1.54_full_digi1mods_ccname.syx
   ```
4. The Digitakt will verify the package, write to flash, and reboot automatically.

---

## Compatibility with digi1_mods

`takt1ccname` is built and verified to coexist harmoniously with the entire `digi1_mods` suite:
- **Digi Poly:** Coexists without conflicts.
- **Digi Mono & Digi Macro:** Preserves all custom synth machine pages.
- **Digi Matrix & Digi EQ:** Zero storage or memory conflicts.
- **Digi Utilities:** Preserves scope, tuner, and spectrum analyser views on the `...` key.
- **digichain:** Combines cleanly in elekloader builds.

---

## License

GPL-2.0-or-later
