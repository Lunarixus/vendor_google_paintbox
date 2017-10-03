package android.EaselManager;

import android.EaselManager.IAppStatusCallback;

/** EaselManager service to control apps running on Easel */
interface IManagerService {
  /**
   * Starts an app and registers the app status callback.
   * Returns the error code.
   *
   * @param app app to start.
   * @param callback the status callback to register.
   */
  int startApp(int app, IAppStatusCallback callback);

  /**
   * Starts an app and registers the app status callback.
   * Returns the error code.
   *
   * @param app app to start.
   */
  int stopApp(int app);
}