// Fuzzes Flugan's shader assembler with signature parsing — the path `cmd_Decompiler -a`
// and the d3d11 wrapper take for ShaderFixes/*.asm files: SignatureParser.cpp builds the
// ISGN/OSGN/PCSG/SFI chunks from the assembly comments, then Assembler.cpp assembles the
// instruction text into DXBC.
#include <cstddef>
#include <cstdint>
#include <exception>
#include <vector>
#include "stdafx.h"

FILE *LogFile = NULL;
bool gLogDebug = false;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	std::vector<char> assembly(data, data + size);
	std::vector<byte> bytecode;
	std::vector<AssemblerParseError> errors;
	try {
		// With a parse_errors vector the assembler records per-line errors and keeps
		// going, as cmd_Decompiler does; signature errors are still thrown.
		AssembleFluganWithSignatureParsing(&assembly, &bytecode, &errors);
	} catch (const std::exception &) {
		// Documented rejection of malformed assembly (AssemblerParseError,
		// std::invalid_argument, ...).
	}
	return 0;
}
