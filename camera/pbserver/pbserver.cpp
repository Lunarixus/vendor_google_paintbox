#include "HdrPlusService.h"

/**
 * pbserver is a daemon process that hosts HDR+ service.
 */

int main(int argc __unused, char *argv[] __unused) {
    pbcamera::HdrPlusService *hdrPlusService = new pbcamera::HdrPlusService();

    int res = hdrPlusService->start();
    if (res == 0) {
        res = hdrPlusService->wait();
    }

    delete hdrPlusService;
    return res;
}
