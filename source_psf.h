#pragma once

#include "source.h"
#include "environment_psx.h"
#include "psf_file.h"
#include <psflib.h>
#include <cstdint>
#include <string>

class SourcePsf : public Source {
private:
	PsfFilePsExe _psf_file;
	std::string _last_status;
	char* _last_error;
public:
	SourcePsf(EnvironmentPsx* psx) noexcept;
	inline virtual ~SourcePsf() noexcept {};
	inline bool open(const char* path) noexcept override {
		return _psf_file.open(path);
	}
	bool close() noexcept override {
		return _psf_file.close();
	}
	inline bool can_decode() const noexcept override {
		return _psf_file.psx()->can_execute();
	}
	int32_t decode(int16_t* buffer, uint16_t max_samples) noexcept override;
	inline const char* last_error() const noexcept override {
		return _last_error == nullptr ? _psf_file.psx()->last_error() : _last_error;
	}
	inline const char* last_status() const noexcept override {
		return _last_status.c_str();
	}
	inline size_t song_length_ms() const noexcept override {
		return _psf_file.song_length_ms();
	}
	inline size_t song_fade_ms() const noexcept override {
		return _psf_file.song_fade_ms();
	}
	inline uint16_t sample_rate() const noexcept override {
		return _psf_file.sample_rate();
	}
};
