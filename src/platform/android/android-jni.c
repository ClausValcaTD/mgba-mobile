#include <jni.h>
#include <string.h>
#include <pthread.h>
#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>

#include <mgba/internal/gba/input.h>
#include "overlay/virtual_buttons.h"

#define LOG_TAG "mGBAMobileJNI"

// Shared state — read by android-main.c
char romPath[512] = {0};
ANativeWindow* nativeWindow = NULL;

pthread_mutex_t stateMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  stateCond  = PTHREAD_COND_INITIALIZER;

uint32_t inputKeys = 0;

// Called from MainActivity after copying ROM to cache
JNIEXPORT void JNICALL
Java_com_m5dev_mgbamobile_MainActivity_loadROM(JNIEnv* env, jobject obj, jstring path) {
    const char* str = (*env)->GetStringUTFChars(env, path, NULL);
    pthread_mutex_lock(&stateMutex);
    strncpy(romPath, str, sizeof(romPath) - 1);
    romPath[sizeof(romPath) - 1] = '\0';
    pthread_cond_signal(&stateCond);
    pthread_mutex_unlock(&stateMutex);
    (*env)->ReleaseStringUTFChars(env, path, str);
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "ROM path: %s", romPath);
}

// Called from MainActivity.surfaceCreated()
JNIEXPORT void JNICALL
Java_com_m5dev_mgbamobile_MainActivity_setSurface(JNIEnv* env, jobject obj, jobject surface) {
    pthread_mutex_lock(&stateMutex);
    if (nativeWindow) ANativeWindow_release(nativeWindow);
    nativeWindow = surface ? ANativeWindow_fromSurface(env, surface) : NULL;
    pthread_cond_signal(&stateCond);
    pthread_mutex_unlock(&stateMutex);
}

// Called from MainActivity on touch events
JNIEXPORT void JNICALL
Java_com_m5dev_mgbamobile_MainActivity_onTouch(JNIEnv* env, jobject obj,
                                                jint x, jint y, jboolean down,
                                                jint surfaceW, jint surfaceH) {
    if (surfaceW <= 0 || surfaceH <= 0) return;

    // Scale touch coords from surface size to the virtual button coordinate space (480x540)
    // virtual_buttons.h uses fixed pixel coords designed for ~480px wide layout
    int vx = (int)((float)x / surfaceW * 480);
    int vy = (int)((float)y / surfaceH * 540);

    size_t numButtons = sizeof(gbaButtons) / sizeof(gbaButtons[0]);
    for (size_t i = 0; i < numButtons; i++) {
        // SDL_Rect has x,y,w,h — reuse the struct directly, no SDL needed at runtime
        int rx = gbaButtons[i].rect.x, ry = gbaButtons[i].rect.y;
        int rw = gbaButtons[i].rect.w, rh = gbaButtons[i].rect.h;
        if (vx >= rx && vx <= rx + rw && vy >= ry && vy <= ry + rh) {
            // Store key state — android-main.c reads this each frame
            // Use a simple bitmask array indexed by gbaKey
            if (down) inputKeys |=  (1u << gbaButtons[i].gbaKey);
            else       inputKeys &= ~(1u << gbaButtons[i].gbaKey);
        }
    }
}

static void* emulationThread(void* arg) {
    extern int emulatorMain(void); // defined in android-main.c
    emulatorMain();
    return NULL;
}

jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    pthread_t thread;
    pthread_create(&thread, NULL, emulationThread, NULL);
    pthread_detach(thread);
    return JNI_VERSION_1_6;
}
