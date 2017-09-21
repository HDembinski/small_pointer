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

template <typename Tpos, std::size_t Nbytes> struct chunk_t {
  chunk_t() = default;
  chunk_t(const chunk_t &other) { pos() = other.pos(); }
  chunk_t &operator=(const chunk_t &other) {
    pos() = other.pos();
    return *this;
  }
  chunk_t(chunk_t &&other) { pos() = other.pos(); }
  chunk_t &operator=(chunk_t &&other) {
    pos() = other.pos();
    return *this;
  }

  Tpos &pos() { return *reinterpret_cast<Tpos *>(buffer); }
  const Tpos &pos() const { return *reinterpret_cast<const Tpos *>(buffer); }
  template <typename T> T *as() { return reinterpret_cast<T *>(buffer); }
  char buffer[Nbytes];
};

template <typename Tpointee, typename Tpos, std::size_t Ncapacity>
struct stack_pool_impl {
  static_assert(Ncapacity <= std::numeric_limits<Tpos>::max(),
                "Ncapacity is too large");

  Tpos allocate() noexcept {
    if (free_pos == Ncapacity) {
      return 0;
    }
    const auto pos = free_pos;
    if (pos == max_pos)
      free_pos = ++max_pos;
    else
      free_pos = chunks[pos].pos();
    return pos + 1;
  }

  void deallocate(Tpos pos) noexcept {
    chunks[--pos].pos() = free_pos;
    free_pos = pos;
  }

  Tpointee *operator[](Tpos pos) noexcept {
    return chunks[--pos].template as<Tpointee>();
  }

  ::boost::mutex mtx;
  Tpos free_pos = 0, max_pos = 0;
  chunk_t<Tpos, max_size<Tpointee, Tpos>()> chunks[Ncapacity];
};

template <typename Tpointee, typename Tpos> struct dynamic_pool_impl {
  static constexpr Tpos Ncapacity = std::numeric_limits<Tpos>::max();

  Tpos allocate() {
    if (free_pos == Ncapacity) {
      return 0;
    }
    const auto pos = free_pos;
    if (pos == chunks.size()) {
      chunks.emplace_back();
      free_pos = chunks.size();
    } else
      free_pos = chunks[pos].pos();
    return pos + 1;
  }

  void deallocate(Tpos pos) noexcept {
    --pos;
    chunks[pos].pos() = free_pos;
    free_pos = pos;
  }

  Tpointee *operator[](Tpos pos) noexcept {
    return chunks[--pos].template as<Tpointee>();
  }

  ::boost::mutex mtx;
  Tpos free_pos = 0;
  std::vector<chunk_t<Tpos, sizeof(Tpointee *)>> chunks;
};

} // namespace detail

namespace tag {
template <unsigned Ncapacity> struct stack_pool;
struct dynamic_pool {};
} // namespace tag

template <typename Tpointee, typename Tinteger, typename PoolTag>
class unique_ptr {
  using pos_type = typename ::boost::make_unsigned<Tinteger>::type;

  static_assert(sizeof(pos_type) <= sizeof(Tpointee *),
                "Using an integer larger than a normal pointer makes no sense");

  template <typename T> struct tag2type {
    using type = detail::dynamic_pool_impl<Tpointee, pos_type>;
  };
  template <unsigned Ncapacity> struct tag2type<tag::stack_pool<Ncapacity>> {
    using type = detail::stack_pool_impl<Tpointee, pos_type, Ncapacity>;
  };

  using storage_type = typename tag2type<PoolTag>::type;
  static storage_type storage;

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
    ::boost::lock_guard<::boost::mutex> lock(storage.mtx);
    storage[pos]->~element_type();
    storage.deallocate(pos);
  }

  pointer get() const noexcept { return storage[pos]; }

  auto operator*() const -> reference {
    BOOST_ASSERT(pos);
    return *get();
  }

  pointer operator->() const noexcept { return get(); }

  void reset(unique_ptr p = unique_ptr()) noexcept { swap(p); }

  void swap(unique_ptr &other) noexcept { std::swap(pos, other.pos); }

private:
  explicit unique_ptr(pos_type p) noexcept : pos(p) {}

  pos_type pos = 0;

  template <typename T, typename U, typename P, class... A>
  friend unique_ptr<T, U, P> make_unique(A &&...);
};

// static member must be defined outside of class
template <typename T, typename U, typename P>
typename unique_ptr<T, U, P>::storage_type unique_ptr<T, U, P>::storage;

template <typename Tpointee, typename Tinteger, typename PoolTag, class... Args>
unique_ptr<Tpointee, Tinteger, PoolTag> make_unique(Args &&... args) {
  using unique_ptr_t = unique_ptr<Tpointee, Tinteger, PoolTag>;
  ::boost::lock_guard<::boost::mutex> lock(unique_ptr_t::storage.mtx);
  const auto pos = unique_ptr_t::storage.allocate();
  if (!pos)
    throw std::bad_alloc();
  new (unique_ptr_t::storage[pos]) Tpointee(std::forward<Args>(args)...);
  return unique_ptr_t(pos);
}

} // namespace small_pointer

#endif
