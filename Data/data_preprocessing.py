import pandas as pd

# Load dataset
df = pd.read_csv("gesture_data.csv")

print("Original data:")
print(df.head())

# 🔹 Remove missing values
df = df.dropna()

# 🔹 Normalize sensor values (simple scaling)
sensor_columns = ["Ax", "Ay", "Az", "Gx", "Gy", "Gz"]

df[sensor_columns] = (df[sensor_columns] - df[sensor_columns].mean()) / df[sensor_columns].std()

# Save cleaned dataset
df.to_csv("gesture_data_cleaned.csv", index=False)

print("\nCleaned data saved as gesture_data_cleaned.csv")