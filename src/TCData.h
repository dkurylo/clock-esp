#include <Arduino.h>

class TCData {
  public:
  static const uint8_t NUMBER_OF_ANIMATIONS_SUPPORTED = 5;

    static const uint8_t* getAnimation( uint8_t index );
    static const uint16_t getAnimationSize( uint8_t index );

    static const uint8_t* getFavIcon();
    static const uint16_t getFavIconSize();
    
};