package com.m5dev.mgbamobile

import android.app.Activity
import android.os.Bundle

class MainActivity : Activity() {

    companion object {
        init {
            System.loadLibrary("mgba-mobile")
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
    }

    external fun loadROM(path: String)
}
