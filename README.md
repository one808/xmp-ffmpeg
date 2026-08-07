# XMPlay FFmpeg Audio Decoder Plugin

An audio decoder plugin for XMPlay based on FFmpeg. Supports MP3, AAC, OGG, Opus, FLAC, WAV, APE, WMA, AC3, DTS, MPC, WV, TTA, TAK, ALAC, AMR, and more.

## Features

- Based on FFmpeg 3.4.9 for Windows XP compatibility
- Static linking - no external DLL dependencies
- Supports 30+ audio formats with file header detection
- Built with MSYS2/MinGW and MSVC v143 toolset

## Supported Formats

| Format | Extensions | Header Check |
|--------|------------|--------------|
| MP3 | mp3, mp2, mpga | ID3 tag / MP3 sync word |
| AAC | aac | ADTS header |
| FLAC | flac | fLaC |
| OGG/Opus | ogg, oga, opus | OggS |
| WAV | wav | RIFF |
| AIFF | aiff | FORM |
| APE | ape | MAC |
| WavPack | wv | wvpk |
| TTA | tta | TTA1 |
| WMA | wma, asf, wmv | GUID |
| AC3 | ac3 | 0x0B77 |
| DTS | dts, thd, mlp, eac3 | DTS headers |
| MP4/MOV | mp4, mov, m4a, m4b, 3gp | ftyp |
| Matroska | webm, mka, mkv | ftyp |
| AVI | avi | RIFF |

## Building

GitHub Actions builds automatically. Manual build:

1. Install MSYS2 with MinGW32 toolchain
2. Clone this repository
3. Run the build workflow or use `build.bat`

## Installation

Copy `xmp-ffmpeg.dll` to the XMPlay plugins directory.

## Debug Version

Two DLLs are built:
- `xmp-ffmpeg.dll` - Normal release version
- `xmp-ffmpeg-debug.dll` - Debug version with logging to `xmp-ffmpeg-debug.log`

## License

GPL v2 or later (same as FFmpeg)
