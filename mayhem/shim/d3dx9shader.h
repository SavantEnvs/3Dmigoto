#pragma once
#include "D3DCompiler.h"
typedef ID3D10Blob *LPD3DXBUFFER;
static inline HRESULT D3DXAssembleShader(const char *, UINT, const void *, void *, DWORD, LPD3DXBUFFER *out, LPD3DXBUFFER *err) { *out = NULL; if (err) *err = NULL; return E_FAIL; }
