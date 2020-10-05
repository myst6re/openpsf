#pragma once

#define OPENPSF_VERSION "1.2.0"

#define IOP_COMPAT_DEFAULT  (0)
#define IOP_COMPAT_HARSH    (1)
#define IOP_COMPAT_FRIENDLY (2)

#include <psflib.h>
#include <string>
#include <vector>
#include <windows.h>
#include <cstdint>

#include "PSXFilter.h"
#include "circular_buffer.h"
#include "openpsf_c.h"

constexpr auto HEBIOS_SIZE = 524288;

struct psf_info_meta_state
{
	int tag_song_ms;
	int tag_fade_ms;
	int refresh;
	bool looping;
};

class Psf
{
private:
	uint8_t compat;
	bool reverb, do_filter;
	const bool suppressEndSilence, suppressOpeningSilence;
	const int endSilenceSeconds;
	bool no_loop, eof;
	bool openingSilenceSuppressed;
	size_t psx_state_size;
	char* psx_state, *psx_initial_state;
	int16_t* sample_buffer;
	CircularBuffer silence_test_buffer;
	void* psf2fs;
	CPSXFilter filter;
	int err;
	uint8_t psf_version;
	unsigned int data_written;
	unsigned int song_len, fade_len;
	uint16_t samplerate;
	char* last_error;
	std::string last_status;
	psf_info_meta_state info_state;
	psf_file_callbacks psf_file_system;
	bool is_open;

	static bool psx_core_was_initialized;
	static uint8_t bios[HEBIOS_SIZE];

	uint8_t open_version(const char* path) noexcept;
	void calcfade() noexcept;
	unsigned int ms_to_samples(unsigned int ms) const noexcept;
public:
	DLLEXPORT static bool initialize_psx_core(const char* bios_path) noexcept;
	DLLEXPORT Psf(uint8_t compat = IOP_COMPAT_FRIENDLY, bool reverb = true,
		bool do_filter = true, bool suppressEndSilence = true, bool suppressOpeningSilence = true,
		int endSilenceSeconds = 5);
	DLLEXPORT virtual ~Psf();
	DLLEXPORT void close() noexcept;
	DLLEXPORT bool open(const char* p_path, bool infinite = false, int default_length = 170000, int default_fade = 10000);
	DLLEXPORT int decode(int16_t* data, int sample_count);
	DLLEXPORT bool rewind() noexcept;
	DLLEXPORT int get_length() const noexcept;
	DLLEXPORT int get_sample_rate() const noexcept;
	DLLEXPORT int get_channel_count() const noexcept;
	DLLEXPORT int get_bits_per_seconds() const noexcept;
	DLLEXPORT const char* get_last_error() const noexcept;
	DLLEXPORT const char* get_last_status() const noexcept;
	DLLEXPORT static bool is_our_path(const char* p_full_path, const char* p_extension) noexcept;
};
