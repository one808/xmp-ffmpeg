# xmp-ffmpeg

XMPlay FFmpeg Input Plugin - 音频解码插件

## 支持格式

**解码器：** AAC, FLAC, MP3, Vorbis, Opus, ALAC, APE, AC3, DTS, TTA, WavPack, PCM

**容器：** MKV, MKA, AVI, MP4, MOV, WebM, OGG, FLAC, WAV, AIFF, CAF, APE, WV

**支持的文件后缀：**
`.aac` `.m4a` `.m4b` `.flac` `.opus` `.ogg` `.oga` `.wv` `.ape` `.ac3` `.dts` `.tta` `.caf` `.aiff` `.aif` `.au` `.snd` `.raw` `.mp3` `.mp4` `.mkv` `.mka` `.avi` `.webm` `.flv` `.mov` `.ts` `.m2ts`

## 功能

- 从视频容器中提取音频播放
- 支持网络流（HTTP/FTP）
- 自动读取元数据标签（标题、艺术家、专辑等）
- 显示音频编码格式（如 AAC、FLAC）
- 无外部 DLL 依赖（FFmpeg 已静态链接）

## 安装

1. 下载 `xmp-ffmpeg.dll`
2. 复制到 XMPlay 的 `Plugins` 目录
3. 重启 XMPlay

## 文件说明

| 文件 | 说明 |
|------|------|
| `xmp-ffmpeg.dll` | 正式版（2.6MB） |
| `xmp-ffmpeg-debug.dll` | 调试版（日志到 `D:\xmpffmpeg.log`） |

## 系统要求

- Windows 32/64 位
- XMPlay 3.8 或更高版本
