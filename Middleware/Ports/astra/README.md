# Astra 的 STM32/u8g2 移植

上游：https://github.com/AstraThreshold/oled-ui-astra

固定提交：`88716a62f84c41838458fd159841fe67e6a86f87`。保留整个 `Core/Src/astra/`、基础 `hal.h/.cpp`、README与GPL-3.0 LICENSE，共25个文件；排除上游板级HAL、外设、u8g2副本和字体。

`astra-source-manifest.json` 记录归档SHA256、上游原文件SHA256、LF标准化后的本地文件SHA256、补丁SHA256。校验：

```powershell
python UserMiddlewares/verify_astra_vendor.py
```

源文件由 `scripts/stage_astra_vendor.py` 从固定归档提取。已有文件时默认拒绝覆盖；维护者明确重建时才使用 `--refresh`，并复查补丁与测试。

## 本模式实际使用范围

链接基础HAL、Menu/List/Tile、Selector、Camera，使用头文件中的原生move/blur。`astra_port.cpp` 把绘图映射到已有u8g2上下文；所有结果先写入独立1024字节模型帧，外层应用通过现有驱动接口发布。

不链接上游Launcher/Widget实现：其原文件保留供后续接入和审阅。本模式使用定时导航脚本，弹窗直接调用原生blur和move进行非阻塞动画；不调用上游阻塞Launcher、HAL::delay或菜单延时退出。不能据此声称已接入全部控件或原开发板应用。

HAL::canvasUpdate为空，不直接发送OLED；HAL::delay中止，防止无意引入阻塞。图元进入u8g2前处理负坐标裁剪；图标采用逐像素XBM读取以支持屏幕外横向移动。演示文字只使用ASCII，统一已有5×7字体。

## 可审计移植补丁

完整差异保存在 `patches/stm32-port.patch`，涉及11个文件：

- 二维坐标返回值由 `vector<float>` 改为 `array<float,2>`，列表边界使用定长array，避免每帧动态分配；保留菜单和图标容器在初始化构建。
- 字体指向本项目u8g2及5×7字体，不引用上游开发板目录。
- 配置工厂由内部链接static函数改为inline，使不同编译单元共享一份配置；Camera每次更新读取配置，确保速度73实际生效。
- Camera在已确认List类型后使用static_cast，兼容既有 `-fno-rtti`；视口判断的按位或改成逻辑或。
- Selector坐标与非拥有菜单指针初始化、目标下标检查、destroy不delete外部持有的菜单并正确返回。
- 取消每个文本节点两张未使用的120字节默认图片；首页四张30×30图标在模型初始化显式安装。
- 模糊/退出辅助函数缓冲长度从8位改成size_t，覆盖完整1024字节；每次重新取得缓冲指针，避免沿用旧绘图缓冲。
- v37.1将菜单与选框中原有move调用移入独立update方法；模型75Hz推进，render只绘制当前状态。图标按裁剪后的整数坐标直接写入CPU模型缓冲，使用与u8g2相同的三种绘图颜色。

菜单树和图标只初始化一次，随后应用封闭C++分配器。75Hz模型与DMA互不占用同一绘图缓冲；运行与错误恢复的主机测试均验证初始化后分配/释放次数不变。

完整场景、参数和验证边界见 [v37说明](../../../Docs/OLED_V37_SHOWCASE.md)。
当前菜单优先播放与板上耗时见 [v37.1说明](../../../Docs/OLED_V37_1_SHOWCASE_TIMING.md)。
