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

#ifndef CHRE_CORE_NANOAPP_H_
#define CHRE_CORE_NANOAPP_H_

#include <cinttypes>

#include "chre/core/event.h"
#include "chre/core/event_ref_queue.h"
#include "chre/platform/platform_nanoapp.h"
#include "chre/util/dynamic_vector.h"
#include "chre/util/fixed_size_vector.h"
#include "chre/util/system/debug_dump.h"
#include "chre/util/system/napp_permissions.h"
#include "chre_api/chre/event.h"

namespace chre {

/**
 * A class that tracks the state of a Nanoapp including incoming events and
 * event registrations.
 *
 * Inheritance is used to separate the common interface with common
 * implementation part (chre::Nanoapp) from the common interface with
 * platform-specific implementation part (chre::PlatformNanoapp) from the purely
 * platform-specific part (chre::PlatformNanoappBase). However, this inheritance
 * relationship does *not* imply polymorphism, and this object must only be
 * referred to via the most-derived type, i.e. chre::Nanoapp.
 */
class Nanoapp : public PlatformNanoapp {
 public:
  Nanoapp();
  ~Nanoapp();

  /**
   * @return The unique identifier for this Nanoapp instance
   */
  uint16_t getInstanceId() const {
    return mInstanceId;
  }

  /**
   * Assigns an instance ID to this Nanoapp. This must be called prior to
   * starting this Nanoapp.
   */
  void setInstanceId(uint16_t instanceId) {
    mInstanceId = instanceId;
  }

  /**
   * @return The current total number of bytes the nanoapp has allocated.
   */
  size_t getTotalAllocatedBytes() const {
    return mTotalAllocatedBytes;
  }

  /**
   * @return The peak total number of bytes the nanoapp has allocated.
   */
  size_t getPeakAllocatedBytes() const {
    return mPeakAllocatedBytes;
  }

  /**
   * Sets the total number of bytes the nanoapp has allocated. Also, modifies
   * the peak allocated bytes if the current total is higher than the peak.
   *
   * @param The total number of bytes the nanoapp has allocated.
   */
  void setTotalAllocatedBytes(size_t totalAllocatedBytes) {
    mTotalAllocatedBytes = totalAllocatedBytes;
    if (mTotalAllocatedBytes > mPeakAllocatedBytes) {
      mPeakAllocatedBytes = mTotalAllocatedBytes;
    }
  }

  /**
   * @return true if the nanoapp should receive broadcast event
   */
  bool isRegisteredForBroadcastEvent(const Event *event) const;

  /**
   * Updates the Nanoapp's registration so that it will receive broadcast events
   * with the given event type.
   *
   * @param eventType The event type that the nanoapp will now be registered to
   *     receive
   * @param groupIdMask A mask of group IDs to register the nanoapp for. If an
   *     event is sent that targets any of the group IDs in the mask, it will
   *     be delivered to the nanoapp.
   */
  void registerForBroadcastEvent(
      uint16_t eventType, uint16_t groupIdMask = kDefaultTargetGroupMask);

  /**
   * Updates the Nanoapp's registration so that it will not receive broadcast
   * events with the given event type.
   *
   * @param eventType The event type that the nanoapp will be unregistered from
   *    assuming the group ID also matches a valid entry.
   * @param groupIdMask The mask of group IDs that will be unregistered from.
   */
  void unregisterForBroadcastEvent(
      uint16_t eventType, uint16_t groupIdMask = kDefaultTargetGroupMask);

  /**
   * Configures whether nanoapp info events will be sent to the nanoapp.
   * Nanoapps are not sent nanoapp start/stop events by default.
   *
   * @param enable true if events are to be sent, false otherwise.
   */
  void configureNanoappInfoEvents(bool enable);

  /**
   * Configures whether host sleep events will be sent to the nanoapp. Nanoapps
   * are not sent sleep/awake events by default.
   *
   * @param enable true if events are to be sent, false otherwise.
   */
  void configureHostSleepEvents(bool enable);

  /**
   * Configures whether debug dump events will be sent to the nanoapp. Nanoapps
   * are not sent debug dump events by default.
   *
   * @param enable true if events are to be sent, false otherwise.
   */
  void configureDebugDumpEvent(bool enable);

  /**
   * Configures whether a user settings event will be sent to the nanoapp
   * for a specified setting (@see CHRE_USER_SETTINGS)
   * Nanoapps are not sent user settings events by default.
   *
   * @param setting The user setting that the nanoapp wishes to configure
   * events for.
   *
   * @param enable true if events are to be sent, false otherwise.
   */
  void configureUserSettingEvent(uint8_t setting, bool enable);

  /**
   * Sends an event to the nanoapp to be processed.
   *
   * @param event A pointer to the event to be processed
   */
  void processEvent(Event *event);

  /**
   * Log info about a single host wakeup that this nanoapp triggered by storing
   * the count of wakeups in mWakeupBuckets.
   */
  void blameHostWakeup();

  /*
   * If buckets not full, then just pushes a 0 to back of buckets. If full, then
   * shifts down all buckets from back to front and sets back to 0, losing the
   * latest bucket value that was in front.
   *
   * @param numBuckets the number of buckets to cycle into to mWakeupBuckets
   */
  void cycleWakeupBuckets(size_t numBuckets);

  /**
   * Prints state in a string buffer. Must only be called from the context of
   * the main CHRE thread.
   *
   * @param debugDump The object that is printed into for debug dump logs.
   */
  void logStateToBuffer(DebugDumpWrapper &debugDump) const;

  /**
   * @return true if the nanoapp is permitted to use the provided permission.
   */
  bool permitPermissionUse(uint32_t permission) const;

  /**
   * Configures notification updates for a given host endpoint.
   *
   * @param hostEndpointId The ID of the host endpoint.
   * @param enable true to enable notifications.
   *
   * @return true if the configuration is successful.
   */
  bool configureHostEndpointNotifications(uint16_t hostEndpointId, bool enable);

  /**
   * Publishes RPC services for this nanoapp.
   *
   * @param services A pointer to the list of RPC services to publish.
   *   Can be null if numServices is 0.
   * @param numServices The number of services to publish, i.e. the length of
   * the services array.
   *
   * @return true if the publishing is successful.
   */
  bool publishRpcServices(struct chreNanoappRpcService *services,
                          size_t numServices);

  /**
   * @return The list of RPC services pushblished by this nanoapp.
   */
  const DynamicVector<struct chreNanoappRpcService> &getRpcServices() const {
    return mRpcServices;
  }

 private:
  uint16_t mInstanceId = kInvalidInstanceId;

  //! The total number of wakeup counts for a nanoapp.
  uint32_t mNumWakeupsSinceBoot = 0;

  //! The total memory allocated by the nanoapp in bytes.
  size_t mTotalAllocatedBytes = 0;

  //! The peak total number of bytes allocated by the nanoapp.
  size_t mPeakAllocatedBytes = 0;

  //! The number of buckets for wakeup logging, adjust along with
  //! EventLoop::kIntervalWakupBucketInMins.
  static constexpr size_t kMaxSizeWakeupBuckets = 4;

  //! A fixed size buffer of buckets that keeps track of the number of host
  //! wakeups over time intervals.
  FixedSizeVector<uint16_t, kMaxSizeWakeupBuckets> mWakeupBuckets;

  //! Metadata needed for keeping track of the registered events for this
  //! nanoapp.
  struct EventRegistration {
    EventRegistration(uint16_t eventType_, uint16_t groupIdMask_)
        : eventType(eventType_), groupIdMask(groupIdMask_) {}

    uint16_t eventType;
    uint16_t groupIdMask;
  };

  //! The set of broadcast events that this app is registered for.
  // TODO: Implement a set container and replace DynamicVector here. There may
  // also be a better way of handling this (perhaps we map event type to apps
  // who care about them).
  DynamicVector<EventRegistration> mRegisteredEvents;

  //! The registered host endpoints to receive notifications for.
  DynamicVector<uint16_t> mRegisteredHostEndpoints;

  //! The list of RPC services for this nanoapp.
  DynamicVector<struct chreNanoappRpcService> mRpcServices;

  //! @return index of event registration if found. mRegisteredEvents.size() if
  //!     not.
  size_t registrationIndex(uint16_t eventType) const;

  /**
   * A special function to deliver GNSS measurement events to nanoapps and
   * handles version compatibility.
   *
   * @param event The pointer to the event
   */
  void handleGnssMeasurementDataEvent(const Event *event);

  bool isRegisteredForHostEndpointNotifications(uint16_t hostEndpointId) const {
    return mRegisteredHostEndpoints.find(hostEndpointId) !=
           mRegisteredHostEndpoints.size();
  }
};

}  // namespace chre

#endif  // CHRE_CORE_NANOAPP_H_
