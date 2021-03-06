/** @file
    @brief App used to determine the contents of BeaconOrder.h - the mapping
   between the identification/order used for beacons in the trakcing software
   and the order they're referred to in the firmware - by matching their
   patterns.

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
#include "PatternString.h"
#include "Patterns.h"
#include "array_init.h"

// Library/third-party includes
// - none

// Standard includes
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <vector>

int findPattern(std::vector<std::string> const &patternList, std::string const &pattern) {
  auto it = std::find(begin(patternList), end(patternList), pattern);
  if (it == end(patternList)) {
    return -1;
  }
  return std::distance(begin(patternList), it);
}

void dumpBeaconOrder(int targetNum, std::vector<int> const &order) {
  std::cout << "static const auto TARGET" << targetNum << "_BEACON_ORDER = {\n";

  for (auto &beacon : order) {
    std::cout << beacon << ", ";
  }
  std::cout << "};" << std::endl;
}

int main() {
  default_array_init();
  initPatterns();
  std::vector<int> targetOrder0(OsvrHdkLedIdentifier_SENSOR0_PATTERNS.size(), -1);
  std::vector<int> targetOrder1(OsvrHdkLedIdentifier_SENSOR1_PATTERNS.size(), -1);
  for (int led = 0; led < NUM_LEDS; ++led) {
    auto patternString = getPatternString(led, pattern_array);
    auto target0 = findPattern(OsvrHdkLedIdentifier_SENSOR0_PATTERNS, patternString);
    if (target0 < 0) {
      auto target1 = findPattern(OsvrHdkLedIdentifier_SENSOR1_PATTERNS, patternString);
      if (target1 < 0) {
        std::cout << "Could not find a match for firmware pattern " << led << ": " << patternString << std::endl;
      } else {
        std::cout << "Firmware pattern " << led << " is rear target pattern " << target1 << std::endl;
        targetOrder1[target1] = led;
      }
    } else {
      std::cout << "Firmware pattern " << led << " is front target pattern " << target0 << std::endl;
      targetOrder0[target0] = led;
    }
  }
  dumpBeaconOrder(0, targetOrder0);
  dumpBeaconOrder(1, targetOrder1);
  return 0;
}
