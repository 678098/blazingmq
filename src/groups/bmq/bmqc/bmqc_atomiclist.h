// Copyright 2018-2023 Bloomberg Finance L.P.
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

// bmqc_atomiclist.h                                                  -*-C++-*-
#ifndef INCLUDED_BMQC_ATOMICLIST
#define INCLUDED_BMQC_ATOMICLIST

//@PURPOSE: Provide a hybrid of static and dynamic array.
//
//@CLASSES:
//  bmqc::ArraySpan: Non-owning view over an array.
//  bmqc::Array: Hybrid of static and dynamic array.
//
//@SEE_ALSO: bsl::vector
//
//@DESCRIPTION: 'bmqc::Array' provides a container which can be seen as a
// hybrid of static and dynamic array (aka, 'vector').  User can declare length
// of the 'static part' of the array at compile time, and array can grow
// dynamically at runtime if required.  An advantage of this hybrid approach is
// that if user has a good intuition about the maximum number of elements that
// this container will hold, user can provide an appropriate value for the
// 'static part' of the array at compile-time, thereby preventing any runtime
// memory allocation in the container.  Note that this optimization comes at
// the cost of additional memory, which is directly proportional to the value
// of static length provided at compile-time.
//
/// Exception Safety
///----------------
// At this time, this component provides *no* exception safety guarantee.  In
// other words, this component is *not* exception neutral.  If any exception is
// thrown during the invocation of a method on the object, the object is left
// in an inconsistent state, and using the object from that point forward will
// cause undefined behavior.
//
/// TBD:
///----
//: o Documentation
//: o Tests (including perf/bench)

// BDE
#include <bsls_atomic.h>
#include <bslma_allocator.h>
#include <bslma_usesbslmaallocator.h>
#include <bslmf_nestedtraitdeclaration.h>

namespace BloombergLP {

namespace bmqc {

// ================
// class AtomicList
// ================


struct DataPlaceholder {
    int d_data;

    bsls::AtomicPointer<DataPlaceholder>  d_next_p;

    DataPlaceholder(int data)
    : d_data(data)
    , d_next_p(0)
    {
        //
    }
//todo replace with something
};

struct AtomicList {
    bsls::AtomicPointer<DataPlaceholder>  d_head;
    bsls::AtomicPointer<DataPlaceholder>  d_tail;
    bsls::AtomicInt                     d_stopGc;
    bslma::Allocator                   *d_allocator_p;

    AtomicList(bslma::Allocator *allocator)
    : d_head(new (*allocator) DataPlaceholder(0))
    , d_tail(d_head.load())
    , d_stopGc(0)
    , d_allocator_p(allocator)
    {
        
    }
    

    ~AtomicList() {
        DataPlaceholder *node = d_head.load();

        while (node) {
            DataPlaceholder *next = node->d_next_p.load();

            d_allocator_p->deleteObject(node);
            node = next;
        }
    }

    void store(int data) {
        DataPlaceholder *version = new (*d_allocator_p) DataPlaceholder(data);
        DataPlaceholder *old     = d_tail.swap(version);

        BSLS_ASSERT_SAFE(old);

        old->d_next_p.store(version);
    }

    void swap(int *previous, int data) {
        DataPlaceholder *version = new (*d_allocator_p) DataPlaceholder(data);
        DataPlaceholder *old     = d_tail.swap(version);

        BSLS_ASSERT_SAFE(old);

        *previous = old->d_data;
        old->d_next_p.store(version);
    }

    void gcHead() {
        if (d_stopGc == 0) {
            DataPlaceholder *node = d_head.load();

            if (node) {
                DataPlaceholder *next = node->d_next_p.load();
                if (next) {
                    d_allocator_p->deleteObject(node);
                    d_head.store(next);
                }
            }
        }
    }

    void load(int *out) {
        ++d_stopGc;
        DataPlaceholder *latest   = d_tail.load();

        BSLS_ASSERT_SAFE(latest);

        *out = latest->d_data;

        //--d_stopGc;
    }
};

// ============================================================================
//                             INLINE DEFINITIONS
// ============================================================================

// ----------------
// class ArraySpan
// ----------------


}  // close package namespace
}  // close enterprise namespace

#endif
