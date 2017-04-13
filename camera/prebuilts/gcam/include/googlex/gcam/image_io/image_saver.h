#ifndef GOOGLEX_GCAM_IMAGE_IO_IMAGE_SAVER_H_
#define GOOGLEX_GCAM_IMAGE_IO_IMAGE_SAVER_H_

// This header is part of the public Gcam API; try not to include any headers
// unnecessarily, since any included headers also become part of the API.

#include <atomic>
#include <string>

#include "googlex/gcam/image/t_image.h"
#include "googlex/gcam/image/yuv.h"

namespace gcam {

// How to save debugging images.
struct ImageSaverParams {
  void Print() const;

  // Destination folder for saving.
  char dest_folder[512] = "";

  // Optional filename prefix. If non-empty, should end in "_".
  char filename_prefix[512] = "";

  // Optional filename suffix. If non-empty, should begin with "_".
  char filename_suffix[512] = "";

  // Whether the debugging output normally saved as PNG should be saved as JPG
  // instead, overriding the ".png" extensions provided in the filename. JPG's
  // write much faster and take up less disk space.
  bool save_as_jpg_override = false;
};

class ImageSaver {
 public:
  ImageSaver(const ImageSaverParams& image_saver_params,
             const std::string& gcam_version);
  void Clear();

  // Get the full path for saving a file, or the empty string if the stage
  //   should not be saved.
  // In the GetUniquePath() variant, the filename is prepended with a sequence
  //   number, corresponding to the number of times that this function has been
  //   called, to ensure that the filename is unique.
  // For both of these functions, if a prefix or suffix were specified in
  //   ImageSaverParams, these will get applied to the filename.
  std::string GetPath(const char* filename) const;
  std::string GetUniquePath(const char* filename);

  // Save the given image to the folder specified in ImageSaverParams, with a
  //   unique name determined by GetUniquePath().
  // The YUV variants assume standard YUV matrix.
  // Returns the full path where the image was saved; this is typically ignored.
  std::string Save(const InterleavedReadViewU8& map, const char* filename);
  std::string Save(const InterleavedReadViewU16& map, const char* filename);

 private:
  const ImageSaverParams image_saver_params_;
  std::string gcam_version_;

  // The number of maps written so far. We make this atomic because it could
  // potentially be accessed by multiple Gcam threads.
  std::atomic<int> map_count_;
};

}  // namespace gcam

#endif  // GOOGLEX_GCAM_IMAGE_IO_IMAGE_SAVER_H_
