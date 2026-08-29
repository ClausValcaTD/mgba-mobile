#include <jni.h>
#include <string.h>
#include <android/log.h>

#define LOG_TAG "mGBAMobileJNI"

char romPath[512] = {0};

JNIEXPORT void JNICALL
Java_com_m5dev_mgbamobile_MainActivity_loadROM(JNIEnv* env, jobject obj, jstring path) {
	if (!path) {
		return;
	}
	const char* nativeString = (*env)->GetStringUTFChars(env, path, NULL);
	if (nativeString) {
		strncpy(romPath, nativeString, sizeof(romPath) - 1);
		romPath[sizeof(romPath) - 1] = '\0';
		(*env)->ReleaseStringUTFChars(env, path, nativeString);
		__android_log_print(ANDROID_LOG_INFO, LOG_TAG, "ROM path set to: %s", romPath);
	}
}
