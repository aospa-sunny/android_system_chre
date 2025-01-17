/*
 * Copyright (C) 2020 The Android Open Source Project
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

#ifndef CHRE_CROSS_VALIDATOR_WIFI_MANAGER_H_
#define CHRE_CROSS_VALIDATOR_WIFI_MANAGER_H_

#include <cinttypes>
#include <cstdint>

#include <pb_common.h>
#include <pb_decode.h>
#include <pb_encode.h>

#include "chre/util/singleton.h"
#include "chre_api/chre.h"

#include "chre_cross_validation_wifi.nanopb.h"
#include "chre_test_common.nanopb.h"

#include "wifi_scan_result.h"

namespace chre {

namespace cross_validator_wifi {

/**
 * Class to manage a CHRE cross validator wifi nanoapp.
 */
class Manager {
 public:
  /**
   * Handle a CHRE event.
   *
   * @param senderInstanceId The instand ID that sent the event.
   * @param eventType The type of the event.
   * @param eventData The data for the event.
   */
  void handleEvent(uint32_t senderInstanceId, uint16_t eventType,
                   const void *eventData);

 private:
  struct CrossValidatorState {
    uint16_t hostEndpoint;
  };

  chre_cross_validation_wifi_Step mStep = chre_cross_validation_wifi_Step_INIT;

  //! Struct that holds some information about the state of the cross validator
  CrossValidatorState mCrossValidatorState;

  // TODO: Find a better max scan results val
  static constexpr uint8_t kMaxScanResults = UINT8_MAX;
  WifiScanResult mApScanResults[kMaxScanResults];
  WifiScanResult mChreScanResults[kMaxScanResults];

  //! The current index that cross validator should assign to when a new scan
  //! result comes in.
  uint8_t mChreScanResultsI = 0;

  // The max chre scan results to be validated
  uint8_t mMaxChreResultSize = 100;

  uint8_t mChreScanResultsSize = 0;
  uint8_t mApScanResultsSize = 0;

  //! The number of wifi scan results processed from CHRE apis
  uint8_t mNumResultsProcessed = 0;

  //! Bools indicating that data collection is complete for each side
  bool mApDataCollectionDone = false;
  bool mChreDataCollectionDone = false;

  /**
   * Handle a message from the host.
   * @param senderInstanceId The instance id of the sender.
   * @param data The message from the host's data.
   */
  void handleMessageFromHost(uint32_t senderInstanceId,
                             const chreMessageFromHostData *data);

  /**
   * Handle a step start message from the host.
   *
   * @param stepStartCommand The step start command proto.
   */
  void handleStepStartMessage(
      chre_cross_validation_wifi_StepStartCommand stepStartCommand);

  /**
   * @param success true if the result was success.
   * @param errMessage The error message that should be sent to host with
   * failure.
   *
   * @return The TestResult proto message that is encoded with these fields.
   */
  chre_test_common_TestResult makeTestResultProtoMessage(
      bool success, const char *errMessage = nullptr);

  /**
   * @param capabilitiesFromChre The number with flags that represent the
   *        different wifi capabilities.
   * @return The wifi capabilities proto message for the host.
   */
  chre_cross_validation_wifi_WifiCapabilities makeWifiCapabilitiesMessage(
      uint32_t capabilitiesFromChre);

  /**
   * Handle a wifi scan result data message sent from AP.
   *
   * @param hostData The message.
   */
  void handleDataMessage(const chreMessageFromHostData *hostData);

  /**
   * Handle a wifi scan result event from a CHRE event.
   *
   * @param event the wifi scan event from CHRE api.
   */
  void handleWifiScanResult(const chreWifiScanEvent *event);

  /**
   * Compare the AP and CHRE wifi scan results and send test result to host.
   */
  void compareAndSendResultToHost();

  /**
   * Verify the wifi scan results are matching between AP and CHRE.
   *
   * @param testResultOut Pointer to the test result proto message which will be
   *                      sent back to host, whose bool and message depends on
   *                      the checks inside this method.
   */
  void verifyScanResults(chre_test_common_TestResult *testResultOut);

  /**
   * Get the scan result that has the same bssid as the scan result passed.
   *
   * @param results        The array of scan results to search through.
   * @param resultsSize    The number of valid scan result objects in the array
   *                       passed.
   * @param queryResult    The result to search with.
   * @param resultIndexOut The pointer where the scan result index will be
   *                       copied to if the result was found.
   * @return true if the scan result was found.
   */
  bool getMatchingScanResult(WifiScanResult *results, uint8_t resultsSize,
                             const WifiScanResult &queryResult,
                             uint8_t *resultIndexOut);

  /**
   * Setup WiFi scan monitoring from CHRE apis.
   *
   * @return true if chreWifiConfigureScanMonitorAsync() returns true
   */
  bool setupWifiScanMonitoring();

  /**
   * Handle wifi async result event with event data.
   *
   * @param result The data for the event.
   */
  void handleWifiAsyncResult(const chreAsyncResult *result);

  /**
   * The function to pass as the encode function pointer for the errorMessage
   * field of the TestResult message.
   *
   * @param stream The stream to write bytes to.
   * @param field The field that should be encoded. Unused by us.
   * @param arg The argument that will be set to a pointer to the string to
   * encode as error message.
   */
  static bool encodeErrorMessage(pb_ostream_t *stream,
                                 const pb_field_t * /*field*/,
                                 void *const *arg);
};

// The chre cross validator manager singleton.
typedef chre::Singleton<Manager> ManagerSingleton;

}  // namespace cross_validator_wifi

}  // namespace chre

#endif  // CHRE_CROSS_VALIDATOR_WIFI_MANAGER_H_
