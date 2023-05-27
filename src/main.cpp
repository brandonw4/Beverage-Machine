#include "Machine.hpp"
#include <Arduino.h>


Machine bevMaker;

void setup() {
  bevMaker.boot();
}

void loop() {
    /*
    if there is serial input (either from debug console or from touchscreen), run the decision tree
    */
   //bevMaker.touchscreen.checkForInput();
}


