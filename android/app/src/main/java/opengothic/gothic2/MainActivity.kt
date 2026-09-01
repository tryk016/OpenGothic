package opengothic.gothic2

import android.app.Activity
import android.content.ActivityNotFoundException
import android.content.Intent
import android.graphics.Color
import android.net.Uri
import android.os.Bundle
import android.os.Environment
import android.provider.Settings
import android.view.Gravity
import android.view.ViewGroup
import android.widget.Button
import android.widget.LinearLayout
import android.widget.TextView

/**
 * Stable launcher and storage gate. Keeping this component name ensures that
 * task stacks retained across an APK update are routed through the gate before
 * the native GameActivity is created.
 */
class MainActivity : Activity() {
    private var permissionRequestStarted = false
    private var gameStarted = false

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        permissionRequestStarted = savedInstanceState?.getBoolean(REQUEST_STARTED) ?: false
        showPermissionScreen()
    }

    override fun onResume() {
        super.onResume()
        if (Environment.isExternalStorageManager()) {
            launchGame()
        } else if (!permissionRequestStarted) {
            requestStorageAccess()
        }
    }

    override fun onSaveInstanceState(outState: Bundle) {
        outState.putBoolean(REQUEST_STARTED, permissionRequestStarted)
        super.onSaveInstanceState(outState)
    }

    private fun showPermissionScreen() {
        val density = resources.displayMetrics.density
        val padding = (32 * density).toInt()

        val content = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            gravity = Gravity.CENTER
            setPadding(padding, padding, padding, padding)
            setBackgroundColor(Color.BLACK)
        }
        content.addView(TextView(this).apply {
            text = getString(R.string.storage_access_message)
            setTextColor(Color.WHITE)
            textSize = 18f
            gravity = Gravity.CENTER
        }, LinearLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT,
            ViewGroup.LayoutParams.WRAP_CONTENT
        ))
        content.addView(Button(this).apply {
            text = getString(R.string.storage_access_button)
            setOnClickListener { requestStorageAccess() }
        }, LinearLayout.LayoutParams(
            ViewGroup.LayoutParams.WRAP_CONTENT,
            ViewGroup.LayoutParams.WRAP_CONTENT
        ).apply {
            topMargin = (24 * density).toInt()
        })
        setContentView(content)
    }

    private fun requestStorageAccess() {
        permissionRequestStarted = true
        val appSettings = Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION).apply {
            data = Uri.parse("package:$packageName")
        }
        try {
            startActivity(appSettings)
        } catch (_: ActivityNotFoundException) {
            try {
                startActivity(Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION))
            } catch (_: ActivityNotFoundException) {
                permissionRequestStarted = false
            }
        }
    }

    private fun launchGame() {
        if (gameStarted)
            return
        gameStarted = true
        startActivity(Intent(this, GameHostActivity::class.java))
        finish()
    }

    companion object {
        private const val REQUEST_STARTED = "storagePermissionRequestStarted"
    }
}
