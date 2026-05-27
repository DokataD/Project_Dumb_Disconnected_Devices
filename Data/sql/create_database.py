import sqlite3
from pathlib import Path

DATABASE_PATH = Path("gesture_database.db")

conn = sqlite3.connect(DATABASE_PATH)
cursor = conn.cursor()

cursor.execute("""
CREATE TABLE IF NOT EXISTS gestures (
    gesture_id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT NOT NULL UNIQUE
)
""")

cursor.execute("""
CREATE TABLE IF NOT EXISTS gesture_samples (
    sample_id INTEGER PRIMARY KEY AUTOINCREMENT,
    gesture_id INTEGER NOT NULL,
    ax REAL NOT NULL,
    ay REAL NOT NULL,
    az REAL NOT NULL,
    gx REAL NOT NULL,
    gy REAL NOT NULL,
    gz REAL NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (gesture_id) REFERENCES gestures(gesture_id)
)
""")

conn.commit()
conn.close()

print("SQL database created successfully: gesture_database.db")