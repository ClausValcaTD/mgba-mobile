package com.m5dev.mgbamobile

import android.net.Uri
import android.os.Bundle
import android.view.MotionEvent
import android.view.Surface
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.WindowManager
import androidx.activity.ComponentActivity
import androidx.activity.result.contract.ActivityResultContracts
import java.io.File

class MainActivity : ComponentActivity(), SurfaceHolder.Callback {

    companion object {
        init { System.loadLibrary("mgba-mobile") }
    }

    private lateinit var surfaceView: SurfaceView
    private var cachedRom: File? = null
    private var statusText: android.widget.TextView? = null

    private val pickRom = registerForActivityResult(
        ActivityResultContracts.OpenDocument()
    ) { uri: Uri? ->
        uri?.let { copyAndLoad(it) }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        initLogger()
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)

        val root = android.widget.FrameLayout(this)

        surfaceView = SurfaceView(this)
        surfaceView.holder.addCallback(this)
        root.addView(surfaceView, android.widget.FrameLayout.LayoutParams(
            android.widget.FrameLayout.LayoutParams.MATCH_PARENT,
            android.widget.FrameLayout.LayoutParams.MATCH_PARENT
        ))

        val tv = android.widget.TextView(this).apply {
            text = "مرحباً بك في mGBA Mobile\nاضغط هنا لاختيار ROM"
            textSize = 20f
            setTextColor(android.graphics.Color.WHITE)
            gravity = android.view.Gravity.CENTER
            setOnClickListener { pickRom.launch(arrayOf("*/*")) }
        }
        statusText = tv
        root.addView(tv, android.widget.FrameLayout.LayoutParams(
            android.widget.FrameLayout.LayoutParams.MATCH_PARENT,
            android.widget.FrameLayout.LayoutParams.MATCH_PARENT
        ))

        setContentView(root)
        pickRom.launch(arrayOf("*/*"))
    }

    private fun initLogger() {
        if (android.os.Build.VERSION.SDK_INT <= android.os.Build.VERSION_CODES.P) {
            if (checkSelfPermission(android.Manifest.permission.WRITE_EXTERNAL_STORAGE)
                != android.content.pm.PackageManager.PERMISSION_GRANTED) {
                requestPermissions(
                    arrayOf(android.Manifest.permission.WRITE_EXTERNAL_STORAGE), 100
                )
            }
        }
        val logFile = getObbDir().absolutePath + "/mgba.log"
        setLogFile(logFile)
    }

    private fun copyAndLoad(uri: Uri) {
        Thread {
            // احتفظ بالـ extension الأصلي عشان mCoreFind يعرف الـ core
            val originalName = uri.lastPathSegment
                ?.substringAfterLast('/')
                ?: "rom.gba"
            val ext = originalName
                .substringAfterLast('.', "gba")
                .lowercase()

            val dest = File(cacheDir, "current_rom.$ext")
            contentResolver.openInputStream(uri)?.use { input ->
                dest.outputStream().use { output -> input.copyTo(output) }
            }
            cachedRom = dest
            runOnUiThread { statusText?.visibility = android.view.View.GONE }
            loadROM(dest.absolutePath)
        }.start()
    }

    override fun surfaceCreated(holder: SurfaceHolder) = setSurface(holder.surface)
    override fun surfaceDestroyed(holder: SurfaceHolder) = setSurface(null)
    override fun surfaceChanged(holder: SurfaceHolder, fmt: Int, w: Int, h: Int) {}

    override fun onTouchEvent(event: MotionEvent): Boolean {
        val w = surfaceView.width
        val h = surfaceView.height
        val down = event.action == MotionEvent.ACTION_DOWN ||
                   event.action == MotionEvent.ACTION_MOVE
        onTouch(event.x.toInt(), event.y.toInt(), down, w, h)
        return true
    }

    override fun onDestroy() {
        super.onDestroy()
        cachedRom?.delete()
    }

    // JNI declarations
    external fun setLogFile(path: String)
    external fun loadROM(path: String)
    external fun setSurface(surface: Surface?)
    external fun onTouch(x: Int, y: Int, down: Boolean, surfaceW: Int, surfaceH: Int)
}
