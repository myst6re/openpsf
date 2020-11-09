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

void AkaoExec::spuNoiseVoice()
{
	// TODO
}

void AkaoExec::spuReverbVoice()
{
	// TODO
}

void AkaoExec::spuPitchLFOVoice()
{
	// TODO
}

void AkaoExec::finishChannel(AkaoPlayerTrack& playerTrack, int argument2)
{
	if (playerTrack.use_global_track == 0) {
		int argument3 = argument2 ^ 0xFFFFFFFF;
		_player.active_voices &= argument3;
		if (_player.active_voices == 0) {
			_player.song_id = 0;
		}
		_player.noise_voices &= argument3;
		_player.pitch_lfo_voices &= argument3;
		_player.reverb_voices &= argument3;

		if (playerTrack.voice_effect_flags & AkaoVoiceEffectFlags::OverlayVoiceEnabled) {
			uint32_t overlay_voice_num = playerTrack.overlay_voice_num;
			if (_unknown62F04 != 0) {
				overlay_voice_num -= 24;
			}
			_player.overlay_voices &= ~(1 << overlay_voice_num);
		}

		if (playerTrack.voice_effect_flags & AkaoVoiceEffectFlags::AlternateVoiceEnabled) {
			_player.alternate_voices &= ~(1 << playerTrack.alternate_voice_num);
		}
	}
	else {
		int argument3 = argument2 ^ 0xFF0000;
		_unknown99FCC &= argument3;
		_noise_voices &= argument3;
		_reverb_voices &= argument3;
		_pitch_lfo_voices &= argument3;

		argument3 = ~argument2;
		_unknown9A10C &= argument3;
		_unknown9A110 &= argument3;
		_unknown9A114 &= argument3;
		_channels[playerTrack.alternate_voice_num].playerTrack.update_flags |= 0x1FF80;
	}
	playerTrack.voice_effect_flags = AkaoVoiceEffectFlags::None;
	_player.spucnt |= 0x10;
	this->spuNoiseVoice();
	this->spuReverbVoice();
	this->spuPitchLFOVoice();
}

bool AkaoExec::loadInstrument(const uint8_t *data, AkaoPlayerTrack& playerTrack, int argument2)
{
	const uint8_t instrument = data[1];
	uint16_t overlay_voice_num = playerTrack.overlay_voice_num;

	if (_unknown62F04 != 0) {
		overlay_voice_num -= 24;
	}

	if (playerTrack.voice_effect_flags & AkaoVoiceEffectFlags::OverlayVoiceEnabled) {
		_player.overlay_voices &= ~(1 << overlay_voice_num);
		playerTrack.voice_effect_flags &= ~AkaoVoiceEffectFlags::OverlayVoiceEnabled;
	}

	if (playerTrack.use_global_track != 0 || !((_player.keyed_voices & argument2) & _unknown99FCC)) {
		playerTrack.update_flags |= 0x10;
		if (_instruments[playerTrack.instrument].pitch[0] == 0) {
			return false;
		}
		playerTrack.pitch_of_note = uint32_t(uint64_t(playerTrack.pitch_of_note) * uint64_t(_instruments[instrument].pitch[0])) / _instruments[playerTrack.instrument].pitch[0];
	}

	if (playerTrack.voice_effect_flags & AkaoVoiceEffectFlags::AlternateVoiceEnabled) {
		// Set instrument without release
		const AkaoInstrumentAttr& instr = _instruments[instrument];
		playerTrack.instrument = instrument;
		playerTrack.spu_addr = instr.addr;
		playerTrack.loop_addr = instr.loop_addr;
		playerTrack.adsr_attack_mode = instr.adsr_attack_mode;
		playerTrack.adsr_sustain_mode = instr.adsr_sustain_mode;
		playerTrack.adsr_attack_rate = instr.adsr_attack_rate;
		playerTrack.adsr_decay_rate = instr.adsr_decay_rate;
		playerTrack.adsr_sustain_level = instr.adsr_sustain_level;
		playerTrack.adsr_sustain_rate = instr.adsr_sustain_rate;
		playerTrack.update_flags |= 0x1BB80;
	}
	else {
		akaoSetInstrument(playerTrack, instrument);
	}

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

void AkaoExec::turnOnReverb(AkaoPlayerTrack& playerTrack, int argument2)
{
	if (playerTrack.use_global_track == 0) {
		_player.reverb_voices |= argument2;
	}
	else {
		_reverb_voices |= argument2;
	}
	this->spuReverbVoice();
}

void AkaoExec::turnOffReverb(AkaoPlayerTrack& playerTrack, int argument2)
{
	if (playerTrack.use_global_track == 0) {
		_player.reverb_voices &= ~argument2;
	}
	else {
		_reverb_voices &= ~argument2;
	}
	this->spuReverbVoice();
}

void AkaoExec::turnOnNoise(AkaoPlayerTrack& playerTrack, int argument2)
{
	if (playerTrack.use_global_track == 0) {
		_player.noise_voices |= argument2;
	}
	else {
		_noise_voices |= argument2;
	}
	_player.spucnt |= 0x10;
	this->spuNoiseVoice();
}

void AkaoExec::turnOffNoise(AkaoPlayerTrack& playerTrack, int argument2)
{
	if (playerTrack.use_global_track == 0) {
		_player.noise_voices &= ~argument2;
	}
	else {
		_noise_voices &= ~argument2;
	}
	_player.spucnt |= 0x10;
	this->spuNoiseVoice();
	playerTrack.noise_on_off_delay_counter = 0;
}

void AkaoExec::turnOnFrequencyModulation(AkaoPlayerTrack& playerTrack, int argument2)
{
	if (playerTrack.use_global_track == 0) {
		_player.pitch_lfo_voices |= argument2;
	}
	else if (argument2 & 0x555555 == 0) {
		_pitch_lfo_voices |= argument2;
	}
	this->spuPitchLFOVoice();
}

void AkaoExec::turnOffFrequencyModulation(AkaoPlayerTrack& playerTrack, int argument2)
{
	if (playerTrack.use_global_track == 0) {
		_player.pitch_lfo_voices &= ~argument2;
	}
	else {
		_pitch_lfo_voices &= ~argument2;
	}
	this->spuPitchLFOVoice();
	playerTrack.pitchmod_on_off_delay_counter = 0;
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

#define OPCODE_SLIDE_U16(name, shift) \
	const uint16_t length = data[1] == 0 ? 256 : data[1]; \
	const uint16_t param = data[2] << shift; \
	name##_slide_length = length; \
	name##_slope = (param - name) / length;

#define OPCODE_SLIDE(name) \
	const uint16_t length = data[1] == 0 ? 256 : data[1]; \
	const uint32_t param = (data[3] << 24) | (data[2] << 16); \
	name##_slide_length = length; \
	name &= 0xFFFF0000; \
	name##_slope = (param - name) / length;

#define OPCODE_UPDATE_DELAY_COUNTER(name) \
	const uint8_t counter = data[1] == 0 ? 256 : data[1]; \
	playerTrack.name##_on_off_delay_counter = counter + 1;

int AkaoExec::execute_channel(channel_t channel, int argument2)
{
	const uint8_t* data = curData(channel);

	if (data + 1 - _akao.data() > _akao.dataSize()) {
		return -2; // No more data
	}

	uint8_t op = data[0];
	AkaoPlayerTrack& playerTrack = _channels[channel].playerTrack;

	if (op < 0x9A) {

	}
	else {
		const uint8_t opcode_len = _opcode_len[op - 0xA0];

		if (data + opcode_len - _akao.data() > _akao.dataSize()) {
			return -2; // No more data
		}

		switch (op) {
		case 0xA0:
			finishChannel(playerTrack, argument2);
			return -1; // Finished
		case 0xA1:
			if (!loadInstrument(data, playerTrack, argument2)) {
				return -2;
			}
			break;
		case 0xA2: // Overwrite Next Note Length
			const uint8_t length = data[1];
			playerTrack.forced_delta_time = 0;
			playerTrack.delta_time_counter = length;
			playerTrack.gate_time_counter = length;
			playerTrack.previous_delta_time = length;
			break;
		case 0xA3: // Channel Master Volume
			const uint8_t master_volume = data[1];
			playerTrack.update_flags |= 0x3;
			playerTrack.master_volume = uint32_t(master_volume);
			break;
		case 0xA4: // Pitch Bend Slide
			// TODO
			break;
		case 0xA5: // Set Octave
			const uint8_t octave = data[1];
			playerTrack.octave = uint16_t(octave);
			break;
		case 0xA6: // Increase Octave
			playerTrack.octave = (playerTrack.octave + 1) & 0xF;
			break;
		case 0xA7: // Decrease Octave
			playerTrack.octave = (playerTrack.octave - 1) & 0xF;
			break;
		case 0xA8: // Channel Volume
			const uint8_t volume = data[1];
			playerTrack.volume_slide_length_counter = 0;
			playerTrack.update_flags |= 0x3;
			playerTrack.volume = uint32_t(volume) << 23;
			break;
		case 0xA9: // Channel Volume Slide
			// TODO
			break;
		case 0xAA: // Set Channel Pan
			const uint8_t pan = data[1];
			playerTrack.pan_slide_length = 0;
			playerTrack.update_flags |= 0x3;
			playerTrack.pan = pan << 8;
			break;
		case 0xAB: // Set Channel Pan Slide
			// TODO
			break;
		case 0xAC: // Noise Clock Frequency
			// TODO
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
		case 0xB6: // Turn Off Vibrato
			playerTrack.vibrato_lfo_amplitude = 0;
			playerTrack.voice_effect_flags &= ~AkaoVoiceEffectFlags::VibratoEnabled;
			playerTrack.update_flags |= 0x10;
			break;
		case 0xB7:
			break;
		case 0xB8:
			break;
		case 0xB9: // Tremolo Depth
			const uint8_t tremolo_depth = data[1];
			playerTrack.tremolo_depth = tremolo_depth << 8;
			break;
		case 0xBA: // Turn Off Tremolo
			playerTrack.tremolo_lfo_amplitude = 0;
			playerTrack.voice_effect_flags &= ~AkaoVoiceEffectFlags::TremoloEnabled;
			playerTrack.update_flags |= 0x3;
			break;
		case 0xBB: // ADSR Sustain Mode
			const uint8_t adsr_sustain_mode = data[1];
			playerTrack.update_flags |= 0x200;
			playerTrack.adsr_sustain_mode = adsr_sustain_mode;
			if (playerTrack.voice_effect_flags & AkaoVoiceEffectFlags::OverlayVoiceEnabled) {
				_channels[playerTrack.overlay_voice_num].playerTrack.adsr_sustain_mode = adsr_sustain_mode;
			}
			break;
		case 0xBC: // Channel Pan LFO
			const uint8_t pan_lfo_rate = data[1] == 0 ? 256 : data[1];
			const uint8_t pan_lfo_form = data[2];
			playerTrack.voice_effect_flags |= AkaoVoiceEffectFlags::ChannelPanLfoENabled;
			playerTrack.pan_lfo_rate = pan_lfo_rate;
			playerTrack.pan_lfo_form = pan_lfo_form;
			playerTrack.pan_lfo_rate_counter = 1;
			playerTrack.pan_lfo_addr = _pan_lfo_addrs[pan_lfo_form];
			break;
		case 0xBD: // Channel Pan LFO Depth
			const uint8_t pan_lfo_depth = data[1];
			playerTrack.pan_lfo_depth = pan_lfo_depth << 7;
			break;
		case 0xBE: // Turn Off Channel Pan LFO
			playerTrack.pan_lfo_amplitude = 0;
			playerTrack.voice_effect_flags &= ~AkaoVoiceEffectFlags::ChannelPanLfoENabled;
			playerTrack.update_flags |= 0x3;
			break;
		case 0xBF: // ADSR Release Mode
			const uint8_t adsr_release_mode = data[1];
			playerTrack.update_flags |= 0x400;
			playerTrack.adsr_release_mode = adsr_release_mode;
			if (playerTrack.voice_effect_flags & AkaoVoiceEffectFlags::OverlayVoiceEnabled) {
				_channels[playerTrack.overlay_voice_num].playerTrack.adsr_release_mode = adsr_release_mode;
			}
			break;
		case 0xC0: // Channel Transpose Absolute
			const int8_t transpose = int8_t(data[1]);
			playerTrack.transpose = int16_t(transpose);
			break;
		case 0xC1: // Channel Transpose Relative
			const int8_t relative = int8_t(data[1]);
			playerTrack.transpose += relative;
			break;
		case 0xC2:
			turnOnReverb(playerTrack, argument2);
			break;
		case 0xC3:
			turnOffReverb(playerTrack, argument2);
			break;
		case 0xC4:
			turnOnNoise(playerTrack, argument2);
			break;
		case 0xC5:
			turnOffNoise(playerTrack, argument2);
			break;
		case 0xC6:
			turnOnFrequencyModulation(playerTrack, argument2);
			break;
		case 0xC7:
			turnOffFrequencyModulation(playerTrack, argument2);
			break;
		case 0xC8: // Loop Point
			playerTrack.loop_layer = (playerTrack.loop_layer + 1) & 0x3;
			playerTrack.loop_addrs[playerTrack.loop_layer] = playerTrack.addr;
			playerTrack.loop_counts[playerTrack.loop_layer] = 0;
			break;
		case 0xC9: // Return To Loop Point Up to N Times
			const uint8_t loop = data[1] == 0 ? 256 : data[1];
			playerTrack.loop_counts[playerTrack.loop_layer] += 1;
			if (playerTrack.loop_counts[playerTrack.loop_layer] != loop) {
				return playerTrack.loop_addrs[playerTrack.loop_layer];
			}
			playerTrack.loop_layer = (playerTrack.loop_layer - 1) & 0x3;
			break;
		case 0xCA: // Return To Loop Point
			playerTrack.loop_counts[playerTrack.loop_layer] += 1;
			return playerTrack.loop_addrs[playerTrack.loop_layer];
		case 0xCB: // Reset Sound Effects
			playerTrack.voice_effect_flags &= ~(
				  AkaoVoiceEffectFlags::VibratoEnabled
				| AkaoVoiceEffectFlags::TremoloEnabled
				| AkaoVoiceEffectFlags::ChannelPanLfoENabled
				| AkaoVoiceEffectFlags::PlaybackRateSideChainEnabled
				| AkaoVoiceEffectFlags::PitchVolumeSideChainEnabled
			);
			turnOffNoise(playerTrack, argument2);
			turnOffFrequencyModulation(playerTrack, argument2);
			turnOffReverb(playerTrack, argument2);
			playerTrack.legato_flags &= ~(0x4 | 0x1);
			break;
		case 0xCC: // Turn On Legato
			playerTrack.legato_flags = 0x1;
			break;
		case 0xCD:
			// Do nothing
			break;
		case 0xCE: // Turn On Noise and Toggle Noise On/Off after a Period of Time
			OPCODE_UPDATE_DELAY_COUNTER(noise);
			turnOnNoise(playerTrack, argument2);
			break;
		case 0xCF: // Toggle Noise On/Off after a Period of Time
			OPCODE_UPDATE_DELAY_COUNTER(noise);
			break;
		case 0xD0: // Turn On Full-Length Note Mode (?)
			playerTrack.legato_flags = 0x4;
			break;
		case 0xD1:
			// Do nothing
			break;
		case 0xD2: // Toggle Freq Modulation Later and Turn On Frequency Modulation
			OPCODE_UPDATE_DELAY_COUNTER(pitchmod);
			turnOnFrequencyModulation(playerTrack, argument2);
			break;
		case 0xD3: // Toggle Freq Modulation Later
			OPCODE_UPDATE_DELAY_COUNTER(pitchmod);
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
		{
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
		}
			break;
		case 0xDD: // Vibrato Depth Slide
			OPCODE_SLIDE_U16(playerTrack.vibrato_depth, 8);
			break;
		case 0xDE: // Tremolo Depth Slide
			OPCODE_SLIDE_U16(playerTrack.tremolo_depth, 8);
			break;
		case 0xDF: // Channel Pan LFO Depth Slide 
			OPCODE_SLIDE_U16(playerTrack.pan_lfo_depth, 7);
			break;
		case 0xE0:
		case 0xE1:
		case 0xE2:
		case 0xE3:
		case 0xE4:
		case 0xE5:
		case 0xE6:
		case 0xE7:
			finishChannel(playerTrack, argument2);
			return -1; // Unused
		case 0xE8: // Tempo
			const uint8_t tempo = (data[2] << 24) | (data[1] << 16);
			_player.tempo_slide_length = 0;
			_player.tempo = tempo;
			break;
		case 0xE9: // Tempo slide
			OPCODE_SLIDE(_player.tempo);
			break;
		case 0xEA: // Reverb depth
			const uint32_t depth = (data[2] << 24) | (data[1] << 16);
			_player.reverb_depth_slide_length = 0;
			_player.spucnt |= 0x80;
			_player.reverb_depth = depth;
			break;
		case 0xEB: // Reverb depth slide
			OPCODE_SLIDE(_player.reverb_depth);
			break;
		case 0xEC: // Turn On Drum Mode
			const int16_t offset = (data[2] << 8) | data[1];
			if (data + 2 + offset + sizeof(AkaoDrumKeyAttr) - _akao.data() > _akao.dataSize()) {
				return -2;
			}

			playerTrack.voice_effect_flags |= AkaoVoiceEffectFlags::DrumModeEnabled;
			playerTrack.drum_addr = reinterpret_cast<const AkaoDrumKeyAttr*>(data) + 2 + offset;
			break;
		case 0xED: // Turn Off Drum Mode
			playerTrack.voice_effect_flags &= ~AkaoVoiceEffectFlags::DrumModeEnabled;
			break;
		case 0xEE: // Unconditional Jump
			const int16_t offset = (data[2] << 8) | data[1];
			return playerTrack.addr + opcode_len + offset;
		case 0xEF: // CPU Conditional Jump
			const uint8_t condition = data[1];
			if (_player.condition != 0 && condition <= _player.condition) {
				const int16_t offset = (data[3] << 8) | data[2];
				_player.condition_ack = condition;
				return playerTrack.addr + opcode_len + offset;
			}
			return playerTrack.addr + opcode_len;
		case 0xF0: // Jump on the Nth Repeat
			const uint16_t times = data[1] == 0 ? 256 : data[1];
			if (playerTrack.loop_counts[playerTrack.loop_layer] + 1 != times) {
				return playerTrack.addr + opcode_len;
			}
			const int16_t offset = (data[3] << 8) | data[2];
			return playerTrack.addr + opcode_len + offset;
		case 0xF1: // Break the loop on the nth repeat
			const uint16_t times = data[1] == 0 ? 256 : data[1];
			if (playerTrack.loop_counts[playerTrack.loop_layer] + 1 != times) {
				return playerTrack.addr + opcode_len;
			}
			const int16_t offset = (data[3] << 8) | data[2];
			playerTrack.loop_layer = (playerTrack.loop_layer - 1) & 0x3;
			return playerTrack.addr + opcode_len + offset;
		case 0xF2: // Load instrument (no attack sample)
			int instrument = data[1];
			AkaoInstrumentAttr instr = _instruments[instrument];
			if (playerTrack.use_global_track != 0 || !((_player.keyed_voices & argument2) & _unknown99FCC)) {
				playerTrack.update_flags |= 0x10;
				AkaoInstrumentAttr playerInstr = _instruments[playerTrack.instrument];
				if (playerInstr.pitch[0] == 0) {
					return -2;
				}
				playerTrack.pitch_of_note = uint32_t(uint64_t(playerTrack.pitch_of_note) * uint64_t(instr.pitch[0])) / playerInstr.pitch[0];
			}
			playerTrack.instrument = instrument;
			playerTrack.spu_addr = playerTrack.use_global_track == 0 ? 0x70000 : 0x76FE0;
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
			playerTrack.overlay_balance & 0xFF00;
			OPCODE_SLIDE_U16(playerTrack.overlay_balance, 8);
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
			playerTrack.adsr_release_rate = _instruments[playerTrack.instrument].adsr_release_rate;
			break;
		case 0xFA:
		case 0xFB:
		case 0xFC:
			finishChannel(playerTrack, argument2);
			return -1; // Unused
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
			finishChannel(playerTrack, argument2);
			return -1; // Unused
		}

		return playerTrack.addr + opcode_len;
	}

	return playerTrack.addr + 1;
}

bool AkaoExec::reset()
{
	_akao.reverbType();
}

int32_t AkaoExec::execute(int16_t* buffer, uint16_t max_samples)
{
	bool finished = true;

	for (channel_t c = 0; c < _akao.channelCount(); ++c) {
		if (!_channels[c].finished) {
			int new_addr = execute_channel(c, 0);
			if (new_addr <= 0) {
				_channels[c].finished = true;
			}
			else {
				_channels[c].playerTrack.addr = new_addr;
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
	1, 1, 0, 0, 1, 0, 2, 2,

	1, 0, 2, 2, 1, 1, 1, 1,
	2, 2, 2, 1, 2, 3, 3, 3,

	0, 0, 0, 0, 0, 0, 0, 0,
	3, 4, 3, 4, 3, 1, 3, 4,

	4, 4, 2, 1, 3, 1, 2, 3,
	2, 1, 0, 0, 0, 3, 3, 0
};

uint32_t AkaoExec::_pan_lfo_addrs[PAN_LFO_ADDRS_LEN] = {
	0x8004A044, // 0x1FFF
	0x8004A05C, // 0x7FFF
	// TODO
};
