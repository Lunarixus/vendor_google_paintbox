package android.EaselManager;

/** Callback to handle app status change. */
interface IAppStatusCallback {
  /** Handles app starts event. */
  void onAppStart();
  /** Handles app ends event. */
  void onAppEnd();
}
