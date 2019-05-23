// The following two lines indicates boost test with Shared Library mode

#include "rw_lock.h"
#include <thread>

#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

using namespace matrix::core;

class counter
{
  public:
    counter() : value_(0) {}
    ~counter() = default;

    size_t get()
    {
      read_lock_guard<rw_lock> lock(m_locker);
      return value_;
    }

    void increase()
    {
      write_lock_guard<rw_lock> lock(m_locker);
      value_++;
    }

    void reset()
    {
      write_lock_guard<rw_lock> lock(m_locker);
      value_ = 0;
    }

private:
  matrix::core::rw_lock m_locker;
  size_t value_;
};

void worker(std::shared_ptr<counter> data)
{
  for (int i = 0; i < 3; i++) {
    data->increase();
    size_t value = data->get();
    (void) value;
  }
}

BOOST_AUTO_TEST_CASE(rw_lock_test)
{
  constexpr uint32_t thead_num = 1000;
  std::thread ts[thead_num];

  std::shared_ptr<counter> data = std::make_shared<counter>();

  for(uint32_t i = 0; i < thead_num; i++)
  {
    ts[i] = std::thread(worker, data);
  }

  // delay 1s
  std::this_thread::sleep_for(std::chrono::milliseconds(2000));

  for(uint32_t i = 0; i < thead_num; i++)
  {
    ts[i].join();
  }

  BOOST_TEST(data->get() == (thead_num * 3));
}
