#pragma once

#include <cstdint>

class Akao {
private:
	uint8_t _reverb_type;
	uint32_t _channel_offsets[32];
	char* _data;
};
