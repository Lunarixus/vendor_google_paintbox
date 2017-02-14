#include <utils/Log.h>

#include "PbTiService.h"

/**
 * pbtiserver is a daemon process that hosts paintbox test service.
 */

int main(int argc __unused, char *argv[] __unused) {
    std::unique_ptr<pbti::PbTiService> PbTiService =
            std::make_unique<pbti::PbTiService>();

    int res = PbTiService->start();
    if (res == 0) {
        PbTiService->wait();
    } else {
        ALOGE("ERROR: PbTiService hasn't been started!");
    }

    return res;
}
