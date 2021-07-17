/****************************************************************************/
//    Original author: kode54                                               //
/****************************************************************************/
#include "openpsf.h"
#include "environment_psx.h"
#include "source_psf.h"

bool Psf::initialize_psx_core(const char* bios_path) noexcept
{
	return EnvironmentPsx::initialize(bios_path);
}

bool Psf::psx_core_was_initialized() noexcept
{
	return EnvironmentPsx::was_initialized();
}

Psf::Psf(PsfFlags flags, int end_silence_seconds) noexcept :
	_flags(flags),
	end_silence_seconds(end_silence_seconds), no_loop(true),
	eof(false), opening_silence_suppressed(false), opening_silence_buffer(),
	opening_silence_remaining_pos(0), opening_silence_remaining_samples(0),
	silence_test_buffer(), err(false),
	data_written(0),
	song_len(0), fade_len(0),
	samplerate(0), last_error(""),
	is_open(false), _psx(nullptr), _source(nullptr)
{
}

Psf::~Psf() noexcept
{
	close();
	if (_psx != nullptr) {
		delete _psx;
	}
	if (_source != nullptr) {
		delete _source;
	}
}

void Psf::close() noexcept
{
	is_open = false;
	if (_source != nullptr) {
		_source->close();
	}
}

bool Psf::open(const char* path, FileType type, bool infinite, int default_length, int default_fade) noexcept
{
	if (!psx_core_was_initialized()) {
		last_error = "Please initialize PSX Core first";
		return false;
	}

	close();

	switch (type) {
	case FileType::Psf:
		try {
			if (_psx == nullptr) {
				_psx = new EnvironmentPsx(!(_flags & NoReverb), _flags & Debug);
			}
			_source = new SourcePsf(_psx);
		}
		catch (std::bad_alloc e) {
			last_error = "Cannot allocate PSF Core";
			return false;
		}
		break;
	}

	if (!_source->open(path)) {
		return false;
	}

	no_loop = !infinite;
	song_len = ms_to_samples(_source->song_length_ms());
	fade_len = ms_to_samples(_source->song_fade_ms());

	if (!song_len) {
		song_len = default_length;
		fade_len = default_fade;
	}

	if (!song_len && no_loop) {
		last_error = "No song duration found";
		return false;
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

bool Psf::suppress_opening_silence() noexcept
{
	if (opening_silence_suppressed) {
		return true;
	}

	unsigned int silence = 0;
	const unsigned start_skip_max = 30 * samplerate;

	while (silence < start_skip_max && _source->can_decode()) {
		const int samples_to_render = _source->decode(&opening_silence_buffer[0], PSF_MAX_SAMPLE_COUNT);
		if (samples_to_render < 0) {
			err = true;
			last_error = "PSX unrecoverable error (opening silence)";
			return false;
		}

		// Test silence
		int i = 0;

		for (; i < samples_to_render; ++i) {
			if ((opening_silence_buffer[i * PSF_CHANNEL_COUNT] | opening_silence_buffer[i * PSF_CHANNEL_COUNT + 1]) != 0) {
				break;
			}
		}

		if (i < samples_to_render) {
			opening_silence_remaining_pos = i * PSF_CHANNEL_COUNT;
			opening_silence_remaining_samples = samples_to_render - i;
			break;
		}

		silence += i;
	}

	opening_silence_suppressed = true;

	// Timeout
	return silence < start_skip_max;
}

int Psf::decode(int16_t* data, int sample_count) noexcept
{
	if (!is_open || sample_count < 0) {
		last_error = !is_open ? "Not opened" : "Negative sample_count";
		return -1;
	}

	if (err) {
		last_error = "PSX unrecoverable error";
		return -1;
	}

	if (sample_count == 0 || ((eof || !_source->can_decode()) && !silence_test_buffer.data_available())) {
		return 0;
	}

	if (sample_count > PSF_MAX_SAMPLE_COUNT) {
		sample_count = PSF_MAX_SAMPLE_COUNT;
	}

	unsigned int written = 0, samples = static_cast<unsigned int>(sample_count);
	int16_t* source_buffer = data;

	if (_flags & SuppressOpeningSilence && !suppress_opening_silence()) {
		err = true;
		last_error = "Cannot skip opening silence";
		return -1;
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
				unsigned int rendered_samples;
				if (opening_silence_remaining_samples > 0) {
					rendered_samples = opening_silence_remaining_samples;
					if (rendered_samples > samples) {
						rendered_samples = samples;
					}
					memcpy(data, &opening_silence_buffer[opening_silence_remaining_pos], rendered_samples * sizeof(int16_t) * PSF_CHANNEL_COUNT);
					opening_silence_remaining_samples -= rendered_samples;
				}
				else {
					unsigned int samples_to_render = free_space;
					if (samples_to_render > samples) {
						samples_to_render = samples;
					}
					rendered_samples = _source->decode(data, samples_to_render);
					if (rendered_samples < 0) {
						err = true;
						last_error = "PSX unrecoverable error (end silence)";
						return -1;
					}
					if (rendered_samples == 0) {
						break;
					}
				}
				silence_test_buffer.write(data, rendered_samples * PSF_CHANNEL_COUNT);
				free_space -= rendered_samples;
				if (!_source->can_decode()) {
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
		silence_test_buffer.read(data, written * PSF_CHANNEL_COUNT);
	}
	else {
		if (opening_silence_remaining_samples > 0) {
			written = opening_silence_remaining_samples;
			if (written > samples) {
				written = samples;
			}
			if (data != nullptr) {
				memcpy(data, &opening_silence_buffer[opening_silence_remaining_pos], written * sizeof(int16_t) * PSF_CHANNEL_COUNT);
			}
			opening_silence_remaining_samples -= written;
		}
		
		if (written < samples) {
			const int w = _source->decode(data, samples - written);
			if (w < 0) {
				err = true;
				last_error = "PSX unrecoverable error";
				return -1;
			}
			written += w;
		}
	}

	if (no_loop) {
		data_written += written;
	}

	if (data == nullptr || written == 0) {
		return written;
	}

	// Fading
	if (no_loop && data_written > song_len) {
		int16_t* samples_a = source_buffer;
		const unsigned int d_start = data_written - written;
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
			samples_a += PSF_CHANNEL_COUNT;
		}
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

	_psx->reset();
	eof = false;
	err = false;
	data_written = 0;
	opening_silence_suppressed = false;
	silence_test_buffer.reset();

	return true;
}

unsigned int Psf::ms_to_samples(unsigned int ms) const noexcept
{
	return static_cast<long long>(ms) * static_cast<long long>(samplerate) / 1000;
}

Psf::FileType Psf::type_from_extension(const char* extension, bool& ok) noexcept
{
	ok = false;

	if (extension == nullptr) {
		return FileType::Psf;
	}

	if (0 == _stricmp(extension, "psf") || 0 == _stricmp(extension, "minipsf") ||
		0 == _stricmp(extension, "psf2") || 0 == _stricmp(extension, "minipsf2")) {
		ok = true;
		return FileType::Psf;
	}

	return FileType::Psf;
}

bool Psf::is_our_path(const char* path, const char* type) noexcept
{
	bool ok;
	type_from_extension(type, ok);
	return ok;
}
