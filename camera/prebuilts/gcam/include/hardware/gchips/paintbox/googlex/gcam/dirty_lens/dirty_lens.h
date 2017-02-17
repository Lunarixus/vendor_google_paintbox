#ifndef HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_DIRTY_LENS_DIRTY_LENS_H_
#define HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_DIRTY_LENS_DIRTY_LENS_H_

// This header is part of the public Gcam API; try not to include any headers
// unnecessarily, since any included headers also become part of the API.

#include <cstdint>
#include <deque>
#include <memory>

#include "hardware/gchips/paintbox/googlex/gcam/image/t_image.h"
#include "hardware/gchips/paintbox/googlex/gcam/image/yuv.h"

namespace gcam {

// Analyzes an image and tells you the probability [0..1] that the lens is
//   dirty.  The image must have at least 307,200 pixels (this is the pixel
//   count of a VGA image), but the aspect ratio is free to vary.
// All parameters are required.
// Returns true on success.
bool GetDirtyLensProbability(const InterleavedReadViewU8& image,
                             float* dirty_probability,
                             float* raw_score);
bool GetDirtyLensProbability(const YuvReadView& yuv_read_view,
                             float* dirty_probability,
                             float* raw_score);
bool GetDirtyLensProbabilityFromJpegInMemory(
    const uint8_t* encoded_jpeg_file_in_memory,
    uint32_t encoded_jpeg_file_size_bytes,
    float* dirty_probability,
    float* raw_score);

// This class is designed to store a history around calls to the
//   'raw_score' values from the above GetDirtyLensProbability()
//   functions, and to recommend when you warn the user that their lens
//   is likely dirty.
// When you start up your camera and construct a DirtyLensHistory object,
//   give it the previous queue of score values.
// On each photo taken:
//   1. Call OnPhoto(), giving it the raw score for the new photo, and the
//        rough time (in microseconds) at which the photo was *taken*.
//   2. If it returns true, show the user a notification (or toast) telling
//        them that their lens is likely dirty, and that they should clean it.
//   3. Save the 'score_history_' deque to nonvolatile memory, so that
//        if the app is killed, battery dies, etc., no history is lost.
//        (Be sure to save the queue from the class, rather than maintaining
//        your own, because the class is careful to prune it over time.)
class DirtyLensHistory {
 public:
  DirtyLensHistory() {
    Initialize();
  }
  explicit DirtyLensHistory(std::deque<float> prev_history) {
    Initialize();
    score_history_ = prev_history;
  }

  void Reset() {
    score_history_.clear();
    score_history_.push_back(initial_score_);
  }

  // Call this once for each photo taken.
  // Be sure to provide the time (in microseconds) that the photo was *taken* -
  //   not the current time when you make the call.
  // Returns false if the lens is not dirty, or if the photo was taken very
  //   shortly after the previous photo, and was excluded from consideration.
  // Returns true if sufficient evidence has amassed and we believe that the
  //   lens is dirty.  In this case, you should alert the user in the UI.
  //   The internal history will be automatically reset.
  bool OnPhoto(uint64_t time_photo_was_taken_microseconds,
               float raw_score);

  std::deque<float> score_history_;

  // The bias towards firing upon initialization and after each alert.
  // Setting to 0 means no bias.
  // Note that if you update this value, you should also set
  //   score_history_[0] to the new value, as well.
  float initial_score_;

  // The minimum number of photos we must have seen, since initializing or
  // alerting, before we alert.
  int   min_photo_count_;

  // The history will be limited to this many scores.  (Because of the
  // exponential decay, this doesn't have to be very high.)
  int   max_photo_count_;

  // How quickly the influence of past photos decays (decreases).
  //   0 = No decay; the most recent N photos (up to max_photo_count_, and
  //     excluding photos taken in rapid succession) are weighted equally.
  //   inf = Maximum decay; only the most recent photo matters.
  // The decay rate is based only on the number of photos, and does not
  //   consider the times at which they were taken.  (This should be okay as
  //   long as 'min_seconds_between_photos_' is tuned reasonably.)
  // The weight of a past photo is determined by:
  //   expf(<number of photos ago> * -frame_influence_decay_rate_)
  float frame_influence_decay_rate_;

  // This controls the sensitivity with which the "dirty lens" signal fires
  //   (i.e. OnPhoto() returning a 'true' result).
  // To get the original intended behavior, use 0; this equates (more or less)
  //   to firing when there is a > 50% chance that the lens is dirty.
  // A reasonable range for this value is [-4..4].  The score values are
  //   fed through a sigmoid which outputs a probability in [0..1], so
  //   the effective bias caused by this threshold corresponds to the inputs
  //   of a sigmoid function:
  //
  //   Value    Fires when lens-dirty probability is greater than...
  //   -----    -------------------------------------------------
  //   -4        2%
  //   -3        5%
  //   -2       12%
  //    1       27%
  //    0       50%
  //    1       73%
  //    2       88%
  //    3       95%
  //    4       98%
  float weighted_score_threshold_;

  // If the user takes photos at a high frequency, they're very likely of the
  //   same scene or subject, and we don't want to enter them all into the
  //   queue.
  // If the time between shots isn't at least this many seconds, we'll skip
  //   analysis of the frame.
  float min_seconds_between_photos_;

 protected:
  uint64_t time_of_most_recent_photo_microseconds_;
  bool     first_photo_this_session_;

 private:
  void Initialize() {
    // Default settings.
    initial_score_ = -100.0f;
    min_photo_count_ = 5;
    max_photo_count_ = 32;
    frame_influence_decay_rate_ = 0.1f;
    weighted_score_threshold_ = 0.0f;
    min_seconds_between_photos_ = 5;  // TODO(geiss): Tune properly.

    // Reset the history.
    Reset();

    first_photo_this_session_ = true;
    time_of_most_recent_photo_microseconds_ = 0;
  }
};

}  // namespace gcam

#endif  // HARDWARE_GCHIPS_PAINTBOX_GOOGLEX_GCAM_DIRTY_LENS_DIRTY_LENS_H_
