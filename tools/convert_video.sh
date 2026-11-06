#!/usr/bin/env bash
# Convert a video into the raw MJPEG stream played by firmware/p4 (video_player):
# 30 fps baseline JPEG frames concatenated, no audio, no container. Each frame
# is pre-rotated into the panel's native portrait layout (OUT_W x OUT_H): the
# 1280x720 landscape video is turned 90 deg clockwise and placed at columns
# STATUS_BAR_PX..OUT_W-1; columns 0..STATUS_BAR_PX-1 (the status bar) are black.
set -euo pipefail

STATUS_BAR_PX=80
JPEG_Q=3
OUT_W=800 OUT_H=1280
VIDEO_FILTER="fps=30,scale=1280:720,transpose=1,pad=${OUT_W}:${OUT_H}:${STATUS_BAR_PX}:0:black"

usage() {
    echo "usage: $(basename "$0") <input.mp4> <output_dir>" >&2
    exit 2
}

file_size() {
    # BSD stat (macOS) first, GNU stat as fallback
    stat -f %z "$1" 2>/dev/null || stat -c %s "$1"
}

main() {
    [[ $# -eq 2 ]] || usage
    local input="$1"
    local out_dir="$2"

    command -v ffmpeg >/dev/null 2>&1 || { echo "error: ffmpeg not found in PATH" >&2; exit 1; }
    [[ -f "$input" ]] || { echo "error: input not found: $input" >&2; exit 1; }
    mkdir -p "$out_dir"

    local name
    name="$(basename "$input")"
    local output="$out_dir/${name%.*}.mjpeg"

    ffmpeg -hide_banner -loglevel error -y -i "$input" \
        -an -vf "$VIDEO_FILTER" -c:v mjpeg -q:v "$JPEG_Q" -pix_fmt yuvj420p \
        -f mjpeg "$output"

    echo "input:  $input ($(file_size "$input") bytes)"
    echo "output: $output ($(file_size "$output") bytes)"
}

main "$@"
