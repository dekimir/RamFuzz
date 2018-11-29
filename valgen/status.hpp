// Copyright 2016-2018 The RamFuzz contributors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

namespace ramfuzz {

enum ResponseStatus {
  OK_TERMINAL = 10,    ///< Successfully processed termination notification.
  OK_VALUE,            //< Successfully processed request for a random value.
  ERR_FEW_PARTS = 20,  ///< Every request must have at least two parts.
  ERR_TERM_TAKES_2,    ///< Termination notification must have exactly 2 parts.
  ERR_VALUE_TAKES_5,   ///< Request for random value must have exactly 5 parts.
  ERR_WRONG_VALUEID,   ///< The last time a value was requested here,
                       ///< it had another valueid.
  END_MARKER_DO_NOT_USE
};

inline uint8_t status(ResponseStatus s) { return s; }

}  // namespace ramfuzz
