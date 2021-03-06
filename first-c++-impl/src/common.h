#ifndef __COMMON_H__
#define __COMMON_H__

#include <vector>
#include <string>
#include <utility>
#include <sstream>
#include <set>
#include <map>
#include <boost/shared_ptr.hpp>
#include <boost/bind.hpp>
#include <boost/noncopyable.hpp>
#include <stdexcept>

#define PTR boost::shared_ptr

namespace pants {

  struct expectation_failure : std::runtime_error {
    expectation_failure(const std::string& msg_) throw ()
      : std::runtime_error(msg_) {}
    ~expectation_failure() throw() {}
  };

}

#endif
