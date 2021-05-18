#ifndef IVANP_SCOPE_GUARD_HH
#define IVANP_SCOPE_GUARD_HH

namespace ivanp {

template <typename F>
class scope_guard {
  F f;
public:
  explicit scope_guard(F&& f): f(std::forward<F>(f)) { }
  scope_guard() = delete;
  scope_guard(const scope_guard&) = delete;
  scope_guard(scope_guard&&) = delete;
  scope_guard& operator=(const scope_guard&) = delete;
  scope_guard& operator=(scope_guard&&) = delete;
  ~scope_guard() { f(); }

  const F& operator*() const noexcept { return f; }
  F& operator*() noexcept { return f; }
  const F* operator->() const noexcept { return &f; }
  F* operator->() noexcept { return &f; }
};

}

#endif
