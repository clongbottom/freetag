//#include "IRReadOnlyRemote.h"
// #include "Tone.cpp"

//IRReadOnlyRemote remote(2);

#include <EEPROM.h>

#include <SPI.h>
//#include <RH_RF69.h>
#include "LoRa.h"

//RH_RF69 rf69;

#include <Adafruit_NeoPixel.h>
//#ifdef __AVR__
//  #include <avr/power.h>
//#endif
//
//// Which pin on the Arduino is connected to the NeoPixels?
//// On a Trinket or Gemma we suggest changing this to 1
#define PIXELPIN              12
//
//// How many NeoPixels are attached to the Arduino?
#define NUMPIXELS             24

#define MAX_USERS             12


#define IR_BUFLEN             40
//
//// When we setup the NeoPixel library, we tell it how many pixels, and which pin to use to send signals.
//// Note that for older NeoPixel strips you might need to change the third parameter--see the strandtest
//// example for more information on possible values.

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, PIXELPIN,    NEO_GRB + NEO_KHZ800);
//Adafruit_NeoPixel pixels_team = Adafruit_NeoPixel(NUMPIXELS, PIXELPIN_PLAYERS, NEO_GRB + NEO_KHZ800);

//#define TEAMPIN_A             5
//#define TEAMPIN_B             6
//#define TEAMPIN_C             9

int TEAMPIN_A = 5;
int TEAMPIN_B = 6;
int TEAMPIN_C = 9;

//      Start of code (copy and paste into arduino sketch)
//
//                       Duino Tag release V1.01
//          Laser Tag for the arduino based on the Miles Tag Protocol.
//           By J44industries:     www.J44industries.blogspot.com
// For information on building your own Duino Tagger go to: https://www.instructables.com/member/j44/
//
// Much credit deserves to go to Duane O'Brien if it had not been for the excellent Duino Tag tutorials he wrote I would have never been able to write this code.
// Duane's tutorials are highly recommended reading in order to gain a better understanding of the arduino and IR communication. See his site http://aterribleidea.com/duino-tag-resources/
//
// This code sets out the basics for arduino based laser tag system and tries to stick to the miles tag protocol where possible.
// Miles Tag details: http://www.lasertagparts.com/mtdesign.htm
// There is much scope for expanding the capabilities of this system, and hopefully the game will continue to evolve for some time to come.
// Licence: Attribution Share Alike: Give credit where credit is due, but you can do what you like with the code.
// If you have code improvements or additions please go to http://duinotag.blogspot.com
//

//int trigger2Pin            = 13;     // Push button for secondary fire. Low = pressed
//int audioPin               = 9;      // Audio Trigger. Can be used to set off sounds recorded in the kind of electronics you can get in greetings card that play a custom message.
//int lifePin                = 6;      // An analogue output (PWM) level corresponds to remaining life. Use PWM pin: 3,5,6,9,10 or 11. Can be used to drive LED bar graphs. eg LM3914N
//int ammoPin                = 5;      // An analogue output (PWM) level corresponds to remaining ammunition. Use PWM pin: 3,5,6,9,10 or 11.
//int hitPin                 = 7;      // LED output pin used to indicate when the player has been hit.
//int IRtransmit2Pin         = 8;      // Secondary fire mode IR transmitter pin:  Use pins 2,4,7,8,12 or 13. DO NOT USE PWM pins!!


// Digital IO's
byte triggerPin      = 3;      // Push button for primary fire. Low = pressed
byte speakerPin      = 11;      // Direct output to piezo sounder/speaker
byte IRtransmitPin   = 13;      // Primary fire mode IR transmitter pin: Use pins 2,4,7,8,12 or 13. DO NOT USE PWM pins!! More info: http://j44industries.blogspot.com/2009/09/arduino-frequency-generation.html#more
byte IRreceivePin    = 2;     // The pin that incoming IR signals are read from
byte IRreceive2Pin   = 10;     // Allows for checking external sensors are attached as well as distinguishing between sensor locations (eg spotting head shots)
// Minimum gun requirements: trigger, receiver, IR led, hit LED.

// Player and Game details
byte myTeamID               = 1;      // 1-7 (0 = system message)

byte guncolour_r = 0;
byte guncolour_g = 0;
byte guncolour_b = 0;

byte myPlayerID             = 1;      // Player ID
byte myGameID               = 1;      // Interprited by configureGane subroutine; allows for quick change of game types.
byte myWeaponID             = 0;      // Deffined by gameType and configureGame subroutine.
byte myWeaponHP             = 0;      // Deffined by gameType and configureGame subroutine.
int maxAmmo                = 0;      // Deffined by gameType and configureGame subroutine.
int maxLife                = 0;      // Deffined by gameType and configureGame subroutine.
byte automatic              = 0;      // Deffined by gameType and configureGame subroutine. Automatic fire 0 = Semi Auto, 1 = Fully Auto.
//int automatic2             = 0;      // Deffined by gameType and configureGame subroutine. Secondary fire auto?

//Incoming signal Details
byte recv[18];                    // Received data: received[0] = which sensor, received[1] - [17] byte1 byte2 parity (Miles Tag structure)
byte recvTmp[18];                  // Temp data
byte check                  = 0;      // Variable used in parity checking

// Stats
int ammo                   = 0;      // Current ammunition
byte life                   = 0;      // Current life

// Code Variables
int timeOut                = 0;      // Deffined in frequencyCalculations (IRpulse + 50)
int FIRE                   = 0;      // 0 = don't fire, 1 = Primary Fire, 2 = Secondary Fire
int TR                     = 0;      // Trigger Reading
int LTR                    = 0;      // Last Trigger Reading
//int T2R                    = 0;      // Trigger 2 Reading (For secondary fire)
//int LT2R                   = 0;      // Last Trigger 2 Reading (For secondary fire)

// Signal Properties
int IRpulse                = 600;    // Basic pulse duration of 600uS MilesTag standard 4*IRpulse for header bit, 2*IRpulse for 1, 1*IRpulse for 0.
int IRfrequency            = 38;     // Frequency in kHz Standard values are: 38kHz, 40kHz. Choose dependant on your receiver characteristics
int IRt                    = 0;      // LED on time to give correct transmission frequency, calculated in setup.
int IRpulses               = 0;      // Number of oscillations needed to make a full IRpulse, calculated in setup.
byte header                 = 4;      // Header lenght in pulses. 4 = Miles tag standard
byte maxSPS                 = 10;     // Maximum Shots Per Seconds. Not yet used.
byte TBS                    = 0;      // Time between shots. Not yet used.

// Transmission data
int byte1[8];                        // String for storing byte1 of the data which gets transmitted when the player fires.
int byte2[8];                        // String for storing byte1 of the data which gets transmitted when the player fires.
int myParity               = 0;      // String for storing parity of the data which gets transmitted when the player fires.

// Received data
byte memory                 = 3;     // Number of signals to be recorded: Allows for the game data to be reviewed after the game, no provision for transmitting / accessing it yet though.
byte hitNo                  = 0;      // Hit number
// Byte1
int player[3];                      // Array must be as large as memory
byte team[3];                        // Array must be as large as memory
// Byte2
byte weapon[3];                      // Array must be as large as memory
int hp[3];                          // Array must be as large as memory
byte parity[3];                      // Array must be as large as memory

unsigned long userId = 71234L;
// unsigned long userId = 112049L;

// How to keep track of all users?
// byte userIds[12][4];

unsigned long user_ids[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
byte user_healths[12];
byte user_teams[12];

byte team_a = 0;
byte team_b = 0;
byte team_c = 0;

bool team_a_in = false;
bool team_b_in = false;
bool team_c_in = false;

static bool debug = 1;                 // Debug mode?

unsigned long last_message_send = 0;
unsigned long last_fire_time = 0;
volatile long nowTime = 0;

volatile unsigned long lastHitTime = 0;
volatile unsigned long lastFireTime = 0;
volatile bool triggerPressed = false;

volatile unsigned long irTime = 0;
volatile unsigned long irTimeAny = 0;
// volatile unsigned long irTimeMicros = 0;
volatile unsigned long irElapsed = 0;

volatile bool irPulseState = false;
volatile int  irMsgLen = 0;
volatile int  irMsgLenFinal = 0;
volatile byte irMsg[40];
volatile bool irMsgAvailable = false;

         byte irMsgFinal[40];


volatile bool firstFire = true;

volatile bool irRecv = false;
volatile bool irError = false;
volatile bool irState = 0;

bool flipper = 0;

byte radioBuf[16];

void setup() {

    Serial.begin(9600);
    while (!Serial);

    // Serial.println("Testing eeprom read");

    // byte eeprom_read[4];

    // for(byte ee=0; ee<4; ee++) {
    //  eeprom_read[ee] = EEPROM.read(ee);
    //  Serial.println(eeprom_read[ee]);
    // }

    // Serial.println("Finished eeprom read");

    unsigned long userTmp_1 = EEPROM.read(0);

    userTmp_1 =  userTmp_1 << 8;
    userTmp_1 += EEPROM.read(1);
    userTmp_1 =  userTmp_1 << 8;
    userTmp_1 += EEPROM.read(2);
    userTmp_1 =  userTmp_1 << 8;
    userTmp_1 += EEPROM.read(3);

    // 0b11111111111111111111111111111111
    // 0xFFFFFFFF

    if(userTmp_1 == 0xFFFFFFFF || userTmp_1 == 0xFF) {

        Serial.println(F("Enter user ID:"));
        // byte serialBuf[16];
        String serialBuf;
        char charArray[16];
        char charCount = 0;

        // unsigned long userTmp_2 = 0;

        // Wait for user input...
        while ( !Serial.available());

        userTmp_1 = Serial.parseInt();

        // while (Serial.available() > 0) {
        //  // userTmp_1 = userTmp_1 + Serial.read() << 24;
        //  // serialBuf[charCount] = Serial.read();
        //  charCount++;
        // }

        // serialBuf.toCharArray(charArray, sizeof(charArray));
  //       userTmp_1 = atol(charArray);

        Serial.print(F("Got userTmp_1 as "));
        Serial.println(userTmp_1);

        if(userTmp_1 > 0) {
            Serial.print(F("Writing EEPROM"));
            Serial.println(userTmp_1);
            EEPROM.write(0, (userTmp_1 >> 24) & 0xFF);
            EEPROM.write(1, (userTmp_1 >> 16) & 0xFF);
            EEPROM.write(2, (userTmp_1 >>  8) & 0xFF);
            EEPROM.write(3, (userTmp_1 >>  0) & 0xFF);
        }

        userId = userTmp_1;

    } else {

        Serial.println(F("EEPROM says user ID is "));
        Serial.println(userTmp_1, DEC);
        userId = userTmp_1;
    }

    Serial.println(F("Starting up..."));
    Serial.print(userId);
    Serial.println("...");

    LoRa.setPins(8,4,7);
    if (!LoRa.begin(434E6)) {
        Serial.println(F("Starting LoRa failed!"));
    // while (1);
    } else {
        Serial.println("Started LoRa");
    }


    nowTime = millis();
  // last_message_send = nowTime;

  // Serial coms set up to help with debugging.
    // Serial.begin(9600);
    if (debug) {
        Serial.print(F("Startup "));
    }


  // Pin declarations
    pinMode(triggerPin, INPUT_PULLUP);
    pinMode(IRreceivePin, INPUT_PULLUP);

    pinMode(IRtransmitPin, OUTPUT);
    digitalWrite(IRtransmitPin, LOW);
    digitalWrite(triggerPin, HIGH);

    attachInterrupt(digitalPinToInterrupt(IRreceivePin), isr_ir,        CHANGE);
    attachInterrupt(digitalPinToInterrupt(triggerPin),   isr_trigger,   CHANGE);

    Serial.println("Enabling pixels...");

  // Start neopixels...
    pixels.begin();
    Serial.println("Enabled pixels...");

//  digitalWrite(TEAMPIN_A, HIGH);
//  digitalWrite(TEAMPIN_B, HIGH);
//  digitalWrite(TEAMPIN_C, HIGH);

    pinMode(speakerPin, OUTPUT);
    digitalWrite(speakerPin, HIGH); 
    Serial.println("Setup speaker pin...");



    for (byte j = 0; j < NUMPIXELS; j++) {
        pixels.setPixelColor(j, 20, 20, 20);
    }
    update_pixels();

    Serial.println("Done first pixel update");

    // everyone_dead();

    Serial.println("Done everyone dead...");

    for (byte j = 0; j < NUMPIXELS; j++) {
        pixels.setPixelColor(j, 20, 20, 20);
    }
    update_pixels();

    Serial.println("Updated pixels...");

  // Startup noise...
    for (int i = 1; i < 254; i++) {
        playTone((3000 - 9 * i), 2);
    }

    Serial.println("Done noises etc...");

    pinMode(TEAMPIN_A, INPUT_PULLUP);
    pinMode(TEAMPIN_B, INPUT_PULLUP);
    pinMode(TEAMPIN_C, INPUT_PULLUP);

    int read_a = HIGH;
    int read_b = HIGH;
    int read_c = HIGH;

    Serial.println("Waiting for a team selection...");

  // Wait for any read of a team button...
    while ( read_a == HIGH && read_b == HIGH && read_c == HIGH ) {
        read_a = digitalRead(TEAMPIN_A);
        read_b = digitalRead(TEAMPIN_B);
        read_c = digitalRead(TEAMPIN_C);
    }

    lastFireTime = millis();

    Serial.print("Chosen team: ");

    if (read_a == LOW) {
        myTeamID = 1;
    } else if (read_b == LOW) {
        myTeamID = 2;
    } else if (read_c == LOW) {
        myTeamID = 3;
    } else {
        myTeamID = 1;
    }

    Serial.println(myTeamID);

    if (myTeamID == 1) {
        guncolour_r = 128;
    } else if (myTeamID == 2) {
        guncolour_g = 128;
    } else if (myTeamID == 3) {
        guncolour_b = 128;
    }

    for (byte j = 0; j < 12; j++) {
        pixels.setPixelColor(j, guncolour_r, guncolour_g, guncolour_b);
    }
    for (byte j = 12; j < 24; j++) {
        pixels.setPixelColor(j, 0, 0, 0);
    }
    update_pixels();

  // Startup noise...
    for (int i = 1; i < 254; i++) {
        playTone((3000 - 9 * i), 2);
    }


  frequencyCalculations();   // Calculates pulse lengths etc for desired frequency
  configureGame();           // Look up and configure game details
  tagCode();                 // Based on game details etc works out the data that will be transmitted when a shot is fired

  digitalWrite(triggerPin, HIGH);      // Not really needed if your circuit has the correct pull up resistors already but doesn't harm

  lifeDisplay();

  //if (!rf69.init())
  //  Serial.println(F("init failed"));
  
  // Defaults after init are 434.0MHz, modulation GFSK_Rb250Fd250, +13dbM (for low power module)
  // No encryption
  
  //if (!rf69.setFrequency(434.550))
  //  Serial.println(F("setFrequency failed"));

  // If you are using a high power RF69 eg RFM69HW, you *must* set a Tx power with the
  // ishighpowermodule flag set like this:
  // rf69.setTxPower(20, true);

}

/// blah

// Main loop most of the code is in the sub routines
void loop() {

    nowTime = millis();

    // IR DEBUG - prints all IR stuff as it comes in...
    if(irRecv) {
        irTimeAny = millis();
        irRecv = false;
        
        if(irError) {
            irError = false;
            if(debug) {
                Serial.print("IR [ERR] ");
                Serial.print(irElapsed);
                Serial.print(" ");
                Serial.println(irPulseState);
            }
        } else {
            if(debug) {
                Serial.print("IR ..... ");
                Serial.print(irElapsed);
                Serial.print(" ");
                Serial.println(irPulseState);
            }
        }

    }


    if(irMsgLen >= 17 && ( millis() - irTimeAny ) > 50) {

        Serial.println("");
        Serial.print("Got enough length to interpret: ");
        Serial.println(irMsgLen);

        for(byte i=0; i<irMsgLen; i++) { //irMsgLenFinal
            irMsgFinal[i] = irMsg[i];
        }

        interpret2();
    }

    if ( (millis() - irTimeAny ) > 100 && irMsgLen > 0 ) {

        if(debug) {
            Serial.println("");
            Serial.print("Resetting IR receiver, len was ");
            Serial.println(irMsgLen);

            for(byte i=0; i<irMsgLen; i++) { //irMsgLenFinal
                Serial.print(irMsg[i]);
            }
            Serial.println("");
        }
        
        irMsgLen = 0;
    }


    // Max fire time is 500ms, meaning most of the time we are listening for being fired upon!
    if (triggerPressed) { //  and nowTime - last_fire_time > 500
        
        if (life > 0) {

            Serial.println("FIRE! :-)");
            last_fire_time = nowTime;
            FIRE = 1;
            shoot();
        
        }

        triggerPressed = false;

        // shoot(); // Shoot twice?
    }

    //  if (rf69.available()) {
    //    if (debug)
    //      Serial.println(F("There is a message from another player"));
    //    recvRadio();
    //  }

    int packetSize = LoRa.parsePacket();
    if (packetSize) {
        recvRadio2();
    }

    if (nowTime - last_message_send > 10000) {
        byte bytes[5];

        bytes[0] = (userId >> 24) & 0xFF;
        bytes[1] = (userId >> 16) & 0xFF;
        bytes[2] = (userId >>  8) & 0xFF;
        bytes[3] = (userId >>  0) & 0xFF;
        bytes[4] = ((myTeamID & 0x0F) << 4) | (life & 0x0F); // This byte is health and 

        //rf69.send(bytes, sizeof(bytes));
        
        LoRa.beginPacket();
        LoRa.write(bytes[0]);
        LoRa.write(bytes[1]);
        LoRa.write(bytes[2]);
        LoRa.write(bytes[3]);
        LoRa.write(bytes[4]);
        LoRa.endPacket();

        // Serial.println("Sent a message!");
        
        last_message_send = nowTime;
    }

}

void update_pixels() {
    detachInterrupt(digitalPinToInterrupt(IRreceivePin));
    detachInterrupt(digitalPinToInterrupt(triggerPin));
    pixels.show();
    attachInterrupt(digitalPinToInterrupt(IRreceivePin), isr_ir,        CHANGE);
    attachInterrupt(digitalPinToInterrupt(triggerPin),   isr_trigger,   CHANGE);
}


// SUB ROUTINES

void isr_ir() {

    irRecv = true;
    // irTimeAny = micros();

    // read instead of toggle :-)
    irPulseState = digitalRead(IRreceivePin);

    if ( !irPulseState) {
        // irPulseState = true;
        irTime = micros();

    } else {

        irElapsed = micros()-irTime;
        if ( irElapsed > (IRpulse+IRpulse-300) && irElapsed < (IRpulse+IRpulse+300) ) {
            // irPulseState = false;
            irMsg[irMsgLen] = 0;
            irMsgLen++;
        } else if ( irElapsed > (IRpulse+IRpulse+IRpulse+IRpulse-300) && irElapsed < IRpulse+IRpulse+IRpulse+IRpulse+300 ) {
            irPulseState = false;
            irMsg[irMsgLen] = 1;
            irMsgLen++;
        } else {
            irError = true;
        }

    }

    if( irMsgLen > IR_BUFLEN ) {
        irMsgLen = 0;
    }

}

void isr_trigger() {

    unsigned long elapsed = millis()-lastFireTime;

    if ( elapsed > 500 ) {
        lastFireTime = millis();
        triggerPressed = true;
    }
}

void recvRadio2() {

    int count = 0;
    byte b;
    while (LoRa.available()) {
        b = (byte) LoRa.read();
        radioBuf[count] = b;
        // Serial.println((int) b);
        count++;
    }

    // print RSSI of packet
    // Serial.print("' with RSSI ");
    // Serial.println(LoRa.packetRssi());

    parseRadioData();
}

void parseRadioData() {

    unsigned long this_user = radioBuf[0];
    this_user = this_user << 8;
    this_user += radioBuf[1];
    this_user = this_user << 8;
    this_user += radioBuf[2];
    this_user = this_user << 8;
    this_user += radioBuf[3];

    Serial.print("RECV user: ");
    Serial.print(this_user);

    byte this_team = (radioBuf[4] & 0xF0) >> 4;
    byte this_health = (radioBuf[4] & 0x0F);

    Serial.print(", RECV team:");
    Serial.print(this_team);

    Serial.print(", RECV hlth:");
    Serial.print(this_health);

    Serial.print(", IR len: ");
    Serial.println(irMsgLen);

  // Figure out if user is in database, if not add them!
    byte tmp_user = 0;
    bool new_user = true;

    for (byte j = 0; j < MAX_USERS; j++) {
        
        // Serial.print(user_ids[j]);
        // Serial.print(" vs ");
        // Serial.println(this_user);

        // We've found our user, break!
        if (user_ids[j] == this_user) {
            // Serial.print("FOUND USER: ");
            // Serial.println(j);
            tmp_user = j;
            new_user = false;
            user_healths[tmp_user] = this_health;
            break;
        }
        // In each loop, increment tmp_user till it's empty...
        if (user_ids[j] != 0) {
            tmp_user++;
        } else {
            break;
        }
    }

    if( ! new_user ) {
        // Serial.println("Broke out of for loop");
    }

    if (new_user) { // add them to the team count!

        Serial.println("New user found! Welcome!");

        user_ids[tmp_user] = this_user;
        
        if (this_team == 1) {
            if (!team_a_in)
                team_a_in = true;
            team_a++;
        } else if (this_team == 2) {
            if (!team_b_in)
                team_b_in = true;
            team_b++;
        } else if (this_team == 3) {
            if (!team_c_in)
                team_c_in = true;
            team_c++;
        }

        user_teams[tmp_user] = this_team;
        user_healths[tmp_user] = this_health;

    } else {
        // Serial.println("Not new user, skip new_use section...");
    }

  setTeamSize();
  lifeDisplay();

}

void setTeamSize() {

    byte team_tmp_a = 0;
    byte team_tmp_b = 0;
    byte team_tmp_c = 0;

    team_tmp_a = 0;
    for (byte j = 0; j<MAX_USERS; j++) {
        if (user_ids[j] != 0 && user_healths[j] > 0 && user_teams[j] == 1) {
            team_tmp_a++;
        }
    }
    team_a = team_tmp_a;

    team_tmp_b = 0;
    for (byte j = 0; j<MAX_USERS; j++) {
        if (user_ids[j] != 0 && user_healths[j] > 0 && user_teams[j] == 2) {
            team_tmp_b++;
        }
    }
    team_b = team_tmp_b;

    team_tmp_c = 0;
    for (byte j = 0; j<MAX_USERS; j++) {
        if (user_ids[j] != 0 && user_healths[j] > 0 && user_teams[j] == 3) {
            team_tmp_c++;
        }
    }
    team_c = team_tmp_c;

}


//void ammoDisplay() { // Updates Ammo LED output
//  float ammoF;
//  ammoF = (260/maxAmmo) * ammo;
//  if(ammoF <= 0){ammoF = 0;}
//  if(ammoF > 255){ammoF = 255;}
//  analogWrite(ammoPin, ((int) ammoF));
//}

void everyone_dead() {

    // return;

    for (byte m = 0; m < 5; m++) {
        for (byte p = 0; p < 255; p++) {
            for (byte j = 0; j < NUMPIXELS; j++) {
                pixels.setPixelColor(j, p, p, p);
            }
            // update_pixels();
            pixels.show();
            delayMicroseconds(100);
        }
        for (int k = 255; k >= 0; k--) {
            for (byte n = 0; n < NUMPIXELS; n++) {
                pixels.setPixelColor(n,k,k,k);
            }
            // update_pixels();
            pixels.show();
            delayMicroseconds(100);
        }
        Serial.println(m);
    }
    Serial.println("Finished everyone dead...");
    // update_pixels();
    // Serial.begin(9600);

}


void lifeDisplay() { // Updates Ammo LED output

  //  float lifeF;
  //  lifeF = (260/maxLife) * life;
  //  if(lifeF <= 0){lifeF = 0;}
  //  if(lifeF > 255){lifeF = 255;}
  //  analogWrite(lifePin, ((int) lifeF));

  // This is the "dead" sequence....
    if ( (team_a_in or team_b_in or team_c_in) and team_a + team_b + team_c == 0) {

        Serial.println("Everyone dead..........");
        everyone_dead();


    } else {

        if (team_a_in || team_b_in || team_c_in) {

      // Stack up the leds for the other teams...
            for (byte j = 0; j < MAX_USERS; j++) {
                if (team_a > j) {
                    pixels.setPixelColor(j + 12, 160, 0, 0);
                }
            }

            for (byte j = 0; j < MAX_USERS; j++) {
                if (team_b > j) {
                    pixels.setPixelColor(j + 12 + team_a, 0, 160, 0);
                }
            }

            for (byte j = 0; j < MAX_USERS; j++) {
                if (team_c > j) {
                    pixels.setPixelColor(j + 12 + team_a + team_b, 0, 0, 160);
                }
            }

            // Kill the LEDs for dead combatants
            for (byte i = 12 + team_a + team_b + team_c; i < NUMPIXELS; i++) {
                pixels.setPixelColor(i, 0, 0, 0);
            }

        } else {
            // Show no teams are yet registered, with entirely white LEDS
            for (byte j = 12; j < NUMPIXELS; j++) {
                pixels.setPixelColor(j, 160, 160, 160);
            }
        }

    // Loop through health LEDs...
        for (byte i = 0; i < 12; i++) {
            if (i > life) {
        pixels.setPixelColor(i, 0, 0, 0); // Moderately bright green color.
    } else {
        pixels.setPixelColor(i, pixels.Color(guncolour_r, guncolour_g, guncolour_b)); // Moderately bright green color.
    }
      update_pixels(); // This sends the updated pixel color to the hardware.
  }

}
}



void interpret2() {

    Serial.println("interpret2 called!");

    if (hitNo == memory) {
        hitNo = 0;
    }

    team[hitNo] = 0;
    player[hitNo] = 0;
    weapon[hitNo] = 0;
    hp[hitNo] = 0;


    team[hitNo]   = (irMsgFinal[1]  << 2) + (irMsgFinal[2]  << 1) + (irMsgFinal[3]  << 0);
    player[hitNo] = (irMsgFinal[4]  << 4) + (irMsgFinal[5]  << 3) + (irMsgFinal[6]  << 2) + (irMsgFinal[7]  << 1) + (irMsgFinal[8]  << 0);
    weapon[hitNo] = (irMsgFinal[9]  << 2) + (irMsgFinal[10] << 1) + (irMsgFinal[11] << 0);
    hp[hitNo]     = (irMsgFinal[12] << 4) + (irMsgFinal[13] << 3) + (irMsgFinal[14] << 2) + (irMsgFinal[15] << 1) + (irMsgFinal[16] << 0);
    parity[hitNo] = (irMsgFinal[17]);

    Serial.print(F("Hit No: "));
    Serial.print(hitNo);
    Serial.print(F("  Player: "));
    Serial.print(player[hitNo]);
    Serial.print(F("  Team: "));
    Serial.print(team[hitNo]);
    Serial.print(F("  Weapon: "));
    Serial.print(weapon[hitNo]);
    Serial.print(F("  HP: "));
    Serial.print(hp[hitNo]);
    Serial.print(F("  Parity: "));
    Serial.println(parity[hitNo]);

    //This is probably where more code should be added to expand the game capabilities at the moment the code just checks that the received data was not a system message and deducts a life if it wasn't.
    // Check it's not the same team ID..! Otherwise anybody can shoot anybody (including yourself)
    if (player[hitNo] != 0 && team[hitNo] != myTeamID && team[hitNo] < 12 && hp[hitNo] < 12 && hp[hitNo] > 0) {
        hit();
    }

    hitNo++;

    lifeDisplay();

}

void shoot() {
  if (FIRE == 1) { // Has the trigger been pressed?

    if (debug) {
        Serial.println(F("FIRE 1"));
    }

    sendPulse(IRtransmitPin, 4); // Transmit Header pulse, send pulse subroutine deals with the details
    delayMicroseconds(IRpulse);

    for (byte i = 0; i < 8; i++) { // Transmit Byte1
        if (byte1[i] == 1) {
            sendPulse(IRtransmitPin, 1);
        //Serial.print("1 ");
        }
      //else{Serial.print("0 ");}
        sendPulse(IRtransmitPin, 1);
        delayMicroseconds(IRpulse);
    }

    for (byte i = 0; i < 8; i++) { // Transmit Byte2
        if (byte2[i] == 1) {
            sendPulse(IRtransmitPin, 1);
        // Serial.print("1 ");
        }
      //else{Serial.print("0 ");}
        sendPulse(IRtransmitPin, 1);
        delayMicroseconds(IRpulse);
    }

    if (myParity == 1) { // Parity
        sendPulse(IRtransmitPin, 1);
    }
    sendPulse(IRtransmitPin, 1);
    delayMicroseconds(IRpulse);

    if (debug) {
        Serial.println("");
        Serial.println(F("DONE 1"));
    }
}

FIRE = 0;

  // Decrement ammo - ignore for now...
  // ammo = ammo - 1;
}


void sendPulse(int pin, int length) { // importing variables like this allows for secondary fire modes etc.
  // This void genertates the carrier frequency for the information to be transmitted
    int i = 0;
    int o = 0;
    while ( i < length ) {
        i++;
        while ( o < IRpulses ) {
            o++;
            digitalWrite(pin, HIGH);
            delayMicroseconds(IRt);
            digitalWrite(pin, LOW);
            delayMicroseconds(IRt);
        }
    }
}

void configureGame() { // Where the game characteristics are stored, allows several game types to be recorded and you only have to change one variable (myGameID) to pick the game.
    if (myGameID == 0) {
        myWeaponID = 1;
        maxAmmo = 30;
        ammo = 30;
        maxLife = 3;
        life = 3;
        myWeaponHP = 1;
    }
    if (myGameID == 1) {
        myWeaponID = 1;
        maxAmmo = 1000;
        ammo = 1000;
        maxLife = 12;
        life = 12;
        myWeaponHP = 1;
    }
}


void frequencyCalculations() { // Works out all the timings needed to give the correct carrier frequency for the IR signal
    IRt = (int) (500 / IRfrequency);
    IRpulses = (int) (IRpulse / (2 * IRt));
    IRt = IRt - 4;
  // Why -4 I hear you cry. It allows for the time taken for commands to be executed.
  // More info: http://j44industries.blogspot.com/2009/09/arduino-frequency-generation.html#more

    if (debug) {
        Serial.print(F("Oscilation time period /2: "));
        Serial.println(IRt);
        Serial.print(F("Pulses: "));
        Serial.println(IRpulses);
    }
  timeOut = IRpulse + 50; // Adding 50 to the expected pulse time gives a little margin for error on the pulse read time out value
}


void tagCode() { // Works out what the players tagger code (the code that is transmitted when they shoot) is
    byte1[0] = myTeamID >> 2 & B1;
    byte1[1] = myTeamID >> 1 & B1;
    byte1[2] = myTeamID >> 0 & B1;

    byte1[3] = myPlayerID >> 4 & B1;
    byte1[4] = myPlayerID >> 3 & B1;
    byte1[5] = myPlayerID >> 2 & B1;
    byte1[6] = myPlayerID >> 1 & B1;
    byte1[7] = myPlayerID >> 0 & B1;


    byte2[0] = myWeaponID >> 2 & B1;
    byte2[1] = myWeaponID >> 1 & B1;
    byte2[2] = myWeaponID >> 0 & B1;

    byte2[3] = myWeaponHP >> 4 & B1;
    byte2[4] = myWeaponHP >> 3 & B1;
    byte2[5] = myWeaponHP >> 2 & B1;
    byte2[6] = myWeaponHP >> 1 & B1;
    byte2[7] = myWeaponHP >> 0 & B1;

    myParity = 0;
    for (int i = 0; i < 8; i++) {
        if (byte1[i] == 1) {
            myParity = myParity + 1;
        }
        if (byte2[i] == 1) {
            myParity = myParity + 1;
        }
        myParity = myParity >> 0 & B1;
    }

    if (debug) {
    // Next few lines print the full tagger code.
        Serial.print(F("Byte1: "));
        Serial.print(byte1[0]);
        Serial.print(byte1[1]);
        Serial.print(byte1[2]);
        Serial.print(byte1[3]);
        Serial.print(byte1[4]);
        Serial.print(byte1[5]);
        Serial.print(byte1[6]);
        Serial.print(byte1[7]);
        Serial.println();

        Serial.print(F("Byte2: "));
        Serial.print(byte2[0]);
        Serial.print(byte2[1]);
        Serial.print(byte2[2]);
        Serial.print(byte2[3]);
        Serial.print(byte2[4]);
        Serial.print(byte2[5]);
        Serial.print(byte2[6]);
        Serial.print(byte2[7]);
        Serial.println();

        Serial.print(F("Parity: "));
        Serial.print(myParity);
        Serial.println();
    }
}

void playTone(int tone, int duration) { // A sub routine for playing tones like the standard arduino melody example
    for (long i = 0; i < duration * 1000L; i += tone * 2) {
        digitalWrite(speakerPin, LOW);
        delayMicroseconds(tone);
        digitalWrite(speakerPin, HIGH);
        delayMicroseconds(tone);
    }
}

void dead() { // void determines what the tagger does when it is out of lives

  // Makes a few noises and flashes some lights
    for (byte i = 1; i < 254; i++) {
        playTone((1000 + 9 * i), 2);
    }

    if (debug) {
        Serial.println(F("DEAD"));
    }
}

//void noAmmo() { // Make some noise and flash some lights when out of ammo
//  playTone(500, 100);
//  playTone(1000, 100);
//}

void hit() { // Make some noise and flash some lights when you get shot
    life = life - hp[hitNo];
    if (debug) {
        Serial.print(F("Life: "));
        Serial.println(life);
    }
    playTone(500, 500);
    if (life <= 0) {
        dead();
    }
    lifeDisplay();
}
