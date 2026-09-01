package opengothic.gothic2

import android.content.Intent
import android.os.Build
import android.os.Bundle
import android.os.Environment
import android.view.View
import android.view.WindowInsets
import android.view.WindowInsetsController
import android.view.WindowManager
import com.google.androidgamesdk.GameActivity

class GameHostActivity : GameActivity() {
    private var returningToStorageGate = false

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        if (!Environment.isExternalStorageManager()) {
            returnToStorageGate()
            return
        }

        // A game gets no touch input during cutscenes/loading, so the display would
        // hit its screen-off timeout and doze. That destroys the Vulkan surface and
        // stalls the engine mid-frame. Keep the screen awake while we are foreground.
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        enterImmersiveMode()
    }

    override fun onResume() {
        super.onResume()
        if (!Environment.isExternalStorageManager())
            returnToStorageGate()
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        if (hasFocus) enterImmersiveMode()
    }

    private fun returnToStorageGate() {
        if (returningToStorageGate)
            return
        returningToStorageGate = true
        startActivity(Intent(this, MainActivity::class.java))
        finish()
    }

    private fun enterImmersiveMode() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            window.setDecorFitsSystemWindows(false)
            window.insetsController?.apply {
                hide(WindowInsets.Type.systemBars())
                systemBarsBehavior =
                    WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
            }
        } else {
            @Suppress("DEPRECATION")
            window.decorView.systemUiVisibility =
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY or
                View.SYSTEM_UI_FLAG_FULLSCREEN or
                View.SYSTEM_UI_FLAG_HIDE_NAVIGATION or
                View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN or
                View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION or
                View.SYSTEM_UI_FLAG_LAYOUT_STABLE
        }
    }

    companion object { init { System.loadLibrary("Gothic2Notr") } }
}
