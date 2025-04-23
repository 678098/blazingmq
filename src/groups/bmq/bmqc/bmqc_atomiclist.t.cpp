// Copyright 2025 Bloomberg Finance L.P.
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

// bmqc_atomiclist.t.cpp                                              -*-C++-*-
#include <bmqc_atomiclist.h>

// BDE
#include <bsl_algorithm.h>
#include <bsl_cstdio.h>
#include <bsl_cstdlib.h>
#include <bsl_string.h>
#include <bsl_vector.h>
#include <bslma_default.h>
#include <bslma_testallocator.h>
#include <bslmf_if.h>
#include <bslmf_isconst.h>

#include <bsls_platform.h>

// TEST DRIVER
#include <bmqtst_testhelper.h>

#ifdef BSLS_PLATFORM_OS_LINUX
// BENCHMARK
#include <benchmark/benchmark.h>
#include <chrono>
#include <cmath>
#include <iostream>
#include <vector>
#endif  // BSLS_PLATFORM_OS_LINUX

// CONVENIENCE
using namespace BloombergLP;
using namespace bsl;

// ============================================================================
//                            TEST HELPERS UTILITY
// ----------------------------------------------------------------------------

namespace {

}  // close unnamed namespace

// ============================================================================
//                                    TESTS
// ----------------------------------------------------------------------------

static void test1_breathingTest()
// ------------------------------------------------------------------------
// BREATHING TEST
//
// Concerns:
//   Exercise the basic functionality of the component.
//
// Testing:
//   Basic functionality
// ------------------------------------------------------------------------
{
    bmqtst::TestHelper::printTestName("BREATHING TEST");

    bmqc::AtomicList list(bmqtst::TestHelperUtil::allocator());

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
    default: {
        cerr << "WARNING: CASE '" << _testCase << "' NOT FOUND." << endl;
        bmqtst::TestHelperUtil::testStatus() = -1;
    } break;
    }
    TEST_EPILOG(bmqtst::TestHelper::e_CHECK_DEF_GBL_ALLOC);
}
