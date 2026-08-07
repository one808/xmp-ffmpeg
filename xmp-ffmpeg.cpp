// XMPlay FFmpeg input plugin
// Decodes audio via FFmpeg (libavformat + libavcodec + libswresample)
// Compatible with FFmpeg 3.4 (Windows XP support)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

// ============ DEBUG LOG ============
#define XMP_FFMPEG_DEBUG_LOG

#ifdef XMP_FFMPEG_DEBUG_LOG
static FILE *g_logfile = NULL;
static CRITICAL_SECTION g_logcs;
static volatile LONG g_callcount = 0;

static void dbglog_init(void) {
	InitializeCriticalSection(&g_logcs);
	g_logfile = fopen("xmp-ffmpeg-debug.log", "w");
	if (g_logfile) {
		fprintf(g_logfile, "[xmp-ffmpeg] Debug log started, PID=%lu\n", GetCurrentProcessId());
		fflush(g_logfile);
	}
}

static void dbglog(const char *fmt, ...) {
	if (!g_logfile) return;
	EnterCriticalSection(&g_logcs);
	va_list ap;
	va_start(ap, fmt);
	fprintf(g_logfile, "[%lu] TID=%lu ",
		GetTickCount(), GetCurrentThreadId());
	vfprintf(g_logfile, fmt, ap);
	fprintf(g_logfile, "\n");
	fflush(g_logfile);
	va_end(ap);
	LeaveCriticalSection(&g_logcs);
}

static void dbglog_close(void) {
	if (g_logfile) {
		fprintf(g_logfile, "[xmp-ffmpeg] Log closing\n");
		fflush(g_logfile);
		fclose(g_logfile);
		g_logfile = NULL;
	}
	DeleteCriticalSection(&g_logcs);
}
#else
#define dbglog_init()
#define dbglog(...)
#define dbglog_close()
#endif

#ifndef MAKEFOURCC
#define MAKEFOURCC(ch0, ch1, ch2, ch3) \
    ((DWORD)(BYTE)(ch0) | ((DWORD)(BYTE)(ch1) << 8) | \
    ((DWORD)(BYTE)(ch2) << 16) | ((DWORD)(BYTE)(ch3) << 24))
#endif

#include "xmpin.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
}

#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "swresample.lib")

static XMPFUNC_IN   *xmpfin;
static XMPFUNC_MISC *xmpfmisc;
static XMPFUNC_FILE *xmpffile;
static XMPFUNC_TEXT  *xmpftext;
static DWORD xmpver;

struct FFContext {
	AVFormatContext *fmtctx;
	AVCodecContext  *decctx;
	SwrContext      *swrctx;
	int              audiostream;
	AVPacket        *pkt;
	AVFrame         *frame;
	float           *outbuf;
	DWORD            outlen, outpos;
	QWORD            totalsamples;
	DWORD            samplerate, channels;
	int              bitspersample;
	BOOL             eof;
	double           bitrate;
};

static FFContext *cur;

static void FreeCtx(FFContext *ctx)
{
	if (!ctx) return;
	dbglog("FreeCtx(%p)", ctx);
	// Flush codec first to release any pending references
	if (ctx->decctx) {
		avcodec_flush_buffers(ctx->decctx);
		// Send flush packet to drain decoder
		avcodec_send_packet(ctx->decctx, NULL);
		// Receive and discard any remaining frames
		while (avcodec_receive_frame(ctx->decctx, ctx->frame) == 0) {
			av_frame_unref(ctx->frame);
		}
	}
	// Free in reverse order of allocation
	if (ctx->pkt)     av_packet_free(&ctx->pkt);
	if (ctx->frame)   av_frame_free(&ctx->frame);
	if (ctx->swrctx)  swr_free(&ctx->swrctx);
	if (ctx->decctx)  avcodec_free_context(&ctx->decctx);
	if (ctx->fmtctx)  avformat_close_input(&ctx->fmtctx);
	if (ctx->outbuf)  free(ctx->outbuf);
	free(ctx);
}

static int xmp_read(void *opaque, uint8_t *buf, int size)
{
	return (int)xmpffile->Read((XMPFILE)opaque, buf, (DWORD)size);
}

static int64_t xmp_seek(void *opaque, int64_t offset, int whence)
{
	XMPFILE file = (XMPFILE)opaque;
	switch (whence) {
	case SEEK_SET:
		if (xmpver >= 0x03080200) xmpffile->Seek64(file, (QWORD)offset);
		else xmpffile->Seek(file, (DWORD)offset);
		return offset;
	case SEEK_CUR: return -1;
	case SEEK_END: {
		QWORD sz = (xmpver >= 0x03080200) ? xmpffile->GetSize64(file) : xmpffile->GetSize(file);
		if (xmpver >= 0x03080200) xmpffile->Seek64(file, (QWORD)(sz + offset));
		else xmpffile->Seek(file, (DWORD)(sz + offset));
		return (int64_t)(sz + offset);
	}
	case AVSEEK_SIZE:
		return (xmpver >= 0x03080200) ? (int64_t)xmpffile->GetSize64(file) : (int64_t)xmpffile->GetSize(file);
	}
	return -1;
}

static FFContext *OpenFile(XMPFILE file)
{
	dbglog("OpenFile(%p)", file);
	FFContext *ctx = (FFContext*)calloc(1, sizeof(FFContext));
	if (!ctx) { dbglog("OpenFile: calloc failed"); return NULL; }

	uint8_t *avio_buf = (uint8_t*)av_malloc(32768);
	if (!avio_buf) { free(ctx); return NULL; }

	AVIOContext *avio = avio_alloc_context(avio_buf, 32768, 0, file, xmp_read, NULL, xmp_seek);
	if (!avio) { av_free(avio_buf); free(ctx); return NULL; }

	ctx->fmtctx = avformat_alloc_context();
	if (!ctx->fmtctx) { avio_context_free(&avio); free(ctx); return NULL; }
	ctx->fmtctx->pb = avio;

	if (avformat_open_input(&ctx->fmtctx, NULL, NULL, NULL) < 0) { FreeCtx(ctx); return NULL; }
	if (avformat_find_stream_info(ctx->fmtctx, NULL) < 0) { FreeCtx(ctx); return NULL; }

	ctx->audiostream = av_find_best_stream(ctx->fmtctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
	if (ctx->audiostream < 0) { FreeCtx(ctx); return NULL; }

	AVStream *stream = ctx->fmtctx->streams[ctx->audiostream];
	AVCodec *codec = avcodec_find_decoder(stream->codecpar->codec_id);
	if (!codec) { FreeCtx(ctx); return NULL; }

	ctx->decctx = avcodec_alloc_context3(codec);
	if (!ctx->decctx) { FreeCtx(ctx); return NULL; }
	if (avcodec_parameters_to_context(ctx->decctx, stream->codecpar) < 0) { FreeCtx(ctx); return NULL; }
	if (avcodec_open2(ctx->decctx, codec, NULL) < 0) { FreeCtx(ctx); return NULL; }

	ctx->channels = ctx->decctx->channels;
	ctx->samplerate = ctx->decctx->sample_rate;
	ctx->bitspersample = av_get_bytes_per_sample(ctx->decctx->sample_fmt) * 8;

	if (stream->duration > 0 && stream->time_base.den > 0)
		ctx->totalsamples = (QWORD)(stream->duration * (int64_t)ctx->samplerate * stream->time_base.num / stream->time_base.den);
	else if (ctx->fmtctx->duration > 0)
		ctx->totalsamples = (QWORD)(ctx->fmtctx->duration * (int64_t)ctx->samplerate / AV_TIME_BASE);

	if (stream->codecpar->bit_rate > 0) ctx->bitrate = (double)stream->codecpar->bit_rate / 1000.0;
	else if (ctx->fmtctx->bit_rate > 0) ctx->bitrate = (double)ctx->fmtctx->bit_rate / 1000.0;

	ctx->swrctx = swr_alloc();
	if (!ctx->swrctx) { FreeCtx(ctx); return NULL; }

	av_opt_set_int(ctx->swrctx, "in_channel_count",  ctx->decctx->channels, 0);
	av_opt_set_int(ctx->swrctx, "out_channel_count", ctx->decctx->channels, 0);
	av_opt_set_int(ctx->swrctx, "in_sample_rate",   ctx->samplerate, 0);
	av_opt_set_int(ctx->swrctx, "out_sample_rate",  ctx->samplerate, 0);
	av_opt_set_sample_fmt(ctx->swrctx, "in_sample_fmt",  ctx->decctx->sample_fmt, 0);
	av_opt_set_sample_fmt(ctx->swrctx, "out_sample_fmt", AV_SAMPLE_FMT_FLT, 0);

	if (swr_init(ctx->swrctx) < 0) { FreeCtx(ctx); return NULL; }

	ctx->pkt   = av_packet_alloc();
	ctx->frame  = av_frame_alloc();
	if (!ctx->pkt || !ctx->frame) { FreeCtx(ctx); return NULL; }

	dbglog("OpenFile: OK ctx=%p fmtctx=%p decctx=%p swr=%p pkt=%p frame=%p",
		ctx, ctx->fmtctx, ctx->decctx, ctx->swrctx, ctx->pkt, ctx->frame);
	return ctx;
}

static volatile LONG g_in_decode = 0;
static volatile LONG g_in_plugin = 0;

static LONG CALLBACK CrashHandler(EXCEPTION_POINTERS *ep)
{
	if (g_in_decode || g_in_plugin) {
		dbglog("CrashHandler: exception=0x%lX in plugin code (in_decode=%ld in_plugin=%ld)",
			ep->ExceptionRecord->ExceptionCode, g_in_decode, g_in_plugin);
		return EXCEPTION_EXECUTE_HANDLER; // suppress the crash
	}
	return EXCEPTION_CONTINUE_SEARCH; // not in our code, let it propagate
}

static void InstallCrashHandler(void)
{
	AddVectoredExceptionHandler(1, CrashHandler);
}

static DWORD DecodeFrame(FFContext *ctx)
{
	InterlockedIncrement(&g_in_decode);
	DWORD result = 0;

	while (1) {
		int ret = avcodec_receive_frame(ctx->decctx, ctx->frame);
		if (ret == 0) {
			int out_samples = ctx->frame->nb_samples;
			int total = out_samples * (int)ctx->channels;
			float *tmp = (float*)malloc(total * sizeof(float));
			if (!tmp) goto done;
			int converted = swr_convert(ctx->swrctx, (uint8_t**)&tmp, out_samples,
				(const uint8_t**)ctx->frame->extended_data, ctx->frame->nb_samples);
			if (converted > 0) {
				total = converted * (int)ctx->channels;
				float *newbuf = (float*)realloc(ctx->outbuf, (ctx->outlen + total) * sizeof(float));
				if (newbuf) {
					ctx->outbuf = newbuf;
					memcpy(ctx->outbuf + ctx->outlen, tmp, total * sizeof(float));
					ctx->outlen += total;
				}
			}
			free(tmp);
			av_frame_unref(ctx->frame);
			result = (converted > 0) ? total : 0;
			goto done;
		}
		if (ret == AVERROR_EOF) { ctx->eof = TRUE; result = 0; goto done; }
		if (ret != AVERROR(EAGAIN)) { result = 0; goto done; }

		ret = av_read_frame(ctx->fmtctx, ctx->pkt);
		if (ret < 0) {
			if (ret == AVERROR_EOF) { avcodec_send_packet(ctx->decctx, NULL); continue; }
			ctx->eof = TRUE; result = 0; goto done;
		}
		if (ctx->pkt->stream_index == ctx->audiostream) avcodec_send_packet(ctx->decctx, ctx->pkt);
		av_packet_unref(ctx->pkt);
	}

done:
	InterlockedDecrement(&g_in_decode);
	return result;
}

static char *BuildTags(AVFormatContext *fmtctx)
{
	AVDictionary *meta = fmtctx->metadata;
	if (!meta) return NULL;

	int count = 0;
	AVDictionaryEntry *entry = NULL;
	while ((entry = av_dict_get(meta, "", entry, AV_DICT_IGNORE_SUFFIX))) count++;
	if (!count) return NULL;

	DWORD buflen = 16;
	entry = NULL;
	while ((entry = av_dict_get(meta, "", entry, AV_DICT_IGNORE_SUFFIX)))
		buflen += (DWORD)(strlen(entry->key) + strlen(entry->value) + 2);

	char *tags = (char*)xmpfmisc->Alloc(buflen + 1);
	if (!tags) return NULL;

	char *p = tags;
	memcpy(p, "filetype\0FFmpeg", 15); p += 15;
	entry = NULL;
	while ((entry = av_dict_get(meta, "", entry, AV_DICT_IGNORE_SUFFIX))) {
		if (!entry->key || !entry->value) continue;
		DWORD klen = (DWORD)strlen(entry->key), vlen = (DWORD)strlen(entry->value);
		memcpy(p, entry->key, klen); p[klen] = 0;
		memcpy(p + klen + 1, entry->value, vlen);
		p += klen + vlen + 2;
	}
	*p = 0;
	return tags;
}

static const char *GetCodecName(FFContext *ctx)
{
	if (!ctx || !ctx->decctx) return "unknown";
	const AVCodecDescriptor *desc = avcodec_descriptor_get(ctx->decctx->codec_id);
	return (desc && desc->name) ? desc->name : "unknown";
}

// ========== XMPlay interface ==========

static BOOL WINAPI FF_CheckFile(const char *filename, XMPFILE file)
{
	BYTE head[12];
	if (xmpffile->Read(file, head, sizeof(head)) != sizeof(head)) { xmpffile->Seek(file, 0); return FALSE; }
	xmpffile->Seek(file, 0);
	DWORD dw = *(DWORD*)head;
	if (dw == MAKEFOURCC('I','D','3',3) || dw == MAKEFOURCC('I','D','3',4)) return TRUE;
	if ((head[0]==0xFF&&(head[1]&0xE0)==0xE0)) return TRUE;
	if (dw == MAKEFOURCC('f','L','a','C')) return TRUE;
	if (dw == MAKEFOURCC('O','g','g','S')) return TRUE;
	if (dw == MAKEFOURCC('R','I','F','F')) return TRUE;
	if (dw == MAKEFOURCC('F','O','R','M')) return TRUE;
	if (dw == MAKEFOURCC('f','t','y','p')) return TRUE;
	if (dw == 0x75B22630) return TRUE;
	if (dw == MAKEFOURCC('M','A','C',' ')) return TRUE;
	if (dw == 0x0B77) return TRUE;
	if (dw == 0x0180FE7F || dw == 0xFE7F0180) return TRUE;
	return TRUE;
}

static DWORD WINAPI FF_GetFileInfo(const char *filename, XMPFILE file, float **length, char **tags)
{
	FFContext *ctx = OpenFile(file);
	if (!ctx) return 0;
	if (length && ctx->totalsamples) {
		float *lens = (float*)xmpfmisc->Alloc(sizeof(float));
		lens[0] = (float)((double)ctx->totalsamples / ctx->samplerate);
		*length = lens;
	}
	if (tags) *tags = BuildTags(ctx->fmtctx);
	FreeCtx(ctx);
	return 1;
}

static DWORD WINAPI FF_Open(const char *filename, XMPFILE file)
{
	InterlockedIncrement(&g_in_plugin);
	DWORD result = 0;
	dbglog("FF_Open: ENTER filename=%s file=%p cur=%p", filename ? filename : "(null)", file, cur);
	FFContext *old = cur;
	if (old) {
		dbglog("FF_Open: WARNING cur=%p not NULL!", old);
	}
	cur = OpenFile(file);
	dbglog("FF_Open: OpenFile returned cur=%p", cur);
	if (!cur) goto done;
	if (cur->totalsamples) {
		float length = (float)((double)cur->totalsamples / cur->samplerate);
		dbglog("FF_Open: calling SetLength, xmpfin=%p", xmpfin);
		xmpfin->SetLength(length, TRUE);
		dbglog("FF_Open: SetLength done");
		if (!cur->bitrate) {
			QWORD sz = (xmpver >= 0x03080200) ? xmpffile->GetSize64(file) : xmpffile->GetSize(file);
			if (sz && length > 0) cur->bitrate = (double)sz / length * 8.0 / 1000.0;
		}
	}
	if (xmpffile->GetType(file) > XMPFILE_TYPE_FILE) {
		DWORD br = cur->bitrate ? (DWORD)(cur->bitrate * 125) : cur->samplerate * cur->channels * 2;
		xmpffile->NetSetRate(file, br);
	}
	result = 1;
done:
	dbglog("FF_Open: LEAVE cur=%p result=%lu", cur, result);
	InterlockedDecrement(&g_in_plugin);
	return result;
}

static void WINAPI FF_Close()
{
	InterlockedIncrement(&g_in_plugin);
	dbglog("FF_Close: ENTER cur=%p", cur);
	if (cur) {
		dbglog("FF_Close: freeing ctx=%p fmtctx=%p", cur, cur->fmtctx);
	}
	FreeCtx(cur);
	cur = NULL;
	dbglog("FF_Close: LEAVE cur=NULL");
	InterlockedDecrement(&g_in_plugin);
}

static char *WINAPI FF_GetTags() { return (cur && cur->fmtctx) ? BuildTags(cur->fmtctx) : NULL; }

static void WINAPI FF_SetFormat(XMPFORMAT *form)
{
	InterlockedIncrement(&g_in_plugin);
	dbglog("FF_SetFormat: cur=%p", cur);
	if (!cur) { InterlockedDecrement(&g_in_plugin); return; }
	form->res  = 4;
	form->chan = (WORD)cur->channels;
	form->rate = cur->samplerate;
	InterlockedDecrement(&g_in_plugin);
}

static void WINAPI FF_GetInfoText(char *format, char *length)
{
	if (!cur) return;
	if (format) {
		const char *name = GetCodecName(cur);
		int off = sprintf(format, "%s", name);
		if (cur->bitrate > 0) off += sprintf(format + off, " - %.0fkbps", cur->bitrate);
		sprintf(format + off, " - %dhz", cur->samplerate);
	}
}

static void WINAPI FF_GetGeneralInfo(char *buf)
{
	if (!cur) return;
	int off = sprintf(buf, "Codec\t%s", GetCodecName(cur));
	if (cur->decctx->codec->long_name) off += sprintf(buf + off, " (%s)", cur->decctx->codec->long_name);
	buf[off++] = '\r';
	if (cur->bitrate > 0) off += sprintf(buf + off, "Bit rate\t%.0f kbps\r", cur->bitrate);
	off += sprintf(buf + off, "Sample rate\t%u hz\rChannels\t%u\rResolution\t", cur->samplerate, cur->channels);
	if (cur->bitspersample > 0) off += sprintf(buf + off, "%u bit", cur->bitspersample);
	else off += sprintf(buf + off, "float");
	buf[off++] = '\r';
	if (cur->totalsamples) {
		DWORD secs = (DWORD)((double)cur->totalsamples / cur->samplerate);
		sprintf(buf + off, "Length\t%u:%02u:%02u\r", secs/3600, (secs/60)%60, secs%60);
	}
}

static void WINAPI FF_GetMessage(char *buf)
{
	if (!cur || !cur->fmtctx) return;
	AVDictionary *meta = cur->fmtctx->metadata;
	if (!meta) return;
	AVDictionaryEntry *entry = NULL;
	while ((entry = av_dict_get(meta, "", entry, AV_DICT_IGNORE_SUFFIX))) {
		if (entry->key && entry->value) buf = xmpfmisc->FormatInfoText(buf, entry->key, entry->value);
	}
}

static DWORD WINAPI FF_Process(float *buffer, DWORD count)
{
	InterlockedIncrement(&g_in_plugin);
	LONG callid = InterlockedIncrement(&g_callcount);
	if (!cur) { dbglog("FF_Process#%ld: cur=NULL RETURN", callid); InterlockedDecrement(&g_in_plugin); return 0; }
	dbglog("FF_Process#%ld: ENTER cur=%p count=%lu", callid, cur, count);
	DWORD done = 0;
	while (done < count) {
		if (cur->outpos < cur->outlen) {
			DWORD avail = cur->outlen - cur->outpos;
			DWORD copy = (count - done < avail) ? count - done : avail;
			memcpy(buffer + done, cur->outbuf + cur->outpos, copy * sizeof(float));
			cur->outpos += copy; done += copy; continue;
		}
		cur->outlen = 0; cur->outpos = 0;
		if (cur->eof) break;
		DecodeFrame(cur);
	}
	dbglog("FF_Process#%ld: LEAVE done=%lu", callid, done);
	InterlockedDecrement(&g_in_plugin);
	return done;
}

static double WINAPI FF_GetGranularity() { return 0.001; }

static double WINAPI FF_SetPosition(DWORD pos)
{
	InterlockedIncrement(&g_in_plugin);
	dbglog("FF_SetPosition: cur=%p pos=%lu", cur, pos);
	if (!cur) { InterlockedDecrement(&g_in_plugin); return 0; }
	double time = pos * FF_GetGranularity();
	int64_t ts = (int64_t)(time * AV_TIME_BASE);
	if (avformat_seek_file(cur->fmtctx, cur->audiostream, INT64_MIN, ts, INT64_MAX, 0) < 0)
		av_seek_frame(cur->fmtctx, cur->audiostream, ts, AVSEEK_FLAG_BACKWARD);
	avcodec_flush_buffers(cur->decctx);
	cur->outlen = 0; cur->outpos = 0; cur->eof = FALSE;
	InterlockedDecrement(&g_in_plugin);
	return time;
}

static XMPIN xmpin = {
	XMPIN_FLAG_CANSTREAM,
	"FFmpeg decoder",
	"Audio\0mp3/aac/m4a/m4b/ogg/opus/flac/wav/aiff/ape/wma/ac3/dts/mpc/wv/tta/tak/alac/amr"
	"/mp2/mpa/mpga/ra/spx/snd/au/raw/pcm/thd/mlp/eac3/dtsma/dtshd/lpcm"
	"/webm/mka/oga/3gp/mp4/mov/asf/wmv/avi/mkv",
	NULL, NULL,
	FF_CheckFile, FF_GetFileInfo, FF_Open, FF_Close, NULL, FF_SetFormat,
	FF_GetTags, FF_GetInfoText, FF_GetGeneralInfo, FF_GetMessage, FF_SetPosition,
	FF_GetGranularity, NULL, FF_Process, NULL, NULL, NULL, NULL, NULL,
};

XMPIN *WINAPI XMPIN_GetInterface(DWORD face, InterfaceProc faceproc)
{
	if (face != XMPIN_FACE) {
		static int shown = 0;
		if (face < XMPIN_FACE && !shown) {
			MessageBoxA(0, "xmp-ffmpeg requires XMPlay 3.8+", 0, MB_ICONEXCLAMATION);
			shown = 1;
		}
		return NULL;
	}

	dbglog_init();
	dbglog("XMPIN_GetInterface: face=%lu", face);

	xmpfin  = (XMPFUNC_IN*)faceproc(XMPFUNC_IN_FACE);
	xmpfmisc = (XMPFUNC_MISC*)faceproc(XMPFUNC_MISC_FACE);
	xmpffile = (XMPFUNC_FILE*)faceproc(XMPFUNC_FILE_FACE);
	xmpftext = (XMPFUNC_TEXT*)faceproc(XMPFUNC_TEXT_FACE);
	xmpver   = xmpfmisc->GetVersion();

	dbglog("XMPIN_GetInterface: xmpfin=%p xmpfmisc=%p xmpffile=%p ver=0x%08X",
		xmpfin, xmpfmisc, xmpffile, xmpver);

	av_register_all();
	avformat_network_init();

	return &xmpin;
}

BOOL WINAPI DllMain(HINSTANCE hDLL, DWORD reason, LPVOID reserved)
{
	if (reason == DLL_PROCESS_ATTACH) {
		DisableThreadLibraryCalls(hDLL);
		dbglog_init();
		dbglog("DllMain: DLL_PROCESS_ATTACH hDLL=%p", hDLL);
		InstallCrashHandler();
	}
	if (reason == DLL_PROCESS_DETACH) {
		dbglog("DllMain: DLL_PROCESS_DETACH");
		dbglog_close();
	}
	return TRUE;
}
