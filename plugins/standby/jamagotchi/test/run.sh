#!/usr/bin/env bash
# Build and run the Jamagotchi pure-C unit tests on the host (no device, no
# ESP-IDF). The game model (jama_core) and mini-game kernels (jama_quiz) have no
# LVGL/firmware dependencies.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
main="$here/../main"
tmp="$(mktemp -d)"

cc -std=c11 -Wall -Wextra -I"$main" "$here/test_jama_core.c" "$main/jama_core.c" -o "$tmp/core"
cc -std=c11 -Wall -Wextra -I"$main" "$here/test_jama_quiz.c" "$main/jama_quiz.c" -lm -o "$tmp/quiz"

echo "== jama_core =="; "$tmp/core"
echo "== jama_quiz =="; "$tmp/quiz"
