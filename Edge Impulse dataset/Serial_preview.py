import sys
import time
import numpy as np
import cv2
import serial
import serial.tools.list_ports

BAUD_RATE   = 115200
FRAME_START = bytes([0xFF, 0xAA])
FRAME_END   = bytes([0xFF, 0xBB])
DISPLAY_SZ  = 256 

lower_skin = np.array([0,  20, 70],  dtype=np.uint8)
upper_skin = np.array([20, 255, 255], dtype=np.uint8)

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

def request_frame(ser):
    length_bytes = ser.read(4)

    if len(length_bytes) != 4:
        return None

    frame_len = int.from_bytes(length_bytes, "little")

    jpg = ser.read(frame_len)

    if len(jpg) != frame_len:
        return None

    frame = cv2.imdecode(
        np.frombuffer(jpg, np.uint8),
        cv2.IMREAD_COLOR
    )

    return frame

def preprocess(frame):
    # Convert to HSV
    hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)

    # Create skin mask
    mask = cv2.inRange(hsv, lower_skin, upper_skin)

    # Clean up
    kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (5, 5))
    mask   = cv2.morphologyEx(mask, cv2.MORPH_OPEN,  kernel)
    mask   = cv2.morphologyEx(mask, cv2.MORPH_CLOSE, kernel)
    mask   = cv2.dilate(mask, kernel, iterations=2)

    # Resize to 96x96
    image  = cv2.resize(mask, (96, 96))

    return image

def main() -> None:
    port = sys.argv[1] if len(sys.argv) > 1 else find_arduino_port()
    print(f"Connecting to {port} at {BAUD_RATE} baud rate.")

    ser = serial.Serial(port, BAUD_RATE, timeout=2)
    ser.reset_input_buffer()

    # wait for Arduino ready signal
    print("Waiting for Arduino ... ", end="", flush=True)
    deadline = time.monotonic() + 5.0
    while time.monotonic() < deadline:
        line = ser.readline().decode(errors="ignore").strip()
        if line == "RDY":
            break
    print("ready.")

    while True:
        frame = request_frame(ser)

        if frame is None:
            print("DECODE FAILED")
            continue

        mask = preprocess(frame)

        masked_display = cv2.resize(mask, (320, 320), interpolation=cv2.INTER_NEAREST)
        frame_display = cv2.resize(frame, (320, 320), interpolation=cv2.INTER_NEAREST)

        if len(masked_display.shape) == 2:
            masked_display = cv2.cvtColor(masked_display, cv2.COLOR_GRAY2BGR)

        if len(frame_display.shape) == 2:
            frame_display = cv2.cvtColor(frame_display, cv2.COLOR_GRAY2BGR)

        combined = np.hstack((masked_display, frame_display))

        cv2.imshow("ESP32-CAM", combined)

        key = cv2.waitKey(1)
        if key == ord('q') or key == 27:
            break
        elif key == ord('s'):
            # Sample skin color from center of frame
            h, w = frame.shape[:2]
            cx, cy = w // 2, h // 2
            hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
            roi = hsv[cy-20:cy+20, cx-20:cx+20]
            mean_hsv = cv2.mean(roi)[:3]
            lower_skin = np.array([
                max(0,   mean_hsv[0] - 10),
                max(0,   mean_hsv[1] - 40),
                max(0,   mean_hsv[2] - 60)
            ], dtype=np.uint8)
            upper_skin = np.array([
                min(179, mean_hsv[0] + 10),
                255,
                255
            ], dtype=np.uint8)
            print(f"HSV range: {lower_skin} - {upper_skin}")

if __name__ == "__main__":
    main()