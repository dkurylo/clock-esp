#include <Arduino.h>

#include <vector>

#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServerMod.h>
#else //ESP32 or ESP32S2
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPUpdateServerMod_LittleFs.h>
#include <mbedtls/base64.h>
#endif

#include <DNSServer.h> //for Captive Portal

#include <NTPClientMod.h>
#include <WiFiUdp.h>
#include <EEPROM.h>
#include <LittleFS.h>

#include <MD_MAX72xx.h>

#include <TCData.h>
#include <TCFonts.h>

#define MAX_HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_MAX_DEVICES 4

#ifdef ESP8266
//#define MAX_DATA_PIN 0 //if using HW SPI then it's automatically MOSI 13 on ESP8266
//#define MAX_CLK_PIN 5 //if using HW SPI then it's automatically SCK 14 on ESP8266
#define MAX_CS_PIN 15 //if using HW SPI then it's SS 15 on ESP8266 (not automatically)
#else //ESP32 or ESP32S2
//#define MAX_DATA_PIN MOSI //if using HW SPI then it's automatically MOSI 11 on ESP32-S2
//#define MAX_CLK_PIN SCK //if using HW SPI then it's automatically SCK 7 on ESP32-S2
#define MAX_CS_PIN SS //if using HW SPI then it's SS 12 on ESP32-S2 (not automatically)
#endif

#ifdef ESP8266
#define BRIGHTNESS_INPUT_PIN A0
#else //ESP32 or ESP32S2
#define BRIGHTNESS_INPUT_PIN 3
#endif

#ifdef ESP8266

#elif defined(ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S2) || defined(ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32S3)
  #define HARD_RESET_PIN 14 //has to be high on load to perform hard reset
#else

#endif

//ESP8266 has 10-bit ADC (0-1023)
//ESP32 has 12-bit ADC (0-4095)
//ESP32-S2 has 13-bit ADC (0-8191)
#ifdef ESP8266
  #define ADC_RESOLUTION 10
#elif defined(ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S2) || defined(ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32S3)
  #define ADC_RESOLUTION 13
#else
  #define ADC_RESOLUTION 12 // Default for ESP32, ESP32-C3, C2, C6, H2
#endif
#define ADC_NUMBER_OF_VALUES ( 1 << ADC_RESOLUTION )
#define ADC_STEP_FOR_BYTE ( ADC_NUMBER_OF_VALUES / ( 1 << ( 8 * sizeof( uint8_t ) ) ) )

uint8_t EEPROM_FLASH_DATA_VERSION = 2; //change to next number when eeprom data format is changed. 255 is a reserved value: is set to 255 when: hard reset pin is at 3.3V (high); during factory reset procedure; when FW is loaded to a new device (EEPROM reads FF => 255)
uint8_t eepromFlashDataVersion = EEPROM_FLASH_DATA_VERSION;
const char* getFirmwareVersion() { const char* result =
#include "fw_version.txt"
; return result; }

//wifi access point configuration
const char* getWiFiAccessPointSsid() { const char* result = "Clock"; return result; };
const char* getWiFiAccessPointPassword() { const char* result = "1029384756"; return result; };
const IPAddress getWiFiAccessPointIp() { IPAddress result( 192, 168, 1, 1 ); return result; };
const IPAddress getWiFiAccessPointNetMask() { IPAddress result( 255, 255, 255, 0 ); return result; };
const uint32_t TIMEOUT_AP = 120000;

//device name
char deviceName[16 + 1];

//wifi client configuration
const char* getWiFiHostName() { const char* result = "Clock"; return result; }
const uint32_t TIMEOUT_CONNECT_WIFI_SYNC = 90000;
const uint32_t DELAY_WIFI_CONNECTION_CHECK = 60000;

//internal on-board status led config
#ifdef ESP8266
const bool INVERT_INTERNAL_LED = true;
#else //ESP32 or ESP32S2
const bool INVERT_INTERNAL_LED = false;
#endif

const bool INTERNAL_LED_IS_USED = false;
const uint16_t DELAY_INTERNAL_LED_ANIMATION_LOW = 59800;
const uint16_t DELAY_INTERNAL_LED_ANIMATION_HIGH = 200;

//time settings
const uint32_t DELAY_NTP_TIME_SYNC = 6 * 60 * 60 * 1000; //sync time every 6 hours
const uint32_t DELAY_NTP_TIME_SYNC_RETRY = 2 * 60 * 60 * 1000; //sync time every 2 hours if regular sync fails
bool isSlowSemicolonAnimation = false; //semicolon period, false = 60 blinks per minute; true = 30 blinks per minute
const uint16_t DELAY_DISPLAY_ANIMATION = 20; //led animation speed, in ms

//variables used in the code, don't change anything here
char wiFiClientSsid[32 + 1];
char wiFiClientPassword[32 + 1];

uint8_t displayFontTypeNumber = 1;
bool isDisplayBoldFontUsed = true;
bool isDisplayCompactLayoutUsed = false;
bool isDisplaySecondsShown = false;
uint8_t displayDayBrightness = 9;
uint8_t displayNightBrightness = 1;
bool isForceDisplaySync = true;
bool isForceDisplaySyncDisplayRenderOverride = false;
bool isSingleDigitHourShown = false;
bool isRotateDisplay = false;
bool isClockAnimated = false;
uint8_t animationTypeNumber = 1;

//brightness settings
#ifdef ESP8266
uint16_t sensorBrightnessNightLevel = 8; //ESP8266 has 10-bit ADC (0-1023)
uint16_t sensorBrightnessDayLevel = 512; //ESP8266 has 10-bit ADC (0-1023)
#else //ESP32 or ESP32S2
uint16_t sensorBrightnessNightLevel = 32; //ESP32 has 12-bit ADC (0-4095); ESP32-S2 has 13-bit ADC (0-8191)
uint16_t sensorBrightnessDayLevel = 4096; //ESP32 has 12-bit ADC (0-4095); ESP32-S2 has 13-bit ADC (0-8191)
#endif

const uint16_t DELAY_SENSOR_BRIGHTNESS_UPDATE_CHECK = 100;
const double SENSOR_BRIGHTNESS_LEVEL_HYSTERESIS = 0.10;
const uint16_t SENSOR_BRIGHTNESS_SUSTAINED_LEVEL_HYSTERESIS_OVERRIDE_MILLIS = 10000;

//custom datetime settings (used when there is no internet connection)
bool isCustomDateTimeSet = false;
unsigned long customDateTimeReceivedAt = 0;
unsigned long customDateTimePrevMillis = 0;
unsigned long customDateTimeReceivedSeconds = 0;

#ifdef ESP8266
ESP8266WebServer wifiWebServer(80);
ESP8266HTTPUpdateServer httpUpdater;
#else //ESP32 or ESP32S2
WebServer wifiWebServer(80);
HTTPUpdateServer httpUpdater;
#endif

DNSServer dnsServer;
WiFiUDP ntpUdp;
NTPClient timeClient(ntpUdp);
//MD_MAX72XX display = MD_MAX72XX( MAX_HARDWARE_TYPE, MAX_DATA_PIN, MAX_CLK_PIN, MAX_CS_PIN, MAX_MAX_DEVICES );
MD_MAX72XX display = MD_MAX72XX( MAX_HARDWARE_TYPE, MAX_CS_PIN, MAX_MAX_DEVICES );


//init methods
bool isFirstLoopRun = true;
unsigned long previousMillisDisplayAnimation = millis();
unsigned long previousMillisSemicolonAnimation = millis();
unsigned long previousMillisInternalLed = millis();
unsigned long previousMillisWiFiStatusCheck = millis();
unsigned long previousMillisNtpStatusCheck = millis();
unsigned long previousMillisSensorBrightnessCheck = millis();

void initVariables() {
  isFirstLoopRun = true;
  unsigned long currentMillis = millis();
  previousMillisDisplayAnimation = currentMillis;
  previousMillisSemicolonAnimation = currentMillis;
  previousMillisInternalLed = currentMillis;
  previousMillisWiFiStatusCheck = currentMillis;
  previousMillisNtpStatusCheck = currentMillis;
  previousMillisSensorBrightnessCheck = currentMillis;
}



//auxiliary functions
unsigned long calculateDiffMillis( unsigned long startMillis, unsigned long endMillis ) { //this function accounts for millis overflow when calculating millis difference
  if( endMillis >= startMillis ) {
    return endMillis - startMillis;
  } else {
    return( ULONG_MAX - startMillis ) + endMillis + 1;
  }
}

String getContentType( String fileExtension ) {
  String contentType = "";
  if( fileExtension == F("htm") || fileExtension == F("html") ) {
    contentType = F("text/html");
  } else if( fileExtension == F("gif") ) {
    contentType = F("image/gif");
  } else if( fileExtension == F("png") ) {
    contentType = F("image/png");
  } else if( fileExtension == F("webp") ) {
    contentType = F("image/webp");
  } else if( fileExtension == F("bmp") ) {
    contentType = F("image/bmp");
  } else if( fileExtension == F("ico") ) {
    contentType = F("image/x-icon");
  } else if( fileExtension == F("svg") ) {
    contentType = F("image/svg+xml");
  } else if( fileExtension == F("js") ) {
    contentType = F("text/javascript");
  } else if( fileExtension == F("css") ) {
    contentType = F("text/css");
  } else if( fileExtension == F("json") ) {
    contentType = F("application/json");
  } else if( fileExtension == F("xml") ) {
    contentType = F("application/xml");
  } else if( fileExtension == F("txt") ) {
    contentType = F("text/plain");
  } else if( fileExtension == F("pdf") ) {
    contentType = F("application/pdf");
  } else if( fileExtension == F("zip") ) {
    contentType = F("application/zip");
  } else if( fileExtension == F("gz") ) {
    contentType = F("application/gzip");
  } else if( fileExtension == F("mp3") ) {
    contentType = F("audio/mp3");
  } else if( fileExtension == F("mp4") ) {
    contentType = F("video/mp4");
  } else {
    contentType = F("application/octet-stream");
  }
  return contentType;
}

File getFileFromFlash( String fileName ) {
  bool isGzippedFileRequested = fileName.endsWith( F(".gz") );

  File root = LittleFS.open("/", "r");
  if (!root || !root.isDirectory()) {
    Serial.println("Failed to open directory");
  } else {
    Serial.println("Succeeded to open directory");
  }
  File file2 = root.openNextFile();
  while (file2) {
    Serial.print(file2.name());
    file2 = root.openNextFile();
  }

  File file = LittleFS.open( "/" + fileName + ( isGzippedFileRequested ? "" : ".gz" ), "r" );
  if( !isGzippedFileRequested && !file ) {
    file = LittleFS.open( "/" + fileName, "r" );
  }
  return file;
}


//eeprom functionality
const uint16_t eepromFlashDataVersionIndex = 0;
const uint16_t eepromWiFiSsidIndex = eepromFlashDataVersionIndex + 1;
const uint16_t eepromWiFiPasswordIndex = eepromWiFiSsidIndex + sizeof(wiFiClientSsid);
const uint16_t eepromDeviceNameIndex = eepromWiFiPasswordIndex + sizeof(wiFiClientPassword);
const uint16_t eepromDisplayFontTypeNumberIndex = eepromDeviceNameIndex + sizeof(deviceName);
const uint16_t eepromIsFontBoldUsedIndex = eepromDisplayFontTypeNumberIndex + 1;
const uint16_t eepromIsDisplaySecondsShownIndex = eepromIsFontBoldUsedIndex + 1;
const uint16_t eepromDisplayDayBrightnessIndex = eepromIsDisplaySecondsShownIndex + 1;
const uint16_t eepromDisplayNightBrightnessIndex = eepromDisplayDayBrightnessIndex + 1;
const uint16_t eepromSensorBrightnessDayLevelIndex = eepromDisplayNightBrightnessIndex + 1;
const uint16_t eepromSensorBrightnessNightLevelIndex = eepromSensorBrightnessDayLevelIndex + 1;
const uint16_t eepromIsSingleDigitHourShownIndex = eepromSensorBrightnessNightLevelIndex + 1;
const uint16_t eepromIsRotateDisplayIndex = eepromIsSingleDigitHourShownIndex + 1;
const uint16_t eepromIsSlowSemicolonAnimationIndex = eepromIsRotateDisplayIndex + 1;
const uint16_t eepromIsClockAnimatedIndex = eepromIsSlowSemicolonAnimationIndex + 1;
const uint16_t eepromAnimationTypeNumberIndex = eepromIsClockAnimatedIndex + 1;
const uint16_t eepromIsCompactLayoutShownIndex = eepromAnimationTypeNumberIndex + 1;
const uint16_t eepromCustomFontIndex = eepromIsCompactLayoutShownIndex + 1;
const uint16_t eepromLastByteIndex = eepromCustomFontIndex + TCFonts::FONT_SYMBOLS * TCFonts::FONT_HEIGHT;

const uint16_t EEPROM_ALLOCATED_SIZE = eepromLastByteIndex;
void initEeprom() {
  EEPROM.begin( EEPROM_ALLOCATED_SIZE ); //init this many bytes
}

bool readEepromCharArray( const uint16_t& eepromIndex, char* variableWithValue, uint8_t maxLength, bool doApplyValue ) {
  bool isDifferentValue = false;
  uint16_t eepromStartIndex = eepromIndex;
  for( uint16_t i = eepromStartIndex; i < eepromStartIndex + maxLength; i++ ) {
    char eepromChar = EEPROM.read(i);
    if( doApplyValue ) {
      variableWithValue[i-eepromStartIndex] = eepromChar;
    } else {
      isDifferentValue = isDifferentValue || variableWithValue[i-eepromStartIndex] != eepromChar;
    }
  }
  return isDifferentValue;
}

bool writeEepromCharArray( const uint16_t& eepromIndex, char* newValue, uint8_t maxLength ) {
  bool isDifferentValue = readEepromCharArray( eepromIndex, newValue, maxLength, false );
  if( !isDifferentValue ) return false;
  uint16_t eepromStartIndex = eepromIndex;
  for( uint16_t i = eepromStartIndex; i < eepromStartIndex + maxLength; i++ ) {
    EEPROM.write( i, newValue[i-eepromStartIndex] );
  }
  EEPROM.commit();
  delay( 20 );
  return true;
}

uint8_t readEepromUintValue( const uint16_t& eepromIndex, uint8_t& variableWithValue, bool doApplyValue ) {
  uint8_t eepromValue = EEPROM.read( eepromIndex );
  if( doApplyValue ) {
    variableWithValue = eepromValue;
  }
  return eepromValue;
}

bool writeEepromUintValue( const uint16_t& eepromIndex, uint8_t newValue ) {
  bool eepromWritten = false;
  if( readEepromUintValue( eepromIndex, newValue, false ) != newValue ) {
    EEPROM.write( eepromIndex, newValue );
    eepromWritten = true;
  }
  if( eepromWritten ) {
    EEPROM.commit();
    delay( 20 );
  }
  return eepromWritten;
}

bool readEepromBoolValue( const uint16_t& eepromIndex, bool& variableWithValue, bool doApplyValue ) {
  uint8_t eepromValue = EEPROM.read( eepromIndex ) != 0 ? 1 : 0;
  if( doApplyValue ) {
    variableWithValue = eepromValue;
  }
  return eepromValue;
}

bool writeEepromBoolValue( const uint16_t& eepromIndex, bool newValue ) {
  bool eepromWritten = false;
  if( readEepromBoolValue( eepromIndex, newValue, false ) != newValue ) {
    EEPROM.write( eepromIndex, newValue ? 1 : 0 );
    eepromWritten = true;
  }
  if( eepromWritten ) {
    EEPROM.commit();
    delay( 20 );
  }
  return eepromWritten;
}

void readEepromFontData() {
  uint8_t (*fontToUse)[TCFonts::FONT_HEIGHT] = TCFonts::getCustomFont();
  for( uint16_t symbolIndex = 0; symbolIndex < TCFonts::FONT_SYMBOLS; symbolIndex++ ) {
    for( uint8_t byteIndex = 0; byteIndex < TCFonts::FONT_HEIGHT; byteIndex++ ) {
      uint16_t eepromIndex = eepromCustomFontIndex + ( symbolIndex * TCFonts::FONT_HEIGHT ) + byteIndex;
      fontToUse[symbolIndex][byteIndex] = EEPROM.read( eepromIndex );
    }
  }
}

bool writeEepromFontData( bool doErase ) {
  bool eepromWritten = false;
  uint8_t (*fontToUse)[TCFonts::FONT_HEIGHT] = TCFonts::getCustomFont();
  for( uint16_t symbolIndex = 0; symbolIndex < TCFonts::FONT_SYMBOLS; symbolIndex++ ) {
    for( uint8_t byteIndex = 0; byteIndex < TCFonts::FONT_HEIGHT; byteIndex++ ) {
      uint16_t eepromIndex = eepromCustomFontIndex + ( symbolIndex * TCFonts::FONT_HEIGHT ) + byteIndex;
      uint8_t byteStored = EEPROM.read( eepromIndex );
      uint8_t byteToStore = doErase ? 0 : fontToUse[symbolIndex][byteIndex];
      if( byteStored != byteToStore ) {
        eepromWritten = true;
        EEPROM.write( eepromIndex, byteToStore );
      }
    }
  }
  if( eepromWritten ) {
    EEPROM.commit();
    delay( 20 );
  }
  return eepromWritten;
}

void loadEepromData() {
  if( eepromFlashDataVersion != 255 ) {
    readEepromUintValue( eepromFlashDataVersionIndex, eepromFlashDataVersion, true );
  }

  if( eepromFlashDataVersion != 255 && eepromFlashDataVersion == EEPROM_FLASH_DATA_VERSION ) {

    readEepromCharArray( eepromWiFiSsidIndex, wiFiClientSsid, sizeof(wiFiClientSsid), true );
    readEepromCharArray( eepromWiFiPasswordIndex, wiFiClientPassword, sizeof(wiFiClientPassword), true );
    readEepromCharArray( eepromDeviceNameIndex, deviceName, sizeof(deviceName), true );
    readEepromUintValue( eepromDisplayFontTypeNumberIndex, displayFontTypeNumber, true );
    if( displayFontTypeNumber < 1 || displayFontTypeNumber > TCFonts::NUMBER_OF_FONTS_SUPPORTED ) displayFontTypeNumber = 1;
    readEepromBoolValue( eepromIsFontBoldUsedIndex, isDisplayBoldFontUsed, true );
    readEepromBoolValue( eepromIsDisplaySecondsShownIndex, isDisplaySecondsShown, true );
    readEepromUintValue( eepromDisplayDayBrightnessIndex, displayDayBrightness, true );
    if( displayDayBrightness > 15 ) displayDayBrightness = 15;
    readEepromUintValue( eepromDisplayNightBrightnessIndex, displayNightBrightness, true );
    if( displayNightBrightness > 15 ) displayNightBrightness = 15;
    if( displayNightBrightness > displayDayBrightness ) displayNightBrightness = displayDayBrightness;
    uint8_t eepromSensorBrightnessDayLevel = 0;
    readEepromUintValue( eepromSensorBrightnessDayLevelIndex, eepromSensorBrightnessDayLevel, true );
    sensorBrightnessDayLevel = ( (uint16_t)eepromSensorBrightnessDayLevel ) * ADC_STEP_FOR_BYTE;
    uint8_t eepromSensorBrightnessNightLevel = 0;
    readEepromUintValue( eepromSensorBrightnessNightLevelIndex, eepromSensorBrightnessNightLevel, true );
    sensorBrightnessNightLevel = ( (uint16_t)eepromSensorBrightnessNightLevel ) * ADC_STEP_FOR_BYTE;
    if( sensorBrightnessNightLevel > sensorBrightnessDayLevel ) sensorBrightnessNightLevel = sensorBrightnessDayLevel;
    readEepromBoolValue( eepromIsSingleDigitHourShownIndex, isSingleDigitHourShown, true );
    readEepromBoolValue( eepromIsRotateDisplayIndex, isRotateDisplay, true );
    readEepromBoolValue( eepromIsSlowSemicolonAnimationIndex, isSlowSemicolonAnimation, true );
    readEepromBoolValue( eepromIsClockAnimatedIndex, isClockAnimated, true );
    readEepromUintValue( eepromAnimationTypeNumberIndex, animationTypeNumber, true );
    if( animationTypeNumber < 1 || animationTypeNumber > TCData::NUMBER_OF_ANIMATIONS_SUPPORTED ) animationTypeNumber = 1;
    readEepromBoolValue( eepromIsCompactLayoutShownIndex, isDisplayCompactLayoutUsed, true );
    readEepromFontData();

  } else { //fill EEPROM with default values when starting the new board
    writeEepromUintValue( eepromFlashDataVersionIndex, EEPROM_FLASH_DATA_VERSION );
    eepromFlashDataVersion = EEPROM_FLASH_DATA_VERSION;

    writeEepromCharArray( eepromWiFiSsidIndex, wiFiClientSsid, sizeof(wiFiClientSsid) );
    writeEepromCharArray( eepromWiFiPasswordIndex, wiFiClientPassword, sizeof(wiFiClientPassword) );
    writeEepromCharArray( eepromDeviceNameIndex, deviceName, sizeof(deviceName) );
    writeEepromUintValue( eepromDisplayFontTypeNumberIndex, displayFontTypeNumber );
    writeEepromBoolValue( eepromIsFontBoldUsedIndex, isDisplayBoldFontUsed );
    writeEepromBoolValue( eepromIsDisplaySecondsShownIndex, isDisplaySecondsShown );
    writeEepromUintValue( eepromDisplayDayBrightnessIndex, displayDayBrightness );
    writeEepromUintValue( eepromDisplayNightBrightnessIndex, displayNightBrightness );
    writeEepromUintValue( eepromSensorBrightnessDayLevelIndex, (uint8_t)( sensorBrightnessDayLevel / ADC_STEP_FOR_BYTE ) );
    writeEepromUintValue( eepromSensorBrightnessNightLevelIndex, (uint8_t)( sensorBrightnessNightLevel / ADC_STEP_FOR_BYTE ) );
    writeEepromBoolValue( eepromIsSingleDigitHourShownIndex, isSingleDigitHourShown );
    writeEepromBoolValue( eepromIsRotateDisplayIndex, isRotateDisplay );
    writeEepromBoolValue( eepromIsSlowSemicolonAnimationIndex, isSlowSemicolonAnimation );
    writeEepromBoolValue( eepromIsClockAnimatedIndex, isClockAnimated );
    writeEepromUintValue( eepromAnimationTypeNumberIndex, animationTypeNumber );
    writeEepromBoolValue( eepromIsCompactLayoutShownIndex, isDisplayCompactLayoutUsed );
    writeEepromFontData( true );

    loadEepromData();
  }
}


//wifi connection as AP
bool isApInitialized = false;
unsigned long apStartedMillis;

void createAccessPoint() {
  if( isApInitialized ) return;
  isApInitialized = true;
  apStartedMillis = millis();
  Serial.print( F("Creating WiFi AP...") );
  WiFi.softAPConfig( getWiFiAccessPointIp(), getWiFiAccessPointIp(), getWiFiAccessPointNetMask() );

  #ifdef ESP8266
  WiFi.softAP( ( String( getWiFiAccessPointSsid() ) + " " + String( ESP.getChipId() ) ).c_str(), getWiFiAccessPointPassword(), 0, false );
  #else //ESP32 or ESP32S2
  String macAddress = String( ESP.getEfuseMac() );
  WiFi.softAP( ( String( getWiFiAccessPointSsid() ) + " " + macAddress.substring( macAddress.length() - 4 ) ).c_str(), getWiFiAccessPointPassword(), 0, false );
  #endif

  IPAddress accessPointIp = WiFi.softAPIP();
  dnsServer.start( 53, "*", accessPointIp );
  Serial.println( String( F(" done | IP: ") ) + accessPointIp.toString() );
}

void shutdownAccessPoint() {
  if( !isApInitialized ) return;
  isApInitialized = false;
  Serial.print( F("Shutting down WiFi AP...") );
  dnsServer.stop();
  WiFi.softAPdisconnect( true );
  Serial.println( F(" done") );
}


//internal led functionality
uint8_t internalLedStatus = HIGH;

uint8_t getInternalLedStatus() {
   return internalLedStatus;
}

void setInternalLedStatus( uint8_t status ) {
  internalLedStatus = status;

  if( INTERNAL_LED_IS_USED ) {
    digitalWrite( LED_BUILTIN, INVERT_INTERNAL_LED ? ( status == HIGH ? LOW : HIGH ) : status );
  }
}

void initInternalLed() {
  if( INTERNAL_LED_IS_USED ) {
    pinMode( LED_BUILTIN, OUTPUT );
  }
  setInternalLedStatus( internalLedStatus );
}


//display functionality
void forceDisplaySync() {
  isForceDisplaySync = true;
  isForceDisplaySyncDisplayRenderOverride = true;
}


//time of day functionality
bool isWithinDstBoundaries( time_t dt ) {
  struct tm *timeinfo = gmtime(&dt);

  // Get the last Sunday of March in the current year
  struct tm lastMarchSunday = {0};
  lastMarchSunday.tm_year = timeinfo->tm_year;
  lastMarchSunday.tm_mon = 2; // March (0-based)
  lastMarchSunday.tm_mday = 31; // Last day of March
  mktime(&lastMarchSunday);
  while (lastMarchSunday.tm_wday != 0) { // 0 = Sunday
    lastMarchSunday.tm_mday--;
    mktime(&lastMarchSunday);
  }
  lastMarchSunday.tm_hour = 1;
  lastMarchSunday.tm_min = 0;
  lastMarchSunday.tm_sec = 0;

  // Get the last Sunday of October in the current year
  struct tm lastOctoberSunday = {0};
  lastOctoberSunday.tm_year = timeinfo->tm_year;
  lastOctoberSunday.tm_mon = 9; // October (0-based)
  lastOctoberSunday.tm_mday = 31; // Last day of October
  mktime(&lastOctoberSunday);
  while (lastOctoberSunday.tm_wday != 0) { // 0 = Sunday
    lastOctoberSunday.tm_mday--;
    mktime(&lastOctoberSunday);
  }
  lastOctoberSunday.tm_hour = 1;
  lastOctoberSunday.tm_min = 0;
  lastOctoberSunday.tm_sec = 0;

  // Convert the struct tm to time_t
  time_t lastMarchSunday_t = mktime(&lastMarchSunday);
  time_t lastOctoberSunday_t = mktime(&lastOctoberSunday);

  // Check if the datetime is within the DST boundaries
  return lastMarchSunday_t <= dt && dt < lastOctoberSunday_t;
}


//display functionality
double displayCurrentBrightness = static_cast<double>(displayNightBrightness);
double displayPreviousBrightness = -1.0;
double sensorBrightnessAverage = -1.0;
int brightnessDiffSustainedMillis = 0;
float steepnessCoefficient = 2.0;

void calculateDisplayBrightness() {
  uint16_t currentBrightness = analogRead( BRIGHTNESS_INPUT_PIN );
  if( sensorBrightnessAverage < 0 ) {
    sensorBrightnessAverage = (double)currentBrightness;
  } else {
    uint8_t sensorBrightnessSamples = 15;
    sensorBrightnessAverage = ( sensorBrightnessAverage * ( static_cast<double>(sensorBrightnessSamples) - static_cast<double>(1.0) ) + currentBrightness ) / static_cast<double>(sensorBrightnessSamples);
  }

  if( sensorBrightnessAverage >= sensorBrightnessDayLevel ) {
    displayCurrentBrightness = static_cast<double>(displayDayBrightness);
  } else if( sensorBrightnessAverage <= sensorBrightnessNightLevel ) {
    displayCurrentBrightness = static_cast<double>(displayNightBrightness);
  } else {
    float normalizedSensorBrightnessAverage = (float)(sensorBrightnessAverage - sensorBrightnessNightLevel) / ( sensorBrightnessDayLevel - sensorBrightnessNightLevel );
    float easingCoefficient = 1 - powf( 1 - normalizedSensorBrightnessAverage, steepnessCoefficient );
    displayCurrentBrightness = displayNightBrightness + static_cast<double>( (displayDayBrightness - displayNightBrightness ) * easingCoefficient );
    //displayCurrentBrightness = displayNightBrightness + static_cast<double>( displayDayBrightness - displayNightBrightness ) * ( sensorBrightnessAverage - sensorBrightnessNightLevel ) / ( sensorBrightnessDayLevel - sensorBrightnessNightLevel );
  }
}

void setDisplayBrightness( uint8_t displayNewBrightness ) {
  display.control( MD_MAX72XX::INTENSITY, displayNewBrightness );
  display.control( MD_MAX72XX::UPDATE, MD_MAX72XX::OFF );
}

void setDisplayBrightness( bool isInit ) {
  bool updateDisplayBrightness = false;
  uint8_t displayCurrentBrightnessInt = static_cast<uint8_t>( round( displayCurrentBrightness ) );
  uint8_t displayPreviousBrightnessInt = static_cast<uint8_t>( round( displayPreviousBrightness ) );

  if( isInit || displayPreviousBrightness < 0 ) {
    updateDisplayBrightness = true;
    brightnessDiffSustainedMillis = 0;
  } else if( displayCurrentBrightnessInt != displayPreviousBrightnessInt ) {
    if( ( displayCurrentBrightness > displayPreviousBrightness && ( displayCurrentBrightness > ( ceil( displayCurrentBrightness ) - 0.5 + SENSOR_BRIGHTNESS_LEVEL_HYSTERESIS ) ) ) ||
        ( displayCurrentBrightness < displayPreviousBrightness && ( displayCurrentBrightness < ( floor(  displayCurrentBrightness ) + 0.5 - SENSOR_BRIGHTNESS_LEVEL_HYSTERESIS ) ) ) ) {
      updateDisplayBrightness = true;
      brightnessDiffSustainedMillis = 0;
    } else {
      brightnessDiffSustainedMillis += DELAY_SENSOR_BRIGHTNESS_UPDATE_CHECK;
      if( brightnessDiffSustainedMillis >= SENSOR_BRIGHTNESS_SUSTAINED_LEVEL_HYSTERESIS_OVERRIDE_MILLIS ) {
        updateDisplayBrightness = true;
        brightnessDiffSustainedMillis = 0;
      }
    }
  } else {
    brightnessDiffSustainedMillis = 0;
  }

  if( updateDisplayBrightness ) {
    setDisplayBrightness( displayCurrentBrightnessInt );
    displayPreviousBrightness = displayCurrentBrightness;
  }
}

void initDisplayPhase1() {
  display.begin();
  display.control( MD_MAX72XX::INTENSITY, 0 );
  display.clear();
}

void initDisplayPhase2() {
  calculateDisplayBrightness();
  setDisplayBrightness( true );
  display.clear();
}

void brightnessProcessLoopTick() {
  if( calculateDiffMillis( previousMillisSensorBrightnessCheck, millis() ) >= DELAY_SENSOR_BRIGHTNESS_UPDATE_CHECK ) {
    calculateDisplayBrightness();
    setDisplayBrightness( false );
    previousMillisSensorBrightnessCheck = millis();
  }
}

const uint8_t DISPLAY_WIDTH = 32;
const uint8_t DISPLAY_HEIGHT = 8;

String textToDisplayLargeAnimated = "";
bool isDisplayAnimationInProgress = false;
unsigned long displayAnimationStartedMillis;
const unsigned long displayAnimationStepLengthMillis = 40;

void cancelDisplayAnimation() {
  textToDisplayLargeAnimated = "";
  isDisplayAnimationInProgress = false;
}

bool isSemicolonShown = true;
void renderDisplayText( String hourStr, String minuteStr, String secondStr, bool doAnimate ) {
  unsigned long currentMillis = millis();

  char charsAnimatable[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9' };
  uint8_t charsAnimatableCount = sizeof(charsAnimatable) / sizeof(charsAnimatable[0]);
  unsigned long displayAnimationLengthMillis = TCFonts::FONT_HEIGHT * displayAnimationStepLengthMillis;
  if( animationTypeNumber == 1 || animationTypeNumber == 2 || animationTypeNumber == 4 ) {
    displayAnimationLengthMillis += displayAnimationStepLengthMillis;
  }

  String textToDisplayLarge = hourStr + ( isSemicolonShown ? ":" : "\t" ) + minuteStr;
  String textToDisplaySmall = secondStr;

  if( !doAnimate ) {
    textToDisplayLargeAnimated = "";
    isDisplayAnimationInProgress = false;
  } else if( textToDisplayLargeAnimated == "" ) {
    textToDisplayLargeAnimated = textToDisplayLarge;
    isDisplayAnimationInProgress = false;
  } else if( !isDisplayAnimationInProgress ) {
    if( textToDisplayLarge.length() == textToDisplayLargeAnimated.length() ) {
      bool doAnimate = false;
      for( size_t charToDisplayIndex = 0; charToDisplayIndex < textToDisplayLarge.length(); ++charToDisplayIndex ) {
        char charToDisplay = textToDisplayLarge.charAt( charToDisplayIndex );
        bool isAnimatable = false;
        for( uint8_t i = 0; i < charsAnimatableCount; i++ ) {
          if( charToDisplay != charsAnimatable[i] ) continue;
          isAnimatable = true;
          break;
        }
        if( !isAnimatable || textToDisplayLargeAnimated.charAt( charToDisplayIndex ) == charToDisplay ) continue;
        doAnimate = true;
        break;
      }
      if( doAnimate ) {
        isDisplayAnimationInProgress = true;
        displayAnimationStartedMillis = currentMillis;
      }
    } else {
      textToDisplayLargeAnimated = textToDisplayLarge;
    }
  } else if( isDisplayAnimationInProgress && calculateDiffMillis( displayAnimationStartedMillis, currentMillis ) > displayAnimationLengthMillis ) {
    isDisplayAnimationInProgress = false;
    textToDisplayLargeAnimated = textToDisplayLarge;
  }

  uint8_t displayWidthUsed = 0;
  bool isWideTextRendered = !isDisplaySecondsShown;

  for( size_t charToDisplayIndex = 0; charToDisplayIndex < textToDisplayLarge.length(); ++charToDisplayIndex ) {
    char charToDisplay = textToDisplayLarge.charAt( charToDisplayIndex );

    uint8_t charLpWidth = TCFonts::getSymbolLp( displayFontTypeNumber, charToDisplay, isDisplayCompactLayoutUsed, isWideTextRendered, false );
    uint8_t charWidth = TCFonts::getSymbolWidth( displayFontTypeNumber, charToDisplay, isDisplayCompactLayoutUsed, isWideTextRendered, false );
    uint8_t charRpWidth = TCFonts::getSymbolRp( displayFontTypeNumber, charToDisplay, isDisplayCompactLayoutUsed, isWideTextRendered, false );

    if( displayWidthUsed + charLpWidth > DISPLAY_WIDTH ) {
      charLpWidth = DISPLAY_WIDTH - displayWidthUsed;
    }
    if( displayWidthUsed + charLpWidth + charWidth > DISPLAY_WIDTH ) {
      charWidth = DISPLAY_WIDTH - displayWidthUsed - charLpWidth;
    }
    if( displayWidthUsed + charLpWidth + charWidth + charRpWidth > DISPLAY_WIDTH ) {
      charRpWidth = DISPLAY_WIDTH - displayWidthUsed - charLpWidth - charWidth;
    }

    if( charWidth == 0 ) continue;

    for( uint8_t charX = 0; charX < charLpWidth; ++charX ) {
      for( uint8_t displayY = 0; displayY < DISPLAY_HEIGHT; ++displayY ) {
        display.setPoint(
          isRotateDisplay ? ( DISPLAY_HEIGHT - 1 - displayY ) : ( displayY ),
          isRotateDisplay ? displayWidthUsed : ( DISPLAY_WIDTH - 1 - displayWidthUsed ),
          false
        );
      }
      displayWidthUsed++;
    }

    std::vector<uint8_t> charImage = TCFonts::getSymbol( displayFontTypeNumber, charToDisplay, isDisplayCompactLayoutUsed, isDisplayBoldFontUsed, isWideTextRendered, false, false );
    std::vector<uint8_t> charImagePrevious;
    if( isDisplayAnimationInProgress ) {
      bool isAnimatable = false;
      for( uint8_t i = 0; i < charsAnimatableCount; i++ ) {
        if( charToDisplay != charsAnimatable[i] ) continue;
        isAnimatable = true;
        break;
      }
      if( isAnimatable && charToDisplayIndex < textToDisplayLargeAnimated.length() ) {
        char charToDisplayPrevious = textToDisplayLargeAnimated.charAt( charToDisplayIndex );
        if( charToDisplay != charToDisplayPrevious ) {
          charImagePrevious = TCFonts::getSymbol( displayFontTypeNumber, charToDisplayPrevious, isDisplayCompactLayoutUsed, isDisplayBoldFontUsed, isWideTextRendered, false, false );
        }
      }
    }

    for( uint8_t charX = 0; charX < charWidth; ++charX ) {
      for( uint8_t displayY = 0; displayY < DISPLAY_HEIGHT; ++displayY ) {
        bool isPointEnabled = false;
        uint8_t charShiftY = DISPLAY_HEIGHT - charImage.size();
        if( displayY >= charShiftY ) {
          uint8_t charY = displayY - charShiftY;
          isPointEnabled = ( charImage[charY] >> ( DISPLAY_HEIGHT - 1 - charX ) ) & 1;
          if( isDisplayAnimationInProgress && charImagePrevious.size() > 0 ) {
            uint8_t animationSteps = charImage.size();
            if( animationTypeNumber == 1 || animationTypeNumber == 2 || animationTypeNumber == 4 ) {
              animationSteps++;
            }
            unsigned long animationStepSize = displayAnimationLengthMillis / animationSteps;
            int currentAnimationStep = calculateDiffMillis( displayAnimationStartedMillis, currentMillis ) / animationStepSize;
            if( currentAnimationStep >= animationSteps ) {
              currentAnimationStep = animationSteps - 1;
            }
            if( animationTypeNumber == 1 ) {
              isPointEnabled = currentAnimationStep == charY
                               ? 0
                               : ( currentAnimationStep < charY
                                 ? ( ( charImagePrevious[charY - currentAnimationStep - 1] >> ( DISPLAY_HEIGHT - 1 - charX ) ) & 1 )
                                 : ( ( charImage[charY + charImage.size() - currentAnimationStep] >> ( DISPLAY_HEIGHT - 1 - charX ) ) & 1 ) );
            } else if( animationTypeNumber == 2 ) {
              isPointEnabled = currentAnimationStep == charY
                               ? 0
                               : ( currentAnimationStep < charY
                                   ? ( ( charImagePrevious[charY] >> ( DISPLAY_HEIGHT - 1 - charX ) ) & 1 )
                                   : ( ( charImage[charY] >> ( DISPLAY_HEIGHT - 1 - charX ) ) & 1 ) );

            } else if( animationTypeNumber == 3 ) {
              isPointEnabled = currentAnimationStep < charY
                                ? ( ( charImagePrevious[charY] >> ( DISPLAY_HEIGHT - 1 - charX ) ) & 1 )
                                : ( ( charImage[charY] >> ( DISPLAY_HEIGHT - 1 - charX ) ) & 1 );
            } else if( animationTypeNumber == 4 ) {
              isPointEnabled = currentAnimationStep == charY
                               ? 0
                               : ( currentAnimationStep < charY
                                 ? ( ( charImagePrevious[charY - currentAnimationStep - 1] >> ( DISPLAY_HEIGHT - 1 - charX ) ) & 1 )
                                 : ( ( charImage[charY] >> ( DISPLAY_HEIGHT - 1 - charX ) ) & 1 ) );
            } else if( animationTypeNumber == 5 ) {
              isPointEnabled = ( currentAnimationStep < charY
                                ? ( ( charImagePrevious[charY - currentAnimationStep - 1] >> ( DISPLAY_HEIGHT - 1 - charX ) ) & 1 )
                                : ( ( charImage[charY] >> ( DISPLAY_HEIGHT - 1 - charX ) ) & 1 ) );
            }
          }
        }
        display.setPoint(
          isRotateDisplay ? ( DISPLAY_HEIGHT - 1 - displayY ) : ( displayY ),
          isRotateDisplay ? displayWidthUsed : ( DISPLAY_WIDTH - 1 - displayWidthUsed ),
          isPointEnabled
        );
      }
      displayWidthUsed++;
    }

    for( uint8_t charX = 0; charX < charRpWidth; ++charX ) {
      for( uint8_t displayY = 0; displayY < DISPLAY_HEIGHT; ++displayY ) {
        display.setPoint(
          isRotateDisplay ? ( DISPLAY_HEIGHT - 1 - displayY ) : ( displayY ),
          isRotateDisplay ? displayWidthUsed : ( DISPLAY_WIDTH - 1 - displayWidthUsed ),
          false
        );
      }
      displayWidthUsed++;
    }
  }

  if( isDisplaySecondsShown ) {
    for( size_t charToDisplayIndex = 0; charToDisplayIndex < textToDisplaySmall.length(); ++charToDisplayIndex ) {
      char charToDisplay = textToDisplaySmall.charAt( charToDisplayIndex );

      uint8_t charLpWidth = TCFonts::getSymbolLp( displayFontTypeNumber, charToDisplay, isDisplayCompactLayoutUsed, isWideTextRendered, true );
      uint8_t charWidth = TCFonts::getSymbolWidth( displayFontTypeNumber, charToDisplay, isDisplayCompactLayoutUsed, isWideTextRendered, true );
      uint8_t charRpWidth = TCFonts::getSymbolRp( displayFontTypeNumber, charToDisplay, isDisplayCompactLayoutUsed, isWideTextRendered, true );

      if( displayWidthUsed + charLpWidth > DISPLAY_WIDTH ) {
        charLpWidth = DISPLAY_WIDTH - displayWidthUsed;
      }
      if( displayWidthUsed + charLpWidth + charWidth > DISPLAY_WIDTH ) {
        charWidth = DISPLAY_WIDTH - displayWidthUsed - charLpWidth;
      }
      if( displayWidthUsed + charLpWidth + charWidth + charRpWidth > DISPLAY_WIDTH ) {
        charRpWidth = DISPLAY_WIDTH - displayWidthUsed - charLpWidth - charWidth;
      }
      
      if( charWidth == 0 ) continue;

      for( uint8_t charX = 0; charX < charLpWidth; ++charX ) {
        for( uint8_t displayY = 0; displayY < DISPLAY_HEIGHT; ++displayY ) {
          display.setPoint(
            isRotateDisplay ? ( DISPLAY_HEIGHT - 1 - displayY ) : ( displayY ),
            isRotateDisplay ? displayWidthUsed : ( DISPLAY_WIDTH - 1 - displayWidthUsed ),
            false
          );
        }
        displayWidthUsed++;
      }

      std::vector<uint8_t> charImage = TCFonts::getSymbol( displayFontTypeNumber, charToDisplay, isDisplayCompactLayoutUsed, isDisplayBoldFontUsed, isWideTextRendered, true, false );
      for( uint8_t charX = 0; charX < charWidth; ++charX ) {
        for( uint8_t displayY = 0; displayY < DISPLAY_HEIGHT; ++displayY ) {
          bool isPointEnabled = false;
          uint8_t charShiftY = DISPLAY_HEIGHT - charImage.size();
          if( displayY >= charShiftY ) {
            uint8_t charY = displayY - charShiftY;
            isPointEnabled = ( charImage[charY] >> ( DISPLAY_HEIGHT - 1 - charX ) ) & 1;
          }
          display.setPoint(
            isRotateDisplay ? ( DISPLAY_HEIGHT - 1 - displayY ) : ( displayY ),
            isRotateDisplay ? displayWidthUsed : ( DISPLAY_WIDTH - 1 - displayWidthUsed ),
            isPointEnabled
          );
        }
        displayWidthUsed++;
      }

      for( uint8_t charX = 0; charX < charRpWidth; ++charX ) {
        for( uint8_t displayY = 0; displayY < DISPLAY_HEIGHT; ++displayY ) {
          display.setPoint(
            isRotateDisplay ? ( DISPLAY_HEIGHT - 1 - displayY ) : ( displayY ),
            isRotateDisplay ? displayWidthUsed : ( DISPLAY_WIDTH - 1 - displayWidthUsed ),
            false
          );
        }
        displayWidthUsed++;
      }
    }
  }

  if( displayWidthUsed < DISPLAY_WIDTH ) { //fill the rest of the screen
    for( uint8_t charX = 0; charX < DISPLAY_WIDTH - displayWidthUsed + 1; ++charX ) {
      for( uint8_t charY = 0; charY < DISPLAY_HEIGHT; ++charY ) {
        display.setPoint(
          isRotateDisplay ? ( DISPLAY_HEIGHT - 1 - charY ) : ( charY ),
          isRotateDisplay ? displayWidthUsed : ( DISPLAY_WIDTH - 1 - displayWidthUsed ),
          false
        );
      }
      displayWidthUsed++;
    }
  }
}

bool timeCanBeCalculated() {
  return timeClient.isTimeSet() || isCustomDateTimeSet;
}

void calculateTimeToShow( String& hourStr, String& minuteStr, String& secondStr, bool isSingleDigitHourShownCurrently ) {
  if( timeCanBeCalculated() ) {
    time_t dt = 0;
    if( timeClient.isTimeSet() ) {
      dt = timeClient.getEpochTime();
    } else if( isCustomDateTimeSet ) {
      unsigned long currentMillis = millis();
      if( currentMillis >= customDateTimeReceivedAt && customDateTimePrevMillis < customDateTimeReceivedAt ) {
        unsigned long wrappedSeconds = ULONG_MAX / 1000 + 1;
        customDateTimeReceivedAt = customDateTimeReceivedAt - ( 1000 - ULONG_MAX % 1000 );
        customDateTimeReceivedSeconds = customDateTimeReceivedSeconds + wrappedSeconds;
      }
      customDateTimePrevMillis = currentMillis;
      unsigned long timeDiff = calculateDiffMillis( customDateTimeReceivedAt, currentMillis );
      dt = customDateTimeReceivedSeconds + timeDiff / 1000;
    }

    struct tm* dtStruct = localtime(&dt);
    uint8_t hour = dtStruct->tm_hour;
    hour += isWithinDstBoundaries( dt ) ? 3 : 2;
    if( hour >= 24 ) {
      hour = hour - 24;
    }
    hourStr = ( hour < 10 ? ( isSingleDigitHourShownCurrently ? " " : "0" ) : "" ) + String( hour );
    uint8_t minute = dtStruct->tm_min;
    minuteStr = ( minute < 10 ? "0" : "" ) + String( minute );
    uint8_t second = dtStruct->tm_sec;
    secondStr = ( second < 10 ? "0" : "" ) + String( second );
  }
}

std::vector<std::vector<bool>> getDisplayPreview( String hourStrPreview, String minuteStrPreview, String secondStrPreview, uint8_t fontNumberPreview, bool isBoldPreview, bool isSecondsShownPreview, bool isCompactLayoutPreview ) {
  std::vector<std::vector<bool>> preview( DISPLAY_HEIGHT, std::vector<bool>( DISPLAY_WIDTH, false ) );

  String textToDisplayLargePreview = hourStrPreview + ":" + minuteStrPreview;
  String textToDisplaySmallPreview = secondStrPreview;

  uint8_t displayWidthUsed = 0;
  bool isWideTextPreview = !isSecondsShownPreview;

  for( size_t charToDisplayIndex = 0; charToDisplayIndex < textToDisplayLargePreview.length(); ++charToDisplayIndex ) {
    char charToDisplay = textToDisplayLargePreview.charAt( charToDisplayIndex );

    uint8_t charLpWidth = TCFonts::getSymbolLp( fontNumberPreview, charToDisplay, isCompactLayoutPreview, isWideTextPreview, false );
    uint8_t charWidth = TCFonts::getSymbolWidth( fontNumberPreview, charToDisplay, isCompactLayoutPreview, isWideTextPreview, false );
    uint8_t charRpWidth = TCFonts::getSymbolRp( fontNumberPreview, charToDisplay, isCompactLayoutPreview, isWideTextPreview, false );

    if( displayWidthUsed + charLpWidth > DISPLAY_WIDTH ) {
      charLpWidth = DISPLAY_WIDTH - displayWidthUsed;
    }
    if( displayWidthUsed + charLpWidth + charWidth > DISPLAY_WIDTH ) {
      charWidth = DISPLAY_WIDTH - displayWidthUsed - charLpWidth;
    }
    if( displayWidthUsed + charLpWidth + charWidth + charRpWidth > DISPLAY_WIDTH ) {
      charRpWidth = DISPLAY_WIDTH - displayWidthUsed - charLpWidth - charWidth;
    }

    if( charWidth == 0 ) continue;

    for( uint8_t charX = 0; charX < charLpWidth; ++charX ) {
      for( uint8_t displayY = 0; displayY < DISPLAY_HEIGHT; ++displayY ) {
        preview[displayY][displayWidthUsed] = false;
      }
      displayWidthUsed++;
    }

    std::vector<uint8_t> charImage = TCFonts::getSymbol( fontNumberPreview, charToDisplay, isCompactLayoutPreview, isBoldPreview, isWideTextPreview, false, false );
    for( uint8_t charX = 0; charX < charWidth; ++charX ) {
      for( uint8_t displayY = 0; displayY < DISPLAY_HEIGHT; ++displayY ) {
        bool isPointEnabled = false;
        uint8_t charShiftY = DISPLAY_HEIGHT - charImage.size();
        if( displayY >= charShiftY ) {
          uint8_t charY = displayY - charShiftY;
          isPointEnabled = ( charImage[charY] >> ( DISPLAY_HEIGHT - 1 - charX ) ) & 1;
        }
        preview[displayY][displayWidthUsed] = isPointEnabled;
      }
      displayWidthUsed++;
    }

    for( uint8_t charX = 0; charX < charRpWidth; ++charX ) {
      for( uint8_t displayY = 0; displayY < DISPLAY_HEIGHT; ++displayY ) {
        preview[displayY][displayWidthUsed] = false;
      }
      displayWidthUsed++;
    }
  }

  if( isSecondsShownPreview ) {
    for( size_t charToDisplayIndex = 0; charToDisplayIndex < textToDisplaySmallPreview.length(); ++charToDisplayIndex ) {
      char charToDisplay = textToDisplaySmallPreview.charAt( charToDisplayIndex );

      uint8_t charLpWidth = TCFonts::getSymbolLp( fontNumberPreview, charToDisplay, isCompactLayoutPreview, isWideTextPreview, true );
      uint8_t charWidth = TCFonts::getSymbolWidth( fontNumberPreview, charToDisplay, isCompactLayoutPreview, isWideTextPreview, true );
      uint8_t charRpWidth = TCFonts::getSymbolRp( fontNumberPreview, charToDisplay, isCompactLayoutPreview, isWideTextPreview, true );

      if( displayWidthUsed + charLpWidth > DISPLAY_WIDTH ) {
        charLpWidth = DISPLAY_WIDTH - displayWidthUsed;
      }
      if( displayWidthUsed + charLpWidth + charWidth > DISPLAY_WIDTH ) {
        charWidth = DISPLAY_WIDTH - displayWidthUsed - charLpWidth;
      }
      if( displayWidthUsed + charLpWidth + charWidth + charRpWidth > DISPLAY_WIDTH ) {
        charRpWidth = DISPLAY_WIDTH - displayWidthUsed - charLpWidth - charWidth;
      }
      
      if( charWidth == 0 ) continue;

      for( uint8_t charX = 0; charX < charLpWidth; ++charX ) {
        for( uint8_t displayY = 0; displayY < DISPLAY_HEIGHT; ++displayY ) {
          preview[displayY][displayWidthUsed] = false;
        }
        displayWidthUsed++;
      }

      std::vector<uint8_t> charImage = TCFonts::getSymbol( fontNumberPreview, charToDisplay, isCompactLayoutPreview, isBoldPreview, isWideTextPreview, true, false );
      for( uint8_t charX = 0; charX < charWidth; ++charX ) {
        for( uint8_t displayY = 0; displayY < DISPLAY_HEIGHT; ++displayY ) {
          bool isPointEnabled = false;
          uint8_t charShiftY = DISPLAY_HEIGHT - charImage.size();
          if( displayY >= charShiftY ) {
            uint8_t charY = displayY - charShiftY;
            isPointEnabled = ( charImage[charY] >> ( DISPLAY_HEIGHT - 1 - charX ) ) & 1;
          }
          preview[displayY][displayWidthUsed] = isPointEnabled;
        }
        displayWidthUsed++;
      }

      for( uint8_t charX = 0; charX < charRpWidth; ++charX ) {
        for( uint8_t displayY = 0; displayY < DISPLAY_HEIGHT; ++displayY ) {
          preview[displayY][displayWidthUsed] = false;
        }
        displayWidthUsed++;
      }
    }
  }

  return preview;
}

void renderDisplay() {
  if( timeCanBeCalculated() ) {
    String hourStr, minuteStr, secondStr;
    calculateTimeToShow( hourStr, minuteStr, secondStr, isSingleDigitHourShown );
    renderDisplayText( hourStr, minuteStr, secondStr, isClockAnimated );
  } else {
    renderDisplayText( "  ", "  ", "  ", false );
  }
  display.update();
}


//NTP functionality
bool isTimeClientInitialised = false;
unsigned long timeClientUpdatedMillis = 0;
void initTimeClient() {
  if( WiFi.isConnected() && !isTimeClientInitialised ) {
    timeClient.setUpdateInterval( DELAY_NTP_TIME_SYNC );
    Serial.print( F("Starting NTP client...") );
    timeClient.begin();
    isTimeClientInitialised = true;
    Serial.println( F(" done") );
  }
}

void ntpProcessLoopTick() {
  if( !WiFi.isConnected() ) return;
  if( !isTimeClientInitialised ) {
    initTimeClient();
  }
  if( isTimeClientInitialised ) {
    unsigned long currentMillis = millis();
    if( calculateDiffMillis( previousMillisNtpStatusCheck, currentMillis ) >= 10 ) {
      NTPClient::Status ntpStatus = timeClient.update();

      if( ntpStatus == NTPClient::STATUS_SUCCESS_RESPONSE ) {
        timeClient.setUpdateInterval( DELAY_NTP_TIME_SYNC );
        if( isCustomDateTimeSet ) {
          isCustomDateTimeSet = false;
        }

        String hourStr, minuteStr, secondStr;
        calculateTimeToShow( hourStr, minuteStr, secondStr, isSingleDigitHourShown );
        Serial.println( String( F("NTP time sync completed. Time: ") ) + hourStr + ":" + minuteStr + ":" + secondStr + "." + timeClient.getSubSeconds() );

        forceDisplaySync();
      } else if( ntpStatus == NTPClient::STATUS_FAILED_RESPONSE ) {
        timeClient.setUpdateInterval( DELAY_NTP_TIME_SYNC_RETRY );
        Serial.println( F("NTP time sync error") );
      }
      previousMillisNtpStatusCheck = currentMillis;
    }
  }
}


//data update helpers
void forceRefreshData() {
  initVariables();
  initTimeClient();
}


//wifi connection as a client
const String getWiFiStatusText( wl_status_t status ) {
  switch (status) {
    case WL_IDLE_STATUS:
      return F("IDLE_STATUS");
    case WL_NO_SSID_AVAIL:
      return F("NO_SSID_AVAIL");
    case WL_SCAN_COMPLETED:
      return F("SCAN_COMPLETED");
    case WL_CONNECTED:
      return F("CONNECTED");
    case WL_CONNECT_FAILED:
      return F("CONNECT_FAILED");
    case WL_CONNECTION_LOST:
      return F("CONNECTION_LOST");
    #ifdef ESP8266
    case WL_WRONG_PASSWORD:
      return F("WRONG_PASSWORD");
    #endif
    case WL_DISCONNECTED:
      return F("DISCONNECTED");
    default:
      return F("Unknown");
  }
}

void disconnectFromWiFi( bool erasePreviousCredentials ) {
  wl_status_t wifiStatus = WiFi.status();
  if( wifiStatus != WL_IDLE_STATUS ) {
    Serial.print( String( F("Disconnecting from WiFi '") ) + WiFi.SSID() + String( F("'...") ) );
    uint8_t previousInternalLedStatus = getInternalLedStatus();
    setInternalLedStatus( HIGH );
    WiFi.disconnect( false, erasePreviousCredentials );
    delay( 10 );
    while( true ) {
      wifiStatus = WiFi.status();
      if( wifiStatus != WL_CONNECTED ) break;
      delay( 50 );
      Serial.print( "." );
    }
    Serial.println( F(" done") );
    setInternalLedStatus( previousInternalLedStatus );
  }
}

bool isRouterSsidProvided() {
  return strlen(wiFiClientSsid) != 0;
}

String getFullWiFiHostName() {
  #ifdef ESP8266
  return String( getWiFiHostName() ) + "-" + String( ESP.getChipId() );
  #else //ESP32 or ESP32S2
  String macAddress = String( ESP.getEfuseMac() );
  return String( getWiFiHostName() ) + "-" + macAddress.substring( macAddress.length() - 4 );
  #endif
}

void connectToWiFiAsync( bool isInit ) {
  if( !isRouterSsidProvided() ) {
    createAccessPoint();
    return;
  }

  Serial.println( String( F("Connecting to WiFi '") ) + String( wiFiClientSsid ) + "'..." );
  WiFi.hostname( getFullWiFiHostName().c_str() );
  WiFi.begin( wiFiClientSsid, wiFiClientPassword );

  if( WiFi.isConnected() ) {
    shutdownAccessPoint();
    return;
  }

  wl_status_t wifiStatus = WiFi.status();
  if( WiFi.isConnected() ) {
    Serial.println( String( F("WiFi is connected. Status: ") ) + getWiFiStatusText( wifiStatus ) );
    shutdownAccessPoint();
    forceRefreshData();
  } else if( !isInit && ( wifiStatus == WL_NO_SSID_AVAIL || wifiStatus == WL_CONNECT_FAILED || wifiStatus == WL_CONNECTION_LOST || wifiStatus == WL_IDLE_STATUS || wifiStatus == WL_DISCONNECTED ) ) {
    Serial.println( String( F("WiFi is NOT connected. Status: ") ) + getWiFiStatusText( wifiStatus ) );
    disconnectFromWiFi( false );
    createAccessPoint();
  }
}


void connectToWiFiSync() {
  if( !isRouterSsidProvided() ) {
    createAccessPoint();
    return;
  }

  Serial.println( String( F("Connecting to WiFi '") ) + String( wiFiClientSsid ) + "'..." );
  WiFi.hostname( getFullWiFiHostName().c_str() );
  WiFi.begin( wiFiClientSsid, wiFiClientPassword );

  if( WiFi.isConnected() ) {
    shutdownAccessPoint();
    return;
  }

  unsigned long wiFiConnectStartedMillis = millis();
  uint8_t previousInternalLedStatus = getInternalLedStatus();
  while( true ) {
    setInternalLedStatus( HIGH );
    delay( 500 );
    wl_status_t wifiStatus = WiFi.status();
    if( WiFi.isConnected() ) {
      Serial.println( F(" done") );
      setInternalLedStatus( previousInternalLedStatus );
      shutdownAccessPoint();
      forceRefreshData();
      break;
    } else if( ( wifiStatus == WL_NO_SSID_AVAIL || wifiStatus == WL_CONNECT_FAILED || wifiStatus == WL_CONNECTION_LOST || wifiStatus == WL_IDLE_STATUS ) && ( calculateDiffMillis( wiFiConnectStartedMillis, millis() ) >= TIMEOUT_CONNECT_WIFI_SYNC ) ) {
      Serial.println( String( F(" ERROR: ") ) + getWiFiStatusText( wifiStatus ) );
      setInternalLedStatus( previousInternalLedStatus );
      disconnectFromWiFi( false );
      createAccessPoint();
      break;
    }
  }
}


//web server
const char HTML_PAGE_START[] PROGMEM = "<!DOCTYPE html>"
"<html>"
  "<head>"
    "<meta charset=\"UTF-8\">"
    "<title>Годинник</title>"
    "<style>"
      ":root{--f:20px;}"
      "body{margin:0;background-color:#333;font-family:sans-serif;color:#FFF;}"
      "body,input,button,select{font-size:var(--f);}"
      ".wrp{width:60%;min-width:460px;max-width:600px;margin:auto;margin-bottom:10px;}"
      "h2{text-align:center;margin-top:0.3em;margin-bottom:1em;}"
      "h2,.fxh{color:#FFF;font-size:calc(var(--f)*1.2);}"
      ".fx{display:flex;flex-wrap:wrap;margin:auto;}"
      ".fx.fxsect{border:1px solid #555;background-color:#444;margin-top:0.5em;border-radius:8px;box-shadow: 4px 4px 5px #222;overflow:hidden;}"
      ".fxsect+.fxsect{/*border-top:none;*/}"
      ".fxh,.fxc{width:100%;}"
      ".fxh{padding:0.2em 0.5em;font-weight:bold;background-color:#606060;background:linear-gradient(#666,#555);border-bottom:0px solid #555;}"
      ".fxc{padding:0.5em 0.5em;}"
      ".fx .fi{display:flex;align-items:center;margin-top:0.3em;width:100%;}"
      ".fx .fi:first-of-type,.fx.fv .fi{margin-top:0;}"
      ".fv{flex-direction:column;align-items:flex-start;}"
      ".ex.ext.exton,.ex.ext.extfwon{cursor:pointer;}"
      ".ex.ext.extoff,.ex.ext.extfwoff{cursor:default;color:#666;}"
      ".ex.ext.exton{color:#AAA;}"
      ".ex.ext.extfwon,.ex.ext.extfwoff{flex:1;display:flex;}"
      ".ex.ext:after{display:inline-block;content:\"▶\";}"
      ".ex.ext.extfwon:after,.ex.ext.extfwoff:after{margin-left:auto;}"
      ".ex.exon .ex.ext:after{transform:rotate(90deg);}"
      ".ex.exc{height:0;margin-top:0;}.ex.exc>*{visibility:hidden;}"
      ".ex.exc.exon{height:inherit;}.ex.exc.exon>*{visibility:initial;}"
      "label{flex:none;padding-right:0.6em;max-width:50%;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;}"
      "input:not(.fixed),select:not(.fixed){width:100%;padding:0.1em 0.2em;}"
      "select.mid{text-align:center;}"
      "input[type=\"radio\"],input[type=\"checkbox\"]{flex:none;margin:0.1em 0;width:calc(var(--f)*1.2);height:calc(var(--f)*1.2);}"
      "input[type=\"radio\"]+label,input[type=\"checkbox\"]+label{padding-left:0.6em;padding-right:initial;flex:1 1 auto;max-width:initial;}"
      "input[type=\"range\"]{-webkit-appearance:none;background:transparent;padding:0;}"
      "input[type=\"range\"]::-webkit-slider-runnable-track{appearance:none;height:calc(0.4*var(--f));border:2px solid #EEE;border-radius:4px;background:#666;}"
      "input[type=\"range\"]::-webkit-slider-thumb{appearance:none;background:#FFF;border-radius:50%;margin-top:calc(-0.4*var(--f));height:calc(var(--f));width:calc(var(--f));}"
      "input[type=\"color\"]{padding:0;height:var(--f);border-radius:0;}"
      "input[type=\"color\"]::-webkit-color-swatch-wrapper{padding:2px;}"
      "output{padding-left:0.6em;}"
      "button:not(.fixed){width:100%;padding:0.2em;}"
      "a{color:#AAA;}"
      "a.act{color:#F88;}"
      ".sub+.sub{padding-left:0.6em;}"
      ".ft{margin-top:1em;}"
      ".pl{padding-left:0.6em;}"
      ".pll{padding-left:calc(var(--f)*1.2 + 0.6em);}"
      ".lnk{margin:auto;color:#AAA;display:inline-block;}"
      ".i{color:#CCC;margin-left:0.2em;border:1px solid #777;border-radius:50%;background-color:#666;cursor:default;font-size:65%;vertical-align:top;width:1em;height:1em;display:inline-block;text-align:center;}"
      ".i:before{content:\"i\";position:relative;top:-0.07em;}"
      ".i:hover{background-color:#777;color:#DDD;}"
      ".stat{font-size:65%;color:#888;border:1px solid #777;border-radius:6px;overflow:hidden;}"
      ".stat>span{padding:1px 4px;display:inline-block;}"
      ".stat>span:not(:last-of-type){border-right:1px solid #777;}"
      ".stat>span.lbl,.stat>span.btn{color:#888;background-color:#444;}"
      ".stat>span.btn{cursor:default;}"
      ".stat>span.btn:hover{background-color:#505050;}"
      ".stat>span.btn.on{color:#48B;background-color:#246;}"
      ".stat>span.btn.on:hover{background-color:#1A3A5A;}"
      "#exw{width:100%;display:flex;background-image:linear-gradient(-180deg,#777,#222 7% 93%,#000);}"
      "#exdw{width:calc(88% - 6px);border:2px solid #1A1A1A;padding:1px;margin:3% 6%;}"
      "#exdw .exdl{display:flex;flex-wrap:nowrap;}"
      "#exdw .exdp{width:calc(100%/32);}"
      "#exdw .exdp.exdp1{background:radial-gradient(RGBA(64,192,0,1) 55%,RGBA(34,34,34,0) 65%);}"
      "#exdw .exdp.exdp0{background:radial-gradient(RGBA(44,44,44,1) 55%,RGBA(34,34,34,0) 65%);}"
      "#exdw:before,#exdw .exdp:before{content:'';float:left;padding-top:100%;}"
      "#exdw:before{padding-top:25%;}"
      "#exdw:after,#exdw .exdp:after{content:'';display:block;clear:both;}"
      ".ap{zoom:3;image-rendering:pixelated;padding-left:0.2em;}"
      "#gc{width:100%;aspect-ratio:2/1;border:1px solid #aaa;display:flex;flex-direction:column;margin:0.2em 0 1.5em 0.8em;}" //graph start
      "#gw{position:relative;width:100%;flex:1;font-size:0.65em;}"
      "#gr,#yg,#xg,.ym,.yl,.xm,.xl,.sm,.cvm,.cvl{position:absolute;pointer-events:none;}"
      "#gr,#yg,#xg{inset:0;}"
      "#gr{display:flex;align-items:flex-end;pointer-events:auto;}"
      ".gbar{background:#4caf50;}"
      ".ym{left:0;right:0;height:1px;border-top:1px dotted RGBA(255,255,255,0.35);}"
      ".xm{top:0;bottom:0;width:0;border-left:1px dotted RGBA(255,255,255,0.35);}"
      ".yl,.xl,.cvl{font-size:0.75em;color:#aaa;text-align:right;}"
      ".yl{left:-2em;top:50%;transform:translateY(-50%);width:1.5em;}"
      ".xl{bottom:-2.5em;transform:translateY(-1.5em);width:3em;}"
      ".xlr{rotate:-90deg;letter-spacing:0.05em;}"
      ".sm{top:0;bottom:0;width:0;border-left:1px solid RGBA(255,255,255,0.45);z-index:4;}"
      ".cvm,.cvl{opacity:0.9;transition:left 0.3s ease;}"
      ".cvm{top:0;bottom:0;width:1px;background:#fe5;z-index:5;}"
      ".cvl{left:0;top:0.2em;color:#fe5;z-index:6;}" //graph end
      "@media(max-device-width:800px) and (orientation:portrait){"
        ":root{--f:4vw;}"
        ".wrp{width:94%;max-width:100%;}"
      "}"
      "@media(orientation:landscape){"
        ":root{--f:22px;}"
      "}"
    "</style>"
  "</head>"
  "<body>"
    "<div class=\"wrp\">"
      "<h2>"
        "<span id=\"title\">ГОДИННИК</span>"
        "<div style=\"line-height:0.5;\">"
          "<div class=\"lnk\" style=\"font-size:50%;\">Розробник: <a href=\"mailto:kurylo.press@gmail.com?subject=Clock\" target=\"_blank\">Дмитро Курило</a></div> "
          "<div class=\"lnk\" style=\"font-size:50%;\"><a id=\"fwup\" href=\"https://github.com/dkurylo/clock-esp\" target=\"_blank\">GitHub</a></div>"
        "</div>"
      "</h2>";
const char HTML_PAGE_END[] PROGMEM = "</div>"
  "</body>"
"</html>";

void addHtmlPageStart( String& pageBody ) {
  char c;
  for( uint16_t i = 0; i < strlen_P(HTML_PAGE_START); i++ ) {
    c = pgm_read_byte( &HTML_PAGE_START[i] );
    pageBody += c;
  }
}

void addHtmlPageEnd( String& pageBody ) {
  char c;
  for( uint16_t i = 0; i < strlen_P(HTML_PAGE_END); i++ ) {
    c = pgm_read_byte( &HTML_PAGE_END[i] );
    pageBody += c;
  }
}

String getHtmlLink( const char* href, String label ) {
  return String( F("<a href=\"") ) + String( href ) + "\">" + label + String( F("</a>") );
}

String getHtmlLabel( String label, const char* elId, bool addColon ) {
  return String( F("<label") ) + (strlen(elId) > 0 ? String( F(" for=\"") ) + String( elId ) + "\"" : "") + ">" + label + ( addColon ? ":" : "" ) + String( F("</label>") );
}

const char* HTML_INPUT_TEXT = "text";
const char* HTML_INPUT_PASSWORD = "password";
const char* HTML_INPUT_CHECKBOX = "checkbox";
const char* HTML_INPUT_RADIO = "radio";
const char* HTML_INPUT_COLOR = "color";
const char* HTML_INPUT_RANGE = "range";

String getHtmlInput( String label, const char* type, const char* value, const char* elId, const char* elName, uint8_t minLength, uint16_t maxLength, uint8_t step, bool isRequired, bool isChecked, const char* additional, String rangeCodeReplacement ) {
  return ( (strcmp(type, HTML_INPUT_TEXT) == 0 || strcmp(type, HTML_INPUT_PASSWORD) == 0 || strcmp(type, HTML_INPUT_COLOR) == 0 || strcmp(type, HTML_INPUT_RANGE) == 0) ? getHtmlLabel( label, elId, true ) : "" ) +
    String( F("<input"
      " type=\"") ) + type + String( F("\""
      " id=\"") ) + String(elId) + F("\""
      " name=\"") + String(elName) + "\"" +
      ( maxLength > 0 && (strcmp(type, HTML_INPUT_TEXT) == 0 || strcmp(type, HTML_INPUT_PASSWORD) == 0) ? String( F(" maxLength=\"") ) + String( maxLength ) + "\"" : "" ) +
      ( (strcmp(type, HTML_INPUT_CHECKBOX) != 0) ? String( F(" value=\"") ) + String( value ) + "\"" : "" ) +
      ( isRequired && (strcmp(type, HTML_INPUT_TEXT) == 0 || strcmp(type, HTML_INPUT_PASSWORD) == 0) ? F(" required") : F("") ) +
      ( isChecked && (strcmp(type, HTML_INPUT_RADIO) == 0 || strcmp(type, HTML_INPUT_CHECKBOX) == 0) ? F(" checked") : F("") ) +
      ( " " + String( additional ) ) +
      ( (strcmp(type, HTML_INPUT_RANGE) == 0) ? String( F(" min=\"") ) + String( minLength ) + String( F("\" max=\"") ) + String(maxLength) + String( F("\"") ) + ( step > 0 ? String( F(" step=\"") ) + String(step) + String( F("\"") ) : "" ) + ( String( rangeCodeReplacement ) == "" ? ( String( F(" oninput=\"this.nextElementSibling.value=this.value;\"><output>") ) + String( value ) + String( F("</output") ) ) : ( String( F(" ") ) + rangeCodeReplacement ) ) : "" ) +
    ">" +
      ( (strcmp(type, HTML_INPUT_TEXT) != 0 && strcmp(type, HTML_INPUT_PASSWORD) != 0 && strcmp(type, HTML_INPUT_COLOR) != 0 && strcmp(type, HTML_INPUT_RANGE) != 0) ? getHtmlLabel( label, elId, false ) : "" );
}

const char* HTML_PAGE_WIFI_SSID_NAME = "ssid";
const char* HTML_PAGE_WIFI_PWD_NAME = "pwd";

const char* HTML_PAGE_FONT_TYPE_NAME = "fnt";
const char* HTML_PAGE_BOLD_FONT_NAME = "bld";
const char* HTML_PAGE_SHOW_SECS_NAME = "sec";
const char* HTML_PAGE_SHOW_SINGLE_DIGIT_HOUR_NAME = "sdh";
const char* HTML_PAGE_COMPACT_LAYOUT_NAME = "cl";
const char* HTML_PAGE_SLOW_SEMICOLON_ANIMATION_NAME = "ssa";
const char* HTML_PAGE_ROTATE_DISPLAY_NAME = "rot";
const char* HTML_PAGE_CLOCK_ANIMATED_NAME = "ca";
const char* HTML_PAGE_ANIMATION_TYPE_NAME = "at";
const char* HTML_PAGE_BRIGHTNESS_DAY_NAME = "brtd";
const char* HTML_PAGE_BRIGHTNESS_NIGHT_NAME = "brtn";
const char* HTML_PAGE_BRIGHTNESS_DAY_SENSOR_NAME = "brsd";
const char* HTML_PAGE_BRIGHTNESS_NIGHT_SENSOR_NAME = "brsn";
const char* HTML_PAGE_DEVICE_NAME_NAME = "dvn";

void handleWebServerGet() {
  wifiWebServer.setContentLength( CONTENT_LENGTH_UNKNOWN );
  wifiWebServer.send( 200, getContentType( F("html") ), "" );

  String content;
  content.reserve( 6000 ); //currently 5600 max (when sending Html Page Start)

  //5600
  addHtmlPageStart( content );
  wifiWebServer.sendContent( content );
  content = "";

  //5200
  content += String( F(""
"<script>"
  "let devnm=\"" ) ) + String( deviceName ) + String( F("\";"
  "let ap=" ) ) + String( isApInitialized ) + String( F(";"
  "let fw=\"" ) ) + String( getFirmwareVersion() ) + String( F("\";"
  "document.addEventListener(\"DOMContentLoaded\",()=>{"
    "if(ap){"
      "setInterval(()=>{"
        "fetch(\"/ping\").catch(e=>{});"
      "},30000);"
    "}else{"
      "fetch(\"https://raw.githubusercontent.com/dkurylo/clock-esp/refs/heads/main/src/fw_version.txt\",{cache:\"no-cache\"}).then(resp=>resp.text()).then(data=>{"
        "data=data.replace(/^\"|\"$/g,\"\");"
        "let fwup=document.getElementById(\"fwup\");if(fwup){fwup.insertAdjacentHTML('afterend',' ('+((fw!=data)?('<a href=\""
        #ifdef ESP8266
        "https://github.com/dkurylo/clock-esp/tree/main/.pio/build/d1_mini"
        #else //ESP32 or ESP32S2
        "https://github.com/dkurylo/clock-esp/tree/main/.pio/build/lolin_s2_mini"
        #endif
        "\" target=\"_blank\" class=\"act\">'+fw+' → '+data+'</a>'):(fw))+')');}"
      "}).catch(e=>{});"
    "}"
    "if(devnm!=''){"
      "document.title+=' - '+devnm;"
      "document.getElementById('title').textContent+=' - '+devnm;"
    "}"
    "dt();"
    "pv();"
    "mnf(true);"
    "graph.draw(true);"
  "});"
  "function dt(){"
    "let ts=") ) + String( timeClient.isTimeSet() || ( isCustomDateTimeSet && calculateDiffMillis( customDateTimeReceivedAt, millis() ) <= DELAY_NTP_TIME_SYNC ) ) + String( F(";"
    "if(ts)return;"
    "fetch('/setdt?t='+Date.now().toString()).catch(e=>{"
    "});"
  "}"
  "function pv(){"
    "fetch('/preview?f='+document.querySelector('#") ) + HTML_PAGE_FONT_TYPE_NAME + String( F("').value+'&b='+(document.querySelector('#") ) + HTML_PAGE_BOLD_FONT_NAME + String( F("').checked?'1':'0')+'&s='+(document.querySelector('#") ) + HTML_PAGE_SHOW_SECS_NAME + String( F("').checked?'1':'0')+'&z='+(document.querySelector('#") ) + HTML_PAGE_SHOW_SINGLE_DIGIT_HOUR_NAME + String( F("').checked?'1':'0')+'&c='+(document.querySelector('#") ) + HTML_PAGE_COMPACT_LAYOUT_NAME + String( F("').checked?'1':'0')).then(res=>{"
      "return res.ok?res.json():[];"
    "}).then(dt=>{"
      "document.querySelector('#exdw').innerHTML=(()=>{"
        "return dt.map(ln=>{"
          "return '<div class=\"exdl\">'+"
                 "ln.split('').map(p=>{"
                   "return '<div class=\"exdp exdp'+(['0',' '].includes(p)?'0':'1')+'\"></div>';"
                 "}).join('')+"
                 "'</div>';"
        "}).join('');"
      "})();"
    "}).catch(e=>{"
    "});"
  "}"
  "function ex(el){"
    "Array.from(el.parentElement.parentElement.children).forEach(ch=>{"
      "if(ch.classList.contains(\"ex\"))ch.classList.toggle(\"exon\");"
    "});"
  "}"
  "let mntIds=new Set();"
  "let mntTimerId;"
  "function mnt(id){"
    "if(!id){"
      "mnf(true);"
      "return;"
    "}"
    "let isAdd=false;"
    "if(mntIds.has(id)){"
      "mntIds.delete(id);"
    "}else{"
      "isAdd=true;"
      "mntIds.add(id);"
    "}"
    "if(isAdd||(!mntIds.size&&mntTimerId)){"
      "clearTimeout(mntTimerId);"
      "mntTimerId=null;"
    "}"
    "if(isAdd){"
      "mnf();"
    "}"
  "}"
  "let monAbortCont=null;"
  "function mnf(isOnce){"
    "if(monAbortCont){"
      "monAbortCont.abort();"
    "}"
    "if(!isOnce&&!mntIds.size)return;"
    "monAbortCont=new AbortController();"
    "const signal=monAbortCont.signal;"
    "const timeoutId=setTimeout(()=>{"
      "monAbortCont.abort();"
    "},3500);"
    "fetch(\"/monitor\",{signal})"
    ".then(resp=>resp.json())"
    ".then(data=>{"
      "clearTimeout(timeoutId);"
      "for(const[key,value]of Object.entries(data.brt)){"
        "document.getElementById(\"b_\"+key).innerText=value;"
      "}"
      "graph.drawCurVal(data.brt.avg);"
    "})"
    ".catch(e=>{"
      "clearTimeout(timeoutId);"
      "if(e.name==='AbortError'){"
      "}else{"
      "}"
    "})"
    ".finally(()=>{"
      "monAbortCont=null;"
    "});"
    "if(isOnce)return;"
    "mntTimerId=setTimeout(()=>{"
      "mnf();"
    "},4000);"
  "}"
  "let graph={" //graph start
    "gp:{"
      "cv:-1,"
      "ns:0,"
      "ds:0,"
      "nb:0,"
      "db:0,"
      "k:" ) ) + String( steepnessCoefficient ).c_str() + String( F(","
      "smin:0,"
      "smax:0,"
      "bl:16"
    "},"
    "segs:[],"
    "brt(s){"
      "let p=this.gp;"
      "if(s>=p.ds)return p.db;"
      "if(s<=p.ns)return p.nb;"
      "let n=(s-p.ns)/(p.ds-p.ns);"
      "let e=1-Math.pow(1-n,p.k);"
      "return Math.round(p.nb+(p.db-p.nb)*e);"
    "},"
    "mkSegs(){"
      "let sg=[],st=this.gp.smin,pb=this.brt(this.gp.smin);"
      "for(let s=this.gp.smin+1;s<=this.gp.smax;s++){"
        "let b=this.brt(s);"
        "if(b!==pb){sg.push({f:st,t:s,b:pb});st=s;pb=b;}"
      "}"
      "sg.push({f:st,t:this.gp.smax,b:pb});"
      "this.segs=sg;"
    "},"
    "drawY(){"
      "let g=document.getElementById(\"yg\");"
      "g.innerHTML=\"\";"
      "for(let l=0;l<this.gp.bl;l++){"
        "let r=document.createElement(\"div\");"
        "r.className=\"ym\";"
        "r.style.top=((this.gp.bl-1-l)/(this.gp.bl-1)*100)+\"%\";"
        "if(l==0||l==this.gp.bl-1)r.style.borderTop=\"none\";"
        "let t=document.createElement(\"div\");"
        "t.className=\"yl\";t.textContent=l;"
        "r.appendChild(t);"
        "g.appendChild(r);"
      "}"
    "},"
    "drawG(){"
      "let g=document.getElementById(\"gr\");g.innerHTML=\"\";"
      "for(let s of this.segs){"
        "let d=document.createElement(\"div\");"
        "d.className=\"gbar\";"
        "d.style.width=((s.t-s.f)/(this.gp.smax-this.gp.smin)*100)+\"%\";"
        "d.style.height=(s.b/(this.gp.bl-1)*100)+\"%\";"
        "g.appendChild(d);"
      "}"
    "},"
    "drawX(){"
      "let g=document.getElementById(\"xg\");g.innerHTML=\"\";"
      "for(let s of this.segs){"
        "if(s.f==this.gp.smin||s.f==this.gp.smax)continue;"
        "let x=((s.f-this.gp.smin)/(this.gp.smax-this.gp.smin)*100)+\"%\";"
        "let l=document.createElement(\"div\");l.className=\"xm\";l.style.left=x;"
        "let t=document.createElement(\"div\");t.className=\"xl xlr\";t.style.left=x;t.textContent=s.f;"
        "g.appendChild(l);g.appendChild(t);"
      "}"
      "let mn=document.createElement(\"div\"),mx=document.createElement(\"div\");"
      "mn.className=mx.className=\"xm sm\";"
      "let r=this.gp.smax-this.gp.smin;"
      "mn.style.left=((this.gp.ns-this.gp.smin)/r*100)+\"%\";"
      "mx.style.left=((this.gp.ds-this.gp.smin)/r*100)+\"%\";"
      "g.appendChild(mn);"
      "g.appendChild(mx);"
    "},"
    "draw(isInit){"
      "let p=this.gp;"
      "let brsd=document.getElementById(\"brsd\");"
      "p.ns=parseInt(document.getElementById(\"brsn\").value);"
      "p.ds=parseInt(brsd.value);"
      "p.nb=parseInt(document.getElementById(\"brtn\").value);"
      "p.db=parseInt(document.getElementById(\"brtd\").value);"
      "p.smin=parseInt(brsd.min);"
      "p.smax=parseInt(brsd.max);"
      "if(isInit){"
        "this.drawY();"
      "}"
      "this.mkSegs();"
      "this.drawX();"
      "this.drawG();"
    "},"
    "drawCurVal(cv){"
      "let p=this.gp;"
      "p.cv=cv;"
      "let elm=document.getElementById(\"cvm\");"
      "let ell=document.getElementById(\"cvl\");"
      "if(p.cv==-1){"
        "if(ell)ell.remove();"
        "if(elm)elm.remove();"
        "return;"
      "}"
      "let pct=(p.cv-p.smin)/(p.smax-p.smin)*100;"
      "if(!elm){"
        "elm=document.createElement(\"div\");"
        "elm.id=\"cvm\";"
        "elm.className=\"cvm\";"
        "document.getElementById(\"xg\").appendChild(elm);"
      "}"
      "elm.style.left=pct+\"%\";"
      "if(!ell){"
        "ell=document.createElement(\"div\");"
        "ell.id=\"cvl\";"
        "ell.className=\"cvl\";"
        "elm.appendChild(ell);"
      "}"
      "ell.style.left=\"calc(\"+pct+\"% - \"+(pct<90?-0.4:2.6)+\"em)\";"
      "ell.textContent=Math.round(p.cv);"
    "}"
  "};" //graph end
"</script>" ) );
  wifiWebServer.sendContent( content );
  content = "";

  //4900
  content += String( F(""
"<form method=\"POST\">"
  "<div class=\"fx fxsect\">"
    "<div class=\"fxh\">"
      "Приєднатись до WiFi"
    "</div>"
    "<div class=\"fxc\">"
      "<div class=\"fi\">") ) + getHtmlInput( F("SSID назва"), HTML_INPUT_TEXT, wiFiClientSsid, HTML_PAGE_WIFI_SSID_NAME, HTML_PAGE_WIFI_SSID_NAME, 0, sizeof(wiFiClientSsid) - 1, 0, true, false, "", "" ) + String( F("</div>"
      "<div class=\"fi\">") ) + getHtmlInput( F("SSID пароль"), HTML_INPUT_PASSWORD, wiFiClientPassword, HTML_PAGE_WIFI_PWD_NAME, HTML_PAGE_WIFI_PWD_NAME, 0, sizeof(wiFiClientPassword) - 1, 0, true, false, "", "" ) + String( F("</div>"
    "</div>"
  "</div>"
  "<div class=\"fx fxsect\">"
    "<div class=\"fxh\">"
      "Попередній перегляд"
    "</div>"
    "<div class=\"fxc\">"
      "<div class=\"fi\"><div id=\"exw\"><div id=\"exdw\"></div></div></div>"
    "</div>"
  "</div>"
  "<div class=\"fx fxsect\">"
    "<div class=\"fxh\">"
      "Вигляд"
    "</div>"
    "<div class=\"fxc\">"
      "<div class=\"fi\">") ) + getHtmlInput( F("Показувати секунди"), HTML_INPUT_CHECKBOX, "", HTML_PAGE_SHOW_SECS_NAME, HTML_PAGE_SHOW_SECS_NAME, 0, 0, 0, false, isDisplaySecondsShown, "onchange=\"pv();\"", "" ) + String( F("</div>"
      "<div class=\"fi\">") ) + getHtmlInput( F("Показувати час без переднього нуля"), HTML_INPUT_CHECKBOX, "", HTML_PAGE_SHOW_SINGLE_DIGIT_HOUR_NAME, HTML_PAGE_SHOW_SINGLE_DIGIT_HOUR_NAME, 0, 0, 0, false, isSingleDigitHourShown, "onchange=\"pv();\"", "" ) + String( F("</div>"
      "<div class=\"fi\">") ) + getHtmlInput( F("Зменшити відстань до двокрапок"), HTML_INPUT_CHECKBOX, "", HTML_PAGE_COMPACT_LAYOUT_NAME, HTML_PAGE_COMPACT_LAYOUT_NAME, 0, 0, 0, false, isDisplayCompactLayoutUsed, "onchange=\"pv();\"", "" ) + String( F("</div>"
      "<div class=\"fi\">") ) + getHtmlInput( F("Повільні двокрапки (30 разів в хв)"), HTML_INPUT_CHECKBOX, "", HTML_PAGE_SLOW_SEMICOLON_ANIMATION_NAME, HTML_PAGE_SLOW_SEMICOLON_ANIMATION_NAME, 0, 0, 0, false, isSlowSemicolonAnimation, "", "" ) + String( F("</div>"
    "</div>"
  "</div>"
  "<div class=\"fx fxsect\">"
    "<div class=\"fxh\">"
      "Шрифт"
    "</div>"
    "<div class=\"fxc\">"
      "<div class=\"fi\">") ) + getHtmlInput( F("Вид шрифта"), HTML_INPUT_RANGE, String(displayFontTypeNumber).c_str(), HTML_PAGE_FONT_TYPE_NAME, HTML_PAGE_FONT_TYPE_NAME, 1, TCFonts::NUMBER_OF_FONTS_SUPPORTED, 0, false, displayFontTypeNumber, "onchange=\"pv();\"", "" ) + String( F("<span class=\"pl\"><a href=\"/fontedit\">Редактор</a></span></div>"
      "<div class=\"fi\">") ) + getHtmlInput( F("Жирний шрифт"), HTML_INPUT_CHECKBOX, "", HTML_PAGE_BOLD_FONT_NAME, HTML_PAGE_BOLD_FONT_NAME, 0, 0, 0, false, isDisplayBoldFontUsed, "onchange=\"pv();\"", "" ) + String( F("</div>"
    "</div>"
  "</div>"
  "<div class=\"fx fxsect\">"
    "<div class=\"fxh\">"
      "Анімація"
    "</div>"
    "<div class=\"fxc\">"
      "<div class=\"fi\">") ) + getHtmlInput( F("Анімований годинник"), HTML_INPUT_CHECKBOX, "", HTML_PAGE_CLOCK_ANIMATED_NAME, HTML_PAGE_CLOCK_ANIMATED_NAME, 0, 0, 0, false, isClockAnimated, "", "" ) + String( F("</div>"
      "<div class=\"fi\">") ) + getHtmlInput( F("Вид анімації"), HTML_INPUT_RANGE, String(animationTypeNumber).c_str(), HTML_PAGE_ANIMATION_TYPE_NAME, HTML_PAGE_ANIMATION_TYPE_NAME, 1, TCData::NUMBER_OF_ANIMATIONS_SUPPORTED, 0, false, animationTypeNumber, "", String( F( "oninput=\"this.nextElementSibling.src='/data?p='+this.value;\"><img class=\"ap\" src=\"/data?p=" ) ) + String( animationTypeNumber ) + String( F( "\"" ) ) ) + String( F("</div>"
    "</div>"
  "</div>"
  "<div class=\"fx fxsect\">"
    "<div class=\"fxh\">"
      "Яскравість"
    "</div>"
    "<div class=\"fxc\">"
      "<div class=\"fi\">") ) + getHtmlInput( F("Яскравість вдень"), HTML_INPUT_RANGE, String(displayDayBrightness).c_str(), HTML_PAGE_BRIGHTNESS_DAY_NAME, HTML_PAGE_BRIGHTNESS_DAY_NAME, 0, 15, 0, false, displayDayBrightness, "onchange=\"graph.draw();\"", "" ) + String( F("</div>"
      "<div class=\"fi\">") ) + getHtmlInput( F("Яскравість вночі"), HTML_INPUT_RANGE, String(displayNightBrightness).c_str(), HTML_PAGE_BRIGHTNESS_NIGHT_NAME, HTML_PAGE_BRIGHTNESS_NIGHT_NAME, 0, 15, 0, false, displayNightBrightness, "onchange=\"graph.draw();\"", "" ) + String( F("</div>"
      "<div class=\"fi\">") ) + getHtmlInput( F("Сенсор яскравості (день)"), HTML_INPUT_RANGE, String(sensorBrightnessDayLevel).c_str(), HTML_PAGE_BRIGHTNESS_DAY_SENSOR_NAME, HTML_PAGE_BRIGHTNESS_DAY_SENSOR_NAME, 0, ADC_NUMBER_OF_VALUES - 1, ADC_STEP_FOR_BYTE, false, sensorBrightnessDayLevel, "onchange=\"graph.draw();\"", "" ) + String( F("</div>"
      "<div class=\"fi\">") ) + getHtmlInput( F("Сенсор яскравості (ніч)"), HTML_INPUT_RANGE, String(sensorBrightnessNightLevel).c_str(), HTML_PAGE_BRIGHTNESS_NIGHT_SENSOR_NAME, HTML_PAGE_BRIGHTNESS_NIGHT_SENSOR_NAME, 0, ADC_NUMBER_OF_VALUES - 1, ADC_STEP_FOR_BYTE, false, sensorBrightnessNightLevel, "onchange=\"graph.draw();\"", "" ) + String( F("</div>"
      "<div class=\"fi fv\">"
        "<div class=\"fi ex\"><div class=\"ex ext extfwon\" onclick=\"ex(this);mnt(2);\">Графік (сенсор &rarr; яскравість)</div></div>"
        "<div class=\"fi ex exc\"><div class=\"fi\"><div id=\"gc\"><div id=\"gw\"><div id=\"gr\"></div><div id=\"yg\"></div><div id=\"xg\"></div></div></div></div></div>"
      "</div>"
    "</div>"
  "</div>"
  "<div class=\"fx fxsect\">"
    "<div class=\"fxh\">"
      "Інші налаштування"
    "</div>"
    "<div class=\"fxc\">"
      "<div class=\"fi\">") ) + getHtmlInput( F("Назва пристрою"), HTML_INPUT_TEXT, deviceName, HTML_PAGE_DEVICE_NAME_NAME, HTML_PAGE_DEVICE_NAME_NAME, 0, sizeof(deviceName) - 1, 0, false, false, "", "" ) + String( F("</div>"
      "<div class=\"fi\">") ) + getHtmlInput( F("Розвернути зображення на 180°"), HTML_INPUT_CHECKBOX, "", HTML_PAGE_ROTATE_DISPLAY_NAME, HTML_PAGE_ROTATE_DISPLAY_NAME, 0, 0, 0, false, isRotateDisplay, "", "" ) + String( F("</div>"
    "</div>"
  "</div>"
  "<div class=\"fx fxsect\">"
    "<div class=\"fxh\">"
      "Дії"
    "</div>"
    "<div class=\"fxc\">"
      "<div class=\"fi\">"
        "<button type=\"submit\">Застосувати</button>"
      "</div>"
      "<div class=\"ft\">"
        "<div class=\"fx\">"
          "<span class=\"sub\"><a href=\"/testdim\">Перевірити нічний режим</a><span class=\"i\" title=\"Застосуйте налаштування перед перевіркою!\"></span></span>"
          "<span class=\"sub\"><a href=\"/testled\">Перевірити матрицю</a></span>"
        "</div>"
        "<div class=\"fx\">"
          "<span class=\"sub\"><a href=\"/update\">Оновити</a><span class=\"i\" title=\"Оновити прошивку\"></span></span>"
          "<span class=\"sub\"><a href=\"/reset\" onclick=\"return confirm('Ви впевнені, що хочете відновити?');\">Відновити</a><span class=\"i\" title=\"Відновити до заводських налаштувань\"></span></span>"
          "<span class=\"sub\"><a href=\"/reboot\">Перезавантажити</a></span>"
        "</div>"
      "</div>"
    "</div>"
  "</div>"
"</form>"
"<div class=\"fx ft\">"
  "<span>"
    "<div class=\"stat\">"
      "<span class=\"btn\" onclick=\"this.classList.toggle('on');mnt(1);\">Яскравість</span>"
      "<span>CUR <span id=\"b_cur\"></span></span>"
      "<span>AVG <span id=\"b_avg\"></span></span>"
      "<span>REQ <span id=\"b_req\"></span></span>"
      "<span>DSP <span id=\"b_dsp\"></span></span>"
    "</div>"
  "</span>"
"</div>") );
  addHtmlPageEnd( content );
  wifiWebServer.sendContent( content );
  content = "";

  if( isApInitialized ) { //this resets AP timeout when user loads the page in AP mode
    apStartedMillis = millis();
  }
}

const char HTML_PAGE_FILLUP_START[] PROGMEM = "<style>"
  "#fill{border:2px solid #FFF;background:#666;margin:1em 0;}#fill>div{width:0;height:2.5vw;background-color:#FFF;animation:fill ";
const char HTML_PAGE_FILLUP_MID[] PROGMEM = "s linear forwards;}"
  "@keyframes fill{0%{width:0;}100%{width:100%;}}"
"</style>"
"<div id=\"fill\"><div></div></div>"  
"<script>"
  "document.addEventListener(\"DOMContentLoaded\",()=>{"
    "setTimeout(()=>{"
      "window.location.href=\"/\";"
    "},";
const char HTML_PAGE_FILLUP_END[] PROGMEM = "000);"
  "});"
"</script>";

String getHtmlPageFillup( String animationLength, String redirectLength ) {
  String result;
  result.reserve( strlen_P(HTML_PAGE_FILLUP_START) + animationLength.length() + strlen_P(HTML_PAGE_FILLUP_MID) + redirectLength.length() + strlen_P(HTML_PAGE_FILLUP_END) + 1 );
  char c;
  for( uint16_t i = 0; i < strlen_P(HTML_PAGE_FILLUP_START); i++ ) {
    c = pgm_read_byte( &HTML_PAGE_FILLUP_START[i] );
    result += c;
  }
  result += animationLength;
  for( uint16_t i = 0; i < strlen_P(HTML_PAGE_FILLUP_MID); i++ ) {
    c = pgm_read_byte( &HTML_PAGE_FILLUP_MID[i] );
    result += c;
  }
  result += redirectLength;
  for( uint16_t i = 0; i < strlen_P(HTML_PAGE_FILLUP_END); i++ ) {
    c = pgm_read_byte( &HTML_PAGE_FILLUP_END[i] );
    result += c;
  }
  return result;
}


bool isDisplayIntensityUpdateRequiredAfterSettingChanged = false;
bool isDisplayRerenderRequiredAfterSettingChanged = false;

void handleWebServerPost() {
  String content;

  String htmlPageSsidNameReceived = wifiWebServer.arg( HTML_PAGE_WIFI_SSID_NAME );
  String htmlPageSsidPasswordReceived = wifiWebServer.arg( HTML_PAGE_WIFI_PWD_NAME );

  if( htmlPageSsidNameReceived.length() > sizeof(wiFiClientSsid) - 1 ) {
    addHtmlPageStart( content );
    content += String( F("<h2>Error: SSID Name exceeds maximum length of ") ) + String( sizeof(wiFiClientSsid) - 1 ) + String( F("</h2>") );
    addHtmlPageEnd( content );
    wifiWebServer.sendHeader( String( F("Content-Length") ).c_str(), String( content.length() ) );
    wifiWebServer.send( 400, getContentType( F("html") ), content );
    return;
  }
  if( htmlPageSsidPasswordReceived.length() > sizeof(wiFiClientPassword) - 1 ) {
    addHtmlPageStart( content );
    content += String( F("<h2>Error: SSID Password exceeds maximum length of ") ) + String( sizeof(wiFiClientPassword) - 1 ) + String( F("</h2>") );
    addHtmlPageEnd( content );
    wifiWebServer.sendHeader( String( F("Content-Length") ).c_str(), String( content.length() ) );
    wifiWebServer.send( 400, getContentType( F("html") ), content );
    return;
  }


  String htmlPageIsSingleDigitHourShownReceived = wifiWebServer.arg( HTML_PAGE_SHOW_SINGLE_DIGIT_HOUR_NAME );
  bool isSingleDigitHourShownReceived = false;
  bool isSingleDigitHourShownReceivedPopulated = false;
  if( htmlPageIsSingleDigitHourShownReceived == "on" ) {
    isSingleDigitHourShownReceived = true;
    isSingleDigitHourShownReceivedPopulated = true;
  } else if( htmlPageIsSingleDigitHourShownReceived == "" ) {
    isSingleDigitHourShownReceived = false;
    isSingleDigitHourShownReceivedPopulated = true;
  }

  String htmlPageIsCompactLayoutReceived = wifiWebServer.arg( HTML_PAGE_COMPACT_LAYOUT_NAME );
  bool isCompactLayoutReceived = false;
  bool isCompactLayoutReceivedPopulated = false;
  if( htmlPageIsCompactLayoutReceived == "on" ) {
    isCompactLayoutReceived = true;
    isCompactLayoutReceivedPopulated = true;
  } else if( htmlPageIsCompactLayoutReceived == "" ) {
    isCompactLayoutReceived = false;
    isCompactLayoutReceivedPopulated = true;
  }

  String htmlPageIsClockAnimatedReceived = wifiWebServer.arg( HTML_PAGE_CLOCK_ANIMATED_NAME );
  bool isClockAnimatedReceived = false;
  bool isClockAnimatedReceivedPopulated = false;
  if( htmlPageIsClockAnimatedReceived == "on" ) {
    isClockAnimatedReceived = true;
    isClockAnimatedReceivedPopulated = true;
  } else if( htmlPageIsClockAnimatedReceived == "" ) {
    isClockAnimatedReceived = false;
    isClockAnimatedReceivedPopulated = true;
  }


  String htmlPageDisplayFontTypeNumberReceived = wifiWebServer.arg( HTML_PAGE_FONT_TYPE_NAME );
  uint displayFontTypeNumberReceived = htmlPageDisplayFontTypeNumberReceived.toInt();
  bool displayFontTypeNumberReceivedPopulated = false;
  if( displayFontTypeNumberReceived > 0 && displayFontTypeNumberReceived <= (uint)TCFonts::NUMBER_OF_FONTS_SUPPORTED ) {
    displayFontTypeNumberReceivedPopulated = true;
  }

  String htmlPageIsDisplayBoldFondUsedReceived = wifiWebServer.arg( HTML_PAGE_BOLD_FONT_NAME );
  bool isDisplayBoldFondUsedReceived = false;
  bool isDisplayBoldFondUsedReceivedPopulated = false;
  if( htmlPageIsDisplayBoldFondUsedReceived == "on" ) {
    isDisplayBoldFondUsedReceived = true;
    isDisplayBoldFondUsedReceivedPopulated = true;
  } else if( htmlPageIsDisplayBoldFondUsedReceived == "" ) {
    isDisplayBoldFondUsedReceived = false;
    isDisplayBoldFondUsedReceivedPopulated = true;
  }

  String htmlPageIsDisplaySecondsShownReceived = wifiWebServer.arg( HTML_PAGE_SHOW_SECS_NAME );
  bool isDisplaySecondsShownReceived = false;
  bool isDisplaySecondsShownReceivedPopulated = false;
  if( htmlPageIsDisplaySecondsShownReceived == "on" ) {
    isDisplaySecondsShownReceived = true;
    isDisplaySecondsShownReceivedPopulated = true;
  } else if( htmlPageIsDisplaySecondsShownReceived == "" ) {
    isDisplaySecondsShownReceived = false;
    isDisplaySecondsShownReceivedPopulated = true;
  }

  String htmlPageIsRotateDisplayReceived = wifiWebServer.arg( HTML_PAGE_ROTATE_DISPLAY_NAME );
  bool isRotateDisplayReceived = false;
  bool isRotateDisplayReceivedPopulated = false;
  if( htmlPageIsRotateDisplayReceived == "on" ) {
    isRotateDisplayReceived = true;
    isRotateDisplayReceivedPopulated = true;
  } else if( htmlPageIsRotateDisplayReceived == "" ) {
    isRotateDisplayReceived = false;
    isRotateDisplayReceivedPopulated = true;
  }

  String htmlPageAnimationTypeNumberReceived = wifiWebServer.arg( HTML_PAGE_ANIMATION_TYPE_NAME );
  uint animationTypeNumberReceived = htmlPageAnimationTypeNumberReceived.toInt();
  bool animationTypeNumberReceivedPopulated = false;
  if( animationTypeNumberReceived > 0 && animationTypeNumberReceived <= (uint)TCData::NUMBER_OF_ANIMATIONS_SUPPORTED ) {
    animationTypeNumberReceivedPopulated = true;
  }

  String htmlPageIsSlowSemicolonAnimationReceived = wifiWebServer.arg( HTML_PAGE_SLOW_SEMICOLON_ANIMATION_NAME );
  bool isSlowSemicolonAnimationReceived = false;
  bool isSlowSemicolonAnimationReceivedPopulated = false;
  if( htmlPageIsSlowSemicolonAnimationReceived == "on" ) {
    isSlowSemicolonAnimationReceived = true;
    isSlowSemicolonAnimationReceivedPopulated = true;
  } else if( htmlPageIsSlowSemicolonAnimationReceived == "" ) {
    isSlowSemicolonAnimationReceived = false;
    isSlowSemicolonAnimationReceivedPopulated = true;
  }

  String htmlPageDisplayDayBrightnessReceived = wifiWebServer.arg( HTML_PAGE_BRIGHTNESS_DAY_NAME );
  uint displayDayBrightnessReceived = htmlPageDisplayDayBrightnessReceived.toInt();
  bool displayDayBrightnessReceivedPopulated = false;
  if( displayDayBrightnessReceived >= 0 && displayDayBrightnessReceived <= 15 ) {
    displayDayBrightnessReceivedPopulated = true;
  }

  String htmlPageDisplayNightBrightnessReceived = wifiWebServer.arg( HTML_PAGE_BRIGHTNESS_NIGHT_NAME );
  uint displayNightBrightnessReceived = htmlPageDisplayNightBrightnessReceived.toInt();
  bool displayNightBrightnessReceivedPopulated = false;
  if( displayNightBrightnessReceived >= 0 && displayNightBrightnessReceived <= 15 ) {
    displayNightBrightnessReceivedPopulated = true;
    if( displayNightBrightnessReceived > displayDayBrightnessReceived ) {
      displayNightBrightnessReceived = displayDayBrightnessReceived;
    }
  }

  String htmlPageSensorBrightnessDayReceived = wifiWebServer.arg( HTML_PAGE_BRIGHTNESS_DAY_SENSOR_NAME );
  uint sensorBrightnessDayReceived = htmlPageSensorBrightnessDayReceived.toInt();
  bool sensorBrightnessDayReceivedPopulated = false;
  if( sensorBrightnessDayReceived >= 0 && sensorBrightnessDayReceived <= ( ADC_NUMBER_OF_VALUES - 1 ) ) {
    sensorBrightnessDayReceivedPopulated = true;
  }

  String htmlPageSensorBrightnessNightReceived = wifiWebServer.arg( HTML_PAGE_BRIGHTNESS_NIGHT_SENSOR_NAME );
  uint sensorBrightnessNightReceived = htmlPageSensorBrightnessNightReceived.toInt();
  bool sensorBrightnessNightReceivedPopulated = false;
  if( sensorBrightnessNightReceived >= 0 && sensorBrightnessNightReceived <= ( ADC_NUMBER_OF_VALUES - 1 ) ) {
    sensorBrightnessNightReceivedPopulated = true;
    if( sensorBrightnessNightReceived > sensorBrightnessDayReceived ) {
      sensorBrightnessNightReceived = sensorBrightnessDayReceived;
    }
  }

  String htmlPageDeviceNameReceived = wifiWebServer.arg( HTML_PAGE_DEVICE_NAME_NAME );


  bool isWiFiChanged = strcmp( wiFiClientSsid, htmlPageSsidNameReceived.c_str() ) != 0 || strcmp( wiFiClientPassword, htmlPageSsidPasswordReceived.c_str() ) != 0;

  String waitTime = isWiFiChanged ? String(TIMEOUT_CONNECT_WIFI_SYNC/1000 + 6) : "2";
  addHtmlPageStart( content );
  content += getHtmlPageFillup( waitTime, waitTime ) + String( F("<h2>Зберігаю...</h2>") );
  addHtmlPageEnd( content );
  wifiWebServer.sendHeader( String( F("Content-Length") ).c_str(), String( content.length() ) );
  wifiWebServer.send( 200, getContentType( F("html") ), content );

  if( isSingleDigitHourShownReceivedPopulated && isSingleDigitHourShownReceived != isSingleDigitHourShown ) {
    isSingleDigitHourShown = isSingleDigitHourShownReceived;
    isDisplayRerenderRequiredAfterSettingChanged = true;
    Serial.println( F("Show single digit hour updated") );
    writeEepromBoolValue( eepromIsSingleDigitHourShownIndex, isSingleDigitHourShownReceived );
  }

  if( isCompactLayoutReceivedPopulated && isCompactLayoutReceived != isDisplayCompactLayoutUsed ) {
    isDisplayCompactLayoutUsed = isCompactLayoutReceived;
    isDisplayRerenderRequiredAfterSettingChanged = true;
    Serial.println( F("Compact layout updated") );
    writeEepromBoolValue( eepromIsCompactLayoutShownIndex, isCompactLayoutReceived );
  }

  if( isClockAnimatedReceivedPopulated && isClockAnimatedReceived != isClockAnimated ) {
    isClockAnimated = isClockAnimatedReceived;
    Serial.println( F("Clock animated updated") );
    writeEepromBoolValue( eepromIsClockAnimatedIndex, isClockAnimatedReceived );
  }


  if( displayFontTypeNumberReceivedPopulated && displayFontTypeNumberReceived != displayFontTypeNumber ) {
    displayFontTypeNumber = displayFontTypeNumberReceived;
    isDisplayRerenderRequiredAfterSettingChanged = true;
    Serial.println( F("Display font updated") );
    writeEepromUintValue( eepromDisplayFontTypeNumberIndex, displayFontTypeNumberReceived );
  }

  if( isDisplayBoldFondUsedReceivedPopulated && isDisplayBoldFondUsedReceived != isDisplayBoldFontUsed ) {
    isDisplayBoldFontUsed = isDisplayBoldFondUsedReceived;
    isDisplayRerenderRequiredAfterSettingChanged = true;
    Serial.println( F("Show seconds updated") );
    writeEepromBoolValue( eepromIsFontBoldUsedIndex, isDisplayBoldFondUsedReceived );
  }

  if( isDisplaySecondsShownReceivedPopulated && isDisplaySecondsShownReceived != isDisplaySecondsShown ) {
    isDisplaySecondsShown = isDisplaySecondsShownReceived;
    isDisplayRerenderRequiredAfterSettingChanged = true;
    Serial.println( F("Show seconds updated") );
    writeEepromBoolValue( eepromIsDisplaySecondsShownIndex, isDisplaySecondsShownReceived );
  }

  if( isRotateDisplayReceivedPopulated && isRotateDisplayReceived != isRotateDisplay ) {
    isRotateDisplay = isRotateDisplayReceived;
    isDisplayRerenderRequiredAfterSettingChanged = true;
    Serial.println( F("Display rotation updated") );
    writeEepromBoolValue( eepromIsRotateDisplayIndex, isRotateDisplayReceived );
  }

  if( animationTypeNumberReceivedPopulated && animationTypeNumberReceived != animationTypeNumber ) {
    animationTypeNumber = animationTypeNumberReceived;
    Serial.println( F("Display animation updated") );
    writeEepromUintValue( eepromAnimationTypeNumberIndex, animationTypeNumberReceived );
  }

  if( isSlowSemicolonAnimationReceivedPopulated && isSlowSemicolonAnimationReceived != isSlowSemicolonAnimation ) {
    isSlowSemicolonAnimation = isSlowSemicolonAnimationReceived;
    isDisplayRerenderRequiredAfterSettingChanged = true;
    Serial.println( F("Display semicolon speed updated") );
    writeEepromBoolValue( eepromIsSlowSemicolonAnimationIndex, isSlowSemicolonAnimationReceived );
    forceDisplaySync();
  }

  if( displayDayBrightnessReceivedPopulated && displayDayBrightnessReceived != displayDayBrightness ) {
    displayDayBrightness = displayDayBrightnessReceived;
    isDisplayIntensityUpdateRequiredAfterSettingChanged = true;
    isDisplayRerenderRequiredAfterSettingChanged = true;
    Serial.println( F("Display brightness updated") );
    writeEepromUintValue( eepromDisplayDayBrightnessIndex, displayDayBrightnessReceived );
  }

  if( displayNightBrightnessReceivedPopulated && displayNightBrightnessReceived != displayNightBrightness ) {
    displayNightBrightness = displayNightBrightnessReceived;
    isDisplayIntensityUpdateRequiredAfterSettingChanged = true;
    isDisplayRerenderRequiredAfterSettingChanged = true;
    Serial.println( F("Display night mode brightness updated") );
    writeEepromUintValue( eepromDisplayNightBrightnessIndex, displayNightBrightnessReceived );
  }

  if( sensorBrightnessDayReceivedPopulated && sensorBrightnessDayReceived != sensorBrightnessDayLevel ) {
    sensorBrightnessDayLevel = sensorBrightnessDayReceived;
    isDisplayIntensityUpdateRequiredAfterSettingChanged = true;
    isDisplayRerenderRequiredAfterSettingChanged = true;
    Serial.println( F("Sensor day brightness level updated") );
    writeEepromUintValue( eepromSensorBrightnessDayLevelIndex, (uint8_t)( sensorBrightnessDayReceived / ADC_STEP_FOR_BYTE ) );
  }

  if( sensorBrightnessNightReceivedPopulated && sensorBrightnessNightReceived != sensorBrightnessNightLevel ) {
    sensorBrightnessNightLevel = sensorBrightnessNightReceived;
    isDisplayIntensityUpdateRequiredAfterSettingChanged = true;
    isDisplayRerenderRequiredAfterSettingChanged = true;
    Serial.println( F("Sensor night brightness level updated") );
    writeEepromUintValue( eepromSensorBrightnessNightLevelIndex, (uint8_t)( sensorBrightnessNightReceived / ADC_STEP_FOR_BYTE ) );
  }

  if( strcmp( deviceName, htmlPageDeviceNameReceived.c_str() ) != 0 ) {
    Serial.println( F("Device name updated") );
    strncpy( deviceName, htmlPageDeviceNameReceived.c_str(), sizeof(deviceName) );
    writeEepromCharArray( eepromDeviceNameIndex, deviceName, sizeof(deviceName) );
  }


  if( isWiFiChanged ) {
    Serial.println( F("WiFi settings updated") );
    strncpy( wiFiClientSsid, htmlPageSsidNameReceived.c_str(), sizeof(wiFiClientSsid) );
    writeEepromCharArray( eepromWiFiSsidIndex, wiFiClientSsid, sizeof(wiFiClientSsid) );
    strncpy( wiFiClientPassword, htmlPageSsidPasswordReceived.c_str(), sizeof(wiFiClientPassword) );
    writeEepromCharArray( eepromWiFiPasswordIndex, wiFiClientPassword, sizeof(wiFiClientPassword) );
    shutdownAccessPoint();
    connectToWiFiSync();
  }
}

bool containsOnlyDigits( const String &str ) {
  size_t length = str.length();
  for( size_t i = 0; i < length; ++i ) {
    if( isdigit( str.charAt(i) ) ) continue;
    return false;
  }
  return true;
}


void handleWebServerGetPreview() {
  String fontNumberStr = wifiWebServer.arg("f");
  if( !fontNumberStr.length() ) fontNumberStr = "1";
  uint8_t fontNumber = static_cast<uint8_t>( atoi( fontNumberStr.c_str() ) );
  if( fontNumber > TCFonts::NUMBER_OF_FONTS_SUPPORTED ) fontNumber = TCFonts::NUMBER_OF_FONTS_SUPPORTED;
  if( fontNumber < 1 ) fontNumber = 1;

  String isBoldStr = wifiWebServer.arg("b");
  bool isBold = isBoldStr == String( F("1") ) || isBoldStr == String( F("true") ) || isBoldStr == String( F("TRUE") ) || isBoldStr == String( F("True") );
  String isSecondsShownStr = wifiWebServer.arg("s");
  bool isSecondsShown = isSecondsShownStr == String( F("1") ) || isSecondsShownStr == String( F("true") ) || isSecondsShownStr == String( F("TRUE") ) || isSecondsShownStr == String( F("True") );
  String isSingleDigitHourShownCurrentlyStr = wifiWebServer.arg("z");
  bool isSingleDigitHourShownCurrently = isSingleDigitHourShownCurrentlyStr == String( F("1") ) || isSingleDigitHourShownCurrentlyStr == String( F("true") ) || isSingleDigitHourShownCurrentlyStr == String( F("TRUE") ) || isSingleDigitHourShownCurrentlyStr == String( F("True") );
  String isCompactLayoutStr = wifiWebServer.arg("c");
  bool isCompactLayout = isCompactLayoutStr == String( F("1") ) || isCompactLayoutStr == String( F("true") ) || isCompactLayoutStr == String( F("TRUE") ) || isCompactLayoutStr == String( F("True") );

  String hourStrPreview, minuteStrPreview, secondStrPreview;
  if( timeCanBeCalculated() ) {
    calculateTimeToShow( hourStrPreview, minuteStrPreview, secondStrPreview, isSingleDigitHourShownCurrently );
  } else {
    hourStrPreview = "21";
    minuteStrPreview = "46";
    secondStrPreview = "37";
  }

  std::vector<std::vector<bool>> preview = getDisplayPreview( hourStrPreview, minuteStrPreview, secondStrPreview, fontNumber, isBold, isSecondsShown, isCompactLayout );
  String response = "";
  uint8_t lineNumber = 0;
  response += "[\n";
  for( const auto& row : preview ) {
    response += "  \"";
    for( const bool value : row ) {
      response += String( value ? "1": " " );
    }
    lineNumber++;
    response += "\"" + String( lineNumber < preview.size() ? "," : "" ) + String( F("\n") );
  }
  response += "]";
  wifiWebServer.send( 200, getContentType( F("json") ), response );

  if( isApInitialized ) { //this resets AP timeout when user loads the page in AP mode
    apStartedMillis = millis();
  }
}

void handleWebServerGetData() {
  String aniPreviewStr = wifiWebServer.arg("p");
  if( !aniPreviewStr.length() ) aniPreviewStr = "0";
  uint8_t aniPreview = static_cast<uint8_t>( atoi( aniPreviewStr.c_str() ) );

  if( aniPreview > 0 && aniPreview <= TCData::NUMBER_OF_ANIMATIONS_SUPPORTED ) {
    wifiWebServer.sendHeader( F("Cache-Control"), String( F("max-age=86400") ) );
    wifiWebServer.sendHeader( F("Content-Encoding"), F("gzip") );
    wifiWebServer.send_P( 200, getContentType( F("gif") ).c_str(), (const char*)TCData::getAnimation(aniPreview), TCData::getAnimationSize(aniPreview) );
  } else {

    String fileName = wifiWebServer.arg("f");
    if( fileName != "" ) {
      File file = getFileFromFlash( fileName );
      if( !file ) {
        wifiWebServer.send( 404, getContentType( F("txt") ), F("File not found") );
      } else {
        String fileExtension = "";
        int fileExtensionDot = fileName.lastIndexOf(".");
        if( fileExtensionDot != -1 ) {
            fileExtension = fileName.substring( fileExtensionDot + 1 );
        }
        wifiWebServer.sendHeader( F("Cache-Control"), String( F("max-age=86400") ) );
        wifiWebServer.streamFile( file, getContentType( fileExtension ) );
        file.close();
      }

      if( isApInitialized ) { //this resets AP timeout when user loads the page in AP mode
        apStartedMillis = millis();
      }
      return;
    }

  }

  if( isApInitialized ) { //this resets AP timeout when user loads the page in AP mode
    apStartedMillis = millis();
  }
}

void handleWebServerSetDate() {
  String dtStr = wifiWebServer.arg("t");
  if( dtStr != "" ) {
    char *strPtr;
    unsigned long long dt = std::strtoull( dtStr.c_str(), &strPtr, 10 );
    isCustomDateTimeSet = true;
    unsigned long currentMillis = millis();
    customDateTimeReceivedSeconds = dt / 1000;
    customDateTimeReceivedAt = currentMillis - ( dt % 1000 ); //align with second start
    customDateTimePrevMillis = currentMillis - ( dt % 1000 ); //align with second start
    wifiWebServer.send( 200, getContentType( F("json") ), "\"" + dtStr + "\"" );
  } else {
    wifiWebServer.send( 404, getContentType( F("txt") ), F("Error: 't' parameter not populated or not an epoch time with millis") );
  }

  if( isApInitialized ) { //this resets AP timeout when user loads the page in AP mode
    apStartedMillis = millis();
  }
}

void handleWebServerGetTestNight() {
  String content;
  addHtmlPageStart( content );
  content += getHtmlPageFillup( "5", "5" ) + String( F("<h2>Перевіряю нічний режим...</h2>") );
  addHtmlPageEnd( content );
  wifiWebServer.sendHeader( String( F("Content-Length") ).c_str(), String( content.length() ) );
  wifiWebServer.send( 200, getContentType( F("html") ), content );
  setDisplayBrightness( displayNightBrightness );
  renderDisplay();
  delay( 6000 );
  setDisplayBrightness( true );
  renderDisplay();

  if( isApInitialized ) { //this resets AP timeout when user loads the page in AP mode
    apStartedMillis = millis();
  }
}

void handleWebServerGetReset() {
  String content;
  addHtmlPageStart( content );
  content += getHtmlPageFillup( "9", "9" ) + String( F("<h2>Відновлюються заводські налаштування...<br>Після цього слід знову приєднати пристрій до WiFi мережі.</h2>") );
  addHtmlPageEnd( content );
  wifiWebServer.sendHeader( String( F("Content-Length") ).c_str(), String( content.length() ) );
  wifiWebServer.send( 200, getContentType( F("html") ), content );

  writeEepromUintValue( eepromFlashDataVersionIndex, 255 );
  EEPROM.commit();
  delay( 20 );

  delay( 500 );
  ESP.restart();
}

void handleWebServerGetTestLeds() {
  String content;
  addHtmlPageStart( content );
  content += getHtmlPageFillup( "20", "20" ) + String( F("<h2>Перевіряю матрицю...</h2>") );
  addHtmlPageEnd( content );
  wifiWebServer.sendHeader( String( F("Content-Length") ).c_str(), String( content.length() ) );
  wifiWebServer.send( 200, getContentType( F("html") ), content );
  for( uint8_t row = 0; row < 8; ++row ) {
    for( uint8_t y = 0; y < 8; ++y ) {
      for( uint8_t x = 0; x < 32; ++x ) {
        display.setPoint( y, 32 - 1 - x, row == y );
      }
    }
    display.update();
    delay( 400 );
  }
  for( uint8_t column = 0; column < 31; ++column ) {
    for( uint8_t y = 0; y < 8; ++y ) {
      for( uint8_t x = 0; x < 32; ++x ) {
        display.setPoint( y, 32 - 1 - x, column == x );
      }
    }
    display.update();
    delay( 200 );
  }
  for( uint8_t row = 2; row <= 8; row+=2 ) {
    for( uint8_t y = 0; y < 8; ++y ) {
      for( uint8_t x = 0; x < 32; ++x ) {
        display.setPoint( y, 32 - 1 - x, ( x % row == y % row || x % row == row - y % row ) );
      }
    }
    display.update();
    delay( 1500 );
  }
  for( uint8_t row = 0; row < 4; ++row ) {
    for( uint8_t y = 0; y < 8; ++y ) {
      for( uint8_t x = 0; x < 32; ++x ) {
        display.setPoint( y, 32 - 1 - x, ( ( x % 4 == row ) || ( y % 4 == row ) ) );
      }
    }
    display.update();
    delay( 1500 );
  }
  for( uint8_t y = 0; y < 8; ++y ) {
    for( uint8_t x = 0; x < 32; ++x ) {
      display.setPoint( y, 32 - 1 - x, true );
    }
  }
  display.update();
  delay( 1800 );

  if( isApInitialized ) { //this resets AP timeout when user loads the page in AP mode
    apStartedMillis = millis();
  }
}

void handleWebServerGetReboot() {
  String content;
  addHtmlPageStart( content );
  content += getHtmlPageFillup( "9", "9" ) + String( F("<h2>Перезавантажуюсь...</h2>") );
  addHtmlPageEnd( content );
  wifiWebServer.sendHeader( String( F("Content-Length") ).c_str(), String( content.length() ) );
  wifiWebServer.send( 200, getContentType( F("html") ), content );
  delay( 200 );
  ESP.restart();
}

void handleWebServerGetPing() {
  wifiWebServer.send( 204, getContentType( F("txt") ), "" );

  if( isApInitialized ) { //this resets AP timeout when user loads the page in AP mode
    apStartedMillis = millis();
  }
}

void handleWebServerGetMonitor() {
  time_t dt = 0;
  if( timeCanBeCalculated() ) {
    if( timeClient.isTimeSet() ) {
      dt = timeClient.getEpochTime();
    } else if( isCustomDateTimeSet ) {
      unsigned long currentMillis = millis();
      if( currentMillis >= customDateTimeReceivedAt && customDateTimePrevMillis < customDateTimeReceivedAt ) {
        unsigned long wrappedSeconds = ULONG_MAX / 1000 + 1;
        customDateTimeReceivedAt = customDateTimeReceivedAt - ( 1000 - ULONG_MAX % 1000 );
        customDateTimeReceivedSeconds = customDateTimeReceivedSeconds + wrappedSeconds;
      }
      customDateTimePrevMillis = currentMillis;
      unsigned long timeDiff = calculateDiffMillis( customDateTimeReceivedAt, currentMillis );
      dt = customDateTimeReceivedSeconds + timeDiff / 1000;
    }
  }
  struct tm *timeinfo = gmtime(&dt);

  String hourStr, minuteStr, secondStr;
  if( timeCanBeCalculated() ) {
    struct tm* dtStruct = localtime(&dt);
    uint8_t hour = dtStruct->tm_hour;
    hour += isWithinDstBoundaries( dt ) ? 3 : 2;
    if( hour >= 24 ) {
      hour = hour - 24;
    }
    hourStr = ( hour < 10 ? "0" : "" ) + String( hour );
    uint8_t minute = dtStruct->tm_min;
    minuteStr = ( minute < 10 ? "0" : "" ) + String( minute );
    uint8_t second = dtStruct->tm_sec;
    secondStr = ( second < 10 ? "0" : "" ) + String( second );
  }

  String flash_mode = "";
  FlashMode_t flash_mode_enum = ESP.getFlashChipMode();
  switch( flash_mode_enum ) {
    case FlashMode_t::FM_QIO: flash_mode = String( F("QIO") ); break;
    case FlashMode_t::FM_QOUT: flash_mode = String( F("QOUT") ); break;
    case FlashMode_t::FM_DIO: flash_mode = String( F("DIO") ); break;
    case FlashMode_t::FM_DOUT: flash_mode = String( F("DOUT") ); break;
    #ifdef ESP8266

    #elif defined(ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S2) || defined(ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32S3)
    case FlashMode_t::FM_FAST_READ: flash_mode = String( F("FAST_READ") ); break;
    case FlashMode_t::FM_SLOW_READ: flash_mode = String( F("SLOW_READ") ); break;
    #else
    case FlashMode_t::FM_FAST_READ: flash_mode = String( F("FAST_READ") ); break;
    case FlashMode_t::FM_SLOW_READ: flash_mode = String( F("SLOW_READ") ); break;
    #endif
    default: flash_mode = String( F("Unknown") ); break;
  }

  String content = String( F(""
  "{\n"
    "\t\"net\": {\n"
      "\t\t\"host\": \"") ) + getFullWiFiHostName() + String( F("\"\n"
    "\t},\n"
    "\t\"brt\": {\n"
      "\t\t\"cur\": ") ) + String( analogRead( BRIGHTNESS_INPUT_PIN ) ) + String( F(",\n"
      "\t\t\"avg\": ") ) + String( sensorBrightnessAverage ) + String( F(",\n"
      "\t\t\"req\": ") ) + String( displayCurrentBrightness ) + String( F(",\n"
      "\t\t\"dsp\": ") ) + String( static_cast<uint8_t>( round( displayPreviousBrightness ) ) ) + String( F("\n"
    "\t},\n"
    "\t\"ram\": {\n"
      "\t\t\"heap\": ") ) + String( ESP.getFreeHeap() );
      #ifdef ESP8266
      content = content + String( F(",\n"
        "\t\t\"frag\": ") ) + String( ESP.getHeapFragmentation() ) + String( F("\n") );
      #else
      content = content + String( F("\n") );
      #endif
      content = content + String( F(""
    "\t},\n"
    "\t\"cpu\": {\n"
      "\t\t\"chip\": \"") ) +
        #ifdef ESP8266
        String( F("ESP8266") ) +
        #elif defined(ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S2) || defined(ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32S3)
        String( F("ESP32S2 / ESP32S3") ) +
        #else
        String( F("ESP32") ) +
        #endif
      String( F("\",\n"
      "\t\t\"cpu_freq\": ") ) + String( ESP.getCpuFreqMHz() ) + String( F(",\n"
      "\t\t\"flash_freq\": ") ) + String( ESP.getFlashChipSpeed() / 1000000 ) + String( F(",\n"
      "\t\t\"flash_mode\": \"") ) + flash_mode + String( F("\",\n"
      "\t\t\"millis\": ") ) + String( millis() ) + String( F("\n"
    "\t},\n"
    "\t\"clock\": {\n"
      "\t\t\"date\": ") ) + ( timeCanBeCalculated() ? ( String( F( "\"" ) ) + String( timeinfo->tm_year + 1900 ) + String( F( "/" ) ) + String( ( timeinfo->tm_mon < 9 ? String( F( "0" ) ) : String( F( "" ) ) ) + String( timeinfo->tm_mon + 1 ) ) + String( F( "/" ) ) + String( ( timeinfo->tm_mday < 10 ?  String( F( "0" ) ) : String( F( "" ) ) ) + String( timeinfo->tm_mday ) ) ) + String( F( "\"" ) ) : String( F( "null" ) ) ) + String( F(",\n"
      "\t\t\"time\": ") ) + ( timeCanBeCalculated() ? ( String( F( "\"" ) ) + hourStr + String( F( ":" ) ) + minuteStr + String( F( ":" ) ) + secondStr + String( F( "\"" ) ) ) : String( F( "null" ) ) ) + String( F(",\n"
      "\t\t\"ntp\": ") ) + ( timeCanBeCalculated() ? ( String( F( "" ) ) + ( timeClient.isTimeSet() ? String( timeClient.getLastUpdateMillis() ) : String( customDateTimeReceivedAt ) ) + String( F( "" ) ) ) : "null" ) + String( F("\n"
    "\t}\n"
  "}" ) );
  wifiWebServer.send( 200, getContentType( F("json") ), content );
}

void handleWebServerGetFavIcon() {
  wifiWebServer.sendHeader( F("Cache-Control"), String( F("max-age=86400") ) );
  wifiWebServer.sendHeader( F("Content-Encoding"), F("gzip") );
  wifiWebServer.send_P( 200, getContentType( F("ico") ).c_str(), (const char*)TCData::getFavIcon(), TCData::getFavIconSize() );

  if( isApInitialized ) { //this resets AP timeout when user loads the page in AP mode
    apStartedMillis = millis();
  }
}

uint16_t getFontContentLength() {
  return 3 + TCFonts::FONT_SYMBOLS * TCFonts::FONT_HEIGHT;
}

const String fontIdentifier = "TC1";

void handleWebServerGetFontEditor() {
  wifiWebServer.setContentLength( CONTENT_LENGTH_UNKNOWN );
  wifiWebServer.send( 200, getContentType( F("html") ), "" );

  String content;
  content.reserve( 6000 ); //currently 5600 max (when sending Html Page Start)

  //5600
  addHtmlPageStart( content );
  wifiWebServer.sendContent( content );
  content = "";

  //3500
  content += String( F("<style>"
    ".wrp{width:96vw;min-width:none;max-width:none;}"
    ".fw{font-size:1vw;user-select:none;}"
    ".hlw,.slw{display:flex;margin-top:10px;gap:10px;}"
    ".hlw>div,.slw>div{width:calc(100%/9);}"
    ".hw.prog_p1,.sw.prog_p1{display:none;}"
    ".hw{align-self:center;text-align:center;}"
    ".shw{align-self:center;text-align:center;font-size:3vw;font-weight:bold;}"
    ".sh{background-color:#444;border:1px solid #555;padding:5px;}"
    ".pxl{display:flex;}"
    ".pxl .px{flex:1;border:1px solid #444;aspect-ratio:1;}"
    ".pxl .px:not(:first-child){border-left:none;}"
    ".pxl:not(:first-child) .px{border-top:none;}"
    ".px.px_off{background-color:#000;}"
    ".px.px_on{background-color:#0C0;}"
    ".px.x0, .px.x1{display:none;}"
    ".sw.prog_p1 .px.y0,.sw.prog_p1 .px.y1,"
    ".sw.hhmm_1.width_w0 .px.x2,.sw.hhmm_1.width_w0 .px.x3,"
    ".sw.hhmm_0 .px.x2,.sw.hhmm_0 .px.x3,"
    ".sw.hhmm_0.width_w0 .px.x2,.sw.hhmm_0.width_w0 .px.x3,.sw.hhmm_0.width_w0 .px.x4,"
    ".slw.s_cf .px.x2,.slw.s_cf .px.x3,.slw.s_cf .px.x4,.slw.s_cf .px.x5,.slw.s_cf .px.x6,"
    ".slw.s_cl .px.x2,.slw.s_cl .px.x3,.slw.s_cl .px.x4,.slw.s_cl .px.x5,.slw.s_cl .px.x6,"
    ".slw.s_cu .px.x2,.slw.s_cu .px.x3,.slw.s_cu .px.x4,.slw.s_cu .px.x5,.slw.s_cu .px.x6{background-color:#444;pointer-events:none;}"
  "</style>"
  "<script>"
    "let fontId='") ) + fontIdentifier + String( F("';"
    "let fontSizeBytes=") ) + String( getFontContentLength() ) + String( F(";"
    "let fontHeight=") ) + String( TCFonts::FONT_HEIGHT ) + String( F(";"
    "let symbols=[{nm:'1',txt:'1'},{nm:'2',txt:'2'},{nm:'3',txt:'3'},{nm:'4',txt:'4'},{nm:'5',txt:'5'},{nm:'6',txt:'6'},{nm:'7',txt:'7'},{nm:'8',txt:'8'},{nm:'9',txt:'9'},{nm:'0',txt:'0'},{nm:'-',txt:'-'},{nm:'cf',txt:':'},{nm:'cl',txt:'.'},{nm:'cu',txt:'˙'}];"
    "let attrs=[{nm:'hhmm',s1:'1',t1:'HH:MM',s0:'0',t0:'SS'},{nm:'prog',s1:'p0',t1:'',s0:'p1',t0:''},{nm:'width',s1:'w1',t1:'Wide',s0:'w0',t0:'Thin'},{nm:'thick',s1:'b0',t1:'Normal',s0:'b1',t0:'Bold'}];"
    "const gid=((id)=>document.getElementById(id));"
    "const gcl=((cl,el)=>[...(el?el:document).querySelectorAll(`${cl}`)]);"
    "const getPixelsDom=(classes=>{"
      "let res='';"
      "for(let y=0;y<=7;y++){"
          "res+='<div class=\"pxl\">';"
          "for(let x=7;x>=0;x--){"
              "res+='<div data-y=\"'+y+'\" data-x=\"'+x+'\" class=\"px y'+y+' x'+x+'\"></div>';"
          "}"
          "res+='</div>';"
      "}"
      "return res;"
    "});"
    "const getSymbols=((attrs,classes)=>{"
      "if(!attrs.length){return[classes.join(' ')];}"
      "return[...getSymbols([...attrs.slice(1)],[...classes,attrs[0].nm+'_'+attrs[0].s1]),...getSymbols([...attrs.slice(1)],[...classes,attrs[0].nm+'_'+attrs[0].s0])];"
    "});"
    "const getSymbolsDom=(()=>{return getSymbols(attrs,[]).reduce((res,item)=>res+'<div class=\"sw '+item+'\">'+getPixelsDom(item)+'</div>','');});"
    "const getHeaders=((attrs,classes,texts)=>{"
      "if(!attrs.length){return[{classes:classes.join(' '),txt:texts}];}"
      "return[...getHeaders([...attrs.slice(1)],[...classes,attrs[0].nm+'_'+attrs[0].s1],[...texts,attrs[0].t1]),...getHeaders([...attrs.slice(1)],[...classes,attrs[0].nm+'_'+attrs[0].s0],[...texts,attrs[0].t0])];"
    "});"
    "const getHeadersDom=(()=>{return getHeaders(attrs,[],[]).reduce((res, item)=>res+'<div class=\"hw '+item.classes+'\">'+item.txt.map(text=>'<div>'+text+'</div>').join('')+'</div>','');});"
    "let btnDown = false;"
    "let btnAdds = false;"
    "const enableEdit=(()=>{"
      "document.addEventListener('mousedown',function(){btnDown=true;});"
      "document.addEventListener('mouseup',function(){btnDown=false;});"
      "gcl('.px').forEach(px=>{"
        "px.addEventListener('mousedown',function(){"
          "btnAdds=px.classList.contains('px_off');"
          "px.classList.toggle('px_on');"
          "px.classList.toggle('px_off');"
        "});"
        "px.addEventListener('mouseover',function(){"
          "if(!btnDown)return;"
          "if((!btnAdds||px.classList.contains('px_on'))&&(btnAdds||px.classList.contains('px_off')))return;"
          "px.classList.toggle('px_on');"
          "px.classList.toggle('px_off');"
        "});"
      "});"
    "});" ) );
  wifiWebServer.sendContent( content );
  content = "";

  //3700
  content += String( F(""
    "const createDom=(()=>{"
      "let dom='<div class=\"fw\"><div class=\"hlw\"><div></div>'+getHeadersDom()+'</div>';"
      "symbols.forEach(s=>{"
        "dom+='<div class=\"slw s_'+s.nm+'\"><div class=\"shw\"><span class=\"sh\">'+s.txt+'</span></div>'+getSymbolsDom()+'</div>';"
      "});"
      "dom+='</div>';"
      "gid('fanchor').innerHTML=dom;"
      ""
      "document.getElementById('uploadFileData').addEventListener('change',function(event) {"
        "if(!event.target.files.length)return;"
        "let file=event.target.files[0];"
        "if(file){"
          "let reader=new FileReader();"
          "reader.onload=function(e){"
            "let dt=new Uint8Array(e.target.result);"
            "populateFont(dt);"
            "document.getElementById('uploadFileData').value='';"
          "};"
          "reader.onerror=function(){"
            "alert('Error reading file!');"
          "};"
          "reader.readAsArrayBuffer(file);"
        "}"
      "});"
    "});"
    "const populateFont=(dt=>{"
      "let ss=[];"
      "let fontIdRes=String.fromCharCode(dt[0],dt[1],dt[2]);"
      "if(fontIdRes!=fontId){alert('Font is not supported');return;}"
      "for(let i=fontId.length;i<dt.length;i+=fontHeight){"
        "ss.push(dt.slice(i,i+fontHeight));"
      "}"
      "gcl('.sw').forEach((sw,swi)=>{"
        "gcl('.pxl',sw).forEach((pl,pli)=>{"
          "gcl('.px',pl).forEach(px=>{"
              "let bitIdx=px.dataset.x;"
              "let fntLine=ss[swi][pli];"
              "let isOn=(fntLine>>bitIdx)&1;"
              "if(isOn==1){"
                "px.classList.add('px_on');"
                "px.classList.remove('px_off');"
              "}else{"
                "px.classList.add('px_off');"
                "px.classList.remove('px_on');"
              "}"
          "});"
        "});"
      "});"
    "});"
    "const getFont=(()=>{"
      "let font=new Uint8Array(fontSizeBytes);"
      "let byteIdx=0;"
      "font.set(new TextEncoder().encode(fontId), byteIdx);"
      "byteIdx+=fontId.length;"
      "gcl('.sw').forEach((sw,swi)=>{"
        "gcl('.pxl',sw).forEach((pl,pli)=>{"
          "let lineByte=0;"
          "gcl('.px',pl).forEach(px=>{"
            "let bitIdx=parseInt(px.dataset.x,10);"
            "let isOn=px.classList.contains('px_on');"
            "if(isOn){"
              "lineByte|=(1<<bitIdx);"
            "}"
          "});"
          "font.set([lineByte],byteIdx);"
          "byteIdx++;"
        "});"
      "});"
      "return font;"
    "});"
    "const readEspData=(isInit=>{"
      "fetch('/font?f='+gid('fontPicker').value).then(res=>{"
        "return res.ok?res.arrayBuffer():null;"
      "}).then(dtb=>{"
        "if(dtb==null)return;"
        "let dt=new Uint8Array(dtb);"
        "populateFont(dt);"
        "if(isInit){"
          "enableEdit();"
        "}"
      "}).catch(e=>{"
        "console.error('Error: ',e);"
      "});"
    "});"
    "const saveEspData=(()=>{"
      "let font=getFont();"
#ifdef ESP8266
      "fetch('/font',{"
        "method:'POST',"
        "headers:{"
          "'Content-Type':'application/octet-stream',"
          "'Content-Length':font.length.toString()"
        "},"
        "body:font"
#else //ESP32 or ESP32S2
      "let fontBin='';"
      "for(let i=0;i<font.length;i++){"
        "fontBin+=String.fromCharCode(font[i]);"
      "}"
      "let fontb64=btoa(fontBin);"
      "fetch('/font',{"
        "method:'POST',"
        "headers:{"
          "'Content-Type':'text/plain',"
          "'Content-Length':fontb64.length.toString()"
        "},"
        "body:fontb64"
#endif
      "}).then(res=>{"
        "if(!res.ok){alert('Failed to upload font');}"
      "}).catch(e=>{"
        "console.error('Error: ',e);"
      "});"
    "});"
    "const downloadFileData=(()=>{"
      "let blob=new Blob([getFont()],{type:'application/octet-stream'});"
      "let url=URL.createObjectURL(blob);"
      "let a=document.createElement('a');"
      "a.href=url;"
      "a.download='font.tcf';"
      "document.body.appendChild(a);"
      "a.click();"
      "document.body.removeChild(a);"
      "URL.revokeObjectURL(url);"
    "});"
    "document.addEventListener(\"DOMContentLoaded\",()=>{"
      "createDom();"
      "readEspData(true);"
    "});"
  "</script>"
  "<div class=\"ft\">"
    "<div class=\"fx\">"
      "<div>"
        "<button class=\"fixed\" onclick=\"gid('uploadFileData').click()\">Прочитати з файла</button><input type=\"file\" class=\"fixed\" id=\"uploadFileData\" accept=\".tcf\" style=\"display:none;\"/>"
        "<button class=\"fixed\" onclick=\"downloadFileData();\">Зберегти в файл</button>"
      "</div>"
      "<div style=\"flex-grow:1;\">"
      "</div>"
      "<div>"
        "<select id=\"fontPicker\" class=\"fixed\" style=\"padding:1px 1px 2px 1px;\">"
          "<option value=\"1\">Шрифт 1</option>"
          "<option value=\"2\">Шрифт 2</option>"
          "<option value=\"3\">Шрифт 3</option>"
          "<option value=\"4\">Шрифт 4</option>"
          "<option value=\"5\" selected>Шрифт 5 (свій)</option>"
        "</select>"
        "<button class=\"fixed\" onclick=\"readEspData(false);\">Прочитати з ESP</button>"
        "<button class=\"fixed\" onclick=\"saveEspData();\">Зберегти в ESP</button>"
        "<span class=\"i\" title=\"Збереження завжди йде в Шрифт 5 (свій)!\"></span>"
      "</div>"
    "</div>"
  "</div>"
  "<div id=\"fanchor\"></div>"
  "<div class=\"ft\">"
    "<div class=\"fx\">"
      "<span>"
        "<span class=\"sub\"><a href=\"/\">Назад</a></span>"
      "</span>"
    "</div>"
  "</div>") );
  addHtmlPageEnd( content );
  wifiWebServer.sendContent( content );
  content = "";
}

void handleWebServerGetFont() {
  String fontNumberStr = wifiWebServer.arg("f");
  if( !fontNumberStr.length() ) fontNumberStr = String( TCFonts::NUMBER_OF_FONTS_SUPPORTED );
  uint8_t fontNumber = static_cast<uint8_t>( atoi( fontNumberStr.c_str() ) );
  if( fontNumber > TCFonts::NUMBER_OF_FONTS_SUPPORTED ) fontNumber = TCFonts::NUMBER_OF_FONTS_SUPPORTED;
  if( fontNumber < 1 ) fontNumber = 1;

  wifiWebServer.setContentLength( getFontContentLength() ); //Chunked transfer encoding
  wifiWebServer.send( 200, "application/octet-stream", "" );

  std::pair<const uint8_t (*)[TCFonts::FONT_HEIGHT], bool> fontInfo = TCFonts::getFont( fontNumber );
  const uint8_t (*fontToUse)[TCFonts::FONT_HEIGHT] = fontInfo.first;
  bool isCustomFont = fontInfo.second;

  uint8_t buffer[256];
  size_t index = 0;

  for( size_t i = 0; i < fontIdentifier.length(); i++ ) {
    buffer[index++] = fontIdentifier[i];
  }
  for( size_t i = 0; i < TCFonts::FONT_SYMBOLS; i++ ) {
      for( size_t j = 0; j < TCFonts::FONT_HEIGHT; j++ ) {
          if( isCustomFont ) {
            buffer[index++] = fontToUse[i][j];
          } else {
            buffer[index++] = pgm_read_byte(&fontToUse[i][j]);
          }
          if( index == sizeof(buffer) ) {
              wifiWebServer.client().write( buffer, index );
              index = 0;
          }
      }
  }
  if( index > 0 ) {
      wifiWebServer.client().write( buffer, index );
  }

  wifiWebServer.client().stop();
}

void handleWebServerPostFont() {
  uint16_t expectedContentLength = getFontContentLength();
  #ifdef ESP8266
  String body = wifiWebServer.arg("plain");
  #else //ESP32 or ESP32S2
  String body = wifiWebServer.arg("plain"); //base64 encoded binary
  size_t actualDecodedResultLength = 0;
  uint8_t decoded[expectedContentLength];
  int convertResult = mbedtls_base64_decode( decoded, sizeof(decoded), &actualDecodedResultLength, (const unsigned char*)body.c_str(), body.length() );
  if( convertResult != 0 ) {
    if( convertResult == MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL ) {
      wifiWebServer.send( 500, getContentType("txt"), String( F("Base64 buffer too small (") ) + String(expectedContentLength) + String( F("). Need at least") ) + String(actualDecodedResultLength) + String( F(" bytes") ) );
    } else if( convertResult == MBEDTLS_ERR_BASE64_INVALID_CHARACTER ) {
      wifiWebServer.send( 400, getContentType("txt"), String( F("Base64 data contains invalid character") ) );
    } else {
      wifiWebServer.send( 400, getContentType("txt"), String( F("Base64 decode error: ") ) + String( convertResult ) );
    }
    return;
  }

  if( actualDecodedResultLength != expectedContentLength ) {
    wifiWebServer.send( 400, getContentType("txt"), String( F("Data size is incorrect") ) );
    return;
  }
  body = String( (const char*)decoded, actualDecodedResultLength ); //actual base64 decoded binary
  #endif

  if( body.length() != expectedContentLength ) {
    wifiWebServer.send( 400, getContentType("txt"), String( F("Data size is incorrect") ) );
    return;
  }

  String receivedIdentifier = body.substring(0, 3);
  if( receivedIdentifier != fontIdentifier ) {
    wifiWebServer.send( 400, getContentType("txt"), String( F("Incorrect data loaded. Is it a font?") ) );
    return;
  }

  uint16_t remainingDataStart = fontIdentifier.length();
  uint8_t (*fontToUse)[TCFonts::FONT_HEIGHT] = new uint8_t[TCFonts::FONT_SYMBOLS][TCFonts::FONT_HEIGHT];
  uint16_t byteIndex = remainingDataStart;
  for( uint16_t i = 0; i < TCFonts::FONT_SYMBOLS; i++ ) {
      for( uint8_t j = 0; j < TCFonts::FONT_HEIGHT; j++ ) {
          fontToUse[i][j] = body[byteIndex++];
      }
  }
  TCFonts::setCustomFont( fontToUse );
  delete[] fontToUse;

  writeEepromFontData( false );

  wifiWebServer.send( 200, getContentType("txt"), String( F("OK") ) );
}

void handleWebServerRedirect() {
  wifiWebServer.sendHeader( F("Location"), String( F("http://") ) + WiFi.softAPIP().toString() );
  wifiWebServer.send( 302, getContentType( F("html") ), "" );
  wifiWebServer.client().stop();
}

bool isWebServerInitialized = false;
void stopWebServer() {
  if( !isWebServerInitialized ) return;
  wifiWebServer.stop();
  isWebServerInitialized = false;
}

void startWebServer() {
  if( isWebServerInitialized ) return;
  Serial.print( F("Starting web server...") );
  wifiWebServer.begin();
  isWebServerInitialized = true;
  Serial.println( " done" );
}

void configureWebServer() {
  wifiWebServer.on( "/", HTTP_GET,  handleWebServerGet );
  wifiWebServer.on( "/", HTTP_POST, handleWebServerPost );
  wifiWebServer.on( "/preview", HTTP_GET, handleWebServerGetPreview );
  wifiWebServer.on( "/data", HTTP_GET, handleWebServerGetData );
  wifiWebServer.on( "/setdt", HTTP_GET, handleWebServerSetDate );
  wifiWebServer.on( "/testdim", HTTP_GET, handleWebServerGetTestNight );
  wifiWebServer.on( "/reset", HTTP_GET, handleWebServerGetReset );
  wifiWebServer.on( "/testled", HTTP_GET, handleWebServerGetTestLeds );
  wifiWebServer.on( "/reboot", HTTP_GET, handleWebServerGetReboot );
  wifiWebServer.on( "/ping", HTTP_GET, handleWebServerGetPing );
  wifiWebServer.on( "/monitor", HTTP_GET, handleWebServerGetMonitor );
  wifiWebServer.on( "/favicon.ico", HTTP_GET, handleWebServerGetFavIcon );
  wifiWebServer.on( "/fontedit", HTTP_GET, handleWebServerGetFontEditor );
  wifiWebServer.on( "/font", HTTP_GET, handleWebServerGetFont );
  wifiWebServer.on( "/font", HTTP_POST, handleWebServerPostFont );
  wifiWebServer.onNotFound([]() {
    handleWebServerRedirect();
  });
  httpUpdater.setup( &wifiWebServer );
}


#ifdef ESP8266
WiFiEventHandler wiFiEventHandler;
void onWiFiConnected( const WiFiEventStationModeConnected& event ) {
  Serial.println( String( F("WiFi is connected to '") ) + String( event.ssid ) + String ( F("'") ) );
}
#else //ESP32 or ESP32S2
void WiFiEvent( WiFiEvent_t event ) {
  switch( event ) {
    case SYSTEM_EVENT_STA_GOT_IP:
      Serial.println( String( F("WiFi is connected to '") ) + String( WiFi.SSID() ) + String ( F("' with IP ") ) + WiFi.localIP().toString() );
      break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
      //
      break;
    default:
      break;
  }
}
#endif


void setup() {
  initVariables();
  initInternalLed();
  initDisplayPhase1();

  Serial.begin( 115200 );
  Serial.println();
  Serial.println( String( F("Clock by Dmytro Kurylo. V@") ) + getFirmwareVersion() + String( F(" CPU@") ) + String( ESP.getCpuFreqMHz() ) );

  #ifdef ESP8266

  #elif defined(ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S2) || defined(ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32S3)
  pinMode( HARD_RESET_PIN, INPUT_PULLDOWN );
  if( digitalRead( HARD_RESET_PIN ) ) {
    eepromFlashDataVersion = 255;
  }
  #else

  #endif

  initEeprom();
  loadEepromData();
  initDisplayPhase2();
  LittleFS.begin();

  configureWebServer();
  #ifdef ESP8266
  wiFiEventHandler = WiFi.onStationModeConnected( &onWiFiConnected );
  #else //ESP32 or ESP32S2
  WiFi.onEvent( WiFiEvent );
  #endif
  connectToWiFiAsync( true );
  startWebServer();
  initTimeClient();
}


void loop() {
  if( isDisplayIntensityUpdateRequiredAfterSettingChanged ) {
    setDisplayBrightness( true );
    isDisplayIntensityUpdateRequiredAfterSettingChanged = false;
  }
  if( isDisplayRerenderRequiredAfterSettingChanged ) {
    renderDisplay();
    isDisplayRerenderRequiredAfterSettingChanged = false;
  }

  unsigned long currentMillis = millis();

  if( isFirstLoopRun || ( calculateDiffMillis( previousMillisInternalLed, currentMillis ) >= ( getInternalLedStatus() == HIGH ? DELAY_INTERNAL_LED_ANIMATION_HIGH : DELAY_INTERNAL_LED_ANIMATION_LOW ) ) ) {
    previousMillisInternalLed = currentMillis;
    setInternalLedStatus( getInternalLedStatus() == HIGH ? LOW : HIGH );
  }

  if( isApInitialized ) {
    dnsServer.processNextRequest();
  }
  wifiWebServer.handleClient();

  brightnessProcessLoopTick();

  currentMillis = millis();
  if( isApInitialized && isRouterSsidProvided() && ( calculateDiffMillis( apStartedMillis, currentMillis ) >= TIMEOUT_AP ) ) {
    shutdownAccessPoint();
    connectToWiFiAsync( true );
    previousMillisWiFiStatusCheck = currentMillis;
  }

  if( /*isFirstLoopRun || */( calculateDiffMillis( previousMillisWiFiStatusCheck, currentMillis ) >= DELAY_WIFI_CONNECTION_CHECK ) ) {
    if( !WiFi.isConnected() ) {
      if( !isApInitialized ) {
        connectToWiFiAsync( false );
      }
    }
    previousMillisWiFiStatusCheck = currentMillis;
  }

  ntpProcessLoopTick();

  currentMillis = millis();
  if( isFirstLoopRun || isForceDisplaySync || ( calculateDiffMillis( previousMillisDisplayAnimation, currentMillis ) >= DELAY_DISPLAY_ANIMATION ) ) {
    bool doRenderDisplay = true;
    if( isForceDisplaySync ) {
      if( timeCanBeCalculated() ) {
        unsigned long timeClientSecondsCurrent = 0;
        int timeClientSubSecondsCurrent = 0;
        if( timeClient.isTimeSet() ) {
          timeClientSecondsCurrent = timeClient.getEpochTime();
          timeClientSubSecondsCurrent = timeClient.getSubSeconds();
        } else if( isCustomDateTimeSet ) {
          timeClientSecondsCurrent = customDateTimeReceivedSeconds + calculateDiffMillis( customDateTimeReceivedAt, currentMillis ) / 1000;
          timeClientSubSecondsCurrent = calculateDiffMillis( customDateTimeReceivedAt, currentMillis ) % 1000;
        }
        bool shouldSemicolonBeShown = isSlowSemicolonAnimation ? ( timeClientSecondsCurrent % 2 == 0 ) : ( timeClientSubSecondsCurrent < 500 );
        if( isSemicolonShown == shouldSemicolonBeShown ) {
          doRenderDisplay = false;
        }
        isSemicolonShown = shouldSemicolonBeShown;
        previousMillisSemicolonAnimation = currentMillis - ( isSlowSemicolonAnimation ? ( timeClientSubSecondsCurrent % 1000 ) : ( timeClientSubSecondsCurrent % 500 ) );
        previousMillisDisplayAnimation = currentMillis - timeClientSubSecondsCurrent % DELAY_DISPLAY_ANIMATION;
        isForceDisplaySync = false;
      } else {
        isSemicolonShown = !isSemicolonShown;
        previousMillisSemicolonAnimation = currentMillis;
        previousMillisDisplayAnimation = currentMillis;
        isForceDisplaySync = false;
      }
    } else {
      previousMillisDisplayAnimation += DELAY_DISPLAY_ANIMATION;
      if( calculateDiffMillis( previousMillisSemicolonAnimation, currentMillis ) >= ( isSlowSemicolonAnimation ? 1000 : 500 ) ) {
        isSemicolonShown = !isSemicolonShown;
        previousMillisSemicolonAnimation += ( isSlowSemicolonAnimation ? 1000 : 500 );
      }
    }

    if( doRenderDisplay || isForceDisplaySyncDisplayRenderOverride ) {
      renderDisplay();
      isForceDisplaySyncDisplayRenderOverride = false;
    }
  }

  isFirstLoopRun = false;
  delay(2); //https://www.tablix.org/~avian/blog/archives/2022/08/saving_power_on_an_esp8266_web_server_using_delays/
}