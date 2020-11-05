#pragma once

#include "source.h"
#include "environment_psx.h"
#include <psflib.h>
#include <cstdint>
#include <string>

class SourcePsf : public Source {
private:
	uint8_t _psf_version;
	psf_file_callbacks _psf_file_system;
	bool _is_open;
	std::string _last_status;
	size_t _length_ms, _fade_ms;
	void* _psf2fs;
	EnvironmentPsx* _psx;
	char* _last_error;

	static unsigned long parse_time_crap(const char* input) noexcept;
	static int psf_info_meta(void* context, const char* name, const char* value) noexcept;
	static int psf_load_cb(void* context, const uint8_t* exe, size_t exe_size,
		const uint8_t* reserved, size_t reserved_size) noexcept;
	uint8_t open_version(const char* path) noexcept;
public:
	SourcePsf(EnvironmentPsx* psx) noexcept;
	virtual ~SourcePsf() noexcept;
	bool open(const char* path) noexcept override;
	bool close() noexcept override;
	inline bool can_decode() const noexcept override {
		return _psx->can_execute();
	}
	int32_t decode(int16_t* buffer, uint16_t max_samples) noexcept override;
	inline const char* last_error() const noexcept override {
		return _last_error == nullptr ? _psx->last_error() : _last_error;
	}
	inline const char* last_status() const noexcept override {
		return _last_status.c_str();
	}
	inline size_t song_length_ms() const noexcept override {
		return _length_ms;
	}
	inline size_t song_fade_ms() const noexcept override {
		return _fade_ms;
	}
	inline uint16_t sample_rate() const noexcept override {
		return _psf_version == 2 ? 48000 : 44100;
	}
};
