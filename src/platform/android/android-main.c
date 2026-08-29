/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <android/log.h>

#include "../sdl/main.h"

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

static void mSDLDeinit(struct mSDLRenderer* renderer);

static int mSDLRun(struct mSDLRenderer* renderer, const char* romPath);

static struct mStandardLogger _logger;

static struct VFile* _state = NULL;

UNUSED static void _loadState(struct mCoreThread* thread) {
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

	const char* romPath = "/sdcard/mgba/rom.gba";

	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		__android_log_print(ANDROID_LOG_INFO, LOG_TAG, "Could not initialize video: %s", SDL_GetError());
		return 1;
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
