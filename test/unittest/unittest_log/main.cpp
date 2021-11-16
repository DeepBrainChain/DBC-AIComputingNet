// entry of the test
#define BOOST_TEST_MODULE "C++ Unit Tests for log"
#include <boost/test/unit_test.hpp>
#include "add_file_log.hpp"

//全局测试夹具
struct global_fixture
{
  global_fixture() {
    std::cout<<"开始准备测试数据------->"<<std::endl;
    // add_file_log1();
    #ifdef ASYNCHRONOUS_LOG
    add_async_file_log();
    #else
    add_sync_file_log();
    #endif
  }
  virtual~global_fixture() {
    std::cout<<"清理测试环境<---------"<<std::endl;
  }
};

void log_timers() {
  for (int i = 0; i < 100; i++) {
    boost::posix_time::ptime start_, stop_;
    start_ = boost::posix_time::microsec_clock::local_time();
    log100times();
    stop_ = boost::posix_time::microsec_clock::local_time();
    float elapsed_milliseconds_ = (stop_ - start_).total_milliseconds();
    float elapsed_microseconds_ = (stop_ - start_).total_microseconds();
    std::cout << "trivial_log_test takes " << elapsed_milliseconds_ << "ms, "
      << elapsed_microseconds_ << "μs"<< std::endl;
  }
}

BOOST_GLOBAL_FIXTURE(global_fixture);
// BOOST_FIXTURE_TEST_SUITE(test_case1, global_fixture)

BOOST_AUTO_TEST_SUITE(suit1)

BOOST_AUTO_TEST_CASE(trivial_log_test) {
  boost::posix_time::ptime start_, stop_;
  start_ = boost::posix_time::microsec_clock::local_time();
  log_timers();
  stop_ = boost::posix_time::microsec_clock::local_time();
  float elapsed_milliseconds_ = (stop_ - start_).total_milliseconds();
  float elapsed_microseconds_ = (stop_ - start_).total_microseconds();
  std::cout << "all_test takes " << elapsed_milliseconds_ << "ms, "
    << elapsed_microseconds_ << "μs"<< std::endl;
}

BOOST_AUTO_TEST_SUITE_END()

// int main(int argc, char* argv[]) {
//   printf("unittest_log\n");
//   return 0;
// }
