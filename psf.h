#pragma once

#define MYVERSION "2.2.5"

#if 0
#include "../psfemucore/psx.h"
#include "../psfemucore/iop.h"
#include "../psfemucore/r3000.h"
#include "../psfemucore/spu.h"
#else
#include "../ESP/PSX/Core/psx.h"
#include "../ESP/PSX/Core/iop.h"
#include "../ESP/PSX/Core/r3000.h"
#include "../ESP/PSX/Core/spu.h"
#include "../ESP/PSX/Core/bios.h"
#endif

#include <psflib.h>
#include <psf2fs.h>
#include <string>
#include <vector>
#include <windows.h>
#include <stdint.h>

#include "PSXFilter.h"

#include "circular_buffer.h"

//#define DBG(a) OutputDebugString(a)
#define DBG(a)

typedef struct {
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
} exec_header_t;

typedef struct {
	char key[8];
	uint32_t text;
	uint32_t data;
	exec_header_t exec;
	char title[60];
} psxexe_hdr_t;

struct psf_info_meta_state
{
	int tag_song_ms;
	int tag_fade_ms;
	char _lib[256];
	int refresh;
	int looping;

	psf_info_meta_state()
		: tag_song_ms(0), tag_fade_ms(0), refresh(0), looping(0)
	{
	}
};

struct psf_load_state
{
	int version;
	void* emu;
	bool first;
	psf_info_meta_state* meta_state;
	void* psf2fs;
};

class Psf
{
private:
	uint8_t compat;
	bool reverb, do_filter;
	bool suppressEndSilence, suppressOpeningSilence;
	int endSilenceSeconds;
	bool no_loop, eof;
	bool openingSilenceSuppressed;
	void *psx_state;
	std::vector<int16_t> sample_buffer;
	circular_buffer<int16_t> silence_test_buffer;
	void* psf2fs;
	CPSXFilter filter;
	int err;
	uint8_t psf_version;
	int data_written, pos_delta;
	long long psfemu_pos_ms;
	int song_len, fade_len;
	uint16 samplerate;
	char* last_error;
	std::string last_status;
	psf_info_meta_state info_state;
	psf_file_callbacks psf_file_system;
	bool is_open;
	static bool psx_core_was_initialized;
public:
	static bool initialize_psx_core(const char* bios_path);
	Psf(uint8_t compat = IOP_COMPAT_FRIENDLY, bool reverb = true,
		bool do_filter = true, bool suppressEndSilence = true, bool suppressOpeningSilence = true,
		int endSilenceSeconds = 5);
	~Psf();
	void close();
	bool open(const char* p_path, bool infinite = false, int default_length = 170000, int default_fade = 10000);
	size_t decode(int16_t* data, unsigned int sample_count);
	bool seek(unsigned int p_ms);
	int get_length() const;
	int get_sample_rate() const;
	int get_channel_count() const;
	int get_bits_per_seconds() const;
	const char* get_last_error() const;
	const char* get_last_status() const;
	static bool is_our_path(const char* p_full_path, const char* p_extension);

private:
	uint8_t open_version(const char* path);
	void calcfade();
};
