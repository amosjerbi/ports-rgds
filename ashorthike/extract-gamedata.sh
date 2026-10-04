#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: extract-gamedata.sh PATH/TO/game-root.dwarfs [OUTPUT-DIR]

Extracts a user-supplied DwarFS game archive and prepares the PortMaster
gamedata directory. Requires dwarfsextract (part of DwarFS).
EOF
}

if [[ ${1:-} == "-h" || ${1:-} == "--help" ]]; then
  usage
  exit 0
fi

if [[ $# -lt 1 || $# -gt 2 ]]; then
  usage >&2
  exit 2
fi

archive=$1
output=${2:-"$(cd "$(dirname "$0")" && pwd)/ashorthike/gamedata"}

if [[ ! -f "$archive" ]]; then
  printf 'Error: archive not found: %s\n' "$archive" >&2
  exit 1
fi
if ! command -v dwarfsextract >/dev/null 2>&1; then
  printf 'Error: dwarfsextract is missing. Install DwarFS first (macOS: brew install dwarfs).\n' >&2
  exit 1
fi
if [[ -e "$output" ]] && [[ -n "$(find "$output" -mindepth 1 -maxdepth 1 -print -quit 2>/dev/null)" ]]; then
  printf 'Error: output directory is not empty: %s\n' "$output" >&2
  printf 'Move or remove its contents before extracting.\n' >&2
  exit 1
fi

mkdir -p "$output"
output=$(cd "$output" && pwd)
work=$(mktemp -d "${TMPDIR:-/tmp}/ashorthike-extract.XXXXXX")
cleanup() { rm -rf "$work"; }
trap cleanup EXIT
mkdir "$work/unpacked"

printf 'Extracting %s ...\n' "$archive"
dwarfsextract -i "$archive" -o "$work/unpacked"

game_exe=$(find "$work/unpacked" -type f -name AShortHike.x86_64 -print -quit)
if [[ -z "$game_exe" ]]; then
  printf 'Error: AShortHike.x86_64 was not found in the archive.\n' >&2
  exit 1
fi
game_root=$(dirname "$game_exe")
if [[ ! -f "$game_root/UnityPlayer.so" || ! -d "$game_root/AShortHike_Data" ]]; then
  printf 'Error: extracted game folder is missing UnityPlayer.so or AShortHike_Data/.\n' >&2
  exit 1
fi

cp -R "$game_root/." "$output/"
printf 'Game files extracted to %s\n' "$output"
