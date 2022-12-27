#ifndef BUTIL_DOUBLY_BUFFERED_MACROS_H
#define BUTIL_DOUBLY_BUFFERED_MACROS_H

namespace butil {

#undef DISALLOW_COPY_AND_ASSIGN
#undef DISALLOW_COPY_AND_ASSIGN

// A macro to disallow the copy constructor and operator= functions
// This should be used in the private: declarations for a class
#define DISALLOW_COPY_AND_ASSIGN(TypeName)                      \
    BUTIL_DELETE_FUNCTION(TypeName(const TypeName&));            \
    BUTIL_DELETE_FUNCTION(void operator=(const TypeName&))

#define BUTIL_DELETE_FUNCTION(decl) decl = delete

// C++11 supports compile-time assertion directly
#define BAIDU_CASSERT(expr, msg) static_assert(expr, #msg)

// Concatenate numbers in c/c++ macros.
#ifndef BAIDU_CONCAT
# define BAIDU_CONCAT(a, b) BAIDU_CONCAT_HELPER(a, b)
# define BAIDU_CONCAT_HELPER(a, b) a##b
#endif

# if defined(__cplusplus)
#   define BAIDU_LIKELY(expr) (__builtin_expect((bool)(expr), true))
#   define BAIDU_UNLIKELY(expr) (__builtin_expect((bool)(expr), false))
# else
#   define BAIDU_LIKELY(expr) (__builtin_expect(!!(expr), 1))
#   define BAIDU_UNLIKELY(expr) (__builtin_expect(!!(expr), 0))
# endif

}  // namespace butil

#endif  // BUTIL_DOUBLY_BUFFERED_MACROS_H
