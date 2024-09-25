#include <EEPROM.h>
#include <SPI.h>
#include <heltec.h>
#include <Adafruit_NeoPixel.h>

// Pin Definitions for Heltec WiFi LoRa 32(V3)
#define PIXELPIN              13    // GPIO13 for NeoPixel
#define NUMPIXELS             24
#define MAX_USERS             12
#define IR_BUFLEN             40

#define BUTTON_PIN            0     // GPIO0 for onboard button

// Create NeoPixel object
Adafruit_NeoPixel pixels(NUMPIXELS, PIXELPIN, NEO_GRB + NEO_KHZ800);

// Variables
unsigned long userId = 71234L;
unsigned long user_ids[MAX_USERS] = {0};
byte user_healths[MAX_USERS];
byte user_teams[MAX_USERS];
byte team_a = 0, team_b = 0, team_c = 0;
bool team_a_in = false, team_b_in = false, team_c_in = false;
bool debug = true;
unsigned long last_message_send = 0, last_fire_time = 0, nowTime = 0;
volatile unsigned long lastHitTime = 0, lastFireTime = 0;
volatile bool triggerPressed = false;
volatile unsigned long irTime = 0, irTimeAny = 0, irElapsed = 0;
volatile bool irPulseState = false;
volatile int irMsgLen = 0;
volatile byte irMsg[IR_BUFLEN];
volatile bool irMsgAvailable = false;
byte irMsgFinal[IR_BUFLEN];
volatile bool firstFire = true, irRecv = false, irError = false;
bool flipper = 0;
byte radioBuf[16];

// Game states
enum GameState {
    TEAM_SELECTION,
    GAME_PLAY
};

GameState gameState = TEAM_SELECTION;

// Button variables
bool buttonState = HIGH;
bool lastButtonState = HIGH;
unsigned long buttonPressTime = 0;
unsigned long buttonReleaseTime = 0;
bool isLongPress = false;
const unsigned long longPressDuration = 1000; // 1 second

byte currentTeam = 1; // 1=A, 2=B, 3=C

byte myTeamID; // To be assigned after selection

byte guncolour_r = 0, guncolour_g = 0, guncolour_b = 0;

// Player and Game details
byte myPlayerID             = 1;      // Player ID
byte myGameID               = 1;      // Interpreted by configureGame subroutine
byte myWeaponID             = 0;      // Defined by gameType and configureGame subroutine
byte myWeaponHP             = 0;      // Defined by gameType and configureGame subroutine
int maxAmmo                = 0;      // Defined by gameType and configureGame subroutine
int maxLife                = 0;      // Defined by gameType and configureGame subroutine
byte automatic              = 0;      // Defined by gameType and configureGame subroutine
byte life                   = 0;      // Current life

// Signal Properties
int IRpulse                = 600;    // Basic pulse duration of 600uS
int IRfrequency            = 38;     // Frequency in kHz
int IRt                    = 0;      // LED on time to give correct transmission frequency
int IRpulses               = 0;      // Number of oscillations needed to make a full IRpulse
byte header                 = 4;      // Header length in pulses

// Transmission data
int byte1[8];                        // Stores byte1 of the data to be transmitted
int byte2[8];                        // Stores byte2 of the data to be transmitted
int myParity               = 0;      // Parity of the data to be transmitted

// Received data
byte memory                 = 3;     // Number of signals to be recorded
byte hitNo                  = 0;     // Hit number
// Byte1
int player[3];                      // Array must be as large as memory
byte team[3];                        // Array must be as large as memory
// Byte2
byte weapon[3];                      // Array must be as large as memory
int hp[3];                          // Array must be as large as memory
byte parity[3];                      // Array must be as large as memory

// Speaker pin (you may need to adjust this depending on your setup)
byte speakerPin      = 21; // GPIO21 for speaker

// Function Prototypes
void setup();
void loop();
void update_pixels();
void IRAM_ATTR isr_ir();
void handleTeamSelection();
void updateDisplayTeamSelection();
void displayLives();
void recvRadio2();
void parseRadioData();
void setTeamSize();
void everyone_dead();
void lifeDisplay();
void interpret2();
void shoot();
void sendPulse(int pin, int length);
void configureGame();
void frequencyCalculations();
void tagCode();
void playTone(int tone, int duration);
void dead();
void hit();

void setup() {
    Heltec.begin(true /*DisplayEnable Enable*/, true /*LoRa Enable*/, true /*Serial Enable*/, true /*PABOOST Enable*/, 915E6 /*long BAND*/);

    EEPROM.begin(512); // Initialize EEPROM

    // Read user ID from EEPROM
    unsigned long userTmp_1 = EEPROM.read(0);
    userTmp_1 = (userTmp_1 << 8) + EEPROM.read(1);
    userTmp_1 = (userTmp_1 << 8) + EEPROM.read(2);
    userTmp_1 = (userTmp_1 << 8) + EEPROM.read(3);

    if (userTmp_1 == 0xFFFFFFFF || userTmp_1 == 0xFF) {
        Serial.println(F("Enter user ID:"));
        while (!Serial.available());
        userTmp_1 = Serial.parseInt();
        if (userTmp_1 > 0) {
            Serial.print(F("Writing EEPROM"));
            Serial.println(userTmp_1);
            EEPROM.write(0, (userTmp_1 >> 24) & 0xFF);
            EEPROM.write(1, (userTmp_1 >> 16) & 0xFF);
            EEPROM.write(2, (userTmp_1 >> 8) & 0xFF);
            EEPROM.write(3, (userTmp_1) & 0xFF);
            EEPROM.commit();
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

    nowTime = millis();

    // Pin configurations
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(IRreceivePin, INPUT_PULLUP);
    pinMode(IRtransmitPin, OUTPUT);
    digitalWrite(IRtransmitPin, LOW);

    attachInterrupt(digitalPinToInterrupt(IRreceivePin), isr_ir, CHANGE);

    pixels.begin();

    for (byte j = 0; j < NUMPIXELS; j++) {
        pixels.setPixelColor(j, 20, 20, 20);
    }
    update_pixels();

    // Initialize speaker
    pinMode(speakerPin, OUTPUT);
    digitalWrite(speakerPin, HIGH);

    for (int i = 1; i < 254; i++) {
        playTone((3000 - 9 * i), 2);
    }

    // Initialize team selection display
    updateDisplayTeamSelection();
}

void loop() {
    nowTime = millis();

    if (gameState == TEAM_SELECTION) {
        // Handle team selection
        handleTeamSelection();
    } else if (gameState == GAME_PLAY) {
        // Game play code

        // Handle button press for firing
        buttonState = digitalRead(BUTTON_PIN);

        if (buttonState != lastButtonState) {
            if (buttonState == LOW) { // Button pressed
                lastFireTime = millis();
                triggerPressed = true;
            }
            lastButtonState = buttonState;
        }

        if (triggerPressed) {
            if (life > 0) {
                Serial.println("FIRE! :-)");
                last_fire_time = nowTime;
                shoot();
            }
            triggerPressed = false;
        }

        // Handle IR messages
        if (irRecv) {
            irTimeAny = millis();
            irRecv = false;

            if (irError) {
                irError = false;
                if (debug) {
                    Serial.print("IR [ERR] ");
                    Serial.print(irElapsed);
                    Serial.print(" ");
                    Serial.println(irPulseState);
                }
            } else {
                if (debug) {
                    Serial.print("IR ..... ");
                    Serial.print(irElapsed);
                    Serial.print(" ");
                    Serial.println(irPulseState);
                }
            }
        }

        if (irMsgLen >= 17 && (millis() - irTimeAny) > 50) {
            Serial.println("");
            Serial.print("Got enough length to interpret: ");
            Serial.println(irMsgLen);

            for (byte i = 0; i < irMsgLen; i++) {
                irMsgFinal[i] = irMsg[i];
            }

            interpret2();
        }

        if ((millis() - irTimeAny) > 100 && irMsgLen > 0) {
            if (debug) {
                Serial.println("");
                Serial.print("Resetting IR receiver, len was ");
                Serial.println(irMsgLen);

                for (byte i = 0; i < irMsgLen; i++) {
                    Serial.print(irMsg[i]);
                }
                Serial.println("");
            }

            irMsgLen = 0;
        }

        // Handle LoRa messages
        int packetSize = LoRa.parsePacket();
        if (packetSize) {
            recvRadio2();
        }

        if (nowTime - last_message_send > 10000) {
            byte bytes[5];
            bytes[0] = (userId >> 24) & 0xFF;
            bytes[1] = (userId >> 16) & 0xFF;
            bytes[2] = (userId >> 8) & 0xFF;
            bytes[3] = (userId >> 0) & 0xFF;
            bytes[4] = ((myTeamID & 0x0F) << 4) | (life & 0x0F);

            LoRa.beginPacket();
            LoRa.write(bytes, sizeof(bytes));
            LoRa.endPacket();

            last_message_send = nowTime;
        }
    }
}

void update_pixels() {
    detachInterrupt(digitalPinToInterrupt(IRreceivePin));
    pixels.show();
    attachInterrupt(digitalPinToInterrupt(IRreceivePin), isr_ir, CHANGE);
}

void IRAM_ATTR isr_ir() {
    irRecv = true;
    irPulseState = digitalRead(IRreceivePin);

    if (!irPulseState) {
        irTime = micros();
    } else {
        irElapsed = micros() - irTime;
        if (irElapsed > (IRpulse + IRpulse - 300) && irElapsed < (IRpulse + IRpulse + 300)) {
            irMsg[irMsgLen] = 0;
            irMsgLen++;
        } else if (irElapsed > (IRpulse + IRpulse + IRpulse + IRpulse - 300) && irElapsed < IRpulse + IRpulse + IRpulse + IRpulse + 300) {
            irMsg[irMsgLen] = 1;
            irMsgLen++;
        } else {
            irError = true;
        }
    }

    if (irMsgLen > IR_BUFLEN) {
        irMsgLen = 0;
    }
}

void handleTeamSelection() {
    buttonState = digitalRead(BUTTON_PIN);

    if (buttonState != lastButtonState) {
        if (buttonState == LOW) { // Button pressed
            buttonPressTime = millis();
            isLongPress = false;
        } else { // Button released
            buttonReleaseTime = millis();
            unsigned long pressDuration = buttonReleaseTime - buttonPressTime;

            if (pressDuration >= longPressDuration) {
                // Long press detected, select the team
                isLongPress = true;
                myTeamID = currentTeam;
                gameState = GAME_PLAY;

                // Set gun colour based on team
                if (myTeamID == 1) {
                    guncolour_r = 128;
                } else if (myTeamID == 2) {
                    guncolour_g = 128;
                } else if (myTeamID == 3) {
                    guncolour_b = 128;
                }

                // Update pixels
                for (byte j = 0; j < 12; j++) {
                    pixels.setPixelColor(j, guncolour_r, guncolour_g, guncolour_b);
                }
                for (byte j = 12; j < 24; j++) {
                    pixels.setPixelColor(j, 0, 0, 0);
                }
                update_pixels();

                // Display lives as proportion of maximum lives
                displayLives();

                // Play startup noise
                for (int i = 1; i < 254; i++) {
                    playTone((3000 - 9 * i), 2);
                }

                // Rest of the initialization code
                frequencyCalculations();
                configureGame();
                tagCode();
                lifeDisplay();

            } else {
                // Short press detected, cycle through teams
                currentTeam++;
                if (currentTeam > 3) {
                    currentTeam = 1;
                }
            }

            // Update display
            updateDisplayTeamSelection();
        }
        lastButtonState = buttonState;
    }
}

void updateDisplayTeamSelection() {
    Heltec.display->clear();
    Heltec.display->setTextAlignment(TEXT_ALIGN_CENTER);
    Heltec.display->setFont(ArialMT_Plain_16);
    String teamName;
    if (currentTeam == 1) {
        teamName = "Team A";
    } else if (currentTeam == 2) {
        teamName = "Team B";
    } else if (currentTeam == 3) {
        teamName = "Team C";
    }
    Heltec.display->drawString(64, 22, "Select Team");
    Heltec.display->drawString(64, 40, teamName);
    Heltec.display->display();
}

void displayLives() {
    Heltec.display->clear();
    Heltec.display->setTextAlignment(TEXT_ALIGN_CENTER);
    Heltec.display->setFont(ArialMT_Plain_16);
    Heltec.display->drawString(64, 0, "Game Started");
    String livesStr = "Lives: " + String(life) + "/" + String(maxLife);
    Heltec.display->drawString(64, 22, livesStr);
    Heltec.display->display();
}

void recvRadio2() {
    int count = 0;
    byte b;
    while (LoRa.available()) {
        b = (byte)LoRa.read();
        radioBuf[count] = b;
        count++;
    }
    parseRadioData();
}

void parseRadioData() {
    unsigned long this_user = radioBuf[0];
    this_user = (this_user << 8) + radioBuf[1];
    this_user = (this_user << 8) + radioBuf[2];
    this_user = (this_user << 8) + radioBuf[3];

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

    // Figure out if user is in database, if not add them
    byte tmp_user = 0;
    bool new_user = true;

    for (byte j = 0; j < MAX_USERS; j++) {
        if (user_ids[j] == this_user) {
            tmp_user = j;
            new_user = false;
            user_healths[tmp_user] = this_health;
            break;
        }
        if (user_ids[j] != 0) {
            tmp_user++;
        } else {
            break;
        }
    }

    if (new_user) {
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

    }

    setTeamSize();
    lifeDisplay();
}

void setTeamSize() {
    byte team_tmp_a = 0;
    byte team_tmp_b = 0;
    byte team_tmp_c = 0;

    for (byte j = 0; j < MAX_USERS; j++) {
        if (user_ids[j] != 0 && user_healths[j] > 0 && user_teams[j] == 1) {
            team_tmp_a++;
        }
    }
    team_a = team_tmp_a;

    for (byte j = 0; j < MAX_USERS; j++) {
        if (user_ids[j] != 0 && user_healths[j] > 0 && user_teams[j] == 2) {
            team_tmp_b++;
        }
    }
    team_b = team_tmp_b;

    for (byte j = 0; j < MAX_USERS; j++) {
        if (user_ids[j] != 0 && user_healths[j] > 0 && user_teams[j] == 3) {
            team_tmp_c++;
        }
    }
    team_c = team_tmp_c;
}

void everyone_dead() {
    for (byte m = 0; m < 5; m++) {
        for (byte p = 0; p < 255; p++) {
            for (byte j = 0; j < NUMPIXELS; j++) {
                pixels.setPixelColor(j, p, p, p);
            }
            pixels.show();
            delayMicroseconds(100);
        }
        for (int k = 255; k >= 0; k--) {
            for (byte n = 0; n < NUMPIXELS; n++) {
                pixels.setPixelColor(n, k, k, k);
            }
            pixels.show();
            delayMicroseconds(100);
        }
    }
}

void lifeDisplay() {
    // Update team indicators
    if ((team_a_in || team_b_in || team_c_in) && team_a + team_b + team_c == 0) {
        Serial.println("Everyone dead..........");
        everyone_dead();
    } else {
        if (team_a_in || team_b_in || team_c_in) {
            // Stack up the LEDs for the other teams
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
            // Show no teams are yet registered, with entirely white LEDs
            for (byte j = 12; j < NUMPIXELS; j++) {
                pixels.setPixelColor(j, 160, 160, 160);
            }
        }

        // Loop through health LEDs
        for (byte i = 0; i < 12; i++) {
            if (i >= life) {
                pixels.setPixelColor(i, 0, 0, 0);
            } else {
                pixels.setPixelColor(i, pixels.Color(guncolour_r, guncolour_g, guncolour_b));
            }
            update_pixels();
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

    // Check it's not the same team ID
    if (player[hitNo] != 0 && team[hitNo] != myTeamID && team[hitNo] < 12 && hp[hitNo] < 12 && hp[hitNo] > 0) {
        hit();
    }

    hitNo++;

    lifeDisplay();
}

void shoot() {
    sendPulse(IRtransmitPin, 4); // Transmit Header pulse
    delayMicroseconds(IRpulse);

    for (byte i = 0; i < 8; i++) { // Transmit Byte1
        if (byte1[i] == 1) {
            sendPulse(IRtransmitPin, 1);
        }
        sendPulse(IRtransmitPin, 1);
        delayMicroseconds(IRpulse);
    }

    for (byte i = 0; i < 8; i++) { // Transmit Byte2
        if (byte2[i] == 1) {
            sendPulse(IRtransmitPin, 1);
        }
        sendPulse(IRtransmitPin, 1);
        delayMicroseconds(IRpulse);
    }

    if (myParity == 1) { // Parity
        sendPulse(IRtransmitPin, 1);
    }
    sendPulse(IRtransmitPin, 1);
    delayMicroseconds(IRpulse);
}

void sendPulse(int pin, int length) {
    int i = 0;
    while (i < length) {
        i++;
        int o = 0;
        while (o < IRpulses) {
            o++;
            digitalWrite(pin, HIGH);
            delayMicroseconds(IRt);
            digitalWrite(pin, LOW);
            delayMicroseconds(IRt);
        }
    }
}

void configureGame() {
    if (myGameID == 0) {
        myWeaponID = 1;
        maxAmmo = 30;
        // ammo = 30;
        maxLife = 3;
        life = 3;
        myWeaponHP = 1;
    }
    if (myGameID == 1) {
        myWeaponID = 1;
        maxAmmo = 1000;
        // ammo = 1000;
        maxLife = 12;
        life = 12;
        myWeaponHP = 1;
    }
}

void frequencyCalculations() {
    IRt = (int)(500 / IRfrequency);
    IRpulses = (int)(IRpulse / (2 * IRt));
    IRt = IRt - 4;
}

void tagCode() {
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
        Serial.print(F("Byte1: "));
        for (int i = 0; i < 8; i++) {
            Serial.print(byte1[i]);
        }
        Serial.println();

        Serial.print(F("Byte2: "));
        for (int i = 0; i < 8; i++) {
            Serial.print(byte2[i]);
        }
        Serial.println();

        Serial.print(F("Parity: "));
        Serial.print(myParity);
        Serial.println();
    }
}

void playTone(int tone, int duration) {
    ledcAttachPin(speakerPin, 0); // Channel 0
    ledcWriteTone(0, tone);
    delay(duration);
    ledcWriteTone(0, 0); // Stop tone
}

void dead() {
    for (byte i = 1; i < 254; i++) {
        playTone((1000 + 9 * i), 2);
    }

    if (debug) {
        Serial.println(F("DEAD"));
    }
}

void hit() {
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
