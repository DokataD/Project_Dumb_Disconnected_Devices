import sqlite3

conn = sqlite3.connect("gesture_image_database.db")
cursor = conn.cursor()

cursor.execute("""
CREATE TABLE IF NOT EXISTS gesture_images (
    image_id INTEGER PRIMARY KEY AUTOINCREMENT,
    image_path TEXT NOT NULL,
    label TEXT NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
""")

conn.commit()
conn.close()

print("Image gesture database created successfully.")