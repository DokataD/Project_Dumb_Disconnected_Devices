import cv2
import os
import time
import mediapipe as mp
from mediapipe.tasks import python as mp_python
from mediapipe.tasks.python import vision

# ── Setup ─────────────────────────────────────────────────────
GESTURES       = ['forward', 'back', 'left', 'right', 'stop']
IMAGES_PER_GESTURE = 150
IMAGE_SIZE     = 96       # Edge Impulse MobileNet input size
OUTPUT_DIR     = "dataset"

# Create folders
for g in GESTURES:
    os.makedirs(f"{OUTPUT_DIR}/{g}", exist_ok=True)

# ── MediaPipe setup ───────────────────────────────────────────
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

# ── Main loop ─────────────────────────────────────────────────
cap = cv2.VideoCapture(0)
cap.set(cv2.CAP_PROP_FRAME_WIDTH,  640)
cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)

recording       = False
current_gesture = None
image_count     = 0
collected       = {g: 0 for g in GESTURES}

GESTURE_KEYS = {
    'f': 'forward',
    'b': 'back',
    'l': 'left',
    'r': 'right',
    's': 'stop'
}

print("\n── Image Dataset Collector ──────────────────────────")
print("Keys: F=forward  B=back  L=left  R=right  S=stop  Q=quit")
print("Hold gesture, press key, move hand naturally until done")
print("─────────────────────────────────────────────────────\n")

while cap.isOpened():
    ret, frame = cap.read()
    if not ret:
        break

    frame  = cv2.flip(frame, 1)
    rgb    = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
    mp_img = mp.Image(image_format=mp.ImageFormat.SRGB, data=rgb)
    result = detector.detect(mp_img)

    hand_detected = bool(result.hand_landmarks)

    if hand_detected and recording:
        # Crop hand region and save as 96x96 grayscale
        lm        = result.hand_landmarks[0]
        h, w      = frame.shape[:2]
        xs        = [l.x * w for l in lm]
        ys        = [l.y * h for l in lm]
        pad       = 40
        x1        = max(0, int(min(xs)) - pad)
        y1        = max(0, int(min(ys)) - pad)
        x2        = min(w, int(max(xs)) + pad)
        y2        = min(h, int(max(ys)) + pad)

        hand_crop = frame[y1:y2, x1:x2]
        if hand_crop.size > 0:
            gray      = cv2.cvtColor(hand_crop, cv2.COLOR_BGR2GRAY)
            resized   = cv2.resize(gray, (IMAGE_SIZE, IMAGE_SIZE))
            filename  = f"{OUTPUT_DIR}/{current_gesture}/{current_gesture}_{collected[current_gesture]:04d}.jpg"
            cv2.imwrite(filename, resized)
            collected[current_gesture] += 1
            image_count += 1

            # Draw crop box on frame
            cv2.rectangle(frame, (x1, y1), (x2, y2), (0, 255, 0), 2)

        if image_count >= IMAGES_PER_GESTURE:
            print(f"  ✓ Done! {IMAGES_PER_GESTURE} images saved for '{current_gesture}'")
            print(f"  Total: {collected}")
            recording   = False
            image_count = 0

    # ── HUD ───────────────────────────────────────────────────
    cv2.rectangle(frame, (0, 0), (640, 95), (0, 0, 0), -1)

    if recording:
        progress = int((image_count / IMAGES_PER_GESTURE) * 400)
        cv2.rectangle(frame, (10, 70), (410, 85), (50, 50, 50), -1)
        cv2.rectangle(frame, (10, 70), (10 + progress, 85), (0, 255, 120), -1)
        status = f"SAVING '{current_gesture.upper()}' — move hand naturally"
        color  = (0, 60, 255)
    else:
        status = "Hand detected — press key" if hand_detected else "No hand detected"
        color  = (0, 255, 120)

    cv2.putText(frame, status, (10, 30),
                cv2.FONT_HERSHEY_SIMPLEX, 0.75, color, 2)
    counts_str = "  ".join([f"{k}:{v}" for k, v in collected.items()])
    cv2.putText(frame, counts_str, (10, 55),
                cv2.FONT_HERSHEY_SIMPLEX, 0.5, (200, 200, 200), 1)
    cv2.putText(frame, "F=fwd B=back L=left R=right S=stop Q=quit", (10, 82),
                cv2.FONT_HERSHEY_SIMPLEX, 0.45, (150, 150, 150), 1)

    cv2.imshow("Image Dataset Collector", frame)

    key = cv2.waitKey(1) & 0xFF
    if key == ord('q'):
        break
    elif not recording:
        for k, label in GESTURE_KEYS.items():
            if key == ord(k):
                if not hand_detected:
                    print(f"  ⚠ No hand detected!")
                else:
                    print(f"  ► Saving '{label}' images...")
                    recording       = True
                    current_gesture = label
                    image_count     = 0

cap.release()
cv2.destroyAllWindows()
print(f"\nDataset saved to '{OUTPUT_DIR}/' folder")
print(f"Final counts: {collected}")