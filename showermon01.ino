#include <SoftwareSerial.h>
#include <SPI.h>
#include <Thermistor.h>
#include "tinyswap.h"
#include "showerhead.h"
#include "nRF24L01.h"
#include "RF24.h"
#include "printf.h"

#define ELTS(x) (sizeof(x) / sizeof((x)[0]))

char EOM = 0x03;

#define START_MODE 'P'
#define BAUD 115200
#define BAUD1 57600
#define TIMEOUT2 500
#define CHECKIN 1000UL
#define SERIAL Serial

#define TEMP_PIN A0
#define NRF_CE 9
#define NRF_CSN 10
#define IRQ_PIN 3 // digital 3, atmega328 chip pin 5
#define RED 5
#define GRN 6
#define BLU 7


#define SERIAL_DELAY 1000 //mS
#define DEFAULT_DELAY 500 //mS
#define LED_ON_TIME 10 //mS
#define WIGGLE_ROOM 4000 //uS
#define PAYLOAD_DELAY 100 //ms
#define CYCLES 12

bool ok = 0; 
double temperature = 0;
uint8_t IRQcount = 0;
uint8_t rotate = 0;
uint8_t rotate_old = 0;
uint8_t pin_ary[3] = {RED, GRN, BLU};
unsigned long IRQtimer = 0;
unsigned long hallTimer = 0;
unsigned long serialTimer = 0;
unsigned long radioTimer = 0;
unsigned long ledTimer = 0;
unsigned long updateTimer = 0;
unsigned long freqTimeDelta = 0;
unsigned long urealPower = 0;
unsigned long urealTemp = 0;
unsigned long urealFreq = 0;
CircularBuffer pulseCircBuff;
Thermistor temp(TEMP_PIN);
float freq = 0;

void timer_check( void (*)(), unsigned long, int);

// ==================================== NRF ==========================
int msg[1];
int lastmsg = 1;
String theMessage = "";

RF24 radio(NRF_CE,NRF_CSN);

// Radio pipe addresses for the 2 nodes to communicate.
const uint64_t pipes[2] = { 0xF0F0F0F0E1LL, 0xF0F0F0F0D2LL };

//
// Role management
//
// Set up role.  This sketch uses the same software for all the nodes
// in this system.  Doing so greatly simplifies testing.  
//

// The various roles supported by this sketch
typedef enum { role_ping_out = 1, role_pong_back, role_power_send, role_message_send, role_message_receive, role_data_receive } role_e;

// The debug-friendly names of those roles
const char* role_friendly_name[] ={ "invalid", "Ping out", "Pong back", "Power Send"};

// The role of the current running sketch
role_e role = role_pong_back;

// the setup function runs once when you press reset or power the board
void setup() {
  init_circ_buff(&pulseCircBuff);
  SERIAL.begin(BAUD);
  // initialize digital pin 13 as an output.
  pinMode(BLINKPIN, OUTPUT);
  pinMode(RED, OUTPUT);
  pinMode(GRN, OUTPUT);
  pinMode(BLU, OUTPUT);
  flourish();
  attachInterrupt(digitalPinToInterrupt(IRQ_PIN), IRQtrigger, FALLING);

  printf_begin();
  printf("\n\rRF24/examples/GettingStarted/\n\r");
  printf("ROLE: %s\n\r",role_friendly_name[role]);
  printf("*** PRESS 'T' to begin transmitting to the other node\n\r");


  //========================= RADIO SETUP ==================
    //
  // Setup and configure rf radio
  //

  radio.begin();

  // optionally, increase the delay between retries & # of retries
  radio.setRetries(15,15);

  // optionally, reduce the payload size.  seems to
  // improve reliability
  //radio.setPayloadSize(8);

  // Open pipes to other nodes for communication

  // This simple sketch opens two pipes for these two nodes to communicate
  // back and forth.
  // Open 'our' pipe for writing
  // Open the 'other' pipe for reading, in position #1 (we can have up to 5 pipes open for reading)

  //if ( role == role_ping_out )
  {
    //radio.openWritingPipe(pipes[0]);
    radio.openReadingPipe(1,pipes[1]);
  }
  //else
  {
    //radio.openWritingPipe(pipes[1]);
    //radio.openReadingPipe(1,pipes[0]);
  }

  radio.startListening();
  // Dump the configuration of the rf unit for debugging
  radio.printDetails();
  // fix the sample code's annoying bug
  underp();
  // ================
  
}

// the loop function runs over and over again forever
void loop() {
  
  // led_timer_check(10); 
  timer_check( led_off, &ledTimer, LED_ON_TIME);
  //timer_check( gwan_rotate, IRQtimer, LED_ON_TIME);
  timer_check( default_update, &updateTimer, DEFAULT_DELAY);
  //timer_check( serial_report, &serialTimer, SERIAL_DELAY );

  // ======================= Stupid radio loop code ===================
    // ============ Power send role
  switch ( role ) {
    case role_power_send:
      timer_check( radio_report, &radioTimer, SERIAL_DELAY);
      break;

    case role_ping_out:
      ping_out;
      break;

    case role_pong_back:
      pong_back();
      break;

    case role_message_send:
      timer_check( hello_message, &radioTimer, SERIAL_DELAY);
      break;

    case role_message_receive:
      receive_string();
      break;

    case role_data_receive:
      receive_data();
      break;
  }
  if (role == role_power_send)
  {
  } // =*=*=*=*= end power send mode
  

  // === Ping out role
  if (role == role_ping_out)
  
    // === Pong back mode
  if ( role == role_pong_back )
  {}
  // =*=*= end of radio roles

  if ( Serial.available() )
  {
    char c = toupper(Serial.read());
    role_change(c);
    
  } //endif serial available

  // end of radio hax =====================
}

void timer_check(void (*your_function)(), unsigned long * timer, int msDelay) {
  // Useful all-purpose event timer
  if ((millis() - *timer) > msDelay ) {
    noInterrupts();
    (*your_function)();
    *timer = millis();
    interrupts();
  }
}

void IRQtrigger() {
  IRQcount++;
  //update_log(&freqTimeDelta, &IRQtimer);
  update_log();
  ledTimer = micros();
  digitalWrite(BLINKPIN, HIGH);
  gwan_rotate();
}

void led_off() { digitalWrite(BLINKPIN, LOW); }
void update_log() { update_pulse_buffer(&pulseCircBuff); }

// ============= Color Stuff

void gwan_rotate() {
  rotate_old = rotate;
  rotate = (rotate + 1) % CYCLES;
  
  if ( rotate % 3  == 0) {
    digitalWrite(RED, LOW);
    digitalWrite(GRN, LOW);
    digitalWrite(BLU, LOW);
  } 
  
   switch (rotate) {
    case (CYCLES / 3) - 1:
      digitalWrite(BLU, LOW);
      digitalWrite(RED, HIGH);
      break;
    case (2 * CYCLES / 3) - 1:
      digitalWrite(RED, LOW);
      digitalWrite(GRN, HIGH);
      break;
    case (3 * CYCLES / 3) - 1:
      digitalWrite(GRN, LOW);
      digitalWrite(BLU, HIGH);
      break;
   }
}

void flourish() {
  for ( int idx = 0; idx < 10; idx++) {
      digitalWrite(BLU, LOW);
      digitalWrite(RED, HIGH);
      delay(50);
      digitalWrite(RED, LOW);
      digitalWrite(GRN, HIGH);
      delay(50); 
      digitalWrite(GRN, LOW);
      digitalWrite(BLU, HIGH);
      delay(50);
  }
}

// ==================  Buffer Stuff =====================

void init_circ_buff(CircularBuffer * buff) {
  buff->elts = ELTS(buff->ary);
  for (int idx = 0; idx < buff->elts; idx++) {
    buff->ary[idx] = 0; 
  }
}

void update_pulse_buffer(CircularBuffer * buff) {
  buff->idx = (buff->idx + 1) % buff->elts;
  buff->ary[buff->idx] = micros();
}

void default_update() {
  update_pulse_buffer(&pulseCircBuff);
}

float compute_freq(CircularBuffer * buff) {
  float freq;
  int idx = buff->idx;
  int idxN1 = ni(buff->idx - 1, buff);
  int idxN2 = ni(buff->idx - 2, buff);
  unsigned long delta1 = (buff->ary[idx]) - (buff->ary[idxN1]);
  unsigned long delta2 = (buff->ary[idxN1]) - (buff->ary[idxN2]);
  if ( (sq(delta2 - delta1) < WIGGLE_ROOM) || ( sq(delta1 - SERIAL_DELAY * 1000 ) < WIGGLE_ROOM) ) {
    /* hacking squelch trick to detect if fans have stopped spinning. 
    Primary artifact is to list the frequency as 1/(update_delay), e.g. 1 hz . So this should
    cram it to 0 when the fan is spinning too slow to detect changes. 
    */
    freq = 0;
  } else {
    freq = 1000000.0 / delta1;
  }
  //if ( freq < 1.1 / (DEFAULT_DELAY * 1000 + WIGGLE_ROOM)) {
  if (freq < 1.1) {
    //other squelch
    freq = 0; 
  }
  return freq;
  
}

int ni(int idx, CircularBuffer * buff) {
  return ((idx + buff->elts)) % buff->elts;
}



// ================== Serial Stuff =======================

void serial_report() {
  freq = compute_freq(&pulseCircBuff);
  temperature = temp.getTemp();
  SERIAL.print(temperature);
  SERIAL.print(", ");
  SERIAL.println(freq);
  //Serial.println(micros());
  //gwan_rotate();
}

void role_change(char C) {
  switch ( C ) {
      case 'T':
        printf("*** CHANGING TO TRANSMIT ROLE -- PRESS 'R' TO SWITCH BACK\n\r");
        // Become the primary transmitter (ping out)
        role = role_ping_out;
        radio.openWritingPipe(pipes[0]);
        radio.openReadingPipe(1,pipes[1]);
        break;
        
      case 'R':
        printf("*** CHANGING TO RECEIVE ROLE -- PRESS 'T' TO SWITCH BACK\n\r");
        // Become the primary receiver (pong back)
        role = role_pong_back;
        radio.openWritingPipe(pipes[1]);
        radio.openReadingPipe(1,pipes[0]);
        break;
        
       case 'P':
        printf("*** CHANGING TO POWER SEND ROLE \n\r");
        // Become the primary receiver (pong back)
        role = role_power_send;
        radio.openWritingPipe(pipes[0]);
        radio.openReadingPipe(1,pipes[1]);
        break;

       case 'S':
        printf("*** CHANGING TO MESSAGE SEND ROLE \n\r");
        // Become the primary receiver (pong back)
        role = role_message_send;
        radio.openWritingPipe(pipes[0]);
        radio.openReadingPipe(1,pipes[1]);
        break;

       case 'M':
        printf("*** CHANGING TO MESSAGE RECEIVE ROLE \n\r");
        // Become the primary receiver (pong back)
        role = role_message_receive;
        radio.openWritingPipe(pipes[1]);
        radio.openReadingPipe(1,pipes[0]);
        break;
        
       case 'Z':
        printf("*** CHANGING TO DATA RECEIVE ROLE \n\r");
        // Become the primary receiver (pong back)
        role = role_data_receive;
        radio.openWritingPipe(pipes[1]);
        radio.openReadingPipe(1,pipes[0]);
        break;
    }
  
}

// ====================== Radio housekeeping functinos
void underp() {

      role_change('T');
//      role = role_ping_out;
//      radio.openWritingPipe(pipes[0]);
//      radio.openReadingPipe(1,pipes[1]);
      
  delay(100);

      role_change('R');
//      role = role_pong_back;
//      radio.openWritingPipe(pipes[1]);
//      radio.openReadingPipe(1,pipes[0]);
//      
    delay(100);

      role_change(START_MODE);
//      role = role_power_send;
//      radio.openWritingPipe(pipes[0]);
//      radio.openReadingPipe(1,pipes[1]);

    
  
} // end underp

// === radio functions
void radio_report() {
    // First, stop listening so we can talk.
    radio.stopListening();

    // Take the time, and send it.  This will block until complete
    unsigned long time = millis();
    freq = compute_freq(&pulseCircBuff);
    temperature = temp.getTemp();
    urealTemp = temperature;
    urealFreq = freq;
    printf("Now sending %lu...",urealFreq);
    urealPower = temperature;
    ok = radio.write( &urealFreq, sizeof(unsigned long) );
    if (ok)
      printf("ok...");
    else
      printf("failed.\n\r");
    delay(PAYLOAD_DELAY);
    printf("Now sending %lu...",urealTemp);
    ok = radio.write( &urealTemp, sizeof(unsigned long) );
    if (ok)
      printf("ok...");
    else
      printf("failed.\n\r");

    // Now, continue listening
    radio.startListening();

  } // =*=*=*=*= end power send mode

void pong_back (){
    // if there is data ready
    if ( radio.available() )
    {
      // Dump the payloads until we've gotten everything
      unsigned long got_time;
      bool done = false;
      while (!done)
      {
        // Fetch the payload, and see if this was the last one.
        done = radio.read( &got_time, sizeof(unsigned long) );

        // Spew it
        printf("Got payload %lu...",got_time);

  delay(20);   // Delay just a little bit to let the other unit
      }

      // First, stop listening so we can talk
      radio.stopListening();
      radio.write( &urealPower, sizeof(unsigned long) );
      printf("Sent response.\n\r");

      // Now, resume listening so we catch the next packets.
      radio.startListening();
    }
  }

void ping_out () {
    
    // First, stop listening so we can talk.
    radio.stopListening();

    // Take the time, and send it.  This will block until complete
    unsigned long time = millis();
    printf("Now sending %lu...",time);
    bool ok = radio.write( &time, sizeof(unsigned long) );
    
    if (ok)
      printf("ok...");
    else
      printf("failed.\n\r");

    // Now, continue listening
    radio.startListening();

    // Wait here until we get a response, or timeout (250ms)
    unsigned long started_waiting_at = millis();
    bool timeout = false;
    while ( ! radio.available() && ! timeout )
      if (millis() - started_waiting_at > TIMEOUT2 )
        timeout = true;

    // Describe the results
    if ( timeout )
    {
      printf("Failed, response timed out.\n\r");
    }
    else
    {
      // Grab the response, compare, and send to debugging spew
      unsigned long got_time;
      radio.read( &got_time, sizeof(unsigned long) );

      // Spew it
      printf("Got response %lu, round-trip delay: %lu\n\r",got_time,millis()-got_time);
    }
  }


void hello_message() {
  String theMessage = "Hello there!";
  radio_string(theMessage);
}
int radio_string(String theMessage) {
  //String theMessage = "Hello there!";
  // why are we doing it one char at a time?
  int messageSize = theMessage.length();
  for (int i = 0; i < messageSize; i++) {
    int charToSend[1];
    charToSend[0] = theMessage.charAt(i);
    radio.write(charToSend,1);
  }
  radio.write(&EOM,1);
  Serial.print("Sent message ");
  Serial.println(theMessage);
  return 1;
}

void receive_string(void) {
  if (radio.available()){
    bool done = false;  
      done = radio.read(msg, 1); 
      char theChar = msg[0];
      if (msg[0] != EOM){
        theMessage.concat(theChar);
        }
      else {
       Serial.println(theMessage);
       theMessage= ""; 
      }
   }
}

void receive_data (){
    // if there is data ready
    if ( radio.available() )
    {
      // Dump the payloads until we've gotten everything
      unsigned long got_time;
      bool done = false;
      while (!done)
      {
        // Fetch the payload, and see if this was the last one.
        done = radio.read( &got_time, sizeof(unsigned long) );

        // Spew it
        printf("Got payload %lu...",got_time);

        delay(20);   // Delay just a little bit to let the other unit
      }

     
    }
  }

