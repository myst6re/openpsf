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
	CircularBuffer(unsigned int p_size);
	unsigned data_available() noexcept;
	unsigned free_space() noexcept;
	bool write(const int16_t* src, unsigned int count);
	unsigned read(int16_t* dst, unsigned int count);
	void reset() noexcept;
	void resize(unsigned int p_size);
	bool test_silence() const noexcept;
};
