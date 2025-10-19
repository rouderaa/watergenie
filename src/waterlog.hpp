/* -------------------------------------------------------------
   WaterLog – RAM logger with CSV dump and SPIFFS persistence
   -------------------------------------------------------------
   • Stores up to 100 entries (timestamp + water level)
   • Circular buffer – oldest overwritten when full
   • Public helpers:
        – clear()                 : erase the log
        – dumpCSV(Print &)       : CSV output (Serial, File, …)
        – saveToSpiffs(path)     : persist to flash
        – loadFromSpiffs(path)   : restore from flash
   ------------------------------------------------------------- */

#ifndef WATERLOG_H
#define WATERLOG_H

#include <Arduino.h>
#include <FS.h>
#include <SPIFFS.h>          // <-- change to <LittleFS.h> if you use LittleFS
#include <time.h> 

class WaterLog {
public:
    // -----------------------------------------------------------------
    // Public constants
    // -----------------------------------------------------------------
    static const uint8_t CAPACITY = 100;   // max number of entries

    // -----------------------------------------------------------------
    // One log entry
    // -----------------------------------------------------------------
    struct Entry {
        time_t      timestamp;   // seconds since epoch
        unsigned long level;     // water‑level reading
    };

    // -----------------------------------------------------------------
    // Constructor – initialise an empty buffer
    // -----------------------------------------------------------------
    WaterLog() : head(0), tail(0), count(0) {}

    // -----------------------------------------------------------------
    // Add a new entry (overwrites oldest when full)
    // -----------------------------------------------------------------
    void add(time_t ts, unsigned long lvl) {
        buffer[head].timestamp = ts;
        buffer[head].level     = lvl;

        head = (head + 1) % CAPACITY;

        if (count < CAPACITY) {
            ++count;
        } else {
            // Buffer full → move tail forward (drop oldest)
            tail = (tail + 1) % CAPACITY;
        }
    }

    // -----------------------------------------------------------------
    // Retrieve an entry by logical index (0 = oldest, size‑1 = newest)
    // Returns true if index is valid, false otherwise.
    // -----------------------------------------------------------------
    bool get(uint8_t idx, Entry &out) const {
        if (idx >= count) return false;               // out of range
        uint8_t pos = (tail + idx) % CAPACITY;
        out = buffer[pos];
        return true;
    }

    // -----------------------------------------------------------------
    // Number of entries currently stored (0 … CAPACITY)
    // -----------------------------------------------------------------
    uint8_t size() const { return count; }

    // -----------------------------------------------------------------
    // Erase the entire log
    // -----------------------------------------------------------------
    void clear() {
        head = tail = 0;
        count = 0;
    }

    // -----------------------------------------------------------------
    // Dump the whole log as CSV lines to telnet:  timestamp,level
    // MM/DD/YYYY HH:MM:SS
    // -----------------------------------------------------------------
    void dumpCSVToTelnet(WiFiClient telnetClient) const {
        struct tm utcTime;

        telnetClient.println(F("datetime, level"));
        for (uint8_t i = 0; i < count; ++i) {
            const Entry &e = buffer[(tail + i) % CAPACITY];
            
            localtime_r((time_t *)&e.timestamp, &utcTime); 
            int year   = utcTime.tm_year + 1900;   // tm_year counts years since 1900
            int month  = utcTime.tm_mon  + 1;      // tm_mon is 0‑based (0 = January)
            int day    = utcTime.tm_mday;
            int hour   = utcTime.tm_hour;
            int minute = utcTime.tm_min;
            int second = utcTime.tm_sec;

            telnetClient.printf(
                "%04d-%02d-%02d %02d:%02d:%02d",
                year, month, day, hour, minute, second
            );

            telnetClient.print(',');
            telnetClient.println(e.level);
        }
    }

    // -----------------------------------------------------------------
    // Persist the log to SPIFFS.
    //   path – full filename inside the SPIFFS mount point (e.g. "/water.log")
    // Returns true on success, false on any error.
    // -----------------------------------------------------------------
    bool saveToSpiffs(const char *path) const {
        // Ensure the filesystem is mounted
        if (!SPIFFS.begin(true)) {           // true → format if missing
            return false;
        }

        File f = SPIFFS.open(path, FILE_WRITE);
        if (!f) {
            return false;
        }

        // Write bookkeeping values first (so we can validate on load)
        f.write(&head,  sizeof(head));
        f.write(&tail,  sizeof(tail));
        f.write(&count, sizeof(count));

        // Then write the raw entry buffer (binary, fixed size)
        size_t size = f.write((const uint8_t *)buffer, sizeof(buffer));

        f.flush();   // ensure data hits flash
        f.close();

        return (size == sizeof(buffer));
    }

    // -----------------------------------------------------------------
    // Load a previously saved log from SPIFFS.
    //   path – same filename used with saveToSpiffs()
    // Returns true if a valid file was found and loaded; false otherwise.
    // -----------------------------------------------------------------
    bool loadFromSpiffs(const char *path) {
        if (!SPIFFS.begin()) {               // try mounting, do NOT format here
            return false;
        }

        File f = SPIFFS.open(path, FILE_READ);
        if (!f) {
            return false;                    // file does not exist
        }

        // Expected total size = 3 bookkeeping bytes + raw buffer
        const size_t expectedSize = sizeof(head) + sizeof(tail) + sizeof(count)
                                    + sizeof(buffer);
        if (f.size() != expectedSize) {
            f.close();
            return false;                    // corrupted / wrong version
        }

        // Read bookkeeping values
        f.readBytes((char *)&head,  sizeof(head));
        f.readBytes((char *)&tail,  sizeof(tail));
        f.readBytes((char *)&count, sizeof(count));

        // Sanity‑check the counters (they must stay within limits)
        if (head >= CAPACITY || tail >= CAPACITY || count > CAPACITY) {
            f.close();
            return false;
        }

        // Read the entry buffer
        f.readBytes((char *)buffer, sizeof(buffer));

        f.close();
        return true;
    }

private:
    // -----------------------------------------------------------------
    // Fixed‑size RAM storage (binary layout is important for persistence)
    // -----------------------------------------------------------------
    Entry buffer[CAPACITY];

    // Circular‑buffer bookkeeping
    uint8_t head;   // next write position
    uint8_t tail;   // oldest entry position
    uint8_t count;  // number of valid entries
};

#endif // WATERLOG_H