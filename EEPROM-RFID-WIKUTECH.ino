#include <SPI.h>        
#include <MFRC522.h>	
#include "extEEPROM.h" 
 
extEEPROM xEEPROM(kbits_256, 1, 64, 0x50); 

#include <Servo.h>
#include <LiquidCrystal_I2C.h>


LiquidCrystal_I2C lcd(0x27, 16, 2);

#define COMMON_ANODE

#ifdef COMMON_ANODE
#define LED_ON LOW
#define LED_OFF HIGH
#else
#define LED_ON HIGH
#define LED_OFF LOW
#endif

#define redLed 7		// Set Led Pins
#define greenLed 6
#define blueLed 5

//#define portal 3	
#define wipeB 2		

boolean match = false;          
boolean programMode = false;	
boolean replaceMaster = false;

int successRead;		

byte storedCard[4];		
byte readCard[4];		
byte masterCard[4];		

#define SS_PIN 10
#define RST_PIN 9
MFRC522 mfrc522(SS_PIN, RST_PIN);	// Create MFRC522 instance.
Servo portal;     // variable untuk menyimpan posisi data
int pos = 00; 

///////////////////////////////////////// Setup ///////////////////////////////////
void setup() {
  //Arduino Pin Configuration
  pinMode(redLed, OUTPUT);
  pinMode(greenLed, OUTPUT);
  pinMode(blueLed, OUTPUT);
  pinMode(wipeB, INPUT_PULLUP);
  portal.attach(3);
  portal.write(00);
  digitalWrite(redLed, LED_OFF);
  digitalWrite(greenLed, LED_OFF);
  digitalWrite(blueLed, LED_OFF);
  //Protocol Configuration
  Serial.begin(9600);
  SPI.begin(); 
  mfrc522.PCD_Init();
  lcd.begin();
  //lcd.init();
  byte i2cStat = xEEPROM.begin(xEEPROM.twiClock100kHz);
  if ( i2cStat != 0 ) {
    Serial.println(F("I2C bermasalah"));
  }
  unsigned int dataSize = 5;
 
  // Nyalakan backlight
  lcd.backlight();
  lcdHello();
 
  Serial.println(F("RFID-EEPROM WIKUTECH"));  
  ShowReaderDetails();	

  if (digitalRead(wipeB) == LOW) {	
    digitalWrite(redLed, LED_ON);	
    Serial.println(F("Tombol hapus ditekan"));
    Serial.println(F("15 detik untuk membatalkan"));
    Serial.println(F("Ini akan menghapus semuanya"));
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("menghapus data?");
    lcd.setCursor(1,1);
    lcd.print("tekan 15 detik");
    delay(15000);                       
    if (digitalRead(wipeB) == LOW) {    
      Serial.println(F("MULAI MENGHAPUS EEPROM"));
      for (int x = 0; x < xEEPROM.length(); x = x + 1) {    
        if (xEEPROM.read(x) == 0) {              
          
        }
        else {
          xEEPROM.write(x, 0); 			
        }
      }
      Serial.println(F("EEPROM berhasil dihapus"));
      lcd.clear();
      lcd.setCursor(1,0);
      lcd.print("data terhapus!");
      digitalWrite(redLed, LED_OFF); 	
      delay(200);
      digitalWrite(redLed, LED_ON);
      delay(200);
      digitalWrite(redLed, LED_OFF);
      delay(200);
      digitalWrite(redLed, LED_ON);
      delay(200);
      digitalWrite(redLed, LED_OFF);
    }
    else {
      Serial.println(F("Wiping Cancelled"));
      digitalWrite(redLed, LED_OFF);
    }
  }
  
  if (xEEPROM.read(1) != 143) {
    Serial.println(F("Tidak ada kartu master"));
    Serial.println(F("Scan A PICC untuk menentukan Kartu Master"));
    lcd.clear();
    lcd.setCursor(2,0);
    lcd.print("kartu master");
    lcd.setCursor(1,1);
    lcd.print("kosong, scan..");
    do {
      successRead = getID();            
      digitalWrite(blueLed, LED_ON);    
      delay(200);
      digitalWrite(blueLed, LED_OFF);
      delay(200);
    }
    while (!successRead);                  
    for ( int j = 0; j < 4; j++ ) {       
      xEEPROM.write( 2 + j, readCard[j] );  
    }
    xEEPROM.write(1, 143);                  
    Serial.println(F("Kartu Master sudah dibuat"));
    lcd.clear();
    lcd.setCursor(3,0);
    lcd.print("menyimpan");
    lcd.setCursor(2,1);
    lcd.print("kartu master");
  }
  Serial.println(F("-------------------"));
  Serial.println(F("Master Card's UID"));
  for ( int i = 0; i < 4; i++ ) {          
    masterCard[i] = xEEPROM.read(2 + i);   
    Serial.print(masterCard[i], HEX);
  }
  Serial.println("");
  Serial.println(F("-------------------"));
  Serial.println(F("Semuanya siap"));
  Serial.println(F("Menunggu scan PCC"));
  lcdHello();
  cycleLeds();   
}


void loop () {
  do {
    successRead = getID(); 	
    if (digitalRead(wipeB) == LOW) {
      digitalWrite(redLed, LED_ON);  
      digitalWrite(greenLed, LED_OFF);  
      digitalWrite(blueLed, LED_OFF); 
      Serial.println(F("Wipe Button Pressed"));
      Serial.println(F("Master Card will be Erased! in 10 seconds"));
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("menghapus data?");
      lcd.setCursor(1,1);
      lcd.print("tekan 10 detik");
      delay(10000);
      if (digitalRead(wipeB) == LOW) {
        xEEPROM.write(1, 0);                  
        Serial.println(F("Restart device to re-program Master Card"));
        lcd.clear();
        lcd.setCursor(2,0);
        lcd.print("REBOOT MESIN");
        lcd.setCursor(4,1);
        lcd.print("SEKARANG");
        while (1);
      }
    }
    if (programMode) {
      cycleLeds();              
    }
    else {
      normalModeOn(); 		
    }
  }
  while (!successRead); 	
  if (programMode) {
    if ( isMaster(readCard) ) { 
      Serial.println(F("Master Card Scanned"));
      Serial.println(F("Exiting Program Mode"));
      Serial.println(F("-----------------------------"));
      lcd.clear();
      lcd.setCursor(3,0);
      lcd.print("KELUAR DARI");
      lcd.setCursor(3,1);
      lcd.print("MODE MASTER");
      programMode = false;
      delay(1000);
      lcdHello();
      return;
    }
    else {
      if ( findID(readCard) ) { 
        Serial.println(F("PICC sudah ada, menghapus..."));
        lcd.clear();
        lcd.setCursor(4,0);
        lcd.print("MENGHAPUS");
        lcd.setCursor(3,1);
        lcd.print("DATA KARTU");
        deleteID(readCard);
        lcd.clear();
        lcd.setCursor(4,0);
        lcd.print("KARTU");
        lcd.setCursor(3,1);
        lcd.print("DIHAPUS");
        Serial.println("-----------------------------");
        Serial.println(F("Scan PICC untuk menambah / menghapus"));
        
      }
      else {                  
        Serial.println(F("I do not know this PICC, adding..."));
        lcd.clear();
        lcd.setCursor(2,0);
        lcd.print("MENAMBAHKAN");
        lcd.setCursor(3,1);
        lcd.print("DATA KARTU");
        writeID(readCard);
        lcd.clear();
        lcd.setCursor(4,0);
        lcd.print("KARTU");
        lcd.setCursor(3,1);
        lcd.print("DITAMBAHKAN");
        Serial.println(F("-----------------------------"));
        Serial.println(F("Scan a PICC to ADD or REMOVE to EEPROM"));
      }
    }
  }
  else {
    if ( isMaster(readCard)) {  	
      programMode = true;
      Serial.println(F("Hello Master - Entered Program Mode"));
      int count = xEEPROM.read(0); 	
      Serial.print(F("I have "));    
      Serial.print(count);
      Serial.print(F(" record(s) on EEPROM"));
      Serial.println("");
      Serial.println(F("Scan a PICC to ADD or REMOVE to EEPROM"));
      Serial.println(F("Scan Master Card again to Exit Program Mode"));
      Serial.println(F("-----------------------------"));
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("MASUK MODE MASTER");
      lcd.setCursor(0,1);
      lcd.print("TAP TAMBAH/HAPUS");
    }
    else {
      if ( findID(readCard) ) {	
        Serial.println(F("Welcome, You shall pass"));
        lcd.clear();
        lcd.setCursor(4,0);
        lcd.print("SILAHKAN");
        lcd.setCursor(5,1);
        lcd.print("MASUK");
        granted(300);        	      
        lcdHello();
      }
      else {			
        Serial.println(F("You shall not pass"));
        lcd.clear();
        lcd.setCursor(2,0);
        lcd.print("MAAF...KARTU");
        lcd.setCursor(2,1);
        lcd.print("TIDAK VALID!");
        denied();        
        lcdHello();
      }
    }
  }
}

/////////////////////////////////////////  Access Granted    ///////////////////////////////////
void granted (int setDelay) {
  digitalWrite(blueLed, LED_OFF); 	
  digitalWrite(redLed, LED_OFF); 	
  digitalWrite(greenLed, LED_ON); 	
  portal.write(90); 		
  delay(setDelay); 					
  portal.write(00); 		
  delay(1000); 						
}
///////////////////////////////////////// Access Denied  ///////////////////////////////////
void denied() {
  digitalWrite(greenLed, LED_OFF); 	
  digitalWrite(blueLed, LED_OFF); 	
  digitalWrite(redLed, LED_ON); 	
  delay(1000);
}


///////////////////////////////////////// Get PICC's UID ///////////////////////////////////
int getID() {
  if ( ! mfrc522.PICC_IsNewCardPresent()) { 
    return 0;
  }
  if ( ! mfrc522.PICC_ReadCardSerial()) {   
    return 0;
  }
  
  Serial.println(F("Scanned PICC's UID:"));
  for (int i = 0; i < 4; i++) {  
    readCard[i] = mfrc522.uid.uidByte[i];
    Serial.print(readCard[i], HEX);
  }
  Serial.println("");
  mfrc522.PICC_HaltA();
  return 1;
}

void ShowReaderDetails() {
  byte v = mfrc522.PCD_ReadRegister(mfrc522.VersionReg);
  Serial.print(F("MFRC522 Software Version: 0x"));
  Serial.print(v, HEX);
  if (v == 0x91)
    Serial.print(F(" = v1.0"));
  else if (v == 0x92)
    Serial.print(F(" = v2.0"));
  else
    Serial.print(F(" (unknown),probably a chinese clone?"));
  Serial.println("");
  
  if ((v == 0x00) || (v == 0xFF)) {
    Serial.println(F("WARNING: Communication failure, is the MFRC522 properly connected?"));
    Serial.println(F("SYSTEM HALTED: Check connections."));
    while (true);
  }
}

///////////////////////////////////////// Cycle Leds (Program Mode) ///////////////////////////////////
void cycleLeds() {
  digitalWrite(redLed, LED_OFF); 	
  digitalWrite(greenLed, LED_ON); 	
  digitalWrite(blueLed, LED_OFF); 	
  delay(200);
  digitalWrite(redLed, LED_OFF); 
  digitalWrite(greenLed, LED_OFF); 	
  digitalWrite(blueLed, LED_ON); 	
  delay(200);
  digitalWrite(redLed, LED_ON); 	
  digitalWrite(greenLed, LED_OFF); 
  digitalWrite(blueLed, LED_OFF); 
  delay(200);
}

//////////////////////////////////////// Normal Mode Led  ///////////////////////////////////
void normalModeOn () {
  digitalWrite(blueLed, LED_ON); 	
  digitalWrite(redLed, LED_OFF); 	
  digitalWrite(greenLed, LED_OFF); 	
  portal.write(90); 
}

//////////////////////////////////////// Read an ID from EEPROM //////////////////////////////
void readID( int number ) {
  int start = (number * 4 ) + 2; 
  for ( int i = 0; i < 4; i++ ) { 	
    storedCard[i] = xEEPROM.read(start + i); 
  }
}

void writeID( byte a[] ) {
  if ( !findID( a ) ) { 
    int num = xEEPROM.read(0); 
    int start = ( num * 4 ) + 6; 
    num++; 
    xEEPROM.write( 0, num );
    for ( int j = 0; j < 4; j++ ) {
      xEEPROM.write( start + j, a[j] );
    }
    successWrite();
    Serial.println(F("Succesfully added ID record to EEPROM"));
  }
  else {
    failedWrite();
    Serial.println(F("Failed! There is something wrong with ID or bad EEPROM"));
  }
}

void deleteID( byte a[] ) {
  if ( !findID( a ) ) {
    failedWrite(); 		
    Serial.println(F("Failed! There is something wrong with ID or bad EEPROM"));
  }
  else {
    int num = xEEPROM.read(0);
    int slot; 
    int start;
    int looping;
    int j;
    int count = xEEPROM.read(0);
    slot = findIDSLOT( a );
    start = (slot * 4) + 2;
    looping = ((num - slot) * 4);
    num--;
    xEEPROM.write( 0, num );
    for ( j = 0; j < looping; j++ ) {
      xEEPROM.write( start + j, xEEPROM.read(start + 4 + j));
    }
    for ( int k = 0; k < 4; k++ ) { 
      xEEPROM.write( start + j + k, 0);
    }
    successDelete();
    Serial.println(F("Succesfully removed ID record from EEPROM"));
  }
}

boolean checkTwo ( byte a[], byte b[] ) {
  if ( a[0] != NULL )
    match = true;
  for ( int k = 0; k < 4; k++ ) { 
    if ( a[k] != b[k] )
      match = false;
  }
  if ( match ) { 
    return true; 
  }
  else  {
    return false; 	
  }
}

int findIDSLOT( byte find[] ) {
  int count = xEEPROM.read(0); 
  for ( int i = 1; i <= count; i++ ) {
    readID(i);
    if ( checkTwo( find, storedCard ) ) {
      return i;
      break;
    }
  }
}

boolean findID( byte find[] ) {
  int count = xEEPROM.read(0);
  for ( int i = 1; i <= count; i++ ) {
    readID(i); 
    if ( checkTwo( find, storedCard ) ) {
      return true;
      break;
    }
    else {
    }
  }
  return false;
}

void successWrite() {
  digitalWrite(blueLed, LED_OFF);
  digitalWrite(redLed, LED_OFF);
  digitalWrite(greenLed, LED_OFF);
  delay(200);
  digitalWrite(greenLed, LED_ON); 
  delay(200);
  digitalWrite(greenLed, LED_OFF);
  delay(200);
  digitalWrite(greenLed, LED_ON);
  delay(200);
  digitalWrite(greenLed, LED_OFF);
  delay(200);
  digitalWrite(greenLed, LED_ON);
  delay(200);
}

void failedWrite() {
  digitalWrite(blueLed, LED_OFF);
  digitalWrite(redLed, LED_OFF);
  digitalWrite(greenLed, LED_OFF);
  delay(200);
  digitalWrite(redLed, LED_ON);
  delay(200);
  digitalWrite(redLed, LED_OFF);
  delay(200);
  digitalWrite(redLed, LED_ON);
  delay(200);
  digitalWrite(redLed, LED_OFF);
  delay(200);
  digitalWrite(redLed, LED_ON);
  delay(200);
}

void successDelete() {
  digitalWrite(blueLed, LED_OFF);
  digitalWrite(redLed, LED_OFF);
  digitalWrite(greenLed, LED_OFF);
  delay(200);
  digitalWrite(blueLed, LED_ON);
  delay(200);
  digitalWrite(blueLed, LED_OFF);
  delay(200);
  digitalWrite(blueLed, LED_ON);
  delay(200);
  digitalWrite(blueLed, LED_OFF);
  delay(200);
  digitalWrite(blueLed, LED_ON);
  delay(200);
}


boolean isMaster( byte test[] ) {
  if ( checkTwo( test, masterCard ) )
    return true;
  else
    return false;
}

void lcdHello(){
  
  lcd.clear();
  lcd.setCursor(3, 0);
  lcd.print("WELCOME TO");
  lcd.setCursor(3, 1);      
  lcd.print("GRAHA ASRI");
  delay(2000);
  lcd.clear();
  lcd.setCursor(4, 0);
  lcd.print("Silahkan");
  lcd.setCursor(1, 1);      
  lcd.print("Tap Kartu Anda");
}
