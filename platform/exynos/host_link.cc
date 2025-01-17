/*
 * Copyright (C) 2022 The Android Open Source Project
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

#include "chre/platform/host_link.h"
#include "chre/core/event_loop_manager.h"
#include "chre/core/host_comms_manager.h"
#include "chre/platform/shared/host_protocol_chre.h"
#include "chre/platform/shared/nanoapp_load_manager.h"
#include "chre/platform/system_time.h"
#include "chre/platform/system_timer.h"
#include "chre/util/flatbuffers/helpers.h"
#include "chre/util/nested_data_ptr.h"
#include "include/chre/target_platform/host_link_base.h"
#include "mailbox.h"

// The delete operator is generated by the compiler but not actually called,
// empty implementations are provided to avoid linker warnings.
void operator delete(void * /*ptr*/) {}
void operator delete(void * /*ptr*/, size_t /*sz*/) {}

namespace chre {
namespace {

struct UnloadNanoappCallbackData {
  uint64_t appId;
  uint32_t transactionId;
  uint16_t hostClientId;
  bool allowSystemNanoappUnload;
};

inline HostCommsManager &getHostCommsManager() {
  return EventLoopManagerSingleton::get()->getHostCommsManager();
}

void setTimeSyncRequestTimer(Nanoseconds delay) {
  static TimerHandle sHandle;
  static bool sHandleInitialized;

  if (sHandleInitialized) {
    EventLoopManagerSingleton::get()->cancelDelayedCallback(sHandle);
  }

  auto callback = [](uint16_t /*type*/, void * /*data*/, void * /*extraData*/) {
    HostLinkBase::sendTimeSyncRequest();
  };
  sHandle = EventLoopManagerSingleton::get()->setDelayedCallback(
      SystemCallbackType::TimerSyncRequest, nullptr /*data*/, callback, delay);
  sHandleInitialized = true;
}

}  // anonymous namespace

void sendDebugDumpResultToHost(uint16_t /*hostClientId*/,
                               const char * /*debugStr*/,
                               size_t /*debugStrSize*/, bool /*complete*/,
                               uint32_t /*dataCount*/) {
  // TODO(b/230134803): Implement this.
}

HostLinkBase::HostLinkBase() {
  int32_t rv = mailboxReadChre(mMsgBuffer, CHRE_MESSAGE_TO_HOST_MAX_SIZE,
                               receive, this /*cookie*/);
  CHRE_ASSERT_LOG((rv == 0),
                  "Failed to register inbound message handler %" PRId32, rv);
}

void HostLinkBase::receive(void *cookie, void *message, int messageLen) {
  auto *instance = static_cast<HostLinkBase *>(cookie);
  // TODO(b/237819962): A crude way to initially determine daemon's up - set
  // a flag on the first message received. This is temporary until a better
  // way to do this is available.
  instance->setInitialized(true);

  if (!HostProtocolChre::decodeMessageFromHost(message, messageLen)) {
    LOGE("Failed to decode msg %p of len %zu", message, messageLen);
  }
}

bool HostLink::sendMessage(const MessageToHost *message) {
  bool success = false;
  if (isInitialized()) {
    constexpr size_t kFixedReserveSize = 80;
    ChreFlatBufferBuilder builder(message->message.size() + kFixedReserveSize);
    HostProtocolChre::encodeNanoappMessage(
        builder, message->appId, message->toHostData.messageType,
        message->toHostData.hostEndpoint, message->message.data(),
        message->message.size(), message->toHostData.appPermissions,
        message->toHostData.messagePermissions, message->toHostData.wokeHost);
    success = (send(builder.GetBufferPointer(), builder.GetSize()) == 0);

    // Only invoke on success as returning false from this method will cause
    // core logic to do the appropriate cleanup.
    if (success) {
      EventLoopManagerSingleton::get()
          ->getHostCommsManager()
          .onMessageToHostComplete(message);
    }
  } else {
    LOGW("Dropping outbound message: host link not initialized yet");
  }
  return success;
}

// TODO(b/239096709): HostMessageHandlers member function implementations are
// expected to be (mostly) identical for any platform that uses flatbuffers
// to encode messages - refactor the host link to merge the multiple copies
// we currently have.
void HostMessageHandlers::handleNanoappMessage(uint64_t appId,
                                               uint32_t messageType,
                                               uint16_t hostEndpoint,
                                               const void * /* messageData */,
                                               size_t messageDataLen) {
  LOGD("Parsed nanoapp message from host: app ID 0x%016" PRIx64
       ", endpoint "
       "0x%" PRIx16 ", msgType %" PRIu32 ", payload size %zu",
       appId, hostEndpoint, messageType, messageDataLen);

  // TODO(b/230134803): Implement this.
}

void HostMessageHandlers::handleHubInfoRequest(uint16_t /* hostClientId */) {
  // TODO(b/230134803): Implement this.
}

void HostMessageHandlers::handleNanoappListRequest(uint16_t hostClientId) {
  // TODO(b/230134803): Implement this.
}

void HostMessageHandlers::handlePulseRequest() {}

void HostMessageHandlers::sendFragmentResponse(uint16_t hostClientId,
                                               uint32_t transactionId,
                                               uint32_t fragmentId,
                                               bool success) {
  constexpr size_t kInitialBufferSize = 52;
  ChreFlatBufferBuilder builder(kInitialBufferSize);
  HostProtocolChre::encodeLoadNanoappResponse(
      builder, hostClientId, transactionId, success, fragmentId);

  if (!getHostCommsManager().send(builder.GetBufferPointer(),
                                  builder.GetSize())) {
    LOGE("Failed to send fragment response for HostClientID: %" PRIx16
         " , FragmentID: %" PRIx32 " transactionID: %" PRIx32,
         hostClientId, fragmentId, transactionId);
  }
}

void HostMessageHandlers::handleLoadNanoappRequest(
    uint16_t hostClientId, uint32_t transactionId, uint64_t appId,
    uint32_t appVersion, uint32_t appFlags, uint32_t targetApiVersion,
    const void *buffer, size_t bufferLen, const char *appFileName,
    uint32_t fragmentId, size_t appBinaryLen, bool respondBeforeStart) {
  UNUSED_VAR(appFileName);

  loadNanoappData(hostClientId, transactionId, appId, appVersion, appFlags,
                  targetApiVersion, buffer, bufferLen, fragmentId, appBinaryLen,
                  respondBeforeStart);
}

void HostMessageHandlers::handleUnloadNanoappRequest(
    uint16_t hostClientId, uint32_t transactionId, uint64_t appId,
    bool allowSystemNanoappUnload) {
  LOGD("Unload nanoapp request from client %" PRIu16 " (txnID %" PRIu32
       ") for appId 0x%016" PRIx64 " system %d",
       hostClientId, transactionId, appId, allowSystemNanoappUnload);
  // TODO(b/230134803): Implement this.
}

void HostMessageHandlers::handleTimeSyncMessage(int64_t offset) {
  LOGD("Time sync msg received with offset %" PRId64, offset);

  SystemTime::setEstimatedHostTimeOffset(offset);

  // Schedule a time sync request since offset may drift
  constexpr Seconds kClockDriftTimeSyncPeriod =
      Seconds(60 * 60 * 6);  // 6 hours
  setTimeSyncRequestTimer(kClockDriftTimeSyncPeriod);
}

void HostMessageHandlers::handleDebugDumpRequest(uint16_t /* hostClientId */) {
  // TODO(b/230134803): Implement this.
}

void HostMessageHandlers::handleSettingChangeMessage(fbs::Setting setting,
                                                     fbs::SettingState state) {
  Setting chreSetting;
  bool chreSettingEnabled;
  if (HostProtocolChre::getSettingFromFbs(setting, &chreSetting) &&
      HostProtocolChre::getSettingEnabledFromFbs(state, &chreSettingEnabled)) {
    EventLoopManagerSingleton::get()->getSettingManager().postSettingChange(
        chreSetting, chreSettingEnabled);
  }
}

void HostMessageHandlers::handleSelfTestRequest(uint16_t /* hostClientId */) {
  // TODO(b/230134803): Implement this.
}

void HostMessageHandlers::handleNanConfigurationUpdate(bool /* enabled */) {
  LOGE("NAN unsupported.");
}

}  // namespace chre
