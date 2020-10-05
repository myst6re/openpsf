/****************************************************************************/
//    Original author: kode54                                               //
/****************************************************************************/

#include "circular_buffer.h"
#include <algorithm>

auto constexpr silence_threshold = 8;

template <typename T>
CircularBuffer<T>::CircularBuffer(unsigned int p_size) :
	readptr(0), writeptr(0), size(p_size), used(0)
{
	buffer.reserve(p_size);
}

template <typename T>
unsigned CircularBuffer<T>::data_available() noexcept
{
	return used;
}

template <typename T>
unsigned CircularBuffer<T>::free_space() noexcept
{
	return size - used;
}

template <typename T>
bool CircularBuffer<T>::write(const T* src, unsigned int count)
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

template <typename T>
unsigned CircularBuffer<T>::read(T* dst, unsigned int count)
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

template <typename T>
void CircularBuffer<T>::reset() noexcept
{
	readptr = writeptr = used = 0;
}

template <typename T>
void CircularBuffer<T>::resize(unsigned int p_size)
{
	size = p_size;
	buffer.reserve(p_size);
	reset();
}

template <typename T>
bool CircularBuffer<T>::test_silence() const noexcept
{
	T* begin = (T*)&buffer[0];
	T first = *begin;
	*begin = silence_threshold * 2;
	T* p = begin + size;
	while ((unsigned int)(*--p + silence_threshold) <= (unsigned int)silence_threshold * 2) {}
	*begin = first;
	return p == begin && ((unsigned int)(first + silence_threshold) <= (unsigned int)silence_threshold * 2);
}
