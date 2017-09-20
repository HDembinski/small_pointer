#include "boost/core/lightweight_test.hpp"
#include "small_pointer/unique_ptr.hpp"
#include "vector"

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

int main() {
  using namespace small_pointer;

  {
    auto p0 = make_unique<int, short, stack_pool_tag<3>>(42);
    BOOST_TEST_EQ(sizeof(p0), sizeof(short));
    BOOST_TEST_EQ(*p0, 42);

    auto p1 = make_unique<int, short, stack_pool_tag<3>>(43);
    BOOST_TEST_EQ(*p0, 42);
    BOOST_TEST_EQ(*p1, 43);

    auto p2 = make_unique<int, short, stack_pool_tag<3>>(44);
    BOOST_TEST_EQ(*p0, 42);
    BOOST_TEST_EQ(*p1, 43);
    BOOST_TEST_EQ(*p2, 44);

    BOOST_TEST_THROWS((make_unique<int, short, stack_pool_tag<3>>(45)),
                      std::bad_alloc);
  }

  {
    auto p0 = make_unique<test_type, short, stack_pool_tag<3>>(42);
    BOOST_TEST_EQ(*p0, 42);

    auto p1 = make_unique<test_type, short, stack_pool_tag<3>>(43);
    BOOST_TEST_EQ(*p0, 42);
    BOOST_TEST_EQ(*p1, 43);

    auto p2 = make_unique<test_type, short, stack_pool_tag<3>>(44);
    BOOST_TEST_EQ(*p0, 42);
    BOOST_TEST_EQ(*p1, 43);
    BOOST_TEST_EQ(*p2, 44);

    p0.reset();
    p0 = make_unique<test_type, short, stack_pool_tag<3>>(45);
    BOOST_TEST_EQ(*p0, 45);
    BOOST_TEST_EQ(*p1, 43);
    BOOST_TEST_EQ(*p2, 44);

    p0.reset();

    p0.reset(make_unique<test_type, short, stack_pool_tag<3>>(46));
    BOOST_TEST_EQ(*p0, 46);
    BOOST_TEST_EQ(*p1, 43);
    BOOST_TEST_EQ(*p2, 44);
  }
  BOOST_TEST_EQ(test_type_ctor_count, 5);
  BOOST_TEST_EQ(test_type_dtor_count, 5);

  return boost::report_errors();
}