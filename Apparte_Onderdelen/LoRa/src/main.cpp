// ----------------------------------------------------------------------------
// Test Code
// - Sending CSV-record through LoRa - SPI connected
// 
// Do this loop every 1 sec.
//
// Notes:
// 
// (1) Set LoRa sync word the same at transmitter & receiver
// (2) LoRa frequency is set for Europe (866 MHz)
//
// ----------------------------------------------------------------------------
// F.Demonie  09/06/25 V1.0 - Initial release
// ----------------------------------------------------------------------------
#include <Arduino.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include "SPI.h" // SPI
#include <Wire.h> // I2C

#include <LoRa.h> // LoRa

#define VERSION 1.0

#define DEBUG // define / undefine to enable / disable debug mode
#define START_OF_NEW_TRANSMISSION "START-OF-NEW-TRANSMISSION"

#define csPIN     5  // Set chip select PIN for SPI connection 
#define resetPIN  14 // Set reset PIN for SPI connection
#define irqPIN    2  // Set IReq PIN for SPIconnection        

// ---------- LoRa Settings ---------------
// Frequentie-instelling (voor Europa: 868 MHz)
#define LORA_FREQ 868E6

// Adresinstellingen
#define SENDER_ADDRESS   0xAA
#define RECEIVER_ADDRESS 0xBB
#define LORA_ID          0x1B

// LoRa Paremeters
// Voor lange afstand — lage datasnelheid, hoge gevoeligheid
// Voor CanSat zou moeten OK zijn zo !
#define TxPower         20      // maximaal vermogen
#define SignalBandwidth 125E3   // 125 kHz standaard
#define CodingRate4     5       // 4/5 codering
#define SpreadingFactor 10      // 6–12 (hoger = groter bereik)
#define CRC             1       // CRC validatie AAN  
// #define SYNCWORD  0xF3 // Set Syncword for LoRa transmitter
                       // !! ensure same syncword is used at LoRa receiver

// General defines
#define MSG_LEN   150 // Message lengt of to be transmitted message
#define DELIMETER ";" // Set CSV DELIMETER to ;
#define DELAY_TIME  10000 // Delay time between 2 transmissions

// Define record types
#define MOTOR_RECORD  "M"
#define BMP_RECORD    "B"
#define GPS_RECORD    "G"

// Declare functions
void sendMsg( String outgoing );
void readRPi( String &msg );
void readBMP280( String &msg );
void readGPS( String &msg );
float randomFloatDec2(float minVal, float maxVal );
void setManualDateTime(int year, int month, int day, int hour, int minute, int second);
String getFormattedTime();

int  msgCount = 1; // Set trasmit message counter to start

// -------------------------------------------------------------------------
// This is the onetime used setup function
// (1) Start LoRa connection (in Europe)
// (2) Send a START-OF-NEW-TRANSMISSION message to the receiver
// -------------------------------------------------------------------------
void setup() {
  unsigned status;

  // Stel tijd in: YYYY, MM, DD, HH, MM, SS
  setManualDateTime(2025, 11, 17, 15, 22, 00); // Set manual date/time to 17-11-2025 15:22:00

  randomSeed(esp_random()); // Reset random generator

  // (1) --- USB Serial Monitor start ------------------------------
  Serial.begin( 115200 ); 
  while (!Serial) {
    delay( 10 );
  }
  delay( 200 ); // Short delay to allow printout message on serial monitor
  Serial.println( "=======================================" );
  Serial.print( "Start of Program\tVersion: " );
  Serial.println( VERSION );
  Serial.println( "=======================================" );
  
  // (1) --- Start LoRa communication -------------------------------
  LoRa.setPins( csPIN, resetPIN, irqPIN );

  // Start de LoRa-module
  if (!LoRa.begin(LORA_FREQ)) {
    Serial.println("Fout: kon LoRa-module niet starten!");
    while (1);
  }

  // Voor lange afstand — lage datasnelheid, hoge gevoeligheid
  LoRa.setTxPower( TxPower );                   // maximaal vermogen
  LoRa.setSignalBandwidth( SignalBandwidth );   // 125 kHz standaard
  LoRa.setCodingRate4( CodingRate4 );           // 4/5 codering
  LoRa.setSpreadingFactor( SpreadingFactor );   // 6–12 (hoger = groter bereik)
  #if CRC
    LoRa.enableCrc();                 // CRC aan (idem pico )
  #endif

  Serial.println( "1. => LoRa module successful connected (SPI)" );   
  
  // (2) --- Send Start of new transmission to receiver --------
  sendMsg( START_OF_NEW_TRANSMISSION );
  Serial.printf( "\t%s\n", START_OF_NEW_TRANSMISSION ); 
  Serial.println( "2. => Start of new transmission message send to receiver" );
  Serial.println();

  Serial.println();
}

// -------------------------------------------------------------------------
// This is the loop function
//  
//  (4) Send CSV record though LoRa
//  (5) Print CSV record on Serial monitor
//  (6) delay for 1 sec
// Restart loop
// -------------------------------------------------------------------------
void loop() {
  String RPi_msg = "";
  String BMP_msg = "";
  String GPS_msg = "";
  
  // Read data
  readRPi( RPi_msg );
  readBMP280( BMP_msg );
  readGPS( GPS_msg );
  
  // Transmit msg through LoRa
  // In 3 payloads, due to limited transmission time
  sendMsg( RPi_msg ); // Transmit RPi msg through LoRa
  sendMsg( BMP_msg ); // Transmit BMP280 msg through LoRa
  sendMsg( GPS_msg ); // Transmit GPS msg through LoRa
  
  #ifdef DEBUG
    Serial.println( RPi_msg ); // Print data on Serial monitor
    Serial.println( BMP_msg ); // Print data on Serial monitor
    Serial.println( GPS_msg ); // Print data on Serial monitor
    Serial.println();
  #endif

  delay( DELAY_TIME );
}


// -------------------------------------------------------------------------
// This function sends the output CSV record through LoRa
// to the receiver
// -------------------------------------------------------------------------
void sendMsg( String outgoing ) {
  char buffer[MSG_LEN];

  outgoing.toCharArray(buffer, MSG_LEN); // convert String to char array

  // Transmit msg through LoRa
  LoRa.beginPacket();
  // Voeg ontvangeradres, zenderadres, ID en bericht lengte toe aan de LoRa header
    LoRa.write(RECEIVER_ADDRESS);
    LoRa.write(SENDER_ADDRESS);
    LoRa.write( LORA_ID );
    LoRa.write( strlen( buffer ) ); // bericht lengte

    // Voeg het bericht toe als één blok
    LoRa.write((const uint8_t*)buffer, strlen( buffer ) );
  
  LoRa.endPacket();

  msgCount++; // increment the message counter
}

// -------------------------------------------------------------------------
// This function reads values from the RPi Zero (through interupt call)
// and sets Engine mode according input.
// The CSV-record is set to include the desired stage of the engines
// -------------------------------------------------------------------------
void readRPi( String &msg ) {
    int engine1 = random( 0, 2 ); // Simulate engine 1 state (0=off, 1=on)
    int engine2 = random( 0, 2 ); // Simulate engine 2 state (0=off, 1=on)

    msg = String( MOTOR_RECORD ) + DELIMETER + String(engine1) + DELIMETER + String(engine2);
}

// -------------------------------------------------------------------------
// This function reads values from the BME280 Sensor
// and appends them to he CSV-record 
// Values read and appended are: Temperature in °C
//                               Pressure in Pa
//                               Altitude in m
//                               Humidity in %
// For accurate altitude calculation, set the sea level pressure correct at 
// begin of this program.
// -------------------------------------------------------------------------
void readBMP280( String &msg ) {
  float temp = (float)randomFloatDec2( -40.00, 40.00 );
  float press = (float)randomFloatDec2( 800.00, 1020.00 );
  float alt = (float )randomFloatDec2( 500.00, 2500.00 );

  msg = String( BMP_RECORD ) + DELIMETER;
  msg = msg + String( temp, 2 ) + DELIMETER;
  msg = msg + String( press, 2 ) + DELIMETER;
  msg = msg + String( alt, 2 );
}

// -------------------------------------------------------------------------
// This function reads values from the GPS Module
// and appends them to he CSV-record 
// Values read and appended are: Number of satelites in vieuw
//                               Quality of GPS data
//                               GPS coordinates - latitude, longitude
//                               GPS time in GMT time
//                               GPS altitude
// Note: altitude can only be measured with > 3 satelites in view
// Ot may take a while before the GPS sensor "connects" to GPOS satelites
// Therefore start this program well ahead of rocket launch to ensure proper 
// GPS reading.
// -------------------------------------------------------------------------
void readGPS( String &msg ) {
  String  date_time = getFormattedTime();
  int     nr_sats = random( 0, 10 ); // Simulate number of satelites in view
  float   hdoop = randomFloatDec2( 0.00, 99.99 ); // Simulate HDOP value
  float   latitude = randomFloatDec2( 50.40, 50.50 );
  float   longitude = randomFloatDec2( 6.20, 6.30 );
  float   altitude = randomFloatDec2( 500.00, 2500.00 );

  msg = String( GPS_RECORD ) + DELIMETER;
  msg = msg + String( nr_sats ) + DELIMETER;
  msg = msg + String( hdoop, 2 ) + DELIMETER;
  msg = msg + String( latitude, 6 ) + DELIMETER;
  msg = msg + String( longitude, 6 ) + DELIMETER;
  msg = msg + date_time + DELIMETER;
  msg = msg + String( altitude, 2 );
}

float randomFloatDec2(float minVal, float maxVal) {
    int minInt = minVal * 100;   // schaal naar integer
    int maxInt = maxVal * 100;

    int value = random(minInt, maxInt + 1);  // willekeurig geheel getal
    return( (float)( value / 100.0 ) );                    // terug naar float met 2 decimalen
}

// ----- Handmatig datum/tijd instellen ----
void setManualDateTime(int year, int month, int day, int hour, int minute, int second)
{
    struct tm timeinfo = {};

    timeinfo.tm_year = year - 1900;  // jaar vanaf 1900
    timeinfo.tm_mon  = month - 1;    // maanden tellen vanaf 0
    timeinfo.tm_mday = day;
    timeinfo.tm_hour = hour;
    timeinfo.tm_min  = minute;
    timeinfo.tm_sec  = second;

    time_t t = mktime(&timeinfo);
    struct timeval now = { .tv_sec = t };
    settimeofday(&now, NULL);
}

String getFormattedTime() {
    struct tm timeinfo;

    if (!getLocalTime(&timeinfo)) {
        return "00-00-00:00:00:00";   // fallback
    }

    char buffer[20];
    strftime(buffer, sizeof(buffer), "%d-%m-%y:%H:%M:%S", &timeinfo);

    return String(buffer);
}