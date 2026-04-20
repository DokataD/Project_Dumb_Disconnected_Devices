import serial
import pandas as pd

SERIAL_PORT = 'COM6'  
BAUD_RATE = 115200

ser = serial.Serial(SERIAL_PORT, BAUD_RATE)

data = []
label = input("Enter gesture label (e.g., left/right/up): ")

print("Collecting data... Press CTRL+C to stop")

try:
    while True:
        line = ser.readline().decode('utf-8').strip()
        print(line)

        values = line.split(",")

        if len(values) == 6:
            ax, ay, az, gx, gy, gz = map(float, values)

            data.append([ax, ay, az, gx, gy, gz, label])

except KeyboardInterrupt:
    print("\nStopped collecting")

# Save to CSV
df = pd.DataFrame(data, columns=["Ax", "Ay", "Az", "Gx", "Gy", "Gz", "Label"])
df.to_csv("gesture_data.csv", mode='a', header=False, index=False)

print("Data saved to gesture_data.csv")