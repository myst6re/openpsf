#include "akao.h"
#include "include/LIBSPU.H"
#include <stdio.h>
#include <bitset>

struct AkaoSeqHeader
{
	char akao[4];
	uint16_t id;                    // song ID, used for playing sequence
	uint16_t length;                // data length - sizeof(header)
	uint16_t reverb_type;           // reverb type (range from 0 to 9)
	uint8_t year_bcd;               // year (in binary coded decimal)
	uint8_t month_bcd;              // month (in binary coded decimal, between 0x01 - 0x12)
	uint8_t day_bcd;                // day (in binary coded decimal, between 0x01 - 0x31)
	uint8_t hours_bcd;              // hours (in binary coded decimal, between 0x00 - 0x23)
	uint8_t minutes_bcd;            // minutes (in binary coded decimal, between 0x00 - 0x59)
	uint8_t seconds_bcd;            // seconds (in binary coded decimal, between 0x00 - 0x59)
	uint32_t channel_mask;          // represents bitmask of used channels in this song
};

bool Akao::open(const char* path) noexcept
{
	FILE* handle = nullptr;
	if (fopen_s(&handle, path, "rb") != 0 || handle == nullptr) {
		return false;
	}

	AkaoSeqHeader header;

	if (fread(&header, sizeof(AkaoSeqHeader), 1, handle) != 1) {
		return false;
	}

	if (strncmp(&header.akao[0], "AKAO", 4) != 0) {
		return false;
	}

	const std::bitset<AKAO_CHANNEL_MAX> mask(header.channel_mask);
	_channel_count = mask.count();

	if (fread(&_channel_offsets[0], sizeof(uint16_t), _channel_count, handle) != _channel_count) {
		return false;
	}

	_data_size = header.length;

	for (int i = 0; i < _channel_count; ++i) {
		if (_channel_offsets[i] >= _data_size) {
			return false;
		}
	}

	_data = new (std::nothrow) uint8_t[_data_size]();

	if (_data == nullptr) {
		return false;
	}

	if (fread(_data, sizeof(uint8_t), _data_size, handle) != _data_size) {
		return false;
	}

	return true;
}

bool AkaoExec::loadInstrument(uint8_t instrument)
{
	return true;
}

void AkaoExec::akaoSetInstrument(AkaoPlayerTrack& playerTrack, uint8_t instrument)
{
	const AkaoInstrumentAttr& instr = _instruments[instrument];

	playerTrack.instrument = instrument;
	playerTrack.spu_addr = instr.addr;
	playerTrack.loop_addr = instr.loop_addr;
	playerTrack.adsr_attack_mode = instr.adsr_attack_mode;
	playerTrack.adsr_sustain_mode = instr.adsr_sustain_mode;
	playerTrack.adsr_release_mode = instr.adsr_release_mode;
	playerTrack.adsr_attack_rate = instr.adsr_attack_rate;
	playerTrack.adsr_decay_rate = instr.adsr_decay_rate;
	playerTrack.adsr_sustain_level = instr.adsr_sustain_level;
	playerTrack.adsr_sustain_rate = instr.adsr_sustain_rate;
	playerTrack.adsr_release_rate = instr.adsr_release_rate;
	playerTrack.update_flags |= 0x1FF80;
}

void AkaoExec::turnOnOverlayVoice(const uint8_t* data, AkaoPlayerTrack& playerTrack)
{
	int voice_mask, voice = 1, voice_num;
	if (!(playerTrack.voice_effect_flags & AkaoVoiceEffectFlags::OverlayVoiceEnabled)) {
		voice_num = -(_unknown62F04 > 0) & 0x18;
		while ((_player.active_voices | _player.overlay_voices | _player.alternate_voices) & voice) {
			voice <<= 1;
			voice_num += 1;
			if ((voice & 0xFFFFFFFF) == 0) {
				return; // Not found
			}
		}
		voice_mask = 0xFF0000;
	}
	else {
		int overlay_voice_num = playerTrack.overlay_voice_num;
		voice_num = playerTrack.overlay_voice_num;
		if (overlay_voice_num >= 24) {
			overlay_voice_num -= 24;
		}
		voice = 1 << overlay_voice_num;
		voice_mask = 0xFFFFFFFF;
	}
	if (voice & voice_mask) {
		_player.overlay_voices |= voice;
		playerTrack.overlay_voice_num = voice_num & 0xFFFF;
		playerTrack.voice_effect_flags |= AkaoVoiceEffectFlags::OverlayVoiceEnabled;
		uint8_t instrument1 = data[1], instrument2 = data[2];
		akaoSetInstrument(playerTrack, instrument1);
		akaoSetInstrument(_channels[voice].playerTrack, instrument2);
	}
}

#define OPCODE_SLIDE_U8(name, shift) \
	const uint16_t length = data[1] == 0 ? 256 : data[1]; \
	const uint32_t param = data[2] << shift; \
	name##_slide_length = length; \
	name##_slope = (param - name) / length;

#define OPCODE_SLIDE(name) \
	const uint16_t length = data[1] == 0 ? 256 : data[1]; \
	const uint32_t param = (data[3] << 24) | (data[2] << 16); \
	name##_slide_length = length; \
	name &= 0xFFFF0000; \
	name##_slope = (param - name) / length;

int AkaoExec::execute_channel(channel_t channel, int argument2)
{
	const uint8_t* data = curData(channel);

	if (data + 1 - _akao.data() > _akao.dataSize()) {
		return 0; // No more data
	}

	uint8_t op = data[0];
	AkaoPlayerTrack& playerTrack = _channels[channel].playerTrack;

	if (op < 0x9A) {

	}
	else {
		const uint8_t opcode_len = _opcode_len[op - 0xA0];

		if (data + opcode_len - _akao.data() > _akao.dataSize()) {
			return 0; // No more data
		}

		switch (op) {
		case 0xA0:
			return 0; // Finished
		case 0xA1:
			return loadInstrument(data[1]) ? opcode_len : 0;
		case 0xA2:
			_channels[channel].note_length = data[1];
			break;
		case 0xA3:
			_channels[channel].master_volume = data[1];
			break;
		case 0xA4:
			//_channels[channel].master_volume = data[1];
			break;
		case 0xA5:
			break;
		case 0xA6:
			break;
		case 0xA7:
			break;
		case 0xA8:
			break;
		case 0xA9:
			break;
		case 0xAA:
			break;
		case 0xAB:
			break;
		case 0xAC:
			break;
		case 0xAD:
			break;
		case 0xAE:
			break;
		case 0xAF:
			break;
		case 0xB0:
			break;
		case 0xB1:
			break;
		case 0xB2:
			break;
		case 0xB3:
			break;
		case 0xB4:
			break;
		case 0xB5:
			break;
		case 0xB6:
			break;
		case 0xB7:
			break;
		case 0xB8:
			break;
		case 0xB9:
			break;
		case 0xBA:
			break;
		case 0xBB:
			break;
		case 0xBC:
			break;
		case 0xBD:
			break;
		case 0xBE:
			break;
		case 0xBF:
			break;
		case 0xC0:
			break;
		case 0xC1:
			break;
		case 0xC2:
			break;
		case 0xC3:
			break;
		case 0xC4:
			break;
		case 0xC5:
			break;
		case 0xC6:
			break;
		case 0xC7:
			break;
		case 0xC8:
			break;
		case 0xC9:
			break;
		case 0xCA:
			break;
		case 0xCB:
			break;
		case 0xCC:
			break;
		case 0xCD:
			// Do nothing
			break;
		case 0xCE:
			break;
		case 0xCF:
			break;
		case 0xD0:
			break;
		case 0xD1:
			// Do nothing
			break;
		case 0xD2:
			break;
		case 0xD3:
			break;
		case 0xD4: // Turn On Playback Rate Side Chain
			playerTrack.voice_effect_flags |= AkaoVoiceEffectFlags::PlaybackRateSideChainEnabled;
			break;
		case 0xD5: // Turn Off Playback Rate Side Chain
			playerTrack.voice_effect_flags &= ~AkaoVoiceEffectFlags::PlaybackRateSideChainEnabled;
			break;
		case 0xD6: // Turn On Pitch-Volume Side Chain
			playerTrack.voice_effect_flags |= AkaoVoiceEffectFlags::PitchVolumeSideChainEnabled;
			break;
		case 0xD7: // Turn Off Pitch-Volume Side Chain
			playerTrack.voice_effect_flags &= ~AkaoVoiceEffectFlags::PitchVolumeSideChainEnabled;
			break;
		case 0xD8: // Channel fine tuning (absolute)
			const int8_t tuning = data[1];
			playerTrack.tuning = tuning;
			break;
		case 0xD9: // Channel fine tuning (relative)
			const int8_t tuning = data[1];
			playerTrack.tuning += tuning;
			break;
		case 0xDA: // Tun On Portamento
			const uint16_t speed = data[1] == 0 ? 256 : data[1];
			playerTrack.portamento_speed = speed;
			playerTrack.previous_transpose = 0;
			playerTrack.previous_note_number = 0;
			playerTrack.legato_flags = 1;
			break;
		case 0xDB: // Turn Off Portamento
			playerTrack.portamento_speed = 0;
			break;
		case 0xDC: // Fix Note Length
			int8_t length = data[1];
			if (length != 0) {
				int32_t len = int32_t(length) + playerTrack.previous_delta_time;
				if (len <= 0) {
					len = 1;
				}
				else if (len >= 256) {
					len = 255;
				}
				length = len;
			}
			playerTrack.forced_delta_time = length;
			break;
		case 0xDD: // Vibrato Depth Slide
			OPCODE_SLIDE_U8(playerTrack.vibrato_depth, 8)
			break;
		case 0xDE: // Tremolo Depth Slide
			OPCODE_SLIDE_U8(playerTrack.tremolo_depth, 8)
			break;
		case 0xDF: // Channel Pan LFO Depth Slide 
			OPCODE_SLIDE_U8(playerTrack.pan_lfo_depth, 7)
			break;
		case 0xE0:
		case 0xE1:
		case 0xE2:
		case 0xE3:
		case 0xE4:
		case 0xE5:
		case 0xE6:
		case 0xE7:
			return 0; // Unused
		case 0xE8: // Tempo
			const uint8_t tempo = (data[2] << 24) | (data[1] << 16);
			_player.tempo_slide_length = 0;
			_player.tempo = tempo;
			break;
		case 0xE9: // Tempo slide
			OPCODE_SLIDE(_player.tempo)
			break;
		case 0xEA: // Reverb depth
			const uint32_t depth = (data[2] << 24) | (data[1] << 16);
			_player.reverb_depth_slide_length = 0;
			_player.spucnt |= 0x80;
			_player.reverb_depth = depth;
			break;
		case 0xEB: // Reverb depth slide
			OPCODE_SLIDE(_player.reverb_depth)
			break;
		case 0xEC: // Turn On Drum Mode
			const int16_t offset = (data[2] << 8) | data[1];
			if (data + 2 + offset + sizeof(AkaoDrumKeyAttr) - _akao.data() > _akao.dataSize()) {
				return 0;
			}

			playerTrack.voice_effect_flags |= AkaoVoiceEffectFlags::DrumModeEnabled;
			playerTrack.drum_addr = reinterpret_cast<const AkaoDrumKeyAttr*>(data) + 2 + offset;
			break;
		case 0xED: // Turn Off Drum Mode
			playerTrack.voice_effect_flags &= ~AkaoVoiceEffectFlags::DrumModeEnabled;
			break;
		case 0xEE: // Unconditional Jump
			const int16_t offset = (data[2] << 8) | data[1];
			return opcode_len + offset;
		case 0xEF: // CPU Conditional Jump
			const uint8_t condition = data[1];
			if (_player.condition != 0 && condition <= _player.condition) {
				const int16_t offset = (data[3] << 8) | data[2];
				_player.condition_ack = condition;
				return opcode_len + offset;
			}
			return opcode_len;
		case 0xF0: // Jump on the Nth Repeat
			const uint16_t times = data[1] == 0 ? 256 : data[1];
			if (playerTrack.loop_counts[playerTrack.loop_layer] + 1 != times) {
				return opcode_len;
			}
			const int16_t offset = (data[3] << 8) | data[2];
			return opcode_len + offset;
		case 0xF1: // Break the loop on the nth repeat
			const uint16_t times = data[1] == 0 ? 256 : data[1];
			if (playerTrack.loop_counts[playerTrack.loop_layer] + 1 != times) {
				return opcode_len;
			}
			const int16_t offset = (data[3] << 8) | data[2];
			playerTrack.loop_layer = (playerTrack.loop_layer - 1) & 0x3;
			return opcode_len + offset;
		case 0xF2: // Load instrument (no attack sample)
			int instrument = data[1];
			AkaoInstrumentAttr instr = _instruments[instrument];
			if (playerTrack.field_54 != 0 || !((_player.keyed_voices & argument2) & _unknownA9FCC)) {
				playerTrack.update_flags |= 0x10;
				AkaoInstrumentAttr playerInstr = _instruments[playerTrack.instrument];
				if (playerInstr.pitch[0] == 0) {
					return 0;
				}
				playerTrack.pitch_of_note = uint32_t(uint64_t(playerTrack.pitch_of_note) * uint64_t(instr.pitch[0])) / playerInstr.pitch[0];
			}
			playerTrack.instrument = instrument;
			playerTrack.spu_addr = playerTrack.field_54 == 0 ? 0x70000 : 0x76FE0;
			playerTrack.loop_addr = instr.loop_addr;
			playerTrack.adsr_attack_rate = instr.adsr_attack_rate;
			playerTrack.adsr_decay_rate = instr.adsr_decay_rate;
			playerTrack.adsr_sustain_level = instr.adsr_sustain_level;
			playerTrack.adsr_sustain_rate = instr.adsr_sustain_rate;
			playerTrack.adsr_attack_mode = instr.adsr_attack_mode;
			playerTrack.adsr_sustain_mode = instr.adsr_sustain_mode;
			if (playerTrack.voice_effect_flags & AkaoVoiceEffectFlags::AlternateVoiceEnabled) {
				playerTrack.update_flags |= 0x1BB80;
			}
			else {
				playerTrack.adsr_release_rate = instr.adsr_release_rate;
				playerTrack.adsr_release_mode = instr.adsr_release_mode;
				playerTrack.update_flags |= 0x1FF80;
			}
			break;
		case 0xF3:
			_player.field_54 = 1;
			break;
		case 0xF4: // Turn On Overlay Voice
			turnOnOverlayVoice(data, playerTrack);
			break;
		case 0xF5: // Turn Off Overlay Voice
			int overlay_voice_num = playerTrack.overlay_voice_num;

			if (_unknown62F04 == 0) {
				overlay_voice_num -= 24;
			}

			if (playerTrack.voice_effect_flags & AkaoVoiceEffectFlags::OverlayVoiceEnabled) {
				playerTrack.voice_effect_flags &= ~AkaoVoiceEffectFlags::OverlayVoiceEnabled; // Remove
				playerTrack.overlay_voice_num &= ~(1 << overlay_voice_num); // Remove
			}
			break;
		case 0xF6: // Overlay Volume Balance
			const uint8_t balance = data[1];
			playerTrack.overlay_balance_slide_length = 0;
			playerTrack.overlay_balance = balance << 8;
			if (playerTrack.voice_effect_flags & AkaoVoiceEffectFlags::OverlayVoiceEnabled) {
				playerTrack.update_flags |= 0x3;
			}
			break;
		case 0xF7: // Overlay Volume Balance Slide
			OPCODE_SLIDE(playerTrack.overlay_balance)
			break;
		case 0xF8: // Turn On Alternate Voice
			playerTrack.adsr_release_rate = data[1];
			if (!(playerTrack.voice_effect_flags & AkaoVoiceEffectFlags::AlternateVoiceEnabled)) {
				uint8_t alternate_voice_num = 0;
				uint32_t alternate_voices = 1;

				while (alternate_voice_num < 24 && (_player.active_voices | _player.overlay_voices | _player.alternate_voices) & alternate_voices) {
					alternate_voices <<= 1;
					alternate_voice_num += 1;
				}

				if (alternate_voice_num < 24) {
					_player.alternate_voices = alternate_voices | _player.alternate_voices;
					playerTrack.alternate_voice_num = alternate_voice_num;
					playerTrack.voice_effect_flags |= AkaoVoiceEffectFlags::AlternateVoiceEnabled;
				}
			}
			break;
		case 0xF9: // Turn Off Alternate Voice
			_player.alternate_voices = !(1 << playerTrack.alternate_voice_num) & _player.alternate_voices;
			playerTrack.voice_effect_flags &= ~AkaoVoiceEffectFlags::AlternateVoiceEnabled; // Disable
			playerTrack.update_flags |= 0x4400;
			// load_u8(0x80075F34 + instr * 0x40):
			playerTrack.adsr_release_rate = _instruments[playerTrack.instrument].adsr_release_rate;
			break;
		case 0xFA:
		case 0xFB:
		case 0xFC:
			return 0; // Unused
		case 0xFD: // Time signature
			_player.ticks_per_beat = data[1];
			_player.tick = 0;
			_player.beat = 0;
			_player.beats_per_measure = data[2];
			break;
		case 0xFE: // Measure number
			_player.measure = (uint16_t(data[2]) << 8) | uint16_t(data[1]);
			break;
		case 0xFF:
			return 0; // Unused
		}

		return opcode_len;
	}

	return 1;
}

bool AkaoExec::reset()
{
	_akao.reverbType()
}

int32_t AkaoExec::execute(int16_t* buffer, uint16_t max_samples)
{
	bool finished = true;

	for (channel_t c = 0; c < _akao.channelCount(); ++c) {
		if (!_channels[c].finished) {
			if (!execute_channel(c, 0)) {
				_channels[c].finished = true;
			}
			else {
				finished = false;
			}
		}
	}
}

uint8_t AkaoExec::_opcode_len[0x60] = {
	0, 2, 2, 2, 3, 2, 1, 1,
	2, 3, 2, 3, 2, 2, 2, 2,

	3, 2, 2, 1, 4, 2, 1, 2,
	4, 2, 1, 2, 3, 2, 1, 2,

	2, 2, 1, 1, 1, 1, 1, 1,
	1, 0, 0, 0, 1, 0, 2, 2,

	1, 0, 2, 2, 1, 1, 1, 1,
	2, 2, 2, 1, 2, 3, 3, 3,

	0, 0, 0, 0, 0, 0, 0, 0,
	3, 4, 3, 4, 3, 1, 3, 4,

	4, 4, 2, 1, 3, 1, 2, 3,
	2, 1, 0, 0, 0, 3, 3, 0
};
