import cv2
import numpy as np
import mediapipe as mp
from mediapipe.tasks import python as mp_python
from mediapipe.tasks.python import vision
import tensorflow as tf
import os
import urllib.request

model   = tf.keras.models.load_model("gesture_model.keras")
classes = np.load("label_classes.npy", allow_pickle=True)

model_path = "hand_landmarker.task"
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

def extract_landmarks(landmarks_list):
    wrist  = landmarks_list[0]
    coords = []
    for lm in landmarks_list:
        coords.extend([lm.x - wrist.x, lm.y - wrist.y, lm.z - wrist.z])
    return np.array(coords, dtype=np.float32)

cap = cv2.VideoCapture(0)
cap.set(cv2.CAP_PROP_FRAME_WIDTH,  640)
cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)

print("Press Q to quit — watch the terminal for raw confidence scores")

while cap.isOpened():
    ret, frame = cap.read()
    if not ret:
        break

    frame  = cv2.flip(frame, 1)
    rgb    = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
    mp_img = mp.Image(image_format=mp.ImageFormat.SRGB, data=rgb)
    result = detector.detect(mp_img)

    if result.hand_landmarks:
        lm_list   = result.hand_landmarks[0]
        draw_landmarks_manual(frame, lm_list)
        landmarks = extract_landmarks(lm_list)

        probs = model.predict(landmarks.reshape(1, -1), verbose=0)[0]

        # Print ALL class probabilities so we can see what's happening
        scores = {classes[i]: f"{probs[i]*100:.1f}%" for i in range(len(classes))}
        print(f"Scores: {scores}  → predicted: {classes[np.argmax(probs)]} ({np.max(probs)*100:.1f}%)")

        # Show on frame with NO threshold — raw prediction
        label = f"{classes[np.argmax(probs)]}  {np.max(probs)*100:.1f}%"
        cv2.rectangle(frame, (0, 0), (640, 50), (0, 0, 0), -1)
        cv2.putText(frame, label, (10, 35),
                    cv2.FONT_HERSHEY_SIMPLEX, 1.0, (0, 255, 120), 2)

    cv2.imshow("Debug", frame)
    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

cap.release()
cv2.destroyAllWindows()