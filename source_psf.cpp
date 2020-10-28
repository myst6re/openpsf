#include "source_psf.h"
#include "environment_psx.h"
#include <psflib.h>
#include <psf2fs.h>

constexpr auto BORK_TIME = 0xC0CAC01A;

struct PsfInfoMetaState
{
	int tag_song_ms;
	int tag_fade_ms;
	bool looping;
};

struct PsfLoadState {
	EnvironmentPsx* psx;
	int version;
	bool first;
	PsfInfoMetaState* meta_state;
	void* psf2fs;
};

unsigned long SourcePsf::parse_time_crap(const char* input) noexcept
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

int SourcePsf::psf_info_meta(void* context, const char* name, const char* value) noexcept
{
	if (name == nullptr) {
		return -1;
	}

	PsfInfoMetaState* state = static_cast<PsfInfoMetaState*>(context);

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

int SourcePsf::psf_load_cb(void* context, const uint8_t* exe, size_t exe_size,
	const uint8_t* reserved, size_t reserved_size) noexcept
{
	PsfLoadState* state = static_cast<PsfLoadState*>(context);
	bool err = false;

	if (state->version == 1) {
		if (state->first) {
			err = state->psx->ps1_upload_psexe(exe, exe_size);
			state->first = false;
		}
		else {
			err = state->psx->ps1_upload_memory(exe, exe_size);
		}
	}
	else {
		err = psf2fs_load_callback(state->psf2fs, exe, exe_size, reserved, reserved_size);
	}

	return err ? -1 : 0;
}

static void* psf_file_fopen(void* context, const char* uri) noexcept
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

SourcePsf::SourcePsf(EnvironmentPsx* psx) noexcept :
	_psf_version(0), _psf_file_system(), _is_open(false),
	_length_ms(0), _fade_ms(0), _psf2fs(nullptr), _psx(psx), _last_error("")
{
}

SourcePsf::~SourcePsf() noexcept
{
	if (_psf2fs != nullptr) {
		delete _psf2fs;
	}
}

uint8_t SourcePsf::open_version(const char* path) noexcept
{
	void* handle = _psf_file_system.fopen(_psf_file_system.context, path);
	if (handle == nullptr) {
		_last_error = "Failed to open file";
		return 0;
	}

	_psf_file_system.fseek(handle, 3, SEEK_SET);
	uint8_t version = 0;
	_psf_file_system.fread(&version, 1, 1, handle);
	_psf_file_system.fclose(handle);
	return version;
}

int EMU_CALL _psf2fs_virtual_readfile(void* psf2fs, const char* path, int offset, char* buffer, int length) noexcept
{
	return psf2fs_virtual_readfile(psf2fs, path, offset, buffer, length);
}

bool SourcePsf::open(const char* path) noexcept
{
	close();

	const uint8_t previous_version = _psf_version;
	_psf_version = open_version(path);

	if (_psf_version == 0) {
		_last_error = "Failed to open PSF file";
		return false;
	}

	if (_psf_version > 2) {
		_last_error = "Not a PSF1 or PSF2 file";
		return false;
	}

	_psx->reset(_psf_version);

	_last_status.clear();

	PsfInfoMetaState info_state = PsfInfoMetaState();
	PsfLoadState state;

	state.version = _psf_version;
	state.meta_state = &info_state;

	if (_psf_version == 1) {
		state.psx = _psx;
		state.first = true;
	}
	else {
		if (_psf2fs != nullptr) {
			psf2fs_delete(_psf2fs);
		}

		_psf2fs = psf2fs_create();
		if (_psf2fs == nullptr) {
			_last_error = "Cannot create psf2fs";
			return false;
		}

		state.psf2fs = _psf2fs;
	}

	const int ret = psf_load(path, &_psf_file_system, 0, psf_load_cb, &state, psf_info_meta, &info_state, 0, print_message, &_last_status);

	if (ret < 0) {
		_last_error = "Invalid PSF file";
		return false;
	}

	try {
		_last_status.append("\nPSF Version: ");
		_last_status.append(_psf_version == 1 ? "1" : "2");
	}
	catch (std::exception e) {}

	_length_ms = info_state.tag_song_ms;
	_fade_ms = info_state.tag_fade_ms;

	if (_psf_version == 2) {
		_psx->ps2_set_readfile(_psf2fs_virtual_readfile, _psf2fs);
	}

	_is_open = true;

	return true;
}

bool SourcePsf::close() noexcept
{
	_is_open = false;

	return true;
}

int SourcePsf::decode(int16_t* buffer, uint16_t max_samples) noexcept
{
	if (!_is_open) {
		return -1;
	}

	return _psx->execute(buffer, max_samples);
}
