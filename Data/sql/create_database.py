import sqlite3

conn = sqlite3.connect("gesture_database.db")
cursor = conn.cursor()

cursor.execute("""
CREATE TABLE IF NOT EXISTS gesture_sessions (
    session_id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
)
""")

cursor.execute("""
CREATE TABLE IF NOT EXISTS gesture_samples (
    sample_id INTEGER PRIMARY KEY AUTOINCREMENT,
    session_id INTEGER,
    ax REAL NOT NULL,
    ay REAL NOT NULL,
    az REAL NOT NULL,
    gx REAL NOT NULL,
    gy REAL NOT NULL,
    gz REAL NOT NULL,
    FOREIGN KEY (session_id) REFERENCES gesture_sessions(session_id)
)
""")

conn.commit()
conn.close()

print("Database created successfully.")