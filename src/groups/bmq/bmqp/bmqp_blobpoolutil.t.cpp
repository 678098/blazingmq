// Copyright 2026 Bloomberg Finance L.P.
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

#include <bmqp_blobpoolutil.h>

// BMQ
#include <bmqtst_table.h>

// BDE
#include <bdlbb_blob.h>
#include <bdlbb_pooledblobbufferfactory.h>
#include <bsl_iostream.h>
#include <bsl_memory.h>
#include <bsl_vector.h>
#include <bslma_newdeleteallocator.h>
#include <bslma_testallocator.h>
#include <bsls_timeutil.h>
#include <bsls_types.h>

// TEST DRIVER
#include <bmqtst_testhelper.h>

// CONVENIENCE
using namespace BloombergLP;
using namespace bsl;

// ============================================================================
//                                 CONSTANTS
// ----------------------------------------------------------------------------

static const int k_BUFFER_SIZE = 4096;

// ============================================================================
//                            TEST HELPERS UTILITY
// ----------------------------------------------------------------------------
namespace {

/// @brief Measure the average time to get `k_BATCH_SIZE` blobs from a blob
///        pool, fill each with `k_DATA_SIZE` bytes and return them to the
///        pool.
///
/// @param results The table to append the measured results to.
template <size_t k_BATCH_SIZE, int k_DATA_SIZE>
void benchmarkBlobPool(bmqtst::Table* results)
{
    // PRECONDITIONS
    BSLS_ASSERT_OPT(results);

    const size_t k_NUM_BLOBS      = 10000000;  // 10M
    const size_t k_NUM_ITERATIONS = k_NUM_BLOBS / k_BATCH_SIZE;

    bslma::Allocator* alloc = &bslma::NewDeleteAllocator::singleton();

    bdlbb::PooledBlobBufferFactory   bufferFactory(k_BUFFER_SIZE, alloc);
    bmqp::BlobPoolUtil::BlobSpPoolSp blobPool =
        bmqp::BlobPoolUtil::createBlobPool(&bufferFactory, alloc);

    bsl::vector<bsl::shared_ptr<bdlbb::Blob> > batch(k_BATCH_SIZE, alloc);

    // Warmup
    for (size_t i = 0; i < k_BATCH_SIZE; ++i) {
        batch[i] = blobPool->getObject();
        batch[i]->setLength(k_DATA_SIZE);
    }
    for (size_t i = 0; i < k_BATCH_SIZE; ++i) {
        batch[i].reset();
    }

    const bsls::Types::Int64 begin = bsls::TimeUtil::getTimer();
    for (size_t it = 0; it < k_NUM_ITERATIONS; ++it) {
        for (size_t i = 0; i < k_BATCH_SIZE; ++i) {
            batch[i] = blobPool->getObject();
            batch[i]->setLength(k_DATA_SIZE);
        }
        for (size_t i = 0; i < k_BATCH_SIZE; ++i) {
            batch[i].reset();
        }
    }
    const bsls::Types::Int64 end = bsls::TimeUtil::getTimer();

    const bsls::Types::Uint64 numBlobs = k_NUM_ITERATIONS * k_BATCH_SIZE;

    results->column("Batch size")
        .insertValue(static_cast<bsls::Types::Uint64>(k_BATCH_SIZE));
    results->column("Data size, bytes")
        .insertValue(static_cast<bsls::Types::Uint64>(k_DATA_SIZE));
    results->column("Time per blob, ns")
        .insertValue(static_cast<bsls::Types::Uint64>(end - begin) / numBlobs);
}

}  // close unnamed namespace

// ============================================================================
//                                    TESTS
// ----------------------------------------------------------------------------

static void test1_breathingTest()
// ------------------------------------------------------------------------
// BREATHING TEST
//
// Concerns:
//   - A blob from the pool is empty and uses the specified buffer factory.
//
// Testing:
//   createBlobPool
// ------------------------------------------------------------------------
{
    bmqtst::TestHelper::printTestName("BREATHING TEST");

    bdlbb::PooledBlobBufferFactory bufferFactory(
        k_BUFFER_SIZE,
        bmqtst::TestHelperUtil::allocator());
    bmqp::BlobPoolUtil::BlobSpPoolSp blobPool =
        bmqp::BlobPoolUtil::createBlobPool(
            &bufferFactory,
            bmqtst::TestHelperUtil::allocator());
    BMQTST_ASSERT(blobPool);

    bsl::shared_ptr<bdlbb::Blob> blob = blobPool->getObject();
    BMQTST_ASSERT(blob);
    BMQTST_ASSERT_EQ(blob->length(), 0);
    BMQTST_ASSERT_EQ(blob->numBuffers(), 0);

    blob->setLength(k_BUFFER_SIZE + 1);
    BMQTST_ASSERT_EQ(blob->length(), k_BUFFER_SIZE + 1);
    BMQTST_ASSERT_EQ(blob->numDataBuffers(), 2);
    BMQTST_ASSERT_EQ(blob->buffer(0).size(), k_BUFFER_SIZE);
}

static void test2_blobIsClearedOnRelease()
// ------------------------------------------------------------------------
// BLOB IS CLEARED ON RELEASE
//
// Concerns:
//   - A blob returned to the pool is available again.
//   - A reused blob holds no data and no buffers.
//
// Testing:
//   createBlobPool
// ------------------------------------------------------------------------
{
    bmqtst::TestHelper::printTestName("BLOB IS CLEARED ON RELEASE");

    bdlbb::PooledBlobBufferFactory bufferFactory(
        k_BUFFER_SIZE,
        bmqtst::TestHelperUtil::allocator());
    bmqp::BlobPoolUtil::BlobSpPoolSp blobPool =
        bmqp::BlobPoolUtil::createBlobPool(
            &bufferFactory,
            bmqtst::TestHelperUtil::allocator());

    bsl::shared_ptr<bdlbb::Blob> blob = blobPool->getObject();
    BMQTST_ASSERT_EQ(blobPool->numAvailableObjects(),
                     blobPool->numObjects() - 1);

    blob->setLength(k_BUFFER_SIZE);
    blob.reset();
    BMQTST_ASSERT_EQ(blobPool->numAvailableObjects(), blobPool->numObjects());

    bsl::vector<bsl::shared_ptr<bdlbb::Blob> > blobs(
        bmqtst::TestHelperUtil::allocator());
    const int numObjects = blobPool->numObjects();
    for (int i = 0; i < numObjects; ++i) {
        blobs.push_back(blobPool->getObject());
        BMQTST_ASSERT_EQ(blobs.back()->length(), 0);
        BMQTST_ASSERT_EQ(blobs.back()->numBuffers(), 0);
    }
}

static void test3_allocator()
// ------------------------------------------------------------------------
// ALLOCATOR
//
// Concerns:
//   - The pool and its blobs use the specified allocator.
//   - All memory is released when the pool is destroyed.
//
// Testing:
//   createBlobPool
// ------------------------------------------------------------------------
{
    bmqtst::TestHelper::printTestName("ALLOCATOR");

    bslma::TestAllocator           alloc("pool");
    bdlbb::PooledBlobBufferFactory bufferFactory(
        k_BUFFER_SIZE,
        bmqtst::TestHelperUtil::allocator());

    {
        bmqp::BlobPoolUtil::BlobSpPoolSp blobPool =
            bmqp::BlobPoolUtil::createBlobPool(&bufferFactory, &alloc);
        BMQTST_ASSERT_GT(alloc.numBlocksInUse(), 0);

        const bsls::Types::Int64     numBlocks = alloc.numBlocksTotal();
        bsl::shared_ptr<bdlbb::Blob> blob      = blobPool->getObject();
        blob->setLength(1);
        BMQTST_ASSERT_GT(alloc.numBlocksTotal(), numBlocks);
    }

    BMQTST_ASSERT_EQ(alloc.numBlocksInUse(), 0);
}

static void testN1_benchmark()
// ------------------------------------------------------------------------
// BENCHMARK
//
// Concerns:
//   - Measure the time to get a blob from the pool, fill it with data and
//     return it to the pool, for different batch and data sizes.
// ------------------------------------------------------------------------
{
    bmqtst::TestHelper::printTestName("BENCHMARK");

    bmqtst::Table results(bmqtst::TestHelperUtil::allocator());

    benchmarkBlobPool<1, 0>(&results);
    benchmarkBlobPool<1, 1024>(&results);
    benchmarkBlobPool<1, 16384>(&results);
    benchmarkBlobPool<128, 0>(&results);
    benchmarkBlobPool<128, 1024>(&results);
    benchmarkBlobPool<128, 16384>(&results);
    benchmarkBlobPool<4096, 0>(&results);
    benchmarkBlobPool<4096, 1024>(&results);
    benchmarkBlobPool<4096, 16384>(&results);

    results.print(bsl::cout);
}

// ============================================================================
//                                 MAIN PROGRAM
// ----------------------------------------------------------------------------

int main(int argc, char* argv[])
{
    TEST_PROLOG(bmqtst::TestHelper::e_DEFAULT);

    switch (_testCase) {
    case 0:
    case 3: test3_allocator(); break;
    case 2: test2_blobIsClearedOnRelease(); break;
    case 1: test1_breathingTest(); break;
    case -1: testN1_benchmark(); break;
    default: {
        cerr << "WARNING: CASE '" << _testCase << "' NOT FOUND." << "\n";
        bmqtst::TestHelperUtil::testStatus() = -1;
    } break;
    }

    TEST_EPILOG(bmqtst::TestHelper::e_CHECK_DEF_GBL_ALLOC);
}
