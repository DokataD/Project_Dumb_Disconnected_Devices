"""
camera_preview.py — Live camera feed with preprocessing preview.

Left  : raw camera feed (colour)
Right : preprocessed 64×64 (grayscale → hist-eq → Otsu) scaled up for display

Controls:
  Q / ESC     quit
  UP arrow    increase threshold bias  (+5)
  DOWN arrow  decrease threshold bias  (-5)
  S           save a snapshot pair to ./snapshots/

Dependencies:
  pip install opencv-python numpy
"""

import os
import time
import numpy as np
import cv2

# ── import preprocessing functions from the existing script ─────────────────
from Preprocessor import histogram_equalize, otsu_threshold

# ── settings ────────────────────────────────────────────────────────────────
CAMERA_INDEX   = 0      # change if you have multiple cameras
DISPLAY_WIDTH  = 640    # width of each panel (total window = 2×)
DISPLAY_HEIGHT = 480
PROC_SIZE      = 64     # must match Arduino / training pipeline
BIAS_STEP      = 5      # how much UP/DOWN arrow changes bias
SNAPSHOT_DIR   = "snapshots"

# ── preprocessing ────────────────────────────────────────────────────────────

def preprocess_frame(frame: np.ndarray, bias: int) -> np.ndarray:
    """Convert a BGR camera frame → 64×64 binary, return display-sized copy."""
    gray    = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    small   = cv2.resize(gray, (PROC_SIZE, PROC_SIZE), interpolation=cv2.INTER_AREA)
    eq      = histogram_equalize(small)
    binary  = otsu_threshold(eq, bias=bias)
    # scale back up for display (nearest-neighbour keeps the crisp binary look)
    big     = cv2.resize(binary, (DISPLAY_WIDTH, DISPLAY_HEIGHT),
                         interpolation=cv2.INTER_NEAREST)
    return big


# ── overlay helpers ──────────────────────────────────────────────────────────

def draw_overlay(canvas: np.ndarray, bias: int, fps: float) -> None:
    """Burn bias value and FPS into the top-left of the canvas."""
    font  = cv2.FONT_HERSHEY_SIMPLEX
    color = (0, 255, 0)
    cv2.putText(canvas, f"bias: {bias:+d}  (UP/DOWN to adjust)",
                (10, 28), font, 0.7, (0, 0, 0), 3, cv2.LINE_AA)
    cv2.putText(canvas, f"bias: {bias:+d}  (UP/DOWN to adjust)",
                (10, 28), font, 0.7, color,   1, cv2.LINE_AA)
    cv2.putText(canvas, f"FPS: {fps:.1f}",
                (10, 56), font, 0.7, (0, 0, 0), 3, cv2.LINE_AA)
    cv2.putText(canvas, f"FPS: {fps:.1f}",
                (10, 56), font, 0.7, color,   1, cv2.LINE_AA)

def draw_labels(canvas: np.ndarray, left_w: int) -> None:
    """Panel labels at the top."""
    font  = cv2.FONT_HERSHEY_SIMPLEX
    for text, x in [("Raw", 10), ("Preprocessed", left_w + 10)]:
        cv2.putText(canvas, text, (x, DISPLAY_HEIGHT - 10),
                    font, 0.8, (0, 0, 0), 3, cv2.LINE_AA)
        cv2.putText(canvas, text, (x, DISPLAY_HEIGHT - 10),
                    font, 0.8, (255, 255, 255), 1, cv2.LINE_AA)


# ── main loop ────────────────────────────────────────────────────────────────

def main() -> None:
    cap = cv2.VideoCapture(CAMERA_INDEX)
    if not cap.isOpened():
        raise RuntimeError(f"Cannot open camera index {CAMERA_INDEX}")

    cap.set(cv2.CAP_PROP_FRAME_WIDTH,  DISPLAY_WIDTH)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, DISPLAY_HEIGHT)

    bias        = 20
    prev_time   = time.time()
    snapshot_n  = 0

    print("Camera preview started.")
    print("  UP / DOWN  — adjust threshold bias")
    print("  S          — save snapshot")
    print("  Q / ESC    — quit")

    while True:
        ret, frame = cap.read()
        if not ret:
            print("Failed to grab frame — exiting.")
            break

        # resize raw frame to fixed display size
        raw_display = cv2.resize(frame, (DISPLAY_WIDTH, DISPLAY_HEIGHT))

        # preprocess
        proc_gray   = preprocess_frame(frame, bias)
        proc_bgr    = cv2.cvtColor(proc_gray, cv2.COLOR_GRAY2BGR)

        # compose side-by-side canvas
        canvas = np.hstack([raw_display, proc_bgr])

        # FPS
        now       = time.time()
        fps       = 1.0 / max(now - prev_time, 1e-6)
        prev_time = now

        draw_overlay(canvas, bias, fps)
        draw_labels(canvas, DISPLAY_WIDTH)

        cv2.imshow("Hand Gesture Preprocessor", canvas)

        key = cv2.waitKey(1) & 0xFF
        if key in (ord("d"), 27):
            break
        elif key == ord("w"):
            bias += BIAS_STEP
        elif key == ord("s"):
            bias -= BIAS_STEP
        elif key == ord("a"):
            os.makedirs(SNAPSHOT_DIR, exist_ok=True)
            path = os.path.join(SNAPSHOT_DIR, f"snap_{snapshot_n:04d}.png")
            cv2.imwrite(path, canvas)
            snapshot_n += 1
            print(f"Saved {path}")

    cap.release()
    cv2.destroyAllWindows()


if __name__ == "__main__":
    main()