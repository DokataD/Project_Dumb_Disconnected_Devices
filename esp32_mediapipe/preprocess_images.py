import cv2
import os
import numpy as np
INPUT_DIR = "dataset"
OUTPUT_DIR = "processed_dataset"

IMG_SIZE = 64

for label in os.listdir(INPUT_DIR):
    input_folder = os.path.join(INPUT_DIR, label)
    output_folder = os.path.join(OUTPUT_DIR, label)

    if not os.path.isdir(input_folder):
        continue

    os.makedirs(output_folder, exist_ok=True)

    for filename in os.listdir(input_folder):
        if not filename.lower().endswith((".jpg", ".jpeg", ".png")):
            continue

        input_path = os.path.join(input_folder, filename)
        output_path = os.path.join(output_folder, filename)

        img = cv2.imread(input_path)

        if img is None:
            print("Could not read:", input_path)
            continue

        # Resize to 64x64
        img = cv2.resize(img, (IMG_SIZE, IMG_SIZE))

        # Convert to grayscale
        gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)

        # Make black/white mask
        _, mask = cv2.threshold(gray, 130, 255, cv2.THRESH_BINARY)

        # Save processed image
        cv2.imwrite(output_path, mask)

        print("Saved:", output_path)