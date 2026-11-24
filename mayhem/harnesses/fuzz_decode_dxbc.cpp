// Fuzzes the DXBC / SM1-3 shader bytecode decoder (BinaryDecompiler/decode.cpp,
// reflect.cpp, decodeDX9.cpp), which 3Dmigoto runs on every shader a game creates and on
// ShaderCache *.bin files before decompiling it to HLSL.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <vector>
#include "internal_includes/structs.h"
#include "internal_includes/decode.h"

FILE *LogFile = NULL;
bool gLogDebug = false;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	// DecodeDXBC(uint32_t *) takes no length: its contract is a pointer to a DXBC
	// container, whose fixed header (DXBCContainerHeader: fourcc, 16-byte digest, version,
	// total size, chunk count) is 32 bytes. Shorter inputs cannot be such a container.
	if (size < 32)
		return -1;
	// Copy into a uint32-aligned buffer of exactly the input size (rounded up to whole
	// dwords with zero padding) so any read past the bytecode is caught by ASan.
	std::vector<uint32_t> buf((size + 3) / 4, 0);
	memcpy(buf.data(), data, size);
	try {
		Shader *shader = DecodeDXBC(buf.data());
		delete shader;
	} catch (const std::exception &) {
		// DecompileError / DecompileErrorDX9: the decoder's documented rejection of
		// unsupported or malformed opcodes (DecompileHLSL.cpp catches it the same way).
	}
	return 0;
}
