#pragma once

#include <stdint.h>
#include <stddef.h>
#if !defined(__cplusplus)
#include <stdbool.h>
#endif

#if defined(__cplusplus)
extern "C" {
#endif

#if defined(_WIN32) || defined(__CYGWIN__)
#ifdef OPENPSF_EXPORTS
#define DLLEXPORT __declspec(dllexport)
#elif defined(OPENPSF_IMPORTS)
#define DLLEXPORT __declspec(dllimport)
#else
#define DLLEXPORT
#endif
#elif defined(OPENPSF_EXPORTS) && defined(__GNUC__)
#define DLLEXPORT __attribute__((visibility("default")))
#else
#define DLLEXPORT
#endif

	typedef void PSF;

	/* Flags for openpsf_create_with_flags (same values as PsfFlags) */
#define OPENPSF_NO_REVERB                  0x01
#define OPENPSF_NO_SIMULATE_FREQ_RESPONSE  0x02
#define OPENPSF_SUPPRESS_OPENING_SILENCE   0x04
#define OPENPSF_DEBUG                      0x08
#define OPENPSF_LEGACY_REVERB              0x20 /* original HE reverb formula */
#define OPENPSF_LEGACY_REVERB_UPSAMPLING   0x40 /* original HE reverb upsampling */

	DLLEXPORT bool openpsf_initialize_psx_core(const char* bios_path);
	DLLEXPORT bool openpsf_is_our_path(const char* p_full_path, const char* p_extension);
	DLLEXPORT PSF* openpsf_create();
	DLLEXPORT PSF* openpsf_create_with_params(bool reverb, bool simulate_frequency_response, bool suppress_opening_silence,
		int end_silence_seconds);
	DLLEXPORT PSF* openpsf_create_with_flags(int flags, int end_silence_seconds);
	DLLEXPORT void openpsf_destroy(PSF* self);
	DLLEXPORT bool openpsf_open(PSF* self, const char* p_path, bool infinite);
	DLLEXPORT void openpsf_close(PSF* self);
	DLLEXPORT const char *openpsf_get_tag(PSF* self, const char *tag_name);
	DLLEXPORT int openpsf_decode(PSF* self, int16_t* data, unsigned int sample_count);
	DLLEXPORT bool openpsf_rewind(PSF* self);
	DLLEXPORT int openpsf_get_sample_count(PSF* self);
	DLLEXPORT int openpsf_get_sample_rate(PSF* self);
	DLLEXPORT int openpsf_get_channel_count(PSF* self);
	DLLEXPORT int openpsf_get_bits_per_seconds(PSF* self);
	DLLEXPORT const char* openpsf_get_last_error(PSF* self);
	DLLEXPORT const char* openpsf_get_last_status(PSF* self);
#if defined(__cplusplus)
}
#endif
