#include <SPI.h>
#include <SD.h>

//#define DEBUG  //turns on serial debugging

//Standard RRD smart controller LCD pins when on a RAMPS 1.4

//lcd pins  
#define LCD_PINS_RS 16 //[RAMPS14-SMART-ADAPTER]  
#define LCD_PINS_ENABLE 17 //[RAMPS14-SMART-ADAPTER]  
#define LCD_PINS_D4 23 //[RAMPS14-SMART-ADAPTER]  
#define LCD_PINS_D5 25 //[RAMPS14-SMART-ADAPTER]  
#define LCD_PINS_D6 27 //[RAMPS14-SMART-ADAPTER]  
#define LCD_PINS_D7 29 //[RAMPS14-SMART-ADAPTER]  

//encoder pins
#define BTN_EN1         31
#define BTN_EN2         33
#define BTN_ENC         35

//SDCARD Pins
#define CS              16
#define MOSI            17
#define SCK             23
#define SD_DETECT_PIN   49
#define SDSS            53

#define BEEPER_PIN      37
#define KILL_PIN        41

#define screenX         20
#define screenY         4

#include <LiquidCrystal.h>
LiquidCrystal lcd(LCD_PINS_RS, LCD_PINS_ENABLE, LCD_PINS_D4, LCD_PINS_D5, LCD_PINS_D6, LCD_PINS_D7); //RS,Enable,D4,D5,D6,D7

int encoderPos = 8;                     //Current encoder position, mid way on scale
int encoderPosLast = 8;                 //Last encoder position, midway on scale
int encoder0PinALast;                   //Used to decode rotory encoder, last value
int encoder0PinNow;                     //Used to decode rotory encoder, current value

Sd2Card card;
SdVolume volume;

int sdcardinit;
int sdcardtype;
int sdvolumeinit;
int sdvolumefattype;
unsigned long sdvolumebpc;
unsigned long sdvolumecc;
char tmp_string[16];

//Customer characters for corner of logo
byte customChar0[] = {
  B00000,   B00000,  B00000,  B00111,  B00100,  B00100,  B00100,  B00100
};
byte customChar1[] = {
  B00000,   B00000,  B00000,  B11100,  B00100,  B00100,  B00100,  B00100
};
byte customChar2[] = {
  B00100,   B00100,  B00100,  B00111,  B00000,  B00000,  B00000,  B00000
};
byte customChar3[] = {
  B00100,   B00100,  B00100,  B11100,  B00000,  B00000,  B00000,  B00000
};


static void logo_lines() {
  lcd.setCursor(0, 0); lcd.print('\x00'); lcd.print( "------------------" );  lcd.write('\x01');
  lcd.setCursor(0, 1);                    lcd.print("|  RRD LCD Tester  |");  
  lcd.setCursor(0, 2); lcd.write('\x02'); lcd.print( "------------------" );  lcd.write('\x03');
  }

static void status_line(char *text) {
  lcd.setCursor(0, 3); lcd.print("                    "); //clear the status line
  lcd.setCursor(0, 3); lcd.print(text); 
  }

static void encoder_status_line(int value) {
  //Serial.println(value);
  lcd.setCursor(0, 3); lcd.print("Enc:                "); 
  lcd.setCursor(4, 3); 
  for (int x=0;x<value;x++) lcd.print("*"); 
  }


static void sdcardinfo() { 
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Initializing SD card");
  if (sdcardinit) {
      lcd.setCursor(0, 1); lcd.print("Card type:");
      switch (sdcardtype) {
        case SD_CARD_TYPE_SD1:
          lcd.setCursor(11, 1); lcd.print("SD1");
          break;
        case SD_CARD_TYPE_SD2:
          lcd.setCursor(11, 1); lcd.print("SD2");
          break;
        case SD_CARD_TYPE_SDHC:
          lcd.setCursor(11, 1); lcd.print("SDHC");
          break;
        default:
          lcd.setCursor(11, 1); lcd.print("Unknown");
      }
      if (!sdvolumeinit) {
        lcd.setCursor(0, 2); lcd.print("No FAT16/FAT32");
      } else {

        // print the type and size of the first FAT-type volume
        lcd.setCursor(0, 2); lcd.print("Volume type: FAT");
        itoa(sdvolumefattype, tmp_string, 10);
        lcd.print(tmp_string);

        uint32_t volumesize;
        volumesize = sdvolumebpc;    // clusters are collections of blocks
        volumesize *= sdvolumecc;       // we'll have a lot of clusters
        volumesize /= 2;                           // SD card blocks are always 512 bytes (2 blocks are 1KB)
        volumesize /= 1024;
        
        lcd.setCursor(0, 3); lcd.print("Volume size(Mb):");
        itoa(volumesize, tmp_string, 10);
        lcd.print(tmp_string);
      }
  } else {
      lcd.setCursor(0, 1); lcd.print("FAILED");
  }
}

void setup() {  

#ifdef DEBUG
  Serial.begin(9600);
  Serial.println("Start:");
#endif
  
  lcd.begin(screenX, screenX);
  // Print a message to the LCD.
  lcd.createChar(0, customChar0);
  lcd.createChar(1, customChar1);
  lcd.createChar(2, customChar2);
  lcd.createChar(3, customChar3);

  pinMode(SD_DETECT_PIN, INPUT);        // Set SD_DETECT_PIN as an unput
  digitalWrite(SD_DETECT_PIN, HIGH);    // turn on pullup resistors
  pinMode(KILL_PIN, INPUT);             // Set KILL_PIN as an unput
  digitalWrite(KILL_PIN, HIGH);         // turn on pullup resistors
  pinMode(BTN_EN1, INPUT);              // Set BTN_EN1 as an unput, half of the encoder
  digitalWrite(BTN_EN1, HIGH);          // turn on pullup resistors
  pinMode(BTN_EN2, INPUT);              // Set BTN_EN2 as an unput, second half of the encoder
  digitalWrite(BTN_EN2, HIGH);          // turn on pullup resistors
  pinMode(BTN_ENC, INPUT);              // Set BTN_ENC as an unput, encoder button
  digitalWrite(BTN_ENC, HIGH);          // turn on pullup resistors
  pinMode(BEEPER_PIN, OUTPUT);              // Set BTN_ENC as an unput, encoder button

#ifdef DEBUG
  Serial.print("BTN_EN1:"); Serial.println(digitalRead(BTN_EN1));
  Serial.print("BTN_EN2:"); Serial.println(digitalRead(BTN_EN2));
#endif
  //dependion on power on position of encoder adjust encoderPosLast  
  if (digitalRead(BTN_EN1) && digitalRead(BTN_EN2)) encoderPosLast--;
  else if ((!digitalRead(BTN_EN1) && digitalRead(BTN_EN2)) || (digitalRead(BTN_EN1) && !digitalRead(BTN_EN2))) encoderPosLast++;
    
  logo_lines();
  status_line("Starting...");
  delay(1000);
  status_line("Ready");
}

//Main arduino loop
void loop() {
  //If sd card is inserted display SD card info
  if (!digitalRead(SD_DETECT_PIN)) {
    status_line("SD Card Inserted");
    sdcardinit = card.init(SPI_HALF_SPEED, SDSS);
    if (!sdcardinit) sdcardinit = card.init(SPI_HALF_SPEED, SDSS);  //try it again.
    sdcardtype = card.type();
    sdvolumeinit = volume.init(card);
    sdvolumefattype = volume.fatType();
    sdvolumebpc = volume.blocksPerCluster();
    sdvolumecc = volume.clusterCount();
    sdcardinfo();
    while (!digitalRead(SD_DETECT_PIN));    //wait for sd card to be removed.
    lcd.clear();
    logo_lines();
    status_line("SD Card Removed"); 
  }
  else {
    // Read the encoder and update encoderPos    
    encoder0PinNow = digitalRead(BTN_EN2);
    if ((encoder0PinALast == LOW) && (encoder0PinNow == HIGH)) {
      if (digitalRead(BTN_EN1) == LOW) {
        encoderPos++;
        if(encoderPos>screenX-4) encoderPos=screenX-4;
      } else {
        encoderPos--;
        if(encoderPos<0) encoderPos=0;
      }
    }
    encoder0PinALast = encoder0PinNow;
    if(encoderPos != encoderPosLast) encoder_status_line(encoderPos);
#ifdef DEBUG
    Serial.print("EncoderPos"); Serial.println(encoderPos);
    Serial.print("EncoderPosLast"); Serial.println(encoderPosLast);
#endif
    encoderPosLast = encoderPos;

    //check if both buttons and sound the ebuzzer
    if ( !digitalRead(BTN_ENC) && !digitalRead(KILL_PIN) ) {
      status_line("Buzzer activated"); 
      digitalWrite(BEEPER_PIN, HIGH);
      while (!digitalRead(BTN_ENC) && !digitalRead(KILL_PIN)); //wait for button release
      digitalWrite(BEEPER_PIN, LOW);
      status_line("Buzzer deactivated"); 
    }

    //check encoder button
    if ( !digitalRead(BTN_ENC) && digitalRead(KILL_PIN) ) {
      status_line("Enc: button pressed"); 
      //digitalWrite(BEEPER_PIN, HIGH);
      while (!digitalRead(BTN_ENC) && digitalRead(KILL_PIN));
      //digitalWrite(BEEPER_PIN, LOW);
      status_line("Enc: button released"); 
    }
    //Check Kill Pin 
    if ( !digitalRead(KILL_PIN) && digitalRead(BTN_ENC) )  {
      status_line("Kill button pressed"); 
      while (!digitalRead(KILL_PIN) && digitalRead(BTN_ENC));
      status_line("Kill button released"); 
    }

  }

#ifdef DEBUG
  delay(2000);
#endif

}
