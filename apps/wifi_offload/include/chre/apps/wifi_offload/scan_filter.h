/*
 * Copyright (C) 2017 The Android Open Source Project
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

#ifndef CHRE_WIFI_OFFLOAD_SCAN_FILTER_H_
#define CHRE_WIFI_OFFLOAD_SCAN_FILTER_H_

#include "chre/apps/wifi_offload/wifi_offload.h"

#include "chre/apps/wifi_offload/generated/flatbuffers_types_generated.h"
#include "chre/apps/wifi_offload/preferred_network.h"

namespace wifi_offload {

/**
 * Instruction on how to filter scan results before waking up the applications
 * processor.
 */
class ScanFilter {
 public:
  /* Corresponding flatbuffers-generated data-type used to serialize and
   * deserialize instances of this class */
  using FbsType = fbs::ScanFilter;

  ScanFilter();

  ~ScanFilter() = default;

  bool operator==(const ScanFilter &other) const;

  flatbuffers::Offset<ScanFilter::FbsType> Serialize(
      flatbuffers::FlatBufferBuilder *builder) const;

  bool Deserialize(const ScanFilter::FbsType &fbs_filter);

  void Log() const;

  Vector<PreferredNetwork> networks_to_match_;  // empty means match all
  int8_t min_rssi_threshold_dbm_;
};

}  // namespace wifi_offload

#endif  // CHRE_WIFI_OFFLOAD_SCAN_FILTER_H_
