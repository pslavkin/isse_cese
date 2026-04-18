#include <iostream>
#include <stdint.h>
#include <unistd.h>

class App {

   private:
    Button  button_  ; 
    Traffic traffic_ ; 
};

class Button {
   private:
      uint8_t pin_       ; 
      bool    state_     ; 
      bool    requested_ ; 
};
class RedLed    {};
class YellowLed {};
class GreenLed  {};

class Traffic {

   private:
    uint8_t   currentState_ ;
    uint32_t  stateTime_    ;
    RedLed    red_          ;
    YellowLed yellow_       ;
    GreenLed  green_        ;

   public:
    static uint32_t GREEN_TIME  ; 
    static uint32_t YELLOW_TIME ; 
    static uint32_t RED_TIME    ; 
    static uint32_t BLINKY_TIME ; 

};
class Timer {};

class Led {
   protected:
    uint8_t pin_   ; 
    bool    state_ ; 
};

int main(void) 
{
   std::cout << "Hello World!" << std::endl;
}
