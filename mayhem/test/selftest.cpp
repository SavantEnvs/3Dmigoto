// Functional oracle for the portable 3Dmigoto shader toolchain + lite ini scanner.
// Prints one result line per input; mayhem/test.sh diffs every line against
// mayhem/test/golden.txt.
//   selftest asm <file.asm>   assemble with signature parsing, then decode the result
//   selftest dxbc <file>      decode shader bytecode
//   selftest ini <file.ini>   look up [Loader] / [System] settings
//   selftest hlsl <file.asm>  assemble, then decompile the bytecode + text to HLSL
//   selftest pack <file.asm>  write a fuzz_decompile_hlsl input (u32 N, bytecode, text) to stdout
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#include "stdafx.h"
#include "util_min.h"
#include "internal_includes/structs.h"
#include "internal_includes/decode.h"
#include "DecompileHLSL.h"

FILE *LogFile = NULL;
bool gLogDebug = false;

static uint32_t fnv1a(const uint8_t *p, size_t n)
{
	uint32_t h = 2166136261u;
	for (size_t i = 0; i < n; i++) { h ^= p[i]; h *= 16777619u; }
	return h;
}

static std::string describe(const uint8_t *data, size_t size)
{
	if (size < 32)
		return "short";
	std::vector<uint32_t> buf((size + 3) / 4, 0);
	memcpy(buf.data(), data, size);
	Shader *s;
	try {
		s = DecodeDXBC(buf.data());
	} catch (const std::exception &) {
		return "decode-error";
	}
	if (!s)
		return "undecodable";
	char out[160];
	snprintf(out, sizeof(out), "type=%d sm=%u.%u decls=%zu insts=%zu",
		(int)s->eShaderType, s->ui32MajorVersion, s->ui32MinorVersion,
		s->asPhase[MAIN_PHASE].ppsDecl[0].size(), s->asPhase[MAIN_PHASE].ppsInst[0].size());
	delete s;
	return out;
}

int main(int argc, char **argv)
{
	if (argc != 3) { fprintf(stderr, "usage: selftest asm|dxbc|ini <file>\n"); return 2; }
	std::ifstream f(argv[2], std::ios::binary);
	if (!f) { fprintf(stderr, "cannot open %s\n", argv[2]); return 2; }
	std::vector<char> in((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
	std::string mode = argv[1];
	if (mode == "asm") {
		std::vector<byte> bc;
		std::vector<AssemblerParseError> errs;
		try {
			if (FAILED(AssembleFluganWithSignatureParsing(&in, &bc, &errs))) { printf("asm-failed\n"); return 0; }
		} catch (const std::exception &e) {
			printf("asm-exception\n");
			return 0;
		}
		printf("bytes=%zu errors=%zu fourcc=%.4s fnv=%08x %s\n", bc.size(), errs.size(),
			bc.size() >= 4 ? (const char *)bc.data() : "----", fnv1a(bc.data(), bc.size()),
			describe(bc.data(), bc.size()).c_str());
	} else if (mode == "pack") {
		std::vector<byte> bc;
		std::vector<AssemblerParseError> errs;
		try {
			if (FAILED(AssembleFluganWithSignatureParsing(&in, &bc, &errs))) return 1;
		} catch (const std::exception &) { return 1; }
		uint32_t n = (uint32_t)bc.size();
		fwrite(&n, 4, 1, stdout);
		fwrite(bc.data(), 1, bc.size(), stdout);
		fwrite(in.data(), 1, in.size(), stdout);
	} else if (mode == "hlsl") {
		std::vector<byte> bc;
		std::vector<AssemblerParseError> errs;
		try {
			if (FAILED(AssembleFluganWithSignatureParsing(&in, &bc, &errs))) { printf("asm-failed\n"); return 0; }
		} catch (const std::exception &) { printf("asm-exception\n"); return 0; }
		std::vector<uint32_t> buf((bc.size() + 3) / 4, 0);
		memcpy(buf.data(), bc.data(), bc.size());
		std::string text(in.begin(), in.end());
		DecompilerSettings settings;
		ParseParameters p = {};
		p.bytecode = buf.data();
		p.decompiled = text.c_str();
		p.decompiledSize = text.size();
		p.G = &settings;
		bool patched = false, error = false;
		std::string model;
		std::string hlsl = DecompileBinaryHLSL(p, patched, model, error);
		if (getenv("SELFTEST_DUMP"))
			fputs(hlsl.c_str(), stderr);
		// The first line is "// ---- Created with 3Dmigoto vX on <current time>": hash
		// everything after it.
		size_t body = hlsl.compare(0, 8, "// ---- ") == 0 ? hlsl.find('\n') : 0;
		if (body == std::string::npos) body = hlsl.size();
		size_t lines = 0;
		for (char c : hlsl) lines += c == '\n';
		printf("model=%s error=%d patched=%d bytes=%zu lines=%zu fnv=%08x main=%d\n", model.c_str(), error, patched,
			hlsl.size(), lines, fnv1a((const uint8_t *)hlsl.data() + body, hlsl.size() - body),
			hlsl.find("void main(") != std::string::npos);
	} else if (mode == "dxbc") {
		printf("%s\n", describe((const uint8_t *)in.data(), in.size()).c_str());
	} else if (mode == "ini") {
		in.push_back('\0');
		char ret[260];
		const char *sec = find_ini_section_lite(in.data(), "loader");
		printf("loader=%d", sec != NULL);
		if (sec) {
			printf(" target=%s", find_ini_setting_lite(sec, "target", ret, sizeof(ret)) ? ret : "(none)");
			printf(" module=%s", find_ini_setting_lite(sec, "module", ret, sizeof(ret)) ? ret : "(none)");
			printf(" check_version=%d delay=%d", find_ini_bool_lite(sec, "check_version", true),
				find_ini_int_lite(sec, "delay", -7));
		}
		sec = find_ini_section_lite(in.data(), "system");
		printf(" system=%d", sec != NULL);
		if (sec)
			printf(" hook=%d allow=%d", find_ini_bool_lite(sec, "hook", false),
				find_ini_int_lite(sec, "allow_create_device", 99));
		printf("\n");
	} else {
		return 2;
	}
	return 0;
}
