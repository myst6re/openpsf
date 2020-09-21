#pragma once

#include <stdint.h>

#ifdef OPENPSF_EXPORTS
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT __declspec(dllimport)
#endif

#if defined(__cplusplus)
extern "C" {
#endif
	typedef void _psf;

	struct openpsf {
		bool (*initialize_psx_core)(const char* bios_path);
		bool (*is_our_path)(const char* p_full_path, const char* p_extension);
		_psf* (*create)();
		_psf* (*create_with_params)(bool reverb, bool do_filter, bool suppressEndSilence, bool suppressOpeningSilence,
			int endSilenceSeconds);
		void (*destroy)(_psf* self);
		bool (*open)(_psf* self, const char* p_path, bool infinite);
		void (*close)(_psf* self);
		size_t (*decode)(_psf* self, int16_t* data, unsigned int sample_count);
		bool (*seek)(_psf* self, unsigned int ms);
		int (*get_length)(_psf* self);
		int (*get_sample_rate)(_psf* self);
		int (*get_channel_count)(_psf* self);
		int (*get_bits_per_seconds)(_psf* self);
		const char* (*get_last_error)(_psf* self);
		const char* (*get_last_status)(_psf* self);
	};

	DLLEXPORT openpsf* get_openpsf();
#if defined(__cplusplus)
}
#endif
