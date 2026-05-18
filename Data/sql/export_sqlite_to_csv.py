import sqlite3
import pandas as pd
from pathlib import Path

BASE_DIR = Path(__file__).resolve().parent.parent
DATABASE_PATH = BASE_DIR / "sql" / "gesture_database.db"

RAW_CSV_PATH = BASE_DIR / "database" / "gesture_data_from_sqlite.csv"
CLEANED_CSV_PATH = BASE_DIR / "database" / "gesture_data_cleaned_from_sqlite.csv"

conn = sqlite3.connect(DATABASE_PATH)

query = """
SELECT 
    s.ax AS Ax,
    s.ay AS Ay,
    s.az AS Az,
    s.gx AS Gx,
    s.gy AS Gy,
    s.gz AS Gz,
    se.label AS Label
FROM gesture_samples s
JOIN gesture_sessions se
ON s.session_id = se.session_id
"""

df = pd.read_sql_query(query, conn)
conn.close()

df.to_csv(RAW_CSV_PATH, index=False)

print("Raw exported data:")
print(df.head())

df = df.dropna()

sensor_columns = ["Ax", "Ay", "Az", "Gx", "Gy", "Gz"]

df[sensor_columns] = (
    df[sensor_columns] - df[sensor_columns].mean()
) / df[sensor_columns].std()

df.to_csv(CLEANED_CSV_PATH, index=False)

print(f"\nRaw CSV saved to: {RAW_CSV_PATH}")
print(f"Cleaned CSV saved to: {CLEANED_CSV_PATH}")