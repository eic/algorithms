#include <algorithms/logger.h>

using namespace algorithms;

class testAlgo : public LoggerMixin {
  public:
  testAlgo() 
    : LoggerMixin("testAlgo")
  {
    std::cout << "should display an info message" << std::endl;
    info() << "1" << endmsg;
    std::cout << "next debug message should not displaY" << std::endl;
    debug() << "2" << endmsg;
    level(LogLevel::kJunk);
    std::cout << "Changed the log level to junk, so the following message should display" << std::endl;
    debug() << "3" << endmsg;
    std::cout << "error message should display" << std::endl;
    error() << "4" << endmsg;
  }
};

int main() {
  testAlgo t;

  return 0;
}
