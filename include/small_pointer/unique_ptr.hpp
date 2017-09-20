#ifndef SMALL_POINTER_UNIQUE_PTR_HPP
#define SMALL_POINTER_UNIQUE_PTR_HPP

#include "boost/assert.hpp"
#include "boost/cstdint.hpp"
#include "boost/type_traits.hpp"
#include "limits"
#include "new" // for bad_alloc

namespace small_pointer {

namespace detail {
template <typename Tpointee, typename Tpos, unsigned Ncapacity>
struct stack_storage {
  static_assert(Ncapacity <= std::numeric_limits<Tpos>::max(),
                "Ncapacity is too large");

  static constexpr unsigned cell_size = sizeof(Tpointee) > sizeof(Tpos)
                                            ? sizeof(Tpointee)
                                            : sizeof(Tpos);

  stack_storage() {
    for (Tpos pos = 0; pos < Ncapacity; ++pos) {
      *reinterpret_cast<Tpos *>(buffer + pos * cell_size) = pos + 1;
    }
  }

  Tpos allocate() {
    if (free_pos == Ncapacity) {
      return 0;
    }
    const auto pos = free_pos;
    free_pos = *reinterpret_cast<Tpos *>(buffer + pos * cell_size);
    return pos + 1;
  }

  void deallocate(Tpos pos) {
    --pos;
    *reinterpret_cast<Tpos *>(buffer + pos * cell_size) = free_pos;
    free_pos = pos;
  }

  Tpointee *operator[](Tpos pos) noexcept {
    return reinterpret_cast<Tpointee *>(buffer + --pos * cell_size);
  }

  Tpos free_pos = 0;
  char buffer[Ncapacity * cell_size];
};
} // namespace detail

template <unsigned Ncapacity> struct stack_pool_tag;

struct dynamic_pool_tag {};

template <typename Tpointee, typename Tinteger, typename PoolTag>
class unique_ptr {
  using pos_type = typename ::boost::make_unsigned<Tinteger>::type;

  template <typename T> struct tag2type;

  template <unsigned Ncapacity> struct tag2type<stack_pool_tag<Ncapacity>> {
    using type = detail::stack_storage<Tpointee, pos_type, Ncapacity>;
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
  const auto pos = unique_ptr_t::storage.allocate();
  if (!pos)
    throw std::bad_alloc();
  new (unique_ptr_t::storage[pos]) Tpointee(std::forward<Args>(args)...);
  return unique_ptr_t(pos);
}

} // namespace small_pointer

#endif
