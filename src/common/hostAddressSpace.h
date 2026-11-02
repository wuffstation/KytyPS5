#ifndef EMULATOR_SRC_COMMON_HOSTADDRESSSPACE_H_
#define EMULATOR_SRC_COMMON_HOSTADDRESSSPACE_H_

#include "common/common.h"

#include <cstdint>
#include <cstdio>

namespace Common {

// The highest user address this process can map, as measured rather than assumed, or 0 when the
// host cannot be asked.
//
// The fixed PS5 windows (user memory up to 0xFBFFFFFFFF, the host arena up to 0xFC00000000) fit
// every x86-64 desktop, where user space is 47 bits. They do not fit an arm64 Linux kernel built
// for a 39-bit VA, which gives a process 512 GiB: the common Android configuration, where the
// Android host runs this build as guest code under FEXCore. The kernel places the process stack
// just under that ceiling, so the highest mapping is the measurement.
inline uint64_t HostUserAddressLimit() {
#if KYTY_PLATFORM == KYTY_PLATFORM_LINUX && !defined(__APPLE__)
	static const uint64_t limit = [] {
		uint64_t highest = 0;
		FILE*    maps    = std::fopen("/proc/self/maps", "r");
		if (maps == nullptr) {
			return highest;
		}
		char line[512];
		while (std::fgets(line, sizeof(line), maps) != nullptr) {
			unsigned long long start = 0;
			unsigned long long end   = 0;
			// [vsyscall] sits above 2^48 on x86-64 and is the kernel's, not ours.
			if (std::sscanf(line, "%llx-%llx", &start, &end) == 2 && end <= (1ull << 48u) &&
			    end > highest) {
				highest = end;
			}
		}
		std::fclose(maps);
		return highest;
	}();
	return limit;
#else
	return 0;
#endif
}

} // namespace Common

#endif // EMULATOR_SRC_COMMON_HOSTADDRESSSPACE_H_
