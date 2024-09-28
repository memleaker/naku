#ifndef NAKU_NETIO_H
#define NAKU_NETIO_H

#include <cstddef>
#include <cstdint>
#include <cerrno>
#include <cstring>

#include <unistd.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#ifdef HTTPS_SUPPORT
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif

#include <naku/base/copool/netio_task.h>

namespace naku { namespace base {

/* @brief 封装socket, 创建非阻塞socket */
static inline int naku_socket(int domain, int type, int protocol)
{
	return socket(domain, type | SOCK_NONBLOCK, protocol);
}

/* @brief 封装bind, listen接口 */
static inline int naku_listen(int fd, uint32_t ipaddr, uint16_t port)
{
	int ret;
	sockaddr_in addr;

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port   = htons(port);
	addr.sin_addr.s_addr = ipaddr;

	ret = bind(fd, (const sockaddr*)&addr, sizeof(addr));
	if (ret == -1)
		return ret;

	ret = listen(fd, 128);
	if (ret == -1)
		return ret;

	return 0;
}

/* @brief 封装connect过程
 * 1. 当connect没有立刻完成时, 挂起协程, 等待EPOLLOUT事件
 * 2. 当事件发生时, 连接成功
 */
class async_connect {
public:
	async_connect(int fd, sockaddr *addr, socklen_t addrlen) : 
				m_fd(fd), m_addr(addr), m_addrlen(addrlen), m_need_suspend(false) {}

    bool await_ready()
	{
		for (;;)
		{
			m_ret = connect(m_fd, m_addr, m_addrlen);
			if (m_ret == -1)
			{
				if (errno == EAGAIN || errno == EINPROGRESS)
				{
					m_need_suspend = true;
					return false;
				}

				if (errno == EINTR)
					continue;
			}

			return true;
		}
	}

    void await_suspend(std::coroutine_handle<netio_task::promise_type> handle)
	{
		handle.promise().fd = m_fd;
		handle.promise().events = EPOLLOUT;
		handle.promise().run_state = CO_IOWAIT;
	}

    ssize_t await_resume()
	{
		if (m_need_suspend)
			return 0;
		return m_ret;
	}

private:
	int       m_ret;
	int       m_fd;
	sockaddr *m_addr;
	socklen_t m_addrlen;
	bool      m_need_suspend;
};

class async_accept {
public:
	async_accept(int fd, sockaddr* addr, socklen_t *addrlen) : 
		m_fd(fd), m_connfd(-1), m_need_suspend(false), m_addr(addr), m_addrlen(addrlen) {}

    bool await_ready()
	{
		for (;;)
		{
			m_connfd = accept4(m_fd, m_addr, m_addrlen, SOCK_NONBLOCK);		
			if (m_connfd == -1)
			{
				if (errno == EAGAIN)
				{
					m_need_suspend = true;
					return false;  // suspend
				}

				if (errno == EINTR)
					continue;
			}

			return true; // dont't suspend
		}
	}

    void await_suspend(std::coroutine_handle<netio_task::promise_type> handle)
	{
		handle.promise().fd = m_fd;
		handle.promise().events = EPOLLIN;
		handle.promise().run_state = CO_IOWAIT;
	}

    ssize_t await_resume()
	{
		if (!m_need_suspend)
			return m_connfd;

		for (;;)
		{
			m_connfd = accept4(m_fd, m_addr, m_addrlen, SOCK_NONBLOCK);	
			if (m_connfd == -1)
			{
				if (errno == EINTR || errno == EAGAIN)
					continue;
			}

			return m_connfd;
		}
	}

private:
	int     	m_fd;
	int         m_connfd;
	bool        m_need_suspend;
	sockaddr   *m_addr;
	socklen_t  *m_addrlen;
};


class async_read {
public:
	async_read(int fd, void *buf, size_t len) : 
			m_fd(fd), m_buf(buf), m_len(len), m_need_suspend(false) {}

    bool await_ready()
	{
		for (;;)
		{
			m_nbytes = read(m_fd, m_buf, m_len);
			if (m_nbytes == -1)
			{
				if (errno == EAGAIN)
				{
					m_need_suspend = true;
					return false;
				}
				
				if (errno == EINTR)
					continue;
			}

			return true;
		}
	}

    void await_suspend(std::coroutine_handle<netio_task::promise_type> handle)
	{
		handle.promise().fd = m_fd;
		handle.promise().events = EPOLLIN;
		handle.promise().run_state = CO_IOWAIT;
	}

    ssize_t await_resume()
	{
		if (!m_need_suspend)
			return m_nbytes;

		for (;;)
		{
			m_nbytes = read(m_fd, m_buf, m_len);
			if (m_nbytes == -1)
			{
				if (errno == EINTR)
					continue;
			}

			return m_nbytes;
		}
	}

private:
	int     m_fd;
	void *  m_buf;
	size_t  m_len;
	ssize_t m_nbytes;
	bool    m_need_suspend;
};

class async_write {
public:
	async_write(int fd, void *buf, size_t len) : 
				m_fd(fd), m_buf(buf), m_len(len), m_need_suspend(false) {}

    bool await_ready()
	{
		for (;;)
		{
			m_nbytes = write(m_fd, m_buf, m_len);
			if (m_nbytes == -1)
			{
				if (errno == EAGAIN)
				{
					m_need_suspend = true;
					return false;
				}

				if (errno == EINTR)
					continue;
			}

			return true;
		}
	}

    void await_suspend(std::coroutine_handle<netio_task::promise_type> handle)
	{
		handle.promise().fd = m_fd;
		handle.promise().events = EPOLLOUT;
		handle.promise().run_state = CO_IOWAIT;
	}

    ssize_t await_resume()
	{
		if (!m_need_suspend)
			return m_nbytes;

		for (;;)
		{
			m_nbytes = write(m_fd, m_buf, m_len);
			if (m_nbytes == -1)
			{
				if (errno == EINTR)
					continue;
			}

			return m_nbytes;
		}
	}

private:
	int     m_fd;
	void *  m_buf;
	size_t  m_len;
	ssize_t m_nbytes;
	bool    m_need_suspend;
};

#ifdef HTTPS_SUPPORT
/* ssl */
class async_sslconnect {
public:
	async_sslconnect(SSL *ssl, int fd) : 
				m_fd(fd), m_ssl(ssl), m_need_suspend(false) {}

    bool await_ready()
	{
		int err;

		for (;;)
		{
			m_ret = SSL_connect(m_ssl);
			if (m_ret <= 0)
			{
				err = SSL_get_error(m_ssl, m_ret);
				if (err == SSL_ERROR_WANT_WRITE)
				{
					m_need_suspend = true;
					m_flag = EPOLLOUT;
					return false;
				}
				else if (err == SSL_ERROR_WANT_READ)
				{
					m_need_suspend = true;
					m_flag = EPOLLIN;
					return false;
				}
			}

			return true;
		}
	}

    void await_suspend(std::coroutine_handle<netio_task::promise_type> handle)
	{
		handle.promise().fd = m_fd;
		handle.promise().events = m_flag;
		handle.promise().run_state = CO_IOWAIT;
	}

    ssize_t await_resume()
	{
		int err;

		if (!m_need_suspend)
			return m_ret;

		m_ret = ::SSL_connect(m_ssl);
		if (m_ret <= 0)
		{
			err = SSL_get_error(m_ssl, m_ret);
			if (err == SSL_ERROR_WANT_WRITE || err == SSL_ERROR_WANT_READ)
			{
				return 0;
			}
		}

		return m_ret;
	}

private:
	int       m_flag;
	int       m_ret;
	int       m_fd;
	SSL      *m_ssl;
	bool      m_need_suspend;
};

class async_sslread {
public:
	async_sslread(SSL *ssl, int fd, void *buf, size_t len) : 
			m_fd(fd), m_buf(buf), m_ssl(ssl), m_len(len), m_need_suspend(false) {}

    bool await_ready()
	{
		int err;

		for (;;)
		{
			m_nbytes = SSL_read(m_ssl, m_buf, m_len);
			if (m_nbytes <= 0)
			{
				err = SSL_get_error(m_ssl, m_nbytes);
				if (err == SSL_ERROR_WANT_WRITE)
				{
					m_need_suspend = true;
					m_flag = EPOLLOUT;
					return false;
				}
				if (err == SSL_ERROR_WANT_READ)
				{
					m_need_suspend = true;
					m_flag = EPOLLIN;
					return false;
				}
			}

			return true;
		}
	}

    void await_suspend(std::coroutine_handle<netio_task::promise_type> handle)
	{
		handle.promise().fd = m_fd;
		handle.promise().events = m_flag;
		handle.promise().run_state = CO_IOWAIT;
	}

    ssize_t await_resume()
	{
		if (!m_need_suspend)
			return m_nbytes;

		return ::SSL_read(m_ssl, m_buf, m_len);
	}

private:
	int     m_fd;
	int     m_flag;
	void *  m_buf;
	SSL  *  m_ssl;
	size_t  m_len;
	ssize_t m_nbytes;
	bool    m_need_suspend;
};

class async_sslwrite {
public:
	async_sslwrite(SSL *ssl, int fd, void *buf, size_t len) : 
				m_fd(fd), m_buf(buf), m_ssl(ssl), m_len(len), m_need_suspend(false) {}

    bool await_ready()
	{
		int err;

		for (;;)
		{
			m_nbytes = SSL_write(m_ssl, m_buf, m_len);
			if (m_nbytes <= 0)
			{
				err = SSL_get_error(m_ssl, m_nbytes);
				if (err == SSL_ERROR_WANT_WRITE)
				{
					m_need_suspend = true;
					m_flag = EPOLLOUT;
					return false;
				}
				if (err == SSL_ERROR_WANT_READ)
				{
					m_need_suspend = true;
					m_flag = EPOLLIN;
					return false;
				}
			}
			return true;
		}

	}

    void await_suspend(std::coroutine_handle<netio_task::promise_type> handle)
	{
		handle.promise().fd = m_fd;
		handle.promise().events = m_flag;
		handle.promise().run_state = CO_IOWAIT;
	}

    ssize_t await_resume()
	{
		if (!m_need_suspend)
			return m_nbytes;

		return ::SSL_write(m_ssl, m_buf, m_len);
	}

private:
	int     m_fd;
	int     m_flag;
	void *  m_buf;
	SSL  *  m_ssl;
	size_t  m_len;
	ssize_t m_nbytes;
	bool    m_need_suspend;
};

#endif // HTTPS_SUPPORT

} } // namespace

#endif // NAKU_NETIO_H