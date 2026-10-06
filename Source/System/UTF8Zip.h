#pragma once

#include "UTF8.h"

#ifdef SYSTEM_MINIZIP
#include <minizip/unzip.h>
#include <minizip/zip.h>
#else
#include "unzip.h"
#include "zip.h"
#endif

namespace RTE::UTF8 {

	inline unzFile OpenZipForReading(const std::string& fileName) {
		// minizip-ng's native OS stream treats narrow path arguments as UTF-8 and
		// converts them to wide paths on Windows. Avoid its legacy fopen callback,
		// which is disabled by the compatibility build used by this project.
		return unzOpen64(fileName.c_str());
	}

	inline zipFile OpenZipForWriting(const std::string& fileName, int appendMode) {
		return zipOpen64(fileName.c_str(), appendMode);
	}

} // namespace RTE::UTF8
