#!/usr/bin/env bash
# clang-format all C++ sources.
#
#   ./scripts/format.sh          format src/ and tests/ in place
#   ./scripts/format.sh --check  verify only (exit 1 on drift; used by CI)
set -euo pipefail

MODE="${1:-}"

fail=0
while IFS= read -r f; do
  if [[ "${MODE}" == "--check" ]]; then
    if ! clang-format --dry-run --Werror "$f" >/dev/null 2>&1; then
      echo "not formatted: $f"
      fail=1
    fi
  else
    clang-format -i "$f"
  fi
done < <(find src tests -type f \( -name '*.cpp' -o -name '*.h' \))

if [[ "${MODE}" == "--check" ]]; then
  if [[ ${fail} -eq 0 ]]; then
    echo "formatting OK"
  else
    echo "run ./scripts/format.sh to apply formatting"
  fi
  exit "${fail}"
fi

echo "formatted."
