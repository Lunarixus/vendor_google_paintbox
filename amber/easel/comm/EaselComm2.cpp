#include "EaselComm2.h"
#include "EaselComm2Impl.h"

namespace EaselComm2 {

Comm::Comm() {}
Comm::~Comm() {}

std::unique_ptr<Comm> Comm::create(Comm::Mode mode) {
  return std::make_unique<CommImpl>(mode);
}

}  // namespace EaselComm2