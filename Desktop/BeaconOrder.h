/** @file
    @brief Header

    @date 2016

    @author
    Sensics, Inc.
    <http://sensics.com/osvr>
*/

// Copyright 2016 Sensics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache 2.0

#ifndef INCLUDED_BeaconOrder_h_GUID_86CA83BB_03DE_49AF_D736_08498ABBC48C
#define INCLUDED_BeaconOrder_h_GUID_86CA83BB_03DE_49AF_D736_08498ABBC48C

// Internal Includes
// - none

// Library/third-party includes
// - none

// Standard includes
#include <vector>

using BeaconOrderContainer = std::vector<int>;

/// These lists contain the indices of LEDs as known by the firmware, in the
/// order that the tracking software refers to them. So, the first element is
/// referred to by the tracking software as (1-based) beacon 1, but the firmware
/// actually thinks of it as LED 34.
static const BeaconOrderContainer TARGET0_BEACON_ORDER = {34, 35, 28, 29, 30, 31, 24, 25, 26, 27, 20, 21,
                                                          22, 23, 16, 17, 18, 19, 12, 13, 14, 15, 8,  9,
                                                          10, 11, 4,  5,  6,  7,  0,  1,  2,  3};
static const auto TARGET0_BEACON_COUNT = TARGET0_BEACON_ORDER.size();
static const BeaconOrderContainer TARGET1_BEACON_ORDER = {36, 37, 38, 39, 32, 33};

static int oneBasedTarget0BeaconToFirmwareBit(int beacon) { return TARGET0_BEACON_ORDER[beacon - 1]; }

static int oneBasedTarget1BeaconToFirmwareBit(int beacon) { return TARGET1_BEACON_ORDER[beacon - 1]; }

static int oneBasedCombinedTargetBeaconToFirmwareBit(int beacon) {
  auto zeroBased = beacon - 1;
  if (zeroBased < TARGET0_BEACON_COUNT) {
    return TARGET0_BEACON_ORDER[zeroBased];
  }
  return TARGET1_BEACON_ORDER[zeroBased - TARGET0_BEACON_COUNT];
}

#endif // INCLUDED_BeaconOrder_h_GUID_86CA83BB_03DE_49AF_D736_08498ABBC48C
