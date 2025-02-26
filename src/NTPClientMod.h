#pragma once

#include "Arduino.h"

#include <Udp.h>

#define SEVENZYYEARS 2208988800UL
#define NTP_PACKET_SIZE 48
#define NTP_DEFAULT_LOCAL_PORT 1337

class NTPClient {
  public:
    enum Status { STATUS_IDLE, STATUS_AWAITING_RESPONSE, STATUS_FAILED_RESPONSE, STATUS_SUCCESS_RESPONSE };

  private:
    UDP*          _udp;
    bool          _udpSetup       = false;

    const char*   _poolServerName = "pool.ntp.org"; // Default time server
    IPAddress     _poolServerIP;
    unsigned int  _port           = NTP_DEFAULT_LOCAL_PORT;
    long          _timeOffset     = 0;

    unsigned long _updateInterval = 60000;  // In ms

    unsigned long _currentEpoc    = 0;      // In s
    unsigned long _lastUpdate     = 0;      // In ms

    byte          _packetBuffer[NTP_PACKET_SIZE];

    void          sendNTPPacket();

    bool          isPacketTravelling = false;
    unsigned long packetSentMillis = 0;
    Status        currentStatus = STATUS_IDLE;

    unsigned long calculateDiffMillis( unsigned long startMillis, unsigned long endMillis ) const;

  public:
    NTPClient(UDP& udp);
    NTPClient(UDP& udp, long timeOffset);
    NTPClient(UDP& udp, const char* poolServerName);
    NTPClient(UDP& udp, const char* poolServerName, long timeOffset);
    NTPClient(UDP& udp, const char* poolServerName, long timeOffset, unsigned long updateInterval);
    NTPClient(UDP& udp, IPAddress poolServerIP);
    NTPClient(UDP& udp, IPAddress poolServerIP, long timeOffset);
    NTPClient(UDP& udp, IPAddress poolServerIP, long timeOffset, unsigned long updateInterval);

    /**
     * Set time server name
     *
     * @param poolServerName
     */
    void setPoolServerName(const char* poolServerName);

     /**
     * Set random local port
     */
    void setRandomPort(unsigned int minValue = 49152, unsigned int maxValue = 65535);

    /**
     * Starts the underlying UDP client with the default local port
     */
    void begin();

    /**
     * Starts the underlying UDP client with the specified local port
     */
    void begin(unsigned int port);

    /**
     * This should be called in the main loop of your application. By default an update from the NTP Server is only
     * made every 60 seconds. This can be configured in the NTPClient constructor.
     *
     * @return NTPClient::Status
     */
    Status update();

    /**
     * This will force the update from the NTP Server.
     *
     * @return NTPClient::Status
     */
    Status forceUpdate();

    /**
     * This allows to check if the NTPClient successfully received a NTP packet and set the time.
     *
     * @return true if time has been set, else false
     */
    bool isTimeSet() const;

    int getDay() const;
    int getHours() const;
    int getMinutes() const;
    int getSeconds() const;
    int getSubSeconds() const;

    /**
     * Changes the time offset. Useful for changing timezones dynamically
     */
    void setTimeOffset(int timeOffset);

    /**
     * Set the update interval to another frequency. E.g. useful when the
     * timeOffset should not be set in the constructor
     */
    void setUpdateInterval(unsigned long updateInterval);

    /**
     * @return time formatted like `hh:mm:ss`
     */
    String getFormattedTime() const;

    /**
     * @return time in seconds since Jan. 1, 1970
     */
    unsigned long getEpochTime() const;

    /**
     * Stops the underlying UDP client
     */
    void end();

    /*
    * gets the last millis when time was updated
    */
    unsigned long getLastUpdateMillis() const;
};
