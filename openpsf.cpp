/****************************************************************************/
//    Original author: kode54                                               //
/****************************************************************************/
#include "openpsf.h"

#define BORK_TIME 0xC0CAC01A

#include "../highly_experimental/Core/psx.h"
#include "../highly_experimental/Core/iop.h"
#include "../highly_experimental/Core/r3000.h"
#include "../highly_experimental/Core/spu.h"
#include "../highly_experimental/Core/bios.h"

#include <psf2fs.h>

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

struct psf_load_state
{
	int version;
	void* emu;
	bool first;
	psf_info_meta_state* meta_state;
	void* psf2fs;
};

static unsigned long parse_time_crap(const char *input) noexcept
{
	unsigned long value = 0;
	unsigned long multiplier = 1000;
	const char* ptr = input;
	unsigned long colon_count = 0;

	while (*ptr && ((*ptr >= '0' && *ptr <= '9') || *ptr == ':')) {
		colon_count += *ptr == ':';
		++ptr;
	}
	if (colon_count > 2) {
		return BORK_TIME;
	}
	if (*ptr && *ptr != '.' && *ptr != ',') {
		return BORK_TIME;
	}
	if (*ptr) {
		++ptr;
	}
	while (*ptr && *ptr >= '0' && *ptr <= '9') {
		++ptr;
	}
	if (*ptr) {
		return BORK_TIME;
	}

	ptr = strrchr(input, ':');
	if (!ptr) {
		ptr = input;
	}

	for (;;) {
		char * end;
		if (ptr != input) {
			++ptr;
		}
		if (multiplier == 1000) {
			const double temp = strtof(ptr, &end);
			if (temp >= 60.0) {
				return BORK_TIME;
			}
			value = long(temp * 1000.0);
		}
		else {
			const unsigned long temp = strtoul(ptr, &end, 10);
			if (temp >= 60 && multiplier < 3600000) {
				return BORK_TIME;
			}
			value += temp * multiplier;
		}
		if (ptr == input) {
			break;
		}
		ptr -= 2;
		while (ptr > input && *ptr != ':') {
			--ptr;
		}
		multiplier *= 60;
	}

	return value;
}

static int psf_info_meta(void* context, const char* name, const char* value) noexcept
{
	if (name == nullptr) {
		return -1;
	}

	psf_info_meta_state* state = static_cast<psf_info_meta_state*>(context);

	if (_stricmp(name, "length") == 0)
	{
		const int temp = parse_time_crap(value);
		if (temp != BORK_TIME)
		{
			state->tag_song_ms = temp;
		}
	}
	else if (_stricmp(name, "fade") == 0)
	{
		const int temp = parse_time_crap(value);
		if (temp != BORK_TIME)
		{
			state->tag_fade_ms = temp;
		}
	}
	else if (_stricmp(name, "_lib") == 0)
	{
	}
	else if (_stricmp(name, "_refresh") == 0)
	{
		state->refresh = atoi(value);
	}
	else if (_stricmp(name, "looping") == 0)
	{
		state->looping = atoi(value);
	}
	else if (*name == '_')
	{
		return -1;
	}

	return 0;
}

#if defined(_M_IX86) || defined(_M_X64) || defined(_M_ARM)
#define PFC_BYTE_ORDER_IS_BIG_ENDIAN 0
#else
#define PFC_BYTE_ORDER_IS_BIG_ENDIAN 1
#endif

static const bool byte_order_is_big_endian = !!PFC_BYTE_ORDER_IS_BIG_ENDIAN;

uint32_t byteswap_if_be_t(uint32_t p_param) noexcept {
	return byte_order_is_big_endian ? _byteswap_ulong(p_param) : p_param;
}

int psf1_load_callback(psf_load_state* state, const uint8_t* exe, size_t exe_size,
	const uint8_t* reserved, size_t reserved_size) noexcept
{
	const psxexe_hdr_t* psx = reinterpret_cast<const psxexe_hdr_t*>(exe);

	if (exe_size < 0x800) {
		return -1;
	}

	uint32_t addr = byteswap_if_be_t(psx->exec.t_addr);
	const uint32_t size = exe_size - 0x800;

	addr &= 0x1fffff;
	if (addr < 0x10000 || size > 0x1f0000 || addr + size > 0x200000) {
		return -1;
	}

	void* pIOP = psx_get_iop_state(state->emu);
	iop_upload_to_ram(pIOP, addr, exe + 0x800, size);

	if (!state->meta_state->refresh)
	{
		if (!_strnicmp((const char*)exe + 113, "Europe", 6)) {
			state->meta_state->refresh = 50;
		}
		else {
			state->meta_state->refresh = 60;
		}
	}

	if (state->first)
	{
		void* pR3000 = iop_get_r3000_state(pIOP);
		r3000_setreg(pR3000, R3000_REG_PC, byteswap_if_be_t(psx->exec.pc0));
		r3000_setreg(pR3000, R3000_REG_GEN + 29, byteswap_if_be_t(psx->exec.s_ptr));
		state->first = false;
	}

	return 0;
}

int psf_load_cb(void* context, const uint8_t* exe, size_t exe_size,
	const uint8_t* reserved, size_t reserved_size) noexcept
{
	psf_load_state* state = static_cast<psf_load_state*>(context);

	if (state->version == 1) {
		return psf1_load_callback(state, exe, exe_size, reserved, reserved_size);
	}
	return psf2fs_load_callback(state->psf2fs, exe, exe_size, reserved, reserved_size);
}

static int EMU_CALL virtual_readfile(void *context, const char *path, int offset, char *buffer, int length) noexcept
{
	return psf2fs_virtual_readfile(context, path, offset, buffer, length);
}

static void * psf_file_fopen(void * context, const char* uri) noexcept
{
	FILE* handle = nullptr;
	if (fopen_s(&handle, uri, "rb") != 0) {
		return nullptr;
	}
	return handle;
}

static size_t psf_file_fread(void* buffer, size_t size, size_t count, void* handle) noexcept
{
	return fread(buffer, size, count, static_cast<FILE*>(handle));
}

static int psf_file_fseek(void* handle, int64_t offset, int whence) noexcept
{
	return fseek(static_cast<FILE*>(handle), long(offset), whence);
}

static int psf_file_fclose(void* handle) noexcept
{
	return fclose(static_cast<FILE*>(handle));
}

static long psf_file_ftell(void* handle) noexcept
{
	return ftell(static_cast<FILE*>(handle));
}

static void print_message(void* context, const char* message)
{
	if (context != nullptr) {
		std::string* string = static_cast<std::string*>(context);
		string->append(message);
	}
}

bool Psf::psx_core_was_initialized = false;

constexpr auto HEBIOS_SIZE = 524288;

bool Psf::initialize_psx_core(const char* bios_path) noexcept
{
	FILE* handle = nullptr;
	if (fopen_s(&handle, bios_path, "rb") != 0 || handle == nullptr) {
		return false;
	}

	uint8_t bios[HEBIOS_SIZE];

	if (fread(&bios[0], sizeof(uint8_t), HEBIOS_SIZE, handle) != sizeof(uint8_t) * HEBIOS_SIZE) {
		return false;
	}

	bios_set_image(&bios[0], HEBIOS_SIZE);
	
	if (psx_init() != 0) {
		return false;
	}

	psx_core_was_initialized = true;

	return true;
}

Psf::Psf(uint8_t compat, bool reverb,
	bool do_filter, bool suppressEndSilence, bool suppressOpeningSilence,
	int endSilenceSeconds) noexcept :
	compat(compat), reverb(reverb),
	do_filter(do_filter), suppressEndSilence(suppressEndSilence),
	suppressOpeningSilence(suppressOpeningSilence),
	endSilenceSeconds(endSilenceSeconds),
	openingSilenceSuppressed(false), psx_state(nullptr),
	silence_test_buffer(0), psf2fs(nullptr),
	psf_version(0), samplerate(0),
	info_state(psf_info_meta_state()),
	is_open(false)
{
	psf_file_system = {
		"\\/|:",
		nullptr,
		psf_file_fopen,
		psf_file_fread,
		psf_file_fseek,
		psf_file_fclose,
		psf_file_ftell
	};
}

Psf::~Psf()
{
	close();
	if (psf2fs != nullptr) {
		psf2fs_delete(psf2fs);
	}
	if (psx_state != nullptr) {
		delete[] psx_state;
	}
}

void Psf::close() noexcept
{
	is_open = false;
}

uint8_t Psf::open_version(const char* path) noexcept
{
	void* handle = psf_file_system.fopen(psf_file_system.context, path);
	if (handle == nullptr) {
		last_error = "Failed to open file";
		return 0;
	}

	psf_file_system.fseek(handle, 3, SEEK_SET);
	uint8_t version = 0;
	psf_file_system.fread(&version, 1, 1, handle);
	psf_file_system.fclose(handle);
	return version;
}

bool Psf::open(const char* p_path, bool infinite, int default_length, int default_fade)
{
	if (!psx_core_was_initialized) {
		last_error = "Please initialize PSX Core first";
		return false;
	}

	close();

	const uint8_t previous_version = psf_version;
	psf_version = open_version(p_path);

	if (psf_version == 0) {
		last_error = "Failed to open PSF file";
		return false;
	}

	if (psf_version > 2) {
		last_error = "Not a PSF1 or PSF2 file";
		return false;
	}

	samplerate = psf_version == 2 ? 48000 : 44100;
	no_loop = !infinite;

	if (psx_state != nullptr && psf_version != previous_version) {
		delete[] psx_state;
		psx_state = nullptr;
	}
	if (psx_state == nullptr) {
		psx_state = new char[psx_get_state_size(psf_version)];
	}

	psx_clear_state(psx_state, psf_version);

	last_status.clear();

	info_state = psf_info_meta_state();

	psf_load_state state;

	state.version = psf_version;
	state.meta_state = &info_state;

	if (psf_version == 1) {
		state.emu = psx_state;
		state.first = true;
	}
	else {
		if (psf2fs != nullptr) {
			psf2fs_delete(psf2fs);
		}

		psf2fs = psf2fs_create();
		if (psf2fs == nullptr) {
			last_error = "Cannot create psf2fs";
			return false;
		}

		state.psf2fs = psf2fs;
	}

	const int ret = psf_load(p_path, &psf_file_system, 0, psf_load_cb, &state, psf_info_meta, &info_state, 0, print_message, &last_status);

	if (ret < 0) {
		last_error = "Invalid PSF file";
		return false;
	}

	if (!info_state.tag_song_ms) {
		info_state.tag_song_ms = default_length;
		info_state.tag_fade_ms = default_fade;
	}

	if (info_state.refresh) {
		psx_set_refresh(psx_state, info_state.refresh);
	}

	if (psf_version == 2) {
		psx_set_readfile(psx_state, virtual_readfile, psf2fs);
	}

	{
		void* pIOP = psx_get_iop_state(psx_state);
		iop_set_compat(pIOP, compat);
		spu_enable_reverb(iop_get_spu_state(pIOP), reverb);
	}

	eof = false;
	err = 0;
	data_written = 0;
	pos_delta = 0;
	psfemu_pos_ms = 0;
	openingSilenceSuppressed = false;

	calcfade();

	if (suppressEndSilence) {
		const unsigned end_skip_max = endSilenceSeconds * samplerate;
		silence_test_buffer.resize(end_skip_max * 2);
	}

	if (do_filter) {
		filter.Redesign(samplerate);
	}

	is_open = true;

	return true;
}

int Psf::get_length() const noexcept
{
	return info_state.tag_song_ms + info_state.tag_fade_ms;
}

int Psf::get_sample_rate() const noexcept
{
	return samplerate;
}

int Psf::get_channel_count() const noexcept
{
	return 2;
}

int Psf::get_bits_per_seconds() const noexcept
{
	return 16;
}

const char* Psf::get_last_error() const noexcept
{
	return last_error;
}

const char* Psf::get_last_status() const noexcept
{
	return last_status.c_str();
}

size_t Psf::decode(int16_t* data, unsigned int sample_count)
{
	if (!is_open) {
		return false;
	}

	int remainder = 0;

	if (suppressOpeningSilence && !openingSilenceSuppressed) { // ohcrap
		int startsilence = 0;
		unsigned int silence = 0;
		unsigned start_skip_max = 60 * samplerate;

		for (;;) {
			unsigned skip_howmany = start_skip_max - silence;
			if (skip_howmany > sample_count) {
				skip_howmany = sample_count;
			}
			sample_buffer.resize(skip_howmany * 2);
			const int rtn = psx_execute(psx_state, 0x7FFFFFFF, sample_buffer.data(), &skip_howmany, 0);
			if (rtn < 0) {
				last_error = "Nothing executed";
				return 0;
			}

			short* foo = sample_buffer.data();
			unsigned i;

			for (i = 0; i < skip_howmany; ++i) {
				if (foo[0] || foo[1]) {
					break;
				}
				foo += 2;
			}

			silence += i;

			if (i < skip_howmany) {
				remainder = skip_howmany - i;
				memmove(sample_buffer.data(), foo, remainder * sizeof(short) * 2);
				break;
			}

			if (silence >= start_skip_max) {
				eof = true;
				break;
			}
		}

		startsilence += silence;
		silence = 0;
		openingSilenceSuppressed = true;
	}

	if ((eof || err < 0) && !silence_test_buffer.data_available()) {
		return 0;
	}

	if (no_loop && info_state.tag_song_ms
		&& pos_delta + MulDiv(data_written, 1000, samplerate) >= info_state.tag_song_ms + info_state.tag_fade_ms) {
		return 0;
	}

	UINT written = 0;

	unsigned int samples;

	if (no_loop) {
		samples = (song_len + fade_len) - data_written;
		if (samples > sample_count) {
			samples = sample_count;
		}
	}
	else {
		samples = sample_count;
	}

	if (suppressEndSilence) {
		sample_buffer.resize(sample_count * 2);

		if (!eof) {
			unsigned free_space = silence_test_buffer.free_space() / 2;
			while (free_space) {
				unsigned samples_to_render;
				if (remainder) {
					samples_to_render = remainder;
					remainder = 0;
				}
				else {
					samples_to_render = free_space;
					if (samples_to_render > sample_count) {
						samples_to_render = sample_count;
					}
					err = psx_execute(psx_state, 0x7FFFFFFF, sample_buffer.data(), & samples_to_render, 0);
					if (err == -2) {
						// FIXME: error
					}
					if (!samples_to_render) {
						last_error = "Nothing executed";
						return 0;
					}
				}
				silence_test_buffer.write(sample_buffer.data(), samples_to_render * 2);
				free_space -= samples_to_render;
				if (err < 0) {
					eof = true;
					break;
				}
			}
		}

		if (silence_test_buffer.test_silence()) {
			eof = true;
			return 0;
		}

		written = silence_test_buffer.data_available() / 2;
		if (written > samples) {
			written = samples;
		}
		silence_test_buffer.read(sample_buffer.data(), written * 2);
	}
	else {
		sample_buffer.resize(samples * 2);

		if (remainder) {
			written = remainder;
		}
		else {
			written = samples;
			//DBG("hw_execute()");
			err = psx_execute(psx_state, 0x7FFFFFFF, sample_buffer.data(), &written, 0);
			if (err == -2) {
				// FIXME: error
			}
			if (!written) {
				last_error = "Nothing executed";
				return 0;
			}
			if (err < 0) {
				eof = true;
			}
		}
	}

	psfemu_pos_ms += (long long)(written / double(samplerate) * 1000.0);

	int d_start, d_end;
	d_start = data_written;
	data_written += written;
	d_end = data_written;

	if (info_state.tag_song_ms && d_end > song_len && no_loop) {
		short* foo = sample_buffer.data();
		int n;
		for (n = d_start; n < d_end; ++n) {
			if (n > song_len) {
				if (n > song_len + fade_len) {
					*(DWORD*)foo = 0;
				}
				else {
					int bleh = song_len + fade_len - n;
					foo[0] = MulDiv(foo[0], bleh, fade_len);
					foo[1] = MulDiv(foo[1], bleh, fade_len);
				}
			}
			foo += 2;
		}
	}

	memcpy(data, sample_buffer.data(), written * sizeof(int16_t) * 2);
	/* 
	int16_t* input = reinterpret_cast<int16_t*>(sample_buffer.data());
	float* output = data;
	float p_scale = float(1.0f / double(0x8000));
	for (size_t num = written * 2; num; num--) {
		*(output++) = float(*(input++)) * p_scale;
	}

	if (do_filter) {
		filter.Process(data, written);
	}*/

	return written;
}

bool Psf::seek(unsigned int p_ms)
{
	if (!is_open) {
		return false;
	}

	eof = false;

	const int buffered_time_ms = int(silence_test_buffer.data_available() / 2.0 / double(samplerate) * 1000.0);

	psfemu_pos_ms += buffered_time_ms;

	silence_test_buffer.reset();

	if (do_filter) {
		filter.Reset();
	}

	void *pEmu = psx_state;
	if (p_ms < psfemu_pos_ms) { // Not initialized
		last_error = "Not initialized";
		return false;
	}
	unsigned int howmany = int(floor(samplerate * ((p_ms - psfemu_pos_ms) / 1000.0) + 0.5));

	// more abortable, and emu doesn't like doing huge numbers of samples per call anyway
	while (howmany) {
		unsigned todo = howmany;
		if (todo > 2048) {
			todo = 2048;
		}
		const int rtn = psx_execute(pEmu, 0x7FFFFFFF, 0, & todo, 0);
		if (rtn < 0 || ! todo) {
			eof = true;
			return false;
		}
		howmany -= todo;
	}

	data_written = 0;
	pos_delta = p_ms;
	psfemu_pos_ms = p_ms;

	calcfade();

	return true;
}

bool Psf::is_our_path(const char* p_full_path, const char* p_extension) noexcept
{
	if (p_extension == nullptr) {
		return false;
	}
	return !_stricmp(p_extension, "psf") || !_stricmp(p_extension, "minipsf") ||
		!_stricmp(p_extension, "psf2") || !_stricmp(p_extension, "minipsf2");
}

void Psf::calcfade() noexcept
{
	song_len = MulDiv(info_state.tag_song_ms - pos_delta, samplerate, 1000);
	fade_len = MulDiv(info_state.tag_fade_ms, samplerate, 1000);
}
