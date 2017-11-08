#include "ImxService.h"

int main(int argc __unused, char *argv[] __unused) {
    auto imxService = std::make_unique<imx::ImxService>();
    return imxService->start();
}
