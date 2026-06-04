import sys
import cv2
import serial
import numpy as np
import serial.tools.list_ports

WIDTH = 64
HEIGHT = 64
COUNT = WIDTH * HEIGHT
BAUD_RATE = 115200

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

while True:
    while True:
        b1 = ser.read(1)
        if b1 == b'\xAA':
            b2 = ser.read(1)
            if b2 == b'\x55':
                break

    count = int.from_bytes(ser.read(4), "little")

    if count != COUNT:
        continue

    raw = ser.read(count * 4)

    if len(raw) != count * 4:
        continue

    img = np.frombuffer(raw, dtype=np.float32)
    img = img.reshape((HEIGHT, WIDTH))

    # visualize
    disp = np.clip(img * 255, 0, 255).astype(np.uint8)

    display = cv2.resize(disp, (320, 320))
    cv2.imshow("pixelBuf", display)

    if cv2.waitKey(1) == 27:
        break