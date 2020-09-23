#pragma once

#define OPENPSF_VERSION "0.1.0"

#define IOP_COMPAT_DEFAULT  (0)
#define IOP_COMPAT_HARSH    (1)
#define IOP_COMPAT_FRIENDLY (2)

#include <psflib.h>
#include <string>
#include <vector>
#include <windows.h>
#include <stdint.h>

#include "PSXFilter.h"
#include "circular_buffer.h"
#include "openpsf_c.h"

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
	bool suppressEndSilence, suppressOpeningSilence;
	int endSilenceSeconds;
	bool no_loop, eof;
	bool openingSilenceSuppressed;
	void *psx_state;
	std::vector<int16_t> sample_buffer;
	circular_buffer<int16_t> silence_test_buffer;
	void* psf2fs;
	CPSXFilter filter;
	int err;
	uint8_t psf_version;
	int data_written, pos_delta;
	long long psfemu_pos_ms;
	int song_len, fade_len;
	uint16_t samplerate;
	char* last_error;
	std::string last_status;
	psf_info_meta_state info_state;
	psf_file_callbacks psf_file_system;
	bool is_open;
	static bool psx_core_was_initialized;
public:
	DLLEXPORT static bool initialize_psx_core(const char* bios_path) noexcept;
	DLLEXPORT Psf(uint8_t compat = IOP_COMPAT_FRIENDLY, bool reverb = true,
		bool do_filter = true, bool suppressEndSilence = true, bool suppressOpeningSilence = true,
		int endSilenceSeconds = 5) noexcept;
	DLLEXPORT ~Psf();
	DLLEXPORT void close() noexcept;
	DLLEXPORT bool open(const char* p_path, bool infinite = false, int default_length = 170000, int default_fade = 10000);
	DLLEXPORT size_t decode(int16_t* data, unsigned int sample_count);
	DLLEXPORT bool seek(unsigned int p_ms);
	DLLEXPORT int get_length() const noexcept;
	DLLEXPORT int get_sample_rate() const noexcept;
	DLLEXPORT int get_channel_count() const noexcept;
	DLLEXPORT int get_bits_per_seconds() const noexcept;
	DLLEXPORT const char* get_last_error() const noexcept;
	DLLEXPORT const char* get_last_status() const noexcept;
	DLLEXPORT static bool is_our_path(const char* p_full_path, const char* p_extension) noexcept;

private:
	uint8_t open_version(const char* path) noexcept;
	void calcfade() noexcept;
};
