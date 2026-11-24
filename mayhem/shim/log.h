#pragma once
// Found before the upstream ../../log.h (mayhem/shim is first on the include path).
// Upstream's variadic log macros rely on MSVC dropping the comma before an empty
// __VA_ARGS__; re-define the variadic ones with the GNU ## form so they expand on clang.
#include "../../log.h"
#undef LogInfo
#undef LogInfoW
#undef LogDebug
#undef LogDebugW
#define LogInfo(fmt, ...) \
	do { if (LogFile) fprintf(LogFile, fmt, ##__VA_ARGS__); } while (0)
#define LogInfoW(fmt, ...) \
	do { if (LogFile) fwprintf(LogFile, fmt, ##__VA_ARGS__); } while (0)
#define LogDebug(fmt, ...) \
	do { if (gLogDebug) LogInfo(fmt, ##__VA_ARGS__); } while (0)
#define LogDebugW(fmt, ...) \
	do { if (gLogDebug) LogInfoW(fmt, ##__VA_ARGS__); } while (0)
