#ifndef PAINTBOX_EASELCOMM2_IMPL_H
#define PAINTBOX_EASELCOMM2_IMPL_H

#include "EaselComm2.h"

#include <unordered_map>

#include "easelcomm.h"

namespace EaselComm2 {

class CommImpl : public Comm {
 public:
  CommImpl(Mode mode);
  ~CommImpl();

  int open(EaselService service_id, long timeout_ms = 0) override;
  int open(EaselService service_id) override;
  int openPersistent(EaselService service_id, bool logging) override;
  bool connected() override;
  void close() override;

  int startReceiving() override;

  void joinReceiving() override;

  int send(int channelId, const HardwareBuffer* payload) override;

  int send(int channelId, const void* body, size_t body_size,
           const HardwareBuffer* payload) override;

  int send(int channelId, const std::string& s,
           const HardwareBuffer* payload) override;

  int send(int channelId, const ::google::protobuf::MessageLite& proto,
           const HardwareBuffer* payload) override;

  int send(int channelId, const std::vector<HardwareBuffer>& buffers,
           int* lastId) override;

  void registerHandler(int channelId, Handler handler) override;

  int receivePayload(const Message& message, HardwareBuffer* buffer) override;

 private:
  std::unique_ptr<EaselComm> mComm;
  std::mutex mHandlerMapMutex;
  std::unordered_map<int, Handler>
      mHandlerMap;  // GUARDED_BY(mHandlerMapMutex);
};

}  // namespace EaselComm2

#endif  // PAINTBOX_EASELCOMM2_IMPL_H
