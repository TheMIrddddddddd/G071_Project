"""Create an OLED fast-mode GIF using Pillow and the supplied lightning motion.

Usage: python scripts/render_fast_mode_preview.py
Requires: Pillow. Outputs a native 128x64 GIF and a nearest-neighbour 4x preview.
The bundled source GIF is a user-provided reference, preserved unchanged.
"""
from pathlib import Path
import argparse
from PIL import Image, ImageDraw, ImageSequence

ROOT = Path(__file__).resolve().parents[1]


def crossed_out(base, amount):
    """Off stroke grows from upper-left to lower-right, without moving the bolt."""
    image = base.copy()
    if amount <= 0:
        return image
    left, top, right, bottom = base.getbbox()
    start = (max(0, left - 3), top + 5)
    end = (min(base.width - 1, right + 2), bottom - 4)
    end = tuple(round(a + (b - a) * amount) for a, b in zip(start, end))
    pen = ImageDraw.Draw(image)
    pen.line((start, end), fill=0, width=4)  # clearance across the white outline
    pen.line((start, end), fill=255, width=2)
    return image


def smoothstep(t):
    return t * t * (3 - 2 * t)


def save_gif(path, frames, durations, scale=1):
    scaled = [f.resize((128 * scale, 64 * scale), Image.Resampling.NEAREST)
              for f in frames]
    scaled[0].save(path, save_all=True, append_images=scaled[1:],
                   duration=durations, loop=0, disposal=2, optimize=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path,
                        default=ROOT / "scripts/assets/lightning_normal_to_fast.gif")
    parser.add_argument("--output-dir", type=Path, default=ROOT / "Docs/previews")
    args = parser.parse_args()
    source_frames, source_durations = [], []
    with Image.open(args.source) as gif:
        if gif.size != (128, 64):
            raise ValueError("Expected a 128x64 source GIF")
        for frame in ImageSequence.Iterator(gif):
            source_frames.append(frame.convert("L").point(lambda v: 255 if v >= 128 else 0))
            source_durations.append(frame.info.get("duration", 40))
    if len(source_frames) < 2 or source_frames[0].getbbox() is None:
        raise ValueError("Source must contain the outline-to-filled transition")

    frames = [crossed_out(source_frames[0], 1.0)]
    durations = [600]
    # Retract the OFF slash, then play the user's original illumination motion.
    for step in range(1, 9):
        frames.append(crossed_out(source_frames[0], 1 - smoothstep(step / 8)))
        durations.append(40)
    for i in range(1, len(source_frames) - 1):
        frames.append(source_frames[i])
        durations.append(max(20, source_durations[i]))
    frames.append(source_frames[-1])
    durations.append(max(700, source_durations[-1]))
    on_index = len(frames) - 1
    # Turn off by reversing the fill, then restore the diagonal OFF stroke.
    for frame in reversed(source_frames[:-1]):
        frames.append(frame)
        durations.append(20)
    for step in range(1, 9):
        frames.append(crossed_out(source_frames[0], smoothstep(step / 8)))
        durations.append(40)

    out = args.output_dir
    out.mkdir(parents=True, exist_ok=True)
    native = out / "fast_mode_off_on_128x64.gif"
    preview = out / "fast_mode_off_on_preview.gif"
    save_gif(native, frames, durations)
    save_gif(preview, frames, durations, 4)
    # Native pixels are strictly monochrome, and the loop joins identical OFF frames.
    assert frames[0].tobytes() == frames[-1].tobytes()
    assert frames[on_index].tobytes() == source_frames[-1].tobytes()
    for path in (native, preview):
        with Image.open(path) as gif:
            total = 0
            for frame in ImageSequence.Iterator(gif):
                assert set(frame.convert("L").getdata()) <= {0, 255}
                total += frame.info.get("duration", 0)
            assert total == sum(durations)
    comparison = Image.new("L", (1024, 256))
    comparison.paste(frames[0].resize((512, 256), Image.Resampling.NEAREST), (0, 0))
    comparison.paste(frames[on_index].resize((512, 256), Image.Resampling.NEAREST), (512, 0))
    comparison.save(out / "fast_mode_off_on_states.png")
    print(f"PASS: {native}, {sum(durations)} ms, monochrome, seamless OFF endpoints")
    print(preview)


if __name__ == "__main__":
    main()
