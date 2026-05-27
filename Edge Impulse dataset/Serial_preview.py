"""
serial_preview.py — Live viewer for preprocessed frames streamed from Arduino.

Request-response model: Python requests each frame individually,
so the Arduino always captures fresh and the serial buffer never fills up.

Controls:
  Q / ESC     quit
  S           save snapshot to ./snapshots/

Dependencies:
  pip install pyserial opencv-python numpy

Usage:
  python serial_preview.py              # auto-detects first Arduino port
  python serial_preview.py COM3         # Windows
  python serial_preview.py /dev/ttyACM0 # Linux / Mac
"""

import sys
import os
import time
import numpy as np
import cv2
import serial
import serial.tools.list_ports

# ── settings ────────────────────────────────────────────────────────────────
BAUD_RATE     = 2000000  # must match gesture_stream.ino
FRAME_W       = 64
FRAME_H       = 64
FRAME_SZ      = FRAME_W * FRAME_H
DISPLAY_SCALE = 8        # 64 × 8 = 512px window
SNAPSHOT_DIR  = "snapshots"
BIAS_STEP     = 5        # must match gesture_stream.ino

FRAME_START = bytes([0xFF, 0xAA])
FRAME_END   = bytes([0xFF, 0xBB])

DISPLAY_W = FRAME_W * DISPLAY_SCALE
DISPLAY_H = FRAME_H * DISPLAY_SCALE


# ── port detection ───────────────────────────────────────────────────────────

def find_arduino_port() -> str:
    ports = serial.tools.list_ports.comports()
    for p in ports:
        desc = (p.description or "").lower()
        mfr  = (p.manufacturer or "").lower()
        if any(k in desc or k in mfr for k in ("arduino", "nano", "genuino", "ch340", "ftdi")):
            return p.device
    if ports:
        return ports[0].device
    raise RuntimeError("No serial port found.")


# ── frame request / receive ──────────────────────────────────────────────────

def send_bias(ser: serial.Serial, direction: str) -> int | None:
    """Send '+' or '-' to Arduino, return the confirmed bias value."""
    ser.reset_input_buffer()
    ser.write(direction.encode())
    deadline = time.monotonic() + 1.0
    while time.monotonic() < deadline:
        line = ser.readline().decode(errors="ignore").strip()
        if line.startswith("BIAS:"):
            try:
                return int(line.split(":")[1])
            except ValueError:
                pass
    return None


def request_frame(ser: serial.Serial) -> tuple[np.ndarray | None, str, float]:
    """
    Request one frame + inference result.
    Returns (frame_array, label, score) or (None, "", 0.0) on failure.
    """
    ser.reset_input_buffer()
    ser.write(b'f')

    # wait for start marker
    buf = b""
    deadline = time.monotonic() + 2.0
    while time.monotonic() < deadline:
        byte = ser.read(1)
        if not byte:
            continue
        buf = (buf + byte)[-2:]
        if buf == FRAME_START:
            break
    else:
        return None, "", 0.0

    data = ser.read(FRAME_SZ)
    if len(data) != FRAME_SZ:
        return None, "", 0.0

    end = ser.read(2)
    if end != FRAME_END:
        return None, "", 0.0

    frame = np.frombuffer(data, dtype=np.uint8).reshape((FRAME_H, FRAME_W))

    # read the LABEL line that follows the frame
    label, score = "…", 0.0
    try:
        line = ser.readline().decode(errors="ignore").strip()
        if line.startswith("LABEL:"):
            parts = line.split(",")
            label = parts[0].split(":")[1]
            score = float(parts[1].split(":")[1])
    except (IndexError, ValueError):
        pass

    return frame, label, score


# ── overlay ──────────────────────────────────────────────────────────────────

def draw_overlay(canvas: np.ndarray, fps: float, port: str,
                 bias: int, label: str, score: float) -> None:
    font  = cv2.FONT_HERSHEY_SIMPLEX
    green = (0, 255, 0)
    lines = [
        f"FPS: {fps:.1f}",
        f"Port: {port}  @{BAUD_RATE//1000}k baud",
        f"Res: {FRAME_W}x{FRAME_H}  (x{DISPLAY_SCALE})",
        f"Bias: {bias:+d}  (UP/DOWN to adjust)",
        "S: snapshot   Q/ESC: quit",
    ]
    for i, text in enumerate(lines):
        y = 24 + i * 26
        cv2.putText(canvas, text, (10, y), font, 0.65, (0, 0, 0), 3, cv2.LINE_AA)
        cv2.putText(canvas, text, (10, y), font, 0.65, green,     1, cv2.LINE_AA)

    # large label + score at the bottom
    bar_w = int(score * (DISPLAY_W - 20))
    cv2.rectangle(canvas, (10, DISPLAY_H - 36), (10 + bar_w, DISPLAY_H - 16),
                  (0, 200, 0), -1)
    cv2.rectangle(canvas, (10, DISPLAY_H - 36), (DISPLAY_W - 10, DISPLAY_H - 16),
                  green, 1)
    label_text = f"{label}  {score*100:.1f}%"
    cv2.putText(canvas, label_text, (10, DISPLAY_H - 44),
                font, 1.0, (0, 0, 0), 4, cv2.LINE_AA)
    cv2.putText(canvas, label_text, (10, DISPLAY_H - 44),
                font, 1.0, (255, 255, 255), 2, cv2.LINE_AA)


# ── main ─────────────────────────────────────────────────────────────────────

def main() -> None:
    port = sys.argv[1] if len(sys.argv) > 1 else find_arduino_port()
    print(f"Connecting to {port} at {BAUD_RATE} baud …")

    ser = serial.Serial(port, BAUD_RATE, timeout=2)
    ser.reset_input_buffer()

    # wait for Arduino ready signal
    print("Waiting for Arduino … ", end="", flush=True)
    deadline = time.monotonic() + 5.0
    while time.monotonic() < deadline:
        line = ser.readline().decode(errors="ignore").strip()
        if line == "RDY":
            break
    print("ready.")

    prev_time  = time.time()
    fps        = 0.0
    snapshot_n = 0
    bias       = 20   # must match BIAS_DEFAULT in gesture_stream.ino
    label      = "…"
    score      = 0.0

    print("Streaming.  UP/DOWN to adjust bias, Q/ESC to quit, S to snapshot.")

    while True:
        frame, label, score = request_frame(ser)
        if frame is None:
            print("Frame timeout — retrying …")
            continue

        display = cv2.resize(frame, (DISPLAY_W, DISPLAY_H),
                             interpolation=cv2.INTER_NEAREST)
        canvas  = cv2.cvtColor(display, cv2.COLOR_GRAY2BGR)

        now       = time.time()
        fps       = 0.9 * fps + 0.1 / max(now - prev_time, 1e-6)
        prev_time = now

        draw_overlay(canvas, fps, port, bias, label, score)
        cv2.imshow("Arduino Gesture Stream  |  Q to quit", canvas)

        key = cv2.waitKey(1) & 0xFF
        if key in (ord("q"), 27):
            break
        elif key == ord("s"):
            os.makedirs(SNAPSHOT_DIR, exist_ok=True)
            path = os.path.join(SNAPSHOT_DIR, f"snap_{snapshot_n:04d}.png")
            cv2.imwrite(path, canvas)
            snapshot_n += 1
            print(f"Saved {path}")
        elif key in (82, 0):    # UP arrow
            confirmed = send_bias(ser, '+')
            if confirmed is not None:
                bias = confirmed
                print(f"bias → {bias:+d}")
        elif key in (84, 1):    # DOWN arrow
            confirmed = send_bias(ser, '-')
            if confirmed is not None:
                bias = confirmed
                print(f"bias → {bias:+d}")

    ser.close()
    cv2.destroyAllWindows()


if __name__ == "__main__":
    main()