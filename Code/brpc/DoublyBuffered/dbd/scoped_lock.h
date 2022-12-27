#ifndef BUTIL_BAIDU_SCOPED_LOCK_H
#define BUTIL_BAIDU_SCOPED_LOCK_H

#include <mutex>                           // std::lock_guard
#include "thread_local.h"

namespace butil {
namespace detail {
template <typename T>
std::lock_guard<typename std::remove_reference<T>::type> get_lock_guard();
}  // namespace detail
}  // namespace butil

#define BAIDU_SCOPED_LOCK(ref_of_lock)                                  \
    decltype(::butil::detail::get_lock_guard<decltype(ref_of_lock)>()) \
    BAIDU_CONCAT(scoped_locker_dummy_at_line_, __LINE__)(ref_of_lock)

namespace std {

template<> class lock_guard<pthread_mutex_t> {
public:
    explicit lock_guard(pthread_mutex_t & mutex) : _pmutex(&mutex) {
#if !defined(NDEBUG)
        const int rc = pthread_mutex_lock(_pmutex);
        if (rc) {
            std::cerr << "Fail to lock pthread_mutex_t=" << _pmutex << ", " << strerror_r(rc, butil::tls_error_buf, butil::ERROR_BUFSIZE) << std::endl;
            _pmutex = NULL;
        }
#else
        pthread_mutex_lock(_pmutex);
#endif  // NDEBUG
    }
    
    ~lock_guard() {
#ifndef NDEBUG
        if (_pmutex) {
            pthread_mutex_unlock(_pmutex);
        }
#else
        pthread_mutex_unlock(_pmutex);
#endif
    }
    
private:
    DISALLOW_COPY_AND_ASSIGN(lock_guard);
    pthread_mutex_t* _pmutex;
};

template<> class lock_guard<pthread_spinlock_t> {
public:
    explicit lock_guard(pthread_spinlock_t & spin) : _pspin(&spin) {
#if !defined(NDEBUG)
        const int rc = pthread_spin_lock(_pspin);
        if (rc) {
            std::cerr << "Fail to lock pthread_spinlock_t=" << _pspin << ", " << strerror_r(rc, butil::tls_error_buf, butil::ERROR_BUFSIZE) << std::endl;
            _pspin = NULL;
        }
#else
        pthread_spin_lock(_pspin);
#endif  // NDEBUG
    }
    
    ~lock_guard() {
#ifndef NDEBUG
        if (_pspin) {
            pthread_spin_unlock(_pspin);
        }
#else
        pthread_spin_unlock(_pspin);
#endif
    }
    
private:
    DISALLOW_COPY_AND_ASSIGN(lock_guard);
    pthread_spinlock_t* _pspin;
};

template<> class unique_lock<pthread_mutex_t> {
    DISALLOW_COPY_AND_ASSIGN(unique_lock);
public:
    typedef pthread_mutex_t         mutex_type;
    unique_lock() : _mutex(NULL), _owns_lock(false) {}
    explicit unique_lock(mutex_type& mutex)
        : _mutex(&mutex), _owns_lock(true) {
        pthread_mutex_lock(_mutex);
    }
    unique_lock(mutex_type& mutex, defer_lock_t)
        : _mutex(&mutex), _owns_lock(false)
    {}
    unique_lock(mutex_type& mutex, try_to_lock_t) 
        : _mutex(&mutex), _owns_lock(pthread_mutex_trylock(&mutex) == 0)
    {}
    unique_lock(mutex_type& mutex, adopt_lock_t) 
        : _mutex(&mutex), _owns_lock(true)
    {}

    ~unique_lock() {
        if (_owns_lock) {
            pthread_mutex_unlock(_mutex);
        }
    }

    void lock() {
        if (_owns_lock) {
            std::cerr << "Detected deadlock issue" << std::endl;
            return;
        }
#if !defined(NDEBUG)
        const int rc = pthread_mutex_lock(_mutex);
        if (rc) {
            std::cerr << "Fail to lock pthread_mutex=" << _mutex << ", " << strerror_r(rc, butil::tls_error_buf, butil::ERROR_BUFSIZE) << std::endl;
            return;
        }
        _owns_lock = true;
#else
        _owns_lock = true;
        pthread_mutex_lock(_mutex);
#endif  // NDEBUG
    }

    bool try_lock() {
        if (_owns_lock) {
            std::cerr << "Detected deadlock issue" << std::endl; 
            return false;
        }
        _owns_lock = !pthread_mutex_trylock(_mutex);
        return _owns_lock;
    }

    void unlock() {
        if (!_owns_lock) {
            std::cerr << "Invalid operation" << std::endl;
            return;
        }
        pthread_mutex_unlock(_mutex);
        _owns_lock = false;
    }

    void swap(unique_lock& rhs) {
        std::swap(_mutex, rhs._mutex);
        std::swap(_owns_lock, rhs._owns_lock);
    }

    mutex_type* release() {
        mutex_type* saved_mutex = _mutex;
        _mutex = NULL;
        _owns_lock = false;
        return saved_mutex;
    }

    mutex_type* mutex() { return _mutex; }
    bool owns_lock() const { return _owns_lock; }
    operator bool() const { return owns_lock(); }

private:
    mutex_type*                     _mutex;
    bool                            _owns_lock;
};

template<> class unique_lock<pthread_spinlock_t> {
    DISALLOW_COPY_AND_ASSIGN(unique_lock);
public:
    typedef pthread_spinlock_t  mutex_type;
    unique_lock() : _mutex(NULL), _owns_lock(false) {}
    explicit unique_lock(mutex_type& mutex)
        : _mutex(&mutex), _owns_lock(true) {
        pthread_spin_lock(_mutex);
    }

    ~unique_lock() {
        if (_owns_lock) {
            pthread_spin_unlock(_mutex);
        }
    }
    unique_lock(mutex_type& mutex, defer_lock_t)
        : _mutex(&mutex), _owns_lock(false)
    {}
    unique_lock(mutex_type& mutex, try_to_lock_t) 
        : _mutex(&mutex), _owns_lock(pthread_spin_trylock(&mutex) == 0)
    {}
    unique_lock(mutex_type& mutex, adopt_lock_t) 
        : _mutex(&mutex), _owns_lock(true)
    {}

    void lock() {
        if (_owns_lock) {
            std::cerr << "Detected deadlock issue" << std::endl;
            return;
        }
#if !defined(NDEBUG)
        const int rc = pthread_spin_lock(_mutex);
        if (rc) {
            std::cerr << "Fail to lock pthread_spinlock=" << _mutex << ", " << strerror_r(rc, butil::tls_error_buf, butil::ERROR_BUFSIZE) << std::endl;
            return;
        }
        _owns_lock = true;
#else
        _owns_lock = true;
        pthread_spin_lock(_mutex);
#endif  // NDEBUG
    }

    bool try_lock() {
        if (_owns_lock) {
            std::cerr << "Detected deadlock issue" << std::endl;
            return false;
        }
        _owns_lock = !pthread_spin_trylock(_mutex);
        return _owns_lock;
    }

    void unlock() {
        if (!_owns_lock) {
            std::cerr << "Invalid operation" << std::endl;
            return;
        }
        pthread_spin_unlock(_mutex);
        _owns_lock = false;
    }

    void swap(unique_lock& rhs) {
        std::swap(_mutex, rhs._mutex);
        std::swap(_owns_lock, rhs._owns_lock);
    }

    mutex_type* release() {
        mutex_type* saved_mutex = _mutex;
        _mutex = NULL;
        _owns_lock = false;
        return saved_mutex;
    }

    mutex_type* mutex() { return _mutex; }
    bool owns_lock() const { return _owns_lock; }
    operator bool() const { return owns_lock(); }

private:
    mutex_type*                     _mutex;
    bool                            _owns_lock;
};

}  // namespace std

#endif  // BUTIL_BAIDU_SCOPED_LOCK_H
