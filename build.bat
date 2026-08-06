@echo off
REM Build xmp-ffmpeg locally (run from VS2022 x86 Native Tools Command Prompt)
REM Requires: VS2022 with v141_xp toolset, MinGW in PATH

echo ============================================
echo  xmp-ffmpeg local build
echo ============================================
echo.

REM Download FFmpeg if not present
if not exist "ffmpeg-src\configure" (
    echo Downloading FFmpeg 9.0...
    curl -L -o ffmpeg-9.0.tar.gz https://ffmpeg.org/releases/ffmpeg-9.0.tar.gz
    tar xzf ffmpeg-9.0.tar.gz
    move ffmpeg-9.0 ffmpeg-src
)

REM Download XMPlay SDK if headers not present
if not exist "xmpin.h" (
    echo Downloading XMPlay SDK...
    curl -L -o xmp-sdk.zip https://www.un4seen.com/files/xmp-sdk.zip
    7z x xmp-sdk.zip -oxmp-sdk-tmp -y
    copy /y xmp-sdk-tmp\xmpin.h .
    copy /y xmp-sdk-tmp\xmpfunc.h .
)

REM Build FFmpeg if not built
if not exist "ffmpeg\lib\avformat.lib" (
    echo Building FFmpeg...
    cd ffmpeg-src
    mkdir ffbuild && cd ffbuild
    ..\configure --toolchain=msvc --arch=x86 --target-os=win32 --enable-static --disable-shared --disable-programs --disable-doc --disable-network --disable-everything --disable-avfilter --disable-avdevice --disable-swscale --disable-debug --disable-x86asm --enable-protocol=file --enable-demuxer=mp3,aac,ogg,flac,wav,aiff,ape,asf,matroska,mp4,mov,avi,dts,eac3,mlp,truehd,mpc,tta,wv,amr,pcm_s16le,pcm_f32le --enable-decoder=mp3,mp3float,aac,aac_latm,vorbis,opus,flac,wmav1,wmav2,ac3,eac3,dca,truehd,alac,ape,tta,wavpack,mpc7,mpc8,amrnb,amrwb,pcm_s16le,pcm_s16be,pcm_s24le,pcm_s32le,pcm_f32le,pcm_f32be,pcm_f64le,pcm_f64be,pcm_u8,pcm_alaw,pcm_mulaw,adpcm_ima_qt,adpcm_ima_wav,adpcm_ms,adpcm_swf,adpcm_yamaha --enable-parser=mpegaudio,aac,ac3,dca,flac,vorbis,opus
    make -j%NUMBER_OF_PROCESSORS%
    cd ..\..

    mkdir ffmpeg\include\libavformat
    mkdir ffmpeg\include\libavcodec
    mkdir ffmpeg\include\libavutil
    mkdir ffmpeg\include\libswresample
    mkdir ffmpeg\lib
    xcopy /y /q ffmpeg-src\libavformat\*.h ffmpeg\include\libavformat\
    xcopy /y /q ffmpeg-src\libavcodec\*.h ffmpeg\include\libavcodec\
    xcopy /y /q ffmpeg-src\libavutil\*.h ffmpeg\include\libavutil\
    xcopy /y /q ffmpeg-src\libswresample\*.h ffmpeg\include\libswresample\
    copy /y ffmpeg-src\ffbuild\config.h ffmpeg\include\
    copy /y ffmpeg-src\ffbuild\libavutil\avconfig.h ffmpeg\include\libavutil\
    copy /y ffmpeg-src\ffbuild\libavformat\avformat.lib ffmpeg\lib\
    copy /y ffmpeg-src\ffbuild\libavcodec\avcodec.lib ffmpeg\lib\
    copy /y ffmpeg-src\ffbuild\libavutil\avutil.lib ffmpeg\lib\
    copy /y ffmpeg-src\ffbuild\libswresample\swresample.lib ffmpeg\lib\
)

echo Building xmp-ffmpeg...
msbuild xmp-ffmpeg.vcxproj /p:Configuration=Release /p:Platform=Win32 /p:PlatformToolset=v141_xp /m

echo.
echo ============================================
echo  Done! Output: Release\xmp-ffmpeg.dll
echo ============================================
pause
