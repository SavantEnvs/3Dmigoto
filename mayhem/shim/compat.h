#pragma once
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstdarg>
#include <cstddef>
#include <ctime>
#include <strings.h>
#include <string>
#include <vector>
#include <stdexcept>
typedef uint32_t DWORD; typedef uint8_t byte; typedef uint8_t BYTE; typedef int32_t HRESULT; typedef unsigned int UINT; typedef int BOOL; typedef uint16_t WORD; typedef char _TCHAR; typedef const char* LPCSTR; typedef void* LPVOID; typedef size_t SIZE_T;
#define S_OK 0
#define S_FALSE 1
#define E_FAIL ((HRESULT)0x80004005)
#define FAILED(x) ((x)<0)
#define SUCCEEDED(x) ((x)>=0)
#define _stricmp strcasecmp
#define _strnicmp strncasecmp
#define localtime_s(a,b) localtime_r(b,a)
#define asctime_s(buf,n,t) asctime_r(t,buf)
#include <cstdarg>
// MSVC sprintf_s: (buf, size, fmt, ...) and the C++ template (buf[N], fmt, ...).
static inline int sprintf_s(char *buf, size_t n, const char *fmt, ...)
{ va_list ap; va_start(ap, fmt); int r = vsnprintf(buf, n, fmt, ap); va_end(ap); return r; }
template <size_t N> static inline int sprintf_s(char (&buf)[N], const char *fmt, ...)
{ va_list ap; va_start(ap, fmt); int r = vsnprintf(buf, N, fmt, ap); va_end(ap); return r; }
#define _snprintf_s(b,n,c,...) snprintf(b,n,__VA_ARGS__)
#define __try try
#define __except(x) catch (...)
#include <cmath>
#include <climits>
#ifndef ARRAYSIZE
#define ARRAYSIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif
#ifndef _countof
#define _countof(a) (sizeof(a) / sizeof((a)[0]))
#endif
// _rotl/_rotr are clang builtins under -fms-extensions.
#define fopen_s(pf, name, mode) ((*(pf) = fopen((name), (mode))) ? 0 : 1)

// MSVC sscanf_s: %s, %c and %[ take an extra (unsigned) buffer-size argument after the
// pointer. glibc has no such function, so scan one conversion at a time, turning the
// size into a field width (truncating rather than failing, which only matters for
// over-long fields).
#include <cstdarg>
static inline int sscanf_s(const char *str, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	int assigned = 0, pos = 0;
	const char *f = fmt;
	std::string seg;
	for (;;) {
		seg.clear();
		// literal prefix up to the next real conversion
		while (*f) {
			if (*f == '%' && f[1] == '%') { seg += "%%"; f += 2; continue; }
			if (*f == '%') break;
			seg += *f++;
		}
		if (!*f) {
			// trailing literal: nothing else to assign
			break;
		}
		f++; // skip '%'
		bool suppress = false;
		if (*f == '*') { suppress = true; f++; }
		int width = 0;
		while (*f >= '0' && *f <= '9') width = width * 10 + (*f++ - '0');
		std::string len;
		while (*f == 'h' || *f == 'l' || *f == 'L' || *f == 'j' || *f == 'z' || *f == 't' || *f == 'q') len += *f++;
		if (!*f) break;
		std::string conv(1, *f);
		if (*f == '[') {
			const char *q = f + 1;
			if (*q == '^') q++;
			if (*q == ']') q++;
			while (*q && *q != ']') q++;
			if (!*q) break;
			conv.assign(f, q + 1 - f);
			f = q + 1;
		} else {
			f++;
		}
		char c = conv[0];
		void *ptr = NULL;
		if (!suppress) {
			ptr = va_arg(ap, void *);
			if (c == 's' || c == 'c' || c == '[') {
				unsigned size = va_arg(ap, unsigned);
				if (size == 0) break;
				int maxw = (c == 'c') ? (int)size : (int)size - 1;
				if (c == 'c' && width == 0) width = 1;
				if (width == 0 || width > maxw) width = maxw;
				if (width <= 0) break;
			}
		}
		seg += '%';
		if (suppress) seg += '*';
		if (width) seg += std::to_string(width);
		seg += len;
		seg += conv;
		seg += "%n";
		int n = -1, r;
		if (c == 'n') {
			// %n itself: just report the consumed position
			int n2 = -1;
			std::string pre = seg.substr(0, seg.size() - conv.size() - len.size() - 3 - (suppress ? 1 : 0) - (width ? std::to_string(width).size() : 0));
			pre += "%n";
			sscanf(str + pos, pre.c_str(), &n2);
			if (n2 < 0) break;
			pos += n2;
			if (ptr) *(int *)ptr = pos;
			continue;
		}
		if (suppress)
			r = sscanf(str + pos, seg.c_str(), &n);
		else
			r = sscanf(str + pos, seg.c_str(), ptr, &n);
		if (r == EOF) { if (assigned == 0) assigned = EOF; break; }
		if (n < 0) { if (!suppress && r == 1) assigned++; break; }
		if (!suppress) assigned++;
		pos += n;
	}
	va_end(ap);
	return assigned;
}

// MSVC strcpy_s / strcat_s / strncpy_s: on a destination too small for the result the CRT
// calls the invalid-parameter handler, which by default terminates the process. Mirror
// that with abort() so an overflow the Windows build would die on is reported here too.
static inline int strcpy_s(char *d, size_t n, const char *s)
{ size_t l = strlen(s); if (!d || n == 0 || l >= n) abort(); memcpy(d, s, l + 1); return 0; }
static inline int strcat_s(char *d, size_t n, const char *s)
{ size_t dl = strnlen(d, n); if (dl == n) abort(); size_t l = strlen(s); if (dl + l >= n) abort(); memcpy(d + dl, s, l + 1); return 0; }
static inline int strncpy_s(char *d, size_t n, const char *s, size_t c)
{ size_t l = strnlen(s, c); if (!d || n == 0 || l >= n) abort(); memcpy(d, s, l); d[l] = '\0'; return 0; }
template <size_t N> static inline int strcpy_s(char (&d)[N], const char *s) { return strcpy_s(d, N, s); }
template <size_t N> static inline int strcat_s(char (&d)[N], const char *s) { return strcat_s(d, N, s); }
