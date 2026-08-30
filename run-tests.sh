#!/usr/bin/env bash

set -euo pipefail

if ! command -v platformio >/dev/null 2>&1; then
    echo "PlatformIO is not available in the WSL PATH" >&2
    exit 127
fi

exec platformio test -e native "$@"
