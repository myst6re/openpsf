#pragma once

#include "environment.h"
#include <cstdint>
#include "../highly_experimental/Core/psx.h"

constexpr auto HEBIOS_SIZE = 524288;

class EnvironmentPsx : public Environment {
private:
	const uint8_t _compat;
	const bool _reverb;
	uint8_t _version;
	size_t _psx_state_size;
	char* _psx_state;
	char* _last_error;
	bool _eof;

	static bool _psx_core_was_initialized;
	static uint8_t _bios[HEBIOS_SIZE];
public:
	static bool initialize(const char* bios_path) noexcept;
	inline static bool was_initialized() noexcept {
		return _psx_core_was_initialized;
	}
	EnvironmentPsx(bool reverb = true, bool debug = false) noexcept;
	virtual ~EnvironmentPsx();
	void reset(uint8_t version) noexcept;
	void reset() noexcept override {
		reset(_version);
	}
	bool ps1_upload_memory(const uint8_t* exe, size_t exe_size) noexcept;
	bool ps1_upload_psexe(const uint8_t* exe, size_t exe_size) noexcept;
	void ps2_set_readfile(psx_readfile_t callback, void* context) noexcept;
	inline bool can_execute() const noexcept override {
		return !_eof;
	}
	int execute(int16_t* buffer, uint16_t max_samples) noexcept override;
	inline const char* last_error() const noexcept override {
		return _last_error;
	}
};
