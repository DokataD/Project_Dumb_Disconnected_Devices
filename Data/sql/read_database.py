import sqlite3
import pandas as pd

DATABASE_NAME = "gesture_database.db"

conn = sqlite3.connect(DATABASE_NAME)

query = """
SELECT 
    gs.sample_id,
    g.label,
    gs.ax,
    gs.ay,
    gs.az,
    gs.gx,
    gs.gy,
    gs.gz,
    gs.created_at
FROM gesture_samples gs
JOIN gestures g
ON gs.gesture_id = g.gesture_id
ORDER BY gs.sample_id
"""

df = pd.read_sql_query(query, conn)

print("Database preview:")
print(df.head())

print("\nGesture label count:")
print(df["label"].value_counts())

print("\nTotal samples:", len(df))

conn.close()