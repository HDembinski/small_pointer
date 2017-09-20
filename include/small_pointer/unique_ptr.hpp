#ifndef SMALL_POINTER_UNIQUE_PTR_HPP
#define SMALL_POINTER_UNIQUE_PTR_HPP

#include "boost/cstdint.hpp"
#include "boost/type_traits.hpp"
#include "boost/assert.hpp"
#include "limits"
#include "new" // for bad_alloc

namespace small_pointer {

namespace detail {
    template <typename Tpointee, typename Tpos, unsigned Ncapacity>
    struct stack_storage {
        static constexpr unsigned cell_size = sizeof(Tpointee) > sizeof(Tpos) ? sizeof(Tpointee) : sizeof(Tpos);
        using pos_type = Tpos;

        stack_storage() {
            for (pos_type pos = 0; pos < Ncapacity; ++pos) {
                *reinterpret_cast<Tpos*>(buffer + pos * cell_size) = pos+1;
            }
        }

        pos_type allocate() {
            if (free_pos == Ncapacity) { return 0; }
            const auto pos = free_pos;
            free_pos = *reinterpret_cast<Tpos*>(buffer + pos * cell_size);
            return pos+1;
        }

        void deallocate(pos_type pos) {
            --pos;
            *reinterpret_cast<Tpos*>(buffer + pos * cell_size) = free_pos;
            free_pos = pos;
        }

        Tpointee* operator[](pos_type pos) noexcept {
            return reinterpret_cast<Tpointee*>(buffer + --pos * cell_size);
        }

        pos_type free_pos = 0;
        char buffer[Ncapacity * cell_size];
    };
}

template <typename Tpointee, typename Tinteger, unsigned Ncapacity>
class unique_ptr {
    using pos_type = typename ::boost::make_unsigned<Tinteger>::type;
    static_assert(Ncapacity <= std::numeric_limits<pos_type>::max(), "Ncapacity is too large");
    using storage_type = detail::stack_storage<Tpointee, pos_type, Ncapacity>;
    static storage_type storage;
public:
    using element_type = Tpointee;
    using pointer = element_type*;
    using reference = element_type&;

    unique_ptr() = default;
    unique_ptr(const unique_ptr&) = delete;
    unique_ptr& operator=(const unique_ptr&) = delete;
    unique_ptr(unique_ptr&& other) noexcept : pos(other.pos) {
        other.pos = 0;
    }
    unique_ptr& operator=(unique_ptr&& other) noexcept {
        if (this != &other) {
            pos = other.pos;
            other.pos = 0;
        }
        return *this;
    }

    ~unique_ptr() {
        if (!pos) return;
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

    template <typename T, typename U, unsigned N, class... A>
    friend unique_ptr<T, U, N> make_unique_ptr(A&&...);
};

// static member must be defined outside of class
template <typename T, typename U, unsigned N>
typename unique_ptr<T, U, N>::storage_type unique_ptr<T, U, N>::storage;

template <typename Tpointee, typename Tinteger, unsigned Ncapacity, class... Args>
unique_ptr<Tpointee, Tinteger, Ncapacity> make_unique_ptr(Args&&... args) {
    using unique_ptr_t = unique_ptr<Tpointee, Tinteger, Ncapacity>;
    const auto pos = unique_ptr_t::storage.allocate();
    if (!pos)
        throw std::bad_alloc();
    new (unique_ptr_t::storage[pos]) Tpointee(std::forward<Args>(args)...);
    return unique_ptr_t(pos);
}

}

#endif
