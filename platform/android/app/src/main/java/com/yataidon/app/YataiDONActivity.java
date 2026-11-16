package com.yataidon.app;

import android.app.Presentation;
import android.content.Intent;
import android.hardware.display.DisplayManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import android.view.Display;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowInsetsController;

import androidx.core.content.FileProvider;

import java.io.File;

import org.libsdl.app.SDLActivity;

// SplashActivity (the actual launcher entry point) extracts bundled Skins/Songs
// to /sdcard/YataiDON before starting this activity, so they're on disk before
// SDL_main() ever looks for them.
public class YataiDONActivity extends SDLActivity {

    private static native void nativeDrumSurface(Surface surface, int width, int height);
    private static native void nativeDrumTouch(boolean down, int id, float x, float y, int width, int height);

    private Presentation drumScreen;
    private final DisplayManager.DisplayListener displayListener = new DisplayManager.DisplayListener() {
        @Override public void onDisplayAdded(int displayId) { showDrumScreen(); }
        @Override public void onDisplayRemoved(int displayId) {}
        @Override public void onDisplayChanged(int displayId) {}
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        hideSystemUI();
        if (mBrokenLibraries) return;
        getSystemService(DisplayManager.class).registerDisplayListener(displayListener, null);
        showDrumScreen();
    }

    @Override
    protected void onDestroy() {
        if (!mBrokenLibraries) {
            getSystemService(DisplayManager.class).unregisterDisplayListener(displayListener);
            if (drumScreen != null) drumScreen.dismiss();
        }
        super.onDestroy();
    }

    private void showDrumScreen() {
        if (drumScreen != null) return;
        int own = getDisplay().getDisplayId();
        for (Display display : getSystemService(DisplayManager.class).getDisplays()) {
            if (display.getDisplayId() == own) continue;
            Presentation screen = new Presentation(this, display);
            SurfaceView view = new SurfaceView(screen.getContext());
            view.getHolder().addCallback(new SurfaceHolder.Callback() {
                @Override public void surfaceCreated(SurfaceHolder holder) {}
                @Override public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
                    nativeDrumSurface(holder.getSurface(), width, height);
                }
                @Override public void surfaceDestroyed(SurfaceHolder holder) {
                    nativeDrumSurface(null, 0, 0);
                }
            });
            view.setOnTouchListener((v, e) -> {
                forwardDrumTouch(e, v.getWidth(), v.getHeight());
                return true;
            });
            screen.setContentView(view);
            screen.setOnDismissListener(d -> { if (drumScreen == screen) drumScreen = null; });
            try {
                screen.show();
            } catch (Exception e) {
                Log.w("YataiDON", "second screen unavailable", e);
                continue;
            }
            drumScreen = screen;
            return;
        }
    }

    private static void forwardDrumTouch(MotionEvent e, int width, int height) {
        int action = e.getActionMasked();
        if (action == MotionEvent.ACTION_CANCEL) {
            for (int i = 0; i < e.getPointerCount(); i++) {
                nativeDrumTouch(false, e.getPointerId(i), e.getX(i), e.getY(i), width, height);
            }
            return;
        }
        boolean down = action == MotionEvent.ACTION_DOWN || action == MotionEvent.ACTION_POINTER_DOWN;
        if (!down && action != MotionEvent.ACTION_UP && action != MotionEvent.ACTION_POINTER_UP) return;
        int i = e.getActionIndex();
        nativeDrumTouch(down, e.getPointerId(i), e.getX(i), e.getY(i), width, height);
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) hideSystemUI();
    }

    private void hideSystemUI() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            getWindow().setDecorFitsSystemWindows(false);
            WindowInsetsController ctrl = getWindow().getInsetsController();
            if (ctrl != null) {
                ctrl.hide(WindowInsets.Type.statusBars() | WindowInsets.Type.navigationBars());
                ctrl.setSystemBarsBehavior(WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
            }
        } else {
            getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_FULLSCREEN);
        }
    }

    // SDL3 is statically linked into libYataiDON.so, so we only load our lib.
    @Override
    protected String[] getLibraries() {
        return new String[] { "YataiDON" };
    }

    // Called from native code (network.cpp) after downloading a new build's
    // APK. Wraps it in a FileProvider content:// URI (a plain file:// Uri
    // throws FileUriExposedException on this targetSdk) and hands off to the
    // system package installer, which owns the confirmation UI from here.
    public void installApk(String path) {
        runOnUiThread(() -> {
            try {
                Uri uri = FileProvider.getUriForFile(this, getPackageName() + ".fileprovider", new File(path));
                Intent intent = new Intent(Intent.ACTION_VIEW);
                intent.setDataAndType(uri, "application/vnd.android.package-archive");
                intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_ACTIVITY_NEW_TASK);
                startActivity(intent);
            } catch (Exception e) {
                Log.e("YataiDON", "installApk failed", e);
            }
        });
    }
}
