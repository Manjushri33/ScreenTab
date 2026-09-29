from pathlib import Path
from PIL import Image

root = Path(__file__).resolve().parents[1]
source = root / "assets" / "app-icon.png"
target = root / "assets" / "app-icon.ico"

sizes = [(16, 16), (20, 20), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)]

image = Image.open(source).convert("RGBA")
image.save(target, format="ICO", sizes=sizes, bitmap_format="png")
print(f"Generated {target} with {len(sizes)} icon sizes")
