package com.m5dev.mgbamobile

import android.app.Activity
import android.app.AlertDialog
import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.provider.OpenableColumns
import org.libsdl.app.SDLActivity
import java.io.File
import java.io.FileOutputStream

class MainActivity : SDLActivity() {

    private val REQUEST_PICK_ROM = 1001

    external fun loadROM(path: String)

    override fun getMainFunction(): String {
        return "SDL_main"
    }

    override fun getMainSharedObject(): String {
        return "libmgba-mobile.so"
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        openFilePicker()
    }

    private fun openFilePicker() {
        val intent = Intent(Intent.ACTION_OPEN_DOCUMENT).apply {
            addCategory(Intent.CATEGORY_OPENABLE)
            type = "*/*"
        }
        startActivityForResult(intent, REQUEST_PICK_ROM)
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        if (requestCode == REQUEST_PICK_ROM) {
            if (resultCode == Activity.RESULT_OK && data?.data != null) {
                val uri = data.data!!
                val fileName = getFileName(uri)
                if (isValidExtension(fileName)) {
                    copyUriToCacheAndLoad(uri)
                } else {
                    showNoFileSelectedDialog()
                }
            } else {
                showNoFileSelectedDialog()
            }
        }
    }

    private fun getFileName(uri: Uri): String {
        var name = ""
        try {
            contentResolver.query(uri, null, null, null, null)?.use { cursor ->
                if (cursor.moveToFirst()) {
                    val nameIndex = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME)
                    if (nameIndex != -1) {
                        name = cursor.getString(nameIndex) ?: ""
                    }
                }
            }
        } catch (e: Exception) {
            // Ignore query failures
        }
        if (name.isEmpty()) {
            name = uri.path ?: ""
        }
        return name
    }

    private fun isValidExtension(fileName: String): Boolean {
        val lower = fileName.lowercase()
        return lower.endsWith(".gba") || lower.endsWith(".gb") || lower.endsWith(".gbc")
    }

    private fun copyUriToCacheAndLoad(uri: Uri) {
        try {
            val cacheFile = File(cacheDir, "current.rom")
            contentResolver.openInputStream(uri)?.use { inputStream ->
                FileOutputStream(cacheFile).use { outputStream ->
                    inputStream.copyTo(outputStream)
                }
            }
            loadROM(cacheFile.absolutePath)
        } catch (e: Exception) {
            showNoFileSelectedDialog()
        }
    }

    private fun showNoFileSelectedDialog() {
        AlertDialog.Builder(this)
            .setMessage("Please select a ROM file to continue")
            .setPositiveButton("OK") { dialog, _ ->
                dialog.dismiss()
                openFilePicker()
            }
            .setCancelable(false)
            .show()
    }
}
