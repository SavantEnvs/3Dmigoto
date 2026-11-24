// Fuzzes 3Dmigoto's HLSL decompiler (HLSLDecompiler/DecompileHLSL.cpp), the code the
// d3d11 wrapper's export/hunting path and `cmd_Decompiler -D` run on a game's shader:
// DecodeDXBC() on the bytecode plus a line-by-line parse of its disassembly text.
//
// Input layout: [u32 little-endian N][N bytes shader bytecode][disassembly text].
// On Windows the text comes from D3DDisassemble of the same bytecode (closed
// Microsoft code that does not exist on Linux), so the harness takes both parts from the
// input, as cmd_Decompiler's Decompile() hands them to DecompileBinaryHLSL.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include "DecompileHLSL.h"

FILE *LogFile = NULL;
bool gLogDebug = false;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	if (size < 4)
		return -1;
	uint32_t n;
	memcpy(&n, data, 4);
	data += 4;
	size -= 4;
	// DecodeDXBC (called first) takes a DXBC container pointer with no length; its
	// fixed header is 32 bytes (see fuzz_decode_dxbc.cpp).
	if (n < 32 || n > size)
		return -1;
	// Bytecode in a uint32-aligned buffer of exactly N bytes (zero-padded to a dword).
	std::vector<uint32_t> bytecode((n + 3) / 4, 0);
	memcpy(bytecode.data(), data, n);
	// Disassembly text, NUL-terminated like D3DDisassemble's blob.
	std::string text(reinterpret_cast<const char *>(data + n), size - n);

	DecompilerSettings settings;
	ParseParameters p = {};
	p.bytecode = bytecode.data();
	p.decompiled = text.c_str();
	p.decompiledSize = text.size();
	p.ZeroOutput = false;
	p.G = &settings;
	bool patched = false, error = false;
	std::string model;
	// DecompileBinaryHLSL catches its own exceptions and reports them via `error`.
	DecompileBinaryHLSL(p, patched, model, error);
	return 0;
}
