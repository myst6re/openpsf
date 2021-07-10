/****************************************************************************/
//    Original author: kode54                                               //
/****************************************************************************/
#include "openpsf/openpsf.h"

constexpr auto BORK_TIME = 0xC0CAC01A;
constexpr auto PSF_CHANNEL_COUNT = 2;
constexpr auto PSF_MAX_SAMPLE_COUNT = 1024;

#include <highly_experimental/psx.h>
#include <highly_experimental/iop.h>
#include <highly_experimental/r3000.h>
#include <highly_experimental/spu.h>
#include <highly_experimental/bios.h>

#include <psflib/psf2fs.h>

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
		char* end;
		if (ptr != input) {
			++ptr;
		}
		if (multiplier == 1000) {
			const double temp = strtof(ptr, &end);
			if (temp >= 60.0) {
				return BORK_TIME;
			}
			value = static_cast<long>(temp * 1000.0);
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
	return fseek(static_cast<FILE*>(handle), static_cast<long>(offset), whence);
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

Psf::Psf(PsfFlags flags, int end_silence_seconds) :
	compat(flags & Debug ? IOP_COMPAT_HARSH : IOP_COMPAT_FRIENDLY), reverb(!(flags & NoReverb)),
	simulate_frequency_response(!(flags & NoSimulateFrequencyResponse)),
	suppress_opening_silence(flags & SuppressOpeningSilence),
	end_silence_seconds(end_silence_seconds), no_loop(true),
	eof(false), opening_silence_suppressed(false),
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
	sample_buffer = new int16_t[PSF_MAX_SAMPLE_COUNT * PSF_CHANNEL_COUNT];
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
	if (psx_initial_state != nullptr) {
		delete[] psx_initial_state;
	}
	delete[] sample_buffer;
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
		if (psx_initial_state != nullptr) {
			delete[] psx_initial_state;
		}
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
	opening_silence_suppressed = false;

	if (end_silence_seconds > 0) {
		const unsigned end_skip_max = end_silence_seconds * samplerate;
		silence_test_buffer.resize(end_skip_max * 2);
	}

	/* if (simulate_frequency_response) {
		filter.Redesign(samplerate);
	} */

	memcpy(psx_initial_state, psx_state, psx_state_size);

	is_open = true;

	return true;
}

int Psf::get_sample_count() const noexcept
{
	return song_len + fade_len;
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

	if (sample_count > PSF_MAX_SAMPLE_COUNT) {
		sample_count = PSF_MAX_SAMPLE_COUNT;
	}

	unsigned int remainder = 0, written = 0, samples = static_cast<unsigned int>(sample_count);
	int16_t* safe_buffer = data == nullptr ? sample_buffer : data;
	int16_t* source_buffer = safe_buffer;

	if (suppress_opening_silence && !opening_silence_suppressed) { // ohcrap
		unsigned int silence = 0;
		const unsigned start_skip_max = 30 * samplerate;

		while (silence < start_skip_max) {
			unsigned int samples_to_render = samples;
			err = psx_execute(psx_state, 0x7FFFFFFF, safe_buffer, &samples_to_render, 0);
			if (err <= -2) {
				last_error = "PSX unrecoverable error (opening silence)";
				return -1;
			}

			// Test silence
			unsigned int i = 0;

			for (; i < samples_to_render; ++i) {
				if (safe_buffer[i * PSF_CHANNEL_COUNT] || safe_buffer[i * PSF_CHANNEL_COUNT + 1]) {
					break;
				}
			}

			if (i < samples_to_render) {
				remainder = samples_to_render - i;
				source_buffer = &safe_buffer[i * PSF_CHANNEL_COUNT];
				break;
			}

			if (err == -1) {
				break;
			}

			silence += i;
		}

		// Timeout
		if (silence >= start_skip_max) {
			eof = true;
			last_error = "Cannot skip opening silence";
			return -2;
		}

		opening_silence_suppressed = true;

		if (err == -1) {
			eof = true;
		}
	}

	if (no_loop) {
		// No more data
		if (song_len + fade_len <= data_written) {
			return 0;
		}

		const unsigned int remaining_to_write = song_len + fade_len - data_written;

		if (remaining_to_write < samples) {
			samples = remaining_to_write;
		}
	}

	if (data != nullptr && end_silence_seconds > 0) {
		if (!eof) {
			unsigned int free_space = silence_test_buffer.free_space() / PSF_CHANNEL_COUNT;

			while (free_space > 0) {
				unsigned int samples_to_render;
				if (remainder > 0) {
					samples_to_render = remainder;
					remainder = 0;
				}
				else {
					samples_to_render = free_space;
					if (samples_to_render > samples) {
						samples_to_render = samples;
					}
					err = psx_execute(psx_state, 0x7FFFFFFF, source_buffer, &samples_to_render, 0);
					if (err <= -2) {
						last_error = "PSX unrecoverable error (end silence)";
						return -1;
					}
					if (samples_to_render == 0) {
						eof = true;
						break;
					}
				}
				silence_test_buffer.write(source_buffer, samples_to_render * PSF_CHANNEL_COUNT);
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
		silence_test_buffer.read(source_buffer, written * PSF_CHANNEL_COUNT);
	}
	else {
		if (remainder > 0) {
			written = remainder;
		}
		else {
			written = samples;
			source_buffer = data;
			err = psx_execute(psx_state, 0x7FFFFFFF, data != nullptr ? data : nullptr, &written, 0);
			if (err <= -2) {
				last_error = "PSX unrecoverable error";
				return -1;
			}
			if (written == 0 || err == -1) {
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
		int16_t* samples_a = source_buffer;
		unsigned int n;
		for (n = d_start; n < data_written; ++n) {
			if (n > song_len) {
				if (n >= song_len + fade_len) {
					*(int32_t*)samples_a = 0;
				}
				else {
					const unsigned int cur = song_len + fade_len - n;
					samples_a[0] = int16_t(int(samples_a[0] * cur) / fade_len);
					samples_a[1] = int16_t(int(samples_a[1] * cur) / fade_len);
				}
			}
			samples_a += 2;
		}
	}

	if (source_buffer != data) {
		memcpy(data, source_buffer, written * sizeof(int16_t) * PSF_CHANNEL_COUNT);
	}

	/* int16_t* input = sample_buffer;
	float* output = data;
	float p_scale = float(1.0f / double(0x8000));
	for (size_t num = written * 2; num; num--) {
		*(output++) = float(*(input++)) * p_scale;
	}

	if (simulate_frequency_response) {
		filter.Process(data, written);
	} */

	return written;
}

bool Psf::rewind() noexcept
{
	if (!is_open) {
		last_error = "Not opened";
		return false;
	}
	// Reinitialize decode // FIXME: verify
	memcpy(psx_state, psx_initial_state, psx_state_size);

	eof = false;
	err = 0;
	data_written = 0;
	opening_silence_suppressed = false;
	silence_test_buffer.reset();

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

unsigned int Psf::ms_to_samples(unsigned int ms) const noexcept
{
	return static_cast<long long>(ms) * static_cast<long long>(samplerate) / 1000;
}
