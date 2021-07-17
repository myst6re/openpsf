#include "akao_instr_file.h"
#include <stdio.h>
#include <bitset>

struct AkaoInstrAdpcmHeader
{
	uint32_t spu_addr;
	uint32_t data_size;
};

int AkaoInstrFile::open(AkaoInstrAttr* akaoAttr, int max_instruments) const noexcept
{
	FILE* handle = nullptr;
	if (fopen_s(&handle, _path, "rb") != 0 || handle == nullptr) {
		return -1;
	}

	for (int i = 0; i < max_instruments; ++i) {
		AkaoInstrAttr attrs;

		if (fread(&attrs, sizeof(AkaoInstrAttr), 1, handle) != 1) {
			return i;
		}

		akaoAttr[i] = attrs;
	}

	return max_instruments;
}

bool AkaoInstrAdpcmFile::open(AkaoInstrAdpcm& akaoAdpcm) const noexcept
{
	FILE* handle = nullptr;
	if (fopen_s(&handle, _path, "rb") != 0 || handle == nullptr) {
		return false;
	}

	AkaoInstrAdpcmHeader header;

	if (fread(&header, sizeof(AkaoInstrAdpcmHeader), 1, handle) != 1) {
		return false;
	}

	if (fseek(handle, 8, SEEK_CUR) != 0) {
		return false;
	}

	akaoAdpcm.setSpuAddr(header.spu_addr);
	akaoAdpcm.setDataSize(header.data_size);

	if (fread(akaoAdpcm.data(), sizeof(uint8_t), header.data_size, handle) != header.data_size) {
		return false;
	}

	return true;
}
