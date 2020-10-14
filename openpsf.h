#pragma once

#define OPENPSF_VERSION "1.3.0"

#include <psflib.h>
#include <string>
#include <vector>
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

enum PsfFlags {
	PsfDefaults = 0x0,
	NoReverb = 0x1,
	NoSimulateFrequencyResponse = 0x2,
	SuppressOpeningSilence = 0x4,
	Debug = 0x8
};

PsfFlags operator|(PsfFlags flags, PsfFlags other) noexcept {
	return static_cast<PsfFlags>(static_cast<int>(flags) | static_cast<int>(other));
}

class Psf
{
private:
	uint8_t compat;
	bool reverb, simulate_frequency_response;
	const bool suppress_opening_silence;
	const int end_silence_seconds;
	bool no_loop, eof;
	bool opening_silence_suppressed;
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
	DLLEXPORT Psf(PsfFlags flags = PsfDefaults, int end_silence_seconds = 5);
	DLLEXPORT virtual ~Psf();
	DLLEXPORT void close() noexcept;
	DLLEXPORT bool open(const char* p_path, bool infinite = false, int default_length = 170000, int default_fade = 10000);
	DLLEXPORT int decode(int16_t* data, int sample_count);
	DLLEXPORT bool rewind() noexcept;
	DLLEXPORT int get_sample_count() const noexcept;
	DLLEXPORT int get_sample_rate() const noexcept;
	DLLEXPORT int get_channel_count() const noexcept;
	DLLEXPORT int get_bits_per_seconds() const noexcept;
	DLLEXPORT const char* get_last_error() const noexcept;
	DLLEXPORT const char* get_last_status() const noexcept;
	DLLEXPORT static bool is_our_path(const char* p_full_path, const char* p_extension) noexcept;
};
