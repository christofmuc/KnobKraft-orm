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
    i=$((i+1))
    if [ "$i" -ge "$max_retries" ]; then
      return 1
    fi
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

  # XProtect/Spotlight can also briefly hold the mount during forced detach.
  # Keep retrying the same temporary image; never detach unrelated volumes.
  if [ -n "${2:-}" ] && retry_hdiutil 10 "$@" -force; then
    exit 0
  fi

  # CPack otherwise hides hdiutil's output in its private log directory.
  echo "Failed to detach temporary image after normal and forced retries: ${2:-}" >&2
  hdiutil info >&2 || true
  exit 1
fi

# For all other commands, just run once.
hdiutil "$@"
