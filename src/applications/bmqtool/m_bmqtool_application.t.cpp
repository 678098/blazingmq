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

#include <m_bmqtool_application.h>

// BMQTOOL
#include <m_bmqtool_filelogger.h>
#include <m_bmqtool_parameters.h>
#include <m_bmqtool_poster.h>

// BMQ
#include <bmqa_abstractsession.h>
#include <bmqa_confirmeventbuilder.h>
#include <bmqa_event.h>
#include <bmqa_messageevent.h>
#include <bmqa_messageproperties.h>
#include <bmqa_mocksession.h>
#include <bmqa_openqueuestatus.h>
#include <bmqa_queueid.h>
#include <bmqp_messageguidgenerator.h>
#include <bmqst_statcontext.h>
#include <bmqt_correlationid.h>
#include <bmqt_messageguid.h>
#include <bmqt_queueflags.h>
#include <bmqt_sessionoptions.h>
#include <bmqt_uri.h>

// BDE
#include <bdlbb_blob.h>
#include <bdlbb_blobutil.h>
#include <bdlbb_pooledblobbufferfactory.h>
#include <bsl_iomanip.h>
#include <bsl_iostream.h>
#include <bsl_memory.h>
#include <bsl_string.h>
#include <bsl_string_view.h>
#include <bsl_vector.h>
#include <bslma_allocator.h>
#include <bslma_defaultallocatorguard.h>
#include <bslma_managedptr.h>
#include <bslma_newdeleteallocator.h>
#include <bslmt_semaphore.h>
#include <bsls_timeutil.h>
#include <bsls_types.h>

// TEST DRIVER
#include <bmqtst_testhelper.h>

// CONVENIENCE
using namespace BloombergLP;
using namespace bsl;

// ============================================================================
//                            TEST HELPERS UTILITY
// ----------------------------------------------------------------------------
namespace {

const bsl::string_view k_URI = "bmq://bmq.test.mem.priority/bench";

/// Shape and size of one benchmark run.
struct Config {
    int d_msgSize;
    int d_eventSize;
    int d_numMessages;
};

const Config k_BENCH_CONFIGS[] = {
    {64, 1, 1000000},
    {64, 100, 1000000},
    {1024, 1, 1000000},
    {1024, 100, 1000000},
    {65536, 1, 50000},
    {65536, 16, 50000},
};

/// Mock session that accepts and drops every posted event and confirm, so
/// that only the work done by `bmqtool` and the SDK event builders is
/// measured.
class BenchSession : public bmqa::MockSession {
  private:
    // DATA
    bsls::Types::Int64 d_numPostedEvents;
    bsls::Types::Int64 d_numConfirmedMessages;

  public:
    // CREATORS

    /// @brief Create a session using the specified `allocator`.
    explicit BenchSession(bslma::Allocator* allocator)
    : bmqa::MockSession(bmqt::SessionOptions(allocator), allocator)
    , d_numPostedEvents(0)
    , d_numConfirmedMessages(0)
    {
    }

    // MANIPULATORS

    /// @brief Count and drop the posted event.
    ///
    /// @return Always 0.
    int post(const bmqa::MessageEvent&) BSLS_KEYWORD_OVERRIDE
    {
        ++d_numPostedEvents;
        return 0;
    }

    /// @brief Count and drop the confirms held by the specified `builder`,
    ///        then reset the `builder`.
    ///
    /// @return Always 0.
    int
    confirmMessages(bmqa::ConfirmEventBuilder* builder) BSLS_KEYWORD_OVERRIDE
    {
        BSLS_ASSERT_SAFE(builder);

        d_numConfirmedMessages += builder->messageCount();
        builder->reset();
        return 0;
    }

    /// @brief Open the benchmark queue.
    ///
    /// @param[out] queueId The opened queue.
    /// @param      flags   The queue flags to open the queue with.
    void openBenchQueue(bmqa::QueueId* queueId, bsls::Types::Uint64 flags)
    {
        BSLS_ASSERT_SAFE(queueId);

        const bsl::string uriString(k_URI);
        const bmqt::Uri   uri(uriString);

        BMQA_EXPECT_CALL(*this, openQueueSync(queueId, uri, flags))
            .returning(bmqa::MockSessionUtil::createOpenQueueStatus(
                *queueId,
                bmqt::OpenQueueResult::e_SUCCESS,
                ""));

        BMQTST_ASSERT(openQueueSync(queueId, uri, flags));
    }

    // ACCESSORS

    /// @return The number of events posted so far.
    bsls::Types::Int64 numPostedEvents() const { return d_numPostedEvents; }

    /// @return The number of messages confirmed so far.
    bsls::Types::Int64 numConfirmedMessages() const
    {
        return d_numConfirmedMessages;
    }
};

/// @brief Create a stat context with the layout `bmqtool` expects.
///
/// @param allocator The allocator to supply memory.
///
/// @return The created stat context.
bsl::shared_ptr<bmqst::StatContext>
createStatContext(bslma::Allocator* allocator)
{
    bmqst::StatContextConfiguration config("bmqtool", allocator);
    config.isTable(true);
    config.value("message", 10)
        .value("event", 10)
        .value("latency", bmqst::StatValue::e_DISCRETE, 10);
    return bsl::allocate_shared<bmqst::StatContext>(allocator, config);
}

/// @brief Post messages shaped by the specified `config` through the
///        `bmqtool` producer.
///
/// @param config    The shape and size of the run.
/// @param allocator The allocator to supply memory.
///
/// @return The time spent posting, in nanoseconds.
bsls::Types::Int64 runProducer(const Config&     config,
                               bslma::Allocator* allocator)
{
    BenchSession  session(allocator);
    bmqa::QueueId queueId(bmqt::CorrelationId(1), allocator);
    session.openBenchQueue(&queueId, bmqt::QueueFlags::e_WRITE);

    const int numEvents = config.d_numMessages / config.d_eventSize;

    m_bmqtool::Parameters parameters(allocator);
    parameters.setMode(m_bmqtool::ParametersMode::e_AUTO)
        .setQueueFlags(bmqt::QueueFlags::e_WRITE)
        .setLatency(m_bmqtool::ParametersLatency::e_NONE)
        .setVerbosity(m_bmqtool::ParametersVerbosity::e_SILENT)
        .setMsgSize(config.d_msgSize)
        .setEventSize(config.d_eventSize)
        .setEventsCount(numEvents)
        .setPostRate(1);

    m_bmqtool::FileLogger               fileLogger("", allocator);
    bsl::shared_ptr<bmqst::StatContext> statContext = createStatContext(
        allocator);
    m_bmqtool::Poster poster(&fileLogger, statContext.get(), allocator);
    bsl::shared_ptr<m_bmqtool::PostingContext> postingContext =
        poster.createPostingContext(&session, parameters, queueId);

    const bsls::Types::Int64 start = bsls::TimeUtil::getTimer();
    while (postingContext->pendingPost()) {
        postingContext->postNext();
    }
    const bsls::Types::Int64 elapsed = bsls::TimeUtil::getTimer() - start;

    BMQTST_ASSERT_EQ(session.numPostedEvents(), numEvents);

    return elapsed;
}

/// @brief Deliver PUSH messages shaped by the specified `config` to the
///        `bmqtool` consumer, which confirms each of them.
///
/// @param config    The shape and size of the run.
/// @param allocator The allocator to supply memory.
///
/// @return The time spent consuming, in nanoseconds.
bsls::Types::Int64 runConsumer(const Config&     config,
                               bslma::Allocator* allocator)
{
    BenchSession  session(allocator);
    bmqa::QueueId queueId(bmqt::CorrelationId(1), allocator);
    session.openBenchQueue(&queueId, bmqt::QueueFlags::e_READ);

    bdlbb::PooledBlobBufferFactory bufferFactory(4096, allocator);
    bdlbb::Blob                    payload(&bufferFactory, allocator);
    const bsl::string              data(config.d_msgSize, 'A', allocator);
    bdlbb::BlobUtil::append(&payload, data.data(), config.d_msgSize);

    bmqp::MessageGUIDGenerator guidGenerator(0, false);
    bmqa::MessageProperties    properties(allocator);
    bsl::vector<bmqa::MockSessionUtil::PushMessageParams> messages(allocator);
    for (int i = 0; i < config.d_eventSize; ++i) {
        bmqt::MessageGUID guid;
        guidGenerator.generateGUID(&guid);
        messages.emplace_back(payload, queueId, guid, properties);
    }
    const bmqa::MessageEvent event = bmqa::MockSessionUtil::createPushEvent(
                                         messages,
                                         &bufferFactory,
                                         allocator)
                                         .messageEvent();

    m_bmqtool::Parameters parameters(allocator);
    parameters.setMode(m_bmqtool::ParametersMode::e_AUTO)
        .setQueueFlags(bmqt::QueueFlags::e_READ)
        .setLatency(m_bmqtool::ParametersLatency::e_NONE)
        .setVerbosity(m_bmqtool::ParametersVerbosity::e_SILENT)
        .setConfirmMsg(true)
        .setShutdownGrace(0);

    bslmt::Semaphore       shutdownSemaphore;
    m_bmqtool::Application app(parameters, &shutdownSemaphore, allocator);

    bslma::ManagedPtr<bmqa::AbstractSession> sessionMp(
        &session,
        0,
        bslma::ManagedPtrUtil::noOpDeleter);
    app.setSession(sessionMp);

    bmqa::SessionEventHandler& handler = app;
    const int numEvents = config.d_numMessages / config.d_eventSize;

    const bsls::Types::Int64 start = bsls::TimeUtil::getTimer();
    for (int i = 0; i < numEvents; ++i) {
        handler.onMessageEvent(event);
    }
    const bsls::Types::Int64 elapsed = bsls::TimeUtil::getTimer() - start;

    BMQTST_ASSERT_EQ(session.numConfirmedMessages(),
                     static_cast<bsls::Types::Int64>(numEvents) *
                         config.d_eventSize);

    return elapsed;
}

/// @brief Print one row of benchmark results.
///
/// @param config    The shape and size of the run.
/// @param elapsedNs The time spent, in nanoseconds.
void printResult(const Config& config, bsls::Types::Int64 elapsedNs)
{
    const double seconds    = static_cast<double>(elapsedNs) / 1e9;
    const double msgsPerSec = config.d_numMessages / seconds;
    const double mbPerSec   = msgsPerSec * config.d_msgSize / (1024 * 1024);
    const double nsPerMsg   = static_cast<double>(elapsedNs) /
                              config.d_numMessages;

    cout << setw(8) << config.d_msgSize << setw(8) << config.d_eventSize
         << setw(10) << config.d_numMessages << fixed << setprecision(0)
         << setw(14) << msgsPerSec << setprecision(1) << setw(12) << mbPerSec
         << setw(10) << nsPerMsg << "\n";
}

/// @brief Run the specified `benchmark` for each benchmark config and print
///        the results.
void runAll(bsls::Types::Int64 (*benchmark)(const Config&, bslma::Allocator*))
{
    // Avoid measuring the bookkeeping of the test allocators.
    bslma::Allocator* allocator = bslma::NewDeleteAllocator::allocator(0);
    bslma::DefaultAllocatorGuard guard(allocator);
    bmqtst::TestHelperUtil::ignoreCheckGblAlloc() = true;

    cout << setw(8) << "msgSize" << setw(8) << "evtSize" << setw(10) << "msgs"
         << setw(14) << "msgs/s" << setw(12) << "MB/s" << setw(10) << "ns/msg"
         << "\n";

    const int numConfigs = sizeof(k_BENCH_CONFIGS) /
                           sizeof(k_BENCH_CONFIGS[0]);
    for (int i = 0; i < numConfigs; ++i) {
        printResult(k_BENCH_CONFIGS[i],
                    benchmark(k_BENCH_CONFIGS[i], allocator));
    }
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
//   - The producer posts every event and the consumer confirms every
//     message when running without a broker.
//
// Testing:
//   PostingContext::postNext
//   Application::setSession
//   Application::onMessageEvent
// ------------------------------------------------------------------------
{
    bmqtst::TestHelper::printTestName("BREATHING TEST");

    // Parameters and the SDK allocate temporaries with the default allocator.
    bmqtst::TestHelperUtil::ignoreCheckDefAlloc() = true;

    const Config config = {64, 10, 100};

    runProducer(config, bmqtst::TestHelperUtil::allocator());
    runConsumer(config, bmqtst::TestHelperUtil::allocator());
}

static void testN1_producerBenchmark()
// ------------------------------------------------------------------------
// PRODUCER BENCHMARK
//
// Concerns:
//   - Measure the highest rate at which `bmqtool` can build and post
//     messages, without a broker.
//
// Plan:
//   - Post messages through `PostingContext` to a session that drops
//     every posted event, and print the rate for each config.
// ------------------------------------------------------------------------
{
    bmqtst::TestHelper::printTestName("PRODUCER BENCHMARK");

    runAll(&runProducer);
}

static void testN2_consumerBenchmark()
// ------------------------------------------------------------------------
// CONSUMER BENCHMARK
//
// Concerns:
//   - Measure the highest rate at which `bmqtool` can process and confirm
//     PUSH messages, without a broker.
//
// Plan:
//   - Build one PUSH event, deliver it repeatedly to
//     `Application::onMessageEvent` with a session that drops every
//     confirm, and print the rate for each config.
// ------------------------------------------------------------------------
{
    bmqtst::TestHelper::printTestName("CONSUMER BENCHMARK");

    runAll(&runConsumer);
}

// ============================================================================
//                                 MAIN PROGRAM
// ----------------------------------------------------------------------------

int main(int argc, char* argv[])
{
    TEST_PROLOG(bmqtst::TestHelper::e_DEFAULT);

    switch (_testCase) {
    case 0:
    case 1: test1_breathingTest(); break;
    case -1: testN1_producerBenchmark(); break;
    case -2: testN2_consumerBenchmark(); break;
    default: {
        cerr << "WARNING: CASE '" << _testCase << "' NOT FOUND." << "\n";
        bmqtst::TestHelperUtil::testStatus() = -1;
    } break;
    }

    TEST_EPILOG(bmqtst::TestHelper::e_CHECK_GBL_ALLOC);
}
