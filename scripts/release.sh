#!/bin/bash
# Builds ray.sniff~, signs it with a Developer ID, notarizes and staples it,
# and packs the Max package into dist/ray.sniff-<version>-macos.zip.
#
# Usage: scripts/release.sh [--check]
#   --check  runs only the checks and stops before building
#            (tests/release_script_test.sh uses it)
#
# Settings are read ONLY from scripts/release.local.env (not tracked by git;
# copy scripts/release.local.env.example). Environment variables are ignored,
# so a value left in the shell can't pick another identity.
#   RAYSNIFF_TEAM_ID             Team ID to sign with (10 characters)
#   RAYSNIFF_NOTARY_PROFILE      notarytool keychain profile
#                                (xcrun notarytool store-credentials)
#   RAYSNIFF_FORBIDDEN_TEAM_IDS  Team IDs that must never be used, separated
#                                by spaces (e.g. an employer's), optional
# The file is read strictly (format in the .example); a line it can't read
# stops the script instead of being skipped.
#
# The signing identity is the one "Developer ID Application" identity of
# RAYSNIFF_TEAM_ID in the keychain; anything else stops the script. Before
# building, it shows that identity and asks for "y". After signing, it checks
# the Team ID the signature actually carries (design.md 9).
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

# --- settings ---------------------------------------------------------------

CONFIG="scripts/release.local.env"
[[ -f "$CONFIG" ]] || die "$CONFIG not found (copy scripts/release.local.env.example and fill it in)"

# The config is read strictly and never run as shell code: every line must be
# blank, a comment, or KEY=value with a known KEY. A value may be wrapped in
# "double quotes" and followed by a # comment. Anything else (export, single
# quotes, spaces around =, unknown or repeated keys) stops the script, so a
# mistyped line can't silently empty a setting such as the forbidden list.
# Error messages give the line number but not the value.
TEAM_ID=""
PROFILE=""
FORBIDDEN=""
seen=" "
re_line='^([A-Za-z_][A-Za-z0-9_]*)=(.*)$'
re_quoted='^"([^"]*)"[[:space:]]*(#.*)?$'
re_plain="^([^\"'#]*)(#.*)?\$"
lineno=0
while IFS= read -r line || [[ -n "$line" ]]; do
    lineno=$((lineno + 1))
    line=${line%$'\r'}
    [[ "$line" =~ ^[[:space:]]*(#.*)?$ ]] && continue
    [[ "$line" =~ $re_line ]] \
        || die "$CONFIG line $lineno: expected KEY=value (no export, no spaces around =)"
    key=${BASH_REMATCH[1]}
    rest=${BASH_REMATCH[2]}
    if [[ "$rest" =~ $re_quoted ]]; then
        value=${BASH_REMATCH[1]}
    elif [[ "$rest" =~ $re_plain ]]; then
        value=${BASH_REMATCH[1]}
        value=${value%"${value##*[![:space:]]}"}   # trailing spaces
    else
        die "$CONFIG line $lineno: quote values with \"double quotes\" only"
    fi
    [[ "$seen" != *" $key "* ]] || die "$CONFIG line $lineno: $key is set twice"
    seen="$seen$key "
    case "$key" in
        RAYSNIFF_TEAM_ID)            TEAM_ID=$value ;;
        RAYSNIFF_NOTARY_PROFILE)     PROFILE=$value ;;
        RAYSNIFF_FORBIDDEN_TEAM_IDS) FORBIDDEN=$value ;;
        *) die "$CONFIG line $lineno: unknown key $key" ;;
    esac
done < "$CONFIG"

re_team='^[A-Z0-9]{10}$'
[[ -n "$TEAM_ID" ]] || die "set RAYSNIFF_TEAM_ID in $CONFIG"
[[ "$TEAM_ID" =~ $re_team ]] || die "RAYSNIFF_TEAM_ID must be 10 capital letters or digits"
for id in $FORBIDDEN; do
    [[ "$id" =~ $re_team ]] \
        || die "RAYSNIFF_FORBIDDEN_TEAM_IDS: each entry must be 10 capital letters or digits, separated by spaces"
done
[[ -n "$PROFILE" ]] || die "set RAYSNIFF_NOTARY_PROFILE in $CONFIG"
[[ "$PROFILE" =~ ^[A-Za-z0-9._-]+$ ]] \
    || die "RAYSNIFF_NOTARY_PROFILE may contain only letters, digits, '.', '_' and '-'"

is_forbidden() {
    local id
    for id in $FORBIDDEN; do
        [[ "$id" == "$1" ]] && return 0
    done
    return 1
}
is_forbidden "$TEAM_ID" && die "RAYSNIFF_TEAM_ID is in RAYSNIFF_FORBIDDEN_TEAM_IDS"

# --- signing identity -------------------------------------------------------

# Lines look like:  2) <40 hex> "Developer ID Application: <name> (<team>)"
identities=$(security find-identity -v -p codesigning)
selected=$(printf '%s\n' "$identities" \
    | sed -nE "s/^[[:space:]]*[0-9]+\)[[:space:]]+([0-9A-F]{40})[[:space:]]+\"(Developer ID Application: .* \\($TEAM_ID\\))\"$/\1 \2/p")
count=$(printf '%s' "$selected" | grep -c . || true)
[[ "$count" == 1 ]] || die "found $count Developer ID Application identities for team $TEAM_ID (need exactly 1)"
IDENTITY=${selected%% *}     # SHA-1 hash
IDENTITY_NAME=${selected#* }

# --- repository -------------------------------------------------------------

VERSION=$(python3 -I -c 'import json; print(json.load(open("package-info.json"))["version"])')
CMAKE_VERSION=$(sed -nE 's/^ *VERSION ([0-9.]+).*/\1/p' CMakeLists.txt | head -1)
[[ "$VERSION" == "$CMAKE_VERSION" ]] || die "package-info.json ($VERSION) and CMakeLists.txt ($CMAKE_VERSION) disagree"

changes=$(git status --porcelain)   # a failing git stops the script here
[[ -z "$changes" ]] || die "the working tree has uncommitted changes"

echo "version:  $VERSION"
echo "identity: $IDENTITY_NAME"
echo "profile:  $PROFILE"

if [[ "$CHECK_ONLY" == 1 ]]; then
    echo "checks passed"
    exit 0
fi

read -r -p "Sign and notarize with this identity? [y/N] " answer || answer=""
[[ "$answer" == y || "$answer" == Y ]] || die "cancelled"

BUNDLE="externals/ray.sniff~.mxo"
DIST="dist"
STAGE="$DIST/ray.sniff"
ZIP="$DIST/ray.sniff-$VERSION-macos.zip"

# --- build ------------------------------------------------------------------

echo "==> build (Release, Universal)"
rm -rf "$BUNDLE" build-release   # no settings left from an earlier configure
cmake -B build-release -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build build-release -j
archs=$(lipo -archs "$BUNDLE/Contents/MacOS/ray.sniff~")
[[ "$archs" == *x86_64* && "$archs" == *arm64* ]] || die "not a Universal Binary: $archs"

# --- sign -------------------------------------------------------------------

echo "==> sign"
codesign --force --options runtime --timestamp --sign "$IDENTITY" "$BUNDLE"
codesign --verify --strict --verbose=2 "$BUNDLE"
signed_team=$(codesign -dv "$BUNDLE" 2>&1 | sed -n 's/^TeamIdentifier=//p')
[[ "$signed_team" == "$TEAM_ID" ]] || die "signed with team '$signed_team', expected $TEAM_ID"
is_forbidden "$signed_team" && die "signed with a team in RAYSNIFF_FORBIDDEN_TEAM_IDS"

# --- notarize ---------------------------------------------------------------

echo "==> notarize (this waits for Apple)"
rm -rf "$DIST"
mkdir -p "$DIST"
ditto -c -k --keepParent "$BUNDLE" "$DIST/notarize.zip"
xcrun notarytool submit "$DIST/notarize.zip" --keychain-profile "$PROFILE" --wait \
    --output-format json > "$DIST/notarize.json" \
    || die "notarytool submit failed (see the message above and $DIST/notarize.json)"
status=$(python3 -I -c 'import json,sys; print(json.load(open(sys.argv[1]))["status"])' "$DIST/notarize.json")
if [[ "$status" != "Accepted" ]]; then
    id=$(python3 -I -c 'import json,sys; print(json.load(open(sys.argv[1]))["id"])' "$DIST/notarize.json")
    xcrun notarytool log "$id" --keychain-profile "$PROFILE" || true
    die "notarization: $status"
fi
rm "$DIST/notarize.zip"

# --- staple -----------------------------------------------------------------

echo "==> staple"
xcrun stapler staple "$BUNDLE"
xcrun stapler validate "$BUNDLE"

# --- package ----------------------------------------------------------------

echo "==> package"
mkdir -p "$STAGE/externals"
ditto "$BUNDLE" "$STAGE/$BUNDLE"
# Only files tracked by git (no .DS_Store or other local files).
git ls-files -z help README.md LICENSE package-info.json \
    | while IFS= read -r -d '' file; do
          mkdir -p "$STAGE/$(dirname "$file")"
          cp "$file" "$STAGE/$file"
      done
# No extended attributes: they would become ._* files in the zip.
ditto -c -k --norsrc --noextattr --keepParent "$STAGE" "$ZIP"
if unzip -Z1 "$ZIP" | grep -E '(^|/)(\._|\.DS_Store$)'; then
    die "the zip contains the files above"
fi

echo
echo "done: $ZIP"
echo "next: git tag -a v$VERSION && git push origin v$VERSION"
echo "      gh release create v$VERSION $ZIP"
