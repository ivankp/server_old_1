#ifndef IVANP_SERVER_HH
#define IVANP_SERVER_HH

#include <cstdint>
#include <cstdlib>
#include <vector>
#include <thread>
#include <iostream>
#include <utility>

#include "server/socket.hh"
#include "thread_safe_queue.hh"

struct epoll_event; // <sys/epoll.h>

namespace ivanp {

class [[ nodiscard ]] server {
public:
  using port_t = uint16_t;

private:
  uniq_socket main_socket, epoll;
  std::vector<std::thread> threads;
  thread_safe_queue<socket> queue;
  epoll_event* epoll_events;
  const unsigned n_epoll_events;
  const int epoll_timeout;

  struct thread_buffer {
    char* m = nullptr;
    size_t size = 0;

    thread_buffer(size_t size) noexcept
    : m(reinterpret_cast<char*>(malloc(size))), size(size) { }
    ~thread_buffer() { free(m); }
    thread_buffer() noexcept = default;
    thread_buffer(const thread_buffer&) = delete;
    thread_buffer& operator=(const thread_buffer&) = delete;
    thread_buffer(thread_buffer&& o) noexcept
    : m(o.m), size(o.size) {
      o.m = nullptr;
      o.size = 0;
    }
    thread_buffer& operator=(thread_buffer&& o) noexcept {
      std::swap(m,o.m);
      std::swap(size,o.size);
      return *this;
    }
  };

  void epoll_add(int);

public:
  server(port_t port, unsigned epoll_buffer_size, int epoll_timeout);
  ~server();

  void loop() noexcept;

  template <typename F>
  void operator()(
    unsigned nthreads, size_t buffer_size,
    F&& worker_function
  ) noexcept {
    threads.reserve(threads.size()+nthreads);
    for (unsigned i=0; i<nthreads; ++i) {
      threads.emplace_back([ &queue = this->queue,
        worker_function,
        buffer = thread_buffer(buffer_size)
      ]() mutable {
        for (;;) {
          try {
            worker_function(queue.pop(), buffer.m, buffer.size);
          } catch (const std::exception& e) {
            std::cerr << "\033[31;1m" << e.what() << "\033[0m" << std::endl;
          }
        }
      });
    }
  }
};

} // end namespace ivanp

#endif
