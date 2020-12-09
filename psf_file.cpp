#include "psf_file.h"
#include <psf2fs.h>

constexpr auto BORK_TIME = 0xC0CAC01A;

unsigned long PsfFile::parse_time_crap(const char* input) noexcept
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

int PsfFile::psf_info_meta(void* context, const char* name, const char* value) noexcept
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

PsfFile::PsfFile() noexcept :
	_psf_version(0), _psf_file_system(), _is_open(false),
	_length_ms(0), _fade_ms(0), _last_error("")
{
}

uint8_t PsfFile::open_version(const char* path) noexcept
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

int PsfFile::psf_load_cb(void* context, const uint8_t* exe, size_t exe_size,
	const uint8_t* reserved, size_t reserved_size) noexcept
{
	PsfFile* ctxt = static_cast<PsfFile*>(context);
	return ctxt->psf_load_callback(exe, exe_size, reserved, reserved_size);
}

bool PsfFile::open(const char* path) noexcept
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

	_last_status.clear();
	PsfInfoMetaState info_state = PsfInfoMetaState();

	const int ret = psf_load(
		path, &_psf_file_system, 0, psf_load_cb, this,
		psf_info_meta, &info_state, false, print_message, &_last_status
	);

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

	_is_open = true;

	return true;
}

bool PsfFile::close() noexcept
{
	_is_open = false;

	return true;
}

int EMU_CALL _psf2fs_virtual_readfile(void* psf2fs, const char* path, int offset, char* buffer, int length) noexcept
{
	return psf2fs_virtual_readfile(psf2fs, path, offset, buffer, length);
}

PsfFilePsExe::PsfFilePsExe(EnvironmentPsx* psx) noexcept :
	PsfFile(), _psf2fs(nullptr), _psx(psx), _first(false)
{
}

PsfFilePsExe::~PsfFilePsExe() noexcept
{
	if (_psf2fs != nullptr) {
		delete _psf2fs;
	}
}

int PsfFilePsExe::psf_load_callback(const uint8_t* exe, size_t exe_size,
	const uint8_t* reserved, size_t reserved_size) noexcept
{
	bool err = false;

	if (version() == 1) {
		if (_first) {
			err = _psx->ps1_upload_psexe(exe, exe_size);
			_first = false;
		}
		else {
			err = _psx->ps1_upload_memory(exe, exe_size);
		}
	}
	else {
		err = psf2fs_load_callback(_psf2fs, exe, exe_size, reserved, reserved_size);
	}

	return err ? -1 : 0;
}

bool PsfFilePsExe::open(const char* path) noexcept
{
	_first = true;

	if (!PsfFile::open(path)) {
		return false;
	}

	if (version() == 2) {
		_psx->ps2_set_readfile(_psf2fs_virtual_readfile, _psf2fs);
	}

	return true;
}
