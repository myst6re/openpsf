#pragma once

#include <stdint.h>

#ifdef OPENPSF_EXPORTS
#define DLLEXPORT __declspec(dllexport)
#elif defined(OPENPSF_IMPORTS)
#define DLLEXPORT __declspec(dllimport)
#else
#define DLLEXPORT
#endif

#if defined(__cplusplus)
extern "C" {
#endif
	typedef void PSF;

	DLLEXPORT bool openpsf_initialize_psx_core(const char* bios_path);
	DLLEXPORT bool openpsf_is_our_path(const char* p_full_path, const char* p_extension);
	DLLEXPORT PSF* openpsf_create();
	DLLEXPORT PSF* openpsf_create_with_params(bool reverb, bool do_filter, bool suppressEndSilence, bool suppressOpeningSilence,
		int endSilenceSeconds);
	DLLEXPORT void openpsf_destroy(PSF* self);
	DLLEXPORT bool openpsf_open(PSF* self, const char* p_path, bool infinite);
	DLLEXPORT void openpsf_close(PSF* self);
	DLLEXPORT size_t openpsf_decode(PSF* self, int16_t* data, unsigned int sample_count);
	DLLEXPORT bool openpsf_rewind(PSF* self);
	DLLEXPORT int openpsf_get_length(PSF* self);
	DLLEXPORT int openpsf_get_sample_rate(PSF* self);
	DLLEXPORT int openpsf_get_channel_count(PSF* self);
	DLLEXPORT int openpsf_get_bits_per_seconds(PSF* self);
	DLLEXPORT const char* openpsf_get_last_error(PSF* self);
	DLLEXPORT const char* openpsf_get_last_status(PSF* self);
#if defined(__cplusplus)
}
#endif
