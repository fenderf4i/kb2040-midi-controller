import json
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

WORKSPACE = Path(r"C:\Users\mike_\Documents\Codex\2026-05-16\i-want-to-start-a-project")
DATA_PATH = WORKSPACE / "outputs" / "hx_stomp_midi_cc" / "hx_stomp_presets.json"
OUT_DIR = WORKSPACE / "outputs" / "display_previews"
OUT_PATH = OUT_DIR / "oled_preset_preview.png"

OLED_W = 128
OLED_H = 64
SCALE = 6


def load_font(size):
    candidates = [
        r"C:\Windows\Fonts\consola.ttf",
        r"C:\Windows\Fonts\lucon.ttf",
        r"C:\Windows\Fonts\cour.ttf",
    ]
    for candidate in candidates:
        if Path(candidate).exists():
            return ImageFont.truetype(candidate, size)
    return ImageFont.load_default()


def text_width(draw, text, font):
    bbox = draw.textbbox((0, 0), text, font=font)
    return bbox[2] - bbox[0]


def fit_text(draw, text, font, max_width):
    if text_width(draw, text, font) <= max_width:
        return text

    ellipsis = "..."
    for length in range(len(text), 0, -1):
        candidate = text[:length].rstrip() + ellipsis
        if text_width(draw, candidate, font) <= max_width:
            return candidate
    return ellipsis


def wrap_text(draw, text, font, max_width, max_lines):
    words = text.split()
    lines = []
    current = ""

    for word in words:
      candidate = word if not current else f"{current} {word}"
      if text_width(draw, candidate, font) <= max_width:
          current = candidate
      else:
          if current:
              lines.append(current)
          current = word
          if len(lines) == max_lines:
              break

    if len(lines) < max_lines and current:
        lines.append(current)

    if len(lines) > max_lines:
        lines = lines[:max_lines]

    if lines and text_width(draw, lines[-1], font) > max_width:
        lines[-1] = fit_text(draw, lines[-1], font, max_width)

    joined = " ".join(lines)
    if len(joined) < len(text) and lines:
        lines[-1] = fit_text(draw, lines[-1], font, max_width)

    return lines


with DATA_PATH.open("r", encoding="utf-8") as f:
    data = json.load(f)

presets = data["presets"]
preview = next((p for p in presets if p["function"] == "Looper Record"), presets[0])

img = Image.new("RGB", (OLED_W * SCALE, OLED_H * SCALE), "black")
draw = ImageDraw.Draw(img)

font_small = load_font(9 * SCALE)
font_mid = load_font(11 * SCALE)
font_bottom = load_font(10 * SCALE)

white = (235, 255, 245)
dim = (90, 110, 100)

pad = 4 * SCALE
screen_w = OLED_W * SCALE
screen_h = OLED_H * SCALE

section = fit_text(draw, preview["section"], font_small, screen_w - 2 * pad)
draw.text((pad, 2 * SCALE), section, fill=white, font=font_small)
draw.line((pad, 14 * SCALE, screen_w - pad, 14 * SCALE), fill=dim, width=1)

function_lines = wrap_text(draw, preview["function"], font_mid, screen_w - 2 * pad, 2)
line_height = 12 * SCALE
total_height = len(function_lines) * line_height
y = 26 * SCALE - total_height // 2
for line in function_lines:
    x = (screen_w - text_width(draw, line, font_mid)) // 2
    draw.text((x, y), line, fill=white, font=font_mid)
    y += line_height

bottom_y = 51 * SCALE
draw.line((pad, 49 * SCALE, screen_w - pad, 49 * SCALE), fill=dim, width=1)
draw.text((pad, bottom_y), f"CC {preview['cc']}", fill=white, font=font_bottom)
value_text = f"VAL {preview['value']}"
draw.text((screen_w - pad - text_width(draw, value_text, font_bottom), bottom_y), value_text, fill=white, font=font_bottom)

OUT_DIR.mkdir(parents=True, exist_ok=True)
img.save(OUT_PATH)

print(json.dumps({"previewPreset": preview, "image": str(OUT_PATH)}, indent=2))
