#!/bin/bash
# Build YataiDON APK (release) and push to connected Android device via ADB.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ANDROID_DIR="$SCRIPT_DIR/platform/android"
ADB="${ANDROID_SDK_ROOT:-$HOME/Android/Sdk}/platform-tools/adb"
APK="$ANDROID_DIR/app/build/outputs/apk/release/app-release.apk"
PACKAGE="com.yataidon.app"
ACTIVITY="com.yataidon.app.YataiDONActivity"

# --- Build ---
echo "==> Building APK..."
cd "$ANDROID_DIR"
./gradlew assembleRelease

if [[ ! -f "$APK" ]]; then
    echo "ERROR: APK not found at $APK"
    exit 1
fi

# --- ADB check ---
if [[ ! -x "$ADB" ]]; then
    echo "ERROR: adb not found at $ADB"
    echo "       Set ANDROID_SDK_ROOT or install platform-tools."
    exit 1
fi

DEVICES=$("$ADB" devices | grep -v "^List" | grep "device$" | wc -l)
if [[ "$DEVICES" -eq 0 ]]; then
    echo "ERROR: No Android device connected."
    echo "       Connect device with USB debugging enabled and retry."
    exit 1
fi

# --- Install ---
echo "==> Installing APK..."
"$ADB" install -r "$APK"

# --- Launch ---
echo "==> Launching $ACTIVITY..."
"$ADB" shell am start -n "$PACKAGE/$ACTIVITY"

echo "==> Done."
