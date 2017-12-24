#include "HdrPlusService.h"

/**
 * pbserver is a daemon process that hosts HDR+ service.
 */

int main(int argc __unused, char *argv[] __unused) {
    std::unique_ptr<pbcamera::HdrPlusService> hdrPlusService =
            std::make_unique<pbcamera::HdrPlusService>();

    int res = hdrPlusService->start();
    if (res == 0) {
        hdrPlusService->wait();
    }

    return res;
}
