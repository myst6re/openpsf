/****************************************************************************/
//    Original author: kode54                                               //
/****************************************************************************/

#pragma once

#include <vector>

class CircularBuffer
{
   std::vector<int16_t> buffer;
   unsigned int readptr, writeptr, used, size;
public:
	CircularBuffer() noexcept;
	explicit CircularBuffer(unsigned int p_size);
	unsigned data_available() noexcept;
	unsigned free_space() noexcept;
	bool write(const int16_t* src, unsigned int count) noexcept;
	unsigned read(int16_t* dst, unsigned int count) noexcept;
	void reset() noexcept;
	bool resize(unsigned int p_size) noexcept;
	bool test_silence() const noexcept;
};
