#pragma once

#include <cstdint>

constexpr auto MAX_INSTRUMENTS = 256;

struct AkaoInstrAttr // 0x75F28 size: 64
{
	/* offsets */
	uint32_t addr;              // 0x00: offset to attack part of instrument (SPU hardware address)
	uint32_t loop_addr;         // 0x04: offset to loop part of instrument (SPU hardware address)
	/* ADSR Envelope settings */
	uint8_t adsr_attack_rate;   // 0x08: ADSR: attack rate (0x00-0x7f)
	uint8_t adsr_decay_rate;    // 0x09: ADSR: decay rate (0x00-0x0f)
	uint8_t adsr_sustain_level; // 0x0a: ADSR: sustain level (0x00-0x0f)
	uint8_t adsr_sustain_rate;  // 0x0b: ADSR: sustain rate (0x00-0x7f)
	uint8_t adsr_release_rate;  // 0x0c: ADSR: release rate (0x00-0x1f)
	uint8_t adsr_attack_mode;   // 0x0d: ADSR: attack mode
						   //    0x05 - (exponential)
						   //    0x01 (default) - (linear)
	uint8_t adsr_sustain_mode;  // 0x0e ADSR: sustain mode
						   //    0x01 - (linear, increase)
						   //    0x05 - (exponential, increase)
						   //    0x07 - (exponential, decrease)
						   //    0x03 (default) - (linear, decrease)
	uint8_t adsr_release_mode;  // 0x0f: ADSR: release mode
						   //    0x07 - (exponential decrease)
						   //    0x03 (default) - (linear decrease)
	uint32_t pitch[12];         // 0x10: predefined pitch set for instrument
};

class AkaoInstrAdpcm {
private:
	uint8_t* _data;
	uint32_t _data_size;
	uint32_t _spu_addr;

public:
	AkaoInstrAdpcm() noexcept :
		_data(nullptr), _data_size(0),
		_spu_addr(0)
	{}
	virtual ~AkaoInstrAdpcm() noexcept;
	inline const uint8_t* data() const noexcept {
		return _data;
	}
	inline uint8_t* data() noexcept {
		return _data;
	}
	inline uint32_t dataSize() const noexcept {
		return _data_size;
	}
	bool setDataSize(uint16_t data_size) noexcept;
	inline uint32_t spuAddr() const noexcept {
		return _spu_addr;
	}
	inline void setSpuAddr(uint32_t spu_addr) noexcept {
		_spu_addr = spu_addr;
	}
};

struct AkaoInstr {
	AkaoInstrAttr attrs[MAX_INSTRUMENTS];
	AkaoInstrAdpcm adpcm;
};
