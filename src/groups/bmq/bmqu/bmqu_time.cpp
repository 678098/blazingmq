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

#include <bmqu_time.h>

#include <bmqscm_version.h>
// BDE
#include <bslma_default.h>
#include <bslmf_allocatorargt.h>
#include <bslmt_once.h>
#include <bsls_assert.h>
#include <bsls_atomicoperations.h>
#include <bsls_performancehint.h>
#include <bsls_systemtime.h>
#include <bsls_timeutil.h>

namespace BloombergLP {
namespace bmqu {

namespace {

/// The currently alive `Time::MockTimeGuard`, if any.  A POD type so that it
/// is zero-initialized at compile time, with no constructor to run.
bsls::AtomicOperations::AtomicTypes::Pointer g_mock;

/// @brief Return the currently alive `Time::MockTimeGuard`, if any.
///
/// Also initialize `bsls::TimeUtil` on the first call.
const Time::MockTimeGuard* loadMock()
{
    BSLMT_ONCE_DO
    {
        bsls::TimeUtil::initialize();
    }

    return static_cast<const Time::MockTimeGuard*>(
        bsls::AtomicOperations::getPtrAcquire(&g_mock));
}

}  // close unnamed namespace

// -----------
// struct Time
// -----------

void Time::initialize()
{
    bsls::TimeUtil::initialize();
}

void Time::shutdown()
{
}

bslma::ManagedPtr<Time::MockTimeGuard>
Time::mockTime(const SystemTimeCb&         realTimeClockCb,
               const SystemTimeCb&         monotonicClockCb,
               const HighResolutionTimeCb& highResTimeCb,
               bslma::Allocator*           allocator)
{
    return bslma::ManagedPtrUtil::allocateManaged<MockTimeGuard>(
        bslma::Default::allocator(allocator),
        realTimeClockCb,
        monotonicClockCb,
        highResTimeCb);
}

bsls::TimeInterval Time::nowRealtimeClock()
{
    const MockTimeGuard* mock = loadMock();
    if (BSLS_PERFORMANCEHINT_PREDICT_UNLIKELY(mock)) {
        BSLS_PERFORMANCEHINT_UNLIKELY_HINT;
        return mock->d_realTimeClock();  // RETURN
    }

    return bsls::SystemTime::nowRealtimeClock();
}

bsls::TimeInterval Time::nowMonotonicClock()
{
    const MockTimeGuard* mock = loadMock();
    if (BSLS_PERFORMANCEHINT_PREDICT_UNLIKELY(mock)) {
        BSLS_PERFORMANCEHINT_UNLIKELY_HINT;
        return mock->d_monotonicClock();  // RETURN
    }

    return bsls::SystemTime::nowMonotonicClock();
}

bsls::Types::Int64 Time::highResolutionTimer()
{
    const MockTimeGuard* mock = loadMock();
    if (BSLS_PERFORMANCEHINT_PREDICT_UNLIKELY(mock)) {
        BSLS_PERFORMANCEHINT_UNLIKELY_HINT;
        return mock->d_highResTimer();  // RETURN
    }

    return bsls::TimeUtil::getTimer();
}

// -------------------------
// class Time::MockTimeGuard
// -------------------------

Time::MockTimeGuard::MockTimeGuard(const SystemTimeCb& realTimeClockCb,
                                   const SystemTimeCb& monotonicClockCb,
                                   const HighResolutionTimeCb& highResTimeCb,
                                   bslma::Allocator*           allocator)
: d_realTimeClock(bsl::allocator_arg, allocator, realTimeClockCb)
, d_monotonicClock(bsl::allocator_arg, allocator, monotonicClockCb)
, d_highResTimer(bsl::allocator_arg, allocator, highResTimeCb)
{
    const void* previous = bsls::AtomicOperations::testAndSwapPtr(&g_mock,
                                                                  0,
                                                                  this);
    BSLS_ASSERT_OPT(!previous && "Another MockTimeGuard is alive");
    (void)previous;
}

Time::MockTimeGuard::~MockTimeGuard()
{
    const void* previous = bsls::AtomicOperations::testAndSwapPtr(&g_mock,
                                                                  this,
                                                                  0);
    BSLS_ASSERT_OPT(previous == this);
    (void)previous;
}

}  // close package namespace
}  // close enterprise namespace
