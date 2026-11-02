#ifndef EMULATOR_SRC_COMMON_ANDROIDHOST_H_
#define EMULATOR_SRC_COMMON_ANDROIDHOST_H_

#include "common/common.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

#if KYTY_PLATFORM == KYTY_PLATFORM_LINUX && !defined(__APPLE__)
#include <dlfcn.h>
#define KYTY_ANDROID_HOST_REACHABLE 1
#else
#define KYTY_ANDROID_HOST_REACHABLE 0
#endif

// The Android host runs this linux-x64 build as guest code inside an app, under FEXCore. Nothing
// about the build is Android: each seam below is selected at runtime by an environment variable the
// host sets, so none of them is reachable on a desktop Linux host.
namespace Common::AndroidHost {

inline bool IsSelected(const char* variable) {
	const char* value = std::getenv(variable);
	if (value == nullptr) {
		return false;
	}
	const char* expected = "android";
	for (; *value != '\0' && *expected != '\0'; value++, expected++) {
		if ((*value | 0x20) != *expected) {
			return false;
		}
	}
	return *value == '\0' && *expected == '\0';
}

// SDL's Linux build has no video driver Android can open (X11, Wayland, KMSDRM). Present through
// VK_EXT_headless_surface instead: the host supplies it and owns the real surface behind it.
inline bool WindowSelected() {
	static const bool selected = IsSelected("KYTY_HOST_WINDOW");
	return selected;
}

// WIDTHxHEIGHT of the host's surface, which only the host knows: it holds the ANativeWindow.
inline bool WindowSize(uint32_t* width, uint32_t* height) {
	const char* value = std::getenv("KYTY_HOST_WINDOW_SIZE");
	unsigned    w     = 0;
	unsigned    h     = 0;
	if (value == nullptr || std::sscanf(value, "%ux%u", &w, &h) != 2 || w == 0 || h == 0) {
		return false;
	}
	*width  = w;
	*height = h;
	return true;
}

} // namespace Common::AndroidHost

#endif // EMULATOR_SRC_COMMON_ANDROIDHOST_H_
