// Copyright 2019-2023 Bloomberg Finance L.P.
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

#ifndef INCLUDED_BMQTST_MOCKTIME
#define INCLUDED_BMQTST_MOCKTIME

//@PURPOSE: Provide a mock of the time accessors usable with 'bmqu::Time'.
//
//@CLASSES:
//   bmqtst::MockTime: mock utility of the 'bmqu::Time' accessors
//
//@DESCRIPTION: 'bmqtst::MockTime' provides a utility that can be used with
//'bmqu::Time' to control the time.
//
/// Usage Example
///-------------
// The following example illustrates typical intended usage of this component.
//
//..
//  MockTime mockTime;
//
//  mockTime.advanceHighResTimer(10);
//..

#include <bmqu_time.h>

// BDE
#include <bslma_allocator.h>
#include <bslma_managedptr.h>
#include <bsls_keyword.h>
#include <bsls_timeinterval.h>
#include <bsls_types.h>

namespace BloombergLP {
namespace bmqtst {

// ==============
// class MockTime
// ==============

/// A mock utility of the time accessors usable with `bmqu::Time`.
class MockTime {
  private:
    // DATA
    bsls::TimeInterval d_realtimeClock;
    bsls::TimeInterval d_monotonicClock;
    bsls::Types::Int64 d_highResTimer;

    /// Keeps this object installed into `bmqu::Time`.
    bslma::ManagedPtr<bmqu::Time::MockTimeGuard> d_guard_mp;

  private:
    // NOT IMPLEMENTED
    MockTime(const MockTime&) BSLS_KEYWORD_DELETED;
    MockTime& operator=(const MockTime&) BSLS_KEYWORD_DELETED;
    // Copy constructor and assignment operator not implemented (because
    // MockTime, for convenience, registers itself to 'bmqu::Time' in
    // its constructor).

  public:
    // CREATORS

    /// @brief Create a mock with all time counters at 0, and install it
    ///        into `bmqu::Time` for the lifetime of this object.
    ///
    /// @param allocator The allocator to use, or the default allocator if
    ///                  not specified.
    explicit MockTime(bslma::Allocator* allocator = 0);

    // MANIPULATORS
    MockTime& setRealTimeClock(const bsls::TimeInterval& value);
    MockTime& setMonotonicClock(const bsls::TimeInterval& value);

    /// Set the corresponding time to the specified `value` and return a
    /// reference offering modifiable access to this object.
    MockTime& setHighResTimer(bsls::Types::Int64 value);

    MockTime& advanceRealTimeClock(const bsls::TimeInterval& offset);
    MockTime& advanceMonotonicClock(const bsls::TimeInterval& offset);

    /// Advance (i.e., increment) the corresponding time by the specified
    /// `offset` and return a reference offering modifiable access to this
    /// object.
    MockTime& advanceHighResTimer(bsls::Types::Int64 offset);

    /// Reset all time counters to 0 and return a reference offering
    /// modifiable access to this object.
    MockTime& reset();

    // ACCESSORS
    bsls::TimeInterval realtimeClock() const;
    bsls::TimeInterval monotonicClock() const;

    /// Return the value representing the corresponding time.
    bsls::Types::Int64 highResTimer() const;
};

}  // close package namespace
}  // close enterprise namespace

#endif
