#include "boost/core/lightweight_test.hpp"
#include "small_pointer/unique_ptr.hpp"
#include "vector"

using namespace small_pointer;

static unsigned test_type_ctor_count = 0;
static unsigned test_type_dtor_count = 0;

struct test_type {
  int x = 0;
  test_type(int a) : x(a) { ++test_type_ctor_count; }
  ~test_type() { ++test_type_dtor_count; }
  bool operator==(const int &y) const { return x == y; }
  friend std::ostream &operator<<(std::ostream &os, const test_type &t) {
    os << "test_type(" << t.x << ")";
    return os;
  }
};

template <typename PoolTag> static void tests() {
  auto p0 = make_unique<int, short, PoolTag>(42);
  BOOST_TEST_EQ(sizeof(p0), sizeof(short));
  BOOST_TEST_EQ(*p0, 42);

  auto p1 = make_unique<int, short, PoolTag>(43);
  BOOST_TEST_EQ(*p0, 42);
  BOOST_TEST_EQ(*p1, 43);

  auto p2 = make_unique<int, short, PoolTag>(44);
  BOOST_TEST_EQ(*p0, 42);
  BOOST_TEST_EQ(*p1, 43);
  BOOST_TEST_EQ(*p2, 44);

  if (std::is_same<PoolTag, tag::stack_pool<3>>::value)
    BOOST_TEST_THROWS((make_unique<int, short, PoolTag>(45)), std::bad_alloc);

  test_type_ctor_count = 0;
  test_type_dtor_count = 0;
  {
    auto p0 = make_unique<test_type, short, PoolTag>(42);
    BOOST_TEST_EQ(*p0, 42);

    auto p1 = make_unique<test_type, short, PoolTag>(43);
    BOOST_TEST_EQ(*p0, 42);
    BOOST_TEST_EQ(*p1, 43);

    auto p2 = make_unique<test_type, short, PoolTag>(44);
    BOOST_TEST_EQ(*p0, 42);
    BOOST_TEST_EQ(*p1, 43);
    BOOST_TEST_EQ(*p2, 44);

    p0.reset();
    p0 = make_unique<test_type, short, PoolTag>(45);
    BOOST_TEST_EQ(*p0, 45);
    BOOST_TEST_EQ(*p1, 43);
    BOOST_TEST_EQ(*p2, 44);

    p0.reset();

    p0.reset(make_unique<test_type, short, PoolTag>(46));
    BOOST_TEST_EQ(*p0, 46);
    BOOST_TEST_EQ(*p1, 43);
    BOOST_TEST_EQ(*p2, 44);
  }
  BOOST_TEST_EQ(test_type_ctor_count, 5);
  BOOST_TEST_EQ(test_type_dtor_count, 5);
}

int main() {
  using namespace small_pointer;

  tests<tag::stack_pool<3>>();
  tests<tag::stack_pool<255>>();
  tests<tag::dynamic_pool<>>();

  return boost::report_errors();
}