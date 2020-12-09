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
	uint16_t _song_id;

public:
	Akao() noexcept :
		_reverb_type(0), _channel_offsets(),
		_channel_count(0), _data(nullptr), _data_size(0),
		_song_id(0)
	{}
	virtual ~Akao() noexcept;
	inline uint16_t* channelOffsets() noexcept {
		return &_channel_offsets[0];
	}
	inline uint16_t channelOffset(channel_t channel) const noexcept {
		return _channel_offsets[channel];
	}
	inline const uint8_t* data() const noexcept {
		return _data;
	}
	inline uint8_t* data() noexcept {
		return _data;
	}
	inline const uint8_t* data(channel_t channel) const noexcept {
		return _data + channelOffset(channel);
	}
	inline uint16_t dataSize() const noexcept {
		return _data_size;
	}
	bool setDataSize(uint16_t data_size) noexcept;
	inline uint16_t songId() const noexcept {
		return _song_id;
	}
	inline void setSongId(uint16_t song_id) noexcept {
		_song_id = song_id;
	}
	inline uint8_t reverbType() const noexcept {
		return _reverb_type;
	}
	inline void setReverbType(uint8_t reverb_type) noexcept {
		_reverb_type = reverb_type;
	}
	inline uint8_t channelCount() const noexcept {
		return _channel_count;
	}
	inline void setChannelCount(uint8_t channel_count) noexcept {
		_channel_count = channel_count;
	}
};
