#pragma once

#include <string>
#include <map>

#include "PSXFilter.h"
#include "circular_buffer.h"
#include "openpsf_c.h"

constexpr auto HEBIOS_SIZE = 524288;

struct psf_file_callbacks;

struct psf_info_meta_state
{
	int tag_song_ms;
	int tag_fade_ms;
	int refresh;
	bool looping;
	std::map<std::string, std::string> tags;
};

enum PsfFlags {
	PsfDefaults = 0x0,
	NoReverb = 0x1,
	NoSimulateFrequencyResponse = 0x2,
	SuppressOpeningSilence = 0x4,
	Debug = 0x8,
	// Reverb accuracy fixes are enabled by default (psx-spx formula and
	// 39-tap FIR upsampling). These flags bring back the original Highly
	// Experimental behaviour. 0x10 is left free for SimulateFrequencyResponse.
	LegacyReverb = 0x20,
	LegacyReverbUpsampling = 0x40
};

static inline PsfFlags operator|(PsfFlags flags, PsfFlags other) noexcept {
	return static_cast<PsfFlags>(static_cast<int>(flags) | static_cast<int>(other));
}

class Psf
{
private:
	uint8_t compat;
	bool reverb, simulate_frequency_response;
	uint32_t spu_fixes;
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
	const char* last_error;
	std::string last_status;
	psf_info_meta_state info_state;
	psf_file_callbacks *psf_file_system;
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
	DLLEXPORT const char *get_tag(const char* tag_name);
	/*
	 * Decode sample_count samples, maximum 1024.
	 * Set data to nullptr to seek forward.
	 */
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
