/****************************************************************************/
//    Original author: kode54                                               //
/****************************************************************************/
#include "openpsf.h"

constexpr auto BORK_TIME = 0xC0CAC01A;
constexpr auto PSF_CHANNEL_COUNT = 2;

#include "../highly_experimental/Core/psx.h"
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

struct psf_load_state {
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
constexpr auto byte_order_is_big_endian = false;
#else
constexpr auto byte_order_is_big_endian = true;
#endif

uint32_t byteswap_if_be_t(uint32_t p_param) noexcept {
	return byte_order_is_big_endian ? _byteswap_ulong(p_param) : p_param;
}

int psf1_load_callback(psf_load_state* state, const uint8_t* exe, size_t exe_size,
	const uint8_t* reserved, size_t reserved_size) noexcept
{
	if (exe_size < 0x800) {
		return -1;
	}

	const psxexe_hdr_t* psx = reinterpret_cast<const psxexe_hdr_t*>(exe);
	uint32_t addr = byteswap_if_be_t(psx->exec.t_addr);
	const uint32_t size = exe_size - 0x800;

	addr &= 0x1fffff;
	if (addr < 0x10000 || size > 0x1f0000 || addr + size > 0x200000) {
		return -1;
	}

	void* pIOP = psx_get_iop_state(state->emu);
	iop_upload_to_ram(pIOP, addr, exe + 0x800, size);

	if (!state->meta_state->refresh) {
		if (_strnicmp((const char*)exe + 113, "Europe", 6) == 0) {
			state->meta_state->refresh = 50;
		}
		else {
			state->meta_state->refresh = 60;
		}
	}

	if (state->first) {
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
	if (context == nullptr) {
		return;
	}

	std::string* string = static_cast<std::string*>(context);
	string->append(message);
}

bool Psf::psx_core_was_initialized = false;
uint8_t Psf::bios[HEBIOS_SIZE] = {};

bool Psf::initialize_psx_core(const char* bios_path) noexcept
{
	FILE* handle = nullptr;
	if (fopen_s(&handle, bios_path, "rb") != 0 || handle == nullptr) {
		return false;
	}

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
	int endSilenceSeconds) :
	compat(compat), reverb(reverb),
	do_filter(do_filter), suppressEndSilence(suppressEndSilence),
	suppressOpeningSilence(suppressOpeningSilence),
	endSilenceSeconds(endSilenceSeconds), no_loop(true),
	eof(false), openingSilenceSuppressed(false),
	psx_state_size(0), psx_state(nullptr), psx_initial_state(nullptr),
	silence_test_buffer(0), psf2fs(nullptr), err(0),
	psf_version(0), data_written(0),
	song_len(0), fade_len(0),
	samplerate(0), last_error(""),
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
		delete[] psx_initial_state;
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
	psx_state_size = psx_get_state_size(psf_version);

	if (psx_state != nullptr && psf_version != previous_version) {
		delete[] psx_state;
		delete[] psx_initial_state;
		psx_state = nullptr;
	}
	if (psx_state == nullptr) {
		psx_state = new char[psx_state_size];
		psx_initial_state = new char[psx_state_size];
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

	last_status.append("\nPSF Version: ");
	last_status.append(psf_version == 1 ? "1" : "2");
	last_status.append(", refresh: ");
	last_status.append(info_state.refresh == 50 ? "50" : (info_state.refresh == 60 ? "60" : "0"));

	if (!info_state.tag_song_ms) {
		info_state.tag_song_ms = default_length;
		info_state.tag_fade_ms = default_fade;
	}

	if (info_state.tag_song_ms) {
		song_len = ms_to_samples(info_state.tag_song_ms);
		fade_len = ms_to_samples(info_state.tag_fade_ms);
	}
	else if (no_loop) {
		last_error = "No song duration found";
		return false;
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
	openingSilenceSuppressed = false;

	if (suppressEndSilence) {
		const unsigned end_skip_max = endSilenceSeconds * samplerate;
		silence_test_buffer.resize(end_skip_max * 2);
	}

	/* if (do_filter) {
		filter.Redesign(samplerate);
	} */

	memcpy(psx_initial_state, psx_state, psx_state_size);

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
	return PSF_CHANNEL_COUNT;
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

int Psf::decode(int16_t* data, int sample_count)
{
	if (!is_open || sample_count < 0) {
		last_error = !is_open ? "Not opened" : "Negative sample_count";
		return -1;
	}

	if (err <= -2) {
		last_error = "PSX unrecoverable error";
		return -1;
	}

	if (sample_count == 0 || ((err == -1 || eof) && !silence_test_buffer.data_available())) {
		return 0;
	}

	unsigned int _sample_count = (unsigned int)sample_count;
	int remainder = 0;

	if (suppressOpeningSilence && !openingSilenceSuppressed) { // ohcrap
		unsigned int silence = 0;
		const unsigned start_skip_max = 60 * samplerate;

		while (silence < start_skip_max) {
			unsigned int skip_howmany = start_skip_max - silence;
			if (skip_howmany > _sample_count) {
				skip_howmany = _sample_count;
			}
			sample_buffer.resize(skip_howmany * PSF_CHANNEL_COUNT);
			err = psx_execute(psx_state, 0x7FFFFFFF, sample_buffer.data(), &skip_howmany, 0);
			if (err <= -2) {
				last_error = "PSX unrecoverable error (opening silence)";
				return -1;
			}

			const int32_t* cur_buff = reinterpret_cast<int32_t*>(sample_buffer.data());
			unsigned int i = 0;

			for (; i < skip_howmany; ++i) {
				if (cur_buff[i]) {
					break;
				}
			}

			silence += i;

			if (i < skip_howmany) {
				remainder = skip_howmany - i;
				memmove(sample_buffer.data(), &cur_buff[i], remainder * sizeof(uint16_t) * PSF_CHANNEL_COUNT);
				break;
			}

			if (err == -1) {
				eof = true;
				break;
			}
		}

		openingSilenceSuppressed = true;

		if (silence >= start_skip_max) {
			eof = true;
			return 0;
		}
	}

	unsigned int written = 0, samples;

	if (no_loop) {
		if (song_len + fade_len <= data_written) {
			return 0;
		}

		samples = song_len + fade_len - data_written;

		if (samples > _sample_count) {
			samples = _sample_count;
		}
	}
	else {
		samples = _sample_count;
	}

	if (data != nullptr && suppressEndSilence) {
		sample_buffer.resize(_sample_count * PSF_CHANNEL_COUNT);

		if (!eof) {
			unsigned free_space = silence_test_buffer.free_space() / PSF_CHANNEL_COUNT;
			while (free_space) {
				unsigned int samples_to_render;
				if (remainder) {
					samples_to_render = remainder;
					remainder = 0;
				}
				else {
					samples_to_render = free_space;
					if (samples_to_render > _sample_count) {
						samples_to_render = _sample_count;
					}
					err = psx_execute(psx_state, 0x7FFFFFFF, sample_buffer.data(), &samples_to_render, 0);
					if (err <= -2) {
						last_error = "PSX unrecoverable error (end silence)";
						return -1;
					}
					if (!samples_to_render) {
						eof = true;
						break;
					}
				}
				silence_test_buffer.write(sample_buffer.data(), samples_to_render * PSF_CHANNEL_COUNT);
				free_space -= samples_to_render;
				if (err == -1) {
					eof = true;
					break;
				}
			}
		}

		if (silence_test_buffer.test_silence()) {
			eof = true;
			return 0;
		}

		written = silence_test_buffer.data_available() / PSF_CHANNEL_COUNT;
		if (written > samples) {
			written = samples;
		}
		silence_test_buffer.read(sample_buffer.data(), written * PSF_CHANNEL_COUNT);
	}
	else {
		if (data != nullptr) {
			sample_buffer.resize(samples * PSF_CHANNEL_COUNT);
		}

		if (remainder) {
			written = remainder;
		}
		else {
			written = samples;
			err = psx_execute(psx_state, 0x7FFFFFFF, data != nullptr ? sample_buffer.data() : nullptr, &written, 0);
			if (err <= -2) {
				last_error = "PSX unrecoverable error";
				return -1;
			}
			if (!written || err == -1) {
				eof = true;
			}
		}
	}

	const unsigned int d_start = data_written;
	data_written += written;

	if (data == nullptr || written == 0) {
		return written;
	}

	if (no_loop && data_written > song_len) {
		int16_t* foo = sample_buffer.data();
		unsigned int n;
		for (n = d_start; n < data_written; ++n) {
			if (n > song_len) {
				if (n >= song_len + fade_len) {
					*(unsigned long*)foo = 0;
				}
				else {
					const unsigned int bleh = song_len + fade_len - n;
					foo[0] = int16_t(int(foo[0] * bleh) / fade_len);
					foo[1] = int16_t(int(foo[1] * bleh) / fade_len);
				}
			}
			foo += 2;
		}
	}

	memcpy(data, sample_buffer.data(), written * sizeof(int16_t) * PSF_CHANNEL_COUNT);

	/* int16_t* input = reinterpret_cast<int16_t*>(sample_buffer.data());
	float* output = data;
	float p_scale = float(1.0f / double(0x8000));
	for (size_t num = written * 2; num; num--) {
		*(output++) = float(*(input++)) * p_scale;
	}

	if (do_filter) {
		filter.Process(data, written);
	} */

	return written;
}

bool Psf::seek(unsigned int p_ms)
{
	if (!is_open) {
		return false;
	}

	const unsigned int seek_pos = ms_to_samples(p_ms);

	if (seek_pos == data_written) {
		return true;
	}

	if (seek_pos < data_written) {
		// Reinitialize decode
		memcpy(psx_state, psx_initial_state, psx_state_size);

		eof = false;
		err = 0;
		data_written = 0;
		openingSilenceSuppressed = false;
	}

	return decode(nullptr, seek_pos - data_written);
}

bool Psf::is_our_path(const char* p_full_path, const char* p_extension) noexcept
{
	if (p_extension == nullptr) {
		return false;
	}
	return !_stricmp(p_extension, "psf") || !_stricmp(p_extension, "minipsf") ||
		!_stricmp(p_extension, "psf2") || !_stricmp(p_extension, "minipsf2");
}

unsigned int Psf::ms_to_samples(unsigned int ms) const noexcept
{
	return (long long)(ms * samplerate) / 1000;
}
