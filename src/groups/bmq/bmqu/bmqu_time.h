// Copyright 2017-2023 Bloomberg Finance L.P.
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

#ifndef INCLUDED_BMQU_TIME
#define INCLUDED_BMQU_TIME

//@PURPOSE: Provide a pluggable functional interface to system clocks.
//
//@CLASSES:
//   bmqu::Time: namespace for pluggable interface to system clocks
//
//@DESCRIPTION: This component provides a 'struct', 'bmqu::Time', in which
// are defined a series of static methods for retrieving the current system
// time from the currently installed mechanism.  This component provides access
// to monotonic clock, real-time (wall) clock and a high resolution timer.  The
// clocks and the timer can be replaced with `mockTime`, which is useful while
// testing components which rely on system clock or timer.

// BDE
#include <bsl_functional.h>
#include <bslma_allocator.h>
#include <bslma_managedptr.h>
#include <bslma_usesbslmaallocator.h>
#include <bslmf_nestedtraitdeclaration.h>
#include <bsls_keyword.h>
#include <bsls_timeinterval.h>
#include <bsls_types.h>

namespace BloombergLP {
namespace bmqu {

// ===========
// struct Time
// ===========

/// Provide a platform-neutral pluggable functional interface to system
/// clocks.
struct Time {
    // TYPES

    /// Signature of the callback for the real time and monotonic clocks.
    typedef bsl::function<bsls::TimeInterval()> SystemTimeCb;

    /// Signature of the callback for the high resolution timer.
    typedef bsl::function<bsls::Types::Int64()> HighResolutionTimeCb;

    class MockTimeGuard;

    // CLASS METHODS

    /// @brief Initialize the platform-provided system clocks and high
    ///        resolution timer.
    ///
    /// May be called multiple times.
    static void initialize();

    /// @brief Do nothing.  Provided for symmetry with `initialize`.
    static void shutdown();

    /// @brief Replace the clocks and the timer with the specified callbacks
    ///        for as long as the returned guard lives.
    ///
    /// The behavior is undefined if another guard is alive.
    ///
    /// @param realTimeClockCb  The real-time clock.
    /// @param monotonicClockCb The monotonic clock.
    /// @param highResTimeCb    The high resolution timer.
    /// @param allocator        The allocator to use, or the default
    ///                         allocator if not specified.
    ///
    /// @return The guard keeping the callbacks installed.
    static bslma::ManagedPtr<MockTimeGuard>
    mockTime(const SystemTimeCb&         realTimeClockCb,
             const SystemTimeCb&         monotonicClockCb,
             const HighResolutionTimeCb& highResTimeCb,
             bslma::Allocator*           allocator = 0);

    /// @brief Return the current time of the real-time clock.
    static bsls::TimeInterval nowRealtimeClock();

    /// @brief Return the current time of the monotonic clock.
    static bsls::TimeInterval nowMonotonicClock();

    /// @brief Return the current value of the high resolution timer.
    static bsls::Types::Int64 highResolutionTimer();
};

// =========================
// class Time::MockTimeGuard
// =========================

/// @brief Install mock clocks and timer into `Time` for the lifetime of this
///        object.
///
/// At most one guard may be alive at a time.  The guard must outlive any
/// concurrent call to `Time`.
class Time::MockTimeGuard {
  private:
    // DATA
    SystemTimeCb         d_realTimeClock;
    SystemTimeCb         d_monotonicClock;
    HighResolutionTimeCb d_highResTimer;

    // FRIENDS
    friend struct Time;

  private:
    // NOT IMPLEMENTED
    MockTimeGuard(const MockTimeGuard&) BSLS_KEYWORD_DELETED;
    MockTimeGuard& operator=(const MockTimeGuard&) BSLS_KEYWORD_DELETED;

  public:
    // TRAITS
    BSLMF_NESTED_TRAIT_DECLARATION(MockTimeGuard, bslma::UsesBslmaAllocator)

    // CREATORS

    /// @brief Install the specified callbacks into `Time`.
    ///
    /// @param realTimeClockCb  The real-time clock.
    /// @param monotonicClockCb The monotonic clock.
    /// @param highResTimeCb    The high resolution timer.
    /// @param allocator        The allocator to use.
    MockTimeGuard(const SystemTimeCb&         realTimeClockCb,
                  const SystemTimeCb&         monotonicClockCb,
                  const HighResolutionTimeCb& highResTimeCb,
                  bslma::Allocator*           allocator);

    /// @brief Uninstall the callbacks from `Time`.
    ~MockTimeGuard();
};

}  // close package namespace
}  // close enterprise namespace

#endif
