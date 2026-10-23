"""Render the reference wiper symbol as an OLED OFF/ON preview with Pillow.
The fixed windshield and moving arm are drawn separately from the supplied
switch symbol and cleaned reference in Docs/previews/wiper/wiper_extracted.png.
Usage: python scripts/render_wiper_preview.py
"""
from pathlib import Path
import math
from PIL import Image, ImageChops, ImageDraw, ImageSequence

OUT = Path(__file__).resolve().parents[1] / "Docs/previews/wiper"
SCALE = 8
FRAME_MS = 40
OFF_MS = 800
SWEEP_MS = 1200
SWEEPS = 4
PERIOD_MS = OFF_MS * 2 + SWEEP_MS * SWEEPS
PIVOT = (64, 51)  # User-marked lower centre, below the inner windshield arc.
ARM_LENGTH = 35
SWEEP_ANGLE = 64


def quadratic(a, b, c):
    return [((1-t)**2*a[0]+2*(1-t)*t*b[0]+t*t*c[0],
             (1-t)**2*a[1]+2*(1-t)*t*b[1]+t*t*c[1])
            for t in (i/80 for i in range(81))]


def line(draw, points, width):
    pts = [(round(x*SCALE), round(y*SCALE)) for x, y in points]
    draw.line(pts, fill=255, width=round(width*SCALE), joint="curve")
    radius = width*SCALE/2
    for x, y in (pts[0], pts[-1]):
        draw.ellipse((x-radius,y-radius,x+radius,y+radius), fill=255)


def windshield():
    image = Image.new("L", (128*SCALE,64*SCALE))
    draw = ImageDraw.Draw(image)
    # Broad upper arc and concave lower arc reproduce the photographed symbol.
    points = quadratic((24,30),(64,-2),(104,30))
    points += [(85,49)]
    points += quadratic((85,49),(64,34),(43,49))
    points += [(24,30)]
    line(draw, points, 2)
    return image


def monochrome(image):
    return image.resize((128,64),Image.Resampling.LANCZOS).point(lambda v:255 if v>=128 else 0)


def render(angle):
    # Rasterize the moving part separately so resampling cannot alter the frame.
    image = Image.new("L", (128*SCALE,64*SCALE))
    draw = ImageDraw.Draw(image)
    pivot = PIVOT
    theta = math.radians(angle)
    tip = (pivot[0]+ARM_LENGTH*math.sin(theta),pivot[1]-ARM_LENGTH*math.cos(theta))
    blade_start = (pivot[0]+12*math.sin(theta),pivot[1]-12*math.cos(theta))
    line(draw,[pivot,tip],2)
    line(draw,[blade_start,tip],2.5)
    px,py = (v*SCALE for v in pivot)
    r=1.5*SCALE
    draw.ellipse((px-r,py-r,px+r,py+r),fill=255)
    return ImageChops.lighter(monochrome(windshield()), monochrome(image))


def angle_at(ms):
    if ms<OFF_MS or ms>=OFF_MS+SWEEPS*SWEEP_MS:
        return -float(SWEEP_ANGLE)
    # Continuous reciprocation with gentle reversal; no stop between sweeps.
    return -SWEEP_ANGLE*math.cos(2*math.pi*(ms-OFF_MS)/SWEEP_MS)


def save_gif(path, frames, scale):
    enlarged=[f.resize((128*scale,64*scale),Image.Resampling.NEAREST) for f in frames]
    enlarged[0].save(path,save_all=True,append_images=enlarged[1:],duration=FRAME_MS,
                     loop=0,disposal=2,optimize=False)


def main():
    OUT.mkdir(parents=True,exist_ok=True)
    frames=[render(angle_at(t)) for t in range(0,PERIOD_MS,FRAME_MS)]
    parked=frames[0].tobytes()
    assert all(f.tobytes()==parked for f in frames[:OFF_MS//FRAME_MS])
    assert all(f.tobytes()==parked for f in frames[-OFF_MS//FRAME_MS:])
    static=monochrome(windshield())
    static_pixels=[i for i,v in enumerate(static.getdata()) if v]
    for frame in frames:
        pixels=frame.tobytes()
        assert all(pixels[i]==255 for i in static_pixels), "Windshield outline moved"
        assert set(pixels)<={0,255}
    assert len({f.tobytes() for f in frames})>10
    frames[0].save(OUT/"wiper_static_128x64.png")
    frames[0].resize((512,256),Image.Resampling.NEAREST).save(OUT/"wiper_static_preview.png")
    save_gif(OUT/"wiper_off_on_128x64.gif",frames,1)
    save_gif(OUT/"wiper_off_on_preview.gif",frames,4)
    # Separate ON-only clip demonstrates uninterrupted sweeping.
    active=frames[OFF_MS//FRAME_MS:(OFF_MS+SWEEP_MS)//FRAME_MS]
    save_gif(OUT/"wiper_running_preview.gif",active,4)
    with Image.open(OUT/"wiper_off_on_128x64.gif") as gif:
        assert sum(f.info.get("duration",0) for f in ImageSequence.Iterator(gif))==PERIOD_MS
    print(f"PASS: {PERIOD_MS} ms, {SWEEPS} continuous sweeps, fixed outline, OFF holds still")
    print(OUT/"wiper_off_on_preview.gif")


if __name__ == "__main__":
    main()
