/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <android/log.h>

#include "../sdl/main.h"
#include "overlay/virtual_buttons.h"

#include <mgba/core/core.h>
#include <mgba/core/config.h>
#include <mgba/core/input.h>
#include <mgba/core/serialize.h>
#include <mgba/core/thread.h>
#include <mgba/internal/gba/input.h>

#include <mgba-util/vfs.h>

#include <SDL.h>

#include <errno.h>
#include <signal.h>

#define PORT "sdl"
#define LOG_TAG "mGBAMobile"

extern char romPath[512];

static void mSDLDeinit(struct mSDLRenderer* renderer);
static int mSDLRun(struct mSDLRenderer* renderer, const char* romPath);
static void mAndroidRunloop(struct mSDLRenderer* renderer, void* user);

static struct mStandardLogger _logger;
ATTRIBUTE_UNUSED static struct VFile* _state = NULL;

ATTRIBUTE_UNUSED static void _loadState(struct mCoreThread* thread) {
	mCoreLoadStateNamed(thread->core, _state, SAVESTATE_RTC);
}

int main(int argc, char** argv) {
	UNUSED(argc);
	UNUSED(argv);

	struct mSDLRenderer renderer = {0};

	struct mCoreOptions opts = {
		.useBios = true,
		.rewindEnable = true,
		.rewindBufferCapacity = 600,
		.rewindBufferInterval = 1,
		.audioBuffers = 1024,
		.videoSync = false,
		.audioSync = true,
		.volume = 0x100,
		.logLevel = mLOG_WARN | mLOG_ERROR | mLOG_FATAL,
	};

	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		__android_log_print(ANDROID_LOG_INFO, LOG_TAG, "Could not initialize video: %s", SDL_GetError());
		return 1;
	}

	// Wait for romPath to be set by JNI before proceeding
	while (romPath[0] == '\0') {
		SDL_Delay(100);
	}

	renderer.core = mCoreFind(romPath);
	if (!renderer.core) {
		__android_log_print(ANDROID_LOG_INFO, LOG_TAG, "Could not run game. Are you sure the file exists and is a compatible game?");
		return 1;
	}

	if (!renderer.core->init(renderer.core)) {
		return 1;
	}

	renderer.core->baseVideoSize(renderer.core, &renderer.width, &renderer.height);
	renderer.ratio = 1;
	opts.width = renderer.width * renderer.ratio;
	opts.height = renderer.height * renderer.ratio;

	mInputMapInit(&renderer.core->inputMap, &GBAInputInfo);
	mCoreInitConfig(renderer.core, PORT);

	mCoreConfigSetDefaultIntValue(&renderer.core->config, "logToStdout", true);
	mCoreConfigLoadDefaults(&renderer.core->config, &opts);
	mCoreLoadConfig(renderer.core);
	mStandardLoggerInit(&_logger);
	mStandardLoggerConfig(&_logger, &renderer.core->config);
	mLogSetDefaultLogger(&_logger.d);

	renderer.viewportWidth = renderer.core->opts.width;
	renderer.viewportHeight = renderer.core->opts.height;
	renderer.player.fullscreen = renderer.core->opts.fullscreen;
	renderer.player.windowUpdated = 0;

	renderer.lockAspectRatio = renderer.core->opts.lockAspectRatio;
	renderer.lockIntegerScaling = renderer.core->opts.lockIntegerScaling;
	renderer.interframeBlending = renderer.core->opts.interframeBlending;
	renderer.filter = renderer.core->opts.resampleVideo;

	mSDLSWCreate(&renderer);
	renderer.runloop = mAndroidRunloop;

	if (!renderer.init(&renderer)) {
		mCoreConfigDeinit(&renderer.core->config);
		renderer.core->deinit(renderer.core);
		return 1;
	}

	renderer.player.bindings = &renderer.core->inputMap;
	mSDLInitBindingsGBA(&renderer.core->inputMap);
	mSDLInitEvents(&renderer.events);
	mSDLEventsLoadConfig(&renderer.events, mCoreConfigGetInput(&renderer.core->config));
	mSDLAttachPlayer(&renderer.events, &renderer.player, -1);
	mSDLPlayerLoadConfig(&renderer.player, mCoreConfigGetInput(&renderer.core->config));

#if SDL_VERSION_ATLEAST(2, 0, 0)
	renderer.core->setPeripheral(renderer.core, mPERIPH_RUMBLE, &renderer.player.rumble.d.d);
#endif

	int ret;

	ret = mSDLRun(&renderer, romPath);
	mSDLDetachPlayer(&renderer.events, &renderer.player);
	mInputMapDeinit(&renderer.core->inputMap);

	mSDLDeinit(&renderer);
	mStandardLoggerDeinit(&_logger);

	mCoreConfigFreeOpts(&opts);
	mCoreConfigDeinit(&renderer.core->config);
	renderer.core->deinit(renderer.core);

	return ret;
}

static void mAndroidRunloop(struct mSDLRenderer* renderer, void* user) {
	struct mCoreThread* context = user;
	SDL_Event event;

	size_t numButtons = sizeof(gbaButtons) / sizeof(gbaButtons[0]);

	while (mCoreThreadIsActive(context)) {
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_FINGERDOWN || event.type == SDL_FINGERUP) {
				int winW = renderer->viewportWidth;
				int winH = renderer->viewportHeight;
				if (renderer->window) {
					SDL_GetWindowSize(renderer->window, &winW, &winH);
				}
				int touchX = (int)(event.tfinger.x * (event.tfinger.x <= 1.0f ? winW : 1));
				int touchY = (int)(event.tfinger.y * (event.tfinger.y <= 1.0f ? winH : 1));

				for (size_t i = 0; i < numButtons; ++i) {
					SDL_Rect r = gbaButtons[i].rect;
					if (touchX >= r.x && touchX <= r.x + r.w && touchY >= r.y && touchY <= r.y + r.h) {
						if (event.type == SDL_FINGERDOWN) {
							context->core->addKeys(context->core, 1 << gbaButtons[i].gbaKey);
						} else {
							context->core->clearKeys(context->core, 1 << gbaButtons[i].gbaKey);
						}
					}
				}
			}
			mSDLHandleEvent(context, &renderer->player, &event);
		}

		if (mCoreSyncWaitFrameStart(&context->impl->sync)) {
			SDL_UnlockTexture(renderer->sdlTex);
			SDL_RenderCopy(renderer->sdlRenderer, renderer->sdlTex, 0, 0);

			// Draw virtual buttons overlay with 120 alpha
			SDL_SetRenderDrawBlendMode(renderer->sdlRenderer, SDL_BLENDMODE_BLEND);
			for (size_t i = 0; i < numButtons; ++i) {
				SDL_SetRenderDrawColor(renderer->sdlRenderer, 200, 200, 200, 120);
				SDL_RenderFillRect(renderer->sdlRenderer, &gbaButtons[i].rect);
			}

			SDL_RenderPresent(renderer->sdlRenderer);
			int stride;
			SDL_LockTexture(renderer->sdlTex, 0, (void**) &renderer->outputBuffer, &stride);
			renderer->core->setVideoBuffer(renderer->core, renderer->outputBuffer, stride / BYTES_PER_PIXEL);
		}
		mCoreSyncWaitFrameEnd(&context->impl->sync);
	}
}

int mSDLRun(struct mSDLRenderer* renderer, const char* romPath) {
	struct mCoreThread thread = {
		.core = renderer->core
	};
	if (!mCoreLoadFile(renderer->core, romPath)) {
		return 1;
	}
	mCoreAutoloadSave(renderer->core);

	renderer->audio.samples = renderer->core->opts.audioBuffers;
	renderer->audio.sampleRate = 44100;
	thread.logger.logger = &_logger.d;

	bool didFail = !mCoreThreadStart(&thread);

	if (!didFail) {
#if SDL_VERSION_ATLEAST(2, 0, 0)
		renderer->core->currentVideoSize(renderer->core, &renderer->width, &renderer->height);
		unsigned width = renderer->width * renderer->ratio;
		unsigned height = renderer->height * renderer->ratio;
		if (width != (unsigned) renderer->viewportWidth && height != (unsigned) renderer->viewportHeight) {
			SDL_SetWindowSize(renderer->window, width, height);
			renderer->player.windowUpdated = 1;
		}
		mSDLSetScreensaverSuspendable(&renderer->events, renderer->core->opts.suspendScreensaver);
		mSDLSuspendScreensaver(&renderer->events);
#endif
		if (mSDLInitAudio(&renderer->audio, &thread)) {
			renderer->runloop(renderer, &thread);
			mSDLPauseAudio(&renderer->audio);
			if (mCoreThreadHasCrashed(&thread)) {
				didFail = true;
				__android_log_print(ANDROID_LOG_INFO, LOG_TAG, "The game crashed!");
				mCoreThreadEnd(&thread);
			}
		} else {
			didFail = true;
			__android_log_print(ANDROID_LOG_INFO, LOG_TAG, "Could not initialize audio.");
		}
#if SDL_VERSION_ATLEAST(2, 0, 0)
		mSDLResumeScreensaver(&renderer->events);
		mSDLSetScreensaverSuspendable(&renderer->events, false);
#endif

		mCoreThreadJoin(&thread);
	} else {
		__android_log_print(ANDROID_LOG_INFO, LOG_TAG, "Could not run game. Are you sure the file exists and is a compatible game?");
	}
	renderer->core->unloadROM(renderer->core);

	return didFail;
}

static void mSDLDeinit(struct mSDLRenderer* renderer) {
	mSDLDeinitEvents(&renderer->events);
	mSDLDeinitAudio(&renderer->audio);
#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_DestroyWindow(renderer->window);
#endif

	renderer->deinit(renderer);

	SDL_Quit();
}
