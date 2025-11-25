// Copyright 2014-2025 Bloomberg Finance L.P.
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

// bmqtst_benchmark.h                                                 -*-C++-*-
#ifndef INCLUDED_BMQTST_BENCHMARK
#define INCLUDED_BMQTST_BENCHMARK

//@PURPOSE: Provide generic benchmark boilerplate code for use in test drivers.
//
//@CLASSES:
//  bmqtst::BlobTestUtil: namespace for a set of blob utilities
//
//@DESCRIPTION: 'bmqtst::BlobTestUtil' provides a set of blob utilities to
// assist in writing test drivers.
//
/// Usage Example
///-------------
// This section illustrates intended use of this component.
//
/// Example 1: Blob from string
///- - - - - - - - - - - - - -
// The following code illustrates how to construct a blob from a string while
// specifying the internal breakdown of the blob buffers in the string.
//
//..
//  bdlbb::Blob blob(bmqtst::TestHelperUtil::allocator());
//  bmqtst::BlobTestUtil::fromString(&blob, "a|b",
//  bmqtst::TestHelperUtil::allocator());
//
//  BMQTST_ASSERT_EQ(blob.length(),         2);
//  BMQTST_ASSERT_EQ(blob.numDataBuffers(), 2);
//
//  // First buffer
//  bsl::string buf1(blob.buffer(0).data(), 1U);
//  BMQTST_ASSERT_EQ(blob.buffer(0).size(), 1);
//  BMQTST_ASSERT_EQ(buf1,                  "a");
//
//  // Second buffer
//  bsl::string buf2(blob.buffer(1).data(), 1U);
//  BMQTST_ASSERT_EQ(blob.buffer(1).size(), 1);
//  BMQTST_ASSERT_EQ(buf2,                  "b");
//..
//
/// Example 2: Blob to string
///- - - - - - - - - - - - -
// The following code illustrates how to convert a blob to a string.
//
// First, let's put the string 'abcdefg' into the blob.
//..
//  bdlbb::Blob blob(bmqtst::TestHelperUtil::allocator());
//  bmqtst::BlobTestUtil::fromString(&blob, "abcdefg",
//  bmqtst::TestHelperUtil::allocator()); BSLS_ASSERT_OPT(blob.length() == 7);
//  BSLS_ASSERT_OPT(blob.numDataBuffers() == 1);
//..
// Finally, we can convert the blob to a string using the 'toString' method.
//..
//  bsl::string str(bmqtst::TestHelperUtil::allocator());
//  BMQTST_ASSERT_EQ("abcdefg", bmqtst::BlobTestUtil::toString(&str, blob));
//..

// BMQ
#include <bmqtst_testhelper.h>

#include <bslmt_barrier.h>
#include <bslmt_latch.h>
#include <bslmt_threadgroup.h>

namespace BloombergLP {
namespace bmqtst {

template <class BENCHMARK_STATE_TYPE>
class GenericBenchmark {
  public:
    // PUBLIC TYPES
    typedef BENCHMARK_STATE_TYPE StateType;

    // CREATORS
    virtual ~GenericBenchmark()
    {
        // NOTHING
    }

    // MANIPULATORS
    virtual void initialize(size_t numThreads) = 0;

    virtual void bench() = 0;

    template <size_t NUM_THREADS>
    void run(StateType& state)
    {
        bslmt::Latch   initThreadLatch(NUM_THREADS);
        bslmt::Barrier startBenchmarkBarrier(NUM_THREADS + 1);
        bslmt::Latch   finishBenchmarkLatch(NUM_THREADS);

        // Prepare all resources needed for thread functions.
        initialize(NUM_THREADS);

        bslmt::ThreadGroup threadGroup(bmqtst::TestHelperUtil::allocator());
        for (size_t i = 0; i < NUM_THREADS; ++i) {
            const int rc = threadGroup.addThread(
                bdlf::BindUtil::bindS(bmqtst::TestHelperUtil::allocator(),
                                      &GenericBenchmark::threadFn,
                                      this,
                                      &initThreadLatch,
                                      &startBenchmarkBarrier,
                                      &finishBenchmarkLatch));
            BMQTST_ASSERT_EQ_D(i, rc, 0);
        }

        initThreadLatch.wait();

        size_t iter = 0;
        for (auto _ : state) {
            // Benchmark time start

            // We don't support running multi-iteration benchmarks because we
            // prepare and start complex tasks in separate threads.
            // Once these tasks are finished, we cannot simply re-run them without
            // reinitialization, and it goes against benchmark library design.
            // Make sure we run this only once.
            BSLS_ASSERT_OPT(0 == iter++ && "Must be run only once");

            startBenchmarkBarrier.wait();
            finishBenchmarkLatch.wait();

            // Benchmark time end
        }

        threadGroup.joinAll();
    }

  private:
    void threadFn(bslmt::Latch*   initLatch_p,
                  bslmt::Barrier* startBarrier_p,
                  bslmt::Latch*   finishLatch_p)
    {
        // PRECONDITIONS
        BSLS_ASSERT_OPT(initLatch_p);
        BSLS_ASSERT_OPT(startBarrier_p);
        BSLS_ASSERT_OPT(finishLatch_p);

        initLatch_p->arrive();
        startBarrier_p->wait();

        bench();

        finishLatch_p->arrive();
    }
};

}  // close package namespace
}  // close enterprise namespace

#endif
