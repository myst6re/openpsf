#pragma once

#include <cstdint>
#include "akao.h"

class AkaoFile {
private:
	const char* _path;
public:
	AkaoFile(const char* path) noexcept : _path(path) {}
	bool open(Akao &akao) const noexcept;
};
