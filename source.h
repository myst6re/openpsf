#pragma once

#include <cstdint>

class Source {
public:
	Source() noexcept {}
	virtual bool open(const char* path) noexcept = 0;
	virtual bool close() noexcept = 0;
	virtual bool can_decode() const noexcept = 0;
	virtual int decode(int16_t* buffer, uint16_t max_samples) noexcept = 0;
	virtual const char* last_error() const noexcept = 0;
	virtual const char* last_status() const noexcept = 0;
	virtual size_t song_length_ms() const noexcept = 0;
	virtual size_t song_fade_ms() const noexcept = 0;
	virtual uint16_t sample_rate() const noexcept = 0;
};
