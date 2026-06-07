import sys
import cv2
import serial
import numpy as np
import serial.tools.list_ports

WIDTH = 64
HEIGHT = 64
COUNT = WIDTH * HEIGHT
BAUD_RATE = 230400
LABEL_NAMES = ["go", "left", "reverse", "right", "stop"]

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

port = sys.argv[1] if len(sys.argv) > 1 else find_arduino_port()
print(f"Connecting to {port} at {BAUD_RATE} baud rate.")

ser = serial.Serial(port, BAUD_RATE, timeout=2)
ser.reset_input_buffer()

def wait_for_header():
    while True:
        b1 = ser.read(1)
        if b1 == b'\xAA':
            b2 = ser.read(1)
            if b2 == b'\x55':
                return
while True:
    # 1. sync to frame start
    wait_for_header()

    # 2. read image size
    raw_count = ser.read(4)
    if len(raw_count) != 4:
        continue

    count = int.from_bytes(raw_count, "little")

    if count != COUNT:
        continue

    # 3. read image
    raw = ser.read(count * 4)
    if len(raw) != count * 4:
        continue

    img = np.frombuffer(raw, dtype=np.float32).reshape((HEIGHT, WIDTH))

    # 4. read labels count
    n_labels_raw = ser.read(1)
    if len(n_labels_raw) != 1:
        continue

    n_labels = n_labels_raw[0]

    labels = []
    scores = []

    # 5. read scores
    for _ in range(n_labels):
        score_bytes = ser.read(4)
        idx_bytes = ser.read(1)

        if len(score_bytes) != 4 or len(idx_bytes) != 1:
            break

        score = np.frombuffer(score_bytes, dtype=np.float32)[0]
        idx = idx_bytes[0]

        labels.append(idx)
        scores.append(score)

    if len(scores) == 0:
        continue

    bluetooth_signal = ser.read(1)[0]
    uart_time = ser.read(4)[0]
    process_time = ser.read(4)[0]

    # 6. best prediction
    best_i = int(np.argmax(scores))

    # 7. visualization
    disp = np.clip(img * 255, 0, 255).astype(np.uint8)
    display = cv2.resize(disp, (320, 320))
    display = cv2.cvtColor(display, cv2.COLOR_GRAY2BGR)

    for i, (label_idx, score) in enumerate(zip(labels, scores)):
        label = LABEL_NAMES[label_idx]

        color = (0, 255, 0) if i == best_i else (0, 0, 255)

        cv2.putText(display, f"{label}: {score:.2f}", (10, 30 + i * 25), 
            cv2.FONT_HERSHEY_SIMPLEX, 0.5, color, 1, cv2.LINE_AA)
    
    cv2.putText(display, f"BLE signal: {bluetooth_signal}", (150, 30), 
        cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 0), 1, cv2.LINE_AA)

    cv2.putText(display, f"Receive: {uart_time} ms", (150, 55), 
        cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 0), 1, cv2.LINE_AA)
    cv2.putText(display, f"Process: {process_time} ms", (150, 80), 
        cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 0), 1, cv2.LINE_AA)
    
    cv2.imshow("frame", display)

    if cv2.waitKey(1) == 27:
        break