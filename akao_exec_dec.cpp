#include "akao_exec_dec.h"
#include "include/LIBETC.h"

#define OPCODE_ADSR_UPDATE(name, upd_flags) \
	const uint8_t param = playerTrack.addr[1]; \
	playerTrack.spuRegisters.update_flags |= upd_flags; \
	playerTrack.spuRegisters.name = param; \
	if (playerTrack.voice_effect_flags & AkaoVoiceEffectFlags::OverlayVoiceEnabled) { \
		_playerTracks[playerTrack.overlay_voice_num].spuRegisters.name = param; \
	}

AkaoExecDec::AkaoExecDec(const Akao& akao, bool loadInstruments2) noexcept :
	_akao(akao)
{
	resetCallback();
	SpuInit();
	uint32_t startAddr = 0x1010 /* 0x800F0000 */, endAddr = 0x1010; /* 0x80166000 */
	akaoInit(&startAddr, &endAddr);
	if (loadInstruments2) {
		akaoLoadInstrumentSet2(0x80168000, 0x801A6000);
	}
	/* _akaoMessage = 0x10; // Load and play AKAO
	_akaoDataAddr = 0x801D0000;
	akaoPostMessage(); */
}

void AkaoExecDec::resetCallback()
{
	//0x80051520 // load_i32(0x80051534) + 0xC
	//resetCallback: main2abis(); // 8003D258
}

int32_t _timeEventDescriptor = 0;

long AkaoExecDec::akaoTimerCallback()
{
	int32_t root_counter_1 = GetRCnt(RCntCNT2);
	const int32_t mode = 1;
	uint32_t time_elapsed = VSync(mode);

	if (time_elapsed < _previous_time_elapsed) {
		_previous_time_elapsed = 0;
	}

	uint32_t loop_count = (time_elapsed - _previous_time_elapsed) / 66;

	if (loop_count == 0 || loop_count >= 9) {
		loop_count = 1;
	}

	_previous_time_elapsed = time_elapsed;

	if (_unknownFlags62FF8 & 0x4) {
		loop_count *= 2;
	}

	bool error = false;

	while (loop_count) {
		if (!akaoMain()) {
			error = true;
			break;
		}
		loop_count -= 1;
	}

	int32_t root_counter_2 = GetRCnt(RCntCNT2);
	int32_t diff = root_counter_2 - root_counter_1;

	if (diff <= 0) {
		diff += 17361;
	}

	_unknown62E04 = diff + _unknown4953C + _unknown49540 + _unknown49544;
	_unknown49544 = diff;
	_unknown49538 = _unknown4953C;
	_unknown4953C = _unknown49540;
	_unknown49540 = _unknown49544;

	return time_elapsed;
}

int AkaoExecDec::akaoCalculateVolumeAndPitch(AkaoPlayerTrack& playerTracks)
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
			int16_t tremolo_lfo_amplitude = ((((right_volume * (playerTracks.tremolo_depth >> 8)) << 9) >> 16) * (*playerTracks.tremolo_lfo_addr)) >> 15;
			playerTracks.tremolo_lfo_addr += 2;
			if (tremolo_lfo_amplitude != playerTracks.tremolo_lfo_amplitude) {
				playerTracks.tremolo_lfo_amplitude = tremolo_lfo_amplitude;
				playerTracks.spuRegisters.update_flags |= AkaoUpdateFlags::VolumeLeft | AkaoUpdateFlags::VolumeRight;
			}
		}
	}
/*
8002ED34 temporary0 = argument0
8002ED4C right_volume = u32(load_i16(temporary0 + 0x46) * load_i32(temporary0 + 0x2C)) >> 7
8002ED5C if load_i32(temporary0 + 0x38) & 0x1 then
8002ED6C   if load_u16(temporary0 + 0x74) == 0 then
8002ED7C     value0 = u32(load_u16(temporary0 + 0x78)) - 1
8002ED80     store_i16(temporary0 + 0x78, value0)
8002ED88     if value0 & 0xFFFF == 0 then
8002ED94       value1 = load_i32(temporary0 + 0x18)
8002ED98       store_i16(temporary0 + 0x78, load_u16(temporary0 + 0x76))
8002EDA4       if load_i16(value1) == 0 then
8002EDB4         if load_i16(value1 + 0x2) == 0 then
8002EDCC           store_i32(temporary0 + 0x18, (load_i16(value1 + 0x4) << 1) + value1)
				 end
			   end
8002EDD0       value1 = load_i32(temporary0 + 0x18)
8002EDDC       store_i32(temporary0 + 0x18, u32(value1) + 2)
8002EDF8       argument3 = i32(load_u16(temporary0 + 0x7C) * load_i16(value1)) >> 16
8002EDFC       if argument3 != load_i16(temporary0 + 0xD6) then
8002EE08         store_i16(temporary0 + 0xD6, argument3)
8002EE14         store_i32(temporary0 + 0xE0, load_i32(temporary0 + 0xE0) | 0x10)
8002EE10         if argument3 >= 0 then
8002EE1C           store_i16(temporary0 + 0xD6, argument3 << 1)
				 end
			   end
			 end
		   end
		 end
8002EE2C if load_i32(temporary0 + 0x38) & 0x2 then
8002EE3C   if load_u16(temporary0 + 0x88) == 0 then
8002EE4C     value0 = u32(load_u16(temporary0 + 0x8C)) - 1
8002EE50     store_i16(temporary0 + 0x8C, value0)
8002EE58     if value0 & 0xFFFF == 0 then
8002EE64       value1 = load_i32(temporary0 + 0x1C)
8002EE68       store_i16(temporary0 + 0x8C, load_u16(temporary0 + 0x8A))
8002EE74       if load_i16(value1) == 0 then
8002EE84         if load_i16(value1 + 0x2) == 0 then
8002EE9C           store_i32(temporary0 + 0x1C, (load_i16(value1 + 0x4) << 1) + value1)
				 end
			   end
8002EEB0       value1 = load_i32(temporary0 + 0x1C)
8002EEBC       store_i32(temporary0 + 0x1C, u32(value1) + 2)
8002EEDC       argument3 = i32((i32((right_volume * (u32(load_u16(temporary0 + 0x90)) >> 8)) << 9) >> 16) * load_i16(value1)) >> 15
8002EEE0       if argument3 != load_i16(temporary0 + 0xD8) then
8002EEEC         store_i16(temporary0 + 0xD8, argument3)
8002EEF4         store_i32(temporary0 + 0xE0, load_i32(temporary0 + 0xE0) | 0x3)
			   end
			 end
		   end
		 end
8002EF04 if load_i32(temporary0 + 0x38) & 0x4 then # ChannelPanLfoENabled
8002EF14   value0 = load_u16(temporary0 + 0x9A) - 1 # pan_lfo_rate_counter
8002EF18   store_i16(temporary0 + 0x9A, value0) # pan_lfo_rate_counter
8002EF20   if value0 & 0xFFFF == 0 then
8002EF2C     pan_lfo_addr = load_i32(temporary0 + 0x20) # pan_lfo_addr
8002EF30     store_i16(temporary0 + 0x9A, load_u16(temporary0 + 0x98)) # pan_lfo_rate
8002EF3C     if load_i16(pan_lfo_addr) == 0 then
8002EF4C       if load_i16(pan_lfo_addr + 0x2) == 0 then
8002EF64         store_i32(temporary0 + 0x20, (load_i16(pan_lfo_addr + 0x4) << 1) + pan_lfo_addr)
			   end
			 end
8002EF68     pan_lfo_addr = load_i32(temporary0 + 0x20) # pan_lfo_addr
8002EF74     store_i32(temporary0 + 0x20, u32(pan_lfo_addr) + 2)
8002EF90     argument3 = i32((u32(load_u16(temporary0 + 0x9E)) >> 8) * load_i16(pan_lfo_addr)) >> 15 # pan_lfo_depth
8002EF94     if argument3 != load_i16(temporary0 + 0xDA) then # pan_lfo_amplitude
8002EFA0       store_i16(temporary0 + 0xDA, argument3) # pan_lfo_amplitude
8002EFA8       store_i32(temporary0 + 0xE0, load_i32(temporary0 + 0xE0) | 0x3) # update_flags
			 end
		   end
		 end
8002EFB8 if load_i32(temporary0 + 0x38) & 0x20 then # PitchVolumeSideChainEnabled
8002EFE0   store_i32(temporary0 + 0xE0, load_i32(temporary0 + 0xE0) | 0x3) # update_flags
8002EFE8   argument0 = u32((i32(load_u16(temporary0 + 0xFFF0) << 17) >> 16) * load_i32(temporary0 + 0x2C)) >> 7 # ???, master_volume
		 end
8002EFF8 if load_i32(temporary0 + 0xE0) & 0x3 then # VolumeLeft | VolumeRight
8002F028   argument0 = i32((argument0 + load_i16(temporary0 + 0xD8)) * (load_u16(0x80062F5E) & 0x7F)) >> 7
8002F024   if load_u16(0x800AC5A8) then
8002F02C     value1 = argument2 & 0xFFFF
8002F058     argument0 = i32(argument0 * load_i16(0x8009C5A2 + (((value1 << 1) + value1) << 2))) >> 7
		   end
8002F06C   argument3 = (u32(load_u16(temporary0 + 0x60)) >> 8) + load_i16(temporary0 + 0xDA) & 0xFF
8002F074   value1 = load_i32(0x800AA104)
8002F07C   if value1 != 1 then
		     if value1 == 4 then
8002F088       value1 = argument3 << 1
8002F104       store_i16(temporary0 + 0x104, i32(argument0 * load_i16(0x80049C44 + value1)) >> 15)
8002F11C       Low = argument0 * load_i16(0x80049E44 + value1)
8002F128       value1 = argument1 & 0xAAAAAA
8002F130       value0 = i32(Low) >> 15
8002F138       store_i16(temporary0 + 0x106, value0)
8002F134       if value1 != 0 then
8002F144         store_i16(temporary0 + 0x106, ~value0)
8002F168         value0 = u32(argument0 * load_i16(0x80059CC4)) >> 15
8002F16C         store_i16(temporary0 + 0x106, value0)
8002F140       else
8002F150         value0 = ~load_u16(temporary0 + 0x104)
		       end
8002F170       store_i16(temporary0 + 0x104, value0)
             end
8002F08C   else
8002F094     value1 = argument3 << 1
8002F0B8     store_i16(temporary0 + 0x104, i32(argument0 * load_i16(0x80049C44 + value1)) >> 15)
8002F0E0     store_i16(temporary0 + 0x106, i32(argument0 * load_i16(0x80049E44 + value1)) >> 15)
8002F0DC   end
		 end
8002F180 if load_i32(temporary0 + 0x38) & 0x10 then # PlaybackRateSideChainEnabled
8002F18C   argument3 = load_u8(0x80062FE6)
8002F1A4   value1 = load_u16(temporary0 + 0xFFF0) + load_i16(temporary0 + 0xD6) + load_i16(temporary0 + 0x36)
8002F1A0   if argument3 != 0 then
8002F1B0     Low = value1 * argument3
8002F1AC     if argument3 < 128 then
8002F1C0       value1 += i32(Low) >> 7
8002F1BC     else
8002F1C8       value1 = i32(Low) >> 8
             end
		   end
8002F1D4   store_i16(temporary0 + 0xF8, value1 & 0x3FFF)
8002F1E0   store_i32(temporary0 + 0xE0, load_i32(temporary0 + 0xE0) | 0x10)
8002F1DC else if load_i32(temporary0 + 0xE0) & 0x10 then # VoicePitch
8002F1FC   argument3 = load_u8(0x80062FE6)
8002F214   value1 = load_i16(temporary0 + 0xD6) + load_i32(temporary0 + 0x30) + load_i16(temporary0 + 0x36)
8002F210   if argument3 then
8002F220     Low = value1 * argument3
8002F21C     if argument3 < 128 then
8002F230       value1 += i32(Low) >> 7
8002F22C     else
8002F238       value1 = i32(Low) >> 8
             end
		   end
8002F240   store_i16(temporary0 + 0xF8, value1 & 0x3FFF)
		 end
8002F244 jump returnAddress
*/

}

void AkaoExecDec::akaoDspMain()
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

	if (_player2.active_voices) {
		const uint32_t global_active_voices = ~(_active_voices | _spuActiveVoices) & _voicesMask62F68;
		const uint32_t active_voices = _player2.active_voices & (global_active_voices & _player2.keyed_voices);
		spu_key = global_active_voices & _player2.key_on_voices;

		for (channel_t voice = 0; voice < AKAO_CHANNEL_MAX; ++voice) {
			const uint32_t voice_mask = 1 << voice;
			if (active_voices & voice_mask) {
				akaoCalculateVolumeAndPitch(_playerTracks2[voice], voice_mask, voice);
				if (int(_playerTracks2[voice].spuRegisters.update_flags) != 0) {
					if (_playerTracks2[voice].voice_effect_flags & AkaoVoiceEffectFlags::OverlayVoiceEnabled) {
						akaoDspOverlayVoice(_playerTracks2[voice], global_active_voices, _playerTracks2[voice].overlay_voice_num - 24);
						continue;
					}
					if (_playerTracks2[voice].voice_effect_flags & AkaoVoiceEffectFlags::AlternateVoiceEnabled) {
						if (_player2.key_on_voices & voice_mask) {
							_playerTracks2[voice].voice_effect_flags ^= AkaoVoiceEffectFlags::Unknown400;
							_playerTracks2[voice].spuRegisters.update_flags |= AkaoUpdateFlags::VolumeLeft | AkaoUpdateFlags::VolumeRight
								| AkaoUpdateFlags::VoicePitch | AkaoUpdateFlags::VoiceStartAddr
								| AkaoUpdateFlags::AdsrAttackMode | AkaoUpdateFlags::AdsrSustainMode
								| AkaoUpdateFlags::AdsrReleaseMode | AkaoUpdateFlags::AdsrAttackRate
								| AkaoUpdateFlags::AdsrDecayRate | AkaoUpdateFlags::AdsrSustainRate
								| AkaoUpdateFlags::AdsrReleaseRate | AkaoUpdateFlags::AdsrSustainLevel
								| AkaoUpdateFlags::VoiceLoopStartAddr;
						}
						if (_playerTracks2[voice].voice_effect_flags & AkaoVoiceEffectFlags::Unknown400) {
							if (global_active_voices & (1 << _playerTracks2[voice].alternate_voice_num)) {
								akaoWriteSpuRegisters(_playerTracks2[voice].alternate_voice_num, _playerTracks2[voice].spuRegisters);
								if (spu_key & voice_mask) {
									spu_key = (spu_key | (1 << _playerTracks2[voice].alternate_voice_num)) & ~voice_mask;
								}
							}
							continue;
						}
					}
					akaoWriteSpuRegisters(_playerTracks2[voice].spuRegisters.voice, _playerTracks2[voice].spuRegisters);
				}
			}
		}

		_player2.key_on_voices = 0;
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

	if (_active_voices) {
		const uint32_t active_voices = _active_voices & _keyed_voices;
		spu_key |= _key_on_voices;

		for (channel_t voice = 0; voice < 8; ++voice) {
			const uint32_t voice_mask = 0x10000 << voice;
			if (active_voices & voice_mask) {
				akaoCalculateVolumeAndPitch(_playerTracks3[voice], voice_mask);
				if (int(_playerTracks3[voice].spuRegisters.update_flags) != 0) {
					akaoWriteSpuRegisters(_playerTracks3[voice].spuRegisters.voice, _playerTracks3[voice].spuRegisters);
				}
			}
		}

		_key_on_voices = 0;
	}

	if (spu_key) {
		SpuSetKey(SPU_ON, spu_key);
	}
}

void AkaoExecDec::akaoDspOverlayVoice(AkaoPlayerTrack& playerTracks, uint32_t active_voices, uint32_t voice)
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

void AkaoExecDec::akaoDspOnTick(AkaoPlayerTrack& playerTracks, AkaoPlayer& player, uint32_t voice)
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

void AkaoExecDec::akaoUnknown2E954(AkaoPlayerTrack& playerTracks, uint32_t voice)
{
	// TODO
	spuNoiseVoice();
	// TODO
	spuPitchLFOVoice();
	// TODO
}

void AkaoExecDec::akaoWriteSpuRegisters(int32_t voice, SpuRegisters &spuRegisters)
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

void AkaoExecDec::akaoLoadTracks()
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


/*
80029C48 stackPointer = u32(stackPointer) + 4294967248
80029C4C store_i32(stackPointer + 0x18, saved2)
80029C60 store_i32(stackPointer + 0x28, returnAddress)
80029C64 store_i32(stackPointer + 0x24, saved5)
80029C68 store_i32(stackPointer + 0x20, saved4)
80029C6C store_i32(stackPointer + 0x1C, saved3)
80029C70 store_i32(stackPointer + 0x14, saved1)
80029C74 store_i32(stackPointer + 0x10, saved0)
80029C7C saved2 = 0x80083584
80029C84 saved3 = 0x80096608
80029C9C store_i32(0x800AA114, load_i32(0x800AA114) | 0xFFFFFF)
80029CA4 store_i32(0x800AA108, load_i32(0x80083584) & 0xFFFFFF)
80029CA8 if saved1 != 0 then
80029CAC   saved4 = 1
80029CB0   saved5 = 16384
80029CB4   saved0 = u32(saved3) + 164
		   do
80029CB8     value0 = saved1 & saved4
80029CC0     argument0 = saved3
80029CBC     if value0 != 0 then
80029CC4       value0 = load_u16(saved2)
80029CC8       saved2 = u32(saved2) + 2
80029CCC       saved1 = saved1 ^ saved4
80029CD0       argument1 = 20
80029CD4       value0 = value0 + saved2
80029CD8       store_i32(saved3, value0)
80029CDC       value0 = 259
80029CE0       store_i16(saved0 + 0xFFB2, value0)
80029CE4       value0 = 127
80029CEC       store_i32(saved0 + 0xFF88, value0)
80029CE8       returnAddress = pc + 8; jump 0x31820
80029CF0       value0 = 0x3FFF0000
80029CF4       store_i32(saved0 + 0xFFA0, value0)
80029CFC       value0 = 0x80083580
80029D00       store_i16(saved0 + 0x22, saved5)
80029D04       store_i16(saved0 + 0xFFBC, saved5)
80029D08       store_i32(saved0 + 0xFF70, value0)
80029D0C       store_i16(saved0 + 0x2A, 0)
80029D10       store_i16(saved0 + 0x28, 0)
80029D14       store_i32(saved0 + 0xFF90, 0)
80029D18       store_i16(saved0 + 0x2E, 0)
80029D1C       store_i16(saved0 + 0xFFC0, 0)
80029D20       store_i16(saved0 + 0xFFC8, 0)
80029D24       store_i16(saved0 + 0x20, 0)
80029D28       store_i16(saved0 + 0x1E, 0)
80029D2C       store_i16(saved0 + 0xFFBA, 0)
80029D30       store_i16(saved0 + 0xFFB8, 0)
80029D34       store_i16(saved0 + 0xFFBE, 0)
80029D38       store_i16(saved0 + 0xFFC8, 0)
80029D3C       store_i32(saved0 + 0xFF94, 0)
80029D40       store_i16(saved0 + 0x14, 0)
80029D44       store_i16(saved0 + 0xFFCA, 0)
80029D48       store_i16(saved0 + 0x36, 0)
80029D4C       store_i16(saved0 + 0xFFFA, 0)
80029D50       store_i16(saved0 + 0xFFEC, 0)
80029D54       store_i16(saved0 + 0xFFDA, 0)
80029D58       store_i16(saved0 + 0xFFFC, 0)
80029D5C       store_i16(saved0 + 0xFFEE, 0)
80029D60       store_i16(saved0 + 0xFFDC, 0)
80029D64       store_i16(saved0 + 0x2, 0)
80029D68       store_i16(saved0, 0)
			 end
80029D6C     saved0 = u32(saved0) + 264
80029D70     saved3 = u32(saved3) + 264
80029D78     saved4 = saved4 << 1
80029D74   while saved1 != 0
		 end
80029D84 value0 = 0xFFFF0000
80029D88 assemblerTemporary = 0x800A0000
80029D8C store_i32(assemblerTemporary + 0xA11C, value0)
80029D90 value0 = 1
80029D94 assemblerTemporary = 0x800A0000
80029D98 store_i32(assemblerTemporary + 0xA124, value0)
80029D9C assemblerTemporary = 0x800A0000
80029DA0 store_i16(assemblerTemporary + 0xA14C, 0)
80029DA4 assemblerTemporary = 0x800A0000
80029DA8 store_i32(assemblerTemporary + 0xA144, 0)
80029DAC assemblerTemporary = 0x800A0000
80029DB0 store_i16(assemblerTemporary + 0xA154, 0)
80029DB4 assemblerTemporary = 0x800A0000
80029DB8 store_i32(assemblerTemporary + 0xA148, 0)
80029DBC assemblerTemporary = 0x800A0000
80029DC0 store_i32(assemblerTemporary + 0xA13C, 0)
80029DC4 assemblerTemporary = 0x800A0000
80029DC8 store_i16(assemblerTemporary + 0xA160, 0)
80029DCC assemblerTemporary = 0x800A0000
80029DD0 store_i16(assemblerTemporary + 0xA15E, 0)
80029DD4 assemblerTemporary = 0x800A0000
80029DD8 store_i16(assemblerTemporary + 0xA15C, 0)
80029DDC assemblerTemporary = 0x800A0000
80029DE0 store_i16(assemblerTemporary + 0xA162, 0)
80029DE4 assemblerTemporary = 0x800A0000
80029DE8 store_i32(assemblerTemporary + 0xA130, 0)
80029DEC assemblerTemporary = 0x800A0000
80029DF0 store_i32(assemblerTemporary + 0xA134, 0)
80029DF4 assemblerTemporary = 0x800A0000
80029DF8 store_i32(assemblerTemporary + 0xA138, 0)
80029DFC assemblerTemporary = 0x800A0000
80029E00 store_i16(assemblerTemporary + 0xA152, 0)
80029E04 assemblerTemporary = 0x800A0000
80029E08 store_i16(assemblerTemporary + 0xA150, 0)
80029E0C assemblerTemporary = 0x800A0000
80029E10 store_i16(assemblerTemporary + 0xA158, 0)
80029E14 assemblerTemporary = 0x800A0000
80029E18 store_i32(assemblerTemporary + 0xA110, 0)
80029E1C assemblerTemporary = 0x800A0000
80029E20 store_i32(assemblerTemporary + 0xA10C, 0)
80029E24 assemblerTemporary = 0x800A0000
80029E28 store_i32(assemblerTemporary + 0xA12C, 0)
80029E2C assemblerTemporary = 0x800A0000
80029E30 store_i32(assemblerTemporary + 0xA128, 0)
80029E38 if load_i32(0x80062FF8) & 0x1 then
80029E54   store_i32(0x800AA118, load_i32(0x800AA108))
80029E4C   store_i32(0x800AA108, 0)
		 end
80029E58 returnAddress = pc + 8; jump 0x2FF4C
80029E60 returnAddress = pc + 8; jump 0x30038
80029E68 returnAddress = pc + 8; jump 0x30148
80029E70 returnAddress = load_i32(stackPointer + 0x28)
80029E74 saved5 = load_i32(stackPointer + 0x24)
80029E78 saved4 = load_i32(stackPointer + 0x20)
80029E7C saved3 = load_i32(stackPointer + 0x1C)
80029E80 saved2 = load_i32(stackPointer + 0x18)
80029E84 saved1 = load_i32(stackPointer + 0x14)
80029E88 saved0 = load_i32(stackPointer + 0x10)
80029E8C stackPointer = u32(stackPointer) + 48
80029E90 jump returnAddress
*/
}

void AkaoExecDec::akaoStopMusic()
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

void AkaoExecDec::stopMusic()
{
	// TODO
}

void AkaoExecDec::akaoPlayMusic()
{
	akaoTransferSeqBody();
	if (_player.song_id == 14) // Main theme
	{
		// Save music current position for next time
		//save_sub_8002A7E8();
		//save_sub_8002B1A8(_akaoPlayerTracks, dword_800804D0, &_player, &dword_80083394);
	}
	akaoStopMusic();
	/* if (_remember_song_id_1 != 0 && _remember_song_id_1 == _akao.songId()) {
		resume_sub_8002AABC(0);
	}
	else if (_remember_song_id_2 != 0 && _remember_song_id_2 == _akao.songId()) {
		resume_sub_8002AABC(1);
	}
	else { */
		akaoLoadTracks();
	//}
	_player.song_id = _akao.songId();
}

int AkaoExecDec::akaoTransferSeqBody()
{
	// Copy _akao data to _akaoDataBeforeChannelMask
}

void AkaoExecDec::akaoPause()
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

bool AkaoExecDec::akaoDispatchVoice(AkaoPlayerTrack& playerTrack, AkaoPlayer &player, uint32_t voice)
{
	uint8_t instruction = 160;

	do {
		playerTrack.addr += 1;
		instruction = *playerTrack.addr;
		if (instruction < 160) {
			break;
		}

		executeChannel(playerTrack, player, voice);
	} while (instruction != 160);

	if (instruction == 160) {
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

void AkaoExecDec::akaoDispatchMessages()
{
	if (_unknown62F8C == 0) {
		while (!_akaoMessageQueue.empty()) {
			const AkaoMessage& message = _akaoMessageQueue.front();
			switch (message.opcode) {
			case 0x10: akaoPlayMusic(); break;
			default: break;
			}
			_akaoMessageQueue.pop();
		}
	}

/*
8002E1BC if load_i32(0x80062F8C) == 0 then
8002E1D0   saved0 = 0x80081DC8
8002E1D4   while load_i32(0x80063010) then
8002E1F4       value0 = load_i32(0x80049548 + (load_u8(saved0) * 4))
8002E200       argument0 = saved0
8002E1FC       returnAddress = pc + 8; jump value0
8002E218       store_i32(0x80063010, load_i32(0x80063010) - 1)
8002E220       saved0 += 36
8002E21C   end
		 end
*/
}

uint32_t AkaoExecDec::akaoReadNextNote(AkaoPlayerTrack& playerTrack)
{
	while (true) {
		const uint8_t op = *playerTrack.addr;
		if (op < 154) {
			if (op >= 143) {
				playerTrack.portamento_speed = 0;
				playerTrack.legato_flags &= ~5;
			}
			return op;
		}

		if (op >= 160) {
			const uint8_t len = _opcode_len[op - 160];
			if (len != 0) {
				playerTrack.addr += len;
				continue;
			}

			if (op < 242) {
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
						playerTrack.addr += 3 + *(uint16_t *)(playerTrack.addr + 1);
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

bool AkaoExecDec::akaoMain()
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

	active_voices = _player2.active_voices;

	if (active_voices) {
		uint32_t tempo = _player2.tempo >> 16;

		if (_unknown62FEA != 0) {
			if (_unknown62FEA < 128) {
				tempo += (tempo * _unknown62FEA) >> 7;
			}
			else {
				tempo = (tempo * _unknown62FEA) >> 8;
			}
		}

		_player2.time_counter += tempo;

		if (_player2.time_counter & 0xFFFF0000 || _unknownFlags62FF8 & 0x4) {
			_player2.time_counter &= 0xFFFF;
			_unknown62F04 = 1;
			uint32_t voice = 1;
			channel_t channel = 0;

			do {
				if (active_voices & voice) {
					_playerTracks2[channel].delta_time_counter -= 1;
					_playerTracks2[channel].gate_time_counter -= 1;

					if (_playerTracks2[channel].delta_time_counter == 0) {
						if (akaoDispatchVoice(_playerTracks2[channel], _player2, voice)) {
							return false;
						}
					}
					else if (_playerTracks2[channel].gate_time_counter == 0) {
						_playerTracks2[channel].gate_time_counter = 1;
						_player2.key_off_voices |= voice;
						_player2.keyed_voices &= ~voice;
					}

					akaoDspOnTick(_playerTracks2[channel], _player2, voice);

					active_voices ^= voice;
				}
				voice <<= 1;
				channel += 1;
			} while (active_voices);

			if (_player2.tempo_slide_length) {
				_player2.tempo_slide_length -= 1;
				_player2.tempo += _player2.tempo_slope;
			}
		}
	}

	active_voices = _active_voices;

	if (active_voices) {
		_time_counter += _tempo >> 16;

		if (_time_counter & 0xFFFF0000 || _unknownFlags62FF8 & 0x4) {
			_time_counter &= 0xFFFF;
			uint32_t voice = 0x10000;
			channel_t channel = 0;

			do {
				if (active_voices & voice) {
					if (!(_unknownFlags62FF8 & 0x2)) {
						_playerTracks3[channel].field_50 += 1;
						_playerTracks3[channel].delta_time_counter -= 1;
						_playerTracks3[channel].gate_time_counter -= 1;

						if (_playerTracks3[channel].delta_time_counter == 0) {
							if (!akaoDispatchVoice(_playerTracks3[channel], _player, voice)) {
								return false;
							}
						}
						else if (_playerTracks3[channel].gate_time_counter == 0) {
							_playerTracks3[channel].gate_time_counter = 1;
							_key_off_voices |= voice;
							_keyed_voices &= ~voice;
						}

						akaoUnknown2E954(_playerTracks3[channel], voice);
					}

					active_voices ^= voice;
				}
				voice <<= 1;
				channel += 1;
			} while (active_voices);
		}
	}

	if (_player.pause) {
		akaoPause();
		_player.pause = 0;
	}

	akaoDispatchMessages();
	akaoUnknown30380();
	akaoKeyOffVoices();

	return true;
}

void AkaoExecDec::turnOffCdInputReverb()
{
	_spuCommonAttr.mask = SPU_COMMON_CDVOLL | SPU_COMMON_CDVOLR | SPU_COMMON_CDREV;
	_spuCommonAttr.cd.reverb = SPU_OFF;
	_spuCommonAttr.cd.volume.right = 0;
	_spuCommonAttr.cd.volume.left = 0;
	return SpuSetCommonAttr(&_spuCommonAttr);
}

void AkaoExecDec::akaoKeyOffVoices()
{
	int32_t voice_bit = 0;

	if (_player2.active_voices != 0) {
		const int32_t mask = (_player2.key_off_voices & _voicesMask62F68) & ~(_active_voices | _spuActiveVoices);
		if (mask != 0) {
			voice_bit = getVoicesBits(_playerTracks2, voice_bit, mask);
		}
		_player2.key_off_voices = 0;
	}

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

void AkaoExecDec::akaoUnknown30380()
{
	_unknown62E0A += 1;

	if (_unknown62E0A & 0x3 == 0) {
		if (_unknown62FCC) {
			_unknown62FCC -= 1;
			_unknown62FD4 += _unknown62FB4;
			turnOffCdInputReverb();
		}

		if (_unknownFlags62FF8 & 0x1 == 0) {
			if (_unknown62F44) {
				_unknown62F44 = _unknown62F44 - 1;
				int saved = _unknown62F5C + _unknown62F2C;
				if (_unknown62F44 << 16 == 0 && saved & 0x7F0000 == 0 && _unknown62F2C < 0) {
					stopMusic();
				}
				else if (saved & 0x7F0000 != _unknown62F5C & 0x7F0000) {
					setAkaoVolume(saved & 0x7F0000);
				}
				_unknown62F5C = saved;
			}

			if (_unknown62F48) {
				_unknown62F48 -= 1;
				_unknown62FE8 += _unknown62F30;
			}

			if (_unknown62F40) {
				_unknown62F40 -= 1;
				const int saved = _unknown62FE4 + _unknown62F28;
				if (saved & 0xFF0000 != _unknown62FE4 & 0xFF0000) {
					for (int voice = 0; voice < AKAO_CHANNEL_MAX; ++voice) {
						_playerTracks[voice].spuRegisters.update_flags |= AkaoUpdateFlags::VoicePitch;
					}
				}
				_unknown62FE4 = saved;
			}
		}

		if (_active_voices != 0) {
			uint32_t active_voices = _active_voices;
			uint32_t voice_mask = 0x10000;
			uint8_t voice = 0;
			do {
				if (active_voices & voice_mask) {
					AkaoPlayerTrack& playerTrack = _playerTracks[voice];
					if (playerTrack.overlay_balance_slide_length != 0) {
						playerTrack.overlay_balance_slide_length -= 1;

						int saved0 = playerTrack.overlay_balance + playerTrack.overlay_balance_slope;

						if (playerTrack.overlay_balance_slide_length == 0
								&& saved0 & 0xFF00 == 0
								&& playerTrack.overlay_balance_slope < 0) {
							_key_off_voices |= voice_mask;
							_key_on_voices &= ~voice_mask;
							_keyed_voices &= ~voice_mask;
							playerTrack.addr = (uint8_t *)&AkaoExecDec::_akaoEndSequence;
						}
					}


					active_voices ^= voice_mask;
				}

				voice_mask <<= 1;
				voice += 1;
			} while (active_voices != 0);
		}
	}
/*
800303AC value0 = load_u16(0x80062E0A) + 1
800303B4 store_i16(0x80062E0A, value0)
800303BC if value0 & 0x3 == 0 then
800303C8   value0 = load_i16(0x80062FCC)
800303D0   if value0 then
800303E4     argument0 = load_i32(0x80062FB4)
800303F0     store_i16(0x80062FCC, value0 - 1)
800303FC     store_i32(0x80062FD4, load_i32(0x80062FD4) + argument0)
80030400     returnAddress = pc + 8; jump 0x2E428
		   end
80030418   if load_i32(0x80062FF8) & 0x1 == 0 then
80030424     value0 = load_i16(0x80062F44)
8003042C     if value0 != 0 then
80030430       argument0 = value0
80030440       argument1 = load_i32(0x80062F2C)
80030444       value0 = u32(argument0) - 1
8003044C       store_i16(0x80062F44, value0)
80030458       saved0 = load_i32(0x80062F5C) + argument1
80030454       if value0 << 16 == 0 then
80030464         if saved0 & 0x7F0000 == 0 then
8003046C           branch argument1 >= 0 jump 0x30488
80030474           returnAddress = pc + 8; jump 0x29F44 # stopMusic
8003047C           jump 0x304A8
				 end
			   end
80030490       argument0 = saved0 & 0x7F0000
80030498       if argument0 != load_i32(0x80062F5C) & 0x7F0000 then
800304A0         returnAddress = pc + 8; jump 0x2A748 # setAkaoVolume
			   end
800304AC       store_i32(0x80062F5C, saved0)
			 end
800304B4     value0 = load_i16(0x80062F48)
800304BC     if value0 != 0 then
800304D0       argument0 = load_i32(0x80062F30)
800304DC       store_i16(0x80062F48, u32(value0) - 1)
800304E8       store_i32(0x80062FE8, load_i32(0x80062FE8) + argument0)
			 end
800304F0     _unknown62F40 = load_i16(0x80062F40)
800304F8     if _unknown62F40 then
80030504       _unknown62FE4 = load_i32(0x80062FE4)
80030518       store_i16(0x80062F40, u32(_unknown62F40) - 1)
80030520       saved0 = _unknown62FE4 + load_i32(0x80062F28)
8003052C       if saved0 & 0xFF0000 != _unknown62FE4 & 0xFF0000 then
80030534         i = 24
8003053C         value1 = 0x800966E8 # _playerTracks[0].spuRegisters.update_flags
				 do
80030544           i -= 1
8003054C           store_i32(value1, load_i32(value1) | 0x10)
80030554           value1 += 264
80030550         while i != 0
			   end
8003055C       store_i32(0x80062FE4, saved0)
			 end
		   end
80030564   argument2 = load_i32(0x800A9FCC) # _player3.active_voices
8003056C   if argument2 != 0 then
80030578     _playerTracks3 = 0x80099788
8003057C     voice_mask = 0x10000
			 do
8003058C       if argument2 & voice_mask then
8003059C         if load_u16(_playerTracks3 + 0x5E) then
800305AC           overlay_balance_slope = load_i16(_playerTracks3 + 0xC8)
800305B4           store_i16(_playerTracks3 + 0x5E, load_u16(_playerTracks3 + 0x5E) - 1)
800305C4           saved0 = load_i16(_playerTracks3 + 0xC6) + overlay_balance_slope
800305C0           if load_u16(_playerTracks3 + 0x5E) == 0
800305CC               && saved0 & 0xFF00 == 0
800305D4               && overlay_balance_slope < 0 then
800305F0             store_i32(_key_off_voices, voice_mask | _key_off_voices)
80030610             store_i32(_key_on_voices, ~voice_mask & _key_on_voices)
80030620             store_i32(_keyed_voices, ~voice_mask & _keyed_voices)
80030628             store_i32(_playerTracks3, 0x80049C40) # _akaoEndSequence
80030638           else if saved0 & 0xFF00 != load_i16(_playerTracks3 + 0xC6) & 0xFF00 then
8003064C             store_i32(_playerTracks3 + 0xE0, load_i32(_playerTracks3 + 0xE0) | 0x3)
				   end
L80030650          store_i16(_playerTracks3 + 0xC6, saved0)
				 end
8003065C         if load_u16(_playerTracks3 + 0x62) then
80030674           saved0 = load_u16(_playerTracks3 + 0x60) + load_i16(_playerTracks3 + 0xCA)
80030678           store_i16(_playerTracks3 + 0x62, load_u16(_playerTracks3 + 0x62) - 1)
80030688           if saved0 & 0xFF00 != load_u16(_playerTracks3 + 0x60) & 0xFF00 then
8003069C             store_i32(_playerTracks3 + 0xE0, load_i32(_playerTracks3 + 0xE0) | 0x3)
				   end
800306A0           store_i16(_playerTracks3 + 0x60, saved0)
				 end
800306AC         if load_u16(_playerTracks3 + 0x5A) then
800306B8           value1 = load_i32(_playerTracks3 + 0x3C)
800306C4           saved0 = value1 + load_i32(_playerTracks3 + 0x40)
800306C8           store_i16(_playerTracks3 + 0x5A, load_u16(_playerTracks3 + 0x5A) - 1)
800306D4           if saved0 & 0xFF00 != value1 & 0xFF00 then
800306E8             store_i32(_playerTracks3 + 0xE0, load_i32(_playerTracks3 + 0xE0) | 0x10)
				   end
800306EC           store_i32(_playerTracks3 + 0x3C, saved0)
				 end
800306F0         argument2 ^= voice_mask
			   end
800306F4       voice_mask <<= 1
800306F8       _playerTracks3 += 264
800306FC     while argument2 != 0
		   end
80030714   if load_u16(0x8009C5A8) then
80030718     saved6 = 0x8009C5A0
80030720     _playerTracks2 = 0x80097EC8
80030724     saved1 = 1
8003072C     saved2 = 0x8009A168 # _player2.active_voices
80030730     saved0 = 0x8009C5A8 # _spuCommonAttr + 0x30
80030734     saved3 = 0x800966E8 # _playerTracks.spuRegisters.update_flags
80030738     saved5 = 0x80097FA8
L8003073C:   do
80030744      if load_u16(saved0) then
80030758       store_i16(saved0, load_u16(saved0) - 1)
80030768       store_i32(saved6, load_i32(saved6) + load_i32(saved0 + 0xFFFC))
80030778       store_i32(saved5, load_i32(saved5) | 0x3)
80030788       store_i32(saved3, load_i32(saved3) | 0x3)
80030794       if load_u16(saved0) == 0 then
800307A0         argument0 = load_i32(0x80062F68)
800307AC         if saved1 & argument0 then
800307BC           store_i32(0x80062F68, saved1 ^ argument0)
800307C4           if saved1 & load_i32(saved2) then
800307C8             argument0 = _playerTracks2
800307CC             argument1 = u32(saved2) - 4
800307D4             argument2 = saved1
800307D0             returnAddress = pc + 8; jump 0x33894 # finishChannel
800307E8             store_i32(saved2 + 0x4, ~saved1 & load_i32(saved2 + 0x4))
800307F4             store_i32(saved2 + 0x8, ~saved1 & load_i32(saved2 + 0x8))
800307FC             store_i32(saved2 + 0xC, saved1 | load_i32(saved2 + 0xC))
				   end
80030810           if saved1 & load_i32(0x800AA108) then
80030824             store_i32(saved3, load_i32(saved3) | 0x1FF90)
80030838             store_i32(saved2 + 0xFFA4, (saved1 & load_i32(saved2 + 0xFFA8)) | load_i32(saved2 + 0xFFA4))
				   end
80030844           value1 = load_u16(0x80062FC8)
80030848           value0 = 0x7F8000
8003084C           Low, High = value0 / value1
80030850           if value1 == 0 then
80030858             breakpoint(7168)
				   end
80030860           if value1 == -1 then
80030868             if value0 == 0x80000000 then
80030870               breakpoint(6144)
					 end
				   end
80030878           store_i16(saved0, value1)
8003087C           store_i32(saved0 + 0xFFFC, Low)
				 end
			   end
80030880       saved1 <<= 1
80030884       saved5 += 264
80030888       _playerTracks2 += 264
8003088C       saved3 += 264
80030890       saved0 += 12
800308A4       saved6 += 12
			 end
		    end
800308A0   while saved1 & 0xFFFFFF
		 end
*/
}

void AkaoExecDec::akaoInit(uint32_t* spuTransferStartAddr, AkaoInstrAttr* spuTransferStopAddr)
{
	SpuInitMalloc(4, &_spuMemoryManagementTable[0]);
	SpuMallocWithStartAddr(0x77000, 8192);
	SpuSetTransferMode(SPU_TRANSFER_BY_DMA);
	akaoLoadInstrumentSet(spuTransferStartAddr, spuTransferStopAddr);
	SpuSetTransferStartAddr(0x76FE0);
	akaoTransferSamples(_empty_adpcm, EMPTY_ADPCM_LEN);
	akaoWaitForSpuTransfer();
	akaoReset();

	uint32_t spec = RCntCNT2; // one-eighth of system clock = ~0.24 microseconds
	 
	do {
		_timeEventDescriptor = OpenEvent(spec, EvSpINT, EvMdINTR, akaoTimerCallback);
	} while (_timeEventDescriptor == -1);

	while (!EnableEvent(_timeEventDescriptor)) {}
	
	uint16_t target = 17361; // 17361 * ~0.24 microseconds = ~0.00416664 seconds
	while (!SetRCnt(spec, target, RCntMdINTR)) {}

	while (!StartRCnt(spec)) {}
}

void AkaoExecDec::akaoLoadInstrumentSet(uint32_t* spuTransferStartAddr, AkaoInstrAttr* instruments)
{
	SpuSetTransferStartAddr(*spuTransferStartAddr);
	akaoTransferSamples(&spuTransferStartAddr[4], spuTransferStartAddr[1]);
	for (int i = 0; i < 32; ++i) {
		memcpy(&_instruments[i], &instruments[i], sizeof(AkaoInstrAttr));
	}
	akaoWaitForSpuTransfer();
}

void AkaoExecDec::akaoLoadInstrumentSet2(uint32_t* spuTransferStartAddr, AkaoInstrAttr* spuTransferStopAddr)
{
	SpuSetTransferStartAddr(*spuTransferStartAddr);
	akaoTransferSamples(&spuTransferStartAddr[4], spuTransferStartAddr[1]);
	/*for (int i = 0; i < 32; ++i) {
		memcpy(&_instruments[i], &instruments[i], sizeof(AkaoInstrAttr));
	}*/
	akaoWaitForSpuTransfer();
}

// global_pointer + 0xC4
bool akaoSpuWaitForTransfer = false;

void akaoSpuTransferCallback()
{
	SpuSetTransferCallback(nullptr);
	akaoSpuWaitForTransfer = false;
}

void AkaoExecDec::akaoSpuSetTransferCallback()
{
	SpuTransferCallbackProc proc = akaoSpuTransferCallback;
	akaoSpuWaitForTransfer = true;
	SpuSetTransferCallback(proc);
}

void AkaoExecDec::akaoTransferSamples(uint8_t* data, uint32_t size)
{
	akaoSpuSetTransferCallback();
	SpuWrite(data, size);
}

void AkaoExecDec::akaoWaitForSpuTransfer()
{
	while (akaoSpuWaitForTransfer) {}
}

void AkaoExecDec::akaoClearSpu()
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

void AkaoExecDec::akaoSetReverbMode(uint8_t reverbType)
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

void AkaoExecDec::akaoReset()
{
	_global218 = 0x7F0000;
	_global290 = 0x7FFF0000;
	_player.stereo_mode = 1;
	_remember_song_id_2 = 0;
	_remember_song_id_1 = 0;
	_player2.song_id = 0;
	_player.song_id = 0;
	_unknown83398 = 0;
	_unknown83338 = 0;
	_active_voices = 0;
	_player2.active_voices = 0;
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

	for (uint8_t voiceNum = 16; voiceNum < 24; ++voiceNum) {
		const uint8_t i = voiceNum - 16;
		_playerTracks3[i].field_5A = 0;
		_playerTracks3[i].field_3C = 0;
		_playerTracks3[i].overlay_balance_slide_length = 0;
		_playerTracks3[i].voice_effect_flags = AkaoVoiceEffectFlags::None;
		_playerTracks3[i].field_50 = 0;
		_playerTracks3[i].alternate_voice_num = voiceNum;
		_playerTracks3[i].use_global_track = 1;
		_playerTracks3[i].spuRegisters.voice = voiceNum;
		_playerTracks3[i].overlay_balance = 0x7F00;
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

int AkaoExecDec::akaoPostMessage()
{
	int ret = 0;
	_unknown62F8C = 1;

	if (_akaoMessage == 37) {
		AkaoMessage message;
		message.opcode = 0x21;
		message.data[0] = _akaoDataAddr;
		message.data[1] = _unknown9A008;
		message.data[2] = _unknown9A008 + 1;
		_akaoMessageQueue.push(message);
	}
	else if ((_akaoMessage >= 20 && _akaoMessage < 22)
		|| _akaoMessage == 0x10 // Play and load akao
		|| (_akaoMessage >= 24 && _akaoMessage < 26)) {
		if (_player.song_id != _akao.songId()) {
			akaoSetReverbMode(_akao.reverbType());
			AkaoMessage message;
			message.opcode = _akaoMessage;
			message.data[0] = uint32_t(_akao.rawData() + 12 /* Before channel_mask */);
			message.data[1] = _akao.dataSize();
			message.data[2] = _akao.songId();
			message.data[3] = _unknown9A008;
			_akaoMessageQueue.push(message);
		}
		else {
			ret = 1;
		}
		/*
		LBL2DB74:
8002DB78   saved0 = load_i32(0x800AA004) # _akaoDataAddr
8002DB80   argument0 = load_u8(saved0)
		   if argument0 == 'A' then
8002DB98     if load_u8(saved0 + 0x1) == 'K' then
			   if load_u8(saved0 + 0x2) == argument0 then
8002DBB0         value1 = load_u8(saved0 + 0x3)
8002DBBC         saved0 += 4
8002DBB8         if value1 == 'O' then
8002DBC0           id = load_u16(saved0) # id
8002DBC4           saved0 += 2
8002DBC8           length = load_u16(saved0) # length
8002DBCC           saved0 += 2
8002DBD0           argument0 = load_u16(saved0) # reverb_type
8002DBE4           saved0 += 8
8002DBE0           if load_u16(0x800AA14E) != id & 0xFFFF then
8002DBE8             returnAddress = pc + 8; jump 0x29AF0
8002DBF4             argument0 = u32(stackPointer) + 16
8002DBF0             returnAddress = pc + 8; jump 0x2DA30
8002DBF8             value0 = load_i32(stackPointer + 0x10)
8002DC00             store_i32(value0 + 0x4, saved0)
8002DC04             store_i32(value0 + 0x8, length)
8002DC08             store_i32(value0 + 0xC, id & 0xFFFF)
8002DC18             store_i32(value0 + 0x10, load_i32(0x800AA008))
8002DC28             store_i16(value0, load_u16(0x800AA000)) # _akaoMessage
8002DC24           else
8002DC30             saved3 = 1
8002DC2C           end
				 end
			   end
			 end
		   else
8002DC38     saved3 = -1
           end
8002DC34   jump LBL2DF5C_end
		*/
	}
	else if (_akaoMessage == 36) {
		AkaoMessage message;
		message.opcode = 32;
		message.data[0] = _akaoDataAddr;
		message.data[1] = _unknown9A008;
		_akaoMessageQueue.push(message);
		/*
		LBL2DC3C:
8002DC40   argument0 = u32(stackPointer) + 16
8002DC3C   returnAddress = pc + 8; jump 0x2DA30
           value1 = load_i32(stackPointer + 0x10)
8002DC54   store_i32(value1 + 0x4, load_i32(0x800AA004)) # _akaoDataAddr
8002DC5C   argument0 = load_i32(0x800AA008)
8002DC64   value0 = 32
8002DC60   jump LBL2DE14
LBL2DE14:
8002DE14 store_i16(value1, value0)
8002DE1C store_i32(value1 + 0x8, argument0)
8002DE18 jump LBL2DF5C_end
		 end

		*/
	}
	else if (_akaoMessage < 39) { // Unreachable?
		AkaoMessage message;
		message.opcode = 34;
		message.data[0] = _akaoDataAddr;
		message.data[1] = _unknown9A008;
		message.data[2] = _unknown9A008 + 1;
		message.data[3] = _unknown9A008 + 2;
		_akaoMessageQueue.push(message);
		/*
		LBL2DCB0:
8002DCB4 argument0 = u32(stackPointer) + 16
8002DCB0 returnAddress = pc + 8; jump 0x2DA30
8002DCB8 argument0 = load_i32(stackPointer + 0x10)
8002DCC8 store_i32(argument0 + 0x4, load_i32(0x800AA004)) # _akaoDataAddr
8002DCD8 store_i32(argument0 + 0x8, load_i32(0x800AA008))
8002DCEC store_i32(argument0 + 0xC, u32(load_i32(0x800AA008)) + 1)
8002DCFC store_i16(argument0, 34)
8002DD08 store_i32(argument0 + 0x10, u32(load_i32(0x800AA008)) + 2)
8002DD04 jump LBL2DF5C_end
		*/
	}
	else if (_akaoMessage == 39) {
		AkaoMessage message;
		message.opcode = 35;
		message.data[0] = _akaoDataAddr;
		message.data[1] = _unknown9A008;
		message.data[2] = _unknown9A008 + 1;
		message.data[3] = _unknown9A008 + 2;
		message.data[4] = _unknown9A008 + 3;
		_akaoMessageQueue.push(message);
		/*
		LBL2DD0C:
8002DD10 argument0 = u32(stackPointer) + 16
8002DD0C returnAddress = pc + 8; jump 0x2DA30
8002DD14 argument0 = load_i32(stackPointer + 0x10)
8002DD24 store_i32(argument0 + 0x4, load_i32(0x800AA004)) # _akaoDataAddr
8002DD34 store_i32(argument0 + 0x8, load_i32(0x800AA008))
8002DD48 store_i32(argument0 + 0xC, u32(load_i32(0x800AA008)) + 1)
8002DD5C store_i32(argument0 + 0x10, u32(load_i32(0x800AA008)) + 2)
8002DD6C store_i16(argument0, 35)
8002DD78 store_i32(argument0 + 0x14, u32(load_i32(0x800AA008)) + 3)
8002DD74 jump LBL2DF5C_end
		*/
	}
	else if (_akaoMessage == 216) {
		AkaoMessage message;
		message.opcode = 208;
		message.data[0] = _akaoDataAddr;
		_akaoMessageQueue.push(message);
		AkaoMessage message;
		message.opcode = 212;
		message.data[0] = _akaoDataAddr;
		_akaoMessageQueue.push(message);
		/*
		
LBL2DD7C:
8002DD80 argument0 = u32(stackPointer) + 16
8002DD7C returnAddress = pc + 8; jump 0x2DA30
8002DD84 value1 = load_i32(stackPointer + 0x10)
8002DD90 argument0 = u32(stackPointer) + 16
8002DD94 store_i32(value1 + 0x4, load_i32(0x800AA004)) # _akaoDataAddr
8002DDA0 store_i16(value1, 208)
8002DD9C returnAddress = pc + 8; jump 0x2DA30
8002DDA4 value1 = load_i32(stackPointer + 0x10)
8002DDB4 store_i32(value1 + 0x4, load_i32(0x800AA004)) # _akaoDataAddr
8002DDBC value0 = 212
8002DDB8 jump LBL2DF54
LBL2DF54:
8002DF58 store_i16(value1, value0)
		*/
	}
	else if (_akaoMessage == 217) {
		AkaoMessage message;
		message.opcode = 209;
		message.data[0] = _akaoDataAddr;
		message.data[1] = _unknown9A008;
		_akaoMessageQueue.push(message);
		AkaoMessage message;
		message.opcode = 213;
		message.data[0] = _akaoDataAddr;
		message.data[1] = _unknown9A008;
		_akaoMessageQueue.push(message);
		/*
		
LBL2DDC0:
8002DDC4 argument0 = u32(stackPointer) + 16
8002DDC0 returnAddress = pc + 8; jump 0x2DA30
8002DDC8 value1 = load_i32(stackPointer + 0x10)
8002DDD4 argument0 = u32(stackPointer) + 16
8002DDD8 store_i32(value1 + 0x4, load_i32(0x800AA004)) # _akaoDataAddr
8002DDE0 argument1 = load_i32(0x800AA008)
8002DDE8 store_i16(value1, 209)
8002DDF0 store_i32(value1 + 0x8, argument1)
8002DDEC returnAddress = pc + 8; jump 0x2DA30
8002DE04 store_i32(load_i32(stackPointer + 0x10) + 0x4, load_i32(0x800AA004)) # _akaoDataAddr
8002DE0C argument0 = load_i32(0x800AA008)
8002DE10 value0 = 213
LBL2DE14:
8002DE14 store_i16(value1, value0)
8002DE1C store_i32(value1 + 0x8, argument0)
8002DE18 jump LBL2DF5C_end
		*/
	}
	else if (_akaoMessage == 218) {
		AkaoMessage message;
		message.opcode = 210;
		message.data[0] = _akaoDataAddr;
		message.data[1] = _unknown9A008;
		message.data[2] = _unknown9A00C;
		_akaoMessageQueue.push(message);
		AkaoMessage message;
		message.opcode = 214;
		message.data[0] = _akaoDataAddr;
		message.data[1] = _unknown9A008;
		message.data[2] = _unknown9A00C;
		_akaoMessageQueue.push(message);
	/*
	LBL2DE20:
8002DE24 argument0 = u32(stackPointer) + 16
8002DE20 returnAddress = pc + 8; jump 0x2DA30
8002DE28 value1 = load_i32(stackPointer + 0x10)
8002DE38 store_i32(value1 + 0x4, load_i32(0x800AA004)) # _akaoDataAddr
8002DE44 argument0 = u32(stackPointer) + 16
8002DE48 store_i32(value1 + 0x8, load_i32(0x800AA008))
8002DE50 argument1 = load_i32(0x800AA00C)
8002DE58 store_i16(value1, 210)
8002DE60 store_i32(value1 + 0xC, argument1)
8002DE5C returnAddress = pc + 8; jump 0x2DA30
8002DE64 value1 = load_i32(stackPointer + 0x10)
8002DE74 store_i32(value1 + 0x4, load_i32(0x800AA004)) # _akaoDataAddr
8002DE84 store_i32(value1 + 0x8, load_i32(0x800AA008))
8002DE94 store_i16(value1, 214)
8002DE9C store_i32(value1 + 0xC, load_i32(0x800AA00C))
8002DE98 jump LBL2DF5C_end
*/
	}
	else if (_akaoMessage == 153) {
		AkaoMessage message;
		message.opcode = 155;
		_akaoMessageQueue.push(message);
		AkaoMessage message;
		message.opcode = 157;
		_akaoMessageQueue.push(message);
	/*
	LBL2DEA0:
8002DEA4 argument0 = u32(stackPointer) + 16
8002DEA0 returnAddress = pc + 8; jump 0x2DA30
8002DEA8 argument0 = u32(stackPointer) + 16
8002DEB8 store_i16(load_i32(stackPointer + 0x10), 155)
8002DEB4 returnAddress = pc + 8; jump 0x2DA30
8002DEBC value1 = load_i32(stackPointer + 0x10)
8002DEC4 value0 = 157
8002DEC0 jump LBL2DF54
LBL2DF54:
8002DF58 store_i16(value1, value0)
	*/
	}
	else if (_akaoMessage == 152) {
		AkaoMessage message;
		message.opcode = 154;
		_akaoMessageQueue.push(message);
		AkaoMessage message;
		message.opcode = 156;
		_akaoMessageQueue.push(message);
		/*
		LBL2DEC8:
	8002DECC argument0 = u32(stackPointer) + 16
	8002DEC8 returnAddress = pc + 8; jump 0x2DA30
	8002DED0 argument0 = u32(stackPointer) + 16
	8002DEE0 store_i16(load_i32(stackPointer + 0x10), 154)
	8002DEDC returnAddress = pc + 8; jump 0x2DA30
	8002DEE4 value1 = load_i32(stackPointer + 0x10)
	8002DEEC value0 = 156
	8002DEE8 jump LBL2DF54
LBL2DF54:
8002DF58 store_i16(value1, value0)*/
	}
	else {
		AkaoMessage message;
		message.opcode = _akaoMessage;
		message.data[0] = _akaoDataAddr;
		message.data[1] = _unknown9A008;
		message.data[2] = _unknown9A00C;
		message.data[3] = _unknown9A010;
		message.data[4] = _unknown9A014;
		_akaoMessageQueue.push(message);
		/*
		LBL2DEF0:
8002DEF4 argument0 = u32(stackPointer) + 16
8002DEF0 returnAddress = pc + 8; jump 0x2DA30
8002DEF8 value1 = load_i32(stackPointer + 0x10)
8002DF08 store_i32(value1 + 0x4, load_i32(0x800AA004)) # _akaoDataAddr
8002DF18 store_i32(value1 + 0x8, load_i32(0x800AA008))
8002DF28 store_i32(value1 + 0xC, load_i32(0x800AA00C))
8002DF38 store_i32(value1 + 0x10, load_i32(0x800AA010))
8002DF48 store_i32(value1 + 0x14, load_i32(0x800AA014))
8002DF50 value0 = load_u16(0x800AA000) # _akaoMessage
LBL2DF54:
8002DF58 store_i16(value1, value0)
		*/
	}
	_unknown62F8C = 0;
	return ret;
	/*
	LBL2DF5C_end:
8002DF60 store_i32(0x80062F8C, 0)
8002DF64 value0 = saved3
	*/
/*
8002DA7C stackPointer = u32(stackPointer) + 4294967248
8002DA80 store_i32(stackPointer + 0x24, saved3)
8002DAA0 store_i32(stackPointer + 0x28, returnAddress)
8002DAA4 store_i32(stackPointer + 0x20, saved2)
8002DAA8 store_i32(stackPointer + 0x1C, saved1)
8002DAB0 store_i32(stackPointer + 0x18, saved0)

8002DA84 saved3 = 0
8002DA8C _akaoMessage = load_u16(0x800AA000) # _akaoMessage
8002DA98 store_i32(0x80062F8C, 1)
8002DAAC if _akaoMessage != 37 then
		   if _akaoMessage < 38 then
			 if _akaoMessage < 22 then
8002DAC8       branch _akaoMessage >= 20 jump LBL2DB74
8002DAD0       branch _akaoMessage == 16 jump LBL2DB74
8002DAD8       jump LBL2DEF0
			 end
8002DAE4     branch _akaoMessage < 24 jump LBL2DEF0
8002DAEC     branch _akaoMessage < 26 jump LBL2DB74
8002DAF4     branch _akaoMessage == 36 jump LBL2DC3C
8002DAFC     jump LBL2DEF0
		   end
8002DB08   branch _akaoMessage == 153 jump LBL2DEA0
		   if _akaoMessage < 154 then
8002DB1C     branch _akaoMessage == 39 jump LBL2DD0C
8002DB24     branch _akaoMessage < 39 jump LBL2DCB0
8002DB2C     branch _akaoMessage == 152 jump LBL2DEC8
8002DB34     jump LBL2DEF0
		   end
8002DB40   branch _akaoMessage == 217 jump LBL2DDC0
		   if _akaoMessage < 218 then
8002DB50     branch _akaoMessage == 216 jump LBL2DD7C
8002DB58     jump LBL2DEF0
		   end
8002DB64   branch _akaoMessage == 218 jump LBL2DE20
8002DB6C   jump LBL2DEF0
[...]
8002DF68 returnAddress = load_i32(stackPointer + 0x28)
8002DF6C saved3 = load_i32(stackPointer + 0x24)
8002DF70 saved2 = load_i32(stackPointer + 0x20)
8002DF74 saved1 = load_i32(stackPointer + 0x1C)
8002DF78 saved0 = load_i32(stackPointer + 0x18)
8002DF7C stackPointer = u32(stackPointer) + 48
*/
}

int32_t AkaoExecDec::getVoicesBits(AkaoPlayerTrack* playerTracks, int32_t voice_bit, int32_t mask)
{
	int i = 1;
	channel_t channel = 0; // FIXME: not incremented?

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

void AkaoExecDec::spuNoiseVoice()
{
	int32_t voice_bit = 0, mask;

	mask = (_player2.noise_voices & _voicesMask62F68) & ~(_active_voices | _spuActiveVoices);

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

void AkaoExecDec::spuReverbVoice()
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

void AkaoExecDec::spuPitchLFOVoice()
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

void AkaoExecDec::finishChannel(AkaoPlayerTrack& playerTrack, AkaoPlayer& player, int voice)
{
	if (playerTrack.use_global_track == 0) {
		int argument3 = voice ^ 0xFFFFFFFF;
		player.active_voices &= argument3;
		if (player.active_voices == 0) {
			player.song_id = 0;
		}
		player.noise_voices &= argument3;
		player.pitch_lfo_voices &= argument3;
		player.reverb_voices &= argument3;

		if (playerTrack.voice_effect_flags & AkaoVoiceEffectFlags::OverlayVoiceEnabled) {
			uint32_t overlay_voice_num = playerTrack.overlay_voice_num;
			if (_unknown62F04 != 0) {
				overlay_voice_num -= 24;
			}
			player.overlay_voices &= ~(1 << overlay_voice_num);
		}

		if (playerTrack.voice_effect_flags & AkaoVoiceEffectFlags::AlternateVoiceEnabled) {
			player.alternate_voices &= ~(1 << playerTrack.alternate_voice_num);
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

bool AkaoExecDec::loadInstrument(AkaoPlayerTrack& playerTrack, AkaoPlayer& player, int voice)
{
	const uint8_t instrument = playerTrack.addr[1];
	uint16_t overlay_voice_num = playerTrack.overlay_voice_num;

	if (_unknown62F04 != 0) {
		overlay_voice_num -= 24;
	}

	if (playerTrack.voice_effect_flags & AkaoVoiceEffectFlags::OverlayVoiceEnabled) {
		player.overlay_voices &= ~(1 << overlay_voice_num);
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

void AkaoExecDec::akaoSetInstrument(AkaoPlayerTrack& playerTrack, uint8_t instrument)
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

void AkaoExecDec::adsrDecayRate(AkaoPlayerTrack& playerTrack)
{
	OPCODE_ADSR_UPDATE(adsr_decay_rate, AkaoUpdateFlags::AdsrDecayRate);
}

void AkaoExecDec::adsrSustainLevel(AkaoPlayerTrack& playerTrack)
{
	OPCODE_ADSR_UPDATE(adsr_sustain_level, AkaoUpdateFlags::AdsrSustainLevel);
}

void AkaoExecDec::turnOnReverb(AkaoPlayerTrack& playerTrack, AkaoPlayer& player, int voice)
{
	if (playerTrack.use_global_track == 0) {
		player.reverb_voices |= voice;
	}
	else {
		_reverb_voices |= voice;
	}
	this->spuReverbVoice();
}

void AkaoExecDec::turnOffReverb(AkaoPlayerTrack& playerTrack, AkaoPlayer& player, int voice)
{
	if (playerTrack.use_global_track == 0) {
		player.reverb_voices &= ~voice;
	}
	else {
		_reverb_voices &= ~voice;
	}
	this->spuReverbVoice();
}

void AkaoExecDec::turnOnNoise(AkaoPlayerTrack& playerTrack, AkaoPlayer& player, int voice)
{
	if (playerTrack.use_global_track == 0) {
		player.noise_voices |= voice;
	}
	else {
		_noise_voices |= voice;
	}
	_player.spucnt |= SpuCnt::SetNoiseClock;
	this->spuNoiseVoice();
}

void AkaoExecDec::turnOffNoise(AkaoPlayerTrack& playerTrack, AkaoPlayer& player, int voice)
{
	if (playerTrack.use_global_track == 0) {
		player.noise_voices &= ~voice;
	}
	else {
		_noise_voices &= ~voice;
	}
	_player.spucnt |= SpuCnt::SetNoiseClock;
	this->spuNoiseVoice();
	playerTrack.noise_on_off_delay_counter = 0;
}

void AkaoExecDec::turnOnFrequencyModulation(AkaoPlayerTrack& playerTrack, AkaoPlayer& player, int voice)
{
	if (playerTrack.use_global_track == 0) {
		player.pitch_lfo_voices |= voice;
	}
	else if (voice & 0x555555 == 0) {
		_pitch_lfo_voices |= voice;
	}
	this->spuPitchLFOVoice();
}

void AkaoExecDec::turnOffFrequencyModulation(AkaoPlayerTrack& playerTrack, AkaoPlayer& player, int voice)
{
	if (playerTrack.use_global_track == 0) {
		player.pitch_lfo_voices &= ~voice;
	}
	else {
		_pitch_lfo_voices &= ~voice;
	}
	this->spuPitchLFOVoice();
	playerTrack.pitchmod_on_off_delay_counter = 0;
}

void AkaoExecDec::turnOnOverlayVoice(AkaoPlayerTrack& playerTrack)
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

int AkaoExecDec::executeChannel(AkaoPlayerTrack& playerTrack, AkaoPlayer& player, uint32_t voice)
{
	const uint8_t* data = playerTrack.addr;

	if (data + 1 - _akao.data() > _akao.dataSize()) {
		return -2; // No more data
	}

	uint8_t op = *playerTrack.addr;

	const uint8_t opcode_len = _opcode_len[op - 0xA0];

	if (data + opcode_len - _akao.data() > _akao.dataSize()) {
		return -2; // No more data
	}

	switch (op) {
	case 0xA0: // Finished
		finishChannel(playerTrack, player, voice);
		return -1;
	case 0xA1: // Load Instrument
		if (!loadInstrument(playerTrack, player, voice)) {
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
				player.noise_clock = (player.noise_clock + (noise_clock & 0x3F)) & 0x3F;
			}
			else {
				player.noise_clock = noise_clock;
			}
		}
		else if (noise_clock & 0xC0) {
			_noise_clock = (_noise_clock + (noise_clock & 0x3F)) & 0x3F;
		}
		else {
			_noise_clock = noise_clock;
		}
		player.spucnt |= SpuCnt::SetNoiseClock;
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
		const AkaoInstrAttr& instr = _instruments[playerTrack.instrument];
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
			AkaoPlayerTrack &otherTrack = _playerTracks[playerTrack.overlay_voice_num];
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
		turnOnReverb(playerTrack, player, voice);
		break;
	case 0xC3: // Turn Off Reverb
		turnOffReverb(playerTrack, player, voice);
		break;
	case 0xC4: // Turn On Noise
		turnOnNoise(playerTrack, player, voice);
		break;
	case 0xC5: // Turn Off Noise
		turnOffNoise(playerTrack, player, voice);
		break;
	case 0xC6: // Turn On Frequency Modulation
		turnOnFrequencyModulation(playerTrack, player, voice);
		break;
	case 0xC7: // Turn Off Frequency Modulation
		turnOffFrequencyModulation(playerTrack, player, voice);
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
		turnOffNoise(playerTrack, player, voice);
		turnOffFrequencyModulation(playerTrack, player, voice);
		turnOffReverb(playerTrack, player, voice);
		playerTrack.legato_flags &= ~(0x4 | 0x1);
		break;
	case 0xCC: // Turn On Legato
		playerTrack.legato_flags = 0x1;
		break;
	case 0xCD: // Do nothing
		break;
	case 0xCE: // Turn On Noise and Toggle Noise On/Off after a Period of Time
		OPCODE_UPDATE_DELAY_COUNTER(noise);
		turnOnNoise(playerTrack, player, voice);
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
		turnOnFrequencyModulation(playerTrack, player, voice);
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
		finishChannel(playerTrack, player, voice);
		return -1;
	case 0xE8: // Tempo
		const uint8_t tempo = (data[2] << 24) | (data[1] << 16);
		player.tempo_slide_length = 0;
		player.tempo = tempo;
		break;
	case 0xE9: // Tempo slide
		OPCODE_SLIDE(player.tempo);
		break;
	case 0xEA: // Reverb depth
		const uint32_t depth = (data[2] << 24) | (data[1] << 16);
		player.reverb_depth_slide_length = 0;
		player.spucnt |= SpuCnt::SetReverbDepth;
		player.reverb_depth = depth;
		break;
	case 0xEB: // Reverb depth slide
		OPCODE_SLIDE(player.reverb_depth);
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
		if (player.condition != 0 && condition <= player.condition) {
			const int16_t offset = (data[3] << 8) | data[2];
			player.condition_ack = condition;
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
		const AkaoInstrAttr &instr = _instruments[instrument];
		if (playerTrack.use_global_track != 0 || !((player.keyed_voices & voice) & _active_voices)) {
			playerTrack.spuRegisters.update_flags |= AkaoUpdateFlags::VoicePitch;
			const AkaoInstrAttr &playerInstr = _instruments[playerTrack.instrument];
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
		player.pause = 1;
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

			while (alternate_voice_num < 24 && (player.active_voices | player.overlay_voices | player.alternate_voices) & alternate_voices) {
				alternate_voices <<= 1;
				alternate_voice_num += 1;
			}

			if (alternate_voice_num < 24) {
				player.alternate_voices = alternate_voices | player.alternate_voices;
				playerTrack.alternate_voice_num = alternate_voice_num;
				playerTrack.voice_effect_flags |= AkaoVoiceEffectFlags::AlternateVoiceEnabled;
			}
		}
		break;
	case 0xF9: // Turn Off Alternate Voice
		player.alternate_voices = !(1 << playerTrack.alternate_voice_num) & player.alternate_voices;
		playerTrack.voice_effect_flags &= ~AkaoVoiceEffectFlags::AlternateVoiceEnabled; // Disable
		playerTrack.spuRegisters.update_flags |= AkaoUpdateFlags::AdsrReleaseMode | AkaoUpdateFlags::AdsrReleaseRate;
		playerTrack.spuRegisters.adsr_release_rate = _instruments[playerTrack.instrument].adsr_release_rate;
		break;
	case 0xFA:
	case 0xFB:
	case 0xFC: // Unused
		finishChannel(playerTrack, player, voice);
		return -1;
	case 0xFD: // Time signature
		player.ticks_per_beat = data[1];
		player.tick = 0;
		player.beat = 0;
		player.beats_per_measure = data[2];
		break;
	case 0xFE: // Measure number
		player.measure = (uint16_t(data[2]) << 8) | uint16_t(data[1]);
		break;
	case 0xFF: // Unused
		finishChannel(playerTrack, player, voice);
		return -1;
	}

	playerTrack.addr += opcode_len;
}

uint8_t AkaoExecDec::_opcode_len[0x60] = {
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

uint8_t AkaoExecDec::_delta_times[12] = {
	0xC0, 0x60, 0x30, 0x18,
	0x0C, 0x06, 0x03, 0x20,
	0x10, 0x08, 0x04, 0x00
};

uint32_t AkaoExecDec::_akaoEndSequence = 0xA0;

uint32_t AkaoExecDec::_pan_lfo_addrs[PAN_LFO_ADDRS_LEN] = {
	0x8004A044, // 0x1FFF
	0x8004A05C, // 0x7FFF
	0x8004A068,
	0x8004A07C,

	// TODO
};

44 A0 04 80
5C A0 04 80

68 A0 04 80 
7C A0 04 80
88 A0 04 80
9C A0 04 80
A8 A0 04 80
E8 A1 04 80
14 A3 04 80
9C A3 04 80
3C A2 04 80
E4 A2 04 80
C4 A3 04 80
9C A3 04 80
14 A3 04 80
9C A3 04 80
00 00 00 00

uint8_t AkaoExecDec::_empty_adpcm[EMPTY_ADPCM_LEN] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x0C, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
}
