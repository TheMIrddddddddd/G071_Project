# 雨刮器预览制作

用户照片先通过内置 imagegen 提取、清理为透明黑色参考图 `wiper_extracted.png`。随后参考原图的弧形挡风玻璃轮廓，用 Python/Pillow 分别绘制固定轮廓与可转动雨刮臂，输出白色单色 OLED 图案。透明参考图适合在浅色底查看。

## 最终抠图提示词

Extract the windshield wiper symbol from this photograph as a clean BLACK INK vector-style icon on a transparent background. Use opaque pure BLACK strokes, not white. It is a plain printed automotive switch symbol: fixed curved windshield outline plus one diagonal wiper arm connected at its lower centre, leaning left. Preserve the source silhouette. Completely discard the photographic background. Flat solid black ink only, sharp uniform strokes with rounded joints, no light, no glow, no shadows, no gradients, no grey, no text. Transparent inside the outline and outside the symbol. A single centred isolated black windshield-wiper pictogram, like a simple SVG export.

## GIF

128×64，纯黑白，40 ms 一帧。转轴按用户红点移到下方中央 `(64,51)`，雨刮臂长 35 像素，左右各摆动 64°，总范围 128°。关闭静止 800 ms，开启后连续四轮往复（每轮 1200 ms），关闭并停靠左侧 800 ms，总计 6400 ms。程序验证固定轮廓每帧一致、关闭时像素不变、循环首尾停靠位置一致。

运行 `python scripts/render_wiper_preview.py` 可重新生成静态图和 GIF。本文件记录预览资源的制作过程；最终动画已通过 `App/app_wiper_icon.cpp` 接入第四个状态栏位置，构建和验证记录见 `Docs/G071_DISPLAY_FRAMEWORK.md`。
