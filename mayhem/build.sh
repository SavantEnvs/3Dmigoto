#!/usr/bin/env bash
#
# mayhem/build.sh — builds 3Dmigoto's portable shader toolchain (BinaryDecompiler DXBC
# decoder, Flugan's assembler + signature parser) and the lite d3dx.ini scanner on Linux.
#
# Upstream builds only with Visual Studio on Windows (StereovisionHacks.sln), so this
# compiles the portable translation units directly with clang, using the Win32/MSVC
# stand-ins in mayhem/shim/ (force-included compat.h + empty/stub Windows headers) and
# clang's MS-compatibility mode. No upstream file is modified.
set -euo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${SRC:=/mayhem}"

cd "$SRC"
OUT="$SRC/mayhem/out"
mkdir -p "$OUT/fuzz" "$OUT/test"

# MSVC dialect: -fms-compatibility (with a modern MSVC version so libstdc++'s char16_t etc.
# stay keywords; -fgnuc-version keeps glibc's GNU inline headers working) accepts upstream's what() without noexcept, empty __VA_ARGS__ commas, etc.
LANG_FLAGS="-std=c++14 -fms-extensions -fms-compatibility -fms-compatibility-version=19.40 -fgnuc-version=4.2.1 -Wno-everything \
  -include $SRC/mayhem/shim/compat.h -I$SRC/mayhem/shim -I$SRC -I$SRC/BinaryDecompiler \
  -I$SRC/BinaryDecompiler/include -I$SRC/D3D_Shaders"

SOURCES="BinaryDecompiler/decode.cpp BinaryDecompiler/reflect.cpp BinaryDecompiler/decodeDX9.cpp
  ini_parser_lite.cpp D3D_Shaders/Assembler.cpp D3D_Shaders/SignatureParser.cpp"

# DecompileHLSL.cpp needs two MSVC-only spellings clang cannot take from a header:
#  - #include "BinaryDecompiler\internal_includes\structs.h" (backslash paths): forwarders
#    named literally like that, generated under mayhem/out/inc;
#  - one `for each (T x in range)` loop: rewritten to a range-for in a generated copy that
#    starts with #line, so diagnostics and DWARF still point at the upstream file.
# Both are regenerated from the upstream sources on every run.
GEN="$OUT/gen"
mkdir -p "$GEN" "$OUT/inc"
for h in structs decode; do
  printf '#include "%s/BinaryDecompiler/internal_includes/%s.h"\n' "$SRC" "$h" \
    > "$OUT/inc/BinaryDecompiler\\internal_includes\\$h.h"
done
{
  printf '#line 1 "%s/HLSLDecompiler/DecompileHLSL.cpp"\n' "$SRC"
  sed -E 's/for each[[:space:]]*\((.*)[[:space:]]+in[[:space:]]+(.*)\)[[:space:]]*$/for (\1 : \2)/' \
    "$SRC/HLSLDecompiler/DecompileHLSL.cpp"
} > "$GEN/DecompileHLSL.cpp.tmp"
# keep the file (and its mtime) when unchanged, so re-runs stay cache-friendly
if cmp -s "$GEN/DecompileHLSL.cpp.tmp" "$GEN/DecompileHLSL.cpp"; then rm "$GEN/DecompileHLSL.cpp.tmp"
else mv "$GEN/DecompileHLSL.cpp.tmp" "$GEN/DecompileHLSL.cpp"; fi
LANG_FLAGS="$LANG_FLAGS -I$OUT/inc -I$SRC/HLSLDecompiler"

# compile <outdir> <flags...> — one object per upstream source
compile() {
  local dir="$1"; shift
  local pids=()
  for s in $SOURCES; do
    $CXX $LANG_FLAGS "$@" -c "$SRC/$s" -o "$dir/$(basename "$s" .cpp).o" &
    pids+=($!)
  done
  # UBSan relaxation, this TU only: ParseBufferDefinitions keys the immediate constant
  # buffer as `mCBufferData[-1 << 16]` on EVERY call, a left shift of a negative value
  # (UB before C++20, intended as -65536), so shift-base would abort on every input.
  # pointer-overflow, this TU only: the literal-list parser (DecompileHLSL.cpp ~1800) ends
  # each loop with `beginPos = endPos + 1` where endPos is NULL after the last element,
  # which UBSan reports on every shader with an immediate-constant list (the result is
  # never dereferenced). The rest of UBSan, and ASan, stay on and halting.
  $CXX $LANG_FLAGS "$@" -fno-sanitize=shift-base,pointer-overflow -c "$GEN/DecompileHLSL.cpp" -o "$dir/DecompileHLSL.o" &
  pids+=($!)
  for p in "${pids[@]}"; do wait "$p"; done
}

# 1) The project itself, sanitized + DWARF 3 + SanCov (fuzzer-no-link, unconditionally: without
#    it only the harness TU is coverage-instrumented and the fuzzer is blind to upstream code).
FLAGS="$SANITIZER_FLAGS $DEBUG_FLAGS -O1 -fsanitize=fuzzer-no-link"
# shellcheck disable=SC2086
compile "$OUT/fuzz" $FLAGS
$CXX $FLAGS -c "$SRC/mayhem/lsan_off.cc" -o "$OUT/fuzz/lsan_off.o"
$CC $FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$OUT/fuzz/standalone_main.o"

D="$OUT/fuzz"
DECODE_OBJS="$D/decode.o $D/reflect.o $D/decodeDX9.o"
ASM_OBJS="$D/Assembler.o $D/SignatureParser.o"
INI_OBJS="$D/ini_parser_lite.o"

# 2) Each harness twice: libFuzzer binary + standalone reproducer.
link_harness() {
  local name="$1"; shift
  # shellcheck disable=SC2086
  $CXX $LANG_FLAGS $FLAGS $LIB_FUZZING_ENGINE "$SRC/mayhem/harnesses/$name.cpp" "$@" "$D/lsan_off.o" -o "$SRC/$name"
  # shellcheck disable=SC2086
  $CXX $LANG_FLAGS $FLAGS "$SRC/mayhem/harnesses/$name.cpp" "$@" "$D/lsan_off.o" "$D/standalone_main.o" -o "$SRC/$name-standalone"
}
# shellcheck disable=SC2086
link_harness fuzz_decode_dxbc $DECODE_OBJS
# shellcheck disable=SC2086
link_harness fuzz_assemble $ASM_OBJS
# shellcheck disable=SC2086
link_harness fuzz_ini_lite $INI_OBJS
# shellcheck disable=SC2086
link_harness fuzz_decompile_hlsl "$D/DecompileHLSL.o" $DECODE_OBJS

# 3) The functional oracle, with normal (unsanitized) flags.
# shellcheck disable=SC2086
compile "$OUT/test" -O1 $COVERAGE_FLAGS
T="$OUT/test"
# shellcheck disable=SC2086
$CXX $LANG_FLAGS -O1 $COVERAGE_FLAGS "$SRC/mayhem/test/selftest.cpp" "$T"/*.o -o "$T/selftest"
