#pragma once

#include <cstdint>
#include "akao_instr.h"

class AkaoInstrFile {
private:
	const char* _path;
public:
	AkaoInstrFile(const char* path) noexcept : _path(path) {}
	int open(AkaoInstrAttr* akaoAttr, int max_instruments) const noexcept;
};

class AkaoInstrAdpcmFile {
private:
	const char* _path;
public:
	AkaoInstrAdpcmFile(const char* path) noexcept : _path(path) {}
	bool open(AkaoInstrAdpcm& akaoAdpcm) const noexcept;
};
