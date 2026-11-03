<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# 资源目录（Assets）

本目录集中存放可复用的资源（字库、图片、音乐、电池参数等），按资源类型分子目录管理。每个资源放在其类型对应的子目录，并记录放置路径、命名方式、集成方式与来源/许可。二进制资源（字体、图片、音频）不属于纯 markdown 文档，请勿与文档混放。涉及版权/授权的资源需注明来源与许可。

## 字库（fonts）

可复用的字库文件与生成的字库源码放在 `fonts/`。

- 命名要能反映字族、字重、字级与格式。
- 记录来源、许可、字符范围、转换命令与目标放置路径。
- 添加字库前评估 Flash 与内部 RAM 影响；ESP32-C3 无 PSRAM。
- 不提交许可不允许分发的字库。

### 木鱼应用字库

- `fonts/muyu-noto-sans-sc-subset.otf` 是
  [Noto Sans SC Regular](https://github.com/notofonts/noto-cjk/tree/main/Sans)
  的 29.8 KB 应用子集，按 [SIL Open Font License 1.1](fonts/muyu-noto-OFL.txt)
  分发；原始字体与子集的 SHA-256 记在 `fonts/muyu-font-manifest.json`。
- `fonts/muyu-characters.txt` 是界面字符清单，另包含可打印 ASCII U+0020–U+007E。
  该子集不支持任意中文输入。
- `fonts/muyu_font_16.c` / `fonts/muyu_font_20.c` 分别导出 `muyu_font_16` /
  `muyu_font_20`，由 `lv_font_conv@1.5.3` 生成，采用不压缩的 4 bpp 格式。
  应用仅编译一次，并在每个中文标签上选择该字体；保留 LVGL 缺字占位符。
- 安装固定的资源开发依赖：
  `npm install --prefix build/muyu-assets-tools --no-audit --no-fund --save-exact lv_font_conv@1.5.3 sharp@0.34.5`，
  再执行 `node tools/generate_muyu_assets.mjs`，即可从保留的 OTF 和 SVG 重建资源。
  字符清单变化时，用 `fonttools==4.60.1` 和
  `python tools/generate_muyu_assets.py --subset /path/to/NotoSansSC-Regular.otf`
  从官方完整字体重新生成子集。
- `python3 tools/generate_muyu_assets.py --check` 检查实际生成的 LVGL cmap、
  界面真实字符串覆盖和字形 ID，并以明确缺失的 U+9F98 为反例。
  位图显示效果和控件字体绑定仍须在实机验收。

### 掼蛋记分牌字库

- `fonts/guandan_noto_subset.otf` 是 Noto Sans SC Regular 2.004 的应用子集；
  `fonts/guandan_montserrat_bold_subset.ttf` 是固定 LVGL 9.5.0 源码中
  Montserrat Bold 8.000 的子集。两者均采用 SIL OFL 1.1 许可，分别保存在
  `fonts/guandan_noto_OFL.txt` 和 `fonts/guandan_montserrat_OFL.txt`；
  上游地址和原始/子集哈希记在 `fonts/guandan_font_manifest.json`。
- `fonts/guandan_characters.txt` 加可打印 ASCII 构成 131 字符清单，供
  `guandan_font_14`、`guandan_font_18` 使用。大字 `guandan_font_84` 仅含
  `0123456789JQKA`，覆盖 2 至 A3 的全部 15 级；均为不压缩的 4 bpp LVGL 字库。
- 安装固定转换器：
  `npm install --prefix build/guandan-assets-tools --no-audit --no-fund --save-exact lv_font_conv@1.5.3`，
  然后执行 `python3 tools/generate_guandan_assets.py --generate`。
  字符变化时先用 `fonttools==4.60.1`、完整字体及脚本的两个 `--subset-*`
  参数重新子集化，再生成字库。
- `python3 tools/generate_guandan_assets.py --check` 检查实际生成的 cmap、
  界面字符串覆盖、缺字反例及子集哈希。`python3 tools/render_guandan_preview.py`
  使用真实 LVGL 9.5.0、24 KB 内存池渲染实际应用界面，配置
  `LV_USE_THEME_DEFAULT=0`、`LV_USE_THEME_SIMPLE=1` 与固件一致，避免未使用的完整
  主题占用大字形展开缓冲所需的内存。这些主机检查不能代替实机显示验收。

## 图片（images）

可复用的源图与生成的显示资产放在 `images/`。

| 文件 | 尺寸与格式 | 用途与来源 |
| --- | --- | --- |
| `images/muyu-woodfish.svg` / `images/muyu-woodfish.png` / `images/muyu_woodfish.c` | 154 × 115；可编辑 SVG、PNG 预览、RGB565 C 数组 | 原创木鱼与木槌插画，采用仓库 MIT 许可。由 `tools/generate_muyu_assets.mjs` 生成；`muyu_woodfish` 占 Flash 35,420 字节，暖纸底色 `#F6EEDC`。 |
| [`images/home.jpg`](images/home.jpg) | 3840 × 2160，JPEG | 嵌入中英文项目 README 的产品主图，突出 AI Passport 产品形象与开放、人人可创作的理念。 |
| [`images/readme-hardware-specs.png`](images/readme-hardware-specs.png) | 2172 × 724，PNG RGBA | 保留为可选技术参考图，不再用于首页主视觉。于 2026-09-17 使用内置图像生成工具为本仓库生成；已根据文档中的硬件能力契约核对图中的六项标签与参数。 |
| [`images/logo-wordmark.png`](images/logo-wordmark.png) | 1648 × 336，PNG RGBA | 从仓库原始 `images/logo.png` 中精确裁切并去除背景的黑色字标；用于中英文项目 README 的浅色主题。 |
| [`images/logo-wordmark-dark.png`](images/logo-wordmark-dark.png) | 1648 × 336，PNG RGBA | 提取字标的白色版本；README 使用 `<picture>` 在 GitHub 深色主题下显示。 |

- 使用描述性命名，并记录尺寸、像素格式、转换步骤与目标路径。
- 优先采用适合 240 × 320 RGB565 显示的格式，并纳入 Flash 与内部 RAM 考量。
- 许可允许时保留可编辑源文件，并记录来源与许可。
- 图片中不得包含设备二维码秘密、凭证或个人数据。

原创掼蛋封面和重置示意图：`images/guandan-{cover,reset}-1152x1536.{svg,png}`。两图均为 1152 × 1536 的竖版界面渲染，不是实机照片。SVG 文字已转路径，无外部依赖；对应 PNG 已完成渲染并在提交前逐张检查。插画采用仓库 MIT 许可；Noto Sans SC 和 Montserrat 字形保留上述字体许可声明。

## 音乐与音效（music）

可复用的音乐与音效源码放在 `music/`。

- 记录来源、许可、采样率、位深、声道、转换命令与目标路径。
- 与当前 BSP 音频路径匹配时优先采用 16 kHz、16 位单声道 PCM。
- 嵌入音频前评估 Flash 与内部 RAM 成本；长录音应流式或分块。
- 无再分发许可不提交媒体文件。

## 电池参数（battery）

[`battery/wanli-500mah-profile2-xil40x-20261020.txt`](battery/wanli-500mah-profile2-xil40x-20261020.txt)
保存万锂新能源 500 mAh 电芯的 CW2017 参数（profile2、XIL40X、2026-10-20）。
该文件是项目提供的 `喜梨_智能工卡_万锂新能源500mAh_profile2_XIL40X_20261020.txt`
的原样副本，按维护者要求保留用于本次固件更新；原文件没有附带单独的许可说明。

格式为 80 行两位十六进制数字，每行一个字节，按寄存器顺序排列。
BSP 在 `components/bsp/src/bsp_battery.c` 中嵌入这 80 字节，主机检查会验证
源文件及数组内容完全一致。文本文件 SHA-256：
`7ea96eba4634f4495a37c25ffacd54c1deaa0693b162c892d671a2ba68460919`。
解码后 80 字节 SHA-256：
`fa7365671093ef449c5bdcfb4adb279e3d275f888109f50b6b2f21e9b96220d5`。
这是电量计参数，不是充电配置，也不能通用于所有 500 mAh 电芯；详见
[硬件指南](../docs/hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.zh_CN.md#9-cw2017-电池计)。
