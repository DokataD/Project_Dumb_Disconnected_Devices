import cv2
import os
import mediapipe as mp
from mediapipe.tasks import python as mp_python
from mediapipe.tasks.python import vision
import urllib.request
import numpy as np

# ── Config ────────────────────────────────────────────────────
ESP32_CAPTURE_URL  = "http://192.168.178.71/capture"
GESTURES           = ['forward', 'back', 'left', 'right', 'stop', 'idle']
IMAGES_PER_GESTURE = 50
IMAGE_SIZE         = 96
OUTPUT_DIR         = "dataset_esp32"

for g in GESTURES:
    os.makedirs(f"{OUTPUT_DIR}/{g}", exist_ok=True)

# ── MediaPipe ─────────────────────────────────────────────────
model_path   = "hand_landmarker.task"
base_options = mp_python.BaseOptions(model_asset_path=model_path)
options      = vision.HandLandmarkerOptions(
    base_options=base_options,
    num_hands=1,
    min_hand_detection_confidence=0.7,
    min_hand_presence_confidence=0.7,
    min_tracking_confidence=0.6
)
detector = vision.HandLandmarker.create_from_options(options)

HAND_CONNECTIONS = [
    (0,1),(1,2),(2,3),(3,4),
    (0,5),(5,6),(6,7),(7,8),
    (0,9),(9,10),(10,11),(11,12),
    (0,13),(13,14),(14,15),(15,16),
    (0,17),(17,18),(18,19),(19,20),
    (5,9),(9,13),(13,17)
]

def draw_landmarks_manual(frame, landmarks_list):
    h, w = frame.shape[:2]
    for lm in landmarks_list:
        cx, cy = int(lm.x * w), int(lm.y * h)
        cv2.circle(frame, (cx, cy), 5, (0, 255, 0), -1)
    for a, b in HAND_CONNECTIONS:
        x1 = int(landmarks_list[a].x * w)
        y1 = int(landmarks_list[a].y * h)
        x2 = int(landmarks_list[b].x * w)
        y2 = int(landmarks_list[b].y * h)
        cv2.line(frame, (x1, y1), (x2, y2), (255, 255, 255), 1)

GESTURE_KEYS = {
    'f': 'forward',
    'b': 'back',
    'l': 'left',
    'r': 'right',
    's': 'stop',
    'i': 'idle'
}

collected = {g: 0 for g in GESTURES}
recording       = False
current_gesture = None
image_count     = 0

def grab_frame():
    try:
        with urllib.request.urlopen(ESP32_CAPTURE_URL, timeout=10) as resp:
            img_array = np.frombuffer(resp.read(), dtype=np.uint8)
            frame = cv2.imdecode(img_array, cv2.IMREAD_COLOR)
            return frame
    except Exception as e:
        return None

print("\n── ESP32-CAM Image Collector v3 ──────────────────────")
print(f"Fetching frames from: {ESP32_CAPTURE_URL}")
print("Keys: F=forward  B=back  L=left  R=right  S=stop  I=idle  Q=quit")
print("Collect 4 rounds in different locations/lighting for best results")
print("──────────────────────────────────────────────────────\n")

# Test connection
print("Testing connection...")
test = None
for i in range(5):
    test = grab_frame()
    if test is not None:
        break
    print(f"  Retry {i+1}/5...")
if test is None:
    print("ERROR: Could not reach ESP32-CAM")
    exit()
print("Connected!\n")

while True:
    frame = grab_frame()
    if frame is None:
        continue

    # Run MediaPipe on frame
    rgb    = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
    mp_img = mp.Image(image_format=mp.ImageFormat.SRGB, data=rgb)
    result = detector.detect(mp_img)

    hand_detected = bool(result.hand_landmarks)

    if recording:
        # For idle — save without needing hand detection
        if current_gesture == 'idle':
            h, w  = frame.shape[:2]
            gray    = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
            resized = cv2.resize(gray, (IMAGE_SIZE, IMAGE_SIZE))
            fname   = f"{OUTPUT_DIR}/{current_gesture}/{current_gesture}_{collected[current_gesture]:04d}.jpg"
            cv2.imwrite(fname, resized)
            collected[current_gesture] += 1
            image_count += 1

        elif hand_detected:
            lm   = result.hand_landmarks[0]
            h, w = frame.shape[:2]
            xs   = [l.x * w for l in lm]
            ys   = [l.y * h for l in lm]
            pad  = 40
            x1   = max(0, int(min(xs)) - pad)
            y1   = max(0, int(min(ys)) - pad)
            x2   = min(w, int(max(xs)) + pad)
            y2   = min(h, int(max(ys)) + pad)

            draw_landmarks_manual(frame, lm)
            cv2.rectangle(frame, (x1, y1), (x2, y2), (0, 255, 0), 2)

            hand_crop = frame[y1:y2, x1:x2]
            if hand_crop.size > 0:
                gray    = cv2.cvtColor(hand_crop, cv2.COLOR_BGR2GRAY)
                resized = cv2.resize(gray, (IMAGE_SIZE, IMAGE_SIZE))
                fname   = f"{OUTPUT_DIR}/{current_gesture}/{current_gesture}_{collected[current_gesture]:04d}.jpg"
                cv2.imwrite(fname, resized)
                collected[current_gesture] += 1
                image_count += 1

        if image_count >= IMAGES_PER_GESTURE:
            print(f"  ✓ Done! {IMAGES_PER_GESTURE} images saved for '{current_gesture}'")
            print(f"  Total so far: {collected}")
            recording   = False
            image_count = 0

    elif hand_detected:
        draw_landmarks_manual(frame, result.hand_landmarks[0])

    # ── HUD ───────────────────────────────────────────────────
    cv2.rectangle(frame, (0, 0), (640, 100), (0, 0, 0), -1)

    if recording:
        progress = int((image_count / IMAGES_PER_GESTURE) * 400)
        cv2.rectangle(frame, (10, 75), (410, 90), (50, 50, 50), -1)
        cv2.rectangle(frame, (10, 75), (10 + progress, 90), (0, 255, 120), -1)
        status = f"SAVING '{current_gesture.upper()}' — {image_count}/{IMAGES_PER_GESTURE}"
        color  = (0, 60, 255)
    else:
        status = "Hand detected — press key" if hand_detected else "No hand — press I for idle or show hand"
        color  = (0, 255, 120)

    cv2.putText(frame, status, (10, 30),
                cv2.FONT_HERSHEY_SIMPLEX, 0.7, color, 2)
    counts_str = "  ".join([f"{k}:{v}" for k, v in collected.items()])
    cv2.putText(frame, counts_str, (10, 55),
                cv2.FONT_HERSHEY_SIMPLEX, 0.45, (200, 200, 200), 1)
    cv2.putText(frame, "F=fwd B=back L=left R=right S=stop I=idle Q=quit", (10, 72),
                cv2.FONT_HERSHEY_SIMPLEX, 0.4, (150, 150, 150), 1)

    cv2.imshow("ESP32-CAM Collector v3", frame)

    key = cv2.waitKey(100) & 0xFF
    if key == ord('q'):
        break
    elif not recording:
        for k, label in GESTURE_KEYS.items():
            if key == ord(k):
                if label != 'idle' and not hand_detected:
                    print(f"  ⚠ No hand detected — show your hand first!")
                else:
                    print(f"  ► Recording '{label}'...")
                    recording       = True
                    current_gesture = label
                    image_count     = 0

cv2.destroyAllWindows()
print(f"\nDataset saved to '{OUTPUT_DIR}/'")
print(f"Final counts: {collected}")