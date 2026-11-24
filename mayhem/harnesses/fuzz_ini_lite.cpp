// Fuzzes 3Dmigoto's minimal d3dx.ini scanner (ini_parser_lite.cpp), the code the loader
// (Injector.cpp) and the DllMain path run on a user-supplied d3dx.ini.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>
#include "util_min.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	// The lite parser works on a NUL-terminated buffer (Injector.cpp reads the file and
	// terminates it), so hand it exactly that.
	std::vector<char> buf(data, data + size);
	buf.push_back('\0');
	const char *ini = buf.data();
	char ret[260];
	char small[8];

	// Same calls Injector.cpp makes on the [Loader] section ...
	const char *section = find_ini_section_lite(ini, "loader");
	if (section) {
		find_ini_setting_lite(section, "target", ret, sizeof(ret));
		find_ini_setting_lite(section, "module", ret, sizeof(ret));
		find_ini_setting_lite(section, "launch", small, sizeof(small));
		find_ini_bool_lite(section, "check_version", true);
		find_ini_bool_lite(section, "require_admin", false);
		find_ini_int_lite(section, "delay", 0);
	}
	// ... and a second section lookup over the same buffer.
	section = find_ini_section_lite(ini, "system");
	if (section) {
		find_ini_setting_lite(section, "proxy_d3d11", ret, sizeof(ret));
		find_ini_bool_lite(section, "hook", false);
		find_ini_int_lite(section, "allow_create_device", 0);
	}
	return 0;
}
