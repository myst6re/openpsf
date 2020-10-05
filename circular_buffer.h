/****************************************************************************/
//    Original author: kode54                                               //
/****************************************************************************/

#pragma once

#include <vector>

template <typename T>
class CircularBuffer
{
   std::vector<T> buffer;
   unsigned int readptr, writeptr, used, size;
public:
	CircularBuffer(unsigned int p_size);
	unsigned data_available() noexcept;
	unsigned free_space() noexcept;
	bool write(const T* src, unsigned int count);
	unsigned read(T* dst, unsigned int count);
	void reset() noexcept;
	void resize(unsigned int p_size);
	bool test_silence() const noexcept;
};
