/** @file
    @brief Implementation

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

// Internal Includes
#include "BeaconOrder.h"
#include "PatternString.h"
#include "array_init.h"

// Library/third-party includes
// - none

// Standard includes
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>
#include <climits>

/// 1-based indices WRT the tracking software of beacons we'd like to disable.
#if 0
/// HDK 1.3
/// Masked LEDs determined by BrightNeighbors using distance-cost method, 7 passes (6 LEDs).
static const auto DISABLED_TARGET0_BEACONS = {33, 13, 18, 32, 34, 5};
#else
/// HDK 2
/// Masked LEDs include the six not present in this hardware revision, with additional masked LEDs
/// informed by BrightNeighbors using the distance-cost method with 5 passes (4 LEDs). The last LED
/// suggsted by the algorithm would have been #5, which would have left one side of the HMD quite low
/// in LEDs, so the second-highest cost from that round, #26, was hand chosen instead.
static const auto DISABLED_TARGET0_BEACONS = {
    /// beacons not present in this hardware revision
    12, 13, 14, 25, 27, 28,
    /// beacons disabled due to interference
    33, 18, 32, 26};
#endif
/// 1-based indices WRT the tracking software of the beacons on the rear that never light up anyway.
static const auto DISABLED_TARGET1_BEACONS = {1, 4};

/// needs a bidirectional iterator
template <typename Iterator> inline uint8_t byteFromBoolContainer(Iterator lsb) {
  uint8_t ret = 0;
  Iterator it = lsb;
  // advancing one too far is OK because of the "one past the end" iterator.
  std::advance(it, CHAR_BIT);
  for (int count = CHAR_BIT; count != 0; --count) {
    // can unconditionally move backwards because we moved it one too far.
    --it;
    if (*it) {
      ret = ret << 1 | static_cast<uint8_t>(0x01);
    } else {
      ret = ret << 1;
    }
  }
  return ret;
}

template <typename Container> inline uint8_t byteFromBoolContainerAtBit(Container &&c, std::size_t bit) {
  auto it = begin(std::forward<Container>(c));
  if (bit > 0) {
    std::advance(it, bit);
  }
  return byteFromBoolContainer(it);
}

template <typename Container> inline uint8_t byteFromBoolContainerAtByte(Container &&c, std::size_t b) {
  return byteFromBoolContainerAtBit(std::forward<Container>(c), b * CHAR_BIT);
}

int main() {
  std::vector<bool> mask(NUM_LEDS, true);
  for (auto &beacon1based : DISABLED_TARGET0_BEACONS) {
    auto bit = oneBasedTarget0BeaconToFirmwareBit(beacon1based);
    mask[bit] = false;
  }

  for (auto &beacon1based : DISABLED_TARGET1_BEACONS) {
    auto bit = oneBasedTarget1BeaconToFirmwareBit(beacon1based);
    mask[bit] = false;
  }

  for (int i = 0; i < LED_LINE_LENGTH; ++i) {
    auto b = byteFromBoolContainerAtByte(mask, i);
    std::cout << "0x" << std::hex << static_cast<unsigned>(b) << ", ";
  }
  std::cout << std::endl;

  return 0;
}
