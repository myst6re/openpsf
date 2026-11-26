/****************************************************************************/
//    Original author: kode54                                               //
/****************************************************************************/

#pragma once

#include <cstdint>
#include <vector>

class CircularBuffer
{
   std::vector<int16_t> buffer;
   unsigned int readptr, writeptr, used, size;
public:
	CircularBuffer(unsigned int p_size);
	unsigned data_available() const noexcept;
	unsigned free_space() const noexcept;
	bool write(const int16_t* src, unsigned int count);
	unsigned read(int16_t* dst, unsigned int count);
	unsigned skip(unsigned int count) noexcept;
	void reset() noexcept;
	void resize(unsigned int p_size);
	bool test_silence() const noexcept;
};
