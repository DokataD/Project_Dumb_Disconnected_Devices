import cv2
import numpy as np
import urllib.request
import time

CAPTURE_URL    = "http://192.168.178.71/capture"
PREDICTION_URL = "http://192.168.178.71/prediction"

GESTURE_COLORS = {
    'forward' : (0,   255, 0),
    'back'    : (0,   100, 255),
    'left'    : (255, 200, 0),
    'right'   : (0,   200, 255),
    'stop'    : (0,   0,   255),
    'idle'    : (100, 100, 100),
}

def grab_frame():
    try:
        with urllib.request.urlopen(CAPTURE_URL, timeout=5) as r:
            return cv2.imdecode(np.frombuffer(r.read(), np.uint8), cv2.IMREAD_COLOR)
    except:
        return None

def grab_prediction():
    try:
        with urllib.request.urlopen(PREDICTION_URL, timeout=5) as r:
            text = r.read().decode()
            label, conf = text.split(":")
            return label.strip(), float(conf.strip())
    except:
        return "unknown", 0.0

print("Connecting to ESP32-CAM...")
while grab_frame() is None:
    print("  Retrying...")
    time.sleep(1)
print("Connected! Press Q to quit")

while True:
    frame = grab_frame()
    if frame is None:
        continue

    label, conf = grab_prediction()
    color = GESTURE_COLORS.get(label, (200, 200, 200))

    cv2.rectangle(frame, (0, 0), (640, 80), (0, 0, 0), -1)
    cv2.putText(frame, label.upper(), (10, 48),
                cv2.FONT_HERSHEY_SIMPLEX, 1.3, color, 3)
    cv2.putText(frame, f"Confidence: {conf*100:.1f}%", (10, 68),
                cv2.FONT_HERSHEY_SIMPLEX, 0.5, (180, 180, 180), 1)

    cv2.imshow("ESP32-CAM Inference View", frame)
    if cv2.waitKey(100) & 0xFF == ord('q'):
        break

cv2.destroyAllWindows()