import sqlite3
from pathlib import Path

# Current file location: Data/sql/label_images_to_sql.py
BASE_DIR = Path(__file__).resolve().parent.parent

DATABASE_PATH = Path(__file__).resolve().parent / "gesture_image_database.db"
IMAGE_DATASET_DIR = BASE_DIR / "image_dataset"

conn = sqlite3.connect(DATABASE_PATH)
cursor = conn.cursor()

cursor.execute("""
CREATE TABLE IF NOT EXISTS gesture_images (
    image_id INTEGER PRIMARY KEY AUTOINCREMENT,
    image_path TEXT NOT NULL,
    label TEXT NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
)
""")

for label_folder in IMAGE_DATASET_DIR.iterdir():
    if label_folder.is_dir():
        label = label_folder.name

        for image_file in label_folder.iterdir():
            if image_file.suffix.lower() in [".jpg", ".jpeg", ".png"]:
                cursor.execute("""
                    INSERT INTO gesture_images (image_path, label)
                    VALUES (?, ?)
                """, (str(image_file), label))

conn.commit()
conn.close()

print("Images labeled and saved to SQL database.")
print("Database:", DATABASE_PATH)
print("Image dataset:", IMAGE_DATASET_DIR)