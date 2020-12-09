#pragma once

#include <cstdint>
#include "akao.h"
#include "psf_file.h"

class PsfFileAkao : public PsfFile {
private:
	const char* _path;
	int psf_load_callback(const uint8_t* exe, size_t exe_size,
		const uint8_t* reserved, size_t reserved_size) noexcept override;
public:
	PsfFileAkao(const char* path) noexcept : _path(path) {}
	bool open(Akao& akao, )
};
