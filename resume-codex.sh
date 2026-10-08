#!/usr/bin/env bash
set -euo pipefail

# Keep Codex attached to this repository even when this script is launched
# from another directory.
repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

exec codex resume --last --cd "$repo_root" "$@"
