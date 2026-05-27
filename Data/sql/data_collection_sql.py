import serial
import sqlite3

SERIAL_PORT = "COM9"   # Change if needed
BAUD_RATE = 115200

label = input("Enter gesture label (e.g., left/right/up/down/circle/stay): ")

conn = sqlite3.connect("gesture_database.db")
cursor = conn.cursor()

cursor.execute(
    "INSERT INTO gesture_sessions (label) VALUES (?)",
    (label,)
)

session_id = cursor.lastrowid
conn.commit()

ser = serial.Serial(SERIAL_PORT, BAUD_RATE)

print("Collecting data... Press CTRL+C to stop")

try:
    while True:
        line = ser.readline().decode("utf-8").strip()
        print(line)

        values = line.split(",")

        if len(values) == 6:
            ax, ay, az, gx, gy, gz = map(float, values)

            cursor.execute("""
                INSERT INTO gesture_samples
                (session_id, ax, ay, az, gx, gy, gz)
                VALUES (?, ?, ?, ?, ?, ?, ?)
            """, (session_id, ax, ay, az, gx, gy, gz))

            conn.commit()

except KeyboardInterrupt:
    print("\nStopped collecting")

finally:
    ser.close()
    conn.close()
    print("Data saved to SQL database.")