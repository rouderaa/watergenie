#include <Arduino.h>
#include <WiFi.h>
#include <esp_system.h>
#include "credentials.h"
#include <time.h>
#include <sys/time.h>
#include "waterlog.hpp"

HardwareSerial Serial1(1); // HACK

WiFiServer telnetServer(23);
WiFiClient telnetClient;

#define MAX_USER_INPUT 256
#define PROBE_PIN 4

extern void setup_led();
extern void set_led(unsigned char red, unsigned char green, unsigned char blue);

// water meter status
static ulong meter_reading;
static ulong meter_reading_previous;
static ulong meter_pulses;
static int meter_probe_current;
static int meter_probe_previous;
// NTP server configuration
const char* ntpServer = "nl.pool.ntp.org";
const long gmtOffset_sec = (3600*1);    // GMT offset in seconds (0 for UTC)
const int daylightOffset_sec = 3600;    // Daylight saving offset (3600 = 1 hour)

void printLocalTime(); // forward declaration

WaterLog waterlog;

extern "C"
{
    int yywrap();
    int yylex();
    char *set_input_string(const char *str);


    void handleLedOn()
    {
        Serial.println("LED ON");
        set_led(0, 50, 0);
    }
    void handleLedOff()
    {
        Serial.println("LED OFF");
        set_led(0, 0, 0);
    }
    void handleHelp()
    {
        telnetClient.println("led [on|off] : set the color led on the esp32-s3 to show blue.");
        telnetClient.println("help : displays this help information.");
        telnetClient.println("reset : performs a hard reset of the esp32-s3.");
        telnetClient.println("set <value> : sets the current value of the water meter.");
        telnetClient.println("get : returns the current value of the water meter.");
        telnetClient.println("time : displays the current date and time as retrieved from timeserver.");
        telnetClient.println("clear : clears the water log.");
        telnetClient.println("log : returns the latest measured water meter values with timestamp.");
    }

    /**
     * Converts two hexadecimal characters to their integer value
     * @param hexChars Pointer to two hex characters (0-9, A-F, a-f)
     * @return Integer value of the hexadecimal pair, or -1 if invalid characters
     */
    int hexPairToInt(const char *hexChars)
    {
        if (hexChars == NULL)
        {
            return -1;
        }

        int result = 0;

        // Process both characters using the same logic
        for (int i = 0; i < 2; i++)
        {
            char c = hexChars[i];
            int val;

            if (c >= '0' && c <= '9')
            {
                val = c - '0';
            }
            else if (c >= 'A' && c <= 'F')
            {
                val = c - 'A' + 10;
            }
            else if (c >= 'a' && c <= 'f')
            {
                val = c - 'a' + 10;
            }
            else
            {
                return -1; // Invalid character
            }

            // For first character, shift left by 4, for second just add
            result = (i == 0) ? (val << 4) : (result | val);
        }

        return result;
    }

    void handleReset()
    {
        esp_restart();
    }

    void handleSet(ulong value) {
        meter_reading = value;
        meter_pulses = meter_reading << 1;
    }

    void handleGet() {
        Serial.printf("Meter value:%lu\n", meter_reading);
        telnetClient.printf("Meter value:%lu\n", meter_reading);
    }

    void handleTime() {
        printLocalTime();
    }

    void handleClear() {
        waterlog.clear();
    }

    void handleLog() {
        waterlog.dumpCSVToTelnet(telnetClient);
    }

}

/**
 * Print current local time
 */
void printLocalTime() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("Failed to obtain time");
        return;
    }
    
    // Print in custom format
    char timeString[80];
    strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S\n", &timeinfo);

    telnetClient.printf(timeString);
}

/**
 * Synchronize ESP32-S3 system time with NTP server
 * @return true if successful, false otherwise
 */
bool syncTimeWithNTP() {
    Serial.println("Synchronizing time with NTP server...");
    
    // Configure time with NTP server
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    
    // Wait for time to be set
    int retry = 0;
    const int maxRetries = 10;
    
    while (retry < maxRetries) {
        time_t now = time(nullptr);
        if (now > 24 * 3600) {  // Check if time is set (after Jan 1, 1970)
            Serial.println("Time synchronized successfully!");
            printLocalTime();
            return true;
        }
        Serial.print(".");
        delay(1000);
        retry++;
    }
    
    Serial.println("\nFailed to synchronize time");
    return false;
}

void setup()
{
    Serial.begin(115200);

    Serial.println("Initializing....");
    
    // Configure static IP before WiFi.begin()
    if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
        Serial.println("STA Failed to configure");
    }
    WiFi.begin(ssid, password);

    // Wait for connection
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();

    setup_led();
    syncTimeWithNTP();

    // Setup input for the probe
    pinMode(PROBE_PIN, INPUT);
    meter_probe_current = digitalRead(PROBE_PIN);
    meter_probe_previous = meter_probe_current;

    telnetServer.begin();
}

static bool showIp = true;
static const char *welcome = "Welcome to project ESP32_Water_Genie";
static const char *prompt = "ESP32> ";
static long timestamp = 0L;

static void scanProbe() {
    if (millis() > timestamp) {
        // 10 times per second
        timestamp = millis() + 100;
        meter_probe_current = digitalRead(PROBE_PIN);
        // Find out if there was a change in the probe
        if (meter_probe_current != meter_probe_previous) {
            // Show status led on the esp32-s3 board
            if (meter_probe_current == 0) {
                // sensor is detecting water meter indicator
                set_led(0,0,50);
            } else {
                set_led(0,0,0);
            }

            // Increase measured amount of pulses, two per litre
            meter_pulses++;             
            meter_reading = meter_pulses >> 1;
            
            if (meter_reading != meter_reading_previous) {
                // Store latest changed water meter value in log
                time_t now;
                time(&now);
                waterlog.add(now, meter_reading);

                meter_reading_previous = meter_reading;
            }

            meter_probe_previous = meter_probe_current;
        }
    }
}

void loop()
{
    scanProbe();

    if (telnetServer.hasClient())
    {
        // Handle incoming connection
        telnetClient = telnetServer.accept();
        showIp = false;
        // Purge initial characters from telnet, may contain rubish
        while(telnetClient.available()) {
            telnetClient.read();
        }
        Serial.println(welcome);
        telnetClient.println(welcome);
        telnetClient.print(prompt);
    }

    // Check for client disconnections
    if (!telnetClient || !telnetClient.connected())
    {
        // Reset the telnetClient object to its original state
        telnetClient.stop();
        showIp = true;
    }

    if (showIp)
    {
        // Keep displaying ip address on serial if not connected
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
        delay(1000);
    }

    if (telnetClient && telnetClient.connected())
    {
        static char inputBuffer[MAX_USER_INPUT];
        static int inputIndex = 0;

        while (telnetClient.available())
        {
            char c = telnetClient.read();
            if (c == '\n')
            {
                if (inputIndex > 0)
                {
                    inputBuffer[inputIndex] = '\0';
                    char *parseError = set_input_string(inputBuffer);
                    telnetClient.println(parseError);
                    inputIndex = 0;
                    telnetClient.print(prompt);
                }
                else
                {
                    // Empty line, just show prompt again
                    telnetClient.print(prompt);
                }
            }
            else if (inputIndex < MAX_USER_INPUT - 1)
            {
                if (c != '\r')
                    inputBuffer[inputIndex++] = c;
            }
        }
    }
}