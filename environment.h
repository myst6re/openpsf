#pragma once

#include <cstdint>

class Environment {
public:
	Environment();
	virtual void reset() noexcept = 0;
	virtual bool can_execute() const noexcept = 0;
	virtual int execute(int16_t* buffer, uint16_t max_samples) noexcept = 0;
	virtual const char* last_error() const noexcept = 0;
};
