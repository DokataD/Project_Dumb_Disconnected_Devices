import pandas as pd

# Load dataset (no headers)
df = pd.read_csv("gesture_data.csv", header=None)

# Assign column names manually
df.columns = ["Ax", "Ay", "Az", "Gx", "Gy", "Gz", "Label"]

print("Original data:")
print(df["Label"].value_counts())

sensor_columns = ["Ax", "Ay", "Az", "Gx", "Gy", "Gz"]

df[sensor_columns] = (df[sensor_columns] - df[sensor_columns].mean()) / df[sensor_columns].std()

# Save WITHOUT header to match the headerless format used in data_collection.py
df.to_csv("gesture_data_cleaned.csv", index=False, header=False)

print("Cleaned data saved to gesture_data_cleaned.csv")
print("\nNormalized data preview:")
print(df.head())