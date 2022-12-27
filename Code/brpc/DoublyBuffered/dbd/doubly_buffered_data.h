#ifndef BUTIL_DOUBLY_BUFFERED_DATA_H
#define BUTIL_DOUBLY_BUFFERED_DATA_H

#include <vector>
#include <pthread.h>
#include <memory>

#include "macros.h"
#include "atomicops.h"
#include "scoped_lock.h"
#include "thread_local.h"
#include "type_traits.h"

namespace butil {

// This data structure makes Read() almost lock-free by making Modify()
// *much* slower. It's very suitable for implementing LoadBalancers which
// have a lot of concurrent read-only ops from many threads and occasional
// modifications of data. As a side effect, this data structure can store
// a thread-local data for user.
//
// Read(): begin with a thread-local mutex locked then read the foreground
// instance which will not be changed before the mutex is unlocked. Since the
// mutex is only locked by Modify() with an empty critical section, the
// function is almost lock-free.
//
// Modify(): Modify background instance which is not used by any Read(), flip
// foreground and background, lock thread-local mutexes one by one to make
// sure all existing Read() finish and later Read() see new foreground,
// then modify background(foreground before flip) again.

/*
    这个数据结构使 Read() 接近于无锁, 代价是 Modify() 会更慢.
    它非常适合用来实现 负载均衡, 有大量的多线程并发只读操作, 和偶尔的修改数据操作. 副作用就是, 数据结构会使用用户的 thread-local 存储数据.

    函数 Read(): 通过 thread-local 锁开启, 然后读取前台实例数据, 在锁释放前不会被修改. 锁仅在函数 Modify() 中上锁, 而且上锁后立即解锁, 所以函数 Read() 几乎是无锁的.

    函数 Modify(): 修改函数 Read() 没有使用的后台实例数据, 翻转前台和后台的数据, 一个个的对所有存在的 Read() thread-local 进行加锁, 确保函数 Read() 稍后读取看到新的前台实例, 然后再修改后台实例数据（之前的前台实例）.
*/

class Void { };

// T: 存储的数据类型, TLS: thread-local数据类型
template <typename T, typename TLS = Void>
class DoublyBufferedData {
    class Wrapper;
public:
    // 不允许拷贝的 作用域指针
    class ScopedPtr {
    friend class DoublyBufferedData;
    public:
        ScopedPtr() : _data(NULL), _w(NULL) {}
        ~ScopedPtr() {
            if (_w) {
                _w->EndRead();  // 释放锁
            }
        }
        const T* get() const { return _data; }
        const T& operator*() const { return *_data; }
        const T* operator->() const { return _data; }
        TLS& tls() { return _w->user_tls(); }   // 返回用的tls引用数据, 使用方可以修改返回的引用
        
    private:
        DISALLOW_COPY_AND_ASSIGN(ScopedPtr);
        const T* _data;
        Wrapper* _w;
    };
    
    DoublyBufferedData();
    ~DoublyBufferedData();

    // Put foreground instance into ptr. The instance will not be changed until
    // ptr is destructed.
    // This function is not blocked by Read() and Modify() in other threads.
    // Returns 0 on success, -1 otherwise.
    // 将前台实例放到ptr中, 在ptr销毁之前 实例数据不会产生变化. 其他线程的函数 Read() 和 Modify() 不会阻塞此函数
    int Read(ScopedPtr* ptr);

    // Modify background and foreground instances. fn(T&, ...) will be called
    // twice. Modify() from different threads are exclusive from each other.
    // NOTE: Call same series of fn to different equivalent instances should
    // result in equivalent instances, otherwise foreground and background
    // instance will be inconsistent.
    // 修改后台和前台实例数据. fn(T&, ...)函数会被调用两次, 不同线程的函数 Modify() 会互斥
    // 注意: 对不同的等价的实例调用一系列的 Fn, 结果应该是一样的, 否则前台和后台实例可能会发生冲突
    // 下面三个 Modify 函数, 只是参数不一致
    template <typename Fn> size_t Modify(Fn& fn);
    template <typename Fn, typename Arg1> size_t Modify(Fn& fn, const Arg1&);
    template <typename Fn, typename Arg1, typename Arg2>
    size_t Modify(Fn& fn, const Arg1&, const Arg2&);

    // fn(T& background, const T& foreground, ...) will be called to background
    // and foreground instances respectively.
    // fn(T& background, const T& foreground, ...) 函数同时处理前台和后台实例数据, fn函数的参数既有前台 也有后台实例数据
    // 下面三个 ModifyWithForeground 函数, 只是参数不一致
    template <typename Fn> size_t ModifyWithForeground(Fn& fn);
    template <typename Fn, typename Arg1>
    size_t ModifyWithForeground(Fn& fn, const Arg1&);
    template <typename Fn, typename Arg1, typename Arg2>
    size_t ModifyWithForeground(Fn& fn, const Arg1&, const Arg2&);
    
private:
    // 调用函数fn数据结构, 没有参数
    template <typename Fn>
    struct WithFG0 {
        WithFG0(Fn& fn, T* data) : _fn(fn), _data(data) { }
        size_t operator()(T& bg) {
            /*
                函数第一个参数是bg
                第二个参数: 如果 bg==_data, 说明bg是_data数组第一个, 那么&bg==_data为true, 即为1, 那么第二个参数就是 _data[1]
                          如果 bg!=_data, 说明bg是_data数组的第二个, 那么&bg==_data为false, 即为0, 那么第二个参数就是 _data[0]
                所以函数的两个参数分别是 _data 数组的两个变量, 即前台实例和后台实例的数据
            */
            return _fn(bg, (const T&)_data[&bg == _data]);
        }
    private:
        Fn& _fn;
        T* _data;
    };

    // 调用函数fn数据结构, 一个参数
    template <typename Fn, typename Arg1>
    struct WithFG1 {
        WithFG1(Fn& fn, T* data, const Arg1& arg1)
            : _fn(fn), _data(data), _arg1(arg1) {}
        size_t operator()(T& bg) {
            return _fn(bg, (const T&)_data[&bg == _data], _arg1);
        }
    private:
        Fn& _fn;
        T* _data;
        const Arg1& _arg1;
    };

    // 调用函数fn数据结构, 两个参数
    template <typename Fn, typename Arg1, typename Arg2>
    struct WithFG2 {
        WithFG2(Fn& fn, T* data, const Arg1& arg1, const Arg2& arg2)
            : _fn(fn), _data(data), _arg1(arg1), _arg2(arg2) {}
        size_t operator()(T& bg) {
            return _fn(bg, (const T&)_data[&bg == _data], _arg1, _arg2);
        }
    private:
        Fn& _fn;
        T* _data;
        const Arg1& _arg1;
        const Arg2& _arg2;
    };

    // 闭包数据结构, 一个参数, 封装函数 Fn + Arg1 作为没有参数的 Fn, 这样可以传入函数 Modify()
    template <typename Fn, typename Arg1>
    struct Closure1 {
        Closure1(Fn& fn, const Arg1& arg1) : _fn(fn), _arg1(arg1) {}
        size_t operator()(T& bg) { return _fn(bg, _arg1); }
    private:
        Fn& _fn;
        const Arg1& _arg1;
    };

    // 闭包数据结构, 两个参数
    template <typename Fn, typename Arg1, typename Arg2>
    struct Closure2 {
        Closure2(Fn& fn, const Arg1& arg1, const Arg2& arg2)
            : _fn(fn), _arg1(arg1), _arg2(arg2) {}
        size_t operator()(T& bg) { return _fn(bg, _arg1, _arg2); }
    private:
        Fn& _fn;
        const Arg1& _arg1;
        const Arg2& _arg2;
    };

    // 获取前台数据实例, 用于返回给调用方使用
    const T* UnsafeRead() const
    { return _data + _index.load(butil::memory_order_acquire); }    // 使用 acquire 内存序, 配合 Modify 函数中的 release 内存许

    // 添加一个Wrapper
    Wrapper* AddWrapper();
    // 移除一个Wrapper
    void RemoveWrapper(Wrapper*);

    // Foreground and background void.
    // 存储前台和后台实例数据, 用于存储真正的数据
    T _data[2];

    // Index of foreground instance.
    // 前台实例在上面数组中的index索引
    butil::atomic<int> _index;

    // Key to access thread-local wrappers.
    // 是否创建了 thread-local wrapper
    bool _created_key;
    // pthread_key_t: 表面上看起来这是一个全局变量, 所有线程都可以使用它, 而它的值在每一个线程中是单独存储的; 即 thread-local 数据
    pthread_key_t _wrapper_key; // 线程用来存储 thread-local 的key

    // All thread-local instances.
    // 所有线程的 thread-local wrappers 实例
    std::vector<Wrapper*> _wrappers;

    // Sequence access to _wrappers.
    // 添加/删除 wrapper 时使用, 等待所有线程读取完毕一次时 使用, 保证顺序的访问 _wrappers
    pthread_mutex_t _wrappers_mutex;

    // Sequence modifications.
    // 修改数据时使用, 保证一系列的修改顺序进行
    pthread_mutex_t _modify_mutex;
};

static const pthread_key_t INVALID_PTHREAD_KEY = (pthread_key_t)-1;

template <typename T, typename TLS>
class DoublyBufferedDataWrapperBase {
public:
    TLS& user_tls() { return _user_tls; }   // 返回用的tls引用数据, 使用方可以修改返回的引用
protected:
    TLS _user_tls;
};

// 用户不设置TLS时, 默认使用Void作为TLS, 继续调用上面的模板类初始化
template <typename T>
class DoublyBufferedDataWrapperBase<T, Void> {
};


template <typename T, typename TLS>
class DoublyBufferedData<T, TLS>::Wrapper : public DoublyBufferedDataWrapperBase<T, TLS> {
friend class DoublyBufferedData;
public:
    explicit Wrapper(DoublyBufferedData* c) : _control(c) {
        pthread_mutex_init(&_mutex, NULL);
    }
    
    ~Wrapper() {
        if (_control != NULL) {
            _control->RemoveWrapper(this);  // 将自己从DBD中删除
        }
        pthread_mutex_destroy(&_mutex);
    }

    // _mutex will be locked by the calling pthread and DoublyBufferedData.
    // Most of the time, no modifications are done, so the mutex is
    // uncontended and fast.
    // 大多数情况下, 没有修改操作, 锁没有竞争而且很快
    inline void BeginRead() {
        pthread_mutex_lock(&_mutex);
    }

    inline void EndRead() {
        pthread_mutex_unlock(&_mutex);
    }

    inline void WaitReadDone() {
        BAIDU_SCOPED_LOCK(_mutex);
    }
    
private:
    DoublyBufferedData* _control;   // 包装的原始数据
    pthread_mutex_t _mutex;     // 保护原始数据的锁
};

// Called when thread initializes thread-local wrapper.
// 线程初始化 wrapper 时调用, 添加一个 wrapper
template <typename T, typename TLS>
typename DoublyBufferedData<T, TLS>::Wrapper*
DoublyBufferedData<T, TLS>::AddWrapper() {
    std::unique_ptr<Wrapper> w(new (std::nothrow) Wrapper(this));
    if (NULL == w) {
        return NULL;
    }
    try {
        BAIDU_SCOPED_LOCK(_wrappers_mutex);
        _wrappers.push_back(w.get());   // 数组存储, 保存所有线程的wrapper数据
    } catch (std::exception& e) {
        return NULL;
    }
    return w.release();
}

// Called when thread quits.
// 线程退出时调用, 删除线程对应的 wrapper 数据
template <typename T, typename TLS>
void DoublyBufferedData<T, TLS>::RemoveWrapper(
    typename DoublyBufferedData<T, TLS>::Wrapper* w) {
    if (NULL == w) {
        return;
    }
    BAIDU_SCOPED_LOCK(_wrappers_mutex);
    for (size_t i = 0; i < _wrappers.size(); ++i) {
        if (_wrappers[i] == w) {
            _wrappers[i] = _wrappers.back();    // 将数组的最后一位放到 被删除 wrapper 的位置
            _wrappers.pop_back();
            return;
        }
    }
}

template <typename T, typename TLS>
DoublyBufferedData<T, TLS>::DoublyBufferedData()
    : _index(0)
    , _created_key(false)
    , _wrapper_key(0) {
    _wrappers.reserve(64);
    pthread_mutex_init(&_modify_mutex, NULL);
    pthread_mutex_init(&_wrappers_mutex, NULL);

    // 每个线程结束时, 系统将调用 第二个函数参数 来释放绑定在这个键上的内存块
    const int rc = pthread_key_create(&_wrapper_key,
                                      butil::delete_object<Wrapper>);
    if (rc != 0) {
        std::cerr << "Fail to pthread_key_create: " << strerror_r(rc, butil::tls_error_buf, butil::ERROR_BUFSIZE) << std::endl;
    } else {
        _created_key = true;
    }

    // Initialize _data for some POD types. This is essential for pointer
    // types because they should be Read() as NULL before any Modify().
    // POD（Plain Old Data）指的是C++定义的和C相兼容的数据结构
    // 初始化某些POD类型的数据. 对于指针类型非常重要, 因为在任何Modify()之前, Read()都应该是NULL
    if (butil::is_integral<T>::value || butil::is_floating_point<T>::value ||
        butil::is_pointer<T>::value || butil::is_member_function_pointer<T>::value) {
        _data[0] = T();
        _data[1] = T();
    }
}

template <typename T, typename TLS>
DoublyBufferedData<T, TLS>::~DoublyBufferedData() {
    // User is responsible for synchronizations between Read()/Modify() and
    // this function.
    if (_created_key) {
        pthread_key_delete(_wrapper_key);
    }
    
    {
        BAIDU_SCOPED_LOCK(_wrappers_mutex);
        for (size_t i = 0; i < _wrappers.size(); ++i) {
            _wrappers[i]->_control = NULL;  // hack: disable removal.
            delete _wrappers[i];
        }
        _wrappers.clear();
    }
    pthread_mutex_destroy(&_modify_mutex);
    pthread_mutex_destroy(&_wrappers_mutex);
}

// 获取前台实例数据
template <typename T, typename TLS>
int DoublyBufferedData<T, TLS>::Read(
    typename DoublyBufferedData<T, TLS>::ScopedPtr* ptr) {
    if (BAIDU_UNLIKELY(!_created_key)) {
        return -1;
    }
    // 获取当前线程的 thread-local 变量wrapper
    Wrapper* w = static_cast<Wrapper*>(pthread_getspecific(_wrapper_key));
    if (BAIDU_LIKELY(w != NULL)) {
        w->BeginRead();     // 加锁
        ptr->_data = UnsafeRead();  // 获取前台数据
        ptr->_w = w;    // 设置 wrapper, 方便 ptr 作用域结束时, 调用 wrapper 的 EndRead() 函数, 释放锁
        return 0;
    }

    // 线程第一次使用 DBD, 添加线程对应的 wrapper 数据
    w = AddWrapper();
    if (BAIDU_LIKELY(w != NULL)) {
        // 将创建的 wrapper 数据设置到线程存储中
        const int rc = pthread_setspecific(_wrapper_key, w);
        if (rc == 0) {
            w->BeginRead();
            ptr->_data = UnsafeRead();
            ptr->_w = w;
            return 0;
        }
    }
    return -1;
}

// 修改前后台实例数据
template <typename T, typename TLS>
template <typename Fn>
size_t DoublyBufferedData<T, TLS>::Modify(Fn& fn) {
    // _modify_mutex sequences modifications. Using a separate mutex rather
    // than _wrappers_mutex is to avoid blocking threads calling
    // AddWrapper() or RemoveWrapper() too long. Most of the time, modifications
    // are done by one thread, contention should be negligible.
    // _modify_mutex 会顺序的进行一系列的修改操作. 使用独立的锁而不是 _wrappers_mutex, 是怕阻塞调用 AddWrapper()/RemoveWrapper() 的线程太长时间.
    // 大多数情况, 修改操作是通过一个线程完成的, 竞争状态可以忽略不计.
    BAIDU_SCOPED_LOCK(_modify_mutex);
    // 获取后台实例数据索引index, 只是获取index存储的数据, 无所谓上下语句的执行顺序, 所以使用 relaxed 内存序即可
    int bg_index = !_index.load(butil::memory_order_relaxed); 
    // background instance is not accessed by other threads, being safe to
    // modify.
    // 后台实例数据 没有被其他线程使用, 可以安全的修改
    const size_t ret = fn(_data[bg_index]);
    if (!ret) {
        return 0;
    }

    // Publish, flip background and foreground.
    // The release fence matches with the acquire fence in UnsafeRead() to
    // make readers which just begin to read the new foreground instance see
    // all changes made in fn.
    // 后台实例修改完成, 翻转前台和后台数据
    // 使用 release 内存序存储 bg_index 到 _index 中, 和 UnsafeRead() 函数中的 acquire 内存序获取数据对应, 确保读取到 前台实例数据的读取者, 可以看到 fn 中对实例数据的所有修改（因为 release-acquire 确保 release 之前的修改不会在 release 语句之后）
    _index.store(bg_index, butil::memory_order_release);
    bg_index = !bg_index;
    
    // Wait until all threads finishes current reading. When they begin next
    // read, they should see updated _index.
    {
        // 对创建过 wrapper 的所有线程, 等待线程的当前次读取完毕, 这样后续线程在读取数据, 就是新的前台实例数据了, 我们就可以安全的修改后台实例数据了
        BAIDU_SCOPED_LOCK(_wrappers_mutex);
        for (size_t i = 0; i < _wrappers.size(); ++i) {
            _wrappers[i]->WaitReadDone();
        }
    }

    // 修改后台实例数据（刚才的前台实例数据）
    const size_t ret2 = fn(_data[bg_index]);
    if (ret2 == ret) {
        std::cerr << "Modify index=" << _index.load(butil::memory_order_relaxed) << std::endl;
    }
    return ret2;
}

// 不用参数个数的 Modify 函数
template <typename T, typename TLS>
template <typename Fn, typename Arg1>
size_t DoublyBufferedData<T, TLS>::Modify(Fn& fn, const Arg1& arg1) {
    Closure1<Fn, Arg1> c(fn, arg1);
    return Modify(c);
}

template <typename T, typename TLS>
template <typename Fn, typename Arg1, typename Arg2>
size_t DoublyBufferedData<T, TLS>::Modify(
    Fn& fn, const Arg1& arg1, const Arg2& arg2) {
    Closure2<Fn, Arg1, Arg2> c(fn, arg1, arg2);
    return Modify(c);
}

template <typename T, typename TLS>
template <typename Fn>
size_t DoublyBufferedData<T, TLS>::ModifyWithForeground(Fn& fn) {
    WithFG0<Fn> c(fn, _data);
    return Modify(c);
}

template <typename T, typename TLS>
template <typename Fn, typename Arg1>
size_t DoublyBufferedData<T, TLS>::ModifyWithForeground(Fn& fn, const Arg1& arg1) {
    WithFG1<Fn, Arg1> c(fn, _data, arg1);
    return Modify(c);
}

template <typename T, typename TLS>
template <typename Fn, typename Arg1, typename Arg2>
size_t DoublyBufferedData<T, TLS>::ModifyWithForeground(
    Fn& fn, const Arg1& arg1, const Arg2& arg2) {
    WithFG2<Fn, Arg1, Arg2> c(fn, _data, arg1, arg2);
    return Modify(c);
}

}  // namespace butil

#endif  // BUTIL_DOUBLY_BUFFERED_DATA_H
