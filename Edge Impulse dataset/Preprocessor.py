"""
Preprocess hand gesture images:
  1. Grayscale
  2. Resize to 64x64
  3. Histogram equalization  (normalize lighting)
  4. Otsu thresholding       (isolate hand from background)

SENSITIVITY:
  THRESHOLD_BIAS shifts the auto-computed Otsu threshold.
  Range: -127 to +127
    0   = pure Otsu (default)
    +N  = higher threshold → less sensitive, ignores more bright background
    -N  = lower threshold  → more sensitive, keeps more detail

Input structure:
  input_folder/
    category1/  category2/  ...

Output structure:
  output_folder/
    category1/  category2/  ...
"""

from pathlib import Path
import numpy as np
from PIL import Image

INPUT_DIR      = "raw"
OUTPUT_DIR     = "preprocessed"
THRESHOLD_BIAS = 40

SUPPORTED_EXTENSIONS = {".jpg", ".jpeg", ".png", ".bmp", ".tiff", ".tif", ".webp"}


# ── core pipeline ────────────────────────────────────────────────────────────

def histogram_equalize(gray: np.ndarray) -> np.ndarray:
    hist, _ = np.histogram(gray.flatten(), bins=256, range=(0, 256))
    cdf = hist.cumsum()
    cdf_min = cdf[cdf > 0].min()
    total = gray.size
    lut = np.round((cdf - cdf_min) / (total - cdf_min) * 255).astype(np.uint8)
    return lut[gray]


def otsu_threshold(gray: np.ndarray, bias: int = 0) -> np.ndarray:
    """
    Compute Otsu's optimal threshold, apply bias, then binarize.
      bias > 0 → raise threshold → suppress bright background elements
      bias < 0 → lower threshold → keep more dim detail
    """
    hist, _ = np.histogram(gray.flatten(), bins=256, range=(0, 256))
    total = gray.size
    total_sum = np.dot(np.arange(256), hist)

    best_thresh, best_var = 0, 0.0
    bg_weight, bg_sum = 0.0, 0.0

    for t in range(256):
        bg_weight += hist[t]
        if bg_weight == 0:
            continue
        fg_weight = total - bg_weight
        if fg_weight == 0:
            break

        bg_sum += t * hist[t]
        bg_mean = bg_sum / bg_weight
        fg_mean = (total_sum - bg_sum) / fg_weight

        between_var = bg_weight * fg_weight * (bg_mean - fg_mean) ** 2
        if between_var > best_var:
            best_var = between_var
            best_thresh = t

    final_thresh = int(np.clip(best_thresh + bias, 0, 255))
    return np.where(gray >= final_thresh, 255, 0).astype(np.uint8)


def preprocess(src_path: Path, dst_path: Path) -> None:
    with Image.open(src_path) as img:
        gray      = np.array(img.convert("L").resize((64, 64), Image.LANCZOS))
        equalized = histogram_equalize(gray)
        binary    = otsu_threshold(equalized, bias=THRESHOLD_BIAS)

        dst_path.parent.mkdir(parents=True, exist_ok=True)
        # always save as PNG — JPEG is lossy and corrupts the binary image
        dst_path = dst_path.with_suffix(".png")
        Image.fromarray(binary).save(dst_path, format="PNG")


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

    print(f"Threshold bias: {THRESHOLD_BIAS:+d}\n")
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