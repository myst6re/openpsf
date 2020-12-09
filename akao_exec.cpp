#include "akao_exec.h"
#include "include/LIBETC.h"

#define OPCODE_ADSR_UPDATE(name, upd_flags) \
	const uint8_t param = data[1]; \
	playerTrack.update_flags |= upd_flags; \
	playerTrack.name = param; \
	if (playerTrack.voice_effect_flags & AkaoVoiceEffectFlags::OverlayVoiceEnabled) { \
		_channels[playerTrack.overlay_voice_num].playerTrack.name = param; \
	}

AkaoExec::AkaoExec(const Akao& akao, bool loadInstruments2) noexcept :
	_akao(akao)
{
	resetCallback();
	SpuInit();
	uint32_t startAddr = 0x1010 /* 0x800F0000 */, endAddr = 0x1010; /* 0x80166000 */
	akaoInit(&startAddr, &endAddr);
	if (loadInstruments2) {
		akaoLoadInstrumentSet2(0x80168000, 0x801A6000);
	}
	_akaoMessage = 0x10; // Load and play AKAO
	_akaoDataAddr = 0x801D0000;
	akaoPostMessage();
}

void AkaoExec::resetCallback()
{
	//0x80051520 // load_i32(0x80051534) + 0xC
	//resetCallback: main2abis(); // 8003D258
}

int32_t _timeEventDescriptor = 0;

long AkaoExec::akaoTimerCallback()
{
	int32_t root_counter_1 = GetRCnt(RCntCNT2);
	const int32_t mode = 1;
	uint32_t time_elapsed = VSync(mode);

	if (time_elapsed < _previous_time_elapsed) {
		_previous_time_elapsed = 0;
	}

	uint32_t unknown2 = (uint64_t(time_elapsed - _previous_time_elapsed) * uint64_t(0x3E0F83E1)) >> 36;

	if (uint16_t(unknown2) == 0 || uint16_t(unknown2) >= 9) {
		unknown2 = 1;
	}

	_previous_time_elapsed = time_elapsed;

	if (_unknown62FF8 & 0x4) {
		unknown2 <<= 1;
	}

	while (unknown2 & 0xFFFF) {
		akaoMain();
		unknown2 -= 1;
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

void AkaoExec::akaoDspMain()
{
	// TODO
}

void AkaoExec::akaoDspOnTick(AkaoPlayerTrack* playerTracks, const AkaoPlayer& player, uint32_t voice)
{
	// TODO
	spuNoiseVoice();
	// TODO
	spuPitchLFOVoice();
	// TODO
}

void AkaoExec::akaoUnknown2E954(AkaoPlayerTrack* playerTracks, uint32_t voice)
{
	// TODO
	spuNoiseVoice();
	// TODO
	spuPitchLFOVoice();
	// TODO
}

void AkaoExec::akaoDispatchVoice(const uint8_t *data, AkaoPlayerTrack& playerTrack, const AkaoPlayer &player, uint32_t voice)
{
	uint32_t cur = playerTrack.addr;
	uint8_t instruction = 160;

	do {
		cur += 1;
		instruction = data[cur];
		if (instruction < 160) {
			break;
		}

		//execute_channel(data, channel_state, player, voice);
	} while (instruction != 160);

	if (instruction == 160) {
		return;
	}

	akaoReadNextNote(data, channel_state);

	if (playerTrack.forced_delta_time) {
		playerTrack.delta_time_counter = playerTrack.forced_delta_time & 0xFF;
		playerTrack.gate_time_counter = (playerTrack.forced_delta_time >> 8) & 0xFF;
	}
	if (playerTrack.delta_time_counter) {
		// FIXME: looks strange
		if ((160 >= 143 || 160 < 132) && !(playerTrack.legato_flags & 0x5)) {
			playerTrack.delta_time_counter -= 2;
		}
	}
	else {
		playerTrack.delta_time_counter = _delta_times[instruction == 0 ? 0 : ((instruction - 1) % 11) + 1];
		playerTrack.gate_time_counter = playerTrack.delta_time_counter;
		// FIXME: looks strange
		if (160 - 132 >= 11 && !(playerTrack.legato_flags & 0x5)) {
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

	if (instruction < 132) {
		saved2 = argument0 = (instruction * 0xBA2E8BA3) >> 35;

		if (playerTrack.voice_effect_flags & AkaoVoiceEffectFlags::DrumModeEnabled) {
			if (!playerTrack.use_global_track) {
				_player.key_on_voices |= voice;
			}
			else {
				_key_on_voices |= voice;
			}
			value1 = ((saved2 & 0xFF) * 0xAAAAAAAB) >> 35;
			const AkaoDrumKeyAttr* drum = nullptr;//playerTrack.drum + instruction; TODO
			if (instrument != drum->instrument) {
				AkaoInstrAttr& instr = _instruments[drum->instrument];
				playerTrack.spu_addr = instr.addr;
				playerTrack.loop_addr = instr.loop_addr;
				playerTrack.adsr_attack_rate = instr.adsr_attack_rate;
				playerTrack.adsr_decay_rate = instr.adsr_decay_rate;
				playerTrack.adsr_sustain_level = instr.adsr_sustain_level;
				playerTrack.adsr_sustain_rate = instr.adsr_sustain_rate;
				playerTrack.adsr_attack_mode = instr.adsr_attack_mode;
				playerTrack.adsr_sustain_mode = instr.adsr_sustain_mode;
				if (!(playerTrack.voice_effect_flags & AkaoVoiceEffectFlags::AlternateVoiceEnabled)) {
					playerTrack.adsr_release_rate = instr.adsr_release_rate;
					playerTrack.adsr_release_mode = instr.adsr_release_mode;
					playerTrack.update_flags |= 0x1FF80;
				}
				else {
					playerTrack.update_flags |= 0x1BB80;
				}
			}
			pitch = _instruments[drum->instrument].pitch[drum->key % 12];
			playerTrack.volume = drum->volume << 16;
			playerTrack.pan = drum->pan << 8;
			/*
80031264       value1 = load_u8(argument2 + 0x1)
800312AC       argument1 = ((value1 * 0xAAAAAAAB) >> 35) & 0xFF
800312B4       argument0 = load_i32(0x80075F38 + (((value1 - (12 * argument1)) & 0xFF) * 4) + ((load_u8(argument2) * 64)))
			   if u32(argument1) >= 7 then
800312C8         argument0 <<= argument1 - 6
800312C4       else if u32(argument1) < 6 then
800312D8         argument0 >>= 6 - argument1
			   end
800312F0       store_i32(channel_state + 0x44, (load_u8(argument2 + 0x2) + (load_u8(argument2 + 0x3) << 8)) << 16)
80031304       store_i16(channel_state + 0x60, load_u8(argument2 + 0x4) << 8)
			*/
		}
		else {
			if (playerTrack.portamento_speed && playerTrack.previous_note_number) {
				saved2 = (playerTrack.previous_note_number & 0xFF) + (playerTrack.previous_transpose & 0xFF);
				playerTrack.pitch_slide_length = playerTrack.portamento_speed;
				playerTrack.pitch_slide_amount = (saved2 & 0xFF) + playerTrack.transpose - playerTrack.previous_note_number - playerTrack.previous_transpose;
				playerTrack.note = playerTrack.previous_note_number - playerTrack.transpose + playerTrack.previous_transpose;
			}
			else {
				note = argument0 + playerTrack.octave * 12;
				playerTrack.note = note & 0xFF;
				saved2 = (playerTrack.transpose & 0xFF) + note;
			}

			saved2 &= 0xFF;
/*
80031324       if load_u16(channel_state + 0x6C)
				   && load_u16(channel_state + 0x6A) then
8003134C         saved2 = load_u8(channel_state + 0x6A) + load_u8(channel_state + 0xD4)
80031360         store_i16(channel_state + 0x68, load_u16(channel_state + 0x6C))
80031370         argument0 = load_u16(channel_state + 0xD4)
80031374         store_i16(channel_state + 0xD2, (saved2 & 0xFF) + load_u16(channel_state + 0xCC) - load_u16(channel_state + 0x6A) - argument0)
80031388         store_i16(channel_state + 0xD0, load_u16(channel_state + 0x6A) - (load_u16(channel_state + 0xCC) - argument0))
80031384       else
80031308         value0 = load_u16(channel_state + 0x66)
80031328         saved2 = argument0 + (((value0 << 1) + value0) << 2)
80031394         store_i16(channel_state + 0xD0, saved2 & 0xFF)
8003139C         saved2 += load_u8(channel_state + 0xCC)
			   end
800313A8       argument0 = saved2 & 0xFF
800313AC       Low, High = u32(argument0) * u32(0xAAAAAAAB)
800313B4       value1 = u32(High) >> 3
800313C4       saved2 = argument0 - (((value1 << 1) + value1) << 2)
800313D8       argument1 = value1 & 0xFF
800313D4       if load_u16(channel_state + 0x6E) & 0x2 == 0 then
800313E4         if load_u16(channel_state + 0x54) == 0 then
800313F4           argument0 = voice | load_i32(player + 0x8)
800313F8           store_i32(player + 0x8, argument0)
80031408           if load_i32(channel_state + 0x38) & 0x100 then
80031414             value1 = load_u16(channel_state + 0x24)
					 if u32(load_i32(channel_state + 0x24)) >= 24 then
80031424               value1 -= 24
					 end
80031434             store_i32(player + 0x8, (1 << value1) | argument0)
				   end
80031430         else
8003144C           store_i32(0x800A9FD0, voice | load_i32(0x800A9FD0))
				 end
80031450         store_i16(channel_state + 0x64, 0)
			   end
80031474       value1 = argument1 & 0xFFFF
80031478       argument0 = load_i32(0x80075F38 + ((saved2 & 0xFF) << 2) + ((load_u16(channel_state + 0x58) << 6)))
80031480       if u32(value1) >= 7 then
80031490         argument0 = argument0 << (u32(value1) + 4294967290)
8003148C       else if u32(value1) < 6 then
800314A0         argument0 = u32(argument0) >> (6 - value1)
			   end
*/
		}
	}

/*
80030E7C stackPointer = u32(stackPointer) + 4294967232
80030E80 store_i32(stackPointer + 0x20, channel_state)
80030E84 channel_state = argument0
80030E88 store_i32(stackPointer + 0x2C, player)
80030E8C player = argument1
80030E90 store_i32(stackPointer + 0x34, voice)
80030E94 voice = argument2
80030E98 store_i32(stackPointer + 0x38, saved6)
80030EA0 saved6 = 0x80049828
80030EA4 store_i32(stackPointer + 0x30, saved4)
80030EA8 saved4 = 160
80030EAC store_i32(stackPointer + 0x3C, returnAddress)
80030EB0 store_i32(stackPointer + 0x28, saved2)
80030EB4 store_i32(stackPointer + 0x24, saved1)

80030EB8 do
           value0 = load_i32(channel_state)
80030EC4   store_i32(channel_state, u32(value0) + 1)
80030EC8   saved2 = load_u8(value0)
80030ED0   saved1 = saved2 & 0xFF
80030EDC   argument0 = channel_state
80030ED8   break if u32(saved1) < 160
80030EEC   argument1 = player
80030EF4   argument2 = voice
80030EF0   returnAddress = pc + 8; jump load_i32((saved1 << 2) + 0x80049828)
		 while saved1 != 160
80030F04 if saved1 != 160 then
80030F10   argument0 = channel_state
80030F0C   returnAddress = pc + 8; jump 0x318BC
80030F14   argument1 = load_i16(channel_state + 0xC4)
80030F1C   if argument1 then
80030F2C     store_i16(channel_state + 0x56, (argument1 << 8) + argument1)
		   end
80030F30   value1 = load_u16(channel_state + 0x56)
		   if value1 & 0xFF != 0 then # delta_time_counter
			 if u32(160) < 143
80030F4C       && (u32(160) >= 132
80030F60       || load_u16(channel_state + 0x6E) & 0x5) then
                (noop)
			 else
80030F70       store_i16(channel_state + 0x56, u32(value1) - 512)
             end
80030F6C   else
80030F7C     Low, High = u32(saved1) * u32(0xBA2E8BA3)
80030F84     value1 = u32(High) >> 3
80030FB0     value1 = load_u16(0x80049C28 + ((saved1 - ((((value1 << 1) + value1) << 2) - value1)) & 0xFF) << 1)
80030FBC     if u32(u32(160) - 132) >= 11
80030FD0         && !(load_u16(channel_state + 0x6E) & 0x5) then
80030FD8       value1 -= 512
		     end
80030FDC     store_i16(channel_state + 0x56, value1)
           end
80030FE4   value1 = saved2 & 0xFF
80030FE8   store_i16(channel_state + 0xC2, load_u8(channel_state + 0x56))
		   if u32(value1) >= 143 then
80030FFC     store_i16(channel_state + 0x6C, 0)
80031000     store_i16(channel_state + 0xD6, 0)
80031004     store_i16(channel_state + 0xD8, 0)
80031010     store_i16(channel_state + 0x6E, load_u16(channel_state + 0x6E) & 0xFFFD)
8003100C     return
		   end
		   if u32(value1) < 132 then
80031028     argument0 = u32(u32(value1) * u32(0xBA2E8BA3)) / 0x800000000
80031038     if load_i32(channel_state + 0x38) & 0x8 then
80031048       if load_u16(channel_state + 0x54) == 0 then
80031060         store_i32(player + 0x8, voice | load_i32(player + 0x8))
8003105C       else
80031078         store_i32(0x800A9FD0, voice | load_i32(0x800A9FD0))
               end
800310A8       argument0 = (argument0 - 12 * ((argument0 & 0xFF) * 0xAAAAAAAB) / 0x800000000)) & 0xFF
800310B4       argument2 = load_i32(channel_state + 0x14) + argument0 * 4 + argument0
800310B8       value1 = load_u8(argument2)
			   if value1 != load_u16(channel_state + 0x58) then
800310CC         store_i16(channel_state + 0x58, value1)
800310F0         store_i32(channel_state + 0xE4, load_i32(0x80075F28 + (load_u8(argument2) << 6)))
80031114         store_i32(channel_state + 0xE8, load_i32(0x80075F2C + (load_u8(argument2) << 6)))
80031138         store_i16(channel_state + 0xFA, load_u8(0x80075F30 + (load_u8(argument2) << 6)))
8003115C         store_i16(channel_state + 0xFC, load_u8(0x80075F31 + (load_u8(argument2) << 6)))
80031180         store_i16(channel_state + 0xFE, load_u8(0x80075F32 + (load_u8(argument2) << 6)))
800311A4         store_i16(channel_state + 0x100, load_u8(0x80075F33 + (load_u8(argument2) << 6)))
800311C8         store_i32(channel_state + 0xEC, load_u8(0x80075F35 + (load_u8(argument2) << 6)))
800311F8         store_i32(channel_state + 0xF0, load_u8(0x80075F36 + (load_u8(argument2) << 6)))
800311F4         if load_i32(channel_state + 0x38) & 0x200 == 0 then
8003121C           store_i16(channel_state + 0x102, load_u8(0x80075F34 + (load_u8(argument2) << 6)))
80031240           store_i32(channel_state + 0xE0, load_i32(channel_state + 0xE0) | 0x1FF80)
80031248           store_i32(channel_state + 0xF4, load_u8(0x80075F37 + (load_u8(argument2) << 6)))
80031244         else
8003125C           store_i32(channel_state + 0xE0, load_i32(channel_state + 0xE0) | 0x1BB80)
                 end
			   end
80031264       value1 = load_u8(argument2 + 0x1)
8003126C       Low, High = u32(value1) * u32(0xAAAAAAAB)
800312AC       argument1 = (u32(High) >> 3) & 0xFF
800312B4       argument0 = load_i32((((value1 - (((argument1 << 1) + argument1) << 2)) & 0xFF) << 2) + ((load_u8(argument2) << 6) + 0x80075F38))
			   if u32(argument1) >= 7 then
800312C8         argument0 = argument0 << (u32(argument1) - 6)
800312C4       else if u32(argument1) < 6 then
800312D8         argument0 >>= 6 - argument1
			   end
800312F0       store_i32(channel_state + 0x44, (load_u8(argument2 + 0x2) + (load_u8(argument2 + 0x3) << 8)) << 16)
80031304       store_i16(channel_state + 0x60, load_u8(argument2 + 0x4) << 8)
80031300     else
80031308       value0 = load_u16(channel_state + 0x66)
80031328       saved2 = argument0 + (((value0 << 1) + value0) << 2)
80031324       if load_u16(channel_state + 0x6C)
			       && load_u16(channel_state + 0x6A) then
8003134C         saved2 = load_u8(channel_state + 0x6A) + load_u8(channel_state + 0xD4)
80031360         store_i16(channel_state + 0x68, load_u16(channel_state + 0x6C))
80031370         argument0 = load_u16(channel_state + 0xD4)
80031374         store_i16(channel_state + 0xD2, (saved2 & 0xFF) + load_u16(channel_state + 0xCC) - load_u16(channel_state + 0x6A) - argument0)
80031388         store_i16(channel_state + 0xD0, load_u16(channel_state + 0x6A) - (load_u16(channel_state + 0xCC) - argument0))
80031384       else
80031394         store_i16(channel_state + 0xD0, saved2 & 0xFF)
8003139C         saved2 += load_u8(channel_state + 0xCC)
               end
800313A8       argument0 = saved2 & 0xFF
800313AC       Low, High = u32(argument0) * u32(0xAAAAAAAB)
800313B4       value1 = u32(High) >> 3
800313C4       saved2 = argument0 - (((value1 << 1) + value1) << 2)
800313D8       argument1 = value1 & 0xFF
800313D4       if load_u16(channel_state + 0x6E) & 0x2 == 0 then
800313E4         if load_u16(channel_state + 0x54) == 0 then
800313F4           argument0 = voice | load_i32(player + 0x8)
800313F8           store_i32(player + 0x8, argument0)
80031408           if load_i32(channel_state + 0x38) & 0x100 then
80031414             value1 = load_u16(channel_state + 0x24)
				     if u32(load_i32(channel_state + 0x24)) >= 24 then
80031424               value1 -= 24
				     end
80031434             store_i32(player + 0x8, (1 << value1) | argument0)
                   end
80031430         else
8003144C           store_i32(0x800A9FD0, voice | load_i32(0x800A9FD0))
                 end
80031450         store_i16(channel_state + 0x64, 0)
			   end
8003145C       argument0 = 0x80075F38
80031474       value1 = argument1 & 0xFFFF
80031478       argument0 = load_i32(((saved2 & 0xFF) << 2) + ((load_u16(channel_state + 0x58) << 6) + argument0))
80031480       if u32(value1) >= 7 then
80031490         argument0 = argument0 << (u32(value1) + 4294967290)
8003148C       else if u32(value1) < 6 then
800314A0         argument0 = u32(argument0) >> (6 - value1)
		       end
			 end
800314AC     if load_u16(channel_state + 0x54) == 0 then
800314C4       store_i32(player + 0xC, voice | load_i32(player + 0xC))
800314C0     else
800314DC       store_i32(0x800A9FD4, voice | load_i32(0x800A9FD4))
             end
800314E4     value1 = load_i16(channel_state + 0xCE)
800314F0     store_i32(channel_state + 0xE0, load_i32(channel_state + 0xE0) | 0x13)
800314EC     if value1 != 0 then
800314F8       Low, High = argument0 * value1
800314F4       if value1 > 0 then
80031504         value0 = u32(Low) >> 7
80031500       else
8003150C         value0 = u32(Low) >> 8
               end
80031514       argument0 = (argument0 + value0) & 0xFFFF
			 end
80031528     store_i32(channel_state + 0x30, argument0)
80031524     if load_i32(channel_state + 0x38) & 0x1 then
8003152C       value0 = load_u16(channel_state + 0x7E)
8003153C       if value0 & 0x8000 != 0 then
80031548         Low, High = (u32(value0 & 0x7F00) >> 8) * argument0
80031544       else
80031558         Low, High = value1 * (u32((argument0 << 4) - argument0) >> 8)
               end
80031568       store_i16(channel_state + 0x7C, u32(Low) >> 7)
80031588       store_i16(channel_state + 0x74, load_u16(channel_state + 0x72))
8003158C       store_i16(channel_state + 0x78, 1)
80031590       store_i32(channel_state + 0x18, load_i32(0x8004A5CC + (load_u16(channel_state + 0x7A) << 2)))
			 end
800315A0     if load_i32(channel_state + 0x38) & 0x2 then
800315C8       store_i16(channel_state + 0x88, load_u16(channel_state + 0x86))
800315CC       store_i16(channel_state + 0x8C, 1)
800315D0       store_i32(channel_state + 0x1C, load_i32(0x8004A5CC + (load_u16(channel_state + 0x8E) << 2)))
			 end
800315E0     if load_i32(channel_state + 0x38) & 0x4 != 0 then
80031608       store_i16(channel_state + 0x9A, 1)
8003160C       store_i32(channel_state + 0x20, load_i32(0x8004A5CC + (load_u16(channel_state + 0x9C) << 2)))
			 end
80031610     store_i16(channel_state + 0xD6, 0)
80031614     store_i16(channel_state + 0xD8, 0)
80031618     store_i32(channel_state + 0x34, 0)
		   end
8003161C   value0 = load_u16(channel_state + 0x6E)
80031620   argument0 = load_i16(channel_state + 0xD2)
80031634   store_i16(channel_state + 0x6E, (value0 & 0xFFFD) | ((value0 & 0x1) << 1))
80031638   if argument0 != 0 then
8003164C     store_i16(channel_state + 0xD0, load_u16(channel_state + 0xD0) + argument0)
8003166C     saved2 = load_u8(channel_state + 0xD0) + load_u8(channel_state + 0xCC)
80031668     if ! load_u16(channel_state + 0x54) then
80031678       argument0 = saved2 & 0xFF
8003167C       Low, High = u32(argument0) * u32(0xAAAAAAAB)
80031684       value0 = u32(High) >> 3
800316B4       argument0 = (((argument0 - (((value0 << 1) + value0) << 2)) & 0xFF) << 2) + ((load_u16(channel_state + 0x58) << 6) + 0x80075F38)
800316B8       value0 = load_i16(channel_state + 0xCE)
800316BC       argument0 = load_i32(argument0)
800316C0       if value0 != 0 then
800316CC         Low, High = argument0 * value0
800316C8         if value0 > 0 then
800316D8           value0 = u32(Low) >> 7
800316D4         else
800316E0           value0 = u32(Low) >> 8
                 end
800316E8         argument0 = (argument0 + value0) & 0xFFFF
			   end
800316F0       argument0 <<= 16
800316EC     else
800316FC       argument0 = saved2 & 0xFF
80031700       Low, High = u32(argument0) * u32(0xAAAAAAAB)
80031708       value0 = u32(High) >> 3
80031744       argument0 = load_i32((((argument0 - (((value0 << 1) + value0) << 2)) & 0xFF) << 2) + ((load_u16(channel_state + 0x58) << 6) + 0x80075F38)) << 16 
             end
80031754     Low, High = u32(saved2 & 0xFF) * u32(0xAAAAAAAB)
80031760     value1 = (u32(High) >> 3) & 0xFF
			 if u32(value1) >= 7 then
80031778       argument0 = argument0 << (u32(value1) - 6)
80031774     else if u32(value1) < 6 then
80031788       argument0 >>= 6 - value1
			 end
80031794     store_i16(channel_state + 0x64, load_u16(channel_state + 0x68))
800317A8     value1 = load_u16(channel_state + 0x64)
800317AC     value0 = argument0 - ((load_i32(channel_state + 0x30) << 16) + load_i32(channel_state + 0x34))
800317B0     Low = value0 / value1
800317B4     if value1 == 0 then
800317BC       breakpoint(7168)
			 end
800317C4     if value1 == -1
800317CC         && value0 == 0x80000000 then
800317D4       breakpoint(6144)
			 end
800317DC     store_i16(channel_state + 0xD2, 0)
800317E0     store_i32(channel_state + 0x4C, Low)
		   end
800317EC   store_i16(channel_state + 0x6A, load_u16(channel_state + 0xD0))
800317F0   store_i16(channel_state + 0xD4, load_u16(channel_state + 0xCC))
		 end

800317F4 returnAddress = load_i32(stackPointer + 0x3C)
800317F8 saved6 = load_i32(stackPointer + 0x38)
800317FC voice = load_i32(stackPointer + 0x34)
80031800 saved4 = load_i32(stackPointer + 0x30)
80031804 player = load_i32(stackPointer + 0x2C)
80031808 saved2 = load_i32(stackPointer + 0x28)
8003180C saved1 = load_i32(stackPointer + 0x24)
80031810 channel_state = load_i32(stackPointer + 0x20)
80031814 stackPointer = u32(stackPointer) + 64
80031818 jump returnAddress
*/
}

uint32_t AkaoExec::akaoReadNextNote(const uint8_t* data, AkaoPlayerTrack& playerTrack)
{
	uint32_t cur = 0; // TODO
	while (true) {
		const uint8_t op = data[cur];
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
				cur += len;
				continue;
			}

			if (op < 242) {
				switch (op) {
				case 0xC9: // Return To Loop Point Up to N Times
					cur += 1;
					if (data[cur] == playerTrack.loop_counts[playerTrack.loop_layer] + 1) {
						cur += 1;
						playerTrack.loop_layer = (playerTrack.loop_layer - 1) & 0x3;
					}
					else {
						cur = playerTrack.loop_addrs[playerTrack.loop_layer];
					}
					continue;
				case 0xCA: // Return To Loop Point
					cur = playerTrack.loop_addrs[playerTrack.loop_layer];
					continue;
				case 0xCB: // Reset Sound Effects
				case 0xCD: // Do nothing
				case 0xD1: // Do nothing
				case 0xDB: // Turn Off Portamento
					cur += 1;
					playerTrack.portamento_speed = 0;
					playerTrack.legato_flags &= ~5;
					continue;
				case 0xEE: // Unconditional Jump
					cur += 1;
					break;
				case 0xEF: // CPU Conditional Jump
					cur += 1;
					if (_player.condition >= data[cur]) {
						cur += 3 + (data[cur + 1] | (data[cur + 2] << 8));
					}
					else {
						cur += 3;
					}
					continue;
				case 0xF0: // Jump on the Nth Repeat
				case 0xF1: // Break the loop on the nth repeat
					cur += 1;
					if (data[cur] == playerTrack.loop_counts[playerTrack.loop_layer] + 1) {
						playerTrack.loop_layer = (playerTrack.loop_layer - 1) & 0x3;
						cur += 3 + (data[cur + 1] | (data[cur + 2] << 8));
					}
					else {
						cur += 3;
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

void AkaoExec::akaoMain()
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

		if (_player.time_counter & 0xFFFF0000 || _unknown62FF8 & 0x4) {
			_player.time_counter &= 0xFFFF;
			_unknown62F04 = 0;
			uint32_t voice = 1;
			channel_t channel = 0;

			do {
				if (active_voices & voice) {
					_playerTracks[channel].delta_time_counter -= 1;
					_playerTracks[channel].gate_time_counter -= 1;

					if (_playerTracks[channel].delta_time_counter == 0) {
						akaoDispatchVoice(_playerTracks[channel], _player, voice);
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
				_player.spucnt |= 0x80;
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

		if (_player2.time_counter & 0xFFFF0000 || _unknown62FF8 & 0x4) {
			_player2.time_counter &= 0xFFFF;
			_unknown62F04 = 1;
			uint32_t voice = 1;
			channel_t channel = 0;

			do {
				if (active_voices & voice) {
					_playerTracks2[channel].delta_time_counter -= 1;
					_playerTracks2[channel].gate_time_counter -= 1;

					if (_playerTracks2[channel].delta_time_counter == 0) {
						akaoDispatchVoice(_playerTracks2[channel], _player2, voice);
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

		if (_time_counter & 0xFFFF0000 || _unknown62FF8 & 0x4) {
			_time_counter &= 0xFFFF;
			uint32_t voice = 0x10000;
			channel_t channel = 0;

			do {
				if (active_voices & voice) {
					if (!(_unknown62FF8 & 0x2)) {
						_playerTracks3[channel].field_50 += 1;
						_playerTracks3[channel].delta_time_counter -= 1;
						_playerTracks3[channel].gate_time_counter -= 1;

						if (_playerTracks3[channel].delta_time_counter == 0) {
							akaoDispatchVoice(_playerTracks3[channel], _player, voice);
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

	if (_player.field_54) {
		akaoUnknown2C8DC(&_akaoMessage);
		_player.field_54 = 0;
	}

	akaoDispatchMessages();
	akaoUnknown30380();
	akaoUnknown2FE48();
}

void AkaoExec::akaoInit(uint32_t* spuTransferStartAddr, AkaoInstrAttr* spuTransferStopAddr)
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

void AkaoExec::akaoLoadInstrumentSet(uint32_t* spuTransferStartAddr, AkaoInstrAttr* instruments)
{
	SpuSetTransferStartAddr(*spuTransferStartAddr);
	akaoTransferSamples(&spuTransferStartAddr[4], spuTransferStartAddr[1]);
	for (int i = 0; i < 32; ++i) {
		memcpy(&_instruments[i], &instruments[i], sizeof(AkaoInstrAttr));
	}
	akaoWaitForSpuTransfer();
}

void AkaoExec::akaoLoadInstrumentSet2(uint32_t* spuTransferStartAddr, AkaoInstrAttr* spuTransferStopAddr)
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

void AkaoExec::akaoClearSpu()
{
	SpuSetTransferCallback(nullptr);
	SpuSetIRQ(SPU_OFF);
	SpuSetIRQCallback(nullptr);
	SpuSetKey(SPU_OFF, _spuActiveVoices);

	if (_spuActiveVoices & 0x10000) {
		_playerTracks[16].update_flags = 0x1FF93;
	}

	if (_spuActiveVoices & 0x20000) {
		_playerTracks[17].update_flags = 0x1FF93;
	}

	_spuActiveVoices = 0;
	spuReverbVoice();
	spuPitchLFOVoice();
	spuNoiseVoice();
}

void AkaoExec::akaoSetReverbMode(uint8_t reverbType)
{
	akaoClearSpu();
	SpuGetReverbModeParam(&_spuReberbAttr);

	if (_spuReberbAttr.mode != reverbType) {
		_player.reverb_type = reverbType;

		SpuSetReverb(SPU_OFF);

		_spuReberbAttr.mask = SPU_REV_MODE;
		_spuReberbAttr.mode = reverbType | SPU_REV_MODE_CLEAR_WA;
		SpuSetReverbModeParam(&_spuReberbAttr);

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

void AkaoExec::akaoReset()
{
	_global218 = 0x7F0000;
	_global290 = 0x7FFF0000;
	_player.stereo_mode = 1;
	_unknown833DE = 0;
	_unknown8337E = 0;
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
		_voiceUnknown9C60[voiceNum]._u0 = 0x7F0000;
		_voiceUnknown9C60[voiceNum]._u4 = 0;
		_voiceUnknown9C60[voiceNum]._u8 = 0;
		_channels[voiceNum].playerTrack.voice_effect_flags = AkaoVoiceEffectFlags::None;
		_channels[voiceNum].playerTrack.field_50 = 0;
		_channels[voiceNum].playerTrack.use_global_track = 0;
		_channels[voiceNum].playerTrack.voice = voiceNum;
		SpuSetVoiceVolumeAttr(voiceNum, 0, 0, SPU_VOICE_DIRECT, SPU_VOICE_DIRECT);
	}

	for (uint8_t voiceNum = 16; voiceNum < 24; ++voiceNum) {
		const uint8_t i = voiceNum - 16;
		_channels3[i].playerTrack.field_5A = 0;
		_channels3[i].playerTrack.field_3C = 0;
		_channels3[i].playerTrack.overlay_balance_slide_length = 0;
		_channels3[i].playerTrack.voice_effect_flags = AkaoVoiceEffectFlags::None;
		_channels3[i].playerTrack.field_50 = 0;
		_channels3[i].playerTrack.alternate_voice_num = voiceNum;
		_channels3[i].playerTrack.use_global_track = 1;
		_channels3[i].playerTrack.voice = voiceNum;
		_channels3[i].playerTrack.overlay_balance = 0x7F00;
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

int AkaoExec::akaoPostMessage()
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
			message.data[0] = uint32_t(_akao.data());
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

int32_t AkaoExec::getVoicesBits(AkaoChannelState* _channels, int32_t voice_bit, int32_t mask)
{
	int i = 1;
	channel_t channel = 0;

	voice_bit |= mask;

	while (mask != 0) {
		if (mask & i != 0) {
			int32_t mask2 = 0;

			if (_channels[channel].playerTrack.voice_effect_flags & AkaoVoiceEffectFlags::OverlayVoiceEnabled) {
				uint32_t overlay_voice_num = _channels[channel].playerTrack.overlay_voice_num;
				if (overlay_voice_num >= 24) {
					overlay_voice_num -= 24;
				}
				mask2 = 1 << overlay_voice_num;
			}
			else if (_channels[channel].playerTrack.voice_effect_flags & AkaoVoiceEffectFlags::AlternateVoiceEnabled) {
				mask2 = 1 << _channels[channel].playerTrack.alternate_voice_num;
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

	mask = _player2.noise_voices & _unknown62F68 & ~(_unknown99FCC | _spuActiveVoices);
	if (mask) {
		voice_bit = getVoicesBits(_channels2, voice_bit, mask);
	}
	mask = ~(_unknown62F68 | _unknown99FCC | _spuActiveVoices) & _player.noise_voices;
	if (mask) {
		voice_bit = getVoicesBits(_channels, voice_bit, mask);
	}

	voice_bit |= _noise_voices;

	// SPU_BIT could be used instead
	voice_bit = SpuSetNoiseVoice(SPU_ON, voice_bit);
	voice_bit = SpuSetNoiseVoice(SPU_OFF, voice_bit ^ 0xFFFFFFFF);
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
		_player.key_on_voices &= argument3;
		_player.keyed_voices &= argument3;
		_player.key_off_voices &= argument3;
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
		const AkaoInstrAttr& instr = _instruments[instrument];
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
	const AkaoInstrAttr& instr = _instruments[instrument];

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

void AkaoExec::adsrDecayRate(const uint8_t *data, AkaoPlayerTrack& playerTrack)
{
	OPCODE_ADSR_UPDATE(adsr_decay_rate, 0x1000);
}

void AkaoExec::adsrSustainLevel(const uint8_t* data, AkaoPlayerTrack& playerTrack)
{
	OPCODE_ADSR_UPDATE(adsr_sustain_level, 0x8000);
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
		case 0xA0: // Finished
			finishChannel(playerTrack, argument2);
			return -1;
		case 0xA1: // Load Instrument
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
			playerTrack.update_flags |= 0x3;
			playerTrack.volume = uint32_t(volume) << 23;
			break;
		case 0xA9: // Channel Volume Slide
			OPCODE_SLIDE_U32(playerTrack.volume, 23);
			break;
		case 0xAA: // Set Channel Pan
			const uint8_t pan = data[1];
			playerTrack.pan_slide_length = 0;
			playerTrack.update_flags |= 0x3;
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
			_player.spucnt |= 0x10;
			break;
		case 0xAD: // ADSR Attack Rate
			OPCODE_ADSR_UPDATE(adsr_attack_rate, 0x900);
			break;
		case 0xAE: // ADSR Decay Rate
			adsrDecayRate(data, playerTrack);
			break;
		case 0xAF: // ADSR Sustain Level
			adsrSustainLevel(data, playerTrack);
			break;
		case 0xB0: // ADSR Decay Rate and Sustain Level
			adsrDecayRate(data, playerTrack);
			adsrSustainLevel(data, playerTrack);
			break;
		case 0xB1: // ADSR Sustain Rate
			OPCODE_ADSR_UPDATE(adsr_sustain_rate, 0x2200);
			break;
		case 0xB2: // ADSR Release Rate
			OPCODE_ADSR_UPDATE(adsr_release_rate, 0x4400);
			break;
		case 0xB3: // ADSR Reset
			const AkaoInstrAttr& instr = _instruments[playerTrack.instrument];
			playerTrack.adsr_attack_rate = instr.adsr_attack_rate;
			playerTrack.adsr_decay_rate = instr.adsr_decay_rate;
			playerTrack.adsr_sustain_level = instr.adsr_sustain_level;
			playerTrack.adsr_sustain_rate = instr.adsr_sustain_rate;
			playerTrack.adsr_release_rate = instr.adsr_release_rate;
			playerTrack.adsr_attack_mode = instr.adsr_attack_mode;
			playerTrack.adsr_sustain_mode = instr.adsr_sustain_mode;
			playerTrack.adsr_release_mode = instr.adsr_release_mode;
			playerTrack.update_flags |= 0xFF00;
			if (playerTrack.voice_effect_flags & AkaoVoiceEffectFlags::OverlayVoiceEnabled) {
				AkaoPlayerTrack &otherTrack = _channels[playerTrack.overlay_voice_num].playerTrack;
				otherTrack.adsr_attack_rate = instr.adsr_attack_rate;
				otherTrack.adsr_decay_rate = instr.adsr_decay_rate;
				otherTrack.adsr_sustain_level = instr.adsr_sustain_level;
				otherTrack.adsr_sustain_rate = instr.adsr_sustain_rate;
				otherTrack.adsr_release_rate = instr.adsr_release_rate;
				otherTrack.adsr_attack_mode = instr.adsr_attack_mode;
				otherTrack.adsr_sustain_mode = instr.adsr_sustain_mode;
				otherTrack.adsr_release_mode = instr.adsr_release_mode;
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
			playerTrack.update_flags |= 0x10;
			break;
		case 0xB7: // ADSR Attack Mode
			OPCODE_ADSR_UPDATE(adsr_attack_mode, 0x100);
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
		case 0xC2: // Turn On Reverb
			turnOnReverb(playerTrack, argument2);
			break;
		case 0xC3: // Turn Off Reverb
			turnOffReverb(playerTrack, argument2);
			break;
		case 0xC4: // Turn On Noise
			turnOnNoise(playerTrack, argument2);
			break;
		case 0xC5: // Turn Off Noise
			turnOffNoise(playerTrack, argument2);
			break;
		case 0xC6: // Turn On Frequency Modulation
			turnOnFrequencyModulation(playerTrack, argument2);
			break;
		case 0xC7: // Turn Off Frequency Modulation
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
		case 0xCD: // Do nothing
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
		case 0xD1: // Do nothing
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
		case 0xE7: // Unused
			finishChannel(playerTrack, argument2);
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
			playerTrack.drum = reinterpret_cast<const AkaoDrumKeyAttr*>(data) + 2 + offset;
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
			const AkaoInstrAttr &instr = _instruments[instrument];
			if (playerTrack.use_global_track != 0 || !((_player.keyed_voices & argument2) & _unknown99FCC)) {
				playerTrack.update_flags |= 0x10;
				const AkaoInstrAttr &playerInstr = _instruments[playerTrack.instrument];
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
		case 0xF3: // Set field 54
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
		case 0xFC: // Unused
			finishChannel(playerTrack, argument2);
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
			finishChannel(playerTrack, argument2);
			return -1;
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

uint8_t AkaoExec::_delta_times[12] = {
	0xC0, 0x60, 0x30, 0x18,
	0x0C, 0x06, 0x03, 0x20,
	0x10, 0x08, 0x04, 0x00
}

uint32_t AkaoExec::_pan_lfo_addrs[PAN_LFO_ADDRS_LEN] = {
	0x8004A044, // 0x1FFF
	0x8004A05C, // 0x7FFF
	// TODO
};

uint8_t AkaoExec::_empty_adpcm[EMPTY_ADPCM_LEN] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x0C, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
}
