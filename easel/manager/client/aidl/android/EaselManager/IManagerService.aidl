package android.EaselManager;

import android.EaselManager.IServiceStatusCallback;

/** EaselManager service to control services running on Easel */
interface IManagerService {
  /**
   * Starts a service and registers the service status callback.
   * Returns the error code.
   *
   * @param service service to start.
   * @param callback the status callback to register.
   */
  int startService(int service, IServiceStatusCallback callback);

  /**
   * Stops a service.
   * Returns the error code.
   *
   * @param service service to stop.
   */
  int stopService(int service);

  /**
   * Request to set Easel to suspend mode.
   * Returns zero for success or error code for failure.
   *
   * @param service service to request suspend.
   */
  int suspend(int service);

  /**
   * Resume Easel from suspend mode.
   * Returns zero for success or error code for failure.
   *
   * @param service service to request suspend.
   */
  int resume(int service);
}
