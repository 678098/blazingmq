// Copyright 2022-2024 Bloomberg Finance L.P.
// SPDX-License-Identifier: Apache-2.0
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

// bmqu_debugmonitor.h                                                -*-C++-*-
#ifndef INCLUDED_BMQU_DEBUGMONITOR
#define INCLUDED_BMQU_DEBUGMONITOR

// BDE
#include <bsls_atomic.h>

namespace BloombergLP {
namespace bmqu {

// =================
// struct DebugValue
// =================

struct DebugValue {
    // TYPES
    enum Enum { e_ROLLOVER = 0, e_CONSTRUCT_DISPATCHER_EVENT = 1 };

    static const char* toAscii(DebugValue::Enum value)
    {
        switch (value) {
        case e_ROLLOVER: return "rollover";
        case e_CONSTRUCT_DISPATCHER_EVENT: return "construct_dispatcher_event";
        default: return "undefined";
        }
    }
};

// ==================
// class DebugMonitor
// ==================

class DebugMonitor {
  public:
    template <DebugValue::Enum>
    inline static bsls::Types::Int64 update(bsls::Types::Int64 delta)
    {
        static bsls::AtomicInt64 value(0);
        return value.addRelaxed(delta);
    }
};

}  // close package namespace
}  // close enterprise namespace

#endif
