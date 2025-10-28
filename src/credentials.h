// WiFi credentials
const char *ssid = "<yourwifissid>";
const char *password = "<yourwifipassword>";

// static IP settings
IPAddress local_IP(192, 168, 21, 50); // My static IP->Use your ip range
IPAddress gateway(192, 168, 21, 1);   // Router's IP ->Use your router IP
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(192, 168, 21, 1); // Optional, not necessary 
IPAddress secondaryDNS(192, 168, 21, 1);  // Optional, not necessary
