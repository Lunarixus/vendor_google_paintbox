#ifndef GOOGLEX_GCAM_BASE_FILE_SAVER_H_
#define GOOGLEX_GCAM_BASE_FILE_SAVER_H_

// This header is part of the public Gcam API; try not to include any headers
// unnecessarily, since any included headers also become part of the API.

#include <cstddef>
#include <cstdint>
#include <string>

// Custom file saving.

namespace gcam {

// Prototype for a custom file saver.  The file saver writes a block of
// memory, described by a raw pointer and number of bytes, to a file with the
// given name.
typedef bool (*FileSaverFunc)(const void* /*data*/, size_t /*byte_count*/,
                              const std::string& /*filename*/);

// Initializes custom file saver.
void InitCustomFileSaver(FileSaverFunc custom_file_saver);

// Gets the custom file saver, or nullptr if one has not been set.
FileSaverFunc GetCustomFileSaver();

// Writes an empty file.  If the file already exists it will be clobbered.
// If a custom file saver was specified using InitCustomFileSaver(), it will be
// used.
bool WriteEmptyFile(const std::string& filename);

// Writes the given block of memory, described by a raw pointer and number of
// bytes, to file.  If the file already exists it will be clobbered.  If a
// custom file saver was specified using InitCustomFileSaver(), it will be used.
// Returns true on success, or false if the file could not be written.
bool WriteMemoryToFile(const uint8_t* data, size_t byte_count,
                       const std::string& filename);

// Writes the sequence of characters making up the string 'str' to file,
// excluding the additional terminating NUL character (if any).  If the file
// already exists it will be clobbered.  If a custom file saver was specified
// using InitCustomFileSaver(), it will be used. Returns true on success, or
// false if the file could not be written.
bool WriteStringToFile(const std::string& str, const std::string& filename);

}  // namespace gcam

#endif  // GOOGLEX_GCAM_BASE_FILE_SAVER_H_
