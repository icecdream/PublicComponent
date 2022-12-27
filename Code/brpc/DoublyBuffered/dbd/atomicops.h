#ifndef BUTIL_ATOMICOPS_H_
#define BUTIL_ATOMICOPS_H_

#include <stdint.h>
#include <atomic>

#include "macros.h"

namespace butil {
using ::std::memory_order;
using ::std::memory_order_relaxed;
using ::std::memory_order_consume;
using ::std::memory_order_acquire;
using ::std::memory_order_release;
using ::std::memory_order_acq_rel;
using ::std::memory_order_seq_cst;
using ::std::atomic_thread_fence;
using ::std::atomic_signal_fence;
template <typename T> class atomic : public ::std::atomic<T> {
public:
    atomic() {}
    atomic(T v) : ::std::atomic<T>(v) {}
    atomic& operator=(T v) {
        this->store(v);
        return *this;
    }
private:
    DISALLOW_COPY_AND_ASSIGN(atomic);
    // Make sure memory layout of std::atomic<T> and boost::atomic<T>
    // are same so that different compilation units seeing different 
    // definitions(enable C++11 or not) should be compatible.
    BAIDU_CASSERT(sizeof(T) == sizeof(::std::atomic<T>), size_must_match);
};
} // namespace butil

#endif  // BUTIL_ATOMICOPS_H_
