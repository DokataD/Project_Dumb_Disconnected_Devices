import cv2
import mediapipe as mp
import numpy as np
from mediapipe.tasks import python as mp_python
from mediapipe.tasks.python import vision
from mediapipe.framework.formats import landmark_pb2
import urllib.request
import os

# ── Download model file if not present ───────────────────────
model_path = "hand_landmarker.task"
if not os.path.exists(model_path):
    print("Downloading hand landmark model...")
    urllib.request.urlretrieve(
        "https://storage.googleapis.com/mediapipe-models/hand_landmarker/hand_landmarker/float16/1/hand_landmarker.task",
        model_path
    )
    print("Downloaded.")

# ── MediaPipe new API setup ───────────────────────────────────
base_options = mp_python.BaseOptions(model_asset_path=model_path)
options = vision.HandLandmarkerOptions(
    base_options=base_options,
    num_hands=1,
    min_hand_detection_confidence=0.7,
    min_hand_presence_confidence=0.7,
    min_tracking_confidence=0.6
)
detector = vision.HandLandmarker.create_from_options(options)

# ── Drawing helper ────────────────────────────────────────────
HAND_CONNECTIONS = mp.solutions.hands.HAND_CONNECTIONS if hasattr(mp, 'solutions') else [
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

# ── Main loop ─────────────────────────────────────────────────
cap = cv2.VideoCapture(0)
cap.set(cv2.CAP_PROP_FRAME_WIDTH,  640)
cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)

print("Press Q to quit")

while cap.isOpened():
    ret, frame = cap.read()
    if not ret:
        break

    frame = cv2.flip(frame, 1)
    rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
    mp_image = mp.Image(image_format=mp.ImageFormat.SRGB, data=rgb)

    result = detector.detect(mp_image)

    gesture_label = "No hand"

    if result.hand_landmarks:
        landmarks_list = result.hand_landmarks[0]
        draw_landmarks_manual(frame, landmarks_list)
        landmarks = extract_landmarks(landmarks_list)
        gesture_label = "Ready for classification"
        print(f"Landmarks shape: {landmarks.shape}  first 6: {landmarks[:6].round(3)}")

    cv2.rectangle(frame, (0, 0), (640, 40), (0, 0, 0), -1)
    cv2.putText(frame, gesture_label, (10, 28),
                cv2.FONT_HERSHEY_SIMPLEX, 0.9, (0, 255, 120), 2)

    cv2.imshow("Gesture pipeline — Phase 2", frame)
    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

cap.release()
cv2.destroyAllWindows()