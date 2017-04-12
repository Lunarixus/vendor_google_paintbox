#define LOG_TAG "EaselControlTest"

#include <utils/Log.h>

#include "easelcontrol.h"

namespace android {

const int SERVER_RUN_SECONDS = 60 * 120;

int body() {
    int ret;
    EaselControlServer easelControl;

    printf("Opening easelControl\n");
    ret = easelControl.open();
    if (ret) {
        printf("easelControl.open() = %d\n", ret);
    }

    sleep(SERVER_RUN_SECONDS);

    printf("Closing easelControl\n");
    easelControl.close();

    return 0;
}

}   // namespace android

int main() {
    return android::body();
}
