#include <jni.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdarg.h>
#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>

#include <mgba/internal/gba/input.h>
#include "overlay/virtual_buttons.h"

#define LOG_TAG "mGBAMobileJNI"
#define mLog android_mLog

/* ── File logger ─────────────────────────────────────────────────────────── */
static FILE* logFile = NULL;

void mLog(const char* level, const char* tag, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    __android_log_vprint(
        strcmp(level, "E") == 0 ? ANDROID_LOG_ERROR :
        strcmp(level, "W") == 0 ? ANDROID_LOG_WARN  : ANDROID_LOG_INFO,
        tag, fmt, args);
    va_end(args);

    if (logFile) {
        va_start(args, fmt);
        fprintf(logFile, "[%s/%s] ", level, tag);
        vfprintf(logFile, fmt, args);
        fprintf(logFile, "\n");
        fflush(logFile);
        va_end(args);
    }
}

JNIEXPORT void JNICALL
Java_com_m5dev_mgbamobile_MainActivity_setLogFile(JNIEnv* env, jobject obj, jstring path) {
    const char* str = (*env)->GetStringUTFChars(env, path, NULL);
    if (logFile) fclose(logFile);
    logFile = fopen(str, "w");
    if (logFile) {
        fprintf(logFile, "=== mGBA Mobile Log ===\n");
        fflush(logFile);
    }
    (*env)->ReleaseStringUTFChars(env, path, str);
}

/* ── Shared state ─────────────────────────────────────────────────────────── */
char            romPath[512]  = {0};
ANativeWindow*  nativeWindow  = NULL;
int             romReady      = 0;  /* 1 = file copy complete, safe to open */

pthread_mutex_t stateMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  stateCond  = PTHREAD_COND_INITIALIZER;

uint32_t inputKeys = 0;

/* ── JNI handlers ────────────────────────────────────────────────────────── */

JNIEXPORT void JNICALL
Java_com_m5dev_mgbamobile_MainActivity_loadROM(JNIEnv* env, jobject obj, jstring path) {
    const char* str = (*env)->GetStringUTFChars(env, path, NULL);
    pthread_mutex_lock(&stateMutex);
    strncpy(romPath, str, sizeof(romPath) - 1);
    romPath[sizeof(romPath) - 1] = '\0';
    romReady = 1;  /* file copy is done, safe to open now */
    pthread_cond_signal(&stateCond);
    pthread_mutex_unlock(&stateMutex);
    (*env)->ReleaseStringUTFChars(env, path, str);
    mLog("I", LOG_TAG, "ROM ready: %s", romPath);
}

JNIEXPORT void JNICALL
Java_com_m5dev_mgbamobile_MainActivity_setSurface(JNIEnv* env, jobject obj, jobject surface) {
    pthread_mutex_lock(&stateMutex);
    if (nativeWindow) ANativeWindow_release(nativeWindow);
    nativeWindow = surface ? ANativeWindow_fromSurface(env, surface) : NULL;
    pthread_cond_signal(&stateCond);
    pthread_mutex_unlock(&stateMutex);
    mLog("I", LOG_TAG, "Surface %s", nativeWindow ? "set" : "cleared");
}

JNIEXPORT void JNICALL
Java_com_m5dev_mgbamobile_MainActivity_onTouch(JNIEnv* env, jobject obj,
                                                jint x, jint y, jboolean down,
                                                jint surfaceW, jint surfaceH) {
    if (surfaceW <= 0 || surfaceH <= 0) return;

    int vx = (int)((float)x / surfaceW * 480);
    int vy = (int)((float)y / surfaceH * 540);

    size_t numButtons = sizeof(gbaButtons) / sizeof(gbaButtons[0]);
    for (size_t i = 0; i < numButtons; i++) {
        int rx = gbaButtons[i].rect.x, ry = gbaButtons[i].rect.y;
        int rw = gbaButtons[i].rect.w, rh = gbaButtons[i].rect.h;
        if (vx >= rx && vx <= rx + rw && vy >= ry && vy <= ry + rh) {
            if (down) inputKeys |=  (1u << gbaButtons[i].gbaKey);
            else       inputKeys &= ~(1u << gbaButtons[i].gbaKey);
        }
    }
}

/* ── Emulation thread ────────────────────────────────────────────────────── */
static void* emulationThread(void* arg) {
    extern int emulatorMain(void);
    emulatorMain();
    return NULL;
}

jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    pthread_t thread;
    pthread_create(&thread, NULL, emulationThread, NULL);
    pthread_detach(thread);
    return JNI_VERSION_1_6;
}
