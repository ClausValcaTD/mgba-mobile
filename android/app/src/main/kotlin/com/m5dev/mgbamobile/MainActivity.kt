package com.m5dev.mgbamobile

import org.libsdl.app.SDLActivity

class MainActivity : SDLActivity() {
    override fun getMainFunction(): String {
        return "SDL_main"
    }

    override fun getMainSharedObject(): String {
        return "libmgba-mobile.so"
    }
}
