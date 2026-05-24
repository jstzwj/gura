#include "gura/driver/Driver.h"

#include <span>

int main(int argc, char** argv) {
  gura::Driver driver;
  return driver.run(std::span<const char* const>(const_cast<const char**>(argv), static_cast<std::size_t>(argc)));
}
