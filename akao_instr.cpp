#include "akao_instr.h"

AkaoInstrAdpcm::~AkaoInstrAdpcm() noexcept
{
	if (_data != nullptr) {
		delete[] _data;
	}
}

bool AkaoInstrAdpcm::setDataSize(uint16_t data_size) noexcept
{
	if (_data != nullptr) {
		delete[] _data;
	}

	_data = new (std::nothrow) uint8_t[data_size]();

	if (_data == nullptr) {
		return false;
	}

	_data_size = data_size;

	return true;
}
