#!/usr/bin/env python3
"""
Live serial viewer for weather-station (Fine Offset / CC1101) reception.

Highlights the events that actually matter for judging reception quality, so
they're visible at a glance instead of scrolling past in a wall of text:

  - green  : a real decode succeeded
  - yellow : a candidate signal was captured but failed to decode (checksum
             error, short/truncated package, or an unparsed signal)
  - cyan   : housekeeping (RSSI calibration, boot messages)
  - dim    : everything else, unmodified

Usage:
    python tools/watch_fineoffset_serial.py [COM_PORT] [BAUD]

Defaults to COM3 / 115200 if not given. Ctrl+C to quit.
"""
import re
import sys
import time

import serial

PORT = sys.argv[1] if len(sys.argv) > 1 else "COM3"
BAUD = int(sys.argv[2]) if len(sys.argv) > 2 else 115200

RESET = "\033[0m"
GREEN = "\033[92m"
YELLOW = "\033[93m"
CYAN = "\033[96m"
DIM = "\033[2m"
BOLD = "\033[1m"

SUCCESS_RE = re.compile(r'"model"\s*:\s*"Fineoffset-(WS69|WH65B)"')
FAIL_RE = re.compile(
    r"Checksum error|short package|abort length|undecoded signal|Unparsed Signal",
    re.IGNORECASE,
)
HOUSEKEEPING_RE = re.compile(r"RSSI Signal|WiFi|Sensor ID", re.IGNORECASE)


def colorize(line: str) -> str:
    if SUCCESS_RE.search(line):
        return f"{BOLD}{GREEN}>>> {line}{RESET}"
    if FAIL_RE.search(line):
        return f"{YELLOW}{line}{RESET}"
    if HOUSEKEEPING_RE.search(line):
        return f"{CYAN}{line}{RESET}"
    return f"{DIM}{line}{RESET}"


def main():
    print(f"{BOLD}Watching {PORT} @ {BAUD} -- Ctrl+C to quit{RESET}")
    print(f"{BOLD}{GREEN}green{RESET} = real decode   "
          f"{YELLOW}yellow{RESET} = capture attempt failed   "
          f"{CYAN}cyan{RESET} = housekeeping\n")

    while True:
        try:
            ser = serial.Serial(PORT, BAUD, timeout=0.5)
            break
        except serial.SerialException as e:
            print(f"Could not open {PORT}: {e} -- retrying in 2s")
            time.sleep(2)

    try:
        while True:
            line = ser.readline()
            if not line:
                continue
            try:
                text = line.decode("utf-8", errors="replace").rstrip()
            except Exception:
                text = repr(line)
            if text:
                print(colorize(text))
    except KeyboardInterrupt:
        print(f"\n{BOLD}Stopped.{RESET}")
    finally:
        ser.close()


if __name__ == "__main__":
    main()
