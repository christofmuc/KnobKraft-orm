#!/usr/bin/env bash

# Workaround XProtect race condition for "hdiutil create" for MacOS 13

set -e

cmd="$1"

retry_hdiutil() {
  local max_retries="$1"
  shift
  local i=0
  until hdiutil "$@"
  do
    if [ "$i" -eq "$max_retries" ]; then
      return 1
    fi
    i=$((i+1))
    sleep 1
  done
}

if [ "$cmd" = "create" ]; then
  # For an `hdiutil create` command, try repeatedly, up to 10 times.
  # This prevents spurious errors caused by a race condition with XProtect.
  # See https://github.com/actions/runner-images/issues/7522
  retry_hdiutil 10 "$@"
  exit 0
fi

if [ "$cmd" = "detach" ]; then
  # DMG detach can intermittently fail in CI due to temporary volume busy states.
  if retry_hdiutil 10 "$@"; then
    exit 0
  fi

  # Last-resort fallback for stale/busy mounts.
  if [ -n "${2:-}" ]; then
    hdiutil detach -force "$2"
    exit 0
  fi
fi

# For all other commands, just run once.
hdiutil "$@"
