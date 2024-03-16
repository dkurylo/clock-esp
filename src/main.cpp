#include <Arduino.h>

#include <vector>

#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#else //ESP32 or ESP32S2
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPUpdateServer.h>
#endif

#include <DNSServer.h> //for Captive Portal

#include <NTPClient.h>
#include <WiFiUdp.h>
#include <EEPROM.h>
#include <LittleFS.h>

#include <MD_MAX72xx.h>

#include <TCFonts.h>

#define MAX_HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_MAX_DEVICES 4
//#define MAX_DATA_PIN 0 //if using HW SPI then it's automatically MOSI 13
//#define MAX_CLK_PIN 5 //if using HW SPI then it's automatically SCK 14
#define MAX_CS_PIN 15 //if using HW SPI then it's SS 15 (not automatically)

bool isNewBoard = false;
const char* getFirmwareVersion() { const char* result = "1.00"; return result; }

//wifi access point configuration
const char* getWiFiAccessPointSsid() { const char* result = "Clock"; return result; };
const char* getWiFiAccessPointPassword() { const char* result = "1029384756"; return result; };
const IPAddress getWiFiAccessPointIp() { IPAddress result( 192, 168, 1, 1 ); return result; };
const IPAddress getWiFiAccessPointNetMask() { IPAddress result( 255, 255, 255, 0 ); return result; };
const uint32_t TIMEOUT_AP = 120000;

//wifi client configuration
const uint8_t WIFI_SSID_MAX_LENGTH = 32;
const uint8_t WIFI_PASSWORD_MAX_LENGTH = 32;
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
const uint16_t TIMEOUT_NTP_CLIENT_CONNECT = 2500;
const uint16_t DELAY_NTP_UPDATED_CHECK = 10000;
const uint32_t DELAY_NTP_TIME_SYNC = 3600000;
bool isSlowSemicolonAnimation = false; //semicolon period, false = 60 blinks per minute; true = 30 blinks per minute
const uint16_t DELAY_DISPLAY_ANIMATION = 20; //led animation speed, in ms

const bool IS_LOW_LEVEL_BUZZER = true;

//variables used in the code, don't change anything here
char wiFiClientSsid[WIFI_SSID_MAX_LENGTH];
char wiFiClientPassword[WIFI_PASSWORD_MAX_LENGTH];

uint8_t displayFontTypeNumber = 0;
bool isDisplayBoldFontUsed = true;
bool isDisplaySecondsShown = false;
uint8_t displayDayModeBrightness = 9;
uint8_t displayNightModeBrightness = 1;
bool isForceDisplaySync = true;
bool isForceDisplaySyncDisplayRenderOverride = false;
bool isSingleDigitHourShown = false;
bool isRotateDisplay = false;
bool isClockAnimated = false;
uint8_t animationTypeNumber = 0;

//brightness settings
const uint16_t DELAY_SENSOR_BRIGHTNESS_UPDATE_CHECK = 100;
const uint16_t SENSOR_BRIGHTNESS_NIGHT_LEVEL = 10;
const uint16_t SENSOR_BRIGHTNESS_DAY_LEVEL = 350;
const double SENSOR_BRIGHTNESS_LEVEL_HYSTERESIS = 0.10;
const uint16_t SENSOR_BRIGHTNESS_SUSTAINED_LEVEL_HYSTERESIS_OVERRIDE_MILLIS = 10000;

//custom datetime settings (used when there is no internet connection)
bool isCustomDateTimeSet = false;
unsigned long customDateTimeReceivedAt = 0;
unsigned long customDateTimePrevMillis = 0;
unsigned long customDateTime = 0;

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
unsigned long previousMillisNtpUpdatedCheck = millis();
unsigned long previousMillisWiFiStatusCheck = millis();
unsigned long previousMillisTimeClientStatusCheck = millis();
unsigned long previousMillisSensorBrightnessCheck = millis();

bool forceNtpUpdate = false;

void initVariables() {
  isFirstLoopRun = true;
  unsigned long currentMillis = millis();
  previousMillisDisplayAnimation = currentMillis;
  previousMillisSemicolonAnimation = currentMillis;
  previousMillisInternalLed = currentMillis;
  previousMillisNtpUpdatedCheck = currentMillis;
  previousMillisWiFiStatusCheck = currentMillis;
  previousMillisTimeClientStatusCheck = currentMillis;
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
const uint16_t eepromIsNewBoardIndex = 0;
const uint16_t eepromWiFiSsidIndex = eepromIsNewBoardIndex + 1;
const uint16_t eepromWiFiPasswordIndex = eepromWiFiSsidIndex + WIFI_SSID_MAX_LENGTH;
const uint16_t eepromDisplayFontTypeNumberIndex = eepromWiFiPasswordIndex + WIFI_PASSWORD_MAX_LENGTH;
const uint16_t eepromIsFontBoldUsedIndex = eepromDisplayFontTypeNumberIndex + 1;
const uint16_t eepromIsDisplaySecondsShownIndex = eepromIsFontBoldUsedIndex + 1;
const uint16_t eepromDisplayDayModeBrightnessIndex = eepromIsDisplaySecondsShownIndex + 1;
const uint16_t eepromDisplayNightModeBrightnessIndex = eepromDisplayDayModeBrightnessIndex + 1;
const uint16_t eepromIsSingleDigitHourShownIndex = eepromDisplayNightModeBrightnessIndex + 1;
const uint16_t eepromIsRotateDisplayIndex = eepromIsSingleDigitHourShownIndex + 1;
const uint16_t eepromIsSlowSemicolonAnimationIndex = eepromIsRotateDisplayIndex + 1;
const uint16_t eepromIsClockAnimatedIndex = eepromIsSlowSemicolonAnimationIndex + 1;
const uint16_t eepromAnimationTypeNumberIndex = eepromIsClockAnimatedIndex + 1;
const uint16_t eepromLastByteIndex = eepromAnimationTypeNumberIndex + 1;

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

uint8_t readEepromIntValue( const uint16_t& eepromIndex, uint8_t& variableWithValue, bool doApplyValue ) {
  uint8_t eepromValue = EEPROM.read( eepromIndex );
  if( doApplyValue ) {
    variableWithValue = eepromValue;
  }
  return eepromValue;
}

bool writeEepromIntValue( const uint16_t& eepromIndex, uint8_t newValue ) {
  bool eepromWritten = false;
  if( readEepromIntValue( eepromIndex, newValue, false ) != newValue ) {
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

void loadEepromData() {
  readEepromBoolValue( eepromIsNewBoardIndex, isNewBoard, true );
  if( !isNewBoard ) {

    readEepromCharArray( eepromWiFiSsidIndex, wiFiClientSsid, WIFI_SSID_MAX_LENGTH, true );
    readEepromCharArray( eepromWiFiPasswordIndex, wiFiClientPassword, WIFI_PASSWORD_MAX_LENGTH, true );
    readEepromIntValue( eepromDisplayFontTypeNumberIndex, displayFontTypeNumber, true );
    readEepromBoolValue( eepromIsFontBoldUsedIndex, isDisplayBoldFontUsed, true );
    readEepromBoolValue( eepromIsDisplaySecondsShownIndex, isDisplaySecondsShown, true );
    readEepromIntValue( eepromDisplayDayModeBrightnessIndex, displayDayModeBrightness, true );
    readEepromIntValue( eepromDisplayNightModeBrightnessIndex, displayNightModeBrightness, true );
    readEepromBoolValue( eepromIsSingleDigitHourShownIndex, isSingleDigitHourShown, true );
    readEepromBoolValue( eepromIsRotateDisplayIndex, isRotateDisplay, true );
    readEepromBoolValue( eepromIsSlowSemicolonAnimationIndex, isSlowSemicolonAnimation, true );
    readEepromBoolValue( eepromIsClockAnimatedIndex, isClockAnimated, true );
    readEepromIntValue( eepromAnimationTypeNumberIndex, animationTypeNumber, true );

  } else { //fill EEPROM with default values when starting the new board
    writeEepromBoolValue( eepromIsNewBoardIndex, false );

    writeEepromCharArray( eepromWiFiSsidIndex, wiFiClientSsid, WIFI_SSID_MAX_LENGTH );
    writeEepromCharArray( eepromWiFiPasswordIndex, wiFiClientPassword, WIFI_PASSWORD_MAX_LENGTH );
    writeEepromIntValue( eepromDisplayFontTypeNumberIndex, displayFontTypeNumber );
    writeEepromBoolValue( eepromIsFontBoldUsedIndex, isDisplayBoldFontUsed );
    writeEepromBoolValue( eepromIsDisplaySecondsShownIndex, isDisplaySecondsShown );
    writeEepromIntValue( eepromDisplayDayModeBrightnessIndex, displayDayModeBrightness );
    writeEepromIntValue( eepromDisplayNightModeBrightnessIndex, displayNightModeBrightness );
    writeEepromBoolValue( eepromIsSingleDigitHourShownIndex, isSingleDigitHourShown );
    writeEepromBoolValue( eepromIsRotateDisplayIndex, isRotateDisplay );
    writeEepromBoolValue( eepromIsSlowSemicolonAnimationIndex, isSlowSemicolonAnimation );
    writeEepromBoolValue( eepromIsClockAnimatedIndex, isClockAnimated );
    writeEepromIntValue( eepromAnimationTypeNumberIndex, animationTypeNumber );

    isNewBoard = false;
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
  WiFi.softAP( ( String( getWiFiAccessPointSsid() ) + " " + String( ESP.getChipId() ) ).c_str(), getWiFiAccessPointPassword(), 0, false );
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


//time of day functionality
bool isTimeClientInitialised = false;
unsigned long timeClientUpdatedMillis = 0;
bool timeClientTimeInitStatus = false;
void initTimeClient() {
  if( WiFi.isConnected() && !isTimeClientInitialised ) {
    timeClient.setUpdateInterval( DELAY_NTP_TIME_SYNC );
    Serial.print( F("Starting NTP client...") );
    timeClient.begin();
    isTimeClientInitialised = true;
    Serial.println( F(" done") );
  }
}

bool updateTimeClient( bool canWait ) {
  if( !WiFi.isConnected() ) return false;
  if( !timeClient.isTimeSet() ) {
    if( !isTimeClientInitialised ) {
      initTimeClient();
    }
    if( isTimeClientInitialised && !isCustomDateTimeSet ) {
      Serial.print( F("Updating NTP time...") );
      timeClient.update();
      if( canWait ) {
        timeClientUpdatedMillis = millis();
        while( !timeClient.isTimeSet() && ( calculateDiffMillis( timeClientUpdatedMillis, millis() ) < TIMEOUT_NTP_CLIENT_CONNECT ) ) {
          delay( 250 );
          Serial.print( "." );
        }
      }
      Serial.println( F(" done") );
      previousMillisTimeClientStatusCheck = millis();
    }
  }
  return true;
}

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
  return dt > lastMarchSunday_t && dt < lastOctoberSunday_t;
}


//display functionality
double displayCurrentBrightness = static_cast<double>(displayNightModeBrightness);
double displayPreviousBrightness = -1.0;
double sensorBrightnessAverage = -1.0;
int brightnessDiffSustainedMillis = 0;

void calculateDisplayBrightness() {
  uint16_t currentBrightness = analogRead(A0);
  if( sensorBrightnessAverage < 0 ) {
    sensorBrightnessAverage = (double)currentBrightness;
  } else {
    uint8_t sensorBrightnessSamples = 15;
    sensorBrightnessAverage = ( sensorBrightnessAverage * ( static_cast<double>(sensorBrightnessSamples) - static_cast<double>(1.0) ) + currentBrightness ) / static_cast<double>(sensorBrightnessSamples);
  }

  if( sensorBrightnessAverage >= SENSOR_BRIGHTNESS_DAY_LEVEL ) {
    displayCurrentBrightness = static_cast<double>(displayDayModeBrightness);
  } else if( sensorBrightnessAverage <= SENSOR_BRIGHTNESS_NIGHT_LEVEL ) {
    displayCurrentBrightness = static_cast<double>(displayNightModeBrightness);
  } else {
    displayCurrentBrightness = displayNightModeBrightness + static_cast<double>( displayDayModeBrightness - displayNightModeBrightness ) * ( sensorBrightnessAverage - SENSOR_BRIGHTNESS_NIGHT_LEVEL ) / ( SENSOR_BRIGHTNESS_DAY_LEVEL - SENSOR_BRIGHTNESS_NIGHT_LEVEL );
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

void initDisplay() {
  calculateDisplayBrightness();
  display.begin();
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
void renderDisplayText( String textToDisplayLarge, String textToDisplaySmall, bool isSecondsShown, bool doAnimate ) {
  unsigned long currentMillis = millis();

  char charsAnimatable[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9' };
  uint8_t charsAnimatableCount = sizeof(charsAnimatable) / sizeof(charsAnimatable[0]);
  unsigned long displayAnimationLengthMillis = 8 * displayAnimationStepLengthMillis;
  if( animationTypeNumber == 0 || animationTypeNumber == 1 ) {
    displayAnimationLengthMillis += displayAnimationStepLengthMillis;
  }

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
  std::map<String, std::vector<uint8_t>> font = TCFonts::getFont( displayFontTypeNumber );

  for( size_t charToDisplayIndex = 0; charToDisplayIndex < textToDisplayLarge.length(); ++charToDisplayIndex ) {
    char charToDisplay = textToDisplayLarge.charAt( charToDisplayIndex );
    uint8_t charWidth = TCFonts::getSymbolWidth( displayFontTypeNumber, charToDisplay, isDisplayBoldFontUsed, !isSecondsShown, false );
    if( displayWidthUsed + charWidth > DISPLAY_WIDTH ) {
      charWidth = DISPLAY_WIDTH - displayWidthUsed;
    }
    if( charWidth == 0 ) continue;
    std::vector<uint8_t> charImage = TCFonts::getSymbol( font, charToDisplay, isDisplayBoldFontUsed, !isSecondsShown, false );
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
          charImagePrevious = TCFonts::getSymbol( font, charToDisplayPrevious, isDisplayBoldFontUsed, !isSecondsShown, false );
        }
      }
    }

    for( uint8_t displayY = 0; displayY < DISPLAY_HEIGHT; ++displayY ) {
      for( uint8_t charX = 0; charX < charWidth; ++charX ) {
        bool isPointEnabled = false;
        uint8_t charShiftY = DISPLAY_HEIGHT - charImage.size();
        if( displayY >= charShiftY ) {
          uint8_t charY = displayY - charShiftY;
          isPointEnabled = ( charImage[charY] >> ( DISPLAY_HEIGHT - 1 - charX ) ) & 1;
          if( isDisplayAnimationInProgress && charImagePrevious.size() > 0 ) {
            uint8_t animationSteps = charImage.size();
            if( animationTypeNumber == 0 || animationTypeNumber == 1 ) {
              animationSteps++;
            }
            unsigned long animationStepSize = displayAnimationLengthMillis / animationSteps;
            int currentAnimationStep = calculateDiffMillis( displayAnimationStartedMillis, currentMillis ) / animationStepSize;
            if( currentAnimationStep >= animationSteps ) {
              currentAnimationStep = animationSteps - 1;
            }
            if( animationTypeNumber == 0 ) {
              isPointEnabled = currentAnimationStep == charY
                               ? 0
                               : ( currentAnimationStep < charY
                                 ? ( ( charImagePrevious[charY - currentAnimationStep - 1] >> ( DISPLAY_HEIGHT - 1 - charX ) ) & 1 )
                                 : ( ( charImage[charY + charImage.size() - currentAnimationStep] >> ( DISPLAY_HEIGHT - 1 - charX ) ) & 1 ) );
            } else if( animationTypeNumber == 1 ) {
              isPointEnabled = currentAnimationStep == charY
                               ? 0
                               : ( currentAnimationStep < charY
                                   ? ( ( charImagePrevious[charY] >> ( DISPLAY_HEIGHT - 1 - charX ) ) & 1 )
                                   : ( ( charImage[charY] >> ( DISPLAY_HEIGHT - 1 - charX ) ) & 1 ) );

            } else if( animationTypeNumber == 2 ) {
              isPointEnabled = currentAnimationStep < charY
                                ? ( ( charImagePrevious[charY] >> ( DISPLAY_HEIGHT - 1 - charX ) ) & 1 )
                                : ( ( charImage[charY] >> ( DISPLAY_HEIGHT - 1 - charX ) ) & 1 );
            }
          }
        }
        display.setPoint(
          isRotateDisplay ? ( DISPLAY_HEIGHT - 1 - displayY ) : ( displayY ),
          isRotateDisplay ? ( displayWidthUsed + charX ) : ( DISPLAY_WIDTH - 1 - ( displayWidthUsed + charX ) ),
          isPointEnabled
        );
      }
    }
    displayWidthUsed += charWidth;
  }

  for( size_t charToDisplayIndex = 0; charToDisplayIndex < textToDisplaySmall.length(); ++charToDisplayIndex ) {
    char charToDisplay = textToDisplaySmall.charAt( charToDisplayIndex );
    uint8_t charWidth = TCFonts::getSymbolWidth( displayFontTypeNumber, charToDisplay, isDisplayBoldFontUsed, !isSecondsShown, true );
    if( displayWidthUsed + charWidth > DISPLAY_WIDTH ) {
      charWidth = DISPLAY_WIDTH - displayWidthUsed;
    }
    if( charWidth == 0 ) continue;
    std::vector<uint8_t> charImage = TCFonts::getSymbol( font, charToDisplay, isDisplayBoldFontUsed, !isSecondsShown, true );

    for( uint8_t displayY = 0; displayY < DISPLAY_HEIGHT; ++displayY ) {
      for( uint8_t charX = 0; charX < charWidth; ++charX ) {
        bool isPointEnabled = false;
        uint8_t charShiftY = DISPLAY_HEIGHT - charImage.size();
        if( displayY >= charShiftY ) {
          uint8_t charY = displayY - charShiftY;
          isPointEnabled = ( charImage[charY] >> ( DISPLAY_HEIGHT - 1 - charX ) ) & 1;
        }
        display.setPoint(
          isRotateDisplay ? ( DISPLAY_HEIGHT - 1 - displayY ) : ( displayY ),
          isRotateDisplay ? ( displayWidthUsed + charX ) : ( DISPLAY_WIDTH - 1 - ( displayWidthUsed + charX ) ),
          isPointEnabled
        );
      }
    }
    displayWidthUsed += charWidth;
  }

  if( displayWidthUsed < DISPLAY_WIDTH ) { //fill the rest of the screen
    for( uint8_t charY = 0; charY < DISPLAY_HEIGHT; ++charY ) {
      for( uint8_t charX = 0; charX < DISPLAY_WIDTH - displayWidthUsed; ++charX ) {
        display.setPoint(
          isRotateDisplay ? ( DISPLAY_HEIGHT - 1 - charY ) : ( charY ),
          isRotateDisplay ? ( displayWidthUsed + charX ) : ( DISPLAY_WIDTH - 1 - ( displayWidthUsed + charX ) ),
          false
        );
      }
    }
  }
}

std::vector<std::vector<bool>> getDisplayPreview( uint8_t fontNumber, bool isBold, bool isSecondsShown, String textToDisplayLarge, String textToDisplaySmall ) {
  std::vector<std::vector<bool>> preview( DISPLAY_HEIGHT, std::vector<bool>( DISPLAY_WIDTH, false ) );

  uint8_t displayWidthUsed = 0;
  std::map<String, std::vector<uint8_t>> font = TCFonts::getFont( fontNumber );

  for( size_t charToDisplayIndex = 0; charToDisplayIndex < textToDisplayLarge.length(); ++charToDisplayIndex ) {
    char charToDisplay = textToDisplayLarge.charAt( charToDisplayIndex );
    uint8_t charWidth = TCFonts::getSymbolWidth( fontNumber, charToDisplay, isBold, !isSecondsShown, false );
    if( displayWidthUsed + charWidth > DISPLAY_WIDTH ) {
      charWidth = DISPLAY_WIDTH - displayWidthUsed;
    }
    if( charWidth == 0 ) continue;

    std::vector<uint8_t> charImage = TCFonts::getSymbol( font, charToDisplay, isBold, !isSecondsShown, false );
    for( uint8_t displayY = 0; displayY < DISPLAY_HEIGHT; ++displayY ) {
      for( uint8_t charX = 0; charX < charWidth; ++charX ) {
        bool isPointEnabled = false;
        uint8_t charShiftY = DISPLAY_HEIGHT - charImage.size();
        if( displayY >= charShiftY ) {
          uint8_t charY = displayY - charShiftY;
          isPointEnabled = ( charImage[charY] >> ( DISPLAY_HEIGHT - 1 - charX ) ) & 1;
        }
        preview[displayY][displayWidthUsed+charX] = isPointEnabled;
      }
    }
    displayWidthUsed += charWidth;
  }

  for( size_t charToDisplayIndex = 0; charToDisplayIndex < textToDisplaySmall.length(); ++charToDisplayIndex ) {
    char charToDisplay = textToDisplaySmall.charAt( charToDisplayIndex );
    uint8_t charWidth = TCFonts::getSymbolWidth( fontNumber, charToDisplay, isBold, !isSecondsShown, true );
    if( displayWidthUsed + charWidth > DISPLAY_WIDTH ) {
      charWidth = DISPLAY_WIDTH - displayWidthUsed;
    }
    if( charWidth == 0 ) continue;

    std::vector<uint8_t> charImage = TCFonts::getSymbol( font, charToDisplay, isBold, !isSecondsShown, true );
    for( uint8_t displayY = 0; displayY < DISPLAY_HEIGHT; ++displayY ) {
      for( uint8_t charX = 0; charX < charWidth; ++charX ) {
        bool isPointEnabled = false;
        uint8_t charShiftY = DISPLAY_HEIGHT - charImage.size();
        if( displayY >= charShiftY ) {
          uint8_t charY = displayY - charShiftY;
          isPointEnabled = ( charImage[charY] >> ( DISPLAY_HEIGHT - 1 - charX ) ) & 1;
        }
        preview[displayY][displayWidthUsed + charX] = isPointEnabled;
      }
    }
    displayWidthUsed += charWidth;
  }

  return preview;
}

void renderDisplay() {
  if( timeClient.isTimeSet() || isCustomDateTimeSet ) {
    time_t dt = 0;
    if( timeClient.isTimeSet() ) {
      dt = timeClient.getEpochTime();
    } else if( isCustomDateTimeSet ) {
      unsigned long currentMillis = millis();
      if( currentMillis >= customDateTimeReceivedAt && customDateTimePrevMillis < customDateTimeReceivedAt ) {
        unsigned long wrappedSeconds = ULONG_MAX / 1000 + 1;
        customDateTimeReceivedAt = customDateTimeReceivedAt - ( 1000 - ULONG_MAX % 1000 );
        customDateTime = customDateTime + wrappedSeconds;
      }
      customDateTimePrevMillis = currentMillis;
      unsigned long timeDiff = calculateDiffMillis( customDateTimeReceivedAt, currentMillis );
      dt = customDateTime + timeDiff / 1000;
    }

    struct tm* dtStruct = localtime(&dt);
    uint8_t hour = dtStruct->tm_hour;
    hour += isWithinDstBoundaries( dt ) ? 3 : 2;
    if( hour >= 24 ) {
      hour = hour - 24;
    }
    String hourStr = ( hour < 10 ? ( isSingleDigitHourShown ? " " : "0" ) : "" ) + String( hour );
    uint8_t minute = dtStruct->tm_min;
    String minuteStr = ( minute < 10 ? "0" : "" ) + String( minute );
    uint8_t second = dtStruct->tm_sec;
    String secondStr = ( second < 10 ? "0" : "" ) + String( second );

    String textToDisplayLarge = hourStr + ( isSemicolonShown ? ":" : "\t" ) + minuteStr;
    String textToDisplaySmall = isDisplaySecondsShown ? secondStr : String( "" );
    renderDisplayText( textToDisplayLarge, textToDisplaySmall, isDisplaySecondsShown, isClockAnimated );
  } else {
    String textToDisplayLarge = String( "  " ) + ( isSemicolonShown ? ":" : "\t" ) + String( "  " );
    String textToDisplaySmall = isDisplaySecondsShown ? String( "  " ) : String( "" );
    renderDisplayText( textToDisplayLarge, textToDisplaySmall, isDisplaySecondsShown, false );
  }
  display.update();
}

unsigned long timeClientSecondsAtDisplaySyncStart = 0;
void forceDisplaySync() {
  if( timeClient.isTimeSet() ) {
    timeClientSecondsAtDisplaySyncStart = timeClient.getEpochTime();
  }
  isForceDisplaySync = true;
  isForceDisplaySyncDisplayRenderOverride = true;
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
    case WL_WRONG_PASSWORD:
      return F("WRONG_PASSWORD");
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
      if( wifiStatus == WL_DISCONNECTED || wifiStatus == WL_IDLE_STATUS ) break;
      delay( 500 );
      Serial.print( "." );
    }
    Serial.println( F(" done") );
    setInternalLedStatus( previousInternalLedStatus );
  }
}

bool isRouterSsidProvided() {
  return strlen(wiFiClientSsid) != 0;
}

void connectToWiFiAsync( bool isInit ) {
  if( !isRouterSsidProvided() ) {
    createAccessPoint();
    return;
  }

  Serial.print( String( F("Connecting to WiFi '") ) + String( wiFiClientSsid ) + "'..." );
  WiFi.hostname( ( String( getWiFiHostName() ) + "-" + String( ESP.getChipId() ) ).c_str() );
  WiFi.begin( wiFiClientSsid, wiFiClientPassword );

  if( WiFi.isConnected() ) {
    shutdownAccessPoint();
    return;
  } else {
    Serial.println();
  }

  wl_status_t wifiStatus = WiFi.status();
  if( WiFi.isConnected() ) {
    Serial.println( String( F(" done. WiFi status: ") ) + getWiFiStatusText( wifiStatus ) );
    shutdownAccessPoint();
    forceRefreshData();
  } else if( !isInit && ( wifiStatus == WL_NO_SSID_AVAIL || wifiStatus == WL_CONNECT_FAILED || wifiStatus == WL_CONNECTION_LOST || wifiStatus == WL_IDLE_STATUS || wifiStatus == WL_DISCONNECTED ) ) {
    Serial.println( String( F("ERROR. WiFi status: ") ) + getWiFiStatusText( wifiStatus ) );
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
  WiFi.hostname( ( String( getWiFiHostName() ) + "-" + String( ESP.getChipId() ) ).c_str() );
  WiFi.begin( wiFiClientSsid, wiFiClientPassword );

  if( WiFi.isConnected() ) {
    shutdownAccessPoint();
    return;
  }

  unsigned long wiFiConnectStartedMillis = millis();
  uint8_t previousInternalLedStatus = getInternalLedStatus();
  while( true ) {
    setInternalLedStatus( HIGH );
    delay( 1000 );
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
      ":root{--f:22px;}"
      "body{margin:0;background-color:#444;font-family:sans-serif;color:#FFF;}"
      "body,input,button{font-size:var(--f);}"
      ".wrp{width:60%;min-width:460px;max-width:600px;margin:auto;margin-bottom:10px;}"
      "h2{color:#FFF;font-size:calc(var(--f)*1.2);text-align:center;margin-top:0.3em;margin-bottom:0.1em;}"
      ".fx{display:flex;flex-wrap:wrap;margin:auto;}"
      ".fx:not(:first-of-type){margin-top:0.3em;}"
      ".fx .fi{display:flex;align-items:center;margin-top:0.3em;width:100%;}"
      ".fx .fi:first-of-type,.fx.fv .fi{margin-top:0;}"
      ".fv{flex-direction:column;align-items:flex-start;}"
      ".ex.ext:before{color:#888;cursor:pointer;content:\"▶\";}"
      ".ex.exon .ex.ext:before{content:\"▼\";}"
      ".ex.exc{height:0;margin-top:0;}.ex.exc>*{visibility:hidden;}"
      ".ex.exc.exon{height:inherit;}.ex.exc.exon>*{visibility:initial;}"
      "label{flex:none;padding-right:0.6em;max-width:50%;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;}"
      "input{width:100%;padding:0.1em 0.2em;}"
      "input[type=\"radio\"],input[type=\"checkbox\"]{flex:none;margin:0.1em 0;width:calc(var(--f)*1.2);height:calc(var(--f)*1.2);}"
      "input[type=\"radio\"]+label,input[type=\"checkbox\"]+label{padding-left:0.6em;padding-right:initial;flex:1 1 auto;max-width:initial;}"
      "input[type=\"range\"]{-webkit-appearance:none;background:transparent;padding:0;}"
      "input[type=\"range\"]::-webkit-slider-runnable-track{appearance:none;height:calc(0.4*var(--f));border:2px solid #EEE;border-radius:4px;background:#666;}"
      "input[type=\"range\"]::-webkit-slider-thumb{appearance:none;background:#FFF;border-radius:50%;margin-top:calc(-0.4*var(--f));height:calc(var(--f));width:calc(var(--f));}"
      "input[type=\"color\"]{padding:0;height:var(--f);}"
      "input[type=\"color\"]::-webkit-color-swatch-wrapper{padding:2px;}"
      "output{padding-left:0.6em;}"
      "button{width:100%;padding:0.2em;}"
      "a{color:#AAA;}"
      ".sub{text-wrap:nowrap;}"
      ".sub:not(:last-of-type){padding-right:0.6em;}"
      ".ft{margin-top:1em;}"
      ".pl{padding-left:0.6em;}"
      ".pll{padding-left:calc(var(--f)*1.2 + 0.6em);}"
      ".lnk{margin:auto;color:#AAA;}"
      ".i{color:#CCC;margin-left:0.2em;border:1px solid #777;border-radius:50%;background-color:#666;cursor:default;font-size:65%;vertical-align:top;width:1em;height:1em;display:inline-block;text-align:center;}"
      ".i:before{content:\"i\";position:relative;top:-0.07em;}"
      ".i:hover{background-color:#777;color:#DDD;}"
      ".stat{opacity:0.3;font-size:65%;border:1px solid #DDD;border-radius:4px;overflow:hidden;}"
      ".stat>span{padding:1px 4px;display:inline-block;}"
      ".stat>span:first-of-type{background-color:#666;}"
      ".stat>span:not(:last-of-type){border-right:1px solid #DDD;}"
      "#exw{width:100%;display:flex;background-image:linear-gradient(-180deg,#777,#222 7% 93%,#000);}"
      "#exdw{width:calc(88% - 6px);border:2px solid #1A1A1A;padding:1px;margin:3% 6%;}"
      "#exdw .exdl{display:flex;flex-wrap:nowrap;}"
      "#exdw .exdp{width:calc(100%/32);}"
      "#exdw .exdp.exdp1{background:radial-gradient(RGBA(64,192,0,1) 55%,RGBA(34,34,34,0) 65%);}"
      "#exdw .exdp.exdp0{background:radial-gradient(RGBA(44,44,44,1) 55%,RGBA(34,34,34,0) 65%);}"
      "#exdw:before,#exdw .exdp:before{content:'';float:left;padding-top:100%;}"
      "#exdw:before{padding-top:25%;}"
      "#exdw:after,#exdw .exdp:after{content:'';display:block;clear:both;}"
      "@media(max-device-width:800px) and (orientation:portrait){"
        ":root{--f:4vw;}"
        ".wrp{width:94%;max-width:100%;}"
      "}"
    "</style>"
  "</head>"
  "<body>"
    "<div class=\"wrp\">"
      "<h2>ГОДИННИК<div class=\"lnk\" style=\"font-size:50%;\">Розробник: <a href=\"mailto:kurylo.press@gmail.com?subject=Clock\">Дмитро Курило</a></div></h2>";
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

String getHtmlInput( String label, const char* type, const char* value, const char* elId, const char* elName, uint8_t minLength, uint8_t maxLength, bool isRequired, bool isChecked, const char* additional ) {
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
      ( (strcmp(type, HTML_INPUT_RANGE) == 0) ? String( F(" min=\"") ) + String( minLength ) + String( F("\" max=\"") ) + String(maxLength) + String( F("\" oninput=\"this.nextElementSibling.value=this.value;\"><output>") ) + String( value ) + String( F("</output") ) : "" ) +
    ">" +
      ( (strcmp(type, HTML_INPUT_TEXT) != 0 && strcmp(type, HTML_INPUT_PASSWORD) != 0 && strcmp(type, HTML_INPUT_COLOR) != 0 && strcmp(type, HTML_INPUT_RANGE) != 0) ? getHtmlLabel( label, elId, false ) : "" );
}

const uint8_t getWiFiClientSsidNameMaxLength() { return WIFI_SSID_MAX_LENGTH - 1;}
const uint8_t getWiFiClientSsidPasswordMaxLength() { return WIFI_PASSWORD_MAX_LENGTH - 1;}

const char* HTML_PAGE_ROOT_ENDPOINT = "/";
const char* HTML_PAGE_PREVIEW_ENDPOINT = "/preview";
const char* HTML_PAGE_DATA_ENDPOINT = "/data";
const char* HTML_PAGE_SET_DATE_ENDPOINT = "/setdt";
const char* HTML_PAGE_REBOOT_ENDPOINT = "/reboot";
const char* HTML_PAGE_TESTLED_ENDPOINT = "/testled";
const char* HTML_PAGE_TEST_NIGHT_ENDPOINT = "/testdim";
const char* HTML_PAGE_UPDATE_ENDPOINT = "/update";
const char* HTML_PAGE_PING_ENDPOINT = "/ping";
const char* HTML_PAGE_FAVICON_ENDPOINT = "/favicon.ico";

const char* HTML_PAGE_WIFI_SSID_NAME = "ssid";
const char* HTML_PAGE_WIFI_PWD_NAME = "pwd";

const char* HTML_PAGE_SHOW_SINGLE_DIGIT_HOUR_NAME = "sdh";
const char* HTML_PAGE_CLOCK_ANIMATED_NAME = "ca";

const char* HTML_PAGE_FONT_TYPE_NAME = "fnt";
const char* HTML_PAGE_BOLD_FONT_NAME = "bld";
const char* HTML_PAGE_SHOW_SECS_NAME = "sec";
const char* HTML_PAGE_SLOW_SEMICOLON_ANIMATION_NAME = "ssa";
const char* HTML_PAGE_ROTATE_DISPLAY_NAME = "rt";
const char* HTML_PAGE_ANIMATION_TYPE_NAME = "at";
const char* HTML_PAGE_BRIGHTNESS_DAY_NAME = "brtd";
const char* HTML_PAGE_BRIGHTNESS_NIGHT_NAME = "brtn";

void handleWebServerGet() {
  String content;
  addHtmlPageStart( content );
  content += String( F("<script>function ex(el){Array.from(el.parentElement.parentElement.children).forEach(ch=>{if(ch.classList.contains(\"ex\"))ch.classList.toggle(\"exon\");});}</script>"
  "<form method=\"POST\">"
  "<div class=\"fx\">"
    "<h2>Приєднатись до WiFi:</h2>"
    "<div class=\"fi pl\">") ) + getHtmlInput( F("SSID назва"), HTML_INPUT_TEXT, wiFiClientSsid, HTML_PAGE_WIFI_SSID_NAME, HTML_PAGE_WIFI_SSID_NAME, 0, getWiFiClientSsidNameMaxLength(), true, false, "" ) + String( F("</div>"
    "<div class=\"fi pl\">") ) + getHtmlInput( F("SSID пароль"), HTML_INPUT_PASSWORD, wiFiClientPassword, HTML_PAGE_WIFI_PWD_NAME, HTML_PAGE_WIFI_PWD_NAME, 0, getWiFiClientSsidPasswordMaxLength(), true, false, "" ) + String( F("</div>"
  "</div>"
  "<div class=\"fx\">"
    "<h2>Налаштування дисплея:</h2>"
    "<div class=\"fi pl\"><div id=\"exw\"><div id=\"exdw\"></div></div></div>"
    "<div class=\"fi pl\">") ) + getHtmlInput( F("Вид шрифта"), HTML_INPUT_RANGE, String(displayFontTypeNumber).c_str(), HTML_PAGE_FONT_TYPE_NAME, HTML_PAGE_FONT_TYPE_NAME, 0, 2, false, displayFontTypeNumber, "onchange=\"pw();\"" ) + String( F("</div>"
    "<div class=\"fi pl\">") ) + getHtmlInput( F("Жирний шрифт"), HTML_INPUT_CHECKBOX, "", HTML_PAGE_BOLD_FONT_NAME, HTML_PAGE_BOLD_FONT_NAME, 0, 0, false, isDisplayBoldFontUsed, "onchange=\"pw();\"" ) + String( F("</div>"
    "<div class=\"fi pl\">") ) + getHtmlInput( F("Показувати секунди"), HTML_INPUT_CHECKBOX, "", HTML_PAGE_SHOW_SECS_NAME, HTML_PAGE_SHOW_SECS_NAME, 0, 0, false, isDisplaySecondsShown, "onchange=\"pw();\"" ) + String( F("</div>"
    "<div class=\"fi pl\">") ) + getHtmlInput( F("Показувати час без переднього нуля"), HTML_INPUT_CHECKBOX, "", HTML_PAGE_SHOW_SINGLE_DIGIT_HOUR_NAME, HTML_PAGE_SHOW_SINGLE_DIGIT_HOUR_NAME, 0, 0, false, isSingleDigitHourShown, "" ) + String( F("</div>"
    "<div class=\"fi pl\">") ) + getHtmlInput( F("Повільні двокрапки (30 разів в хв)"), HTML_INPUT_CHECKBOX, "", HTML_PAGE_SLOW_SEMICOLON_ANIMATION_NAME, HTML_PAGE_SLOW_SEMICOLON_ANIMATION_NAME, 0, 0, false, isSlowSemicolonAnimation, "" ) + String( F("</div>"
    "<div class=\"fi pl\">") ) + getHtmlInput( F("Розвернути зображення на 180°"), HTML_INPUT_CHECKBOX, "", HTML_PAGE_ROTATE_DISPLAY_NAME, HTML_PAGE_ROTATE_DISPLAY_NAME, 0, 0, false, isRotateDisplay, "onchange=\"pw();\"" ) + String( F("</div>"
    "<div class=\"fi pl\">") ) + getHtmlInput( F("Анімований годинник"), HTML_INPUT_CHECKBOX, "", HTML_PAGE_CLOCK_ANIMATED_NAME, HTML_PAGE_CLOCK_ANIMATED_NAME, 0, 0, false, isClockAnimated, "" ) + String( F("</div>"
    "<div class=\"fi pl\">") ) + getHtmlInput( F("Вид анімації"), HTML_INPUT_RANGE, String(animationTypeNumber).c_str(), HTML_PAGE_ANIMATION_TYPE_NAME, HTML_PAGE_ANIMATION_TYPE_NAME, 0, 2, false, animationTypeNumber, "" ) + String( F("</div>"
    "<div class=\"fi pl\">") ) + getHtmlInput( F("Яскравість вдень"), HTML_INPUT_RANGE, String(displayDayModeBrightness).c_str(), HTML_PAGE_BRIGHTNESS_DAY_NAME, HTML_PAGE_BRIGHTNESS_DAY_NAME, 0, 15, false, displayDayModeBrightness, "" ) + String( F("</div>"
    "<div class=\"fi pl\">") ) + getHtmlInput( F("Яскравість вночі"), HTML_INPUT_RANGE, String(displayNightModeBrightness).c_str(), HTML_PAGE_BRIGHTNESS_NIGHT_NAME, HTML_PAGE_BRIGHTNESS_NIGHT_NAME, 0, 15, false, displayNightModeBrightness, "" ) + String( F("</div>"
  "</div>"
  "<div class=\"fx ft\">"
    "<div class=\"fi\"><button type=\"submit\">Застосувати</button></div>"
  "</div>"
"</form>"
"<div class=\"ft\">"
  "<div class=\"fx\">"
    "<span>"
      "<span class=\"sub\">") ) + getHtmlLink( HTML_PAGE_TEST_NIGHT_ENDPOINT, F("Перевірити нічний режим") ) + String( F("<span class=\"i\" title=\"Застосуйте налаштування перед перевіркою!\"></span></span>"
      "<span class=\"sub\">") ) + getHtmlLink( HTML_PAGE_TESTLED_ENDPOINT, F("Перевірити матрицю") ) + String( F("</span>"
    "</span>"
  "</div>"
  "<div class=\"fx\">"
    "<span>"
      "<span class=\"sub\">") ) + getHtmlLink( HTML_PAGE_UPDATE_ENDPOINT, F("Оновити прошивку") ) + String( F("<span class=\"i\" title=\"Поточна версія: ") ) + String( getFirmwareVersion() ) + String( F("\"></span></span>"
      "<span class=\"sub\">") ) + getHtmlLink( HTML_PAGE_REBOOT_ENDPOINT, F("Перезавантажити") ) + String( F("</span>"
    "</span>"
  "</div>"
  "<div class=\"fx\" style=\"padding-top:0.3em;\">"
    "<span>"
      "<div class=\"stat\"><span>Яскравість</span><span>CUR ") ) + String( analogRead(A0) ) + String( F("</span><span>AVG ") ) + String( sensorBrightnessAverage ) + String( F("</span><span>REQ ") ) + String( displayCurrentBrightness ) + String( F("</span><span>DSP ") ) + String( static_cast<uint8_t>( round( displayPreviousBrightness ) ) ) + String( F("</span></div>"
    "</span>"
  "</div>"
"</div>"
"<script>"
  "function dt(){"
    "let ts=") ) + String( timeClient.isTimeSet() || ( isCustomDateTimeSet && calculateDiffMillis( customDateTimeReceivedAt, millis() ) <= DELAY_NTP_TIME_SYNC ) ) + String( F(";"
    "if(ts)return;"
    "fetch('/setdt?t='+Date.now().toString()).catch(e=>{"
    "});"
  "}"
  "function pw(){"
    "fetch('/preview?f='+document.querySelector('#fnt').value+'&b='+(document.querySelector('#bld').checked?'1':'0')+'&s='+(document.querySelector('#sec').checked?'1':'0')).then(res=>{"
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
  "function pg(){"
    "let ap=") ) + String( isApInitialized ) + String( F(";"
    "if(!ap)return;"
    "setInterval(()=>{"
      "fetch('/ping').catch(e=>{"
      "});"
    "},60000);"
  "}"
  "document.addEventListener(\"DOMContentLoaded\",()=>{"
    "dt();"
    "pw();"
    "pg();"
  "});"
"</script>") );
  addHtmlPageEnd( content );

  wifiWebServer.sendHeader( String( F("Content-Length") ).c_str(), String( content.length() ) );
  wifiWebServer.send( 200, getContentType( F("html") ), content );

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
"<script>document.addEventListener(\"DOMContentLoaded\",()=>{setTimeout(()=>{window.location.href=\"/\";},";
const char HTML_PAGE_FILLUP_END[] PROGMEM = "000);});</script>";

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

void handleWebServerPost() {
  String content;

  String htmlPageSsidNameReceived = wifiWebServer.arg( HTML_PAGE_WIFI_SSID_NAME );
  String htmlPageSsidPasswordReceived = wifiWebServer.arg( HTML_PAGE_WIFI_PWD_NAME );

  if( htmlPageSsidNameReceived.length() == 0 ) {
    addHtmlPageStart( content );
    content += String( F("<h2>Error: Missing SSID Name</h2>") );
    addHtmlPageEnd( content );
    wifiWebServer.sendHeader( String( F("Content-Length") ).c_str(), String( content.length() ) );
    wifiWebServer.send( 400, getContentType( F("html") ), content );
    return;
  }
  if( htmlPageSsidPasswordReceived.length() == 0 ) {
    addHtmlPageStart( content );
    content += String( F("<h2>Error: Missing SSID Password</h2>") );
    addHtmlPageEnd( content );
    wifiWebServer.sendHeader( String( F("Content-Length") ).c_str(), String( content.length() ) );
    wifiWebServer.send( 400, getContentType( F("html") ), content );
    return;
  }
  if( htmlPageSsidNameReceived.length() > getWiFiClientSsidNameMaxLength() ) {
    addHtmlPageStart( content );
    content += String( F("<h2>Error: SSID Name exceeds maximum length of ") ) + String( getWiFiClientSsidNameMaxLength() ) + String( F("</h2>") );
    addHtmlPageEnd( content );
    wifiWebServer.sendHeader( String( F("Content-Length") ).c_str(), String( content.length() ) );
    wifiWebServer.send( 400, getContentType( F("html") ), content );
    return;
  }
  if( htmlPageSsidPasswordReceived.length() > getWiFiClientSsidPasswordMaxLength() ) {
    addHtmlPageStart( content );
    content += String( F("<h2>Error: SSID Password exceeds maximum length of ") ) + String( getWiFiClientSsidPasswordMaxLength() ) + String( F("</h2>") );
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
  if( displayFontTypeNumberReceived >= 0 && displayFontTypeNumberReceived <= 2 ) {
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
  if( animationTypeNumberReceived >= 0 && animationTypeNumberReceived <= 2 ) {
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

  String htmlPageDisplayDayModeBrightnessReceived = wifiWebServer.arg( HTML_PAGE_BRIGHTNESS_DAY_NAME );
  uint displayDayModeBrightnessReceived = htmlPageDisplayDayModeBrightnessReceived.toInt();
  bool displayDayModeBrightnessReceivedPopulated = false;
  if( displayDayModeBrightnessReceived > 0 || displayDayModeBrightnessReceived <= 15 ) {
    displayDayModeBrightnessReceivedPopulated = true;
  }

  String htmlPageDisplayNightModeBrightnessReceived = wifiWebServer.arg( HTML_PAGE_BRIGHTNESS_NIGHT_NAME );
  uint displayNightModeBrightnessReceived = htmlPageDisplayNightModeBrightnessReceived.toInt();
  bool displayNightModeBrightnessReceivedPopulated = false;
  if( displayNightModeBrightnessReceived > 0 || displayNightModeBrightnessReceived <= 15 ) {
    displayNightModeBrightnessReceivedPopulated = true;
    if( displayNightModeBrightnessReceived > displayDayModeBrightnessReceived ) {
      displayNightModeBrightnessReceived = displayDayModeBrightnessReceived;
    }
  }

  bool isWiFiChanged = strcmp( wiFiClientSsid, htmlPageSsidNameReceived.c_str() ) != 0 || strcmp( wiFiClientPassword, htmlPageSsidPasswordReceived.c_str() ) != 0;

  String waitTime = isWiFiChanged ? String(TIMEOUT_CONNECT_WIFI_SYNC/1000 + 6) : "2";
  addHtmlPageStart( content );
  content += getHtmlPageFillup( waitTime, waitTime ) + String( F("<h2>Зберігаю...</h2>") );
  addHtmlPageEnd( content );
  wifiWebServer.sendHeader( String( F("Content-Length") ).c_str(), String( content.length() ) );
  wifiWebServer.send( 200, getContentType( F("html") ), content );

  bool isDisplayIntensityUpdateRequired = false;
  bool isDisplayRerenderRequired = false;
  bool isFontUpdateRequired = false;

  if( isSingleDigitHourShownReceivedPopulated && isSingleDigitHourShownReceived != isSingleDigitHourShown ) {
    isSingleDigitHourShown = isSingleDigitHourShownReceived;
    isDisplayRerenderRequired = true;
    Serial.println( F("Show single digit hour updated") );
    writeEepromBoolValue( eepromIsSingleDigitHourShownIndex, isSingleDigitHourShownReceived );
  }

  if( isClockAnimatedReceivedPopulated && isClockAnimatedReceived != isClockAnimated ) {
    isClockAnimated = isClockAnimatedReceived;
    Serial.println( F("Clock animated updated") );
    writeEepromBoolValue( eepromIsClockAnimatedIndex, isClockAnimatedReceived );
  }


  if( displayFontTypeNumberReceivedPopulated && displayFontTypeNumberReceived != displayFontTypeNumber ) {
    displayFontTypeNumber = displayFontTypeNumberReceived;
    isDisplayRerenderRequired = true;
    isFontUpdateRequired = true;
    Serial.println( F("Display font updated") );
    writeEepromIntValue( eepromDisplayFontTypeNumberIndex, displayFontTypeNumberReceived );
  }

  if( isDisplayBoldFondUsedReceivedPopulated && isDisplayBoldFondUsedReceived != isDisplayBoldFontUsed ) {
    isDisplayBoldFontUsed = isDisplayBoldFondUsedReceived;
    isDisplayRerenderRequired = true;
    Serial.println( F("Show seconds updated") );
    writeEepromBoolValue( eepromIsFontBoldUsedIndex, isDisplayBoldFondUsedReceived );
  }

  if( isDisplaySecondsShownReceivedPopulated && isDisplaySecondsShownReceived != isDisplaySecondsShown ) {
    isDisplaySecondsShown = isDisplaySecondsShownReceived;
    isDisplayRerenderRequired = true;
    Serial.println( F("Show seconds updated") );
    writeEepromBoolValue( eepromIsDisplaySecondsShownIndex, isDisplaySecondsShownReceived );
  }

  if( isRotateDisplayReceivedPopulated && isRotateDisplayReceived != isRotateDisplay ) {
    isRotateDisplay = isRotateDisplayReceived;
    isDisplayRerenderRequired = true;
    Serial.println( F("Display rotation updated") );
    writeEepromBoolValue( eepromIsRotateDisplayIndex, isRotateDisplayReceived );
  }

  if( animationTypeNumberReceivedPopulated && animationTypeNumberReceived != animationTypeNumber ) {
    animationTypeNumber = animationTypeNumberReceived;
    Serial.println( F("Display animation updated") );
    writeEepromIntValue( eepromAnimationTypeNumberIndex, animationTypeNumberReceived );
  }

  if( isSlowSemicolonAnimationReceivedPopulated && isSlowSemicolonAnimationReceived != isSlowSemicolonAnimation ) {
    isSlowSemicolonAnimation = isSlowSemicolonAnimationReceived;
    isDisplayRerenderRequired = true;
    Serial.println( F("Display semicolon speed updated") );
    writeEepromBoolValue( eepromIsSlowSemicolonAnimationIndex, isSlowSemicolonAnimationReceived );
  }

  if( displayDayModeBrightnessReceivedPopulated && displayDayModeBrightnessReceived != displayDayModeBrightness ) {
    displayDayModeBrightness = displayDayModeBrightnessReceived;
    isDisplayIntensityUpdateRequired = true;
    isDisplayRerenderRequired = true;
    Serial.println( F("Display brightness updated") );
    writeEepromIntValue( eepromDisplayDayModeBrightnessIndex, displayDayModeBrightnessReceived );
  }

  if( displayNightModeBrightnessReceivedPopulated && displayNightModeBrightnessReceived != displayNightModeBrightness ) {
    displayNightModeBrightness = displayNightModeBrightnessReceived;
    isDisplayIntensityUpdateRequired = true;
    isDisplayRerenderRequired = true;
    Serial.println( F("Display night mode brightness updated") );
    writeEepromIntValue( eepromDisplayNightModeBrightnessIndex, displayNightModeBrightnessReceived );
  }

  if( isFontUpdateRequired ) {
    TCFonts::setFont( displayFontTypeNumber );
  }
  if( isDisplayIntensityUpdateRequired ) {
    setDisplayBrightness( true );
  }
  if( isDisplayRerenderRequired ) {
    renderDisplay();
  }

  if( isWiFiChanged ) {
    Serial.println( F("WiFi settings updated") );
    strncpy( wiFiClientSsid, htmlPageSsidNameReceived.c_str(), sizeof(wiFiClientSsid) );
    strncpy( wiFiClientPassword, htmlPageSsidPasswordReceived.c_str(), sizeof(wiFiClientPassword) );
    writeEepromCharArray( eepromWiFiSsidIndex, wiFiClientSsid, WIFI_SSID_MAX_LENGTH );
    writeEepromCharArray( eepromWiFiPasswordIndex, wiFiClientPassword, WIFI_PASSWORD_MAX_LENGTH );
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
  if( !fontNumberStr.length() ) fontNumberStr = "0";
  uint8_t fontNumber = static_cast<uint8_t>( atoi( fontNumberStr.c_str() ) );
  String isBoldStr = wifiWebServer.arg("b");
  bool isBold = isBoldStr == String( F("1") ) || isBoldStr == String( F("true") ) || isBoldStr == String( F("TRUE") ) || isBoldStr == String( F("True") );
  String isSecondsShownStr = wifiWebServer.arg("s");
  bool isSecondsShown = isSecondsShownStr == String( F("1") ) || isSecondsShownStr == String( F("true") ) || isSecondsShownStr == String( F("TRUE") ) || isSecondsShownStr == String( F("True") );

  std::vector<std::vector<bool>> preview = getDisplayPreview( fontNumber, isBold, isSecondsShown, "21:46", "37" );
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
      wifiWebServer.streamFile( file, getContentType( fileExtension ) );
      file.close();
    }

    if( isApInitialized ) { //this resets AP timeout when user loads the page in AP mode
      apStartedMillis = millis();
    }
    return;
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
    customDateTimeReceivedAt = currentMillis;
    customDateTimePrevMillis = currentMillis;
    customDateTime = dt / 1000;
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
  setDisplayBrightness( displayNightModeBrightness );
  renderDisplay();
  delay( 6000 );
  setDisplayBrightness( true );
  renderDisplay();

  if( isApInitialized ) { //this resets AP timeout when user loads the page in AP mode
    apStartedMillis = millis();
  }
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

const uint8_t FAVICON_ICO_GZ[] PROGMEM = {
  0x1F, 0x8B, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x63, 0x60, 0x60, 0x04, 0x42, 0x01, 0x01, 0x26, 0x30, 0xBD, 0x81, 0x81, 0x81, 0x41, 0x0C, 0x88, 0x35, 0x80, 0x58, 0x00, 0x88, 0x15,
  0x80, 0x18, 0x24, 0x0E, 0x02, 0x0E, 0x0C, 0x08, 0xC0, 0x04, 0xC5, 0xD6, 0xD6, 0xD6, 0x0C, 0x15, 0x7F, 0x36, 0xC3, 0xC5, 0x0F, 0xF0, 0x43, 0xB0, 0xB1, 0x01, 0x04, 0x1B, 0xA0, 0x61, 0x98, 0x38,
  0x4C, 0x1D, 0xB5, 0x00, 0x00, 0xE9, 0xAE, 0x62, 0x09, 0xC6, 0x00, 0x00, 0x00
};

void handleWebServerGetFavIcon() {
  wifiWebServer.sendHeader( F("Content-Encoding"), F("gzip") );
  wifiWebServer.send_P( 200, getContentType( F("ico") ).c_str(), (const char*)FAVICON_ICO_GZ, sizeof(FAVICON_ICO_GZ) );

  if( isApInitialized ) { //this resets AP timeout when user loads the page in AP mode
    apStartedMillis = millis();
  }
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
  wifiWebServer.on( HTML_PAGE_ROOT_ENDPOINT, HTTP_GET,  handleWebServerGet );
  wifiWebServer.on( HTML_PAGE_ROOT_ENDPOINT, HTTP_POST, handleWebServerPost );
  wifiWebServer.on( HTML_PAGE_PREVIEW_ENDPOINT, HTTP_GET, handleWebServerGetPreview );
  wifiWebServer.on( HTML_PAGE_DATA_ENDPOINT, HTTP_GET, handleWebServerGetData );
  wifiWebServer.on( HTML_PAGE_SET_DATE_ENDPOINT, HTTP_GET, handleWebServerSetDate );
  wifiWebServer.on( HTML_PAGE_TEST_NIGHT_ENDPOINT, HTTP_GET, handleWebServerGetTestNight );
  wifiWebServer.on( HTML_PAGE_TESTLED_ENDPOINT, HTTP_GET, handleWebServerGetTestLeds );
  wifiWebServer.on( HTML_PAGE_REBOOT_ENDPOINT, HTTP_GET, handleWebServerGetReboot );
  wifiWebServer.on( HTML_PAGE_PING_ENDPOINT, HTTP_GET, handleWebServerGetPing );
  wifiWebServer.on( HTML_PAGE_FAVICON_ENDPOINT, HTTP_GET, handleWebServerGetFavIcon );
  wifiWebServer.onNotFound([]() {
    handleWebServerRedirect();
  });
  httpUpdater.setup( &wifiWebServer );
}



WiFiEventHandler wiFiEventHandler;
void onWiFiConnected( const WiFiEventStationModeConnected& event ) {
  Serial.println( String( F("WiFi is connected to '") + String( event.ssid ) ) + String ( F("'") ) );
  forceNtpUpdate = true;
}

void setup() {
  initInternalLed();

  Serial.begin( 115200 );
  Serial.println();
  Serial.println( String( F("Clock by Dmytro Kurylo. V@") ) + getFirmwareVersion() + String( F(" CPU@") ) + String( ESP.getCpuFreqMHz() ) );

  initEeprom();
  loadEepromData();
  initVariables();
  initDisplay();
  TCFonts::setFont( displayFontTypeNumber );
  LittleFS.begin();

  configureWebServer();
  wiFiEventHandler = WiFi.onStationModeConnected( &onWiFiConnected );
  connectToWiFiAsync( true );
  startWebServer();
  initTimeClient();
}

void loop() {
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
    connectToWiFiSync();
  }

  if( /*isFirstLoopRun || */( calculateDiffMillis( previousMillisWiFiStatusCheck, currentMillis ) >= DELAY_WIFI_CONNECTION_CHECK ) ) {
    if( !WiFi.isConnected() ) {
      if( !isApInitialized ) {
        connectToWiFiAsync( false );
      }
    }
    previousMillisWiFiStatusCheck = currentMillis;
  }

  currentMillis = millis();
  if( isFirstLoopRun || forceNtpUpdate || ( calculateDiffMillis( previousMillisNtpUpdatedCheck, millis() ) >= DELAY_NTP_UPDATED_CHECK ) ) {
    if( updateTimeClient( false ) ) {
      forceNtpUpdate = false;
    }
    previousMillisNtpUpdatedCheck = currentMillis;
  }
  bool timeClientInitStatusChanged = timeClientTimeInitStatus != timeClient.isTimeSet();
  if( timeClientInitStatusChanged ) {
    forceDisplaySync();
    timeClientTimeInitStatus = timeClient.isTimeSet();
  }

  currentMillis = millis();
  if( isFirstLoopRun || isForceDisplaySync || ( calculateDiffMillis( previousMillisDisplayAnimation, currentMillis ) >= DELAY_DISPLAY_ANIMATION ) ) {
    bool doRenderDisplay = true;
    if( isForceDisplaySync ) {
      if( timeClient.isTimeSet() || isCustomDateTimeSet ) {
        unsigned long timeClientSecondsCurrent = 0;
        if( timeClient.isTimeSet() ) {
          timeClientSecondsCurrent = timeClient.getEpochTime();
        } else if( isCustomDateTimeSet ) {
          timeClientSecondsCurrent = customDateTime + calculateDiffMillis( customDateTimeReceivedAt, currentMillis );
        }
        if( isSlowSemicolonAnimation ) {
          isSemicolonShown = timeClientSecondsCurrent % 2 == 0;
        } else { //else if 500 let it remain blank until the next second comes
          isSemicolonShown = false;
        }
        if( timeClientInitStatusChanged || timeClientSecondsAtDisplaySyncStart != timeClientSecondsCurrent ) {
          if( !isSlowSemicolonAnimation ) {
            isSemicolonShown = true;
          }
          previousMillisSemicolonAnimation = currentMillis;
          previousMillisDisplayAnimation = currentMillis;
          isForceDisplaySync = false;
        } else {
          doRenderDisplay = false;
        }
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