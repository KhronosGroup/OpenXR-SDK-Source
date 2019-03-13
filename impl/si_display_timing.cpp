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

#include "impl/si_display_timing.h"
#include "impl/nanoseconds.h"
#ifdef XR_OS_WINDOWS
#include <thread>
#else
#include "unistd.h"
#endif
//  This simple timer assumes vsyncs happen every period beginning at start.

DisplayTiming::DisplayTiming() {
    period = XrDuration((1.0 / 60.0) * 1000ULL * 1000ULL * 1000ULL);
    phase = -period;
}

void DisplayTiming::WaitForNextFrame() {
    XrTime now = GetTimeNanosecondsXr();
    XrDuration delta = now % period;
    next = now + period - delta;
    while (now > (next + phase)) {
        // missed this period, shoot for the next one
        next += period;
    }
    XrDuration sleepNanos = (next + phase) - now;
#ifdef XR_OS_WINDOWS
    std::this_thread::sleep_for(std::chrono::nanoseconds(sleepNanos));
#else
    usleep(sleepNanos / 1000);
#endif
}
