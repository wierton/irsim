#include "irsim.h"

#include <fstream>
#include <stdio.h>
#include <string>

int main(int argc, const char *argv[]) {
  if (argc <= 1) {
    printf("usage: irsim [*.ir]\n");
    return 0;
  }

  printf("load %s\n", argv[1]);
  std::ifstream ifs(argv[1]);

  if (!ifs.good()) {
    printf("'%s' no such file\n", argv[1]);
    return 0;
  }

  using namespace irsim;
  Compiler compiler;
  auto prog = compiler.compile(ifs);
  prog->setInstsLimit(-1u);
  prog->setMemoryLimit(128 * 1024 * 1024);
  auto code = prog->run(compiler.getFunction("main"));
  printf("ret with %d, reason %d\n", code,
      (int)prog->exception);
  return 0;
}
