#ifndef IVANP_SERVER_HH
#define IVANP_SERVER_HH

#include <cstdint>
#include <vector>
#include <thread>
#include <condition_variable>
#include <iostream>

#include "file_desc.hh"

struct epoll_event; // <sys/epoll.h>

namespace ivanp {

class [[ nodiscard ]] server {
public:
  using port_t = uint16_t;

private:
  uniq_file_desc main_socket, epoll;
  unsigned nevents_max, nevents = 0;
  epoll_event* epoll_events;
  std::mutex worker_mutex; //, main_mutex;
  std::condition_variable cv;
  std::vector<std::thread> threads;

public:
  server(port_t port, unsigned nevents_max);
  ~server();

  file_desc accept();
  bool accept(file_desc&);

  void loop();
  [[ nodiscard ]] int wait();

  void epoll_add(file_desc);
  file_desc check_event();

  template <typename F>
  void operator()(
    unsigned nthreads, size_t buffer_size,
    F&& worker_function
  ) {
    threads.reserve(threads.size()+nthreads);
    for (unsigned i=0; i<nthreads; ++i) {
      threads.emplace_back([ this,
        worker_function = std::forward<F>(worker_function),
        buffer = std::vector<char>(buffer_size)
      ]() mutable {
        for (;;) {
          file_desc socket;
          try {
            { std::unique_lock lock(worker_mutex);
              while (nevents == 0) cv.wait(lock); // wait implies yield
              socket = check_event();
              // if (nevents == 0) main_mutex.unlock();
            }
            worker_function(*this, socket, buffer);
          } catch (const std::exception& e) {
            std::cerr << "\033[31;1m" << e.what() << "\033[0m\n";
          }
        }
      });
    }
  }
};

} // end namespace ivanp

#endif
