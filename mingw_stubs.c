// Stub implementations for MinGW runtime functions when linking with MSVC
// These are needed when FFmpeg is built with MinGW but linked with MSVC

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <time.h>

// clock_gettime stub for MinGW compatibility
int clock_gettime(int clk_id, struct timespec *tp) {
    static int initialized = 0;
    static LARGE_INTEGER frequency;
    
    if (!initialized) {
        QueryPerformanceFrequency(&frequency);
        initialized = 1;
    }
    
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    
    tp->tv_sec = counter.QuadPart / frequency.QuadPart;
    tp->tv_nsec = (long)((counter.QuadPart % frequency.QuadPart) * 1000000000 / frequency.QuadPart);
    
    return 0;
}

// nanosleep stub for MinGW compatibility
int nanosleep(const struct timespec *req, struct timespec *rem) {
    DWORD ms = (DWORD)(req->tv_sec * 1000 + req->tv_nsec / 1000000);
    if (ms == 0) ms = 1;
    Sleep(ms);
    if (rem) {
        rem->tv_sec = 0;
        rem->tv_nsec = 0;
    }
    return 0;
}

// MinGW math error handler stub
void __mingw_raise_matherr(int type, const char *name, double a1, double a2, double r) {
    // Do nothing - just a stub
}
