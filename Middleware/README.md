# 中间件来源

当前完整框架包括 OledRefresh、u8g2、Astra、smooth_ui_toolkit 和 SHOWCASE。说明及验证见 `../Docs/G071_DISPLAY_FRAMEWORK.md`；`showcase-source-manifest.json` 固定相邻工程的来源哈希和本地适配哈希，`python scripts/verify_ui_sources.py` 可校验。Astra 的 GPL-3.0 与 smooth_ui_toolkit 的原许可证均保留在各 Third_Party 目录。

- `Ringbuffer`：沿用当前工程的环形缓冲区。
- `Ports/u8g2`、`Third_Party/u8g2`：从相邻 `oled_test/UserMiddlewares` 复制绘图所需子集，版本 2.37.1，提交 `414efcdb7a908667ea461c9e3a3a31da14fc4ea5`。当前项目可独立构建。
- u8g2 源码保留原版权声明及 `Third_Party/u8g2/LICENSE`；抽取的 5×7 字体保留其 public-domain 声明。端口只操作内存，显示总线由 BSP 独立管理。
- `u8g2-subset-manifest.json` 记录复制文件的来源 SHA256；端口及 CMake 的本地注释调整另记录在 `local_sha256`，厂商源码未修改。
