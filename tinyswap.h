#if defined (__AVR_ATtiny85__)
  #define BLINKPIN 4
  //#define SERIAL SoftwareSerial mySerial(3,1)
  
#else
  #define BLINKPIN 13
  //#define SERIAL Serial
#endif
