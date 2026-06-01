import cv2
import numpy as np
import csv
import os
import mediapipe as mp
from mediapipe.tasks import python as mp_python
from mediapipe.tasks.python import vision
import urllib.request
import random

# ── Model download ────────────────────────────────────────────
model_path = "hand_landmarker.task"
if not os.path.exists(model_path):
    urllib.request.urlretrieve(
        "https://storage.googleapis.com/mediapipe-models/hand_landmarker/hand_landmarker/float16/1/hand_landmarker.task",
        model_path
    )

base_options  = mp_python.BaseOptions(model_asset_path=model_path)
options       = vision.HandLandmarkerOptions(
    base_options=base_options,
    num_hands=1,
    min_hand_detection_confidence=0.7,
    min_hand_presence_confidence=0.7,
    min_tracking_confidence=0.6
)
detector = vision.HandLandmarker.create_from_options(options)

GESTURES = {
    'f': 'forward',
    'b': 'back',
    'l': 'left',
    'r': 'right',
    's': 'stop'
}
SAMPLES_PER_SESSION = 300
CSV_FILE = "gesture_data.csv"

if not os.path.exists(CSV_FILE):
    with open(CSV_FILE, 'w', newline='') as f:
        writer = csv.writer(f)
        header = [f"{axis}{i}" for i in range(21) for axis in ['x','y','z']]
        header.append('label')
        writer.writerow(header)

def extract_landmarks(landmarks_list):
    wrist = landmarks_list[0]
    coords = []
    for lm in landmarks_list:
        coords.extend([
            lm.x - wrist.x,
            lm.y - wrist.y,
            lm.z - wrist.z
        ])
    return np.array(coords, dtype=np.float32)

def augment_landmarks(landmarks):
    """
    Add small random noise to simulate natural hand variation.
    This multiplies your effective dataset size significantly.
    """
    augmented = []
    augmented.append(landmarks)  # original

    for _ in range(4):           # 4 augmented copies per real sample
        noise = np.random.normal(0, 0.008, landmarks.shape).astype(np.float32)
        augmented.append(landmarks + noise)

    return augmented

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

cap = cv2.VideoCapture(0)
cap.set(cv2.CAP_PROP_FRAME_WIDTH,  640)
cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)

recording       = False
current_gesture = None
sample_count    = 0
collected       = {g: 0 for g in GESTURES.values()}

print("\n── Improved Gesture Collector ───────────────────────")
print("Keys: F=forward  B=back  L=left  R=right  S=stop  Q=quit")
print("")
print("IMPORTANT — while recording:")
print("  • Slowly move your hand closer and farther from camera")
print("  • Gently rotate your wrist left and right")
print("  • Raise and lower your hand slightly")
print("  • Keep the gesture shape but let it move naturally")
print("─────────────────────────────────────────────────────\n")

while cap.isOpened():
    ret, frame = cap.read()
    if not ret:
        break

    frame  = cv2.flip(frame, 1)
    rgb    = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
    mp_img = mp.Image(image_format=mp.ImageFormat.SRGB, data=rgb)
    result = detector.detect(mp_img)

    hand_detected = False

    if result.hand_landmarks:
        hand_detected = True
        lm_list       = result.hand_landmarks[0]
        draw_landmarks_manual(frame, lm_list)
        landmarks = extract_landmarks(lm_list)

        if recording:
            # Save original + augmented copies
            augmented_samples = augment_landmarks(landmarks)
            with open(CSV_FILE, 'a', newline='') as f:
                writer = csv.writer(f)
                for sample in augmented_samples:
                    writer.writerow(list(sample) + [current_gesture])

            sample_count += len(augmented_samples)
            collected[current_gesture] = collected.get(current_gesture, 0) + len(augmented_samples)

            if sample_count >= SAMPLES_PER_SESSION * 5:  # x5 due to augmentation
                print(f"  ✓ Done! Saved {sample_count} samples for '{current_gesture}'")
                print(f"  Total collected: {collected}")
                recording    = False
                sample_count = 0

    # ── HUD ───────────────────────────────────────────────────
    cv2.rectangle(frame, (0, 0), (640, 95), (0, 0, 0), -1)

    if recording:
        progress = int((sample_count / (SAMPLES_PER_SESSION * 5)) * 400)
        cv2.rectangle(frame, (10, 70), (410, 85), (50, 50, 50), -1)
        cv2.rectangle(frame, (10, 70), (10 + progress, 85), (0, 255, 120), -1)
        status = f"RECORDING '{current_gesture.upper()}' — move hand naturally!"
        color  = (0, 60, 255)
    else:
        status = "Hand detected — press key" if hand_detected else "No hand detected"
        color  = (0, 255, 120)

    cv2.putText(frame, status, (10, 30),
                cv2.FONT_HERSHEY_SIMPLEX, 0.75, color, 2)
    counts_str = "  ".join([f"{k}:{v}" for k, v in collected.items()])
    cv2.putText(frame, counts_str, (10, 55),
                cv2.FONT_HERSHEY_SIMPLEX, 0.5, (200, 200, 200), 1)

    cv2.imshow("Gesture Collector v2", frame)

    key = cv2.waitKey(1) & 0xFF
    if key == ord('q'):
        break
    elif not recording:
        for k, label in GESTURES.items():
            if key == ord(k):
                if not hand_detected:
                    print(f"  ⚠ No hand — show your hand first!")
                else:
                    print(f"  ► Recording '{label}' — move hand naturally...")
                    recording       = True
                    current_gesture = label
                    sample_count    = 0

cap.release()
cv2.destroyAllWindows()
print(f"\nFinal counts: {collected}")
print(f"Saved to {CSV_FILE}")