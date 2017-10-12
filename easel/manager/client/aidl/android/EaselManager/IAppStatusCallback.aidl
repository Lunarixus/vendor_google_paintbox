package android.EaselManager;

/** Callback to handle app status change. */
interface IAppStatusCallback {
  /** Handles app starts event. */
  oneway void onAppStart();
  /**
   * Handles app ends event.
   *
   * exit the exit code of the app.
   */
  oneway void onAppEnd(int exit);
  /** Handles app error event. */
  oneway void onAppError(int error);
}
