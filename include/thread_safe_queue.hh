#ifndef IVANP_THREAD_SAFE_QUEUE_HH
#define IVANP_THREAD_SAFE_QUEUE_HH

#include <queue>
#include <mutex>
#include <condition_variable>

template <typename T>
class thread_safe_queue {
  std::queue<T> q;
  std::mutex mx;
  std::condition_variable cv;

public:
  template <typename... Args>
  void push(Args&&... args) {
    { std::lock_guard lock(mx);
      q.emplace(std::forward<Args>(args)...);
    }
    cv.notify_all();
  }

  auto pop() {
    std::unique_lock lock(mx);
    while (q.empty()) cv.wait(lock);
    auto x = std::move(q.front());
    q.pop();
    return x;
  }
};

#endif
