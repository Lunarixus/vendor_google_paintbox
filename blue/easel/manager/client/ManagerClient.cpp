#include "EaselManager.h"
#include "ManagerClientImpl.h"

namespace android {
namespace EaselManager {

ManagerClient::ManagerClient() {}
ManagerClient::~ManagerClient() {}

std::unique_ptr<ManagerClient> ManagerClient::create() {
  return std::make_unique<ManagerClientImpl>();
}

}  // namespace EaselManager
}  // namespace android
