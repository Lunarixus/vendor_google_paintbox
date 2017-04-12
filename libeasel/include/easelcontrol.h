/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Easel systemcontrol service API used by the Android framework and
 * corresponding Easel-side servers.
 */

#ifndef ANDROID_EASELCONTROL_H
#define ANDROID_EASELCONTROL_H

#include <functional>
#include <stdint.h>

#ifndef DISALLOW_COPY_AND_ASSIGN
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&) = delete;      \
  void operator=(const TypeName&) = delete
#endif  // DISALLOW_COPY_AND_ASSIGN

#ifndef ANDROID
// Supply the Android priority enum values for non-Android build
typedef enum android_LogPriority {
    ANDROID_LOG_UNKNOWN = 0,
    ANDROID_LOG_DEFAULT,    /* only for SetMinPriority() */
    ANDROID_LOG_VERBOSE,
    ANDROID_LOG_DEBUG,
    ANDROID_LOG_INFO,
    ANDROID_LOG_WARN,
    ANDROID_LOG_ERROR,
    ANDROID_LOG_FATAL,
    ANDROID_LOG_SILENT,     /* only for SetMinPriority(); must be last */
} android_LogPriority;
#endif

// Payload wrapper for a request or response with anonymous type.
struct ControlData {
    void const *body;  // body of the object.
    uint64_t size;  // size of the object.

    ControlData(void *body, size_t size) {
        this->body = body;
        this->size = size;
    }

    // Builds ControlData from an object.
    template<typename T>
    ControlData(const T &object) {
        this->body = (void *)&object;
        this->size = sizeof(T);
    }

    // Gets the object out of the ControlData as immutable constant.
    // returns null if sizeof(T) does not match size of payload.
    template<typename T>
    const T *getImmutable() const {
        if (size != sizeof(T)) {
            return nullptr;
        }
        return (T *)body;
    }

    // Gets the object out of the ControlData as mmutable constant.
    // size of the object will be modified.
    template<typename T>
    T *getMutable() {
        size = sizeof(T);
        return (T *)body;
    }
};

class EaselControlClient {
public:
    enum Camera { MAIN, FRONT };

    /*
     * Open an easelcontrol connection to Easel.  Initialize easelcomm
     * communications for the easelcontrol service.
     *
     * Returns zero for success or -1 for failure.
     */
    int open();
    /*
     * Temporary for TCP/IP-based mock.  Connects to the default easelcontrol
     * port on the named host.
     *
     * Returns zero for success or -1 for failure (errno is set).
     */
    int open(const char *easelhost);
    /*
     * Close the easelcontrol connection.
     */
    void close();
    /*
     * Activate Easel to HDR+ mode.
     *
     * Returns zero for success or error code for failure.
     */
    static int activate();
    /*
     * Deactivate Easel when not in HDR+ mode.
     *
     * Returns zero for success or error code for failure.
     */
    static int deactivate();
    /*
     * Start and/or configure one of the MIPI TX channels. This function will
     * block until Easel is powered.
     *
     * Returns zero for success or error code for failure.
     */
    static int startMipi(enum Camera camera, int rate);
    /*
     * Stop one of the MIPI RX and TX channels.
     *
     * Returns zero for success or error code for failure.
     */
    static int stopMipi(enum Camera camera);
    /*
     * Resume Easel, as when the camera app is started.  If Easel is suspended
     * then this will resume it. This function is non-blocking.
     *
     * Returns zero for success or error code for failure.
     */
    static int resume();
    /*
     * Suspend Easel, as when the camera app is closed.
     *
     * Returns zero for success or error code for failure.
     */
    static int suspend();

    /*
     * Sends a request to server.
     *
     * handlerId id of the handler on server side to handle this request.
     * rpcId id of the RPC to match the service of the handler.
     * request request to be sent.
     *
     * Returns zero for success or error code for failure.
     */
    static int sendRequest(int handlerId, int rpcId, const ControlData &request);

    /*
     * Sends a request to server with callback for returned response.
     *
     * handlerId id of the handler on server side to handle this request.
     * rpcId id of the RPC to match the service of the handler.
     * request request to be sent.
     * callback callback function to handle returned response from server.
     *
     * Returns zero for success or error code for failure.
     */
    static int sendRequestWithCallback(
            int handlerId,
            int rpcId,
            const ControlData &request,
            std::function<void(const ControlData &response)> callback);
};

// Interface to handle RPC.
class RequestHandler {
public:
    virtual ~RequestHandler() {};
    /*
     * Handles a RPC request.
     *
     * rpcId Identifies the actual RPC service in this handler.
     * request request from client to be handled, type is void*,
     * needs to be dynamically casted.
     * response response send back to client. If no response is needed,
     * response is set to nullptr.
     */
    virtual void handleRequest(
            int rpcId,
            const ControlData &request,
            ControlData *response) = 0;

protected:
    RequestHandler() {};

private:
    DISALLOW_COPY_AND_ASSIGN(RequestHandler);
};

class EaselControlServer {
public:
    /*
     * Open an easelcontrol connection.  Initialize easelcomm communications
     * for the easelcontrol service.
     *
     * Returns zero for success or -1 for failure.
     */
    int open();
    /*
     * Close the easelcontrol connection.
     */
    void close();
    /*
     * Return the Easel-side clock that maintains a time sync'ed with the
     * AP-side CLOCK_BOOTTIME.  This value is compatible with Android
     * systemTime(CLOCK_BOOTTIME) values of type nsecs_t read at the same
     * time on the AP side.
     *
     * clockval points to the returned clock value in nanoseconds.
     *
     * Returns zero for success or negative errno value for failure.
     * -EAGAIN means the clock has not been updated since boot or Easel last
     *     activated.
     */
    static int getApSynchronizedClockBoottime(int64_t *clockval);

    /*
     * Convert the local Easel-side clock to the clock sync'ed with the
     * AP-side CLOCK_BOOTTIME.  This value is compatible with Android
     * systemTime(CLOCK_BOOTTIME) values of type nsecs_t read at the same
     * time on the AP side.
     *
     * localClockval is the input easel side clock value in nanoseconds.
     * apSyncedClockval is the converted ap synced clock value in nanoseconds.
     *
     * Returns zero for success or negative errno value for failure.
     * -EAGAIN means the clock has not been updated since boot or Easel last
     * activated.
     */
    static int localToApSynchronizedClockBoottime(
            int64_t localClockval, int64_t *apSyncedClockval);

    /*
     * Return the last-recorded Vsync timestamp.  The timestamp is recorded
     * by Easel upon Vsync interrupt, using a clock synchronized with the
     * AP-side CLOCK_BOOTTIME.
     *
     * timestamp points to the returned timestamp value in nanoseconds.
     *
     * Returns zero for success or negative errno value for failure.
     * -EAGAIN means no timestamp has been recorded since boot or Easel last
     *     activated.
     */
    static int getLastEaselVsyncTimestamp(int64_t *timestamp);
    /*
     * Log to Android main log buffer.
     *
     * prio is a log priority such as ANDROID_LOG_INFO.
     * tag is the log tag string to use.
     * text is the string to log.
     */
    static void log(int prio, const char *tag, const char *text);

    /*
     * Registers a handler to handle CustomMsg with handlerId.
     * Returns the error code as int.
     *
     * handler handler to be registered
     * handlerId id of the handler. The CustomMsg with handlerId will be routed
     * to the registered handler.
     */
    int registerHandler(RequestHandler *handler, int handlerId);
};

/* Convenience wrapper for EaselControlServer::log() */
extern void easelLog(int prio, const char *tag, const char *format, ...);

/* returns true if Easel is present in the system */
extern bool isEaselPresent();

#endif // ANDROID_EASELCONTROL_H
