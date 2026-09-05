/* mGBA Mobile — Android native main loop
 * Single-threaded: runFrame() + ANativeWindow blit + AAudio
 * No mCoreThread, no SDL, no data races.
 */

#include <android/log.h>
#include <android/native_window.h>
#include <aaudio/AAudio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <mgba/core/core.h>
#include <mgba/core/serialize.h>
#include <mgba-util/audio-buffer.h>
#include <mgba/internal/gba/input.h>

#include "overlay/virtual_buttons.h"

#define LOG_TAG "mGBAMobileMain"
#define GBA_W   240
#define GBA_H   160

/* ── Shared state (written by android-jni.c) ─────────────────────────────── */
extern char             romPath[512];
extern ANativeWindow*   nativeWindow;
extern pthread_mutex_t  stateMutex;
extern pthread_cond_t   stateCond;
extern uint32_t         inputKeys;

/* ── Audio ───────────────────────────────────────────────────────────────── */
static AAudioStream* audioStream = NULL;

static aaudio_data_callback_result_t audioCallback(
        AAudioStream* stream, void* userData,
        void* audioData, int32_t numFrames) {

    struct mCore* core = (struct mCore*)userData;
    int16_t* out = (int16_t*)audioData;
    size_t   total = (size_t)numFrames * 2; /* stereo */

    if (!core) {
        memset(out, 0, total * sizeof(int16_t));
        return AAUDIO_CALLBACK_RESULT_CONTINUE;
    }

    struct mAudioBuffer* buf = core->getAudioBuffer(core);
    if (buf) {
        size_t avail  = mAudioBufferAvailable(buf);
        size_t toRead = avail < (size_t)numFrames ? avail : (size_t)numFrames;
        size_t got    = mAudioBufferRead(buf, out, toRead);
        if (got < (size_t)numFrames) {
            memset(out + got * 2, 0, (numFrames - (int32_t)got) * 2 * sizeof(int16_t));
        }
    } else {
        memset(out, 0, total * sizeof(int16_t));
    }
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

static void initAudio(struct mCore* core) {
    AAudioStreamBuilder* builder = NULL;
    aaudio_result_t res = AAudio_createStreamBuilder(&builder);
    if (res != AAUDIO_OK || !builder) {
        __android_log_print(ANDROID_LOG_WARN, LOG_TAG, "AAudio builder failed: %d", res);
        return;
    }
    AAudioStreamBuilder_setFormat(builder,          AAUDIO_FORMAT_PCM_I16);
    AAudioStreamBuilder_setChannelCount(builder,    2);
    AAudioStreamBuilder_setSampleRate(builder,      32768); /* mGBA native rate */
    AAudioStreamBuilder_setDataCallback(builder,    audioCallback, core);
    AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);

    res = AAudioStreamBuilder_openStream(builder, &audioStream);
    AAudioStreamBuilder_delete(builder);

    if (res != AAUDIO_OK || !audioStream) {
        __android_log_print(ANDROID_LOG_WARN, LOG_TAG, "AAudio open failed: %d", res);
        audioStream = NULL;
        return;
    }
    AAudioStream_requestStart(audioStream);
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "AAudio started");
}

static void stopAudio(void) {
    if (audioStream) {
        AAudioStream_requestStop(audioStream);
        AAudioStream_close(audioStream);
        audioStream = NULL;
    }
}

/* ── Blit one frame to ANativeWindow ─────────────────────────────────────── */
static void blitFrame(ANativeWindow* win, const uint32_t* src, int winW, int winH) {
    ANativeWindow_Buffer buf;
    if (ANativeWindow_lock(win, &buf, NULL) != 0) return;

    uint32_t* dst = (uint32_t*)buf.bits;

    /* Largest integer scale that fits */
    int scaleX = winW / GBA_W;
    int scaleY = winH / GBA_H;
    int scale  = (scaleX < scaleY) ? scaleX : scaleY;
    if (scale < 1) scale = 1;

    int drawW = GBA_W * scale;
    int drawH = GBA_H * scale;
    int offX  = (winW - drawW) / 2;
    int offY  = (winH - drawH) / 2;

    /* Clear letterbox to black */
    memset(dst, 0, (size_t)buf.stride * (size_t)winH * 4);

    /* Nearest-neighbor upscale */
    for (int y = 0; y < drawH; y++) {
        const uint32_t* srcRow = src + (y / scale) * GBA_W;
        uint32_t*       dstRow = dst + (y + offY) * buf.stride + offX;
        for (int x = 0; x < drawW; x++) {
            dstRow[x] = srcRow[x / scale];
        }
    }

    /* Virtual button outlines */
    size_t nb = sizeof(gbaButtons) / sizeof(gbaButtons[0]);
    for (size_t i = 0; i < nb; i++) {
        int bx = (int)((float)gbaButtons[i].rect.x / 480.0f * (float)winW);
        int by = (int)((float)gbaButtons[i].rect.y / 540.0f * (float)winH);
        int bw = (int)((float)gbaButtons[i].rect.w / 480.0f * (float)winW);
        int bh = (int)((float)gbaButtons[i].rect.h / 540.0f * (float)winH);
        for (int ry = by; ry < by + bh && ry < winH; ry++) {
            for (int rx = bx; rx < bx + bw && rx < winW; rx++) {
                if (rx == bx || rx == bx+bw-1 || ry == by || ry == by+bh-1) {
                    dst[ry * buf.stride + rx] = 0xFFFFFFFF;
                }
            }
        }
    }

    ANativeWindow_unlockAndPost(win);
}

/* ── Main emulator entry (called from emulationThread in android-jni.c) ──── */
int emulatorMain(void) {

    /* 1. Wait until file copy is done AND surface is ready */
    extern int romReady;
    pthread_mutex_lock(&stateMutex);
    while (!romReady || nativeWindow == NULL) {
        pthread_cond_wait(&stateCond, &stateMutex);
    }
    /* Take local copies while holding the lock */
    char           localRom[512];
    ANativeWindow* win = nativeWindow;
    strncpy(localRom, romPath, sizeof(localRom) - 1);
    localRom[sizeof(localRom) - 1] = '\0';
    pthread_mutex_unlock(&stateMutex);

    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "Starting emulator: %s", localRom);

    /* 2. Find core */
    struct mCore* core = mCoreFind(localRom);
    if (!core) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "No core for: %s", localRom);
        return 1;
    }

    /* 3. Init core */
    if (!core->init(core)) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "core->init failed");
        return 1;
    }

    /* 4. Pixel buffer */
    uint32_t* pixelBuf = (uint32_t*)calloc(GBA_W * GBA_H, sizeof(uint32_t));
    if (!pixelBuf) {
        core->deinit(core);
        return 1;
    }
    core->setVideoBuffer(core, (mColor*)pixelBuf, GBA_W);

    /* 5. Configure surface */
    int winW = ANativeWindow_getWidth(win);
    int winH = ANativeWindow_getHeight(win);
    ANativeWindow_setBuffersGeometry(win, winW, winH, WINDOW_FORMAT_RGBX_8888);

    /* 6. Load ROM */
    if (!mCoreLoadFile(core, localRom)) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "mCoreLoadFile failed: %s", localRom);
        core->deinit(core);
        free(pixelBuf);
        return 1;
    }
    mCoreAutoloadSave(core);

    /* 7. Reset */
    core->reset(core);

    /* 8. Audio */
    initAudio(core);

    /* 9. Game loop — single threaded, no data race */
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "Entering game loop %dx%d", winW, winH);

    struct timespec ts = { 0, 16666667L }; /* ~60 fps */

    for (;;) {
        /* Check if surface is still alive */
        pthread_mutex_lock(&stateMutex);
        int alive = (nativeWindow != NULL);
        pthread_mutex_unlock(&stateMutex);
        if (!alive) break;

        /* Run one GBA frame */
        core->runFrame(core);

        /* Apply input */
        core->setKeys(core, inputKeys);

        /* Draw */
        blitFrame(win, pixelBuf, winW, winH);

        /* Cap to ~60fps */
        nanosleep(&ts, NULL);
    }

    /* 10. Cleanup */
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "Emulator stopping");
    stopAudio();
    core->unloadROM(core);
    core->deinit(core);
    free(pixelBuf);
    ANativeWindow_release(win);

    return 0;
}
