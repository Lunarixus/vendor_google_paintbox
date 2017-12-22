package android.EaselManager;

/** Callback to handle service status change. */
interface IServiceStatusCallback {
  /** Handles app service starts event. */
  void onServiceStart();

  /**
   * Handles service ends event.
   *
   * exit the exit code of the service.
   */
  void onServiceEnd(int exit);

  /** Handles service error event. */
  void onServiceError(int error);
}
