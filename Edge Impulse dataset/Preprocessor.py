from pathlib import Path
from PIL import Image
import numpy as np
import cv2

INPUT_DIR            = "raw"
OUTPUT_DIR           = "preprocessed"
SUPPORTED_EXTENSIONS = {".jpg", ".jpeg", ".png", ".bmp", ".tiff", ".tif", ".webp"}

# Default skin color range in HSV
lower_skin = np.array([0,  20, 70],  dtype=np.uint8)
upper_skin = np.array([20, 255, 255], dtype=np.uint8)

def preprocess(src_path: Path, dst_path: Path) -> None:
    # Convert to HSV
    frame = np.array(Image.open(src_path).convert("RGB"))
    frame = cv2.cvtColor(frame, cv2.COLOR_RGB2BGR)
    hsv   = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)

    # Create skin mask
    mask = cv2.inRange(hsv, lower_skin, upper_skin)

    # Clean up
    kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (5, 5))
    mask   = cv2.morphologyEx(mask, cv2.MORPH_OPEN,  kernel)
    mask   = cv2.morphologyEx(mask, cv2.MORPH_CLOSE, kernel)
    mask   = cv2.dilate(mask, kernel, iterations=2)

    # Resize to 96x96
    image  = cv2.resize(mask, (96, 96), interpolation=cv2.INTER_NEAREST)
    
    # Save as PNG
    dst_path.parent.mkdir(parents=True, exist_ok=True)
    dst_path = dst_path.with_suffix(".png")
    Image.fromarray(image).save(dst_path, format="PNG")


# ── dataset walker ───────────────────────────────────────────────────────────

def process_dataset(input_dir: str, output_dir: str) -> None:
    input_root  = Path(input_dir)
    output_root = Path(output_dir)

    if not input_root.exists():
        raise FileNotFoundError(f"Input directory not found: {input_root}")

    category_dirs = [d for d in sorted(input_root.iterdir()) if d.is_dir()]
    if not category_dirs:
        print("No category subfolders found.")
        return

    total_ok, total_err = 0, 0

    for category in category_dirs:
        images = [
            f for f in category.iterdir()
            if f.is_file() and f.suffix.lower() in SUPPORTED_EXTENSIONS
        ]
        ok, err = 0, 0
        for ok, src in enumerate(images):
            dst = output_root / category.name / f"{category.name}_{ok:02d}{src.suffix}"
            try:
                preprocess(src, dst)
                ok += 1
            except Exception as e:
                print(f"  [ERROR] {src.name}: {e}")
                err += 1

        print(f"{category.name:20s}  {ok} saved, {err} failed")
        total_ok += ok
        total_err += err

    print(f"\nDone — {total_ok} images saved to '{output_root}', {total_err} errors.")


if __name__ == "__main__":
    process_dataset(INPUT_DIR, OUTPUT_DIR)