#pragma once

#include "environment_psx.h"
#include <psflib.h>
#include <cstdint>
#include <string>

struct PsfInfoMetaState
{
	int tag_song_ms;
	int tag_fade_ms;
	bool looping;
};

struct PsfLoadState {
	EnvironmentPsx* psx;
	int version;
	bool first;
	void* psf2fs;
};

class PsfFile {
private:
	uint8_t _psf_version;
	psf_file_callbacks _psf_file_system;
	bool _is_open;
	std::string _last_status;
	size_t _length_ms, _fade_ms;
	char* _last_error;

	static unsigned long parse_time_crap(const char* input) noexcept;
	static int psf_info_meta(void* context, const char* name, const char* value) noexcept;
	uint8_t open_version(const char* path) noexcept;
	static int psf_load_cb(void* context, const uint8_t* exe, size_t exe_size,
		const uint8_t* reserved, size_t reserved_size) noexcept;
protected:
	virtual int psf_load_callback(const uint8_t* exe, size_t exe_size,
		const uint8_t* reserved, size_t reserved_size) noexcept = 0;
public:
	PsfFile() noexcept;
	inline virtual ~PsfFile() noexcept {}
	virtual bool open(const char* path) noexcept;
	bool close() noexcept;
	inline bool isOpen() const noexcept {
		return _is_open;
	}
	inline const char* last_error() const noexcept {
		return _last_error;
	}
	inline const char* last_status() const noexcept {
		return _last_status.c_str();
	}
	inline size_t song_length_ms() const noexcept {
		return _length_ms;
	}
	inline size_t song_fade_ms() const noexcept {
		return _fade_ms;
	}
	inline uint8_t version() const noexcept {
		return _psf_version;
	}
	inline uint16_t sample_rate() const noexcept {
		return _psf_version == 2 ? 48000 : 44100;
	}
};

class PsfFilePsExe : public PsfFile {
private:
	void* _psf2fs;
	EnvironmentPsx* _psx;
	PsfLoadState _state;
	bool _first;

	int psf_load_callback(const uint8_t* exe, size_t exe_size,
		const uint8_t* reserved, size_t reserved_size) noexcept override;
public:
	explicit PsfFilePsExe(EnvironmentPsx* _psx) noexcept;
	virtual ~PsfFilePsExe() noexcept;
	bool open(const char* path) noexcept override;
	inline EnvironmentPsx* psx() const noexcept {
		return _psx;
	}
	inline void setPsx(EnvironmentPsx* psx) noexcept {
		_psx = psx;
	}
};
