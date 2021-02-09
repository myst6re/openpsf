#include "akao_file.h"
#include <stdio.h>
#include <bitset>

struct AkaoSeqHeader
{
	char akao[4];
	uint16_t id;                    // song ID, used for playing sequence
	uint16_t data_size;             // data length - sizeof(AkaoSeqHeader) + sizeof(channel_mask)
	uint16_t reverb_type;           // reverb type (range from 0 to 9)
	uint8_t year_bcd;               // year (in binary coded decimal)
	uint8_t month_bcd;              // month (in binary coded decimal, between 0x01 - 0x12)
	uint8_t day_bcd;                // day (in binary coded decimal, between 0x01 - 0x31)
	uint8_t hours_bcd;              // hours (in binary coded decimal, between 0x00 - 0x23)
	uint8_t minutes_bcd;            // minutes (in binary coded decimal, between 0x00 - 0x59)
	uint8_t seconds_bcd;            // seconds (in binary coded decimal, between 0x00 - 0x59)
};

bool AkaoFile::open(Akao& akao) const noexcept
{
	FILE* handle = nullptr;
	if (fopen_s(&handle, _path, "rb") != 0 || handle == nullptr) {
		return false;
	}

	AkaoSeqHeader header;

	if (fread(&header, sizeof(AkaoSeqHeader), 1, handle) != 1) {
		return false;
	}

	if (strncmp(&header.akao[0], "AKAO", 4) != 0) {
		return false;
	}

	if (!akao.setDataSize(header.data_size)) {
		return false;
	}
	akao.setSongId(header.id);

	if (fread(akao.data(), sizeof(uint8_t), header.data_size, handle) != header.data_size) {
		return false;
	}

	return true;
}
