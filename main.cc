#include "irsim.h"
#include "fmt/printf.h"

#include <string>
#include <iostream>
#include <fstream>
#include <cstdint>

int main(int argc, const char *argv[]) {
  std::istream *is = &std::cin;
  std::ifstream ifs;

  if (argc > 1) {
    ifs.open(argv[1]);
    if (!ifs.good()) {
      fmt::fprintf(std::cerr, "'%s' no such file\n", argv[1]);
      return 0;
    } 
    fmt::printf("load %s\n", argv[1]);
    is = &ifs;
  }

  using namespace irsim;
  Compiler compiler;
  auto prog = compiler.compile(*is);
  prog->setInstsLimit(INT_MAX);
  prog->setMemoryLimit(INT_MAX);
  auto code = prog->run(compiler.getFunction("main"));
  fprintf(stderr, "ret with %d, reason %d\n", code, (int)prog->exception);
  return 0;
}
