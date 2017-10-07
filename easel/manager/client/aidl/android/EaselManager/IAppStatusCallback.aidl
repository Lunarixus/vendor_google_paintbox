package android.EaselManager;

/** Callback to handle app status change. */
interface IAppStatusCallback {
  /** Handles app starts event. */
  oneway void onAppStart();
  /** Handles app ends event. */
  oneway void onAppEnd();
  /** Handles app error event. */
  oneway void onAppError(int error);
}
