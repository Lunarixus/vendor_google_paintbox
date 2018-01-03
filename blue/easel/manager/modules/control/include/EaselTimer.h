#ifndef __EASEL_TIMER_H__
#define __EASEL_TIMER_H__

#include <chrono>
#include <thread>

/*
 * This class abstracts a timer. Upon expiration of the timer, a callback is
 * called.
 */
class EaselTimer {
 public:
  /*
   * Starts timer.
   *
   * Returns 0 on success or a negative error for failure.
   */
  int start(const std::chrono::milliseconds duration,
            std::function<void()> callback,
            bool fireOnce = false);

  /*
   * Restarts timer.
   *
   * Returns 0 on success or a negative error for failure.
   */
  int restart();

  /*
   * Stops timer.
   *
   * NOTE: Do not call stop() within the callback. This will result in a
   * deadlock.
   *
   * Returns 0 on success or a negative error for failure.
   */
  int stop();

 private:
  /* Thread waiting for timer to expire before bite */
  std::thread mThread;

  /*
   * Lock used for synchronizing the waiting thread with the application thread.
   */
  std::mutex mMutex;

  /* Condition used to notify thread. */
  std::condition_variable mCondition;

  /* Flag for coordinating with the thread. Protected with mMutex. */
  bool mStopFlag;

  /* Thread function for mThread. */
  void timer(std::chrono::milliseconds period, std::function<void()> callback,
             bool fireOnce);
};

#endif  // __EASEL_TIMER_H__
