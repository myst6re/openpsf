
#include "environment_psx.h"

#include "../highly_experimental/Core/iop.h"
#include "../highly_experimental/Core/r3000.h"
#include "../highly_experimental/Core/spu.h"
#include "../highly_experimental/Core/bios.h"

#include <psf2fs.h>

struct exec_header_t {
	uint32_t pc0;
	uint32_t gp0;
	uint32_t t_addr;
	uint32_t t_size;
	uint32_t d_addr;
	uint32_t d_size;
	uint32_t b_addr;
	uint32_t b_size;
	uint32_t s_ptr;
	uint32_t s_size;
	uint32_t sp, fp, gp, ret, base;
};

struct psxexe_hdr_t {
	char key[8];
	uint32_t text;
	uint32_t data;
	exec_header_t exec;
	char title[60];
};

#if defined(_M_IX86) || defined(_M_X64) || defined(_M_ARM)
constexpr auto byte_order_is_big_endian = false;
#else
constexpr auto byte_order_is_big_endian = true;
#endif

uint32_t byteswap_if_be_t(uint32_t p_param) noexcept {
	return byte_order_is_big_endian ? _byteswap_ulong(p_param) : p_param;
}

bool EnvironmentPsx::_psx_core_was_initialized = false;
uint8_t EnvironmentPsx::_bios[HEBIOS_SIZE] = {};

bool EnvironmentPsx::initialize(const char* bios_path) noexcept
{
	FILE* handle = nullptr;
	if (fopen_s(&handle, bios_path, "rb") != 0 || handle == nullptr) {
		return false;
	}

	if (fread(&_bios[0], sizeof(uint8_t), HEBIOS_SIZE, handle) != sizeof(uint8_t) * HEBIOS_SIZE) {
		return false;
	}

	bios_set_image(&_bios[0], HEBIOS_SIZE);

	if (psx_init() != 0) {
		return false;
	}

	_psx_core_was_initialized = true;

	return true;
}

EnvironmentPsx::EnvironmentPsx(bool debug, bool reverb) noexcept :
	_compat(debug ? IOP_COMPAT_HARSH : IOP_COMPAT_FRIENDLY), _reverb(reverb),
	_version(0), _psx_state_size(0), _psx_state(nullptr),
	_last_error(""), _eof(true)
{
}

EnvironmentPsx::~EnvironmentPsx()
{
	if (_psx_state != nullptr) {
		delete[] _psx_state;
	}
}

void EnvironmentPsx::reset(uint8_t version) noexcept
{
	_psx_state_size = psx_get_state_size(version);

	if (_psx_state != nullptr && version != _version) {
		delete[] _psx_state;
		_psx_state = nullptr;
	}
	if (_psx_state == nullptr) {
		_psx_state = new char[_psx_state_size];
	}

	psx_clear_state(_psx_state, version);
	void* pIOP = psx_get_iop_state(_psx_state);
	iop_set_compat(pIOP, _compat);
	spu_enable_reverb(iop_get_spu_state(pIOP), _reverb);

	_version = version;
}

bool EnvironmentPsx::ps1_upload_memory(const uint8_t* exe, size_t exe_size) noexcept
{
	if (exe_size < 0x800) {
		_last_error = "Exe too short";
		return false;
	}

	const psxexe_hdr_t* psx = reinterpret_cast<const psxexe_hdr_t*>(exe);
	uint32_t addr = byteswap_if_be_t(psx->exec.t_addr);
	const uint32_t size = exe_size - 0x800;

	addr &= 0x1fffff;
	if (addr < 0x10000 || size > 0x1f0000 || addr + size > 0x200000) {
		_last_error = "Invalid exe";
		return false;
	}

	void* pIOP = psx_get_iop_state(_psx_state);
	iop_upload_to_ram(pIOP, addr, exe + 0x800, size);

	return true;
}

bool EnvironmentPsx::ps1_upload_psexe(const uint8_t* exe, size_t exe_size) noexcept
{
	_last_error = "";
	return psx_upload_psxexe(_psx_state, const_cast<uint8_t*>(exe), exe_size) >= 0;
}

void EnvironmentPsx::ps2_set_readfile(psx_readfile_t callback, void* context) noexcept
{
	psx_set_readfile(_psx_state, callback, context);
}

int32_t EnvironmentPsx::execute(int16_t* buffer, uint16_t max_samples) noexcept
{
	if (_eof || max_samples == 0) {
		return 0;
	}

	// Emulator limitation
	if (max_samples > 2048) {
		max_samples = 2048;
	}

	uint32_t samples = max_samples;
	const int err = psx_execute(_psx_state, 0x7FFFFFFF, buffer, &samples, 0);
	if (err <= -2) {
		_last_error = "PSX unrecoverable error";
		return -1;
	}
	if (-1 == err || 0 == samples) {
		_eof = true;
	}

	return static_cast<int32_t>(samples);
}
