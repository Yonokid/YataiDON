package com.yataidon.app;

import android.app.Activity;
import android.content.Intent;
import android.content.res.AssetManager;
import android.graphics.Color;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.provider.Settings;
import android.util.Log;
import android.view.View;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

// Real launcher entry point (see AndroidManifest.xml). Extracts the bundled
// Songs (skins are downloaded by the onboarding screen) (android/app/build.gradle's copyGameDataAssets task, mirroring
// the iOS GameData bundle -- see cmake/ios_assets.cmake) to /sdcard/YataiDON
// on a background thread, then starts YataiDONActivity/SDL_main.
//
// This has to be a separate plain Activity, not part of YataiDONActivity's
// onCreate(): extraction of a first install's ~1.5GB blocked the calling
// thread long enough that the system's activity-pause/stop watchdog fired
// and froze the process before it ever finished (observed on-device). A
// background thread here keeps onCreate() itself fast, so the normal
// activity lifecycle isn't affected while extraction runs behind it.
public class SplashActivity extends Activity {
    // Matches the hardcoded cwd in set_working_directory_to_executable() (src/libs/file/filesystem.cpp).
    private static final String GAME_DATA_DIR = "/sdcard/YataiDON";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        View background = new View(this);
        background.setBackgroundColor(Color.BLACK);
        setContentView(background);

        // Request MANAGE_EXTERNAL_STORAGE on Android 11+ so we can write /sdcard/YataiDON/.
        // Same pre-existing fire-and-continue behavior as before this moved here:
        // if the user hasn't granted it yet, extraction below will just fail per-file
        // (logged, not fatal) until a later launch after they grant it.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R && !Environment.isExternalStorageManager()) {
            startActivity(new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,
                    Uri.parse("package:" + getPackageName())));
        }

        new Thread(() -> {
            extractGameData();
            runOnUiThread(this::launchGame);
        }, "GameDataExtract").start();
    }

    private void launchGame() {
        startActivity(new Intent(this, YataiDONActivity.class));
        finish();
    }

    private void extractGameData() {
        AssetManager assets = getAssets();
        extractAssetDir(assets, "Songs", new File(GAME_DATA_DIR, "Songs"));
    }

    // Skips files that already exist so later app updates don't clobber
    // the user's own Songs.
    private void extractAssetDir(AssetManager assets, String assetPath, File destDir) {
        String[] children;
        try {
            children = assets.list(assetPath);
        } catch (IOException e) {
            Log.e("YataiDON", "Failed to list assets at " + assetPath, e);
            return;
        }
        if (children == null || children.length == 0) return;
        destDir.mkdirs();
        for (String child : children) {
            String childAssetPath = assetPath + "/" + child;
            File childDest = new File(destDir, child);
            String[] grandchildren;
            try {
                grandchildren = assets.list(childAssetPath);
            } catch (IOException e) {
                grandchildren = null;
            }
            if (grandchildren != null && grandchildren.length > 0) {
                extractAssetDir(assets, childAssetPath, childDest);
            } else if (!childDest.exists()) {
                extractAssetFile(assets, childAssetPath, childDest);
            }
        }
    }

    private void extractAssetFile(AssetManager assets, String assetPath, File dest) {
        try (InputStream in = assets.open(assetPath);
             OutputStream out = new FileOutputStream(dest)) {
            byte[] buf = new byte[64 * 1024];
            int n;
            while ((n = in.read(buf)) > 0) out.write(buf, 0, n);
        } catch (IOException e) {
            Log.e("YataiDON", "Failed to extract asset " + assetPath, e);
        }
    }
}
