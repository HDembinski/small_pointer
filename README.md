# Small pointer

On most modern computers, a pointer occupies 64 bit. That's a whopping 8 bytes. What if you could use a smaller pointer? This C++11 header-only library provides a pointer type which works just like `std::unique_ptr`, but has a configurable size. It can be made as small as a byte.

Why is that useful? We have a lot of main memory nowadays, so some people think that we can live with fat pointers. But that's not always true. Modern CPUs can only use their full potential if they operate on data in their local cache, which is small. There are also embedded systems, which also have more tight memory bounds.

The code is released unter the Boost Source License v1.0 (see LICENSE file).

## Caveats

*Smaller address space.* Using a smaller pointer means that you can only allocate a finite number of objects of the same type before you run out of address space. If you use pointer with a size of 1 byte, you can only create 255 instances of the same type. Of course you can have more pointers, but not more pointees.

<!-- *Slower access.* Dereferencing a small pointer may be slower than deferencing a normal pointer. Benchmarks will follow.
 -->

*Conflicts with custom allocation.* As you may be able to guess, this library manages memory in a special way to make small pointers work. It *may* not work with classes that also customize memory allocation.

## Benchmarks

A small pointer with a stack-based memory pool is faster to create than a standard pointer. Access speed is the same.

|Benchmark                                                            |CPU [ns]|
|:--------------------------------------------------------------------|-------:|
|`std_ptr_create_destroy<char>`                                       |      77|
|`small_ptr_create_destroy<char, tag::stack_pool<3>>`                 |      51|
|`small_ptr_create_destroy<char, tag::stack_pool<255>>`               |      51|
|`std_ptr_create_destroy<std::array<char, 256>>`                      |     125|
|`small_ptr_create_destroy<std::array<char, 256>, tag::stack_pool<3>>`|      50|
|`std_ptr_access<char>`                                               |       2|
|`small_ptr_access<char, tag::stack_pool<3>>`                         |       2|
|`std_ptr_access<std::array<char, 256>>`                              |       2|
|`small_ptr_access<std::array<char, 256>, tag::stack_pool<3>>`        |       2|