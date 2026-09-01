#!/usr/bin/env bash
set -euo pipefail

# Local/manual reference build script — mirrors what .github/workflows/android.yml
# does in CI. Requires: Android SDK + NDK (ndkVersion pinned in app/build.gradle),
# JDK 17, and glslangValidator on PATH (Tempest's host shader compiler).
#
# Usage: bash android/build-android.sh [buildNum]

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_NUM="${1:-0}"

echo "== Building signed release APK (buildNum=$BUILD_NUM) =="
cd "$ROOT/android"
if [ ! -f gradlew ]; then
  if ! command -v gradle >/dev/null 2>&1; then
    echo "ERROR: Gradle is required once to generate the pinned wrapper" >&2
    exit 1
  fi
  gradle wrapper --gradle-version 8.7
fi
chmod +x gradlew
./gradlew assembleRelease -PbuildNum="$BUILD_NUM"

APK="$(find "$ROOT/android" -name '*-release.apk' | head -n1)"
if [ -z "$APK" ]; then
  echo "ERROR: no APK produced" >&2
  exit 1
fi
echo "== Built: $APK =="
