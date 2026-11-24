#!/usr/bin/env python3
"""Interactive hardware test for SidechainCompressor (requires TEST_KNOBS_MODE)."""

import argparse
import glob
import os
import select
import sys
import termios
import time

# Knobs in requested test order (matches TK packet order from firmware)
KNOBS = [
    ("ratio", "Ratio"),
    ("ramp", "Ramp"),
    ("mix", "Mix"),
    ("threshold", "Threshold"),
    ("saturation", "Saturation"),
    ("cutoff", "Cutoff"),
    ("attack", "Attack"),
    ("release", "Release"),
    ("output", "Output"),
    ("width", "Width"),
]

MIN_THRESHOLD_DEFAULT = 0.05
MAX_THRESHOLD_DEFAULT = 0.95
STABLE_FRAMES = 2          # consecutive frames required to confirm a pass (~100ms)
OTHER_KNOB_WARN_DELTA = 0.15

# ANSI styling (auto-disabled if not a TTY)
USE_COLOR = sys.stdout.isatty()
GREEN = "\033[32m" if USE_COLOR else ""
YELLOW = "\033[33m" if USE_COLOR else ""
RED = "\033[31m" if USE_COLOR else ""
CYAN = "\033[36m" if USE_COLOR else ""
BOLD = "\033[1m" if USE_COLOR else ""
DIM = "\033[2m" if USE_COLOR else ""
RESET = "\033[0m" if USE_COLOR else ""
CLEAR_EOL = "\033[K" if USE_COLOR else ""


def select_port(explicit_port=None):
    if explicit_port:
        if not os.path.exists(explicit_port):
            print(f"{RED}Error: Serial port '{explicit_port}' not found.{RESET}", file=sys.stderr)
            sys.exit(1)
        return explicit_port

    ports = sorted(glob.glob("/dev/cu.usbmodem*"))
    if not ports:
        print(f"{RED}Error: No Daisy serial ports (/dev/cu.usbmodem*) found.{RESET}", file=sys.stderr)
        sys.exit(1)

    if len(ports) == 1:
        print(f"Connecting to: {ports[0]}")
        return ports[0]

    print("Multiple Daisy serial ports found:")
    for idx, port in enumerate(ports, start=1):
        print(f"  [{idx}] {port}")

    try:
        choice = input(f"Select a port (1-{len(ports)}): ").strip()
    except (EOFError, KeyboardInterrupt):
        print()
        sys.exit(1)

    if choice.isdigit() and 1 <= int(choice) <= len(ports):
        port = ports[int(choice) - 1]
        print(f"Connecting to: {port}")
        return port

    print(f"{RED}Error: Invalid selection.{RESET}", file=sys.stderr)
    sys.exit(1)


class SerialReader:
    """Minimal zero-dependency serial line reader using termios + select."""

    def __init__(self, port):
        self.port = port
        self.fd = None
        self._buf = bytearray()

    def open(self):
        self.fd = os.open(self.port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        attrs = termios.tcgetattr(self.fd)
        # Raw mode, 8N1, CLOCAL | CREAD, 115200 baud (USB CDC ignores baud, but set cleanly)
        attrs[0] = termios.IGNBRK
        attrs[1] = 0
        attrs[2] = (attrs[2] & ~(termios.CSIZE | termios.PARENB | termios.CSTOPB)) | termios.CS8 | termios.CLOCAL | termios.CREAD
        attrs[3] = 0
        attrs[4] = termios.B115200
        attrs[5] = termios.B115200
        attrs[6][termios.VMIN] = 0
        attrs[6][termios.VTIME] = 0
        termios.tcsetattr(self.fd, termios.TCSANOW, attrs)
        termios.tcflush(self.fd, termios.TCIFLUSH)

    def close(self):
        if self.fd is not None:
            try:
                os.close(self.fd)
            except OSError:
                pass
            self.fd = None

    def read_line(self, timeout=0.25):
        while True:
            nl = self._buf.find(b"\n")
            if nl != -1:
                raw = bytes(self._buf[:nl])
                del self._buf[: nl + 1]
                return raw.decode("ascii", errors="replace").strip()

            ready, _, _ = select.select([self.fd], [], [], timeout)
            if not ready:
                return None

            try:
                chunk = os.read(self.fd, 512)
            except BlockingIOError:
                return None
            except OSError as exc:
                raise ConnectionError(f"Serial read failed: {exc}") from exc

            if not chunk:
                raise ConnectionError("Serial port disconnected.")
            self._buf.extend(chunk)


def parse_tk_line(line):
    """Parse 'TK P p0..p9 B b0 b1 b2 S filt sat' -> dict or None."""
    parts = line.split()
    if len(parts) != 19:
        return None
    if parts[0] != "TK" or parts[1] != "P" or parts[12] != "B" or parts[16] != "S":
        return None

    try:
        pots = {}
        for i, (key, _) in enumerate(KNOBS):
            pots[key] = int(parts[2 + i]) / 1000.0
        buttons = {
            "footsw": int(parts[13]) != 0,
            "rampbtn": int(parts[14]) != 0,
            "clrsw": int(parts[15]) != 0,
        }
    except ValueError:
        return None

    switches = {
        "filt": parts[17],
        "satmode": parts[18],
    }
    return {"pots": pots, "buttons": buttons, "switches": switches}


def next_frame(reader):
    """Block until the next valid TK telemetry frame arrives."""
    debug_lines_seen = 0
    while True:
        line = reader.read_line(timeout=0.25)
        if line is None:
            continue
        frame = parse_tk_line(line)
        if frame is not None:
            return frame
        if line.startswith("T:") or line.startswith("Pre:") or "Sidechain Compressor Test" in line:
            debug_lines_seen += 1
            if debug_lines_seen >= 2:
                print(
                    f"\n{RED}Daisy appears to be running DEBUG_LOG instead of TEST_KNOBS_MODE.{RESET}\n"
                    f"Uncomment {BOLD}#define TEST_KNOBS_MODE 1{RESET} in TestSidechainCompressor.cpp, "
                    f"rebuild, and re-flash."
                )
                sys.exit(1)


def render_bar(val, width=20):
    filled = int(round(max(0.0, min(1.0, val)) * width))
    return "[" + ("█" * filled) + ("░" * (width - filled)) + "]"


def find_wrong_knob(frame, target_key, baseline):
    best_name = None
    best_delta = OTHER_KNOB_WARN_DELTA
    best_val = 0.0
    for key, label in KNOBS:
        if key == target_key:
            continue
        delta = abs(frame["pots"][key] - baseline[key])
        if delta > best_delta:
            best_delta = delta
            best_name = label
            best_val = frame["pots"][key]
    if best_name is not None:
        return f"  {YELLOW}(! {best_name} moving: {best_val:.3f}){RESET}"
    return ""


def test_knob_endpoint(reader, idx, total, key, label, target, thresh):
    frame = next_frame(reader)
    baseline = dict(frame["pots"])
    stable = 0

    if target == "min":
        goal_text = f"turn to {BOLD}MIN{RESET} (<= {thresh:.2f})"
        check = lambda v: v <= thresh
    else:
        goal_text = f"turn to {BOLD}MAX{RESET} (>= {thresh:.2f})"
        check = lambda v: v >= thresh

    while True:
        val = frame["pots"][key]
        warn = find_wrong_knob(frame, key, baseline)
        bar = render_bar(val)
        status = f"  [{idx:2d}/{total}] {CYAN}{label:<10}{RESET} — {goal_text}  {bar} {val:.3f}{warn}{CLEAR_EOL}"
        sys.stdout.write("\r" + status)
        sys.stdout.flush()

        if check(val):
            stable += 1
            if stable >= STABLE_FRAMES:
                sys.stdout.write(
                    f"\r  [{idx:2d}/{total}] {CYAN}{label:<10}{RESET} — "
                    f"{'MIN' if target == 'min' else 'MAX'} "
                    f"{GREEN}PASS{RESET} ({val:.3f}){CLEAR_EOL}\n"
                )
                sys.stdout.flush()
                return
        else:
            stable = 0

        frame = next_frame(reader)


def test_momentary_button(reader, key, label):
    frame = next_frame(reader)
    # If already held down at step start, wait for release first
    if frame["buttons"][key]:
        while frame["buttons"][key]:
            sys.stdout.write(
                f"\r  {CYAN}{label:<24}{RESET} — Release button first...{CLEAR_EOL}"
            )
            sys.stdout.flush()
            frame = next_frame(reader)

    # Wait for press
    stable = 0
    while True:
        pressed = frame["buttons"][key]
        sys.stdout.write(
            f"\r  {CYAN}{label:<24}{RESET} — Press button... "
            f"[{'PRESSED' if pressed else 'released'}]{CLEAR_EOL}"
        )
        sys.stdout.flush()
        if pressed:
            stable += 1
            if stable >= 1:
                break
        else:
            stable = 0
        frame = next_frame(reader)

    # Wait for release so we confirm it isn't stuck LOW
    while frame["buttons"][key]:
        sys.stdout.write(
            f"\r  {CYAN}{label:<24}{RESET} — Detected! Release button...{CLEAR_EOL}"
        )
        sys.stdout.flush()
        frame = next_frame(reader)

    sys.stdout.write(f"\r  {CYAN}{label:<24}{RESET} — {GREEN}PASS{RESET}{CLEAR_EOL}\n")
    sys.stdout.flush()


def test_latching_footswitch(reader, key, label):
    frame = next_frame(reader)
    initial = frame["buttons"][key]

    def state_name(s):
        return "ENGAGED" if s else "BYPASSED"

    # Toggle 1: move to opposite state
    target1 = not initial
    stable = 0
    while True:
        cur = frame["buttons"][key]
        sys.stdout.write(
            f"\r  {CYAN}{label:<24}{RESET} — Currently {BOLD}{state_name(initial)}{RESET}; "
            f"press to switch to {BOLD}{state_name(target1)}{RESET}...{CLEAR_EOL}"
        )
        sys.stdout.flush()
        if cur == target1:
            stable += 1
            if stable >= STABLE_FRAMES:
                break
        else:
            stable = 0
        frame = next_frame(reader)

    # Toggle 2: move back to initial state
    stable = 0
    while True:
        cur = frame["buttons"][key]
        sys.stdout.write(
            f"\r  {CYAN}{label:<24}{RESET} — Now {BOLD}{state_name(target1)}{RESET}; "
            f"press again to switch to {BOLD}{state_name(initial)}{RESET}...{CLEAR_EOL}"
        )
        sys.stdout.flush()
        if cur == initial:
            stable += 1
            if stable >= STABLE_FRAMES:
                break
        else:
            stable = 0
        frame = next_frame(reader)

    sys.stdout.write(
        f"\r  {CYAN}{label:<24}{RESET} — {GREEN}PASS{RESET} (both ENGAGED and BYPASSED verified){CLEAR_EOL}\n"
    )
    sys.stdout.flush()


def test_three_pos_switch(reader, key, label, positions):
    seen = set()
    last_pos = None
    stable = 0

    while len(seen) < len(positions):
        frame = next_frame(reader)
        cur = frame["switches"][key]

        if cur == last_pos:
            stable += 1
        else:
            last_pos = cur
            stable = 1

        if cur in positions and stable >= STABLE_FRAMES:
            seen.add(cur)

        checks = "  ".join(
            f"{GREEN}[✓] {p}{RESET}" if p in seen else f"[ ] {p}"
            for p in positions
        )
        err_note = f"  {RED}(! BOTH PINS ACTIVE — WIRING ERROR){RESET}" if cur == "ERR" else ""
        sys.stdout.write(
            f"\r  {CYAN}{label:<20}{RESET} — {checks}  {DIM}(current: {cur}){RESET}{err_note}{CLEAR_EOL}"
        )
        sys.stdout.flush()

    checks = "  ".join(f"{GREEN}[✓] {p}{RESET}" for p in positions)
    sys.stdout.write(
        f"\r  {CYAN}{label:<20}{RESET} — {checks}  {GREEN}PASS{RESET}{CLEAR_EOL}\n"
    )
    sys.stdout.flush()


def main():
    parser = argparse.ArgumentParser(
        description="Interactive knob, button, and switch test for SidechainCompressor."
    )
    parser.add_argument("port", nargs="?", help="Optional serial port path (default: auto-detect /dev/cu.usbmodem*)")
    parser.add_argument("--min-thresh", type=float, default=MIN_THRESHOLD_DEFAULT, help="Max raw value accepted as MIN (default: 0.05)")
    parser.add_argument("--max-thresh", type=float, default=MAX_THRESHOLD_DEFAULT, help="Min raw value accepted as MAX (default: 0.95)")
    args = parser.parse_args()

    port = select_port(args.port)
    reader = SerialReader(port)

    try:
        reader.open()
        print("Waiting for Daisy telemetry (TEST_KNOBS_MODE)...")
        next_frame(reader)
        print(f"{GREEN}Connected!{RESET} Starting hardware test.\n")

        # ── 1. Knobs ────────────────────────────────────────────────────────
        print(f"{BOLD}=== 1. Knobs ({len(KNOBS)}) ==={RESET}")
        for idx, (key, label) in enumerate(KNOBS, start=1):
            test_knob_endpoint(reader, idx, len(KNOBS), key, label, "min", args.min_thresh)
            test_knob_endpoint(reader, idx, len(KNOBS), key, label, "max", args.max_thresh)

        # ── 2. Buttons ──────────────────────────────────────────────────────
        print(f"\n{BOLD}=== 2. Buttons (3) ==={RESET}")
        test_momentary_button(reader, "rampbtn", "Mystery / Ramp Button")
        test_momentary_button(reader, "clrsw", "Clear / Boot Button")
        test_latching_footswitch(reader, "footsw", "Bypass Footswitch")

        # ── 3. 3-Position Switches ──────────────────────────────────────────
        print(f"\n{BOLD}=== 3. 3-Position Switches (2) ==={RESET}")
        print(f"  {DIM}Flip each switch through all 3 positions (in any order).{RESET}")
        test_three_pos_switch(reader, "filt", "Filter Switch", ["LPF", "BPF", "HPF"])
        test_three_pos_switch(reader, "satmode", "Saturation Switch", ["Soft", "Fold", "Ducker"])

        print(f"\n{GREEN}{BOLD}ALL HARDWARE TESTS PASSED!{RESET} (10 knobs, 3 buttons, 2 switches)")

    except KeyboardInterrupt:
        print(f"\n{YELLOW}Test cancelled.{RESET}")
        sys.exit(130)
    except ConnectionError as exc:
        print(f"\n{RED}Error: {exc}{RESET}", file=sys.stderr)
        sys.exit(1)
    finally:
        reader.close()


if __name__ == "__main__":
    main()
