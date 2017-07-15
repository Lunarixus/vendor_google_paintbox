#ifndef GOOGLEX_GCAM_IMAGE_ALLOCATOR_H_
#define GOOGLEX_GCAM_IMAGE_ALLOCATOR_H_

// This header is part of the public Gcam API; try not to include any headers
// unnecessarily, since any included headers also become part of the API.

#include <cstddef>

// Custom memory allocation for images created by gcam.

namespace gcam {

//-----------------------------------------------------------------------------
// Custom memory allocation for the sample arrays of image objects.
// During initialization of a Gcam object the constructor calls
// InitCustomMemoryAllocator(), supplying a custom malloc() and free()
// routines.  Subsequent memory allocations for the sample arrays of
// images will call the custom malloc() and free() routines, and the
// total amount of memory used for sample arrays will be tracked.  The
// current and peak usage can be queried by calling GetGcamImageMemCurrent()
// and GetGcamImageMemPeak() respectively.

//-----------------------------------------------------------------------------
// Custom memory allocation, based on functions for allocate and deallocate
// that follow the prototypes for malloc() and free(). This is exposed via a
// C-based API.

typedef void* (*MallocFunc)(size_t);
typedef void (*FreeFunc)(void*);

// Initialize custom memory allocation.
void InitCustomMemoryAllocator(MallocFunc custom_malloc,
                               FreeFunc custom_free);

//-----------------------------------------------------------------------------
// Memory allocation with memory alignment and tracking of the current
// and peak memory usage.

void* TrackedMemoryAllocate(size_t num_bytes);
void TrackedMemoryDeallocate(void* memory);

int GetGcamImageMemCurrent();
int GetGcamImageMemPeak();

}  // namespace gcam

#endif  // GOOGLEX_GCAM_IMAGE_ALLOCATOR_H_
