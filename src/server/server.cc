#include "server/server.hh"

#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/epoll.h>

#include "error.hh"
#include "debug.hh"

namespace ivanp {

server::~server() {
  delete[] epoll_events;
}

server::server(port_t port, unsigned nevents_max)
: main_socket(::socket(AF_INET, SOCK_STREAM, 0)),
  nevents_max(nevents_max)
{
  if (!main_socket.is_valid()) THROW_ERRNO("socket()");
  int enable = 1;
  if (::setsockopt(
    *main_socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)
  ) < 0) THROW_ERRNO("setsockopt(SO_REUSEADDR)");

  sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  ::memset(&addr.sin_zero, '\0', sizeof(addr.sin_zero));
  if (::bind(*main_socket,reinterpret_cast<sockaddr*>(&addr),sizeof(addr)) < 0)
    THROW_ERRNO("bind()");
  if (::listen(*main_socket, 8/*backlog*/) < 0) THROW_ERRNO("listen()");

  epoll = ::epoll_create1(0);
  if (!epoll.is_valid()) THROW_ERRNO("epoll_create1()");
  try {
    epoll_events = new epoll_event[nevents_max];
    epoll_add(main_socket);
  } catch (...) {
    delete[] epoll_events;
    throw;
  }
}

void server::epoll_add(file_desc sock) {
  epoll_event event {
    .events = EPOLLIN | EPOLLET,
    .data = { .fd = *sock }
  };
  if (::epoll_ctl(*epoll,EPOLL_CTL_ADD,*sock,&event) < 0)
    THROW_ERRNO("epoll_ctl()");
}

file_desc server::accept() {
  sockaddr_in addr;
  socklen_t addr_size = sizeof(addr);
  int new_socket = ::accept(
    *main_socket, reinterpret_cast<sockaddr*>(&addr), &addr_size
  );
  if (new_socket < 0) THROW_ERRNO("accept()");
  return new_socket;
}

bool server::accept(file_desc& socket) {
  if (socket == main_socket) {
    socket = accept();
    return true;
  }
  return false;
}

int server::wait() {
  const int n = ::epoll_wait(*epoll,epoll_events,nevents_max,-1);
  if (n < 0) THROW_ERRNO("epoll_wait()");
  return n;
}

file_desc server::check_event() {
  auto& e = epoll_events[--nevents];
  const auto flags = e.events;
  if (flags & EPOLLERR) ERROR("EPOLLERR");
  if (flags & EPOLLHUP) ERROR("EPOLLHUP");
  if (!(flags & EPOLLIN)) ERROR("not EPOLLIN");

  return e.data.fd;
}

void server::loop() {
  for (;;) {
    try {
      INFO("35;1","Waiting for events");
      nevents = wait();
      INFO("35;1","notify_all");
      cv.notify_all();
      // main_mutex.lock(); // wait for events to be consumed
    } catch (const std::exception& e) {
      std::cerr << "\033[31m" << e.what() << "\033[0m\n";
    }
  }
}

}
