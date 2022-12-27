#include <stdexcept>
#include <sys/resource.h>

#include "tcp.h"

/*
 * 构造
 */
CTcp::CTcp(const USHORT port, const std::string& ip, const int timeoutms)
 : m_port(port), m_ip(ip), m_timeoutms(timeoutms)
{
    m_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if(m_fd == -1) {
        ERROR("%s.\n", strerror(errno));
        throw std::runtime_error("get socket fd error.");
    }

    m_bconnect = false;
    m_conn_time = 0;

    if(m_timeoutms > 0 && !set_timeoutms(timeoutms)) {
        throw std::runtime_error("set socket time out error.");
    }

}

/*
 * 构造
 */
CTcp::CTcp(const int fd) : m_fd(fd)
{
    m_port = 0;
    m_ip.clear();

    m_bconnect = true;
}

/*
 * 析构
 */
CTcp::~CTcp() throw()
{
    tcp_close();
}

/*
 * set socket ip
 */
void CTcp::set_ip(const std::string& ip)
{
    m_ip = ip;
}

/*
 * set port
 */
void CTcp::set_port(const USHORT port)
{
    m_port = port;
}

/*
 * set block
 */
bool CTcp::set_block()
{
    ulong ul = 0;

    return !::ioctl(m_fd, FIONBIO, &ul);//恢复socket 阻塞;
}

/*
 * set non block
 */
bool CTcp::set_nonblock()
{
    ulong ul = 1;

    return !::ioctl(m_fd, FIONBIO, &ul);//socket 阻塞;
}

/*
 * set be connect
 */
void CTcp::set_bconnect(bool bconnect)
{
    m_bconnect = bconnect;
}

/*
 * set timeout
 *
 * parameters:
 * iTimeoutMs: timeout millisecond
 *
 * return-value: true or false
 */
bool CTcp::set_timeoutms(int timeoutms)
{
    m_timeoutms = timeoutms;
    struct timeval temp;

    temp.tv_sec = timeoutms / 1000;
    temp.tv_usec = (timeoutms % 1000) * 1000;

    if (FAILURE == setsockopt(m_fd, SOL_SOCKET, SO_SNDTIMEO, &temp, sizeof(temp))) {
        ERROR("%s", strerror(errno));
        return false;
    }

    if (FAILURE == setsockopt(m_fd, SOL_SOCKET, SO_RCVTIMEO, &temp, sizeof(temp))) {
        ERROR("%s", strerror(errno));
        return false;
    }

    return true;
}

int CTcp::set_limit(int limit)
{
    struct rlimit temp;

    memset(&temp, '\0', sizeof(struct rlimit));
    temp.rlim_max = temp.rlim_cur = limit;

    if (FAILURE == setrlimit(RLIMIT_NOFILE, &temp)) {
        ERROR("%s", strerror(errno));
        return FAILURE;
    }

    return SUCCESS;
}

int CTcp::set_send_buffer(UINT size)
{
    if (FAILURE == setsockopt(m_fd, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size))) {
        ERROR("%s", strerror(errno));
        return FAILURE;
    }

    return SUCCESS;
}

int CTcp::set_recv_buffer(UINT size)
{
    if (FAILURE == setsockopt(m_fd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size))) {
        ERROR("%s", strerror(errno));
        return FAILURE;
    }

    return SUCCESS;
}

int CTcp::set_nodelay()
{
    int on = 1;

    if (FAILURE == setsockopt(m_fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on))) {
        ERROR("%s", strerror(errno));
        return FAILURE;
    }

    return SUCCESS;
}

int CTcp::set_reused()
{
    int status = 1;

    if (FAILURE == setsockopt(m_fd, SOL_SOCKET, SO_REUSEADDR, &status, sizeof(status))) {
        ERROR("%s", strerror(errno));
        return FAILURE;
    }

    return SUCCESS;
}

/*
 * 获取socket ip
 */
void CTcp::get_ip(std::string& ip)
{
    ip = m_ip;
}

/*
 * get port
 */
USHORT CTcp::get_port()
{
    return m_port;
}

/*
 * 获取socket描述符
 * 返回值：socket 描述符
 */
int CTcp::get_fd()
{
    return m_fd;
}

/*
 * get is connect
 */
bool CTcp::get_bconnect()
{
    return m_bconnect;
}

/*
 * get timeout
 */
int CTcp::get_timeout()
{
    return m_timeoutms;
}

time_t CTcp::get_connect_time()
{
    return m_conn_time;
}

/*
 * socket 绑定ip 端口
 * 参数：
 * port: 连接端口
 * ip:绑定ip  默认为空
 * 返回值：成功true  失败false
 */
bool CTcp::server_bind()
{
    if(m_port <= 0) {
        return false;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(m_port);

    if(m_ip.empty()) {//ip为空
        addr.sin_addr.s_addr = INADDR_ANY;//任意地址监听
    } else {//ip非空
        inet_aton(m_ip.c_str(),&addr.sin_addr);//指定ip段监听
    }

    if(::bind(m_fd, (struct sockaddr *)&addr, sizeof(struct sockaddr)) == -1) {
        ERROR("%s", strerror(errno));
        return false;
    }

    return true;
}

/*
 * socket 监听
 * 参数：
 * reqNum: 监听队列最大长度， 默认1
 * 返回值:成功true 失败false
 */
bool CTcp::server_listen(const int request_num)
{
    if(::listen(m_fd, request_num) < 0) {
        ERROR("%s", strerror(errno));
        return false;
    }

    return true;
}

/*
 * server 接收client 连接
 * 参数：
 * 返回值：成功：客户端socket 指针, 失败：空
 */
CTcp* CTcp::server_accept()
{
    CTcp* tcp = NULL;

    if(m_fd <= 0) {
        return tcp;
    }

    struct sockaddr_in addr;
    u_int size = sizeof(struct sockaddr_in);
    int cfd = -1;

    if((cfd = ::accept(m_fd, (struct sockaddr *)&addr, &size)) < 0) {//accept失败
        ERROR("%s", strerror(errno));
        return tcp;
    }

    tcp = new CTcp(cfd);

    return tcp;
}

/*
 * connect server
 * 参数：
 * 返回值：-1 出错  -2 超时  0 成功
 */
int CTcp::client_connect()
{
    int ret = FAILURE;

    if(m_fd < 0 || m_ip.empty() || m_port == 0) {
        // ERROR("parameters  error [fd:%d] [ip:%s] [port:%d].", m_iFd, m_strIp.c_str(), m_usPort);
        return ret;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    inet_aton(m_ip.c_str(), &addr.sin_addr);
    addr.sin_port = htons(m_port);

    ret = ::connect(m_fd, (struct sockaddr *)&addr, sizeof(addr));

    if(ret < 0 && errno == EAGAIN) {
        errno = 0;
        ret = ERR_TIMEOUT;
    } else if(ret < 0) {
        ERROR("%s.", strerror(errno));
        ret = FAILURE;
    } else if(ret == 0) {
        m_conn_time = time(0);
        m_bconnect = true;
        ret = SUCCESS;
    } else {
        ret = FAILURE;
    }

    return ret;
}

/*
 * 重连socket
 * 参数：
 * 返回值：-1 出错  -2 超时  0 成功
 */

int CTcp::client_reconnect()
{
    if(!tcp_reset()) {
        return FAILURE;
    }

    if(m_timeoutms >0) {
        set_timeoutms(m_timeoutms);
    }

    return client_connect();
}

/*
 * socket读取数据
 * 参数：
 * recvBuf:保存读取数据buf
 * recvLen:读取长度
 * 返回值：成功返回读取到的长度   失败返回-1  超时－2 读取对象不存在 0
 */
int CTcp::tcp_read(void* const readbuf, const UINT bufsize)
{
    int ret = FAILURE;

    if(readbuf == NULL || bufsize <= 0 || m_fd <= 0) {//参数有误
        ERROR("parameters error.\n");
        return ret;
    }

    ret = ::recv(m_fd, readbuf, bufsize, 0);

    switch(ret)
    {
        case -1:
            if(errno == EAGAIN) {//超时
                errno = 0;
                ret = ERR_TIMEOUT;
                break;
            } else {
                ERROR("%s.\n", strerror(errno));
                ret = FAILURE;
            }

            break;
        case 0://socket 读取对象不存在
            ret = TCP_DISCONNECT;
            break;
        default:
            if(ret < 0) {
                ret = FAILURE;
            }
            break;
    }

    return ret;
}

/*
 * socket读取数据
 * 参数：
 * recvBuf:保存读取数据buf
 * recvLen:读取长度
 * 返回值：成功返回接收长度   失败返回-1  超时－2 读取对象不存在 0
 */
int CTcp::tcp_readall(void* const readbuf, const UINT readsize)
{
    int ret = 0;
    int length = 0;

    if(readbuf == NULL || readsize <= 0) {//参数有误
        ERROR("parameters error.\n");
        return FAILURE;
    }

    while((length = tcp_read(((char *)readbuf + ret), readsize - ret)) > 0)
    {
        ret += length;//已读取的长度

        if(ret >= (int)readsize) {//读取recvLen完毕
            break;
        }
    }

    if(length <= 0) {
        return length;
    }

    return ret;
}

/*
 *向socket发送数据
 *参数：
 *sockfd: socket句柄
 *sendBuf: 写入的内容
 *sendLen: 写入的长度
 *返回值： 成功返回接收长度   失败返回-1  超时－2
 */
int CTcp::tcp_write(const void* sendbuf, const UINT bufsize)
{
    int ret = FAILURE;

    if(sendbuf == NULL || bufsize <= 0 || m_fd <= 0) {//参数有误
        ERROR("parameters error.\n");
        return ret;
    }

    ret = ::send(m_fd, sendbuf, bufsize, 0);

    if(ret < 0 && errno == EAGAIN) {
        errno = 0;
        ret = ERR_TIMEOUT;
    } else if(ret < 0) {
        ERROR("%s.", strerror(errno));
        ret = FAILURE;
    }

    return ret;
}

/*
 * write all
 * 返回值： 成功返回接收长度   失败返回-1  超时－2
 */
int CTcp::tcp_writeall(const void* sendbuf, const UINT bufsize)
{
    int ret = 0;
    int length = 0;

    if(sendbuf == NULL || bufsize <= 0) {//参数有误
        ERROR("parameters error.\n");
        return FAILURE;
    }

    while((length = tcp_write(((char *)sendbuf + ret), bufsize - ret)) > 0)
    {
        ret += length;

        if((u_int)ret >= bufsize) {
            break;
        }
    }

    if(length <= 0) {
        return length;
    }

    return ret;
}

/*
 * 关闭socket
 */
bool CTcp::tcp_close()
{
    if(m_fd > 0) {
        ::close(m_fd);
        m_bconnect = false;
    }

    return true;
}

/*
 *
 */
bool CTcp::tcp_reset()
{
    tcp_close();

    m_fd = ::socket(AF_INET,SOCK_STREAM,0);
    if(m_fd == -1) {
        ERROR("%s.", strerror(errno));
        return false;
    }

    return true;
}
