import tensorflow as tf
import numpy as np

# Load your trained model
model = tf.keras.models.load_model("gesture_model.keras")

# Convert to TFLite with full integer quantization
converter = tf.lite.TFLiteConverter.from_keras_model(model)
converter.optimizations = [tf.lite.Optimize.DEFAULT]

# Generate representative dataset for quantization
classes = np.load("label_classes.npy", allow_pickle=True)
import pandas as pd
df = pd.read_csv("gesture_data.csv")
X = df.drop('label', axis=1).values.astype(np.float32)

def representative_dataset():
    for i in range(min(100, len(X))):
        yield [X[i:i+1]]

converter.representative_dataset = representative_dataset
converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
converter.inference_input_type  = tf.int8
converter.inference_output_type = tf.int8

tflite_model = converter.convert()

# Save
with open("gesture_model.tflite", "wb") as f:
    f.write(tflite_model)

print(f"TFLite model size: {len(tflite_model)} bytes")

# Convert to C array for Arduino
with open("gesture_model.tflite", "rb") as f:
    data = f.read()

c_array = ", ".join([f"0x{b:02x}" for b in data])
c_code = f"""#ifndef GESTURE_MODEL_H
#define GESTURE_MODEL_H

const unsigned char gesture_model[] = {{
  {c_array}
}};
const unsigned int gesture_model_len = {len(data)};

#endif
"""
with open("gesture_model.h", "w") as f:
    f.write(c_code)

print("Saved gesture_model.h — copy this to your Arduino sketch folder")