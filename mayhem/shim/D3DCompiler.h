#pragma once
// Linux stand-in for Microsoft's closed d3dcompiler_47 header. Only the types the
// fuzzed (portable) code paths name are provided; D3DDisassemble is a stub that
// always fails, so disassembler() is never reachable from the harnesses.
#include "compat.h"
struct ID3D10Blob {
	virtual void *GetBufferPointer() = 0;
	virtual SIZE_T GetBufferSize() = 0;
	virtual unsigned long Release() = 0;
	virtual ~ID3D10Blob() {}
};
typedef ID3D10Blob ID3DBlob;
#define D3D_DISASM_ENABLE_DEFAULT_VALUE_PRINTS 0x2
#define D3D_DISASM_DISABLE_DEBUG_INFO 0x10
static inline HRESULT D3DDisassemble(const void *, SIZE_T, UINT, const char *, ID3DBlob **out) { *out = NULL; return E_FAIL; }
