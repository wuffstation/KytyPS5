#ifndef EMULATOR_SRC_COMMON_ANDROIDHOST_H_
#define EMULATOR_SRC_COMMON_ANDROIDHOST_H_

#include "common/common.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <type_traits>

#if KYTY_PLATFORM == KYTY_PLATFORM_LINUX && !defined(__APPLE__)
#include <dlfcn.h>
#include <time.h>
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

// None of SDL's Linux audio backends (PipeWire, PulseAudio, JACK, ALSA) is reachable from an app.
inline bool AudioSelected() {
	static const bool selected = IsSelected("KYTY_HOST_AUDIO");
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

// --- AAudio --------------------------------------------------------------------------------------
//
// Through the guest-side libaaudio.so the host layer stages beside glibc, whose every export is a
// stub into the host's real AAudio. Resolved by name at runtime so that nothing links against a
// library a desktop does not have. The data callback is never used: the host boundary is one-way, so
// there is no thread that could call back into this process.

inline constexpr int32_t AAUDIO_OK                     = 0;
inline constexpr int32_t AAUDIO_FORMAT_PCM_I16         = 1;
inline constexpr int32_t AAUDIO_FORMAT_PCM_FLOAT       = 2;
inline constexpr int32_t AAUDIO_DIRECTION_OUTPUT       = 0;
inline constexpr int32_t AAUDIO_SHARING_MODE_SHARED    = 1;
inline constexpr int32_t AAUDIO_PERFORMANCE_LOW_LATENCY = 12;
inline constexpr int32_t AAUDIO_USAGE_GAME             = 14;

struct AAudioApi {
	int32_t (*create_stream_builder)(void** builder)                     = nullptr;
	void (*set_sample_rate)(void* builder, int32_t rate)                 = nullptr;
	void (*set_channel_count)(void* builder, int32_t channels)           = nullptr;
	void (*set_format)(void* builder, int32_t format)                    = nullptr;
	void (*set_direction)(void* builder, int32_t direction)              = nullptr;
	void (*set_sharing_mode)(void* builder, int32_t mode)                = nullptr;
	void (*set_performance_mode)(void* builder, int32_t mode)            = nullptr;
	void (*set_usage)(void* builder, int32_t usage)                      = nullptr;
	int32_t (*open_stream)(void* builder, void** stream)                 = nullptr;
	int32_t (*delete_builder)(void* builder)                             = nullptr;
	int32_t (*request_start)(void* stream)                               = nullptr;
	int32_t (*request_stop)(void* stream)                                = nullptr;
	int32_t (*close)(void* stream)                                       = nullptr;
	int32_t (*write)(void* stream, const void* data, int32_t frames, int64_t timeout_ns) = nullptr;
	int32_t (*get_buffer_capacity)(void* stream)                         = nullptr;
	int32_t (*set_buffer_size)(void* stream, int32_t frames)             = nullptr;
	int32_t (*get_sample_rate)(void* stream)                             = nullptr;
	int32_t (*get_channel_count)(void* stream)                           = nullptr;
	int32_t (*get_format)(void* stream)                                  = nullptr;
};

inline const AAudioApi* GetAAudio() {
#if KYTY_ANDROID_HOST_REACHABLE
	static const AAudioApi* api = []() -> const AAudioApi* {
		void* library = ::dlopen("libaaudio.so", RTLD_NOW);
		if (library == nullptr) {
			std::printf("[android-host] libaaudio.so: %s\n", ::dlerror());
			return nullptr;
		}
		static AAudioApi resolved;
		bool             complete = true;
		auto             bind     = [&](auto& slot, const char* name) {
            slot = reinterpret_cast<std::remove_reference_t<decltype(slot)>>(::dlsym(library, name));
            if (slot == nullptr) {
                std::printf("[android-host] AAudio is missing %s\n", name);
                complete = false;
            }
		};
		bind(resolved.create_stream_builder, "AAudio_createStreamBuilder");
		bind(resolved.set_sample_rate, "AAudioStreamBuilder_setSampleRate");
		bind(resolved.set_channel_count, "AAudioStreamBuilder_setChannelCount");
		bind(resolved.set_format, "AAudioStreamBuilder_setFormat");
		bind(resolved.set_direction, "AAudioStreamBuilder_setDirection");
		bind(resolved.set_sharing_mode, "AAudioStreamBuilder_setSharingMode");
		bind(resolved.set_performance_mode, "AAudioStreamBuilder_setPerformanceMode");
		bind(resolved.set_usage, "AAudioStreamBuilder_setUsage");
		bind(resolved.open_stream, "AAudioStreamBuilder_openStream");
		bind(resolved.delete_builder, "AAudioStreamBuilder_delete");
		bind(resolved.request_start, "AAudioStream_requestStart");
		bind(resolved.request_stop, "AAudioStream_requestStop");
		bind(resolved.close, "AAudioStream_close");
		bind(resolved.write, "AAudioStream_write");
		bind(resolved.get_buffer_capacity, "AAudioStream_getBufferCapacityInFrames");
		bind(resolved.set_buffer_size, "AAudioStream_setBufferSizeInFrames");
		bind(resolved.get_sample_rate, "AAudioStream_getSampleRate");
		bind(resolved.get_channel_count, "AAudioStream_getChannelCount");
		bind(resolved.get_format, "AAudioStream_getFormat");
		return complete ? &resolved : nullptr;
	}();
	return api;
#else
	return nullptr;
#endif
}

// An output stream, or nullptr. The device buffer is sized to latency_ms, and that depth is what
// paces the writer: a full buffer accepts nothing.
inline void* AAudioOpenOutput(uint32_t sample_rate, uint32_t channels, bool is_float,
                              uint32_t latency_ms = 60) {
	const auto* api = GetAAudio();
	if (api == nullptr) {
		return nullptr;
	}
	void* builder = nullptr;
	if (api->create_stream_builder(&builder) != AAUDIO_OK || builder == nullptr) {
		return nullptr;
	}
	api->set_sample_rate(builder, static_cast<int32_t>(sample_rate));
	api->set_channel_count(builder, static_cast<int32_t>(channels));
	api->set_format(builder, is_float ? AAUDIO_FORMAT_PCM_FLOAT : AAUDIO_FORMAT_PCM_I16);
	api->set_direction(builder, AAUDIO_DIRECTION_OUTPUT);
	api->set_sharing_mode(builder, AAUDIO_SHARING_MODE_SHARED);
	api->set_performance_mode(builder, AAUDIO_PERFORMANCE_LOW_LATENCY);
	api->set_usage(builder, AAUDIO_USAGE_GAME);
	void*      stream = nullptr;
	const auto status = api->open_stream(builder, &stream);
	api->delete_builder(builder);
	if (status != AAUDIO_OK || stream == nullptr) {
		std::printf("[android-host] AAudio open failed: %d\n", status);
		return nullptr;
	}

	// AAudio negotiates. Refuse rather than write samples in a layout the device did not agree to.
	if (api->get_sample_rate(stream) != static_cast<int32_t>(sample_rate) ||
	    api->get_channel_count(stream) != static_cast<int32_t>(channels) ||
	    api->get_format(stream) != (is_float ? AAUDIO_FORMAT_PCM_FLOAT : AAUDIO_FORMAT_PCM_I16)) {
		std::printf("[android-host] AAudio negotiated %d Hz %d ch format %d for %u Hz %u ch %s\n",
		            api->get_sample_rate(stream), api->get_channel_count(stream),
		            api->get_format(stream), sample_rate, channels, is_float ? "float" : "s16");
		api->close(stream);
		return nullptr;
	}

	const auto wanted   = static_cast<int32_t>(sample_rate * latency_ms / 1000u);
	const auto capacity = api->get_buffer_capacity(stream);
	if (wanted > 0 && capacity > 0) {
		api->set_buffer_size(stream, wanted < capacity ? wanted : capacity);
	}
	if (api->request_start(stream) != AAUDIO_OK) {
		api->close(stream);
		return nullptr;
	}
	return stream;
}

inline void AAudioCloseOutput(void* stream) {
	const auto* api = GetAAudio();
	if (api != nullptr && stream != nullptr) {
		api->request_stop(stream);
		api->close(stream);
	}
}

// Hands every frame to the device, or gives up after 250 ms. The write itself never waits: a guest
// thread parked inside a host driver call cannot be reached by the host layer's signal delivery,
// which happens at syscall exits, so the waiting is a sleep of ours between non-blocking writes.
inline bool AAudioWrite(void* stream, const void* data, uint32_t frames, uint32_t bytes_per_frame,
                        bool blocking) {
#if KYTY_ANDROID_HOST_REACHABLE
	const auto* api = GetAAudio();
	if (api == nullptr || stream == nullptr) {
		return false;
	}
	constexpr int64_t deadline_ns = 250'000'000;
	const auto        now_ns      = [] {
        timespec ts {};
        ::clock_gettime(CLOCK_MONOTONIC, &ts);
        return static_cast<int64_t>(ts.tv_sec) * 1'000'000'000 + ts.tv_nsec;
	};
	const auto start  = now_ns();
	const auto* bytes = static_cast<const uint8_t*>(data);
	uint32_t    done  = 0;
	while (done < frames) {
		const auto written = api->write(stream, bytes + static_cast<uint64_t>(done) * bytes_per_frame,
		                                static_cast<int32_t>(frames - done), 0);
		if (written < 0) {
			return false;
		}
		done += static_cast<uint32_t>(written);
		if (done >= frames || !blocking || now_ns() - start > deadline_ns) {
			break;
		}
		timespec pause {0, 1'000'000};
		::nanosleep(&pause, nullptr);
	}
	return true;
#else
	(void)stream;
	(void)data;
	(void)frames;
	(void)bytes_per_frame;
	(void)blocking;
	return false;
#endif
}

} // namespace Common::AndroidHost

#endif // EMULATOR_SRC_COMMON_ANDROIDHOST_H_
