#include "source_psf.h"
#include "environment_psx.h"
#include <psflib.h>
#include <psf2fs.h>

constexpr auto BORK_TIME = 0xC0CAC01A;

SourcePsf::SourcePsf(EnvironmentPsx* psx) noexcept :
	_psf_file(psx), _last_error("")
{
}

int32_t SourcePsf::decode(int16_t* buffer, uint16_t max_samples) noexcept
{
	if (!_psf_file.isOpen()) {
		_last_error = "No opened";
		return -1;
	}

	const int32_t read = _psf_file.psx()->execute(buffer, max_samples);

	if (read < 0) {
		_last_error = nullptr; // Force psx last error
	}

	return read;
}
