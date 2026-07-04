#!/usr/bin/env bash
# Removes every generated build directory this project's scripts create.
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
rm -rf "${repo_root}"/build "${repo_root}"/build-*
echo "Cleaned build directories under ${repo_root}"
