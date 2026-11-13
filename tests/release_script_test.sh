#!/bin/bash
# Tests the checks in scripts/release.sh (run with --check, so nothing is
# built, signed or sent to Apple).
#
# Each case copies the script into a temporary directory laid out like the
# repository, and puts fake `security` and `git` commands first in PATH:
#   FAKE_IDENTITIES  what `security find-identity` prints
#   FAKE_GIT_STATUS  what `git status --porcelain` prints
#
# Usage: tests/release_script_test.sh

set -uo pipefail

REPO=$(cd "$(dirname "$0")/.." && pwd)
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

# Two Developer ID Application identities (work and personal), as on a Mac
# that has both, plus a development identity.
WORK_HASH=1111111111111111111111111111111111111111
PERSONAL_HASH=2222222222222222222222222222222222222222
IDENTITIES="  1) 0000000000000000000000000000000000000000 \"Apple Development: Test User (DEVTEAM000)\"
  2) $WORK_HASH \"Developer ID Application: Example Inc. (WORKTEAM01)\"
  3) $PERSONAL_HASH \"Developer ID Application: Test User (PERSONAL01)\"
     3 valid identities found"

mkdir -p "$WORK/bin"
cat > "$WORK/bin/security" <<'EOF'
#!/bin/bash
printf '%s\n' "$FAKE_IDENTITIES"
EOF
cat > "$WORK/bin/git" <<'EOF'
#!/bin/bash
printf '%s' "$FAKE_GIT_STATUS"
EOF
chmod +x "$WORK/bin/security" "$WORK/bin/git"

# A fresh copy of the repository layout the script reads.
# $1: version in package-info.json, $2: version in CMakeLists.txt
make_repo() {
    local dir="$WORK/repo"
    rm -rf "$dir"
    mkdir -p "$dir/scripts"
    cp "$REPO/scripts/release.sh" "$dir/scripts/"
    printf '{ "name": "ray.sniff", "version": "%s" }\n' "$1" > "$dir/package-info.json"
    printf 'project(ray.sniff\n  VERSION %s\n  LANGUAGES C CXX)\n' "$2" > "$dir/CMakeLists.txt"
    echo "$dir"
}

passed=0
failed=0

# $1: description, $2: expected exit (pass|fail), $3: text the output must
# contain; the rest: environment assignments for the run. VERSION_JSON,
# VERSION_CMAKE and ARG (default --check), set before the call, change the
# repository copy and the argument.
check() {
    local name=$1 expect=$2 text=$3
    shift 3
    local repo
    repo=$(make_repo "${VERSION_JSON:-0.2.0}" "${VERSION_CMAKE:-0.2.0}")
    local out status
    out=$(env PATH="$WORK/bin:$PATH" FAKE_IDENTITIES="$IDENTITIES" FAKE_GIT_STATUS="" \
              RAYSNIFF_SIGN_IDENTITY="$PERSONAL_HASH" RAYSNIFF_NOTARY_PROFILE="notarytool-personal" \
              "$@" "$repo/scripts/release.sh" "${ARG:---check}" 2>&1)
    status=$?
    local ok=1
    if [[ "$expect" == pass && $status -ne 0 ]]; then ok=0; fi
    if [[ "$expect" == fail && $status -eq 0 ]]; then ok=0; fi
    if [[ "$out" != *"$text"* ]]; then ok=0; fi
    if [[ $ok == 1 ]]; then
        passed=$((passed + 1))
        echo "ok    $name"
    else
        failed=$((failed + 1))
        echo "FAIL  $name (exit $status)"
        printf '      %s\n' "$out"
    fi
}

check "personal identity by SHA-1 passes" pass "checks passed (version 0.2.0)"
check "personal identity by full name passes" pass "checks passed" \
    RAYSNIFF_SIGN_IDENTITY="Developer ID Application: Test User (PERSONAL01)"
check "missing identity stops" fail "set RAYSNIFF_SIGN_IDENTITY" \
    RAYSNIFF_SIGN_IDENTITY=""
check "missing notary profile stops" fail "set RAYSNIFF_NOTARY_PROFILE" \
    RAYSNIFF_NOTARY_PROFILE=""
check "a name that matches both Developer IDs stops" fail "matches 2 Developer ID Application identities" \
    RAYSNIFF_SIGN_IDENTITY="Developer ID Application"
check "an unknown identity stops" fail "matches 0 Developer ID Application identities" \
    RAYSNIFF_SIGN_IDENTITY="3333333333333333333333333333333333333333"
check "a development (not Developer ID) identity stops" fail "matches 0 Developer ID Application identities" \
    RAYSNIFF_SIGN_IDENTITY="0000000000000000000000000000000000000000"
check "uncommitted changes stop" fail "uncommitted changes" \
    FAKE_GIT_STATUS=" M README.md"
VERSION_JSON=0.3.0 check "different versions stop" fail "disagree"
ARG=--upload check "an unknown argument stops" fail "unknown argument"

echo
echo "$passed passed, $failed failed"
[[ $failed == 0 ]]
