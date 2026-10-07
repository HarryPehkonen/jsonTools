#!/usr/bin/env bash
#
# wire — split out of the old tools/ci.sh (2026-10-06, card t_075c0a6f).
#
# Called from gate.toml as `[stage.wire] cmd = "scripts/wire.sh"`. The verdict
# vocabulary (ci_begin/ci_pass/ci_fail/ci_skip) is defined in scripts/gate-env.sh.
set -uo pipefail
. "$(dirname "$0")/gate-env.sh"

run_stage() {
    ci_begin "wire (every tool built, installed, tested, documented)"
    if ! command -v python3 >/dev/null 2>&1; then
        ci_skip wire "python3 not installed"
        return 0
    fi
    if ! python3 "$REPO_ROOT/tools/check_wiring.py" > "$CI_LOG_DIR/wire.log" 2>&1; then
        ci_fail wire "a tool is not fully wired (the missing wires are listed above)" "$CI_LOG_DIR/wire.log"
    fi
    grep -E '^wire:' "$CI_LOG_DIR/wire.log" | sed 's/^/      /'
    ci_pass wire
}

run_stage "$@"
