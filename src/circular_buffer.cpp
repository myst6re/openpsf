/****************************************************************************/
//    Original author: kode54                                               //
/****************************************************************************/

#include "openpsf/circular_buffer.h"
#include <algorithm>

auto constexpr silence_threshold = 8;

CircularBuffer::CircularBuffer(unsigned int p_size) :
	buffer(p_size), readptr(0), writeptr(0), used(0), size(p_size)
{
}

unsigned CircularBuffer::data_available() const noexcept
{
	return used;
}

unsigned CircularBuffer::free_space() const noexcept
{
	return size - used;
}

bool CircularBuffer::write(const int16_t* src, unsigned int count)
{
	if (count > free_space()) {
		return false;
	}

	while (count) {
		unsigned delta = size - writeptr;
		if (delta > count) {
			delta = count;
		}
		std::copy(src, src + delta, buffer.begin() + writeptr);
		used += delta;
		writeptr = (writeptr + delta) % size;
		src += delta;
		count -= delta;
	}

	return true;
}

unsigned CircularBuffer::read(int16_t* dst, unsigned int count)
{
	unsigned done = 0;
	for (;;) {
		unsigned delta = size - readptr;
		if (delta > used) delta = used;
		if (delta > count) delta = count;
		if (!delta) break;

		std::copy(buffer.begin() + readptr, buffer.begin() + readptr + delta, dst);
		dst += delta;
		done += delta;
		readptr = (readptr + delta) % size;
		count -= delta;
		used -= delta;
	}
	return done;
}

unsigned CircularBuffer::skip(unsigned int count) noexcept
{
	if (count > used) {
		count = used;
	}
	if (count > 0) {
		readptr = (readptr + count) % size;
		used -= count;
	}
	return count;
}

void CircularBuffer::reset() noexcept
{
	readptr = writeptr = used = 0;
}

void CircularBuffer::resize(unsigned int p_size)
{
	size = p_size;
	buffer.assign(p_size, 0);
	reset();
}

// True when every buffered (not yet read) sample is within the silence threshold
bool CircularBuffer::test_silence() const noexcept
{
	auto is_silent = [](int16_t sample) noexcept {
		return sample >= -silence_threshold && sample <= silence_threshold;
	};
	const unsigned int first_part = used < size - readptr ? used : size - readptr;
	const auto begin = buffer.begin() + readptr;
	return std::all_of(begin, begin + first_part, is_silent)
		&& std::all_of(buffer.begin(), buffer.begin() + (used - first_part), is_silent);
}
