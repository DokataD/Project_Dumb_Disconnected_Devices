import sqlite3
import pandas as pd

conn = sqlite3.connect("gesture_database.db")

query = """
SELECT 
    s.sample_id,
    se.label,
    s.ax, s.ay, s.az,
    s.gx, s.gy, s.gz,
    se.created_at
FROM gesture_samples s
JOIN gesture_sessions se
ON s.session_id = se.session_id
"""

df = pd.read_sql_query(query, conn)

print(df.head())
print("\nGesture counts:")
print(df["label"].value_counts())

conn.close()