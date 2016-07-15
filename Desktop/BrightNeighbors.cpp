/** @file
    @brief App that looks at each step of the patterns to find simultaneously
   bright beacons, records them all pairwise, and outputs them sorted by
   straight-line distance.

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

// Internal Includes
#include "Patterns.h"
#include "Positions.h"

// Library/third-party includes
#include <Eigen/Eigen>

// Standard includes
#include <utility>
#include <vector>
#include <iostream>
#include <cmath>

using BeaconList = std::vector<int>;
using Eigen::Vector3d;

inline double beaconSquaredDistance(int a, int b) {
  return (Vector3d::Map(OsvrHdkLedLocations_SENSOR0[a].data()) -
          Vector3d::Map(OsvrHdkLedLocations_SENSOR0[b].data()))
      .squaredNorm();
}

struct BeaconAdjacentBright {
  BeaconAdjacentBright(int idA, int idB, int patternStepNum)
      : beaconA(std::min(idA, idB)), beaconB(std::max(idA, idB)),
        patternStep(patternStepNum),
        squaredDistance(beaconSquaredDistance(beaconA, beaconB)) {}
  int beaconA;
  int beaconB;
  int patternStep;
  double squaredDistance;
};

std::ostream &operator<<(std::ostream &os, BeaconAdjacentBright const &adj) {
  os << "(" << adj.beaconA + 1 << "--" << adj.beaconB + 1 << ") @ "
     << adj.patternStep << " [" << std::sqrt(adj.squaredDistance) << " mm]";
  return os;
}

using AdjacentBrightnessList = std::vector<BeaconAdjacentBright>;

class BrightnessTracking {
public:
  void startPatternStep(int patternStep) { currentPatternStep_ = patternStep; }

  void recordBrightBeacon(int newBright) {
    for (auto &existingBright : brightBeacons_) {
      adjacentBrightness_.emplace_back(existingBright, newBright,
                                       currentPatternStep_);
      listSorted_ = false;
    }
    brightBeacons_.push_back(newBright);
  }

  void endPatternStep() { moveBrightBeaconsToList(); }
  AdjacentBrightnessList const &getSortedAdjacentBrightnessList() {
    if (!listSorted_) {
      sortAdjacentList();
    }
    return adjacentBrightness_;
  }

private:
  void moveBrightBeaconsToList() {
    brightBeaconsPerStep_.emplace_back(std::move(brightBeacons_));
    brightBeacons_.clear();
  }
  void sortAdjacentList() {
    std::sort(begin(adjacentBrightness_), end(adjacentBrightness_),
              [](BeaconAdjacentBright const &a, BeaconAdjacentBright const &b) {
                return a.squaredDistance < b.squaredDistance;
              });
    listSorted_ = true;
  }
  std::vector<BeaconList> brightBeaconsPerStep_;
  AdjacentBrightnessList adjacentBrightness_;
  bool listSorted_ = false;
  BeaconList brightBeacons_;
  int currentPatternStep_;
};

int main() {
  initPatterns();
#if 0
  auto numLeds = static_cast<int>(OsvrHdkLedIdentifier_SENSOR0_PATTERNS.size());
  auto patternLength =
      static_cast<int>(OsvrHdkLedLocations_SENSOR0_PATTERNS.front().length());
#else
  auto numLeds = static_cast<int>(NUM_LEDS_SENSOR0);
  auto patternLength = static_cast<int>(PATTERN_LENGTH);
#endif

  BrightnessTracking brightness;

  for (int patternStep = 0; patternStep < patternLength; ++patternStep) {
    brightness.startPatternStep(patternStep);
    for (int ledNum = 0; ledNum < numLeds; ++ledNum) {
      if (OsvrHdkLedIdentifier_SENSOR0_PATTERNS[ledNum][patternStep] == '*') {
        brightness.recordBrightBeacon(ledNum);
      }
    }
    brightness.endPatternStep();
  }

  for (auto &adj : brightness.getSortedAdjacentBrightnessList()) {
    std::cout << adj << std::endl;
  }
  return 0;
}