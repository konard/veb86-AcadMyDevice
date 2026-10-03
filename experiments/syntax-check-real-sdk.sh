#!/usr/bin/env bash
# Проверка компиляции исходников против реальных заголовков ObjectARX 2021
# с помощью zig (clang + заголовки mingw-w64) на Linux. Линковка не выполняется
# (библиотеки ObjectARX собраны MSVC), но каждый .cpp компилируется в объектный файл.
#
# Использование:
#   ARX_SDK=/path/to/ObjectARX2021 ZIG="python3 -m ziglang" ./experiments/syntax-check-real-sdk.sh
#
# Особенности:
#  * в заголовках SDK встречаются #include с другим регистром имени файла
#    (например, "AcCoreDefs.h" при файле accoredefs.h) — на регистрозависимой
#    ФС создаётся каталог с символическими ссылками;
#  * __STDC_LIB_EXT1__ отключает PAL-реализацию функций *_s из c11_Annex_K.h,
#    которые конфликтуют с mingw-w64 (в MSVC их предоставляет CRT).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
: "${ARX_SDK:?set ARX_SDK to ObjectARX 2021 root}"
ZIG="${ZIG:-zig}"
INC="$ARX_SDK/inc"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

# Каталог-заплатка для include с неверным регистром.
FIX="$WORK/casefix"
mkdir -p "$FIX"
grep -rhoE '#\s*include\s*[<"][^">]+[">]' "$INC" "$ROOT/src" \
  | sed -E 's/.*[<"]([^">]+)[">]/\1/' | sort -u \
  | while read -r f; do
      [ -e "$INC/$f" ] && continue
      m="$(cd "$INC" && find . -ipath "./$f" 2>/dev/null | head -1)"
      if [ -n "$m" ]; then
        mkdir -p "$FIX/$(dirname "$f")"
        ln -sf "$INC/$m" "$FIX/$f"
      fi
    done

status=0
for f in "$ROOT"/src/*.cpp; do
  echo "== $(basename "$f")"
  if $ZIG c++ -target x86_64-windows-gnu -c -std=c++17 \
      -fms-extensions -fdeclspec \
      -D__STDC_LIB_EXT1__ -DUNICODE -D_UNICODE -D_WIN64 -D_AFXDLL -DNDEBUG \
      -Wno-everything \
      -I"$INC" -I"$ARX_SDK/inc-x64" -I"$FIX" \
      "$f" -o "$WORK/$(basename "$f" .cpp).o"; then
    echo "   OK"
  else
    status=1
  fi
done
exit $status
