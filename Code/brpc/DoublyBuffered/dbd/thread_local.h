#ifndef BUTIL_THREAD_LOCAL_H
#define BUTIL_THREAD_LOCAL_H

namespace butil {

const size_t ERROR_BUFSIZE = 64;
__thread char tls_error_buf[ERROR_BUFSIZE];

// Delete the typed-T object whose address is `arg'. This is a common function
// to thread_atexit.
template <typename T> void delete_object(void* arg) {
    delete static_cast<T*>(arg);
}

}  // namespace butil

#endif  // BUTIL_THREAD_LOCAL_H
