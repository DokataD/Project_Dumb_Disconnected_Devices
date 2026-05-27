import serial
import sqlite3

SERIAL_PORT = "COM9"   # Change this if your Arduino uses another COM port
BAUD_RATE = 115200
DATABASE_NAME = "gesture_database.db"

label = input("Enter gesture label (left/right/up/down/circle/stay): ")

conn = sqlite3.connect(DATABASE_NAME)
cursor = conn.cursor()

# Insert gesture label if it does not already exist
cursor.execute("""
INSERT OR IGNORE INTO gestures (label)
VALUES (?)
""", (label,))

# Get gesture_id for selected label
cursor.execute("""
SELECT gesture_id FROM gestures
WHERE label = ?
""", (label,))

gesture_id = cursor.fetchone()[0]

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
            (gesture_id, ax, ay, az, gx, gy, gz)
            VALUES (?, ?, ?, ?, ?, ?, ?)
            """, (gesture_id, ax, ay, az, gx, gy, gz))

            conn.commit()

except KeyboardInterrupt:
    print("\nStopped collecting")

finally:
    ser.close()
    conn.close()
    print("Data saved to SQL database.")