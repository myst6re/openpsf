#pragma once

#include <cstdint>

constexpr auto AKAO_CHANNEL_MAX = 24;

typedef uint8_t channel_t;

class Akao {
private:
	uint8_t _reverb_type;
	uint16_t _channel_offsets[AKAO_CHANNEL_MAX];
	uint8_t _channel_count;
	uint8_t* _data;
	uint16_t _data_size;
	
public:
	Akao() noexcept :
		_reverb_type(0), _channel_offsets(),
		_channel_count(0), _data(nullptr), _data_size(0)
	{}
	bool open(const char* path) noexcept;
	inline const uint8_t* data() const noexcept {
		return _data;
	}
	inline const uint8_t* data(channel_t channel) const noexcept {
		return _data + _channel_offsets[channel];
	}
	inline uint16_t dataSize() const noexcept {
		return _data_size;
	}
	inline uint8_t reverbType() const noexcept {
		return _reverb_type;
	}
	inline uint8_t channelCount() const noexcept {
		return _channel_count;
	}
};

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
	uint32_t spucnt;                      // 0x38: SPUCNT shadow (0xA13C)
	uint32_t reverb_type;                 // 0x3c: reverb type
	uint32_t reverb_depth;                // 0x40: reverb depth (Q16 fixed-point number)
	int32_t reverb_depth_slope;           // 0x44: slope of reverb depth slider (Q16 fixed-point number)
	uint16_t tempo_slide_length;          // 0x48: tempo slide length
	uint16_t song_id;                     // 0x4a: current song id
	uint16_t condition_ack;               // 0x4c: the last matched condition value (opcode 0xEF)
	uint16_t condition;                   // 0x4e: condition variable for dynamic branching according to game status (opcode 0xEF)
	uint16_t reverb_depth_slide_length;   // 0x50: length of reverb depth slide
	uint16_t noise_clock;                 // 0x52: noise clock frequency
	uint16_t field_54;                    // 0x54: unknown (can be altered by opcode 0xF3)
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

struct AkaoDrumKeyAttr
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
};

AkaoVoiceEffectFlags operator|(AkaoVoiceEffectFlags flags, AkaoVoiceEffectFlags other) noexcept {
	return static_cast<AkaoVoiceEffectFlags>(static_cast<uint32_t>(flags) | static_cast<uint32_t>(other));
}

bool operator&(AkaoVoiceEffectFlags flags, AkaoVoiceEffectFlags other) noexcept {
	return bool(static_cast<AkaoVoiceEffectFlags>(static_cast<uint32_t>(flags) & static_cast<uint32_t>(other)));
}

AkaoVoiceEffectFlags& operator|=(AkaoVoiceEffectFlags &flags, const AkaoVoiceEffectFlags &other) noexcept {
	flags = static_cast<AkaoVoiceEffectFlags>(static_cast<uint32_t>(flags) | static_cast<uint32_t>(other));
	return flags;
}

AkaoVoiceEffectFlags& operator&=(AkaoVoiceEffectFlags& flags, const AkaoVoiceEffectFlags& other) noexcept {
	flags = static_cast<AkaoVoiceEffectFlags>(static_cast<uint32_t>(flags) & static_cast<uint32_t>(other));
	return flags;
}

AkaoVoiceEffectFlags operator~(AkaoVoiceEffectFlags flags) noexcept {
	return static_cast<AkaoVoiceEffectFlags>(~static_cast<uint32_t>(flags));
}

struct AkaoPlayerTrack // 0x80096608, size = 0x108
{
	uint32_t addr;                                    // 0x00
	uint32_t loop_addrs[4];                           // 0x04
	const AkaoDrumKeyAttr* drum_addr;                 // 0x14
	int16_t* vibrato_lfo_addr;                        // 0x18
	int16_t* tremolo_lfo_addr;                        // 0x1c
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
	uint16_t volume_slide_length_counter;             // 0x5c
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
	uint32_t voice;                                   // 0xdc: voice number
	uint32_t update_flags;                            // 0xe0: bitset that indicates what SPU registers need to be updated
	uint32_t spu_addr;                                // 0xe4: waveform data start address (SPU address)
	uint32_t loop_addr;                               // 0xe8: loop start address (SPU address)
	uint32_t adsr_attack_mode;                        // 0xec: ADSR: attack mode
	uint32_t adsr_sustain_mode;                       // 0xf0: ADSR: sustain mode
	uint32_t adsr_release_mode;                       // 0xf4: ADSR: release mode
	uint16_t pitch;                                   // 0xf8: pitch
	uint16_t adsr_attack_rate;                        // 0xfa: ADSR: attack rate
	uint16_t adsr_decay_rate;                         // 0xfc: ADSR: decay rate
	uint16_t adsr_sustain_level;                      // 0xfe: ADSR: sustain level
	uint16_t adsr_sustain_rate;                       // 0x100: ADSR: sustain rate
	uint16_t adsr_release_rate;                       // 0x102: ADSR: release rate
	uint32_t volume;                                  // 0x104: volume (left and right)
};

struct AkaoInstrumentAttr // 0x80075F28
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

struct AkaoChannelState {
	bool finished;
	AkaoPlayerTrack playerTrack;
};

constexpr auto PAN_LFO_ADDRS_LEN = 52;

class AkaoExec {
private:
	static uint8_t _opcode_len[0x60];
	static uint32_t _pan_lfo_addrs[PAN_LFO_ADDRS_LEN]; // 0x4A5CC
	AkaoPlayer _player;
	AkaoChannelState _channels[AKAO_CHANNEL_MAX];
	AkaoInstrumentAttr _instruments[256];
	uint32_t _unknown62F04;
	int32_t _unknown99FCC;
	int32_t _noise_voices; // 0x9FEC
	int32_t _reverb_voices; // 0x9FF0
	int32_t _pitch_lfo_voices; // 0x9FF4
	int32_t _unknown9A10C;
	int32_t _unknown9A110;
	int32_t _unknown9A114;
	const Akao& _akao;
	void spuNoiseVoice();
	void spuReverbVoice();
	void spuPitchLFOVoice();
	void akaoSetInstrument(AkaoPlayerTrack& playerTrack, uint8_t instrument);
	bool loadInstrument(const uint8_t* data, AkaoPlayerTrack& playerTrack, int argument2);
	void finishChannel(AkaoPlayerTrack& playerTrack, int argument2);
	void turnOnReverb(AkaoPlayerTrack& playerTrack, int argument2);
	void turnOffReverb(AkaoPlayerTrack& playerTrack, int argument2);
	void turnOnNoise(AkaoPlayerTrack& playerTrack, int argument2);
	void turnOffNoise(AkaoPlayerTrack& playerTrack, int argument2);
	void turnOnFrequencyModulation(AkaoPlayerTrack& playerTrack, int argument2);
	void turnOffFrequencyModulation(AkaoPlayerTrack& playerTrack, int argument2);
	void turnOnOverlayVoice(const uint8_t* data, AkaoPlayerTrack& playerTrack);
	int execute_channel(channel_t channel, int argument2);
	inline const uint8_t* curData(channel_t channel) const noexcept {
		return _akao.data(channel) + _channels[channel].playerTrack.addr;
	}
public:
	explicit AkaoExec(const Akao& akao) noexcept :
		_channels(), _akao(akao) {}
	bool reset();
	int32_t execute(int16_t* buffer, uint16_t max_samples);
};

