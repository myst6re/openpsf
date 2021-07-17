#pragma once

#define OPENPSF_VERSION "1.4.0"

constexpr auto PSF_CHANNEL_COUNT = 2;
constexpr auto PSF_MAX_SAMPLE_COUNT = 1024;

#include <psflib.h>
#include <string>
#include <vector>
#include <cstdint>

//#include "psx_filter.h"
#include "circular_buffer.h"
#include "openpsf_c.h"
#include "source.h"

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

class EnvironmentPsx;

class Psf
{
private:
	PsfFlags _flags;
	const int end_silence_seconds;
	bool no_loop, eof;
	bool opening_silence_suppressed;
	int16_t opening_silence_buffer[PSF_MAX_SAMPLE_COUNT * PSF_CHANNEL_COUNT];
	int opening_silence_remaining_pos;
	size_t opening_silence_remaining_samples;
	CircularBuffer silence_test_buffer;
	//CPSXFilter filter;
	bool err;
	unsigned int data_written;
	unsigned int song_len, fade_len;
	uint16_t samplerate;
	char* last_error;
	std::string last_status;
	bool is_open;
	EnvironmentPsx* _psx;
	Source* _source;

	void calcfade() noexcept;
	unsigned int ms_to_samples(unsigned int ms) const noexcept;
	bool suppress_opening_silence() noexcept;
public:
	enum class FileType {
		Psf
	};
	DLLEXPORT static bool initialize_psx_core(const char* bios_path) noexcept;
	DLLEXPORT inline static bool psx_core_was_initialized() noexcept;
	DLLEXPORT Psf(PsfFlags flags = PsfDefaults, int end_silence_seconds = 5) noexcept;
	DLLEXPORT virtual ~Psf() noexcept;
	DLLEXPORT void close() noexcept;
	DLLEXPORT inline bool open(const char* path, bool infinite = false, int default_length = 170000, int default_fade = 10000) noexcept {
		return open(path, FileType::Psf, infinite, default_length, default_fade);
	}
	DLLEXPORT bool open(const char* path, FileType type, bool infinite = false, int default_length = 170000, int default_fade = 10000) noexcept;
	/*
	 * Decode sample_count samples, maximum 2048.
	 * Set data to nullptr to seek forward.
	 */
	DLLEXPORT int decode(int16_t* data, int sample_count) noexcept;
	DLLEXPORT bool rewind() noexcept;
	DLLEXPORT int get_sample_count() const noexcept;
	DLLEXPORT int get_sample_rate() const noexcept;
	DLLEXPORT int get_channel_count() const noexcept;
	DLLEXPORT int get_bits_per_seconds() const noexcept;
	DLLEXPORT const char* get_last_error() const noexcept;
	DLLEXPORT const char* get_last_status() const noexcept;
	DLLEXPORT static FileType type_from_extension(const char* extension, bool &ok) noexcept;
	DLLEXPORT static bool is_our_path(const char* path, const char* type) noexcept;
};
