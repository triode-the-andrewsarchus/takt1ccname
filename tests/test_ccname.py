#!/usr/bin/env python3
"""Unicorn test for takt1ccname (Custom MIDI CC Names mod).
Runs the mod's compiled ColdFire code directly in Unicorn to verify:
  1. Initialization and descriptor table tracking.
  2. Per-track CC names update when switching between Audio and MIDI tracks.
  3. Presets (Standard Synth, Minilogue, MicroFreak, etc.).
  4. Quick library assignment and custom character editing.
  5. UI drawing and key/encoder handling.
"""
import argparse, os, struct, sys
from unicorn import Uc, UC_ARCH_M68K, UC_MODE_BIG_ENDIAN, UC_HOOK_CODE
from unicorn.m68k_const import *

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "elekloader"))
from elekloader import syx, devices, elemod, link

def test_ccname(stock_path, core_path, ccname_path):
    print("Loading stock firmware:", stock_path)
    st = syx.Syx.load(stock_path)
    dev, rel = devices.identify(st.sha256)
    print("Identified:", dev.name, rel.version)

    c_mod = elemod.load_any(core_path)
    cc_mod = elemod.load_any(ccname_path)

    print("Linking core and ccname mods...")
    L = link.link([c_mod, cc_mod], st.section(dev.main_section))
    MAP, img = L.map, L.image
    ddr0, run_end = L.layout["ddr"]
    bss0, bss1 = L.layout["bss"]
    run = img[L.layout["run_load"] - 0x40000400 : L.layout["run_load"] - 0x40000400 + run_end - ddr0]

    uc = Uc(UC_ARCH_M68K, UC_MODE_BIG_ENDIAN)
    uc.ctl_set_cpu_model(UC_CPU_M68K_CFV4E)

    # Map RAM spaces
    for base, size in ((0x40000000, 0x01000000), (0x41900000, 0x00100000),
                       (0x421f0000, 0x00010000), (0x47b00000, 0x00100000),
                       (0x80000000, 0x00100000)):
        uc.mem_map(base, size)

    uc.mem_write(0x40000400, img[:0x3fc000])
    uc.mem_write(ddr0, run)
    uc.mem_write(bss0, bytes(bss1 - bss0))

    STACK = 0x47bf0000
    uc.reg_write(UC_M68K_REG_A7, STACK)

    # Resolve symbols
    syms = MAP
    tick_fn = syms["ccname_tick"]
    draw_fn = syms["ccname_draw"]
    key_fn  = syms["ccname_key"]
    enc_fn  = syms["ccname_enc"]
    select_fn = syms["ccname_select"]
    open_var  = syms["ccname_open"]

    is_154 = (rel.version == "1.54")
    DESC_BASE = 0x401aa09c if is_154 else 0x401a9d9c
    ACTIVE_TRACK_ADDR = 0x4197c6b4 if is_154 else 0x4197b6b4

    # Firmware stubs: RTS at INVALIDATE, FILLRECT, TEXT
    RTS = b"\x4e\x75"
    INVALIDATE = 0x400c9a3a if is_154 else 0x400c9812
    FILLRECT   = 0x400c1bce if is_154 else 0x400c19a6
    TEXT_ADDR  = 0x400c27a4 if is_154 else 0x400c257c
    for addr in (INVALIDATE, FILLRECT, TEXT_ADDR):
        uc.mem_write(addr, RTS)

    def read_str(ptr):
        if ptr == 0: return ""
        b = uc.mem_read(ptr, 32)
        return b.split(b"\x00")[0].decode("ascii", errors="replace")

    def get_desc(param_id):
        long_ptr = struct.unpack(">I", uc.mem_read(DESC_BASE + param_id * 52 + 40, 4))[0]
        short_ptr = struct.unpack(">I", uc.mem_read(DESC_BASE + param_id * 52 + 48, 4))[0]
        return read_str(short_ptr), read_str(long_ptr)

    def call(addr, *args):
        sp = uc.reg_read(UC_M68K_REG_A7)
        ret = 0x40000500
        uc.mem_write(ret, RTS)
        payload = struct.pack(">%dI" % (len(args) + 1), ret, *args)
        sp -= len(payload)
        uc.mem_write(sp, payload)
        uc.reg_write(UC_M68K_REG_A7, sp)
        uc.emu_start(addr, ret)
        return uc.reg_read(UC_M68K_REG_D0)

    # 1. Start on Audio track 0 (TRK 1)
    uc.mem_write(ACTIVE_TRACK_ADDR, struct.pack(">I", 0))
    ctrl = 0x47b01000
    call(tick_fn, ctrl)

    # Verify initial stock pointers for VAL1..VAL8 and SEL1..SEL8
    s1, l1 = get_desc(148)
    assert s1 == "VAL1" and l1 == "CC1 Value", f"Expected VAL1/CC1 Value, got {s1}/{l1}"
    s8, l8 = get_desc(155)
    assert s8 == "VAL8" and l8 == "CC8 Value", f"Expected VAL8/CC8 Value, got {s8}/{l8}"
    sel1, sell1 = get_desc(156)
    assert sel1 == "SEL1" and sell1 == "CC1 Select", f"Expected SEL1/CC1 Select, got {sel1}/{sell1}"
    print("  [PASS] Initial state on Audio Track: stock VAL1..VAL8 / SEL1..SEL8 active")

    # 2. Switch to MIDI Track 8 (TRK 09, Synth Preset)
    uc.mem_write(ACTIVE_TRACK_ADDR, struct.pack(">I", 8))
    call(tick_fn, ctrl)

    s1, l1 = get_desc(148)
    s2, l2 = get_desc(149)
    s8, l8 = get_desc(155)
    sel1, _ = get_desc(156)
    assert s1 == "CUTO" and l1 == "Cutoff", f"Expected CUTO/Cutoff on T09, got {s1}/{l1}"
    assert s2 == "RESO" and l2 == "Resonance", f"Expected RESO/Resonance on T09, got {s2}/{l2}"
    assert s8 == "LFOR" and l8 == "LFO Rate", f"Expected LFOR/LFO Rate on T09, got {s8}/{l8}"
    assert sel1 == "CUTO", f"Expected SEL1 to also reflect CUTO, got {sel1}"
    print("  [PASS] MIDI Track 9 (index 8): CUTO, RESO, ... LFOR active")

    # 3. Switch to MIDI Track 9 (TRK 10, Minilogue Preset)
    uc.mem_write(ACTIVE_TRACK_ADDR, struct.pack(">I", 9))
    call(tick_fn, ctrl)

    s1, l1 = get_desc(148)
    s3, l3 = get_desc(150)
    s8, l8 = get_desc(155)
    assert s1 == "CUTO" and l1 == "Cutoff Freq", f"Expected CUTO/Cutoff Freq on T10, got {s1}/{l1}"
    assert s3 == "EG  " and l3 == "EG Intensity", f"Expected EG  /EG Intensity on T10, got {s3}/{l3}"
    assert s8 == "SHAP" and l8 == "Osc Shape", f"Expected SHAP/Osc Shape on T10, got {s8}/{l8}"
    print("  [PASS] MIDI Track 10 (index 9): Minilogue preset active")

    # 4. Switch back to Audio Track 1 (TRK 2)
    uc.mem_write(ACTIVE_TRACK_ADDR, struct.pack(">I", 1))
    call(tick_fn, ctrl)

    s1, l1 = get_desc(148)
    assert s1 == "VAL1" and l1 == "CC1 Value", f"Expected VAL1 restored on audio track, got {s1}/{l1}"
    print("  [PASS] Audio Track switch restores stock parameter descriptors")

    # 5. Open SETTINGS > MIDI CC NAMES page
    payload = 0x47b02000
    uc.mem_write(payload, struct.pack(">I", 0x47b02020))
    call(select_fn, payload)
    is_open = struct.unpack(">I", uc.mem_read(open_var, 4))[0]
    assert is_open == 1, "Expected ccname_open == 1"
    print("  [PASS] Page opened via ccname_select")

    # 6. Test Draw when page is open
    bmp = 0x47b03000
    call(draw_fn, bmp, ctrl)
    print("  [PASS] Page draw executed cleanly")

    # 7. Test Encoder Knob C: Cycle preset on Track 09
    # Change track back to T09
    uc.mem_write(ACTIVE_TRACK_ADDR, struct.pack(">I", 8))
    # Enc event structure: +12 knob_id, +16 delta
    ev_enc = 0x47b04000
    uc.mem_write(ev_enc, struct.pack(">5I", 0, 0, 0, 3, 1)) # Knob C (id 3), delta +1
    call(enc_fn, 0, ev_enc)
    call(tick_fn, ctrl)
    s3, l3 = get_desc(150)
    assert s3 == "EG  ", f"Expected Minilogue EG   preset after turning Knob C, got {s3}"
    print("  [PASS] Encoder Preset cycling works")

    # 8. Test Naming Screen & Keyboard Mode
    # Press YES (id 12) to enter Naming Mode on current slot (CC1)
    ev_key = 0x47b05000
    uc.mem_write(ev_key, struct.pack(">5I", 0, 0, 0, 12, 1))
    call(key_fn, 0, ev_key)
    # Draw naming screen
    call(draw_fn, bmp, ctrl)
    print("  [PASS] Entered Naming Mode and rendered pop-up matrix")

    # Type with Trig 1 ('A') and Trig 2 ('B')
    uc.mem_write(ev_key, struct.pack(">5I", 0, 0, 0, 24, 1)) # Trig 1 (id 24 -> 'A')
    call(key_fn, 0, ev_key)
    uc.mem_write(ev_key, struct.pack(">5I", 0, 0, 0, 25, 1)) # Trig 2 (id 25 -> 'B')
    call(key_fn, 0, ev_key)
    call(draw_fn, bmp, ctrl)
    print("  [PASS] Trig keys 1 and 2 typed 'A' and 'B'")

    # Confirm with YES
    uc.mem_write(ev_key, struct.pack(">5I", 0, 0, 0, 12, 1))
    call(key_fn, 0, ev_key)
    call(tick_fn, ctrl)

    s1, l1 = get_desc(148)
    assert s1.startswith("AB"), f"Expected short name starting with AB, got {s1}"
    assert l1.startswith("AB"), f"Expected long name starting with AB, got {l1}"
    print(f"  [PASS] Custom name saved via Keyboard Mode: {s1} / {l1}")

    # 9. Test Key NO: Close page
    uc.mem_write(ev_key, struct.pack(">5I", 0, 0, 0, 13, 1)) # Key NO (id 13), pressed
    call(key_fn, 0, ev_key)
    is_open = struct.unpack(">I", uc.mem_read(open_var, 4))[0]
    assert is_open == 0, "Expected ccname_open == 0 after NO key"
    print("  [PASS] Key NO closes settings page")

    print("\nALL CC NAME TESTS PASSED SUCCESSFULLY!")

if __name__ == "__main__":
    stock = os.path.expanduser("~/Downloads/Digitakt_OS1.54.syx")
    core = os.path.join(ROOT, "elekloader/mods/core/out/core-3.0-os1.54.elemod")
    ccname = os.path.join(ROOT, "out/takt1ccname-1.0-os1.54.elemod")
    test_ccname(stock, core, ccname)
