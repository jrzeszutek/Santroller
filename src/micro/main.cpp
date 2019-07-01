#include "../shared/controller/input/InputHandler.h"
#include "../shared/controller/output/OutputHandler.h"
#include "../shared/sharedmain.h"
InputHandler controller;
OutputHandler out;
Main main2;

int main() {
  main2.main();
  out.init();
  controller.init();
  while (true) {
    controller.process();
    out.process(&controller.controller);
  }
}
void configChangeHandler() {}