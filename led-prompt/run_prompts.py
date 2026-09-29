"""Prompt with the breadboard LEDs and log each response to a text file.

Lights each colour's prompt LED in turn and waits for its button. Every
button press while a prompt is lit gets one line in the log, stamped with
this computer's clock:

    2026-09-29T17:40:12.345+10:00 prompted=blue pressed=blue match
    2026-09-29T17:40:15.020+10:00 prompted=red pressed=yellow wrong
    2026-09-29T17:40:45.020+10:00 prompted=red pressed=none timeout

A wrong press leaves the prompt lit; only the matching button clears it.

Run with:  uv run run_prompts.py [--port /dev/ttyACM0] [--rounds 3] [--log prompts.txt]
"""

import argparse
import sys
import time
from datetime import datetime
from pathlib import Path

import serial

COLOURS = ["red", "yellow", "blue"]


def stamp() -> str:
    return datetime.now().astimezone().isoformat(timespec="milliseconds")


def wait_for_ready(port: serial.Serial) -> None:
    """Opening the port can restart the board; give it a moment to report in."""
    deadline = time.monotonic() + 4
    while time.monotonic() < deadline:
        line = port.readline().decode(errors="replace").strip()
        if line.startswith("READY"):
            print(f"board: {line}")
            if "=HIGH" in line:
                sys.exit("a button reads pressed while idle; check its wiring")
            return


def prompt(port: serial.Serial, colour: str, timeout: float, log) -> None:
    def record(pressed: str, result: str) -> None:
        entry = f"{stamp()} prompted={colour} pressed={pressed} {result}"
        log.write(entry + "\n")
        log.flush()
        print(entry)

    port.write(f"ON {colour}\n".encode())
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        line = port.readline().decode(errors="replace").strip()
        if line.startswith("ACK "):
            record(line.split()[1], "match")
            return
        if line.startswith("WRONG "):
            record(line.split()[1], "wrong")
    port.write(b"OFF\n")
    record("none", "timeout")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--port", default="/dev/ttyACM0")
    parser.add_argument("--rounds", type=int, default=3)
    parser.add_argument("--timeout", type=float, default=30, help="seconds to wait per prompt")
    parser.add_argument("--gap", type=float, default=1, help="seconds between prompts")
    parser.add_argument("--log", type=Path, default=Path("prompts.txt"))
    args = parser.parse_args()

    print(f"logging to {args.log.resolve()}  (Ctrl+C to stop)")
    with serial.Serial(args.port, 115200, timeout=0.05) as port, args.log.open("a", encoding="utf-8") as log:
        wait_for_ready(port)
        try:
            for _ in range(args.rounds):
                for colour in COLOURS:
                    prompt(port, colour, args.timeout, log)
                    time.sleep(args.gap)
        finally:
            port.write(b"OFF\n")


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
