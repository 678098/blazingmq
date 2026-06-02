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

#include <mqbstat_flatjsonprinter.h>

// MQB
#include <mqbcfg_brokerconfig.h>
#include <mqbcfg_messages.h>
#include <mqbstat_brokerstats.h>
#include <mqbstat_queuestats.h>

// BMQ
#include <bmqst_statcontext.h>

// BDE
#include <bdlt_currenttime.h>
#include <bsl_cstdio.h>
#include <bsl_fstream.h>
#include <bsl_memory.h>
#include <bsl_sstream.h>
#include <bsl_list.h>
#include <bsl_string.h>
#include <bslmf_movableref.h>
#include <bsls_stopwatch.h>
#include <bsls_types.h>

// TEST DRIVER
#include <bmqtst_testhelper.h>

// CONVENIENCE
using namespace BloombergLP;
using namespace bsl;

// ============================================================================
//                                    TESTS
// ----------------------------------------------------------------------------
static void test1_printQueueWithAppIds()
// ------------------------------------------------------------------------
// PRINT QUEUE WITH APPIDS
//
// Concerns:
//   - FlatJsonPrinter correctly prints stats for a queue with appId
//     subcontexts.
//   - Output contains expected JSON fields: domain, queue, app, type,
//     and metric values.
//
// Plan:
//   - Create a domainQueues stat context hierarchy with a domain, queue,
//     and appId subcontexts.
//   - Report some events and snapshot.
//   - Call printStats and verify the output contains expected data.
//
// Testing:
//   FlatJsonPrinter::printStats
// ------------------------------------------------------------------------
{
    bmqtst::TestHelper::printTestName("printQueueWithAppIds");

    bslma::Allocator* alloc = bmqtst::TestHelperUtil::allocator();

    const int k_HISTORY_SIZE = 2;

    // Create the "domainQueues" top-level stat context
    bsl::shared_ptr<bmqst::StatContext> domainQueuesCtx =
        mqbstat::QueueStatsUtil::initializeStatContextDomains(k_HISTORY_SIZE,
                                                             alloc);

    // Add a domain subcontext
    bslma::ManagedPtr<bmqst::StatContext> domainCtx =
        domainQueuesCtx->addSubcontext(
            bmqst::StatContextConfiguration("my.domain", alloc));

    // Add a queue subcontext under the domain
    bslma::ManagedPtr<bmqst::StatContext> queueCtx =
        domainCtx->addSubcontext(
            bmqst::StatContextConfiguration("bmq://my.domain/my-queue",
                                            alloc));

    // Add appId subcontexts under the queue
    bslma::ManagedPtr<bmqst::StatContext> appFooCtx =
        queueCtx->addSubcontext(
            bmqst::StatContextConfiguration("foo", alloc));
    bslma::ManagedPtr<bmqst::StatContext> appBarCtx =
        queueCtx->addSubcontext(
            bmqst::StatContextConfiguration("bar", alloc));

    // Report some stats on the queue context
    queueCtx->adjustValue(mqbstat::DomainQueueStats::e_STAT_PUT, 1024);
    queueCtx->adjustValue(mqbstat::DomainQueueStats::e_STAT_PUSH, 512);
    queueCtx->adjustValue(mqbstat::DomainQueueStats::e_STAT_ACK, 1);
    queueCtx->adjustValue(mqbstat::DomainQueueStats::e_STAT_MESSAGES, 1);
    queueCtx->adjustValue(mqbstat::DomainQueueStats::e_STAT_BYTES, 100);
    queueCtx->setValue(mqbstat::DomainQueueStats::e_STAT_NB_PRODUCER, 2);
    queueCtx->setValue(mqbstat::DomainQueueStats::e_STAT_NB_CONSUMER, 3);

    // Report stats on appId subcontexts
    appFooCtx->adjustValue(mqbstat::DomainQueueStats::e_STAT_CONFIRM, 5);
    appBarCtx->adjustValue(mqbstat::DomainQueueStats::e_STAT_CONFIRM, 7);

    // Snapshot
    domainQueuesCtx->snapshot();

    // Build the contexts map
    mqbstat::FlatJsonPrinter::StatContextsMap ctxMap(alloc);
    ctxMap.insert(
        bsl::make_pair(bsl::string("domainQueues", alloc),
                       domainQueuesCtx.get()));

    // Create the printer
    mqbstat::FlatJsonPrinter printer(ctxMap, alloc);

    // Print stats
    bsl::ostringstream os(alloc);
    printer.printStats(os, 42);

    const bsl::string output = os.str();

    // Verify we get 3 lines: one for the queue, one for each appId
    int lineCount = 0;
    for (bsl::string::size_type i = 0; i < output.size(); ++i) {
        if (output[i] == '\n') {
            ++lineCount;
        }
    }
    BMQTST_ASSERT_EQ(lineCount, 3);

    // Verify the queue line
    BMQTST_ASSERT(output.find("\"type\":\"queue\"") != bsl::string::npos);
    BMQTST_ASSERT(output.find("\"domain\":\"my.domain\"") !=
                  bsl::string::npos);
    BMQTST_ASSERT(output.find("\"queue\":\"my-queue\"") !=
                  bsl::string::npos);

    // Verify appId lines
    BMQTST_ASSERT(output.find("\"type\":\"queue_app\"") !=
                  bsl::string::npos);
    BMQTST_ASSERT(output.find("\"app\":\"foo\"") != bsl::string::npos);
    BMQTST_ASSERT(output.find("\"app\":\"bar\"") != bsl::string::npos);

    // Verify stat_id
    BMQTST_ASSERT(output.find("\"stat_id\":42") != bsl::string::npos);

    // Verify some metric values from the queue line
    BMQTST_ASSERT(output.find("\"queue_producers_count\":\"2\"") !=
                  bsl::string::npos);
    BMQTST_ASSERT(output.find("\"queue_consumers_count\":\"3\"") !=
                  bsl::string::npos);
    BMQTST_ASSERT(output.find("\"queue_put_bytes_abs\":\"1024\"") !=
                  bsl::string::npos);
    BMQTST_ASSERT(output.find("\"queue_push_bytes_abs\":\"512\"") !=
                  bsl::string::npos);
    BMQTST_ASSERT(output.find("\"queue_ack_msgs_abs\":\"1\"") !=
                  bsl::string::npos);

    // Verify each JSON line starts with {"ts":
    BMQTST_ASSERT(output.find("{\"ts\":\"") == 0);
}

static void testN1_performanceTest()
// ------------------------------------------------------------------------
// PERFORMANCE TEST
//
// Concerns:
//   - FlatJsonPrinter can handle a large number of queues without
//     excessive runtime.
//
// Plan:
//   - Create a domainQueues stat context with many queues spread across
//     multiple domains.
//   - Snapshot and call printStats, directing output to a temp file.
//   - Measure elapsed time and report it.
//
// Testing:
//   FlatJsonPrinter::printStats performance
// ------------------------------------------------------------------------
{
    bmqtst::TestHelper::printTestName("performanceTest");

    bslma::Allocator* alloc = bmqtst::TestHelperUtil::allocator();

    const int k_HISTORY_SIZE  = 2;
    const int k_NUM_QUEUES    = 30000;
    const int k_NUM_DOMAINS   = 10;
    const int k_QUEUES_PER_DOMAIN = k_NUM_QUEUES / k_NUM_DOMAINS;

    // Create the "domainQueues" top-level stat context
    bsl::shared_ptr<bmqst::StatContext> domainQueuesCtx =
        mqbstat::QueueStatsUtil::initializeStatContextDomains(k_HISTORY_SIZE,
                                                             alloc);

    // Store managed pointers to keep subcontexts alive (list because
    // ManagedPtr is move-only, cannot be stored in vector under C++03)
    bsl::list<bslma::ManagedPtr<bmqst::StatContext> > domainContexts(alloc);
    bsl::list<bslma::ManagedPtr<bmqst::StatContext> > queueContexts(alloc);

    for (int d = 0; d < k_NUM_DOMAINS; ++d) {
        bsl::ostringstream domainName(alloc);
        domainName << "domain" << d << ".test.svc";

        bslma::ManagedPtr<bmqst::StatContext> domainCtx =
            domainQueuesCtx->addSubcontext(
                bmqst::StatContextConfiguration(domainName.str(), alloc));

        for (int q = 0; q < k_QUEUES_PER_DOMAIN; ++q) {
            bsl::ostringstream queueUri(alloc);
            queueUri << "bmq://" << domainName.str() << "/queue-" << q;

            bslma::ManagedPtr<bmqst::StatContext> queueCtx =
                domainCtx->addSubcontext(
                    bmqst::StatContextConfiguration(queueUri.str(), alloc));

            // Set some values
            queueCtx->adjustValue(mqbstat::DomainQueueStats::e_STAT_PUT, 100);
            queueCtx->adjustValue(mqbstat::DomainQueueStats::e_STAT_PUSH, 50);
            queueCtx->adjustValue(mqbstat::DomainQueueStats::e_STAT_ACK, 1);
            queueCtx->setValue(mqbstat::DomainQueueStats::e_STAT_NB_PRODUCER,
                               1);
            queueCtx->setValue(mqbstat::DomainQueueStats::e_STAT_NB_CONSUMER,
                               2);

            queueContexts.emplace_back(
                bslmf::MovableRefUtil::move(queueCtx));
        }

        domainContexts.emplace_back(
            bslmf::MovableRefUtil::move(domainCtx));
    }

    // Snapshot
    domainQueuesCtx->snapshot();

    // Build the contexts map
    mqbstat::FlatJsonPrinter::StatContextsMap ctxMap(alloc);
    ctxMap.insert(
        bsl::make_pair(bsl::string("domainQueues", alloc),
                       domainQueuesCtx.get()));

    // Create the printer
    mqbstat::FlatJsonPrinter printer(ctxMap, alloc);

    // Print to a temp file
    const char* tmpFile = "/tmp/mqbstat_flatjsonprinter_perf.json";
    bsl::ofstream ofs(tmpFile);
    BMQTST_ASSERT(ofs.is_open());

    bsls::Stopwatch timer;
    timer.start();

    printer.printStats(ofs, -1);

    timer.stop();
    ofs.close();

    const double elapsedSec = timer.elapsedTime();
    cout << "Printed " << k_NUM_QUEUES << " queues in " << elapsedSec
         << " seconds" << endl;

    // Verify the file is non-empty and has the expected number of lines
    bsl::ifstream ifs(tmpFile);
    BMQTST_ASSERT(ifs.is_open());

    int lineCount = 0;
    bsl::string line(alloc);
    while (bsl::getline(ifs, line)) {
        ++lineCount;
    }
    ifs.close();

    BMQTST_ASSERT_EQ(lineCount, k_NUM_QUEUES);

    // Clean up tmp file
    bsl::remove(tmpFile);
}

// ============================================================================
//                                 MAIN PROGRAM
// ----------------------------------------------------------------------------

int main(int argc, char* argv[])
{
    TEST_PROLOG(bmqtst::TestHelper::e_DEFAULT);

    {
        mqbcfg::AppConfig brokerConfig(bmqtst::TestHelperUtil::allocator());
        mqbcfg::BrokerConfig::set(brokerConfig);

        bsl::shared_ptr<bmqst::StatContext> statContext =
            mqbstat::BrokerStatsUtil::initializeStatContext(
                30,
                bmqtst::TestHelperUtil::allocator());

        switch (_testCase) {
        case 0:
        case 1: test1_printQueueWithAppIds(); break;
        case -1: testN1_performanceTest(); break;
        default: {
            cerr << "WARNING: CASE '" << _testCase << "' NOT FOUND." << endl;
            bmqtst::TestHelperUtil::testStatus() = -1;
        } break;
        }
    }

    TEST_EPILOG(bmqtst::TestHelper::e_DEFAULT);
}
