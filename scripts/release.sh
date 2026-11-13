#!/bin/bash
# Builds ray.sniff~, signs it with a Developer ID, notarizes and staples it,
# and packs the Max package into dist/ray.sniff-<version>-macos.zip.
#
# Usage:
#   RAYSNIFF_SIGN_IDENTITY=<SHA-1 hash or full identity name> \
#   RAYSNIFF_NOTARY_PROFILE=<notarytool keychain profile> \
#   scripts/release.sh [--check]
#
# --check runs only the checks below and stops before building
# (tests/release_script_test.sh uses it).
#
# Both must be given explicitly: the keychain may hold more than one
# "Developer ID Application" identity (design.md 9).
#   security find-identity -v -p codesigning   lists the identities
#   xcrun notarytool store-credentials          creates a profile
#
# The version is read from package-info.json. Nothing is uploaded.

set -euo pipefail

die() { echo "error: $*" >&2; exit 1; }

CHECK_ONLY=0
case "${1:-}" in
    "") ;;
    --check) CHECK_ONLY=1 ;;
    *) die "unknown argument: $1 (only --check is accepted)" ;;
esac

ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"

IDENTITY=${RAYSNIFF_SIGN_IDENTITY:-}
PROFILE=${RAYSNIFF_NOTARY_PROFILE:-}
[[ -n "$IDENTITY" ]] || die "set RAYSNIFF_SIGN_IDENTITY (SHA-1 hash or full name of a Developer ID Application identity)"
[[ -n "$PROFILE" ]] || die "set RAYSNIFF_NOTARY_PROFILE (a notarytool keychain profile)"

# The identity must match exactly one Developer ID Application identity.
matches=$(security find-identity -v -p codesigning | grep "Developer ID Application" | grep -cF -- "$IDENTITY" || true)
[[ "$matches" == 1 ]] || die "RAYSNIFF_SIGN_IDENTITY matches $matches Developer ID Application identities (need exactly 1)"

VERSION=$(python3 -I -c 'import json; print(json.load(open("package-info.json"))["version"])')
CMAKE_VERSION=$(sed -nE 's/^ *VERSION ([0-9.]+).*/\1/p' CMakeLists.txt | head -1)
[[ "$VERSION" == "$CMAKE_VERSION" ]] || die "package-info.json ($VERSION) and CMakeLists.txt ($CMAKE_VERSION) disagree"

[[ -z "$(git status --porcelain)" ]] || die "the working tree has uncommitted changes"

if [[ "$CHECK_ONLY" == 1 ]]; then
    echo "checks passed (version $VERSION)"
    exit 0
fi

BUNDLE="externals/ray.sniff~.mxo"
DIST="dist"
STAGE="$DIST/ray.sniff"
ZIP="$DIST/ray.sniff-$VERSION-macos.zip"

echo "==> build (Release, Universal)"
rm -rf "$BUNDLE"
cmake -B build-release -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build build-release -j
archs=$(lipo -archs "$BUNDLE/Contents/MacOS/ray.sniff~")
[[ "$archs" == *x86_64* && "$archs" == *arm64* ]] || die "not a Universal Binary: $archs"

echo "==> sign"
codesign --force --options runtime --timestamp --sign "$IDENTITY" "$BUNDLE"
codesign --verify --strict --verbose=2 "$BUNDLE"

echo "==> notarize (this waits for Apple)"
rm -rf "$DIST"
mkdir -p "$DIST"
ditto -c -k --keepParent "$BUNDLE" "$DIST/notarize.zip"
xcrun notarytool submit "$DIST/notarize.zip" --keychain-profile "$PROFILE" --wait \
    --output-format json > "$DIST/notarize.json"
status=$(python3 -I -c 'import json,sys; print(json.load(open(sys.argv[1]))["status"])' "$DIST/notarize.json")
if [[ "$status" != "Accepted" ]]; then
    id=$(python3 -I -c 'import json,sys; print(json.load(open(sys.argv[1]))["id"])' "$DIST/notarize.json")
    xcrun notarytool log "$id" --keychain-profile "$PROFILE" || true
    die "notarization: $status"
fi
rm "$DIST/notarize.zip"

echo "==> staple"
xcrun stapler staple "$BUNDLE"
xcrun stapler validate "$BUNDLE"

echo "==> package"
mkdir -p "$STAGE"
ditto "$BUNDLE" "$STAGE/$BUNDLE"
ditto help "$STAGE/help"
cp README.md LICENSE package-info.json "$STAGE/"
ditto -c -k --keepParent "$STAGE" "$ZIP"

echo
echo "done: $ZIP"
echo "next: git tag -a v$VERSION && git push origin v$VERSION"
echo "      gh release create v$VERSION $ZIP"
