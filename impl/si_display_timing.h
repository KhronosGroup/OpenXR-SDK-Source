// Copyright (c) 2017 The Khronos Group Inc.
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
//
// Author:
//

#ifndef SRC_IMPL_SI_DISPLAY_TIMING_H_
#define SRC_IMPL_SI_DISPLAY_TIMING_H_

#include "xr_dependencies.h"
#include <openxr/openxr.h>

struct DisplayTiming {
    DisplayTiming();

    void WaitForNextFrame();

    XrDuration period;
    XrDuration phase;
    XrTime next;
};

#endif  // SRC_IMPL_SI_DISPLAY_TIMING_H_
