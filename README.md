# xmp-ffmpeg

XMPlay FFmpeg Input Plugin - Audio decoding plugin for XMPlay

## Supported Formats

**Decoders:** AAC, FLAC, MP3, Vorbis, Opus, ALAC, APE, AC3, DTS, TTA, WavPack, PCM

**Containers:** MKV, MKA, AVI, MP4, MOV, WebM, OGG, FLAC, WAV, AIFF, CAF, APE, WV

**Supported file extensions:**
`.aac` `.m4a` `.m4b` `.flac` `.opus` `.ogg` `.oga` `.wv` `.ape` `.ac3` `.dts` `.tta` `.caf` `.aiff` `.aif` `.au` `.snd` `.raw` `.mp3` `.mp4` `.mkv` `.mka` `.avi` `.webm` `.flv` `.mov` `.ts` `.m2ts`

## Features

- Extract audio from video containers
- Network streaming support (HTTP/FTP)
- Read metadata tags (title, artist, album, etc.)
- Display audio codec format (AAC, FLAC, etc.)
- No external DLL dependencies (FFmpeg statically linked)

## Installation

1. Download `xmp-ffmpeg.dll`
2. Copy to XMPlay's `Plugins` directory
3. Restart XMPlay

## Files

| File | Description |
|------|-------------|
| `xmp-ffmpeg.dll` | Release version (2.6MB) |
| `xmp-ffmpeg-debug.dll` | Debug version (logs to `D:\xmpffmpeg.log`) |

## Requirements

- Windows 32/64-bit
- XMPlay 3.8 or later

## Build

Built with FFmpeg 7.1 + XMPlay SDK, cross-compiled for Win32.
