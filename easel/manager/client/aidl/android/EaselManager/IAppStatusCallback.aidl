package android.EaselManager;

/** Callback to handle app status change. */
interface IAppStatusCallback {
  /** Handles app starts event. */
  void onAppStart();
  /**
   * Handles app ends event.
   *
   * exit the exit code of the app.
   */
  void onAppEnd(int exit);
  /** Handles app error event. */
  void onAppError(int error);
}
