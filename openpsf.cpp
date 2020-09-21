#include "openpsf.h"
#include "psf.h"

Psf* instance(_psf* self) {
	return (Psf*)self;
}

bool initialize_psx_core(const char* bios_path)
{
	return Psf::initialize_psx_core(bios_path);
}

bool is_our_path(const char* p_full_path, const char* p_extension)
{
	return Psf::is_our_path(p_full_path, p_extension);
}

_psf* create_with_params(bool reverb, bool do_filter, bool suppressEndSilence, bool suppressOpeningSilence,
	int endSilenceSeconds)
{
	return new Psf(IOP_COMPAT_FRIENDLY, reverb, do_filter, suppressEndSilence,
		suppressOpeningSilence, endSilenceSeconds);
}

_psf* create()
{
	return create_with_params(true, true, true, true, 5);
}

void destroy(_psf* self)
{
	if (self != nullptr) {
		delete instance(self);
	}
}

bool open(_psf* self, const char* p_path, bool infinite)
{
	if (self == nullptr) {
		return false;
	}
	return instance(self)->open(p_path, infinite);
}

void close(_psf* self)
{
	if (self == nullptr) {
		return;
	}
	return instance(self)->close();
}

size_t decode(_psf* self, int16_t* data, unsigned int sample_count)
{
	if (self == nullptr) {
		return 0;
	}
	return instance(self)->decode(data, sample_count);
}

bool seek(_psf* self, unsigned int ms)
{
	if (self == nullptr) {
		return false;
	}
	return instance(self)->seek(ms);
}

int get_length(_psf* self)
{
	if (self == nullptr) {
		return 0;
	}
	return instance(self)->get_length();
}

int get_sample_rate(_psf* self)
{
	if (self == nullptr) {
		return 0;
	}
	return instance(self)->get_sample_rate();
}

int get_channel_count(_psf* self)
{
	if (self == nullptr) {
		return 0;
	}
	return instance(self)->get_channel_count();
}

int get_bits_per_seconds(_psf* self)
{
	if (self == nullptr) {
		return 0;
	}
	return instance(self)->get_bits_per_seconds();
}

const char* get_last_error(_psf* self)
{
	if (self == nullptr) {
		return nullptr;
	}
	return instance(self)->get_last_error();
}

const char* get_last_status(_psf* self)
{
	if (self == nullptr) {
		return nullptr;
	}
	return instance(self)->get_last_status();
}

openpsf _openpsf = {
	initialize_psx_core,
	is_our_path,
	create,
	create_with_params,
	destroy,
	open,
	close,
	decode,
	seek,
	get_length,
	get_sample_rate,
	get_channel_count,
	get_bits_per_seconds,
	get_last_error,
	get_last_status
};

openpsf * get_openpsf()
{
	return &_openpsf;
}
