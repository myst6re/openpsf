#pragma once

#include "akao.h"
#include "akao_instr.h"
#include <cstdint>
#include <queue>
#include "include/LIBSPU.H"
#include "include/LIBAPI.H"

#define DECLARE_ENUM_METHODS(Name) \
	inline Name operator|(Name flags, Name other) noexcept { \
		return static_cast<Name>(static_cast<uint32_t>(flags) | static_cast<uint32_t>(other)); \
	} \
\
	inline bool operator&(Name flags, Name other) noexcept { \
		return bool(static_cast<Name>(static_cast<uint32_t>(flags) & static_cast<uint32_t>(other))); \
	} \
\
	inline Name& operator|=(Name& flags, const Name& other) noexcept { \
		flags = static_cast<Name>(static_cast<uint32_t>(flags) | static_cast<uint32_t>(other)); \
		return flags; \
	} \
\
	inline Name& operator&=(Name& flags, const Name& other) noexcept { \
		flags = static_cast<Name>(static_cast<uint32_t>(flags) & static_cast<uint32_t>(other)); \
		return flags; \
	} \
\
	inline Name& operator^=(Name& flags, const Name& other) noexcept { \
		flags = static_cast<Name>(static_cast<uint32_t>(flags) ^ static_cast<uint32_t>(other)); \
		return flags; \
	} \
\
	inline Name operator~(Name flags) noexcept { \
		return static_cast<Name>(~static_cast<uint32_t>(flags)); \
	}

struct AkaoMessage // 36 bytes long
{
	uint8_t opcode;
	uint8_t _padding1;
	uint16_t _padding2;
	uint32_t data[8];
};

enum class SpuCnt : uint32_t {
	None = 0x00000,
	Unknown1 = 0x00001,
	Unknown2 = 0x00002,
	Unknown4 = 0x00004,
	Unknown8 = 0x00008,
	SetNoiseClock = 0x00010,
	Unknown20 = 0x00020,
	Unknown40 = 0x00040,
	SetReverbDepth = 0x00080
};

DECLARE_ENUM_METHODS(SpuCnt);

struct AkaoPlayer // 0x8009A104
{
	uint32_t stereo_mode;                 // 0x00: stereo mode (1: stereo, 4: surround, otherwise: mono)
	uint32_t active_voices;               // 0x04: bitset that indicates the voices currently in use
	uint32_t key_on_voices;               // 0x08: bitset that indicates the voices to key on
	uint32_t keyed_voices;                // 0x0c: bitset that indicates the voices currently keyed on
	uint32_t key_off_voices;              // 0x10: bitset that indicates the voices to key off
	uint32_t saved_active_voices;         // 0x14: unknown voice bitset
	uint32_t tempo;                       // 0x18: tempo (Q16 fixed-point number)
	int32_t tempo_slope;                  // 0x1c: slope of tempo slider (Q16 fixed-point number)
	uint32_t time_counter;                // 0x20: time counter (0x10000 = 1 tick)
	uint32_t overlay_voices;              // 0x24: bitset that indicates the sub-voices used for overlay (opcode 0xF4)
	uint32_t alternate_voices;            // 0x28: bitset that indicates the sub-voices used for alternate voice (opcode 0xF8)
	uint32_t noise_voices;                // 0x2c: bitset that indicates the voices with noise mode enabled
	uint32_t reverb_voices;               // 0x30: bitset that indicates the voices with reverb enabled
	uint32_t pitch_lfo_voices;            // 0x34: bitset that indicates the voices with pitch LFO (frequency modulation) mode enabled
	SpuCnt spucnt;                        // 0x38: SPUCNT shadow (0xA13C)
	uint32_t reverb_type;                 // 0x3c: reverb type
	uint32_t reverb_depth;                // 0x40: reverb depth (Q16 fixed-point number)
	int32_t reverb_depth_slope;           // 0x44: slope of reverb depth slider (Q16 fixed-point number)
	uint16_t tempo_slide_length;          // 0x48: tempo slide length
	uint16_t song_id;                     // 0x4a: current song id
	// Only player1 after that
	uint16_t condition_ack;               // 0x4c: the last matched condition value (opcode 0xEF)
	uint16_t condition;                   // 0x4e: condition variable for dynamic branching according to game status (opcode 0xEF)
	uint16_t reverb_depth_slide_length;   // 0x50: length of reverb depth slide
	uint16_t noise_clock;                 // 0x52: noise clock frequency
	uint16_t pause;                       // 0x54: set pause
	uint16_t beats_per_measure;           // 0x56: beats per measure
	uint16_t beat;                        // 0x58: current beat
	uint16_t ticks_per_beat;              // 0x5a: ticks per beat
	uint16_t tick;                        // 0x5c: current ticks per beat
	uint16_t measure;                     // 0x5e: current measure
};

struct AkaoSpuVoiceAttr
{
	uint32_t voice;          // 0x00: voice number
	uint32_t update_flags;   // 0x04: bitset that indicates what SPU registers need to be updated
	uint32_t addr;           // 0x08: waveform data start address (SPU address)
	uint32_t loop_addr;      // 0x0c: loop start address (SPU address)
	uint32_t a_mode;         // 0x10: ADSR: attack mode
	uint32_t s_mode;         // 0x14: ADSR: sustain mode
	uint32_t r_mode;         // 0x18: ADSR: release mode
	uint16_t pitch;          // 0x1c: pitch
	uint16_t ar;             // 0x1e: ADSR: attack rate
	uint16_t dr;             // 0x20: ADSR: decay rate
	uint16_t sl;             // 0x22: ADSR: sustain level
	uint16_t sr;             // 0x24: ADSR: sustain rate
	uint16_t rr;             // 0x26: ADSR: release rate
	SpuVolume volume;        // 0x28: volume (left and right)
};

struct AkaoDrumKeyAttr // Size = 5 bytes (warning: padding)
{
	uint8_t instrument;   // corresponding to opcode 0xA1
	uint8_t key;          // note number when playing the drum note
	uint16_t volume;      // corresponding to opcode 0xA8 (lower 8bit is a fractional part of the volume)
	uint8_t pan;          // corresponding to opcode 0xAA
};

enum class AkaoVoiceEffectFlags : uint32_t {
	None = 0x0000,
	VibratoEnabled = 0x0001,
	TremoloEnabled = 0x0002,
	ChannelPanLfoENabled = 0x0004,
	DrumModeEnabled = 0x0008,
	PlaybackRateSideChainEnabled = 0x0010,
	PitchVolumeSideChainEnabled = 0x0020,
	Unknown40 = 0x0040,
	Unknown80 = 0x0080,
	OverlayVoiceEnabled = 0x0100,
	AlternateVoiceEnabled = 0x0200,
	Unknown400 = 0x0400, // AlternateVoice Related
};

DECLARE_ENUM_METHODS(AkaoVoiceEffectFlags);

enum class AkaoUpdateFlags : uint32_t {
	None = 0x00000,
	VolumeLeft = 0x00001,
	VolumeRight = 0x00002,
	Unknown4 = 0x00004,
	Unknown8 = 0x00008,
	VoicePitch = 0x00010,
	Unknown20 = 0x00020,
	Unknown40 = 0x00040,
	VoiceStartAddr = 0x00080,
	AdsrAttackMode = 0x00100,
	AdsrSustainMode = 0x00200,
	AdsrReleaseMode = 0x00400,
	AdsrAttackRate = 0x00800,
	AdsrDecayRate = 0x01000,
	AdsrSustainRate = 0x02000,
	AdsrReleaseRate = 0x04000,
	AdsrSustainLevel = 0x08000,
	VoiceLoopStartAddr = 0x10000,
};

DECLARE_ENUM_METHODS(AkaoUpdateFlags);

struct SpuRegisters { // 0x7EBE4 (size = 0x2C)
	uint32_t voice;                                   // 0x00
	AkaoUpdateFlags update_flags;                     // 0x04 (7EBE8)
	uint32_t voice_start_addr;                        // 0x08 (7EBEC)
	uint32_t voice_loop_start_addr;                   // 0x0C (7EBF0)
	uint32_t adsr_attack_rate_mode;                   // 0x10 (7EBF4)
	uint32_t adsr_sustain_rate_mode;                  // 0x14 (7EBF8)
	uint32_t adsr_release_rate_mode;                  // 0x18 (7EBFC)
	uint16_t voice_pitch;                             // 0x1C (7EC00)
	uint16_t adsr_attack_rate;                        // 0x1E (7EC02)
	uint16_t adsr_decay_rate;                         // 0x20 (7EC04)
	uint16_t adsr_sustain_level;                      // 0x22 (7EC06)
	uint16_t adsr_sustain_rate;                       // 0x24 (7EC08)
	uint16_t adsr_release_rate;                       // 0x26 (7EC0A)
	int16_t volume_left;                              // 0x28 (7EC0C)
	int16_t volume_right;                             // 0x2A (7EC0E)
};

struct AkaoPlayerTrack // 0x96608, size = 0x108
{
	const uint8_t* addr;                              // 0x00
	const uint8_t* loop_addrs[4];                     // 0x04
	const AkaoDrumKeyAttr* drum;                      // 0x14
	const uint16_t* vibrato_lfo_addr;                 // 0x18
	const uint16_t* tremolo_lfo_addr;                 // 0x1c
	uint32_t pan_lfo_addr;                            // 0x20
	uint32_t overlay_voice_num;                       // 0x24
	uint32_t alternate_voice_num;                     // 0x28
	uint32_t master_volume;                           // 0x2c
	uint32_t pitch_of_note;                           // 0x30
	int32_t pitch_bend_slide_amplitude;               // 0x34
	AkaoVoiceEffectFlags voice_effect_flags;          // 0x38
	uint16_t field_3C;                                // 0x3c
	uint16_t field_3E;                                // 0x3e
	uint32_t field_40;                                // 0x40
	uint32_t volume;                                  // 0x44
	int32_t volume_slope;                             // 0x48
	int32_t pitch_bend_slope;                         // 0x4c
	uint16_t field_50;                                // 0x50
	uint16_t field_52;                                // 0x52
	uint16_t use_global_track;                        // 0x54
	uint8_t delta_time_counter;                       // 0x56
	uint8_t gate_time_counter;                        // 0x57
	uint16_t instrument;                              // 0x58
	uint16_t field_5A;                                // 0x5a
	uint16_t volume_slide_length;                     // 0x5c
	uint16_t overlay_balance_slide_length;            // 0x5e
	uint16_t pan;                                     // 0x60
	uint16_t pan_slide_length;                        // 0x62
	int16_t pitch_slide_length_counter;               // 0x64
	uint16_t octave;                                  // 0x66
	uint16_t pitch_slide_length;                      // 0x68
	uint16_t previous_note_number;                    // 0x6a
	uint16_t portamento_speed;                        // 0x6c
	uint16_t legato_flags;                            // 0x6e
	uint16_t field_70;                                // 0x70
	uint16_t vibrato_delay;                           // 0x72
	uint16_t vibrato_delay_counter;                   // 0x74
	uint16_t vibrato_rate;                            // 0x76
	uint16_t vibrato_rate_counter;                    // 0x78
	uint16_t vibrato_form;                            // 0x7a
	uint16_t vibrato_max_amplitude;                   // 0x7c
	uint16_t vibrato_depth;                           // 0x7e
	uint16_t vibrato_depth_slide_length;              // 0x80
	int16_t vibrato_depth_slope;                      // 0x82
	uint16_t field_84;                                // 0x84
	uint16_t tremolo_delay;                           // 0x86
	uint16_t tremolo_delay_counter;                   // 0x88
	uint16_t tremolo_rate;                            // 0x8a
	uint16_t tremolo_rate_counter;                    // 0x8c
	uint16_t tremolo_form;                            // 0x8e
	uint16_t tremolo_depth;                           // 0x90
	uint16_t tremolo_depth_slide_length;              // 0x92
	int16_t tremolo_depth_slope;                      // 0x94
	uint16_t field_96;                                // 0x96
	uint16_t pan_lfo_rate;                            // 0x98
	uint16_t pan_lfo_rate_counter;                    // 0x9a
	uint16_t pan_lfo_form;                            // 0x9c
	uint16_t pan_lfo_depth;                           // 0x9e
	uint16_t pan_lfo_depth_slide_length;              // 0xa0
	int16_t pan_lfo_depth_slope;                      // 0xa2
	uint16_t noise_on_off_delay_counter;              // 0xa4
	uint16_t pitchmod_on_off_delay_counter;           // 0xa6
	uint8_t field_A8[16];                             // 0xa8
	uint16_t loop_layer;                              // 0xb8
	uint16_t loop_counts[4];                          // 0xba
	uint16_t previous_delta_time;                     // 0xc2
	uint16_t forced_delta_time;                       // 0xc4
	uint16_t overlay_balance;                         // 0xc6
	int16_t overlay_balance_slope;                    // 0xc8
	int16_t pan_slope;                                // 0xca
	int16_t transpose;                                // 0xcc
	int16_t tuning;                                   // 0xce
	uint16_t note;                                    // 0xd0
	int16_t pitch_slide_amount;                       // 0xd2
	int16_t previous_transpose;                       // 0xd4
	int16_t vibrato_lfo_amplitude;                    // 0xd6
	int16_t tremolo_lfo_amplitude;                    // 0xd8
	int16_t pan_lfo_amplitude;                        // 0xda
	SpuRegisters spuRegisters;                        // 0xdc: voice number
	//AkaoUpdateFlags update_flags;                   // 0xe0: bitset that indicates what SPU registers need to be updated
	//uint32_t spu_addr;                              // 0xe4: waveform data start address (SPU address)
	//uint32_t loop_addr;                             // 0xe8: loop start address (SPU address)
	//uint32_t adsr_attack_mode;                      // 0xec: ADSR: attack mode
	//uint32_t adsr_sustain_mode;                     // 0xf0: ADSR: sustain mode
	//uint32_t adsr_release_mode;                     // 0xf4: ADSR: release mode
	//uint16_t voice_pitch;                           // 0xf8: pitch
	//uint16_t adsr_attack_rate;                      // 0xfa: ADSR: attack rate
	//uint16_t adsr_decay_rate;                       // 0xfc: ADSR: decay rate
	//uint16_t adsr_sustain_level;                    // 0xfe: ADSR: sustain level
	//uint16_t adsr_sustain_rate;                     // 0x100: ADSR: sustain rate
	//uint16_t adsr_release_rate;                     // 0x102: ADSR: release rate
	//uint16_t volume_left;                           // 0x104: volume left
	//uint16_t volume_right;                          // 0x106: volume right
};

struct VoiceUnknown9C60 { // size = 12
	uint32_t _u0;
	uint32_t _u4;
	uint32_t _u8;
};

typedef uint32_t addr_t;

constexpr auto PAN_LFO_ADDRS_LEN = 52;
constexpr auto EMPTY_ADPCM_LEN = 32;
constexpr auto INSTRUMENTS_LEN = 256;

class AkaoExecDec {
private:
	int32_t _unknown49538;
	int32_t _unknown4953C;
	int32_t _unknown49540;
	int32_t _unknown49544;
	//void *_message_handlers; // 0x49548 -> 0x49948
	static uint8_t _opcode_len[0x60]; // 0x49948
	static uint8_t _delta_times[12]; // 0x49C28
	static uint32_t _akaoEndSequence; // 0x49C40
	static uint32_t _pan_lfo_addrs[PAN_LFO_ADDRS_LEN]; // 0x4A5CC
	static uint8_t _empty_adpcm[EMPTY_ADPCM_LEN]; // 0x4A60C
	int32_t _unknown62E04;
	uint16_t _unknown62E0A;
	int32_t _spuActiveVoices; // 0x62F00
	uint32_t _unknown62F04;
	int32_t _unknown62F28;
	int32_t _unknown62F2C;
	int32_t _unknown62F30;
	int16_t _unknown62F40;
	int16_t _unknown62F44;
	int16_t _unknown62F48;
	int32_t _unknown62F5C;
	int32_t _voicesMask62F68;
	int32_t _reverb_position; // 64 is center, 0x62F70
	int32_t _previous_time_elapsed; // 0x62F78
	int32_t _unknown62F8C;
	int32_t _unknown62FB4;
	int32_t _unknownReverbRelated62FB8;
	int32_t _unknown62FD4;
	int32_t _unknown62FD8;
	int32_t _unknown62FE4;
	int32_t _unknown62FE8;
	uint8_t _unknown62FEA;
	int32_t _unknownFlags62FF8; // 0x10: force reverb enabled for all channels
	uint16_t _unknown62FCC;
	//int32_t _akaoNumQueuedMessages; // 0x63010
	AkaoInstrAttr _instruments[32]; // 0x75F28 -> 0x76728 (size = 2048)
	AkaoInstrAttr _instruments2[18]; // 0x76C68 -> 0x77118 (size = 1200)
	SpuRegisters _spuRegisters; // 0x7EBE4 -> 0x7EC10
	//std::queue<AkaoMessage> _akaoMessageQueue; // 0x81DC8
	int32_t _unknown83338;
	int16_t _remember_song_id_1; // 0x833DE;
	int16_t _remember_song_id_2; // 0x8337E;
	int32_t _unknown83398;
	//_akaoDataBeforeChannelMask // 0x83580
	char _spuMemoryManagementTable[SPU_MALLOC_RECSIZ * (4 + 1)];
	AkaoPlayerTrack _playerTracks[AKAO_CHANNEL_MAX]; // 0x96608 -> 0x97EC8
	AkaoPlayerTrack _playerTracks2[AKAO_CHANNEL_MAX]; // 0x97EC8 -> 0x99788
	AkaoPlayerTrack _playerTracks3[8]; // 0x99788 -> 0x99FC8
	int32_t _active_voices; // 0x99FCC
	int32_t _key_on_voices; // 0x9FD0
	int32_t _keyed_voices; // 0x9FD4
	int32_t _key_off_voices; // 0x99FD8;
	int32_t _unknown99FDC;
	int32_t _tempo; // 0x99FE0
	int32_t _time_counter; // 0x99FE8
	int32_t _noise_voices; // 0x99FEC
	int32_t _reverb_voices; // 0x99FF0
	int32_t _pitch_lfo_voices; // 0x99FF4
	int16_t _noise_clock; // 0x99FFA
	int16_t _akaoMessage; // 0x9A000
	int32_t _akaoDataAddr; // 0x9A004
	int32_t _unknown9A008;
	int32_t _unknown9A00C;
	int32_t _unknown9A010;
	int32_t _unknown9A014;
	AkaoPlayer _player; // 0x9A104 -> 0x9A164
	AkaoPlayer _player2; // 0x9A164 -> 0x9A1C4
	SpuReverbAttr _spuReverbAttr; // 0x9C564
	SpuCommonAttr _spuCommonAttr; // 0x9C578 -> 0x9C5C0
	VoiceUnknown9C60 _voiceUnknown9C6C0[AKAO_CHANNEL_MAX]; // 0x9C6C0 -> 0x9C7E0
	const Akao& _akao; // 0x1D0000
	int32_t getVoicesBits(AkaoPlayerTrack* playerTracks, int32_t voice_bit, int32_t mask);
	void spuNoiseVoice();
	void spuReverbVoice();
	void spuPitchLFOVoice();
	void finishChannel(AkaoPlayerTrack& playerTrack, AkaoPlayer& player, int voice);
	bool loadInstrument(AkaoPlayerTrack& playerTrack, AkaoPlayer& player, int voice);
	void akaoSetInstrument(AkaoPlayerTrack& playerTrack, uint8_t instrument);
	void adsrDecayRate(AkaoPlayerTrack& playerTrack);
	void adsrSustainLevel(AkaoPlayerTrack& playerTrack);
	void turnOnReverb(AkaoPlayerTrack& playerTrack, AkaoPlayer& player, int voice);
	void turnOffReverb(AkaoPlayerTrack& playerTrack, AkaoPlayer& player, int voice);
	void turnOnNoise(AkaoPlayerTrack& playerTrack, AkaoPlayer& player, int voice);
	void turnOffNoise(AkaoPlayerTrack& playerTrack, AkaoPlayer& player, int voice);
	void turnOnFrequencyModulation(AkaoPlayerTrack& playerTrack, AkaoPlayer& player, int voice);
	void turnOffFrequencyModulation(AkaoPlayerTrack& playerTrack, AkaoPlayer& player, int voice);
	void turnOnOverlayVoice(AkaoPlayerTrack& playerTrack);
	int executeChannel(AkaoPlayerTrack& playerTrack, AkaoPlayer& player, uint32_t voice);
	inline const uint8_t* curData(channel_t channel) const noexcept {
		return _akao.data(channel) + _playerTracks[channel].addr;
	}
	void resetCallback();
	void akaoInit(uint32_t* spuTransferStartAddr, AkaoInstrAttr* spuTransferStopAddr);
	int akaoPostMessage();
	void akaoLoadInstrumentSet(uint32_t* spuTransferStartAddr, AkaoInstrAttr* instruments);
	void akaoLoadInstrumentSet2(uint32_t* spuTransferStartAddr, AkaoInstrAttr* instruments);
	void akaoSpuSetTransferCallback();
	void akaoTransferSamples(uint8_t* addr, uint32_t size);
	void akaoWaitForSpuTransfer();
	void akaoWriteSpuRegisters(int32_t voice, SpuRegisters &a2);
	void akaoSetReverbMode(uint8_t reverbType);
	void akaoReset();
	void akaoClearSpu();
	long akaoTimerCallback();
	bool akaoMain();
	int akaoCalculateVolumeAndPitch(AkaoPlayerTrack &playerTracks);
	void akaoDspMain();
	void akaoDspOnTick(AkaoPlayerTrack& playerTracks, AkaoPlayer& player, uint32_t voice);
	void akaoDspOverlayVoice(AkaoPlayerTrack& playerTracks, uint32_t active_voices, uint32_t voice);
	void akaoUnknown2E954(AkaoPlayerTrack& playerTracks, uint32_t voice);
	void akaoUnknown30380();
	void turnOffCdInputReverb();
	void akaoKeyOffVoices(); // 0x2FE48
	void akaoLoadTracks();
	void akaoPlayMusic();
	int akaoTransferSeqBody();
	void akaoStopMusic(); // 0x29E98
	void stopMusic(); // 0x29F44
	void akaoPause();
	bool akaoDispatchVoice(AkaoPlayerTrack& playerTrack, AkaoPlayer& player, uint32_t voice);
	void akaoDispatchMessages();
	uint32_t akaoReadNextNote(AkaoPlayerTrack& playerTrack);
public:
	explicit AkaoExecDec(const Akao& akao, bool loadInstruments2) noexcept;
};
