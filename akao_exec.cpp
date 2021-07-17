#include "akao_exec.h"
#include "include/LIBETC.h"

#define OPCODE_ADSR_UPDATE(name, upd_flags) \
	const uint8_t param = playerTrack.addr[1]; \
	playerTrack.spuRegisters.update_flags |= upd_flags; \
	playerTrack.spuRegisters.name = param; \
	if (playerTrack.voice_effect_flags & AkaoVoiceEffectFlags::OverlayVoiceEnabled) { \
		_playerTracks[playerTrack.overlay_voice_num].spuRegisters.name = param; \
	}

AkaoExec::AkaoExec(const Akao& akao, AkaoInstr& instr, AkaoInstr& instr2 = AkaoInstr()) noexcept :
	_akao(akao)
{
	SpuInit();
	const int spuInitNum = 4;
	memset(&_spuMemoryManagementTable[0], 0, SPU_MALLOC_RECSIZ * (spuInitNum + 1));
	SpuInitMalloc(spuInitNum, &_spuMemoryManagementTable[0]);
	SpuMallocWithStartAddr(0x77000, 8192);
	SpuSetTransferMode(SPU_TRANSFER_BY_DMA);
	akaoLoadInstrumentSet(instr);
	SpuSetTransferStartAddr(0x76FE0);
	akaoTransferSamples(_empty_adpcm, EMPTY_ADPCM_LEN);
	akaoWaitForSpuTransfer();
	akaoReset();
	if (instr2.adpcm.data() != nullptr) {
		akaoLoadInstrumentSet2(instr2);
	}
	akaoSetReverbMode(_akao.reverbType());
	akaoPlayMusic();
}

int32_t AkaoExec::decode(int16_t* buffer, uint16_t max_samples) noexcept
{
	uint32_t samples = 0;

	while (samples < max_samples) {
		//akaoMain(); // TODO
	}

	return samples;
}

void AkaoExec::akaoLoadInstrumentSet(AkaoInstr& instr)
{
	SpuSetTransferStartAddr(instr.adpcm.spuAddr()); // 0x1010
	akaoTransferSamples(instr.adpcm.data(), instr.adpcm.dataSize()); // Size: 0x75F90
	memcpy(&_instruments[0], &instr.attrs[0], sizeof(AkaoInstrAttr) * MAX_INSTRUMENTS);
	akaoWaitForSpuTransfer();
}

void AkaoExec::akaoLoadInstrumentSet2(AkaoInstr& instr)
{
	SpuSetTransferStartAddr(instr.adpcm.spuAddr()); // 0x38560
	akaoTransferSamples(instr.adpcm.data(), instr.adpcm.dataSize()); // Size: 0x3D4E0
	memcpy(&_instruments2[0], &instr.attrs[0], sizeof(AkaoInstrAttr) * MAX_INSTRUMENTS);
	akaoWaitForSpuTransfer();
}

// global_pointer + 0xC4
bool akaoSpuWaitForTransfer = false;

void akaoSpuTransferCallback()
{
	SpuSetTransferCallback(nullptr);
	akaoSpuWaitForTransfer = false;
}

void AkaoExec::akaoSpuSetTransferCallback()
{
	SpuTransferCallbackProc proc = akaoSpuTransferCallback;
	akaoSpuWaitForTransfer = true;
	SpuSetTransferCallback(proc);
}

void AkaoExec::akaoTransferSamples(uint8_t* data, uint32_t size)
{
	akaoSpuSetTransferCallback();
	SpuWrite(data, size);
}

void AkaoExec::akaoWaitForSpuTransfer()
{
	while (akaoSpuWaitForTransfer) {}
}

void AkaoExec::akaoSetReverbMode(uint8_t reverbType)
{
	akaoClearSpu();
	SpuGetReverbModeParam(&_spuReverbAttr);

	if (_spuReverbAttr.mode != reverbType) {
		_player.reverb_type = reverbType;

		SpuSetReverb(SPU_OFF);

		_spuReverbAttr.mask = SPU_REV_MODE;
		_spuReverbAttr.mode = reverbType | SPU_REV_MODE_CLEAR_WA;
		SpuSetReverbModeParam(&_spuReverbAttr);

		SpuSetReverb(SPU_ON);
	}
}

void AkaoExec::akaoClearSpu()
{
	SpuSetTransferCallback(nullptr);
	SpuSetIRQ(SPU_OFF);
	SpuSetIRQCallback(nullptr);
	SpuSetKey(SPU_OFF, _spuActiveVoices);

	if (_spuActiveVoices & 0x10000) {
		_playerTracks[16].spuRegisters.update_flags = AkaoUpdateFlags::VolumeLeft | AkaoUpdateFlags::VolumeRight
			| AkaoUpdateFlags::VoicePitch | AkaoUpdateFlags::VoiceStartAddr
			| AkaoUpdateFlags::AdsrAttackMode | AkaoUpdateFlags::AdsrSustainMode
			| AkaoUpdateFlags::AdsrReleaseMode | AkaoUpdateFlags::AdsrAttackRate
			| AkaoUpdateFlags::AdsrDecayRate | AkaoUpdateFlags::AdsrSustainRate
			| AkaoUpdateFlags::AdsrReleaseRate | AkaoUpdateFlags::AdsrSustainLevel
			| AkaoUpdateFlags::VoiceLoopStartAddr;
	}

	if (_spuActiveVoices & 0x20000) {
		_playerTracks[17].spuRegisters.update_flags = AkaoUpdateFlags::VolumeLeft | AkaoUpdateFlags::VolumeRight
			| AkaoUpdateFlags::VoicePitch | AkaoUpdateFlags::VoiceStartAddr
			| AkaoUpdateFlags::AdsrAttackMode | AkaoUpdateFlags::AdsrSustainMode
			| AkaoUpdateFlags::AdsrReleaseMode | AkaoUpdateFlags::AdsrAttackRate
			| AkaoUpdateFlags::AdsrDecayRate | AkaoUpdateFlags::AdsrSustainRate
			| AkaoUpdateFlags::AdsrReleaseRate | AkaoUpdateFlags::AdsrSustainLevel
			| AkaoUpdateFlags::VoiceLoopStartAddr;
	}

	_spuActiveVoices = 0;
	spuReverbVoice();
	spuPitchLFOVoice();
	spuNoiseVoice();
}

int AkaoExec::akaoCalculateVolumeAndPitch(AkaoPlayerTrack& playerTracks)
{
	uint32_t right_volume = ((playerTracks.volume >> 16) * playerTracks.master_volume) >> 7;

	if (playerTracks.voice_effect_flags & AkaoVoiceEffectFlags::VibratoEnabled
		&& playerTracks.vibrato_delay_counter == 0) {
		playerTracks.vibrato_rate_counter -= 1;
		if (playerTracks.vibrato_rate_counter == 0) {
			playerTracks.vibrato_rate_counter = playerTracks.vibrato_rate;
			if (*((uint32_t*)playerTracks.vibrato_lfo_addr) == 0) {
				playerTracks.vibrato_lfo_addr += *(playerTracks.vibrato_lfo_addr + 2) * 2;
			}
			int16_t vibrato_lfo_amplitude = (playerTracks.vibrato_max_amplitude * (*playerTracks.vibrato_lfo_addr)) >> 16;
			playerTracks.vibrato_lfo_addr += 1;
			if (vibrato_lfo_amplitude != playerTracks.vibrato_lfo_amplitude) {
				playerTracks.vibrato_lfo_amplitude = vibrato_lfo_amplitude;
				playerTracks.spuRegisters.update_flags |= AkaoUpdateFlags::VoicePitch;
				if (vibrato_lfo_amplitude >= 0) {
					playerTracks.vibrato_lfo_amplitude *= 2;
				}
			}
		}
	}
	if (playerTracks.voice_effect_flags & AkaoVoiceEffectFlags::TremoloEnabled
		&& playerTracks.tremolo_delay_counter == 0) {
		playerTracks.tremolo_rate_counter -= 1;
		if (playerTracks.tremolo_rate_counter == 0) {
			playerTracks.tremolo_rate_counter = playerTracks.tremolo_rate;
			if (*((uint32_t*)playerTracks.tremolo_lfo_addr) == 0) {
				playerTracks.tremolo_lfo_addr += *(playerTracks.tremolo_lfo_addr + 2) << 1;
			}
			((((right_volume * (playerTracks.tremolo_depth >> 8)) << 9) >> 16) * (*playerTracks.tremolo_lfo_addr)) >> 15;
			playerTracks.tremolo_lfo_addr += 1;
		}

	}
	// TODO

}

void AkaoExec::akaoDspMain()
{
	uint32_t spu_key = 0;

	if (_player.spucnt & SpuCnt::SetReverbDepth) {
		uint32_t reverb_depth = _player.reverb_depth >> 16;
		_spuReverbAttr.mask = SPU_REV_DEPTHL | SPU_REV_DEPTHR;
		uint32_t low = reverb_depth * _unknownReverbRelated62FB8;
		if (_unknownReverbRelated62FB8 < 128) {
			reverb_depth += low >> 7;
		}
		else {
			reverb_depth = low >> 8;
		}

		if (_reverb_position < 64) {
			_spuReverbAttr.depth.left = reverb_depth;
			_spuReverbAttr.depth.right = reverb_depth - ((reverb_depth * (_reverb_position ^ 0x3F)) >> 6);
		}
		else {
			_spuReverbAttr.depth.right = reverb_depth;
			_spuReverbAttr.depth.left = reverb_depth - ((reverb_depth * (_reverb_position & 0x3F)) >> 6);
		}

		SpuSetReverbDepth(&_spuReverbAttr);

		_player.spucnt ^= SpuCnt::SetReverbDepth;
	}

	if (_player.spucnt & SpuCnt::SetNoiseClock) {
		SpuSetNoiseClock(_active_voices ? _noise_clock : _player.noise_clock);

		_player.spucnt ^= SpuCnt::SetNoiseClock;
	}

	if (_player.active_voices) {
		const uint32_t global_active_voices = ~(_voicesMask62F68 | _active_voices | _spuActiveVoices);
		const uint32_t active_voices = _player.active_voices & (global_active_voices & _player.keyed_voices);
		spu_key |= global_active_voices & _player.key_on_voices;

		for (channel_t voice = 0; voice < AKAO_CHANNEL_MAX; ++voice) {
			const uint32_t voice_mask = 1 << voice;
			if (active_voices & voice_mask) {
				akaoCalculateVolumeAndPitch(_playerTracks[voice], voice_mask, voice);
				if (int(_playerTracks[voice].spuRegisters.update_flags) != 0) {
					if (_unknown62FD8 & voice_mask) {
						_playerTracks[voice].spuRegisters.volume_left = 0;
						_playerTracks[voice].spuRegisters.volume_right = 0;
					}
					if (_playerTracks[voice].voice_effect_flags & AkaoVoiceEffectFlags::OverlayVoiceEnabled) {
						akaoDspOverlayVoice(_playerTracks[voice], global_active_voices, _playerTracks[voice].overlay_voice_num - 24);
						continue;
					}
					if (_playerTracks[voice].voice_effect_flags & AkaoVoiceEffectFlags::AlternateVoiceEnabled) {
						if (_player.key_on_voices & voice_mask) {
							_playerTracks[voice].voice_effect_flags ^= AkaoVoiceEffectFlags::Unknown400;
							_playerTracks[voice].spuRegisters.update_flags |= AkaoUpdateFlags::VolumeLeft | AkaoUpdateFlags::VolumeRight
								| AkaoUpdateFlags::VoicePitch | AkaoUpdateFlags::VoiceStartAddr
								| AkaoUpdateFlags::AdsrAttackMode | AkaoUpdateFlags::AdsrSustainMode
								| AkaoUpdateFlags::AdsrReleaseMode | AkaoUpdateFlags::AdsrAttackRate
								| AkaoUpdateFlags::AdsrDecayRate | AkaoUpdateFlags::AdsrSustainRate
								| AkaoUpdateFlags::AdsrReleaseRate | AkaoUpdateFlags::AdsrSustainLevel
								| AkaoUpdateFlags::VoiceLoopStartAddr;
						}
						if (_playerTracks[voice].voice_effect_flags & AkaoVoiceEffectFlags::Unknown400) {
							if (global_active_voices & (1 << _playerTracks[voice].alternate_voice_num)) {
								akaoWriteSpuRegisters(_playerTracks[voice].alternate_voice_num, _playerTracks[voice].spuRegisters);
								if (spu_key & voice_mask) {
									spu_key = (spu_key | (1 << _playerTracks[voice].alternate_voice_num)) & ~voice_mask;
								}
							}
							continue;
						}
					}
					akaoWriteSpuRegisters(_playerTracks[voice].spuRegisters.voice, _playerTracks[voice].spuRegisters);
				}
			}
		}

		_player.key_on_voices = 0;
	}

	if (spu_key) {
		SpuSetKey(SPU_ON, spu_key);
	}
}

void AkaoExec::akaoDspOverlayVoice(AkaoPlayerTrack& playerTracks, uint32_t active_voices, uint32_t voice)
{
	AkaoPlayerTrack& playerTracksOverlay = _playerTracks[playerTracks.overlay_voice_num];

	uint16_t balance = (127 - (playerTracks.overlay_balance >> 8)) & 0xFFFF;
	playerTracksOverlay.spuRegisters.volume_left = (playerTracks.spuRegisters.volume_left * playerTracks.overlay_balance) >> 16;
	playerTracks.spuRegisters.volume_left = (playerTracks.spuRegisters.volume_left * balance) >> 8;
	playerTracksOverlay.spuRegisters.volume_right = (playerTracks.spuRegisters.volume_right * playerTracks.overlay_balance) >> 16;
	playerTracks.spuRegisters.volume_right = (playerTracks.spuRegisters.volume_right * balance) >> 8;

	playerTracksOverlay.spuRegisters.voice_pitch = playerTracks.spuRegisters.voice_pitch;
	playerTracksOverlay.spuRegisters.update_flags |= playerTracks.spuRegisters.update_flags;

	akaoWriteSpuRegisters(playerTracks.spuRegisters.voice, playerTracks.spuRegisters);

	if (active_voices & (1 << voice)) {
		akaoWriteSpuRegisters(voice, playerTracksOverlay.spuRegisters);
	}
}

void AkaoExec::akaoDspOnTick(AkaoPlayerTrack& playerTracks, AkaoPlayer& player, uint32_t voice)
{
	if (playerTracks.volume_slide_length != 0) {
		const uint32_t volume = playerTracks.volume, newVolume = playerTracks.volume_slope + volume;
		playerTracks.volume_slide_length -= 1;
		if (newVolume & 0xFFE00000 != volume & 0xFFE00000) {
			playerTracks.spuRegisters.update_flags |= AkaoUpdateFlags::VolumeLeft | AkaoUpdateFlags::VolumeRight;
		}
		playerTracks.volume = newVolume;
	}

	if (playerTracks.overlay_balance_slide_length != 0) {
		const uint32_t balance = playerTracks.overlay_balance, newBalance = playerTracks.overlay_balance_slope + balance;
		playerTracks.overlay_balance_slide_length -= 1;
		if (playerTracks.voice_effect_flags & AkaoVoiceEffectFlags::OverlayVoiceEnabled
			&& newBalance & 0xFF00 != balance & 0xFF00) {
			playerTracks.spuRegisters.update_flags |= AkaoUpdateFlags::VolumeLeft | AkaoUpdateFlags::VolumeRight;
		}
		playerTracks.overlay_balance = newBalance;
	}

	if (playerTracks.pan_slide_length != 0) {
		const uint32_t pan = playerTracks.pan, newPan = pan + playerTracks.pan_slope;
		playerTracks.pan_slide_length -= 1;
		if (newPan & 0xFF00 != pan & 0xFF00) {
			playerTracks.spuRegisters.update_flags |= AkaoUpdateFlags::VolumeLeft | AkaoUpdateFlags::VolumeRight;
		}
		playerTracks.pan = newPan;
	}

	if (playerTracks.vibrato_delay_counter != 0) {
		playerTracks.vibrato_delay_counter -= 1;
	}

	if (playerTracks.tremolo_delay_counter != 0) {
		playerTracks.tremolo_delay_counter -= 1;
	}

	if (playerTracks.noise_on_off_delay_counter != 0) {
		playerTracks.noise_on_off_delay_counter -= 1;
		if (playerTracks.noise_on_off_delay_counter == 0) {
			player.noise_voices ^= voice;
			player.spucnt |= SpuCnt::SetNoiseClock;
			spuNoiseVoice();
		}
	}

	if (playerTracks.pitchmod_on_off_delay_counter != 0) {
		playerTracks.pitchmod_on_off_delay_counter -= 1;
		if (playerTracks.pitchmod_on_off_delay_counter == 0) {
			player.pitch_lfo_voices ^= voice;
			spuPitchLFOVoice();
		}
	}

	if (playerTracks.vibrato_depth_slide_length != 0) {
		const uint16_t newVibratoDepth = playerTracks.vibrato_depth + playerTracks.vibrato_depth_slope;
		playerTracks.vibrato_depth_slide_length -= 1;
		playerTracks.vibrato_depth = newVibratoDepth;
		const uint32_t argument0 = (newVibratoDepth & 0x7F00) >> 8;
		const uint32_t vibratoMaxAmplitude = newVibratoDepth & 0x8000 ? playerTracks.pitch_of_note : ((playerTracks.pitch_of_note << 4) - playerTracks.pitch_of_note) >> 8;
		playerTracks.vibrato_max_amplitude = argument0 * (vibratoMaxAmplitude >> 7);
		if (playerTracks.vibrato_delay_counter == 0) {
			if (playerTracks.vibrato_rate_counter != 1) {
				if (*((uint32_t*)playerTracks.vibrato_lfo_addr) == 0) {
					playerTracks.vibrato_lfo_addr += *((uint16_t*)(playerTracks.vibrato_lfo_addr + 4)) * 2;
				}
				const int16_t amplitude = (playerTracks.vibrato_max_amplitude * (*((uint16_t*)playerTracks.vibrato_lfo_addr))) >> 16;
				if (amplitude != playerTracks.vibrato_lfo_amplitude) {
					playerTracks.vibrato_lfo_amplitude = amplitude;
					playerTracks.spuRegisters.update_flags |= AkaoUpdateFlags::VoicePitch;
					if (amplitude >= 0) {
						playerTracks.vibrato_lfo_amplitude = amplitude * 2;
					}
				}
			}
		}
	}

	if (playerTracks.tremolo_depth_slide_length) {
		playerTracks.tremolo_depth_slide_length -= 1;
		playerTracks.tremolo_depth += playerTracks.tremolo_depth_slope;

		if (playerTracks.tremolo_delay_counter == 0) {
			if (playerTracks.tremolo_rate_counter != 1) {
				if (*((uint32_t*)playerTracks.tremolo_lfo_addr) == 0) {
					playerTracks.tremolo_lfo_addr += *((uint16_t*)(playerTracks.tremolo_lfo_addr + 4)) * 2;
				}
				// FIXME: load_i16(saved0 + 0x46)
				const int16_t amplitude = ((((((playerTracks.volume >> 16) * playerTracks.master_volume) >> 7) * (playerTracks.tremolo_depth >> 8)) >> 7) * (*((uint16_t*)playerTracks.tremolo_lfo_addr))) >> 15;
				if (amplitude != playerTracks.tremolo_lfo_amplitude) {
					playerTracks.tremolo_lfo_amplitude = amplitude;
					playerTracks.spuRegisters.update_flags |= AkaoUpdateFlags::VolumeLeft | AkaoUpdateFlags::VolumeRight;
				}
			}
		}
	}

	if (playerTracks.pan_lfo_depth_slide_length != 0) {
		playerTracks.pan_lfo_depth += playerTracks.pan_lfo_depth_slope;
		playerTracks.pan_lfo_depth_slide_length -= 1;
		if (playerTracks.pan_lfo_rate_counter != 1) {
			playerTracks.pan_lfo_addr;
			if (*((uint32_t*)playerTracks.pan_lfo_addr) == 0) {
				playerTracks.pan_lfo_addr += *((uint16_t*)(playerTracks.pan_lfo_addr + 4)) * 2;
			}
			const int16_t amplitude = ((playerTracks.pan_lfo_depth >> 8) * (*((uint16_t*)playerTracks.pan_lfo_addr))) >> 15;
			if (amplitude != playerTracks.pan_lfo_amplitude) {
				playerTracks.pan_lfo_amplitude = amplitude;
				playerTracks.spuRegisters.update_flags |= AkaoUpdateFlags::VolumeLeft | AkaoUpdateFlags::VolumeRight;
			}
		}
	}

	if (playerTracks.pitch_slide_length_counter != 0) {
		const int32_t pitchBlendSlideAmplitude = playerTracks.pitch_bend_slide_amplitude,
			newPitchBlendSlideAmplitude = pitchBlendSlideAmplitude + playerTracks.pitch_bend_slope;
		playerTracks.pitch_slide_length_counter -= 1;
		if (newPitchBlendSlideAmplitude & 0xFFFF0000 != pitchBlendSlideAmplitude & 0xFFFF0000) {
			playerTracks.spuRegisters.update_flags |= AkaoUpdateFlags::VoicePitch;
		}
		playerTracks.pitch_bend_slide_amplitude = newPitchBlendSlideAmplitude;
	}
}

void AkaoExec::akaoWriteSpuRegisters(int32_t voice, SpuRegisters& spuRegisters)
{
	if (spuRegisters.update_flags & AkaoUpdateFlags::VoicePitch) {
		SpuSetVoicePitch(voice, spuRegisters.voice_pitch);
		spuRegisters.update_flags &= ~AkaoUpdateFlags::VoicePitch;

		if (spuRegisters.update_flags == AkaoUpdateFlags::None) {
			return;
		}
	}

	if (spuRegisters.update_flags & (AkaoUpdateFlags::VolumeLeft | AkaoUpdateFlags::VolumeRight)) {
		SpuSetVoiceVolume(voice, spuRegisters.volume_left, spuRegisters.volume_right);
		spuRegisters.update_flags &= ~(AkaoUpdateFlags::VolumeLeft | AkaoUpdateFlags::VolumeRight);

		if (spuRegisters.update_flags == AkaoUpdateFlags::None) {
			return;
		}
	}

	if (spuRegisters.update_flags & AkaoUpdateFlags::VoiceStartAddr) {
		SpuSetVoiceStartAddr(voice, spuRegisters.voice_start_addr);
		spuRegisters.update_flags &= ~AkaoUpdateFlags::VoiceStartAddr;

		if (spuRegisters.update_flags == AkaoUpdateFlags::None) {
			return;
		}
	}

	if (spuRegisters.update_flags & AkaoUpdateFlags::VoiceLoopStartAddr) {
		SpuSetVoiceLoopStartAddr(voice, spuRegisters.voice_loop_start_addr);
		spuRegisters.update_flags &= ~AkaoUpdateFlags::VoiceLoopStartAddr;

		if (spuRegisters.update_flags == AkaoUpdateFlags::None) {
			return;
		}
	}

	if (spuRegisters.update_flags & (AkaoUpdateFlags::AdsrSustainMode | AkaoUpdateFlags::AdsrSustainRate)) {
		SpuSetVoiceSRAttr(voice, spuRegisters.adsr_sustain_rate, spuRegisters.adsr_sustain_rate_mode);
		spuRegisters.update_flags &= ~(AkaoUpdateFlags::AdsrSustainMode | AkaoUpdateFlags::AdsrSustainRate);

		if (spuRegisters.update_flags == AkaoUpdateFlags::None) {
			return;
		}
	}

	if (spuRegisters.update_flags & (AkaoUpdateFlags::AdsrAttackMode | AkaoUpdateFlags::AdsrAttackRate)) {
		SpuSetVoiceARAttr(voice, spuRegisters.adsr_attack_rate, spuRegisters.adsr_attack_rate_mode);
		spuRegisters.update_flags &= ~(AkaoUpdateFlags::AdsrAttackMode | AkaoUpdateFlags::AdsrAttackRate);

		if (spuRegisters.update_flags == AkaoUpdateFlags::None) {
			return;
		}
	}

	if (spuRegisters.update_flags & (AkaoUpdateFlags::AdsrReleaseMode | AkaoUpdateFlags::AdsrReleaseRate)) {
		SpuSetVoiceRRAttr(voice, spuRegisters.adsr_release_rate, spuRegisters.adsr_release_rate_mode);
		spuRegisters.update_flags &= ~(AkaoUpdateFlags::AdsrReleaseMode | AkaoUpdateFlags::AdsrReleaseRate);

		if (spuRegisters.update_flags == AkaoUpdateFlags::None) {
			return;
		}
	}

	if (spuRegisters.update_flags & (AkaoUpdateFlags::AdsrDecayRate | AkaoUpdateFlags::AdsrSustainLevel)) {
		SpuSetVoiceDR(voice, spuRegisters.adsr_decay_rate);
		SpuSetVoiceSL(voice, spuRegisters.adsr_sustain_level);
		spuRegisters.update_flags &= ~(AkaoUpdateFlags::AdsrDecayRate | AkaoUpdateFlags::AdsrSustainLevel);
	}
}

void AkaoExec::akaoLoadTracks()
{
	_player.key_off_voices |= 0xFFFFFF;
	_player.active_voices &= _akaoDataBeforeChannelMask + 0x4;

	for (channel_t channel = 0; channel < AKAO_CHANNEL_MAX; ++channel) {
		if (_akao.isChannelUsed(channel)) {
			AkaoPlayerTrack& playerTrack = _playerTracks[channel];
			playerTrack.addr = _akao.data(channel);
			playerTrack.delta_time_counter = 3;
			playerTrack.gate_time_counter = 1;
			playerTrack.master_volume = 127;
			akaoSetInstrument(playerTrack, 20);
			playerTrack.volume = 0x3FFF0000;
			playerTrack.overlay_balance = 0x4000;
			playerTrack.pan = 0x4000;
			//playerTrack.drum = _akao.data(); // TODO
			playerTrack.tuning = 0;
			playerTrack.transpose = 0;
			playerTrack.pitch_bend_slide_amplitude = 0;
			playerTrack.pitch_slide_amount = 0;
			playerTrack.pitch_slide_length_counter = 0;
			playerTrack.portamento_speed = 0;
			playerTrack.forced_delta_time = 0;
			playerTrack.previous_delta_time = 0;
			playerTrack.overlay_balance_slide_length = 0;
			playerTrack.volume_slide_length = 0;
			playerTrack.pan_slide_length = 0;
			//playerTrack.portamento_speed = 0; // FIXME: present twice? pitch_slide_length?
			playerTrack.voice_effect_flags = AkaoVoiceEffectFlags::None;
			playerTrack.loop_layer = 0;
			playerTrack.legato_flags = 0;
			playerTrack.pan_lfo_amplitude = 0;
			playerTrack.pan_lfo_depth = 0;
			playerTrack.tremolo_depth = 0;
			playerTrack.vibrato_depth = 0;
			playerTrack.pan_lfo_depth_slide_length = 0;
			playerTrack.tremolo_depth_slide_length = 0;
			playerTrack.vibrato_depth_slide_length = 0;
			playerTrack.pitchmod_on_off_delay_counter = 0;
			playerTrack.noise_on_off_delay_counter = 0;
		}
	}

	_player.tempo = 0xFFFF0000;
	_player.time_counter = 1;
	_player.tempo_slide_length = 0;
	_player.reverb_depth = 0;
	_player.reverb_depth_slide_length = 0;
	_player.reverb_depth_slope = 0;
	_player.spucnt = SpuCnt::None;
	_player.tick = 0;
	_player.ticks_per_beat = 0;
	_player.beat = 0;
	_player.measure = 0;
	_player.noise_voices = 0;
	_player.reverb_voices = 0;
	_player.pitch_lfo_voices = 0;
	_player.condition = 0;
	_player.condition_ack = 0;
	_player.pause = 0;
	_player.keyed_voices = 0;
	_player.key_on_voices = 0;
	_player.alternate_voices = 0;
	_player.overlay_voices = 0;

	if (_unknownFlags62FF8 & 0x1) {
		_player.saved_active_voices = _player.active_voices;
		_player.active_voices = 0;
	}

	spuNoiseVoice();
	spuReverbVoice();
	spuPitchLFOVoice();
}

void AkaoExec::akaoPlayMusic()
{
	akaoStopMusic();
	akaoLoadTracks();
	_player.song_id = _akao.songId();
}

void AkaoExec::akaoStopMusic()
{
	if (_player.active_voices) {
		_player.active_voices |= _player.overlay_voices | _player.alternate_voices;
		_player.key_off_voices |= _player.active_voices;
		_player.alternate_voices = 0;
		_player.overlay_voices = 0;
		_player.keyed_voices = 0;
		_player.key_on_voices = 0;

		for (channel_t channel = 0; channel < AKAO_CHANNEL_MAX; ++channel) {
			if ((_player.active_voices >> channel) & 1) {
				AkaoPlayerTrack& playerTrack = _playerTracks[channel];
				playerTrack.delta_time_counter = 4;
				playerTrack.gate_time_counter = 2;
				playerTrack.addr = (uint8_t*)&_akaoEndSequence;
			}
		}
	}
}

void AkaoExec::akaoPause()
{
	if (_player.active_voices) {
		uint32_t active_voices = (_player.active_voices | _player.overlay_voices | _player.alternate_voices) & ~(_active_voices | _spuActiveVoices);

		if (active_voices) {
			uint32_t i = 1, voice = 0;
			_spuRegisters.volume_left = 0;
			_spuRegisters.volume_right = 0;
			_spuRegisters.adsr_sustain_rate = 127;
			// FIXME where _spuRegisters.adsr_sustain_rate_mode is set?

			do {
				if (active_voices & i) {
					_spuRegisters.update_flags = AkaoUpdateFlags::AdsrSustainRate | AkaoUpdateFlags::AdsrSustainMode
						| AkaoUpdateFlags::VolumeRight | AkaoUpdateFlags::VolumeLeft;

					akaoWriteSpuRegisters(voice, _spuRegisters);

					active_voices ^= i;
				}

				i <<= 1;
				voice += 1;
			} while (i);

			_player.saved_active_voices = _player.active_voices;
			_player.active_voices = 0;
		}
	}

	_unknownFlags62FF8 |= 0x1;
}

bool AkaoExec::akaoDispatchVoice(AkaoPlayerTrack& playerTrack, AkaoPlayer& player, uint32_t voice)
{
	uint8_t instruction = 0xA0;

	do {
		playerTrack.addr += 1;
		instruction = *playerTrack.addr;
		if (instruction < 0xA0) {
			break;
		}

		executeChannel(playerTrack, player, voice);
	} while (instruction != 0xA0);

	if (instruction == 0xA0) {
		return;
	}

	uint32_t note = akaoReadNextNote(playerTrack);

	if (playerTrack.forced_delta_time) {
		playerTrack.delta_time_counter = playerTrack.forced_delta_time & 0xFF;
		playerTrack.gate_time_counter = (playerTrack.forced_delta_time >> 8) & 0xFF;
	}
	if (playerTrack.delta_time_counter) {
		if ((note >= 143 || note < 132) && !(playerTrack.legato_flags & 0x5)) {
			playerTrack.delta_time_counter -= 2;
		}
	}
	else {
		playerTrack.delta_time_counter = _delta_times[instruction == 0 ? 0 : ((instruction - 1) % 11) + 1];
		playerTrack.gate_time_counter = playerTrack.delta_time_counter;
		if (note - 132 >= 11 && !(playerTrack.legato_flags & 0x5)) {
			playerTrack.delta_time_counter -= 2;
		}
	}

	playerTrack.previous_delta_time = playerTrack.delta_time_counter;

	if (instruction >= 143) {
		playerTrack.portamento_speed = 0;
		playerTrack.vibrato_lfo_amplitude = 0;
		playerTrack.tremolo_lfo_amplitude = 0;
		playerTrack.legato_flags &= ~0x2;

		return;
	}

	uint32_t pitch;

	if (instruction < 132) {
		if (playerTrack.voice_effect_flags & AkaoVoiceEffectFlags::DrumModeEnabled) {
			if (!playerTrack.use_global_track) {
				_player.key_on_voices |= voice;
			}
			else {
				_key_on_voices |= voice;
			}
			const AkaoDrumKeyAttr* drum = nullptr;//playerTrack.drum + instruction; TODO
			if (playerTrack.instrument != drum->instrument) {
				AkaoInstrAttr& instr = _instruments[drum->instrument];
				playerTrack.spuRegisters.voice_start_addr = instr.addr;
				playerTrack.spuRegisters.voice_loop_start_addr = instr.loop_addr;
				playerTrack.spuRegisters.adsr_attack_rate = instr.adsr_attack_rate;
				playerTrack.spuRegisters.adsr_decay_rate = instr.adsr_decay_rate;
				playerTrack.spuRegisters.adsr_sustain_level = instr.adsr_sustain_level;
				playerTrack.spuRegisters.adsr_sustain_rate = instr.adsr_sustain_rate;
				playerTrack.spuRegisters.adsr_attack_rate_mode = instr.adsr_attack_mode;
				playerTrack.spuRegisters.adsr_sustain_rate_mode = instr.adsr_sustain_mode;
				if (!(playerTrack.voice_effect_flags & AkaoVoiceEffectFlags::AlternateVoiceEnabled)) {
					playerTrack.spuRegisters.adsr_release_rate = instr.adsr_release_rate;
					playerTrack.spuRegisters.adsr_release_rate_mode = instr.adsr_release_mode;
					playerTrack.spuRegisters.update_flags |= AkaoUpdateFlags::VoiceStartAddr
						| AkaoUpdateFlags::AdsrAttackMode | AkaoUpdateFlags::AdsrSustainMode
						| AkaoUpdateFlags::AdsrReleaseMode | AkaoUpdateFlags::AdsrAttackRate
						| AkaoUpdateFlags::AdsrDecayRate | AkaoUpdateFlags::AdsrSustainRate
						| AkaoUpdateFlags::AdsrReleaseRate | AkaoUpdateFlags::AdsrSustainLevel
						| AkaoUpdateFlags::VoiceLoopStartAddr;
				}
				else {
					playerTrack.spuRegisters.update_flags |= AkaoUpdateFlags::VoiceStartAddr
						| AkaoUpdateFlags::AdsrAttackMode | AkaoUpdateFlags::AdsrSustainMode
						| AkaoUpdateFlags::AdsrAttackRate
						| AkaoUpdateFlags::AdsrDecayRate | AkaoUpdateFlags::AdsrSustainRate
						| AkaoUpdateFlags::AdsrSustainLevel
						| AkaoUpdateFlags::VoiceLoopStartAddr;
				}
			}
			pitch = _instruments[drum->instrument].pitch[drum->key % 12];
			uint8_t key_scaled = drum->key / 12;
			if (key_scaled >= 7) {
				pitch <<= key_scaled - 6;
			}
			else if (key_scaled < 6) {
				pitch >>= 6 - key_scaled;
			}
			playerTrack.volume = drum->volume << 16;
			playerTrack.pan = drum->pan << 8;
		}
		else {
			uint8_t transposed_note;
			if (playerTrack.portamento_speed && playerTrack.previous_note_number) {
				transposed_note = (playerTrack.previous_note_number & 0xFF) + (playerTrack.previous_transpose & 0xFF);
				playerTrack.pitch_slide_length = playerTrack.portamento_speed;
				playerTrack.pitch_slide_amount = transposed_note + playerTrack.transpose - playerTrack.previous_note_number - playerTrack.previous_transpose;
				playerTrack.note = playerTrack.previous_note_number - playerTrack.transpose + playerTrack.previous_transpose;
			}
			else {
				uint32_t note = instruction / 11 + playerTrack.octave * 12;
				playerTrack.note = note & 0xFF;
				transposed_note = (playerTrack.transpose & 0xFF) + note;
			}

			if (!(playerTrack.legato_flags & 0x2)) {
				if (!playerTrack.use_global_track) {
					player.key_on_voices |= voice;
					if (playerTrack.voice_effect_flags & AkaoVoiceEffectFlags::OverlayVoiceEnabled) {
						uint32_t overlay_voice_num = playerTrack.overlay_voice_num;
						if (overlay_voice_num >= 24) {
							overlay_voice_num -= 24;
						}
						player.key_on_voices |= 1 << overlay_voice_num;
					}
				}
				else {
					_key_on_voices |= voice;
				}
				playerTrack.pitch_slide_length_counter = 0;
			}
			// FIXME: 0???
			pitch = _instruments[playerTrack.instrument].pitch[0];
			uint8_t key_scaled = transposed_note / 12;
			if (key_scaled >= 7) {
				pitch <<= key_scaled - 6;
			}
			else if (key_scaled < 6) {
				pitch >>= 6 - key_scaled;
			}
		}
		if (!playerTrack.use_global_track) {
			player.keyed_voices |= voice;
		}
		else {
			_keyed_voices |= voice;
		}

		playerTrack.spuRegisters.update_flags |= AkaoUpdateFlags::VoicePitch | AkaoUpdateFlags::VolumeLeft | AkaoUpdateFlags::VolumeRight;
		if (playerTrack.tuning) {
			if (playerTrack.tuning > 0) {
				pitch = (pitch + ((pitch * playerTrack.tuning) >> 7)) & 0xFFFF;
			}
			else {
				pitch = (pitch + ((pitch * playerTrack.tuning) >> 8)) & 0xFFFF;
			}
		}

		playerTrack.pitch_of_note = pitch;

		if (playerTrack.voice_effect_flags & AkaoVoiceEffectFlags::VibratoEnabled) {
			uint32_t vibrato_max_amplitude;
			uint8_t vibrato_depth = playerTrack.vibrato_depth >> 8;
			if (vibrato_depth & 0x80) {
				vibrato_max_amplitude = (vibrato_depth & 0x7F) * pitch;
			}
			else {
				vibrato_max_amplitude = (vibrato_depth & 0x7F) * ((pitch * 15) >> 8);
			}
			playerTrack.vibrato_max_amplitude = vibrato_max_amplitude >> 7;
			playerTrack.vibrato_delay_counter = playerTrack.vibrato_delay;
			playerTrack.vibrato_rate_counter = 1;
			playerTrack.vibrato_lfo_addr = _pan_lfo_addrs[playerTrack.vibrato_form]; // TODO: max 52
		}

		if (playerTrack.voice_effect_flags & AkaoVoiceEffectFlags::TremoloEnabled) {
			playerTrack.tremolo_delay_counter = playerTrack.tremolo_delay;
			playerTrack.tremolo_rate_counter = 1;
			playerTrack.tremolo_lfo_addr = _pan_lfo_addrs[playerTrack.tremolo_form]; // TODO: max 52
		}

		if (playerTrack.voice_effect_flags & AkaoVoiceEffectFlags::ChannelPanLfoENabled) {
			playerTrack.pan_lfo_rate_counter = 1;
			playerTrack.pan_lfo_addr = _pan_lfo_addrs[playerTrack.pan_lfo_form]; // TODO: max 52
		}

		playerTrack.vibrato_lfo_amplitude = 0;
		playerTrack.tremolo_lfo_amplitude = 0;
		playerTrack.pitch_bend_slide_amplitude = 0;
	}

	playerTrack.legato_flags = (playerTrack.legato_flags & 0xFFFD) | ((playerTrack.legato_flags & 0x1) << 1);
	if (playerTrack.pitch_slide_amount) {
		playerTrack.note += playerTrack.pitch_slide_amount;
		uint8_t transposed_note = (uint8_t(playerTrack.note) + uint8_t(playerTrack.transpose)) & 0xFF;
		if (!playerTrack.use_global_track) {
			// FIXME: 0???
			pitch = _instruments[playerTrack.instrument].pitch[0];
			if (playerTrack.tuning) {
				if (playerTrack.tuning > 0) {
					pitch = (pitch + ((pitch * playerTrack.tuning) >> 7)) & 0xFFFF;
				}
				else {
					pitch = (pitch + ((pitch * playerTrack.tuning) >> 8)) & 0xFFFF;
				}
			}
			pitch <<= 16;
		}
		else {
			// FIXME: 0???
			pitch = _instruments[playerTrack.instrument].pitch[0] << 16;
		}

		uint8_t key_scaled = transposed_note / 12;
		if (key_scaled >= 7) {
			pitch <<= key_scaled - 6;
		}
		else if (key_scaled < 6) {
			pitch >>= 6 - key_scaled;
		}

		playerTrack.pitch_slide_length_counter = playerTrack.pitch_slide_length;
		if (playerTrack.pitch_slide_length_counter == 0) {
			return false;
		}
		playerTrack.pitch_slide_amount = 0;
		playerTrack.pitch_bend_slope = (pitch - ((playerTrack.pitch_of_note << 16) + playerTrack.pitch_bend_slide_amplitude)) / playerTrack.pitch_slide_length_counter;
	}

	playerTrack.previous_note_number = playerTrack.note;
	playerTrack.previous_transpose = playerTrack.transpose;

	return true;
}

uint32_t AkaoExec::akaoReadNextNote(AkaoPlayerTrack& playerTrack)
{
	while (true) {
		const uint8_t op = *playerTrack.addr;
		if (op < 0x9A) {
			if (op >= 0x8F) {
				playerTrack.portamento_speed = 0;
				playerTrack.legato_flags &= ~5;
			}
			return op;
		}

		if (op >= 0xA0) {
			const uint8_t len = _opcode_len[op - 0xA0];
			if (len != 0) {
				playerTrack.addr += len;
				continue;
			}

			if (op < 0xF2) {
				switch (op) {
				case 0xC9: // Return To Loop Point Up to N Times
					playerTrack.addr += 1;
					if (*playerTrack.addr == playerTrack.loop_counts[playerTrack.loop_layer] + 1) {
						playerTrack.addr += 1;
						playerTrack.loop_layer = (playerTrack.loop_layer - 1) & 0x3;
					}
					else {
						playerTrack.addr = playerTrack.loop_addrs[playerTrack.loop_layer];
					}
					continue;
				case 0xCA: // Return To Loop Point
					playerTrack.addr = playerTrack.loop_addrs[playerTrack.loop_layer];
					continue;
				case 0xCB: // Reset Sound Effects
				case 0xCD: // Do nothing
				case 0xD1: // Do nothing
				case 0xDB: // Turn Off Portamento
					playerTrack.addr += 1;
					playerTrack.portamento_speed = 0;
					playerTrack.legato_flags &= ~5;
					continue;
				case 0xEE: // Unconditional Jump
					playerTrack.addr += 1;
					break;
				case 0xEF: // CPU Conditional Jump
					playerTrack.addr += 1;
					if (_player.condition >= *playerTrack.addr) {
						playerTrack.addr += 3 + *(uint16_t*)(playerTrack.addr + 1);
					}
					else {
						playerTrack.addr += 3;
					}
					continue;
				case 0xF0: // Jump on the Nth Repeat
				case 0xF1: // Break the loop on the nth repeat
					playerTrack.addr += 1;
					if (*playerTrack.addr == playerTrack.loop_counts[playerTrack.loop_layer] + 1) {
						playerTrack.loop_layer = (playerTrack.loop_layer - 1) & 0x3;
						playerTrack.addr += 3 + *(uint16_t*)(playerTrack.addr + 1);
					}
					else {
						playerTrack.addr += 3;
					}
					continue;
				default:
					playerTrack.portamento_speed = 0;
					playerTrack.legato_flags &= ~5;
					break;
				}
			}
		}

		break;
	}

	return 160;
}

bool AkaoExec::akaoMain()
{
	akaoDspMain();

	uint32_t active_voices = _player.active_voices;

	if (active_voices) {
		uint32_t tempo = _player.tempo >> 16;

		if (_unknown62FEA != 0) {
			if (_unknown62FEA < 128) {
				tempo += (tempo * _unknown62FEA) >> 7;
			}
			else {
				tempo = (tempo * _unknown62FEA) >> 8;
			}
		}

		_player.time_counter += tempo;

		if (_player.time_counter & 0xFFFF0000 || _unknownFlags62FF8 & 0x4) {
			_player.time_counter &= 0xFFFF;
			_unknown62F04 = 0;
			uint32_t voice = 1;
			channel_t channel = 0;

			do {
				if (active_voices & voice) {
					_playerTracks[channel].delta_time_counter -= 1;
					_playerTracks[channel].gate_time_counter -= 1;

					if (_playerTracks[channel].delta_time_counter == 0) {
						if (!akaoDispatchVoice(_playerTracks[channel], _player, voice)) {
							return false;
						}
					}
					else if (_playerTracks[channel].gate_time_counter == 0) {
						_playerTracks[channel].gate_time_counter = 1;
						_player.key_off_voices |= voice;
						_player.keyed_voices &= ~voice;
					}

					akaoDspOnTick(_playerTracks[channel], _player, voice);

					active_voices ^= voice;
				}
				voice <<= 1;
				channel += 1;
			} while (active_voices);

			if (_player.tempo_slide_length) {
				_player.tempo_slide_length -= 1;
				_player.tempo += _player.tempo_slope;
			}

			if (_player.reverb_depth_slide_length) {
				_player.reverb_depth_slide_length -= 1;
				_player.reverb_depth += _player.reverb_depth_slope;
				_player.spucnt |= SpuCnt::SetReverbDepth;
			}

			if (_player.ticks_per_beat) {
				_player.tick += 1;

				if (_player.tick == _player.ticks_per_beat) {
					_player.tick = 0;
					_player.beat += 1;

					if (_player.beat == _player.beats_per_measure) {
						_player.beat = 0;
						_player.measure += 1;
					}
				}
			}
		}
	}

	if (_player.pause) {
		akaoPause();
		_player.pause = 0;
	}

	akaoUnknown30380();
	akaoKeyOffVoices();

	return true;
}

void AkaoExec::akaoKeyOffVoices()
{
	int32_t voice_bit = 0;

	if (_player.active_voices != 0) {
		const int32_t mask = (~(_voicesMask62F68 | _active_voices) | _spuActiveVoices) & _player.key_off_voices;
		if (mask != 0) {
			voice_bit = getVoicesBits(_playerTracks, voice_bit, mask);
		}
		_player.key_off_voices = 0;
	}

	voice_bit |= _key_off_voices;
	_key_off_voices = 0;

	if (voice_bit != 0) {
		SpuSetKey(SPU_OFF, voice_bit);
	}
}

int32_t _global218;
int32_t _global290;
int32_t _global224;
int32_t _global1BC;
int16_t _global288;
int16_t _global200;
int16_t _global1FC;
int32_t _global2A0;
int16_t _global204;
int16_t _global234;
int16_t _global274;
int32_t _global2A4;
int32_t _global248;
int32_t _global2CC;
int32_t _global294;
int32_t _global224;
int32_t _global2B4;
int16_t _global22C;

void AkaoExec::akaoReset()
{
	_global218 = 0x7F0000;
	_global290 = 0x7FFF0000;
	_player.stereo_mode = 1;
	_player.song_id = 0;
	_unknown83398 = 0;
	_unknown83338 = 0;
	_active_voices = 0;
	_player.active_voices = 0;
	_unknown99FDC = 0;
	_player.saved_active_voices = 0;
	_pitch_lfo_voices = 0;
	_player.pitch_lfo_voices = 0;
	_reverb_voices = 0;
	_player.reverb_voices = 0;
	_noise_voices = 0;
	_player.noise_voices = 0;
	//_akaoMessageQueue = 0;
	_player.ticks_per_beat = 0;
	_player.beat = 0;
	_player.beats_per_measure = 0;
	_player.measure = 0;
	_player.reverb_depth_slide_length = 0;
	_player.reverb_depth = 0;
	_player.reverb_type = 0;

	_spuCommonAttr.mask = 0x3FCF; // All except mvolx
	_spuCommonAttr.mvol.left = 0x3FFF;
	_spuCommonAttr.mvol.right = 0x3FFF;
	_spuCommonAttr.mvolmode.left = 0;
	_spuCommonAttr.mvolmode.right = 0;
	_spuCommonAttr.cd.volume.left = 0x7FFF;
	_spuCommonAttr.cd.volume.right = 0x7FFF;
	_spuCommonAttr.cd.reverb = 0;
	_spuCommonAttr.cd.mix = 1;
	_spuCommonAttr.ext.volume.left = 0;
	_spuCommonAttr.ext.volume.right = 0;
	_spuCommonAttr.ext.reverb = 0;
	_spuCommonAttr.ext.mix = 0;

	SpuSetCommonAttr(&_spuCommonAttr);

	_global274 = 0;
	_global234 = 0;
	_global248 = 0;
	_global2CC = 0;
	_global294 = 0;
	_global224 = 0;
	_global2B4 = 0;
	_global22C = 64;

	for (uint8_t voiceNum = 0; voiceNum < 24; ++voiceNum) {
		_voiceUnknown9C6C0[voiceNum]._u0 = 0x7F0000;
		_voiceUnknown9C6C0[voiceNum]._u4 = 0;
		_voiceUnknown9C6C0[voiceNum]._u8 = 0;
		_playerTracks[voiceNum].voice_effect_flags = AkaoVoiceEffectFlags::None;
		_playerTracks[voiceNum].field_50 = 0;
		_playerTracks[voiceNum].use_global_track = 0;
		_playerTracks[voiceNum].spuRegisters.voice = voiceNum;
		SpuSetVoiceVolumeAttr(voiceNum, 0, 0, SPU_VOICE_DIRECT, SPU_VOICE_DIRECT);
	}

	_time_counter = 1;
	_key_off_voices = 0;
	_keyed_voices = 0;
	_key_on_voices = 0;
	_player.key_off_voices = 0;
	_player.keyed_voices = 0;
	_player.key_on_voices = 0;
	_tempo = 0x66A80000;
}

int32_t AkaoExec::getVoicesBits(AkaoPlayerTrack* playerTracks, int32_t voice_bit, int32_t mask)
{
	int i = 1;
	channel_t channel = 0;

	voice_bit |= mask;

	while (mask != 0) {
		if ((mask & i) != 0) {
			int32_t mask2 = 0;

			if (playerTracks[channel].voice_effect_flags & AkaoVoiceEffectFlags::OverlayVoiceEnabled) {
				uint32_t overlay_voice_num = playerTracks[channel].overlay_voice_num;
				if (overlay_voice_num >= 24) {
					overlay_voice_num -= 24;
				}
				mask2 = 1 << overlay_voice_num;
			}
			else if (playerTracks[channel].voice_effect_flags & AkaoVoiceEffectFlags::AlternateVoiceEnabled) {
				mask2 = 1 << playerTracks[channel].alternate_voice_num;
			}

			if (mask & mask2) {
				voice_bit |= mask2;
			}
			mask ^= i;
		}
		i <<= 1;
	}

	return voice_bit;
}

void AkaoExec::spuNoiseVoice()
{
	int32_t voice_bit = 0, mask;

	mask = _player2.noise_voices & _voicesMask62F68 & ~(_active_voices | _spuActiveVoices);

	if (mask) {
		voice_bit = getVoicesBits(_playerTracks2, voice_bit, mask);
	}

	mask = ~(_voicesMask62F68 | _active_voices | _spuActiveVoices) & _player.noise_voices;

	if (mask) {
		voice_bit = getVoicesBits(_playerTracks, voice_bit, mask);
	}

	voice_bit |= _noise_voices;

	// SPU_BIT could be used instead
	voice_bit = SpuSetNoiseVoice(SPU_ON, voice_bit);
	voice_bit = SpuSetNoiseVoice(SPU_OFF, voice_bit ^ 0xFFFFFFFF);
}

void AkaoExec::spuReverbVoice()
{
	int32_t voice_bit = 0, mask;

	mask = (_player2.reverb_voices & _voicesMask62F68) & ~(_active_voices | _spuActiveVoices);
	if (mask) {
		voice_bit = getVoicesBits(_playerTracks2, voice_bit, mask);
	}

	if (_unknownFlags62FF8 & 0x10) {
		voice_bit = 0xFFFFFF;
	}
	else {
		mask = ~(_voicesMask62F68 | _active_voices | _spuActiveVoices) & _player.reverb_voices;

		if (mask) {
			voice_bit = getVoicesBits(_playerTracks, voice_bit, mask);
		}
	}

	voice_bit |= _reverb_voices;

	// SPU_BIT could be used instead
	SpuSetReverbVoice(SPU_ON, voice_bit);
	SpuSetReverbVoice(SPU_OFF, voice_bit ^ 0xFFFFFF);
}

void AkaoExec::spuPitchLFOVoice()
{
	int32_t voice_bit = 0, mask;

	mask = (_player2.pitch_lfo_voices & _voicesMask62F68) & ~(_active_voices | _spuActiveVoices);

	if (mask) {
		voice_bit = getVoicesBits(_playerTracks2, voice_bit, mask);
	}

	mask = ~(_voicesMask62F68 | _active_voices | _spuActiveVoices) & _player.pitch_lfo_voices;

	if (mask) {
		voice_bit = getVoicesBits(_playerTracks, voice_bit, mask);
	}

	voice_bit |= _pitch_lfo_voices;

	// SPU_BIT could be used instead
	SpuSetPitchLFOVoice(SPU_ON, voice_bit);
	SpuSetPitchLFOVoice(SPU_OFF, voice_bit ^ 0xFFFFFF);
}

void AkaoExec::finishChannel(AkaoPlayerTrack& playerTrack, int voice)
{
	if (playerTrack.use_global_track == 0) {
		int argument3 = voice ^ 0xFFFFFFFF;
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
		int argument3 = voice ^ 0xFF0000;
		_active_voices &= argument3;
		_noise_voices &= argument3;
		_reverb_voices &= argument3;
		_pitch_lfo_voices &= argument3;

		argument3 = ~voice;
		_player.key_on_voices &= argument3;
		_player.keyed_voices &= argument3;
		_player.key_off_voices &= argument3;
		_playerTracks[playerTrack.alternate_voice_num].spuRegisters.update_flags |= AkaoUpdateFlags::VoiceStartAddr
			| AkaoUpdateFlags::AdsrAttackMode | AkaoUpdateFlags::AdsrSustainMode
			| AkaoUpdateFlags::AdsrReleaseMode | AkaoUpdateFlags::AdsrAttackRate
			| AkaoUpdateFlags::AdsrDecayRate | AkaoUpdateFlags::AdsrSustainRate
			| AkaoUpdateFlags::AdsrReleaseRate | AkaoUpdateFlags::AdsrSustainLevel
			| AkaoUpdateFlags::VoiceLoopStartAddr;
	}
	playerTrack.voice_effect_flags = AkaoVoiceEffectFlags::None;
	_player.spucnt |= SpuCnt::SetNoiseClock;
	spuNoiseVoice();
	spuReverbVoice();
	spuPitchLFOVoice();
}

bool AkaoExec::loadInstrument(AkaoPlayerTrack& playerTrack, int voice)
{
	const uint8_t instrument = playerTrack.addr[1];
	uint16_t overlay_voice_num = playerTrack.overlay_voice_num;

	if (_unknown62F04 != 0) {
		overlay_voice_num -= 24;
	}

	if (playerTrack.voice_effect_flags & AkaoVoiceEffectFlags::OverlayVoiceEnabled) {
		_player.overlay_voices &= ~(1 << overlay_voice_num);
		playerTrack.voice_effect_flags &= ~AkaoVoiceEffectFlags::OverlayVoiceEnabled;
	}

	if (playerTrack.use_global_track != 0 || !((_player.keyed_voices & voice) & _active_voices)) {
		playerTrack.spuRegisters.update_flags |= AkaoUpdateFlags::VoicePitch;
		if (_instruments[playerTrack.instrument].pitch[0] == 0) {
			return false;
		}
		playerTrack.pitch_of_note = uint32_t(uint64_t(playerTrack.pitch_of_note) * uint64_t(_instruments[instrument].pitch[0])) / _instruments[playerTrack.instrument].pitch[0];
	}

	if (playerTrack.voice_effect_flags & AkaoVoiceEffectFlags::AlternateVoiceEnabled) {
		// Set instrument without release
		const AkaoInstrAttr& instr = _instruments[instrument];
		playerTrack.instrument = instrument;
		playerTrack.spuRegisters.voice_start_addr = instr.addr;
		playerTrack.spuRegisters.voice_loop_start_addr = instr.loop_addr;
		playerTrack.spuRegisters.adsr_attack_rate_mode = instr.adsr_attack_mode;
		playerTrack.spuRegisters.adsr_sustain_rate_mode = instr.adsr_sustain_mode;
		playerTrack.spuRegisters.adsr_attack_rate = instr.adsr_attack_rate;
		playerTrack.spuRegisters.adsr_decay_rate = instr.adsr_decay_rate;
		playerTrack.spuRegisters.adsr_sustain_level = instr.adsr_sustain_level;
		playerTrack.spuRegisters.adsr_sustain_rate = instr.adsr_sustain_rate;
		playerTrack.spuRegisters.update_flags |= AkaoUpdateFlags::VoiceStartAddr
			| AkaoUpdateFlags::AdsrAttackMode | AkaoUpdateFlags::AdsrSustainMode
			| AkaoUpdateFlags::AdsrAttackRate
			| AkaoUpdateFlags::AdsrDecayRate | AkaoUpdateFlags::AdsrSustainRate
			| AkaoUpdateFlags::AdsrSustainLevel
			| AkaoUpdateFlags::VoiceLoopStartAddr;
	}
	else {
		akaoSetInstrument(playerTrack, instrument);
	}

	return true;
}

void AkaoExec::akaoSetInstrument(AkaoPlayerTrack& playerTrack, uint8_t instrument)
{
	const AkaoInstrAttr& instr = _instruments[instrument];

	playerTrack.instrument = instrument;
	playerTrack.spuRegisters.voice_start_addr = instr.addr;
	playerTrack.spuRegisters.voice_loop_start_addr = instr.loop_addr;
	playerTrack.spuRegisters.adsr_attack_rate_mode = instr.adsr_attack_mode;
	playerTrack.spuRegisters.adsr_sustain_rate_mode = instr.adsr_sustain_mode;
	playerTrack.spuRegisters.adsr_release_rate_mode = instr.adsr_release_mode;
	playerTrack.spuRegisters.adsr_attack_rate = instr.adsr_attack_rate;
	playerTrack.spuRegisters.adsr_decay_rate = instr.adsr_decay_rate;
	playerTrack.spuRegisters.adsr_sustain_level = instr.adsr_sustain_level;
	playerTrack.spuRegisters.adsr_sustain_rate = instr.adsr_sustain_rate;
	playerTrack.spuRegisters.adsr_release_rate = instr.adsr_release_rate;
	playerTrack.spuRegisters.update_flags |= AkaoUpdateFlags::VoiceStartAddr
		| AkaoUpdateFlags::AdsrAttackMode | AkaoUpdateFlags::AdsrSustainMode
		| AkaoUpdateFlags::AdsrReleaseMode | AkaoUpdateFlags::AdsrAttackRate
		| AkaoUpdateFlags::AdsrDecayRate | AkaoUpdateFlags::AdsrSustainRate
		| AkaoUpdateFlags::AdsrReleaseRate | AkaoUpdateFlags::AdsrSustainLevel
		| AkaoUpdateFlags::VoiceLoopStartAddr;
}

void AkaoExec::adsrDecayRate(AkaoPlayerTrack& playerTrack)
{
	OPCODE_ADSR_UPDATE(adsr_decay_rate, AkaoUpdateFlags::AdsrDecayRate);
}

void AkaoExec::adsrSustainLevel(AkaoPlayerTrack& playerTrack)
{
	OPCODE_ADSR_UPDATE(adsr_sustain_level, AkaoUpdateFlags::AdsrSustainLevel);
}

void AkaoExec::turnOnReverb(AkaoPlayerTrack& playerTrack, int voice)
{
	if (playerTrack.use_global_track == 0) {
		_player.reverb_voices |= voice;
	}
	else {
		_reverb_voices |= voice;
	}
	this->spuReverbVoice();
}

void AkaoExec::turnOffReverb(AkaoPlayerTrack& playerTrack, int voice)
{
	if (playerTrack.use_global_track == 0) {
		_player.reverb_voices &= ~voice;
	}
	else {
		_reverb_voices &= ~voice;
	}
	this->spuReverbVoice();
}

void AkaoExec::turnOnNoise(AkaoPlayerTrack& playerTrack, int voice)
{
	if (playerTrack.use_global_track == 0) {
		_player.noise_voices |= voice;
	}
	else {
		_noise_voices |= voice;
	}
	_player.spucnt |= SpuCnt::SetNoiseClock;
	this->spuNoiseVoice();
}

void AkaoExec::turnOffNoise(AkaoPlayerTrack& playerTrack, int voice)
{
	if (playerTrack.use_global_track == 0) {
		_player.noise_voices &= ~voice;
	}
	else {
		_noise_voices &= ~voice;
	}
	_player.spucnt |= SpuCnt::SetNoiseClock;
	this->spuNoiseVoice();
	playerTrack.noise_on_off_delay_counter = 0;
}

void AkaoExec::turnOnFrequencyModulation(AkaoPlayerTrack& playerTrack, int voice)
{
	if (playerTrack.use_global_track == 0) {
		_player.pitch_lfo_voices |= voice;
	}
	else if (voice & 0x555555 == 0) {
		_pitch_lfo_voices |= voice;
	}
	this->spuPitchLFOVoice();
}

void AkaoExec::turnOffFrequencyModulation(AkaoPlayerTrack& playerTrack, int voice)
{
	if (playerTrack.use_global_track == 0) {
		_player.pitch_lfo_voices &= ~voice;
	}
	else {
		_pitch_lfo_voices &= ~voice;
	}
	this->spuPitchLFOVoice();
	playerTrack.pitchmod_on_off_delay_counter = 0;
}

void AkaoExec::turnOnOverlayVoice(AkaoPlayerTrack& playerTrack)
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
		uint8_t instrument1 = playerTrack.addr[1], instrument2 = playerTrack.addr[2];
		akaoSetInstrument(playerTrack, instrument1);
		akaoSetInstrument(_playerTracks[voice], instrument2);
	}
}

#define OPCODE_SLIDE_U16(name, shift) \
	const uint16_t length = data[1] == 0 ? 256 : data[1]; \
	const uint16_t param = data[2] << shift; \
	name##_slide_length = length; \
	name##_slope = (param - name) / length;

#define OPCODE_SLIDE_U32(name, shift) \
	const uint16_t length = data[1] == 0 ? 256 : data[1]; \
	const uint32_t param = data[2] << shift; \
	name##_slide_length = length; \
	name &= 0xFFFF0000; \
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

int AkaoExec::executeChannel(AkaoPlayerTrack& playerTrack, AkaoPlayer& player, uint32_t voice)
{
	const uint8_t* data = playerTrack.addr;

	if (data + 1 - _akao.data() > _akao.dataSize()) {
		return -2; // No more data
	}

	uint8_t op = *playerTrack.addr;

	if (op < 0x9A) {

	}
	else {
		const uint8_t opcode_len = _opcode_len[op - 0xA0];

		if (data + opcode_len - _akao.data() > _akao.dataSize()) {
			return -2; // No more data
		}

		switch (op) {
		case 0xA0: // Finished
			finishChannel(playerTrack, voice);
			return -1;
		case 0xA1: // Load Instrument
			if (!loadInstrument(playerTrack, voice)) {
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
			playerTrack.spuRegisters.update_flags |= AkaoUpdateFlags::VolumeLeft | AkaoUpdateFlags::VolumeRight;
			playerTrack.master_volume = uint32_t(master_volume);
			break;
		case 0xA4: // Pitch Bend Slide
			const uint8_t length = data[1] == 0 ? 256 : data[1];
			const int8_t amount = data[2];
			playerTrack.pitch_slide_length = length;
			playerTrack.pitch_slide_amount = amount;
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
			playerTrack.volume_slide_length = 0;
			playerTrack.spuRegisters.update_flags |= AkaoUpdateFlags::VolumeLeft | AkaoUpdateFlags::VolumeRight;
			playerTrack.volume = uint32_t(volume) << 23;
			break;
		case 0xA9: // Channel Volume Slide
			OPCODE_SLIDE_U32(playerTrack.volume, 23);
			break;
		case 0xAA: // Set Channel Pan
			const uint8_t pan = data[1];
			playerTrack.pan_slide_length = 0;
			playerTrack.spuRegisters.update_flags |= AkaoUpdateFlags::VolumeLeft | AkaoUpdateFlags::VolumeRight;
			playerTrack.pan = pan << 8;
			break;
		case 0xAB: // Set Channel Pan Slide
			OPCODE_SLIDE_U16(playerTrack.pan, 8);
			break;
		case 0xAC: // Noise Clock Frequency
			const uint8_t noise_clock = data[1];
			if (playerTrack.use_global_track == 0) {
				if (noise_clock & 0xC0) {
					_player.noise_clock = (_player.noise_clock + (noise_clock & 0x3F)) & 0x3F;
				}
				else {
					_player.noise_clock = noise_clock;
				}
			}
			else if (noise_clock & 0xC0) {
				_noise_clock = (_noise_clock + (noise_clock & 0x3F)) & 0x3F;
			}
			else {
				_noise_clock = noise_clock;
			}
			_player.spucnt |= SpuCnt::SetNoiseClock;
			break;
		case 0xAD: // ADSR Attack Rate
			OPCODE_ADSR_UPDATE(adsr_attack_rate, AkaoUpdateFlags::AdsrAttackMode | AkaoUpdateFlags::AdsrAttackRate);
			break;
		case 0xAE: // ADSR Decay Rate
			adsrDecayRate(playerTrack);
			break;
		case 0xAF: // ADSR Sustain Level
			adsrSustainLevel(playerTrack);
			break;
		case 0xB0: // ADSR Decay Rate and Sustain Level
			adsrDecayRate(playerTrack);
			adsrSustainLevel(playerTrack);
			break;
		case 0xB1: // ADSR Sustain Rate
			OPCODE_ADSR_UPDATE(adsr_sustain_rate, AkaoUpdateFlags::AdsrSustainMode | AkaoUpdateFlags::AdsrSustainRate);
			break;
		case 0xB2: // ADSR Release Rate
			OPCODE_ADSR_UPDATE(adsr_release_rate, AkaoUpdateFlags::AdsrReleaseMode | AkaoUpdateFlags::AdsrReleaseRate);
			break;
		case 0xB3: // ADSR Reset
			const AkaoInstrAttr & instr = _instruments[playerTrack.instrument];
			playerTrack.spuRegisters.adsr_attack_rate = instr.adsr_attack_rate;
			playerTrack.spuRegisters.adsr_decay_rate = instr.adsr_decay_rate;
			playerTrack.spuRegisters.adsr_sustain_level = instr.adsr_sustain_level;
			playerTrack.spuRegisters.adsr_sustain_rate = instr.adsr_sustain_rate;
			playerTrack.spuRegisters.adsr_release_rate = instr.adsr_release_rate;
			playerTrack.spuRegisters.adsr_attack_rate_mode = instr.adsr_attack_mode;
			playerTrack.spuRegisters.adsr_sustain_rate_mode = instr.adsr_sustain_mode;
			playerTrack.spuRegisters.adsr_release_rate_mode = instr.adsr_release_mode;
			playerTrack.spuRegisters.update_flags |= AkaoUpdateFlags::AdsrAttackMode | AkaoUpdateFlags::AdsrSustainMode
				| AkaoUpdateFlags::AdsrReleaseMode | AkaoUpdateFlags::AdsrAttackRate
				| AkaoUpdateFlags::AdsrDecayRate | AkaoUpdateFlags::AdsrSustainRate
				| AkaoUpdateFlags::AdsrReleaseRate | AkaoUpdateFlags::AdsrSustainLevel;
			if (playerTrack.voice_effect_flags & AkaoVoiceEffectFlags::OverlayVoiceEnabled) {
				AkaoPlayerTrack& otherTrack = _playerTracks[playerTrack.overlay_voice_num];
				otherTrack.spuRegisters.adsr_attack_rate = instr.adsr_attack_rate;
				otherTrack.spuRegisters.adsr_decay_rate = instr.adsr_decay_rate;
				otherTrack.spuRegisters.adsr_sustain_level = instr.adsr_sustain_level;
				otherTrack.spuRegisters.adsr_sustain_rate = instr.adsr_sustain_rate;
				otherTrack.spuRegisters.adsr_release_rate = instr.adsr_release_rate;
				otherTrack.spuRegisters.adsr_attack_rate_mode = instr.adsr_attack_mode;
				otherTrack.spuRegisters.adsr_sustain_rate_mode = instr.adsr_sustain_mode;
				otherTrack.spuRegisters.adsr_release_rate_mode = instr.adsr_release_mode;
			}
			break;
		case 0xB4: // Vibrato Channel Pitch LFO
			const uint8_t depth_or_delay = data[1];
			const uint8_t rate = data[2] == 0 ? 256 : data[2];
			const uint8_t form = data[3];
			playerTrack.voice_effect_flags |= AkaoVoiceEffectFlags::VibratoEnabled;
			if (playerTrack.use_global_track != 0) {
				playerTrack.vibrato_delay = 0;
				if (depth_or_delay != 0) {
					playerTrack.vibrato_depth = depth_or_delay << 8;
				}
			}
			else {
				playerTrack.vibrato_delay = depth_or_delay;
			}
			playerTrack.vibrato_rate = rate;
			playerTrack.vibrato_form = form;
			playerTrack.vibrato_delay_counter = playerTrack.vibrato_delay;
			playerTrack.vibrato_rate_counter = 1;
			playerTrack.vibrato_lfo_addr = _pan_lfo_addrs[form];

			const uint8_t depth = playerTrack.vibrato_depth >> 8;

			playerTrack.vibrato_max_amplitude = ((depth & 0x7F) * (depth & 0x80 != 0
				? playerTrack.pitch_of_note
				: (((playerTrack.pitch_of_note << 4) - playerTrack.pitch_of_note) >> 8)
				)) >> 7;
			break;
		case 0xB5: // Vibrato Depth
			const uint8_t depth = data[1];
			playerTrack.vibrato_depth = depth << 8;
			playerTrack.vibrato_max_amplitude = ((depth & 0x7F) * (depth & 0x80 != 0
				? playerTrack.pitch_of_note
				: (((playerTrack.pitch_of_note << 4) - playerTrack.pitch_of_note) >> 8)
				)) >> 7;
			break;
		case 0xB6: // Turn Off Vibrato
			playerTrack.vibrato_lfo_amplitude = 0;
			playerTrack.voice_effect_flags &= ~AkaoVoiceEffectFlags::VibratoEnabled;
			playerTrack.spuRegisters.update_flags |= AkaoUpdateFlags::VoicePitch;
			break;
		case 0xB7: // ADSR Attack Mode
			OPCODE_ADSR_UPDATE(adsr_attack_rate_mode, AkaoUpdateFlags::AdsrAttackMode);
			break;
		case 0xB8: // Tremolo Channel Volume LFO
			const uint8_t depth_or_delay = data[1];
			const uint8_t rate = data[2] == 0 ? 256 : data[2];
			const uint8_t form = data[3];
			playerTrack.voice_effect_flags |= AkaoVoiceEffectFlags::TremoloEnabled;
			if (playerTrack.use_global_track != 0) {
				playerTrack.tremolo_delay = 0;
				if (depth_or_delay != 0) {
					playerTrack.tremolo_depth = depth_or_delay << 8;
				}
			}
			else {
				playerTrack.tremolo_delay = depth_or_delay;
			}
			playerTrack.tremolo_rate = rate;
			playerTrack.tremolo_form = form;
			playerTrack.tremolo_delay_counter = playerTrack.tremolo_delay;
			playerTrack.tremolo_rate_counter = 1;
			playerTrack.tremolo_lfo_addr = _pan_lfo_addrs[form];
			break;
		case 0xB9: // Tremolo Depth
			const uint8_t tremolo_depth = data[1];
			playerTrack.tremolo_depth = tremolo_depth << 8;
			break;
		case 0xBA: // Turn Off Tremolo
			playerTrack.tremolo_lfo_amplitude = 0;
			playerTrack.voice_effect_flags &= ~AkaoVoiceEffectFlags::TremoloEnabled;
			playerTrack.spuRegisters.update_flags |= AkaoUpdateFlags::VolumeLeft | AkaoUpdateFlags::VolumeRight;
			break;
		case 0xBB: // ADSR Sustain Mode
			const uint8_t adsr_sustain_mode = data[1];
			playerTrack.spuRegisters.update_flags |= AkaoUpdateFlags::AdsrSustainMode;
			playerTrack.spuRegisters.adsr_sustain_rate_mode = adsr_sustain_mode;
			if (playerTrack.voice_effect_flags & AkaoVoiceEffectFlags::OverlayVoiceEnabled) {
				_playerTracks[playerTrack.overlay_voice_num].spuRegisters.adsr_sustain_rate_mode = adsr_sustain_mode;
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
			playerTrack.spuRegisters.update_flags |= AkaoUpdateFlags::VolumeLeft | AkaoUpdateFlags::VolumeRight;
			break;
		case 0xBF: // ADSR Release Mode
			const uint8_t adsr_release_mode = data[1];
			playerTrack.spuRegisters.update_flags |= AkaoUpdateFlags::AdsrReleaseMode;
			playerTrack.spuRegisters.adsr_release_rate_mode = adsr_release_mode;
			if (playerTrack.voice_effect_flags & AkaoVoiceEffectFlags::OverlayVoiceEnabled) {
				_playerTracks[playerTrack.overlay_voice_num].spuRegisters.adsr_release_rate_mode = adsr_release_mode;
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
		case 0xC2: // Turn On Reverb
			turnOnReverb(playerTrack, voice);
			break;
		case 0xC3: // Turn Off Reverb
			turnOffReverb(playerTrack, voice);
			break;
		case 0xC4: // Turn On Noise
			turnOnNoise(playerTrack, voice);
			break;
		case 0xC5: // Turn Off Noise
			turnOffNoise(playerTrack, voice);
			break;
		case 0xC6: // Turn On Frequency Modulation
			turnOnFrequencyModulation(playerTrack, voice);
			break;
		case 0xC7: // Turn Off Frequency Modulation
			turnOffFrequencyModulation(playerTrack, voice);
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
				playerTrack.addr = playerTrack.loop_addrs[playerTrack.loop_layer];
				return;
			}
			playerTrack.loop_layer = (playerTrack.loop_layer - 1) & 0x3;
			break;
		case 0xCA: // Return To Loop Point
			playerTrack.loop_counts[playerTrack.loop_layer] += 1;
			playerTrack.addr = playerTrack.loop_addrs[playerTrack.loop_layer];
			return;
		case 0xCB: // Reset Sound Effects
			playerTrack.voice_effect_flags &= ~(
				AkaoVoiceEffectFlags::VibratoEnabled
				| AkaoVoiceEffectFlags::TremoloEnabled
				| AkaoVoiceEffectFlags::ChannelPanLfoENabled
				| AkaoVoiceEffectFlags::PlaybackRateSideChainEnabled
				| AkaoVoiceEffectFlags::PitchVolumeSideChainEnabled
				);
			turnOffNoise(playerTrack, voice);
			turnOffFrequencyModulation(playerTrack, voice);
			turnOffReverb(playerTrack, voice);
			playerTrack.legato_flags &= ~(0x4 | 0x1);
			break;
		case 0xCC: // Turn On Legato
			playerTrack.legato_flags = 0x1;
			break;
		case 0xCD: // Do nothing
			break;
		case 0xCE: // Turn On Noise and Toggle Noise On/Off after a Period of Time
			OPCODE_UPDATE_DELAY_COUNTER(noise);
			turnOnNoise(playerTrack, voice);
			break;
		case 0xCF: // Toggle Noise On/Off after a Period of Time
			OPCODE_UPDATE_DELAY_COUNTER(noise);
			break;
		case 0xD0: // Turn On Full-Length Note Mode (?)
			playerTrack.legato_flags = 0x4;
			break;
		case 0xD1: // Do nothing
			break;
		case 0xD2: // Toggle Freq Modulation Later and Turn On Frequency Modulation
			OPCODE_UPDATE_DELAY_COUNTER(pitchmod);
			turnOnFrequencyModulation(playerTrack, voice);
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
		case 0xE7: // Unused
			finishChannel(playerTrack, voice);
			return -1;
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
			_player.spucnt |= SpuCnt::SetReverbDepth;
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
			playerTrack.drum = reinterpret_cast<const AkaoDrumKeyAttr*>(data) + 2 + offset;
			break;
		case 0xED: // Turn Off Drum Mode
			playerTrack.voice_effect_flags &= ~AkaoVoiceEffectFlags::DrumModeEnabled;
			break;
		case 0xEE: // Unconditional Jump
			const int16_t offset = (data[2] << 8) | data[1];
			playerTrack.addr += opcode_len + offset;
			return;
		case 0xEF: // CPU Conditional Jump
			const uint8_t condition = data[1];
			if (_player.condition != 0 && condition <= _player.condition) {
				const int16_t offset = (data[3] << 8) | data[2];
				_player.condition_ack = condition;
				playerTrack.addr += opcode_len + offset;
				return;
			}
			playerTrack.addr += opcode_len;
			return;
		case 0xF0: // Jump on the Nth Repeat
			const uint16_t times = data[1] == 0 ? 256 : data[1];
			if (playerTrack.loop_counts[playerTrack.loop_layer] + 1 != times) {
				playerTrack.addr += opcode_len;
				return;
			}
			const int16_t offset = (data[3] << 8) | data[2];
			playerTrack.addr += opcode_len + offset;
			return;
		case 0xF1: // Break the loop on the nth repeat
			const uint16_t times = data[1] == 0 ? 256 : data[1];
			if (playerTrack.loop_counts[playerTrack.loop_layer] + 1 != times) {
				playerTrack.addr += opcode_len;
				return;
			}
			const int16_t offset = (data[3] << 8) | data[2];
			playerTrack.loop_layer = (playerTrack.loop_layer - 1) & 0x3;
			playerTrack.addr += opcode_len + offset;
			return;
		case 0xF2: // Load instrument (no attack sample)
			int instrument = data[1];
			const AkaoInstrAttr& instr = _instruments[instrument];
			if (playerTrack.use_global_track != 0 || !((_player.keyed_voices & voice) & _active_voices)) {
				playerTrack.spuRegisters.update_flags |= AkaoUpdateFlags::VoicePitch;
				const AkaoInstrAttr& playerInstr = _instruments[playerTrack.instrument];
				if (playerInstr.pitch[0] == 0) {
					return -2;
				}
				playerTrack.pitch_of_note = uint32_t(uint64_t(playerTrack.pitch_of_note) * uint64_t(instr.pitch[0])) / playerInstr.pitch[0];
			}
			playerTrack.instrument = instrument;
			playerTrack.spuRegisters.voice_start_addr = playerTrack.use_global_track == 0 ? 0x70000 : 0x76FE0;
			playerTrack.spuRegisters.voice_loop_start_addr = instr.loop_addr;
			playerTrack.spuRegisters.adsr_attack_rate = instr.adsr_attack_rate;
			playerTrack.spuRegisters.adsr_decay_rate = instr.adsr_decay_rate;
			playerTrack.spuRegisters.adsr_sustain_level = instr.adsr_sustain_level;
			playerTrack.spuRegisters.adsr_sustain_rate = instr.adsr_sustain_rate;
			playerTrack.spuRegisters.adsr_attack_rate_mode = instr.adsr_attack_mode;
			playerTrack.spuRegisters.adsr_sustain_rate_mode = instr.adsr_sustain_mode;
			if (playerTrack.voice_effect_flags & AkaoVoiceEffectFlags::AlternateVoiceEnabled) {
				playerTrack.spuRegisters.update_flags |= AkaoUpdateFlags::VoiceStartAddr
					| AkaoUpdateFlags::AdsrAttackMode | AkaoUpdateFlags::AdsrSustainMode
					| AkaoUpdateFlags::AdsrAttackRate
					| AkaoUpdateFlags::AdsrDecayRate | AkaoUpdateFlags::AdsrSustainRate
					| AkaoUpdateFlags::AdsrSustainLevel
					| AkaoUpdateFlags::VoiceLoopStartAddr;
			}
			else {
				playerTrack.spuRegisters.adsr_release_rate = instr.adsr_release_rate;
				playerTrack.spuRegisters.adsr_release_rate_mode = instr.adsr_release_mode;
				playerTrack.spuRegisters.update_flags |= AkaoUpdateFlags::VoiceStartAddr
					| AkaoUpdateFlags::AdsrAttackMode | AkaoUpdateFlags::AdsrSustainMode
					| AkaoUpdateFlags::AdsrReleaseMode | AkaoUpdateFlags::AdsrAttackRate
					| AkaoUpdateFlags::AdsrDecayRate | AkaoUpdateFlags::AdsrSustainRate
					| AkaoUpdateFlags::AdsrReleaseRate | AkaoUpdateFlags::AdsrSustainLevel
					| AkaoUpdateFlags::VoiceLoopStartAddr;
			}
			break;
		case 0xF3: // Set Pause
			_player.pause = 1;
			break;
		case 0xF4: // Turn On Overlay Voice
			turnOnOverlayVoice(playerTrack);
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
				playerTrack.spuRegisters.update_flags |= AkaoUpdateFlags::VolumeLeft | AkaoUpdateFlags::VolumeRight;
			}
			break;
		case 0xF7: // Overlay Volume Balance Slide
			playerTrack.overlay_balance & 0xFF00;
			OPCODE_SLIDE_U16(playerTrack.overlay_balance, 8);
			break;
		case 0xF8: // Turn On Alternate Voice
			playerTrack.spuRegisters.adsr_release_rate = data[1];
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
			playerTrack.spuRegisters.update_flags |= AkaoUpdateFlags::AdsrReleaseMode | AkaoUpdateFlags::AdsrReleaseRate;
			playerTrack.spuRegisters.adsr_release_rate = _instruments[playerTrack.instrument].adsr_release_rate;
			break;
		case 0xFA:
		case 0xFB:
		case 0xFC: // Unused
			finishChannel(playerTrack, voice);
			return -1;
		case 0xFD: // Time signature
			_player.ticks_per_beat = data[1];
			_player.tick = 0;
			_player.beat = 0;
			_player.beats_per_measure = data[2];
			break;
		case 0xFE: // Measure number
			_player.measure = (uint16_t(data[2]) << 8) | uint16_t(data[1]);
			break;
		case 0xFF: // Unused
			finishChannel(playerTrack, voice);
			return -1;
		}

		playerTrack.addr += opcode_len;
		return;
	}

	playerTrack.addr += 1;

	return;
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

uint8_t AkaoExec::_delta_times[12] = {
	0xC0, 0x60, 0x30, 0x18,
	0x0C, 0x06, 0x03, 0x20,
	0x10, 0x08, 0x04, 0x00
};

uint32_t AkaoExec::_akaoEndSequence = 0xA0;

uint32_t AkaoExec::_pan_lfo_addrs[PAN_LFO_ADDRS_LEN] = {
	0x8004A044, // 0x1FFF
	0x8004A05C, // 0x7FFF
	0x8004A068,
	0x8004A07C,

	// TODO
};

uint8_t AkaoExec::_empty_adpcm[EMPTY_ADPCM_LEN] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x0C, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
}
