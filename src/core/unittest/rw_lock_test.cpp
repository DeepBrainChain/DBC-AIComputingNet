// The following two lines indicates boost test with Shared Library mode
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>


using namespace boost::unit_test;
using namespace std;





#include <boost/thread/locks.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <boost/thread/strict_lock.hpp>

BOOST_AUTO_TEST_CASE(boost_rwlock_test) {

    boost::shared_mutex _access;

    {
        boost::strict_lock<boost::shared_mutex> x(_access);
        //auto y = x ; //new boost::strict_lock<boost::shared_mutex>(_access);

    }

    {
        boost::shared_lock<boost::shared_mutex> lock1(_access);
        boost::shared_lock<boost::shared_mutex> lock2(_access);
    }

    {
        boost::shared_lock<boost::shared_mutex> lock1(_access);
//        boost::unique_lock< boost::shared_mutex > lock2(_access);
//        boost::unique_lock< boost::shared_mutex > lock3(_access);
    }

    {
        boost::upgrade_lock<boost::shared_mutex> lock(_access);                 // shared ownership
        //...
        boost::upgrade_to_unique_lock<boost::shared_mutex> unique_lock(lock);   // upgrade to exclusive ownership
    }

}



#include "rw_lock.h"

BOOST_AUTO_TEST_CASE(rwlock2_test) {
    matrix::core::rw_lock lock;
    lock.read_lock();
    lock.read_lock();

    // write lock will be blocked if any read lock exists.
    //lock.write_lock();
}