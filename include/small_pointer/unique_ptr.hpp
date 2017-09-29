#ifndef SMALL_POINTER_UNIQUE_PTR_HPP
#define SMALL_POINTER_UNIQUE_PTR_HPP

#include "algorithm"
#include "array"
#include "boost/assert.hpp"
#include "boost/cstdint.hpp"
#include "boost/thread.hpp"
#include "boost/type_traits.hpp"
#include "limits"
#include "new" // for bad_alloc
#include "vector"

namespace small_pointer {

namespace detail {
template <typename A, typename B> constexpr unsigned max_size() {
  return sizeof(A) > sizeof(B) ? sizeof(A) : sizeof(B);
}
struct std_alloc {
  static void *alloc(std::size_t size) { return std::malloc(size); }
  static void free(void *ptr) { std::free(ptr); };
};
} // namespace detail

namespace tag {
template <unsigned Ncapacity> struct stack_pool;
template <unsigned Ncapacity> struct thread_local_stack_pool;
template <typename Alloc = ::small_pointer::detail::std_alloc>
struct dynamic_pool;
template <typename Alloc = ::small_pointer::detail::std_alloc>
struct thread_local_dynamic_pool;
} // namespace tag

namespace detail {

template <typename Tpointee, typename Tpos, std::size_t Ncapacity>
struct stack_pool_impl {
  static_assert(Ncapacity <= std::numeric_limits<Tpos>::max(),
                "Ncapacity is too large");
  using chunk = union {
    Tpos pos;
    char buffer[max_size<Tpointee, Tpos>()];
  };

  Tpos allocate() noexcept {
    if (free_pos == Ncapacity) {
      return 0;
    }
    const auto pos = free_pos;
    if (pos == max_pos)
      free_pos = ++max_pos;
    else
      free_pos = chunks[pos].pos;
    return pos + 1;
  }

  void deallocate(Tpos pos) noexcept {
    chunks[--pos].pos = free_pos;
    free_pos = pos;
  }

  Tpointee *operator[](Tpos pos) noexcept {
    return reinterpret_cast<Tpointee *>(chunks[--pos].buffer);
  }

  Tpos free_pos = 0, max_pos = 0;
  chunk chunks[Ncapacity];
};

template <typename Tpointee, typename Tpos, typename Alloc>
struct dynamic_pool_impl {
  static constexpr Tpos Ncapacity = std::numeric_limits<Tpos>::max();
  struct chunk {
    chunk() : ptr(Alloc::alloc(sizeof(Tpointee))) {}
    chunk(const chunk &) = delete;
    chunk &operator=(const chunk &) = delete;
    chunk(chunk &&other) : ptr(other.ptr) { other.ptr = 0; }
    chunk &operator=(chunk &&other) {
      if (this != &other) {
        ptr = other.ptr;
        other.ptr = 0;
        return *this;
      }
    }
    ~chunk() { Alloc::free(ptr); }
    Tpos pos;
    void *ptr;
  };

  Tpos allocate() {
    if (free_pos == Ncapacity) {
      return 0;
    }
    const auto pos = free_pos;
    if (pos == chunks.size()) {
      chunks.emplace_back();
      free_pos = chunks.size();
    } else
      free_pos = chunks[pos].pos;
    return pos + 1;
  }

  void deallocate(Tpos pos) noexcept {
    --pos;
    chunks[pos].pos = free_pos;
    free_pos = pos;
  }

  Tpointee *operator[](Tpos pos) noexcept {
    return static_cast<Tpointee *>(chunks[--pos].ptr);
  }

  Tpos free_pos = 0;
  std::vector<chunk> chunks;
};

template <typename Tpointee, typename Tpos, typename P> struct storage_t;

// global implementations

template <typename Tpointee, typename Tpos, unsigned Ncapacity>
struct storage_t<Tpointee, Tpos,
                 ::small_pointer::tag::stack_pool<Ncapacity>> {
  static stack_pool_impl<Tpointee, Tpos, Ncapacity> storage;
  static ::boost::mutex mtx;
  template <typename... Args> static Tpos create(Args &&... args) {
    ::boost::lock_guard<::boost::mutex> lock(mtx);
    const auto pos = storage.allocate();
    if (!pos)
      throw std::bad_alloc();
    new (storage[pos]) Tpointee(std::forward<Args>(args)...);
    return pos;
  }
  static void destroy(Tpos pos) {
    ::boost::lock_guard<::boost::mutex> lock(mtx);
    storage[pos]->~Tpointee();
    storage.deallocate(pos);
  }
};

template <typename T, typename U, unsigned N>
stack_pool_impl<T, U, N>
    storage_t<T, U, ::small_pointer::tag::stack_pool<N>>::storage;
template <typename T, typename U, unsigned N>
::boost::mutex storage_t<T, U, ::small_pointer::tag::stack_pool<N>>::mtx;

template <typename Tpointee, typename Tpos, typename Alloc>
struct storage_t<Tpointee, Tpos,
                 ::small_pointer::tag::dynamic_pool<Alloc>> {
  static dynamic_pool_impl<Tpointee, Tpos, Alloc> storage;
  static ::boost::mutex mtx;
  template <typename... Args> static Tpos create(Args &&... args) {
    ::boost::lock_guard<::boost::mutex> lock(mtx);
    const auto pos = storage.allocate();
    if (!pos)
      throw std::bad_alloc();
    new (storage[pos]) Tpointee(std::forward<Args>(args)...);
    return pos;
  }
  static void destroy(Tpos pos) {
    ::boost::lock_guard<::boost::mutex> lock(mtx);
    storage[pos]->~Tpointee();
    storage.deallocate(pos);
  }
};

template <typename T, typename U, typename A>
dynamic_pool_impl<T, U, A>
    storage_t<T, U, ::small_pointer::tag::dynamic_pool<A>>::storage;
template <typename T, typename U, typename A>
::boost::mutex
    storage_t<T, U, ::small_pointer::tag::dynamic_pool<A>>::mtx;

// thread local implementations

template <typename Tpointee, typename Tpos, unsigned Ncapacity>
struct storage_t<Tpointee, Tpos,
                 ::small_pointer::tag::thread_local_stack_pool<Ncapacity>> {
  static thread_local stack_pool_impl<Tpointee, Tpos, Ncapacity> storage;
  template <typename... Args> static Tpos create(Args &&... args) {
    const auto pos = storage.allocate();
    if (!pos)
      throw std::bad_alloc();
    new (storage[pos]) Tpointee(std::forward<Args>(args)...);
    return pos;
  }
  static void destroy(Tpos pos) {
    storage[pos]->~Tpointee();
    storage.deallocate(pos);
  }
};

template <typename T, typename U, unsigned N>
thread_local stack_pool_impl<T, U, N>
    storage_t<T, U, ::small_pointer::tag::thread_local_stack_pool<N>>::storage;

template <typename Tpointee, typename Tpos, typename Alloc>
struct storage_t<Tpointee, Tpos,
                 ::small_pointer::tag::thread_local_dynamic_pool<Alloc>> {
  static thread_local dynamic_pool_impl<Tpointee, Tpos, Alloc> storage;
  template <typename... Args> static Tpos create(Args &&... args) {
    const auto pos = storage.allocate();
    if (!pos)
      throw std::bad_alloc();
    new (storage[pos]) Tpointee(std::forward<Args>(args)...);
    return pos;
  }
  static void destroy(Tpos pos) {
    storage[pos]->~Tpointee();
    storage.deallocate(pos);
  }
};

template <typename T, typename U, typename A>
thread_local dynamic_pool_impl<T, U, A> storage_t<
    T, U, ::small_pointer::tag::thread_local_dynamic_pool<A>>::storage;

} // namespace detail

template <typename Tpointee, typename Tpos, typename PoolTag>
class unique_ptr : private detail::storage_t<Tpointee, Tpos, PoolTag> {

  static_assert(sizeof(Tpos) <= sizeof(Tpointee *),
                "Using an integer larger than a normal pointer makes no sense");

public:
  using element_type = Tpointee;
  using pointer = element_type *;
  using reference = element_type &;

  unique_ptr() = default;
  unique_ptr(const unique_ptr &) = delete;
  unique_ptr &operator=(const unique_ptr &) = delete;
  unique_ptr(unique_ptr &&other) noexcept : pos(other.pos) { other.pos = 0; }
  unique_ptr &operator=(unique_ptr &&other) noexcept {
    if (this != &other) {
      pos = other.pos;
      other.pos = 0;
    }
    return *this;
  }

  ~unique_ptr() {
    if (!pos)
      return;
    detail::storage_t<Tpointee, Tpos, PoolTag>::destroy(pos);
  }

  pointer get() const noexcept {
    return detail::storage_t<Tpointee, Tpos, PoolTag>::storage[pos];
  }

  auto operator*() const -> reference {
    BOOST_ASSERT(pos);
    return *get();
  }

  pointer operator->() const noexcept { return get(); }

  void reset(unique_ptr p = unique_ptr()) noexcept { swap(p); }

  void swap(unique_ptr &other) noexcept { std::swap(pos, other.pos); }

private:
  explicit unique_ptr(Tpos p) noexcept : pos(p) {}

  Tpos pos = 0;

  template <typename T, typename U, typename P, class... A>
  friend unique_ptr<T, U, P> make_unique(A &&...);
};

template <typename Tpointee, typename Tpos, typename PoolTag, class... Args>
unique_ptr<Tpointee, Tpos, PoolTag> make_unique(Args &&... args) {
  using unique_ptr_t = unique_ptr<Tpointee, Tpos, PoolTag>;
  const auto pos = unique_ptr_t::create(std::forward<Args>(args)...);
  return unique_ptr_t(pos);
}

} // namespace small_pointer

#endif
