#!/bin/bash
# Tests the checks in scripts/release.sh. Nothing is built, signed or sent to
# Apple: most cases use --check, and the confirmation cases answer "no".
#
# Each case copies the script into a temporary directory laid out like the
# repository, writes scripts/release.local.env there, and puts fake
# `security` and `git` commands first in PATH:
#   FAKE_IDENTITIES  what `security find-identity` prints
#   FAKE_GIT_STATUS  what `git status --porcelain` prints
#
# Usage: tests/release_script_test.sh

set -uo pipefail

REPO=$(cd "$(dirname "$0")/.." && pwd)
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

PERSONAL=PERSONAL01
EMPLOYER=EMPLOYER01
TWICE=TWOCERTS01   # a team with two Developer ID Application certificates

IDENTITIES="  1) 0000000000000000000000000000000000000000 \"Apple Development: Test User (DEVTEAM000)\"
  2) 1111111111111111111111111111111111111111 \"Developer ID Application: Example Inc. ($EMPLOYER)\"
  3) 2222222222222222222222222222222222222222 \"Developer ID Application: Test User ($PERSONAL)\"
  4) 3333333333333333333333333333333333333333 \"Developer ID Application: Twice (old) ($TWICE)\"
  5) 4444444444444444444444444444444444444444 \"Developer ID Application: Twice (new) ($TWICE)\"
     5 valid identities found"

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

# Settings for the next case (reset after each one).
reset_case() {
    CONFIG="RAYSNIFF_TEAM_ID=$PERSONAL
RAYSNIFF_NOTARY_PROFILE=notarytool-personal
RAYSNIFF_FORBIDDEN_TEAM_IDS=$EMPLOYER"
    NO_CONFIG=0
    VERSION_JSON=0.2.0
    VERSION_CMAKE=0.2.0
    GIT_STATUS=""
    ARG=--check
    ANSWER=""
}
reset_case

make_repo() {
    local dir="$WORK/repo"
    rm -rf "$dir"
    mkdir -p "$dir/scripts"
    cp "$REPO/scripts/release.sh" "$dir/scripts/"
    printf '{ "name": "ray.sniff", "version": "%s" }\n' "$VERSION_JSON" > "$dir/package-info.json"
    printf 'project(ray.sniff\n  VERSION %s\n  LANGUAGES C CXX)\n' "$VERSION_CMAKE" > "$dir/CMakeLists.txt"
    [[ "$NO_CONFIG" == 1 ]] || printf '%s\n' "$CONFIG" > "$dir/scripts/release.local.env"
    echo "$dir"
}

passed=0
failed=0

# $1: description, $2: expected exit (pass|fail), $3: text the output must
# contain, $4 (optional): text the output must not contain. The variables
# set by reset_case can be changed before the call; extra environment
# assignments can follow after "--".
check() {
    local name=$1 expect=$2 text=$3 absent=${4:-}
    shift 4 2>/dev/null || shift $#
    [[ "${1:-}" == "--" ]] && shift
    local repo out status
    repo=$(make_repo)
    out=$(printf '%s\n' "$ANSWER" \
          | env -u RAYSNIFF_TEAM_ID -u RAYSNIFF_NOTARY_PROFILE -u RAYSNIFF_FORBIDDEN_TEAM_IDS \
                PATH="$WORK/bin:$PATH" FAKE_IDENTITIES="$IDENTITIES" FAKE_GIT_STATUS="$GIT_STATUS" \
                "$@" "$repo/scripts/release.sh" $ARG 2>&1)
    status=$?
    local ok=1
    if [[ "$expect" == pass && $status -ne 0 ]]; then ok=0; fi
    if [[ "$expect" == fail && $status -eq 0 ]]; then ok=0; fi
    if [[ "$out" != *"$text"* ]]; then ok=0; fi
    if [[ -n "$absent" && "$out" == *"$absent"* ]]; then ok=0; fi
    if [[ $ok == 1 ]]; then
        passed=$((passed + 1))
        echo "ok    $name"
    else
        failed=$((failed + 1))
        echo "FAIL  $name (exit $status)"
        printf '%s\n' "$out" | sed 's/^/      /'
    fi
    reset_case
}

# --- the personal team is selected ---
check "the personal team passes and its identity is shown" pass \
    "identity: Developer ID Application: Test User ($PERSONAL)"
CONFIG="RAYSNIFF_TEAM_ID=\"$PERSONAL\"   # quoted, with a comment
RAYSNIFF_NOTARY_PROFILE=notarytool-personal"
check "a quoted value with a comment is read" pass "checks passed"

# --- the employer's team is never used ---
CONFIG="RAYSNIFF_TEAM_ID=$EMPLOYER
RAYSNIFF_NOTARY_PROFILE=notarytool-personal
RAYSNIFF_FORBIDDEN_TEAM_IDS=OTHER00001 $EMPLOYER"
check "a forbidden team stops" fail "is in RAYSNIFF_FORBIDDEN_TEAM_IDS" "checks passed"
CONFIG="RAYSNIFF_NOTARY_PROFILE=notarytool-personal"
check "a Team ID in the environment is ignored" fail "set RAYSNIFF_TEAM_ID in" "checks passed" \
    -- RAYSNIFF_TEAM_ID=$PERSONAL
CONFIG="RAYSNIFF_TEAM_ID=$EMPLOYER
RAYSNIFF_NOTARY_PROFILE=notarytool-personal
RAYSNIFF_FORBIDDEN_TEAM_IDS=$EMPLOYER"
check "the file's forbidden list applies even if the environment clears it" fail "is in RAYSNIFF_FORBIDDEN_TEAM_IDS" "checks passed" \
    -- RAYSNIFF_FORBIDDEN_TEAM_IDS=""

# --- a forbidden list that can't be read stops instead of being empty ---
base="RAYSNIFF_TEAM_ID=$PERSONAL
RAYSNIFF_NOTARY_PROFILE=notarytool-personal"
CONFIG="$base
RAYSNIFF_FORBIDDEN_TEAM_IDS='$EMPLOYER'"
check "a single-quoted value stops" fail "double quotes" "checks passed"
CONFIG="$base
RAYSNIFF_FORBIDDEN_TEAM_IDS=$EMPLOYER,OTHER00001"
check "a comma-separated forbidden list stops" fail "separated by spaces" "checks passed"
CONFIG="$base
RAYSNIFF_FORBIDDEN_TEAM_IDS=employer01"
check "a lowercase forbidden Team ID stops" fail "separated by spaces" "checks passed"
CONFIG="$base
export RAYSNIFF_FORBIDDEN_TEAM_IDS=$EMPLOYER"
check "an export line stops" fail "line 3: expected KEY=value" "checks passed"
CONFIG="$base
RAYSNIFF_FORBIDDEN_TEAM_IDS=\"$EMPLOYER\" \"OTHER00001\""
check "several quoted values on one line stop" fail "double quotes" "checks passed"
CONFIG="$base
RAYSNIFF_FORBIDDEN_TEAM_IDS = $EMPLOYER"
check "spaces around = stop" fail "expected KEY=value" "checks passed"
CONFIG="$base
RAYSNIFF_FORBIDEN_TEAM_IDS=$EMPLOYER"
check "a misspelled key stops" fail "unknown key RAYSNIFF_FORBIDEN_TEAM_IDS" "checks passed"
CONFIG="$base
RAYSNIFF_TEAM_ID=$EMPLOYER"
check "a key set twice stops" fail "RAYSNIFF_TEAM_ID is set twice" "checks passed"
CONFIG="RAYSNIFF_TEAM_ID=$EMPLOYER
RAYSNIFF_NOTARY_PROFILE=notarytool-personal
# comment line

RAYSNIFF_FORBIDDEN_TEAM_IDS=\"OTHER00001 $EMPLOYER\"  # two entries"
check "comments, blank lines and a quoted list are read" fail "is in RAYSNIFF_FORBIDDEN_TEAM_IDS" "checks passed"
CONFIG="RAYSNIFF_TEAM_ID=$EMPLOYER
RAYSNIFF_NOTARY_PROFILE=notarytool-personal
RAYSNIFF_FORBIDDEN_TEAM_IDS=$EMPLOYER"
check "the forbidden-team message does not print the Team ID" fail "is in RAYSNIFF_FORBIDDEN_TEAM_IDS" "$EMPLOYER"
CONFIG="RAYSNIFF_TEAM_ID=$PERSONAL
RAYSNIFF_NOTARY_PROFILE='notarytool-personal'"
check "a single-quoted notary profile stops before building" fail "double quotes" "checks passed"

# --- the identity must be exactly one Developer ID Application ---
CONFIG="RAYSNIFF_TEAM_ID=NOSUCHTEAM
RAYSNIFF_NOTARY_PROFILE=notarytool-personal"
check "a team without a certificate stops" fail "found 0 Developer ID Application identities"
CONFIG="RAYSNIFF_TEAM_ID=DEVTEAM000
RAYSNIFF_NOTARY_PROFILE=notarytool-personal"
check "a team with only Apple Development stops" fail "found 0 Developer ID Application identities"
CONFIG="RAYSNIFF_TEAM_ID=$TWICE
RAYSNIFF_NOTARY_PROFILE=notarytool-personal"
check "a team with two certificates stops" fail "found 2 Developer ID Application identities"
CONFIG="RAYSNIFF_TEAM_ID=PERSONAL0
RAYSNIFF_NOTARY_PROFILE=notarytool-personal"
check "a shortened Team ID stops" fail "must be 10 capital letters or digits"

# --- settings and repository ---
NO_CONFIG=1
check "a missing settings file stops" fail "release.local.env not found"
CONFIG="RAYSNIFF_TEAM_ID=$PERSONAL"
check "a missing notary profile stops" fail "set RAYSNIFF_NOTARY_PROFILE in"
GIT_STATUS=" M README.md"
check "uncommitted changes stop" fail "uncommitted changes" "checks passed"
VERSION_JSON=0.3.0
check "different versions stop" fail "disagree"
ARG=--upload
check "an unknown argument stops" fail "unknown argument"

# --- confirmation (no --check; must stop before building) ---
ARG=""
ANSWER=n
check "answering n stops before the build" fail "cancelled" "==> build"
ARG=""
ANSWER=""
check "no answer stops before the build" fail "cancelled" "==> build"

echo
echo "$passed passed, $failed failed"
[[ $failed == 0 ]]
