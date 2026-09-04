#include <android/log.h>
#include <android/native_window.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <mgba/core/core.h>
#include <mgba/core/thread.h>
#include <mgba/core/serialize.h>
#include <mgba-util/audio-buffer.h>
#include <mgba/internal/gba/input.h>
#include <aaudio/AAudio.h>

#include "overlay/virtual_buttons.h"

#define LOG_TAG  "mGBAMobileMain"
#define GBA_W    240
#define GBA_H    160

// Defined in android-jni.c
extern char            romPath[512];
extern ANativeWindow*  nativeWindow;
extern pthread_mutex_t stateMutex;
extern pthread_cond_t  stateCond;

extern uint32_t inputKeys;   // written by JNI touch handler, read here each frame

// ── Audio ─────────────────────────────────────────────────────────────────────
static AAudioStream* audioStream = NULL;

static aaudio_data_callback_result_t audioCallback(
        AAudioStream* stream, void* userData,
        void* audioData, int32_t numFrames) {
    struct mCore* core = (struct mCore*)userData;
    if (!core) {
        memset(audioData, 0, numFrames * 2 * sizeof(int16_t));
        return AAUDIO_CALLBACK_RESULT_CONTINUE;
    }
    struct mAudioBuffer* audioBuf = core->getAudioBuffer(core);
    if (audioBuf) {
        size_t available = mAudioBufferAvailable(audioBuf);
        size_t toRead = available < (size_t)numFrames ? available : (size_t)numFrames;
        mAudioBufferRead(audioBuf, (int16_t*)audioData, toRead);
        if (toRead < (size_t)numFrames) {
            memset((int16_t*)audioData + toRead * 2, 0,
                   (numFrames - toRead) * 2 * sizeof(int16_t));
        }
    } else {
        memset(audioData, 0, numFrames * 2 * sizeof(int16_t));
    }
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

static void initAudio(struct mCore* core) {
    AAudioStreamBuilder* builder;
    AAudio_createStreamBuilder(&builder);
    AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_I16);
    AAudioStreamBuilder_setChannelCount(builder, 2);
    AAudioStreamBuilder_setSampleRate(builder, 44100);
    AAudioStreamBuilder_setDataCallback(builder, audioCallback, core);
    AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
    AAudioStreamBuilder_openStream(builder, &audioStream);
    AAudioStreamBuilder_delete(builder);
    AAudioStream_requestStart(audioStream);
}

// ── Scale & draw one frame ────────────────────────────────────────────────────
static void blitFrame(ANativeWindow* win, uint32_t* src, int winW, int winH) {
    ANativeWindow_Buffer buf;
    if (ANativeWindow_lock(win, &buf, NULL) != 0) return;

    uint32_t* dst = (uint32_t*)buf.bits;

    // Integer scale: largest multiple of GBA_W/GBA_H that fits
    int scaleX = winW / GBA_W;
    int scaleY = winH / GBA_H;
    int scale  = scaleX < scaleY ? scaleX : scaleY;
    if (scale < 1) scale = 1;

    int drawW  = GBA_W * scale;
    int drawH  = GBA_H * scale;
    int offX   = (winW - drawW) / 2;
    int offY   = (winH - drawH) / 2;

    // Clear to black
    memset(dst, 0, buf.stride * winH * 4);

    // Nearest-neighbor upscale
    for (int y = 0; y < drawH; y++) {
        for (int x = 0; x < drawW; x++) {
            int srcPx = (y / scale) * GBA_W + (x / scale);
            dst[(y + offY) * buf.stride + (x + offX)] = src[srcPx];
        }
    }

    // Draw virtual button outlines (semi-transparent grey overlay)
    size_t nb = sizeof(gbaButtons) / sizeof(gbaButtons[0]);
    for (size_t i = 0; i < nb; i++) {
        // Scale button rects from the 480x540 virtual space to actual window
        int bx = (int)((float)gbaButtons[i].rect.x / 480 * winW);
        int by = (int)((float)gbaButtons[i].rect.y / 540 * winH);
        int bw = (int)((float)gbaButtons[i].rect.w / 480 * winW);
        int bh = (int)((float)gbaButtons[i].rect.h / 540 * winH);
        for (int ry = by; ry < by + bh && ry < winH; ry++) {
            for (int rx = bx; rx < bx + bw && rx < winW; rx++) {
                // Simple outline only (border pixels)
                if (rx == bx || rx == bx+bw-1 || ry == by || ry == by+bh-1) {
                    dst[ry * buf.stride + rx] = 0xAAFFFFFF; // white outline
                }
            }
        }
    }

    ANativeWindow_unlockAndPost(win);
}

// ── Main entry ────────────────────────────────────────────────────────────────
int emulatorMain(void) {
    // Wait until both romPath and nativeWindow are ready
    pthread_mutex_lock(&stateMutex);
    while (romPath[0] == '\0' || nativeWindow == NULL) {
        pthread_cond_wait(&stateCond, &stateMutex);
    }
    pthread_mutex_unlock(&stateMutex);

    // Find and init the core (handles .gba / .gb / .gbc / .zip / .7z)
    struct mCore* core = mCoreFind(romPath);
    if (!core) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "No core found for: %s", romPath);
        return 1;
    }
    core->init(core);

    // Software pixel buffer — 240×160 ARGB8888
    uint32_t* pixelBuf = (uint32_t*)malloc(GBA_W * GBA_H * sizeof(uint32_t));
    core->setVideoBuffer(core, (mColor*)pixelBuf, GBA_W);

    // Configure ANativeWindow to ARGB8888 32-bit
    int winW = ANativeWindow_getWidth(nativeWindow);
    int winH = ANativeWindow_getHeight(nativeWindow);
    ANativeWindow_setBuffersGeometry(nativeWindow, winW, winH, WINDOW_FORMAT_RGBX_8888);

    if (!mCoreLoadFile(core, romPath)) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "Failed to load ROM: %s", romPath);
        core->deinit(core);
        free(pixelBuf);
        return 1;
    }
    mCoreAutoloadSave(core);

    initAudio(core);

    // Start emulator thread
    struct mCoreThread thread = { .core = core };
    mCoreThreadStart(&thread);

    // Render loop — runs on this thread
    while (mCoreThreadIsActive(&thread)) {
        // Feed input each frame
        core->setKeys(core, inputKeys);

        // blit the current frame to screen
        blitFrame(nativeWindow, pixelBuf, winW, winH);

        // ~60fps cap
        struct timespec ts = { 0, 16666667L };
        nanosleep(&ts, NULL);
    }

    // Cleanup
    AAudioStream_requestStop(audioStream);
    AAudioStream_close(audioStream);
    mCoreThreadJoin(&thread);
    core->unloadROM(core);
    core->deinit(core);
    free(pixelBuf);
    ANativeWindow_release(nativeWindow);

    return 0;
}
