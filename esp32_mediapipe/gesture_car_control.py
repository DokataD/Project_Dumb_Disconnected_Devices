import cv2
import numpy as np
import mediapipe as mp
from mediapipe.tasks import python as mp_python
from mediapipe.tasks.python import vision
import tensorflow as tf
import serial
import time
from collections import deque

# ── Configuration ─────────────────────────────────────────────
NANO_PORT         = "COM10"
BAUD_RATE         = 115200
CONFIDENCE_THRESH = 0.80
SMOOTH_WINDOW     = 5

GESTURE_COMMANDS = {
    'forward' : '1',
    'back'    : '2',
    'left'    : '3',
    'right'   : '4',
    'stop'    : 's',
}

GESTURE_COLORS = {
    'forward' : (0,   255, 0),
    'back'    : (0,   100, 255),
    'left'    : (255, 200, 0),
    'right'   : (0,   200, 255),
    'stop'    : (0,   0,   255),
}

# ── Load model ────────────────────────────────────────────────
model   = tf.keras.models.load_model("gesture_model.keras")
classes = np.load("label_classes.npy", allow_pickle=True)
print(f"Model loaded. Classes: {classes}")

# ── Serial connection ─────────────────────────────────────────
print(f"Connecting to Nano on {NANO_PORT}...")
nano = serial.Serial(NANO_PORT, BAUD_RATE, timeout=1)
time.sleep(2)
print("Serial connected!")

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

# ── Command sender ────────────────────────────────────────────
last_command = None

def send_gesture(gesture_name):
    global last_command
    cmd = GESTURE_COMMANDS.get(gesture_name)
    if cmd and cmd != last_command:
        last_command = cmd
        nano.write(cmd.encode())
        print(f"Sent: {gesture_name} → '{cmd}'")

# ── Main camera loop ──────────────────────────────────────────
cap = cv2.VideoCapture(0)
cap.set(cv2.CAP_PROP_FRAME_WIDTH,  640)
cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)

prediction_buffer = deque(maxlen=SMOOTH_WINDOW)

print("Press Q to quit")

while cap.isOpened():
    ret, frame = cap.read()
    if not ret:
        break

    frame  = cv2.flip(frame, 1)
    rgb    = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
    mp_img = mp.Image(image_format=mp.ImageFormat.SRGB, data=rgb)
    result = detector.detect(mp_img)

    gesture_label = "No hand"
    confidence    = 0.0
    color         = (100, 100, 100)

    if result.hand_landmarks:
        lm_list   = result.hand_landmarks[0]
        draw_landmarks_manual(frame, lm_list)
        landmarks = extract_landmarks(lm_list)

        probs      = model.predict(landmarks.reshape(1, -1), verbose=0)[0]
        confidence = float(np.max(probs))
        predicted  = classes[np.argmax(probs)]

        if confidence >= CONFIDENCE_THRESH:
            prediction_buffer.append(predicted)

        if prediction_buffer:
            gesture_label = max(set(prediction_buffer),
                                key=list(prediction_buffer).count)
            color = GESTURE_COLORS.get(gesture_label, (200, 200, 200))
            send_gesture(gesture_label)
        else:
            gesture_label = "Uncertain"

    # ── HUD ───────────────────────────────────────────────────
    cv2.rectangle(frame, (0, 0), (640, 80), (0, 0, 0), -1)
    cv2.putText(frame, gesture_label.upper(), (10, 48),
                cv2.FONT_HERSHEY_SIMPLEX, 1.3, color, 3)
    cv2.putText(frame, f"Confidence: {confidence*100:.1f}%", (10, 68),
                cv2.FONT_HERSHEY_SIMPLEX, 0.5, (180, 180, 180), 1)
    cv2.putText(frame, f"Serial: {NANO_PORT}", (450, 20),
                cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 120), 1)

    cv2.imshow("Gesture Car Control", frame)
    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

# ── Cleanup ───────────────────────────────────────────────────
cap.release()
cv2.destroyAllWindows()
nano.write(b's')
nano.close()
print("Done")