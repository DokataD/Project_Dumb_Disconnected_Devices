import numpy as np
import pandas as pd
from sklearn.model_selection import train_test_split
from sklearn.preprocessing import LabelEncoder
from sklearn.metrics import classification_report, confusion_matrix
import tensorflow as tf
from tensorflow import keras
import matplotlib.pyplot as plt
import seaborn as sns

# ── Load data ─────────────────────────────────────────────────
df = pd.read_csv("gesture_data.csv")
print(f"Dataset shape: {df.shape}")
print(f"Samples per gesture:\n{df['label'].value_counts()}\n")

X = df.drop('label', axis=1).values.astype(np.float32)
y = df['label'].values

# ── Encode labels ─────────────────────────────────────────────
le = LabelEncoder()
y_encoded = le.fit_transform(y)
print(f"Classes: {le.classes_}")

# ── Train/test split ──────────────────────────────────────────
X_train, X_test, y_train, y_test = train_test_split(
    X, y_encoded, test_size=0.2, random_state=42, stratify=y_encoded
)
print(f"Train: {X_train.shape}  Test: {X_test.shape}")

# ── Model ─────────────────────────────────────────────────────
# Small dense network — fits comfortably in Nano BLE 33 Sense RAM
model = keras.Sequential([
    keras.layers.Input(shape=(63,)),
    keras.layers.Dense(64, activation='relu'),
    keras.layers.BatchNormalization(),
    keras.layers.Dropout(0.3),
    keras.layers.Dense(32, activation='relu'),
    keras.layers.BatchNormalization(),
    keras.layers.Dropout(0.2),
    keras.layers.Dense(len(le.classes_), activation='softmax')
])

model.summary()

model.compile(
    optimizer='adam',
    loss='sparse_categorical_crossentropy',
    metrics=['accuracy']
)

# ── Train ─────────────────────────────────────────────────────
early_stop = keras.callbacks.EarlyStopping(
    monitor='val_accuracy', patience=15, restore_best_weights=True
)

history = model.fit(
    X_train, y_train,
    epochs=100,
    batch_size=32,
    validation_split=0.15,
    callbacks=[early_stop],
    verbose=1
)

# ── Evaluate ──────────────────────────────────────────────────
test_loss, test_acc = model.evaluate(X_test, y_test, verbose=0)
print(f"\nTest accuracy: {test_acc*100:.1f}%")

y_pred = np.argmax(model.predict(X_test), axis=1)
print("\nClassification report:")
print(classification_report(y_test, y_pred, target_names=le.classes_))

# ── Confusion matrix ──────────────────────────────────────────
cm = confusion_matrix(y_test, y_pred)
plt.figure(figsize=(7, 5))
sns.heatmap(cm, annot=True, fmt='d', cmap='Blues',
            xticklabels=le.classes_, yticklabels=le.classes_)
plt.title("Confusion Matrix")
plt.ylabel("True label")
plt.xlabel("Predicted label")
plt.tight_layout()
plt.savefig("confusion_matrix.png")
plt.show()
print("Confusion matrix saved to confusion_matrix.png")

# ── Save model ────────────────────────────────────────────────
model.save("gesture_model.keras")
np.save("label_classes.npy", le.classes_)
print("\nModel saved to gesture_model.keras")
print("Labels saved to label_classes.npy")