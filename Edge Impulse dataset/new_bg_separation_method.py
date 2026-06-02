import cv2
import numpy as np
import urllib.request
import time

MJPEG = True

def grab_frame():
    if MJPEG:
        ESP32_CAPTURE_URL = "http://145.76.19.73/mjpeg/1"
        try:
            stream = urllib.request.urlopen(ESP32_CAPTURE_URL, timeout=10)
            buffer = b""
            while True:
                buffer += stream.read(4096)
                # JPEG frames start with FFD8 and end with FFD9
                start = buffer.find(b'\xff\xd8')
                end   = buffer.find(b'\xff\xd9')
                if start != -1 and end != -1 and end > start:
                    jpg = buffer[start:end+2]
                    buffer = buffer[end+2:]
                    img_array = np.frombuffer(jpg, dtype=np.uint8)
                    frame = cv2.imdecode(img_array, cv2.IMREAD_COLOR)
                    if frame is not None:
                        return frame
        except:
            return None
    else:
        ESP32_CAPTURE_URL = "http://145.76.19.73/capture"
        try:
            with urllib.request.urlopen(ESP32_CAPTURE_URL, timeout=10) as resp:
                img_array = np.frombuffer(resp.read(), dtype=np.uint8)
                return cv2.imdecode(img_array, cv2.IMREAD_COLOR)
        except:
            return None

print("Testing connection...")
frame = None
for i in range(5):
    frame = grab_frame()
    if frame is not None:
        break
    print(f"  Retry {i+1}/5...")
    time.sleep(1)

if frame is None:
    print("ERROR: Could not connect")
    exit()

print("Connected! Show your hand to the camera...")
print("Press S to sample your skin color, Q to quit")

# Default skin color range in HSV
lower_skin = np.array([0,  20, 70],  dtype=np.uint8)
upper_skin = np.array([20, 255, 255], dtype=np.uint8)

while True:
    frame = grab_frame()
    if frame is None:
        continue

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
    small   = cv2.resize(mask, (96, 96))
    display = cv2.resize(small, (320, 320), interpolation=cv2.INTER_NEAREST)

    # Show side by side
    combined = np.zeros((320, 640, 3), dtype=np.uint8)
    combined[:, :320] = cv2.resize(frame, (320, 320))
    combined[:, 320:] = cv2.cvtColor(display, cv2.COLOR_GRAY2BGR)

    cv2.putText(combined, "Original", (10, 30),
                cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)
    cv2.putText(combined, "Skin mask (model sees)", (330, 30),
                cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 255, 0), 2)
    cv2.putText(combined, "S=sample skin  Q=quit", (10, 310),
                cv2.FONT_HERSHEY_SIMPLEX, 0.5, (180, 180, 180), 1)

    cv2.imshow("Skin Detection Test", combined)

    key = cv2.waitKey(1) & 0xFF
    if key == ord('q'):
        break
    elif key == ord('s'):
        # Sample skin color from center of frame
        h, w = frame.shape[:2]
        cx, cy = w // 2, h // 2
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
        print(f"Skin sampled! HSV range: {lower_skin} - {upper_skin}")

cv2.destroyAllWindows()
print("Done")