#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
vs_root="/mnt/c/Program Files/Microsoft Visual Studio/18/Community"
sdk_root="/mnt/c/Program Files (x86)/Windows Kits/10/Include"

msvc_inc="$(ls -d "$vs_root/VC/Tools/MSVC/"*/include 2>/dev/null | sort -V | tail -1)"
sdk_inc="$(ls -d "$sdk_root/"*/ 2>/dev/null | sort -V | tail -1)"

if [[ -z "$msvc_inc" || -z "$sdk_inc" ]]; then
  echo "Could not find MSVC or Windows SDK include paths." >&2
  exit 1
fi

cat > "$repo_root/.clangd" <<EOF
CompileFlags:
  Add:
    - --target=x86_64-pc-windows-msvc
    - -std=c++20
    - -fms-compatibility
    - -fms-extensions
    - -D_WIN64
    - -DWIN32
    - -isystem
    - $msvc_inc
    - -isystem
    - ${sdk_inc}ucrt
    - -isystem
    - ${sdk_inc}um
    - -isystem
    - ${sdk_inc}shared

Completion:
  HeaderInsertion: Never
EOF

echo "Wrote $repo_root/.clangd"
echo "  MSVC: $msvc_inc"
echo "  SDK:  ${sdk_inc%/}"
