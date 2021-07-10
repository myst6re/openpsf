#include "openpsf/openpsf_c.h"
#include "openpsf/openpsf.h"

Psf* instance(PSF* self) noexcept {
	return static_cast<Psf*>(self);
}

bool openpsf_initialize_psx_core(const char* bios_path)
{
	return Psf::initialize_psx_core(bios_path);
}

bool openpsf_is_our_path(const char* p_full_path, const char* p_extension)
{
	return Psf::is_our_path(p_full_path, p_extension);
}

PSF* openpsf_create_with_params(bool reverb, bool simulate_frequency_response, bool suppress_opening_silence,
	int end_silence_seconds)
{
	PsfFlags flags = PsfDefaults;
	if (reverb == false) {
		flags = flags | NoReverb;
	}
	if (simulate_frequency_response == false) {
		flags = flags | NoSimulateFrequencyResponse;
	}
	if (suppress_opening_silence == true) {
		flags = flags | SuppressOpeningSilence;
	}

	return new Psf(flags, end_silence_seconds);
}

PSF* openpsf_create()
{
	return new Psf();
}

void openpsf_destroy(PSF* self)
{
	if (self != nullptr) {
		delete instance(self);
	}
}

bool openpsf_open(PSF* self, const char* p_path, bool infinite)
{
	if (self == nullptr) {
		return false;
	}
	return instance(self)->open(p_path, infinite);
}

void openpsf_close(PSF* self)
{
	if (self == nullptr) {
		return;
	}
	return instance(self)->close();
}

size_t openpsf_decode(PSF* self, int16_t* data, unsigned int sample_count)
{
	if (self == nullptr) {
		return 0;
	}
	return instance(self)->decode(data, sample_count);
}

bool openpsf_rewind(PSF* self)
{
	if (self == nullptr) {
		return false;
	}
	return instance(self)->rewind();
}

int openpsf_get_sample_count(PSF* self)
{
	if (self == nullptr) {
		return 0;
	}
	return instance(self)->get_sample_count();
}

int openpsf_get_sample_rate(PSF* self)
{
	if (self == nullptr) {
		return 0;
	}
	return instance(self)->get_sample_rate();
}

int openpsf_get_channel_count(PSF* self)
{
	if (self == nullptr) {
		return 0;
	}
	return instance(self)->get_channel_count();
}

int openpsf_get_bits_per_seconds(PSF* self)
{
	if (self == nullptr) {
		return 0;
	}
	return instance(self)->get_bits_per_seconds();
}

const char* openpsf_get_last_error(PSF* self)
{
	if (self == nullptr) {
		return nullptr;
	}
	return instance(self)->get_last_error();
}

const char* openpsf_get_last_status(PSF* self)
{
	if (self == nullptr) {
		return nullptr;
	}
	return instance(self)->get_last_status();
}
