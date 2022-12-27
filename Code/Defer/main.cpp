#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <functional>
#include <list>


// RAII, 局部变量存储在栈中, 在析构局部变量时, 调用析构函数执行一些资源释放的操作
// 局部变量在栈中是先进后出, 后面的defer先于前面的defer被调用, 同时也需要保证defer函数中使用的临时变量需要在defer前定义
class DeferHelper
{
public:
	DeferHelper(std::function<void()> &&func) : m_func(std::move(func)) {}
	~DeferHelper() { if (m_func) m_func(); }

private:
	std::function<void()> m_func;
};

#define DEFER_CONNECTION(text1,text2) text1##text2
#define DEFER_CONNECT(text1,text2) DEFER_CONNECTION(text1,text2)
#define defer(func)  DeferHelper DEFER_CONNECT(L,__LINE__) ([&](){func;});  // 引用捕获


// fifo也可以用一个前置变量 实现最后释放, 不过如果需要多个defer按照顺序释放 需要使用list
class DeferHelperFifo
{
public:
	DeferHelperFifo() {}
	~DeferHelperFifo() { 
        for (auto &func : m_funcs) {
            func();
        }
        m_funcs.clear();
    }

    void add_func(std::function<void()> &&func) { m_funcs.push_back(std::move(func)); }

private:
	std::list<std::function<void()>> m_funcs;
};

#define use_defer_fifo()  DeferHelperFifo DEFER_FIFO;
#define defer_fifo(func)  DEFER_FIFO.add_func ([&](){func;});


void defer_test();
void defer_test_fifo();

int main()
{
    defer_test();

    fprintf(stdout, "-------------------\n");

    defer_test_fifo();
    return 0;
}

void defer_test()
{
    defer(
        fprintf(stdout, "defer_test 4\n");
    );

    defer(
        fprintf(stdout, "defer_test 3\n");
    );

    fprintf(stdout, "defer_test 1\n");

    defer(
        fprintf(stdout, "defer_test 2\n");
    );

    return ;
}

void defer_test_fifo()
{
    use_defer_fifo();

    defer_fifo(
        fprintf(stdout, "defer_test 2\n");
    );

    defer_fifo(
        fprintf(stdout, "defer_test 3\n");
    );

    fprintf(stdout, "defer_test 1\n");

    defer_fifo(
        fprintf(stdout, "defer_test 4\n");
    );

    return ;
}

