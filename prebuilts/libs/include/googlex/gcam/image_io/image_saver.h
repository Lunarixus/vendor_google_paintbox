#ifndef GOOGLEX_GCAM_IMAGE_IO_IMAGE_SAVER_H_
#define GOOGLEX_GCAM_IMAGE_IO_IMAGE_SAVER_H_

// This header is part of the public Gcam API; try not to include any headers
// unnecessarily, since any included headers also become part of the API.

#include <atomic>
#include <string>

#include "googlex/gcam/image/t_image.h"

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
};

class ImageSaver {
 public:
  ImageSaver(const std::string& dest_folder,
             const std::string& filename_prefix,
             const std::string& filename_suffix,
             const std::string& gcam_version);
  ImageSaver(const ImageSaverParams& params, const std::string& gcam_version)
      : ImageSaver(params.dest_folder, params.filename_prefix,
                   params.filename_suffix, gcam_version) {}
  void Clear();

  // Get the full path for saving a file, or the empty string if the stage
  //   should not be saved.
  // In the GetUniquePath() variant, the filename is prepended with a sequence
  //   number, corresponding to the number of times that this function has been
  //   called, to ensure that the filename is unique.
  // For both of these functions, if a prefix or suffix were specified in
  //   ImageSaverParams, these will get applied to the filename.
  std::string GetPath(const std::string& filename) const;
  std::string GetUniquePath(const std::string& filename);

  // Save the given image to the folder specified in ImageSaverParams, with a
  //   unique name determined by GetUniquePath().
  // Returns the full path where the image was saved; this is typically ignored.
  std::string Save(const InterleavedReadViewU8& map,
                   const std::string& filename);
  std::string Save(const InterleavedReadViewU16& map,
                   const std::string& filename);
  std::string Save(const PlanarReadViewU16& map,
                   const std::string& filename);

 private:
  std::string dest_folder_;
  std::string filename_prefix_;
  std::string filename_suffix_;
  std::string gcam_version_;

  // The number of maps written so far. We make this atomic because it could
  // potentially be accessed by multiple Gcam threads.
  std::atomic<int> map_count_;
};

}  // namespace gcam

#endif  // GOOGLEX_GCAM_IMAGE_IO_IMAGE_SAVER_H_
