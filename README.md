# XMPlay FFmpeg Audio Decoder Plugin

基于 FFmpeg 的 XMPlay 音频解码插件，支持 MP3, AAC, OGG, Opus, FLAC, WAV, APE, WMA, AC3, DTS 等格式。

## 编译

GitHub Actions 自动编译，使用 VS2022 + v141_xp 工具集。

## 本地编译

1. 安装 VS2022，勾选 "MSVC v141 - VS 2017 C++ x64/x86 build tools" 和 "C++ Windows XP Support"
2. 运行 `build.bat`（自动下载 FFmpeg 源码并编译）
3. 输出在 `Release/xmp-ffmpeg.dll`

## 安装

把 `xmp-ffmpeg.dll` 放到 XMPlay 插件目录即可。
