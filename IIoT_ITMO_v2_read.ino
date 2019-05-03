// Libraries
#include <SPI.h>
#include <mcp_can.h>
#include <ESP8266WiFi.h>
#include <TimeLib.h>
#include <WiFiUDP.h>
#include <ArduinoJson.h>

// Constants
#define WIFI_SSID "TTK42" // SSID of the hotspot
#define WIFI_PASSWORD "rcitycvx" // Password for the hotspot
#define POST_URL "/t/ofjnk-1555876622/post" //////////////////////////////////////////
const char* host = "ptsv2.com"; // Web address of the server to which we are connecting
const uint16_t port = 80; // Port that will be used

// Variables
INT32U canId = 0x000; // CAN ID of the "send" node
bool running = false; // Loop mode "false"
int ts = 0; // Current timestamp (epoch)
String dev_name = "00:00:00:00:00:00"; // Name of the device (MAC adress)
unsigned char len = 0; // Length of the received message
unsigned char buff[8]; // The buffer for the received message

// Set CS pin
const int SPI_CS_PIN = 15; // Setup CAN shield cs pin
MCP_CAN CAN(SPI_CS_PIN);

// Code here runs once
void setup() {
  Serial.begin(115200); // Set baudrate
  Serial.println();

  // CANBUS setup
  if (CAN_OK == CAN.begin(CAN_125KBPS, MCP_8MHz)) Serial.println("CAN BUS Shield init OK");
  else Serial.println("CAN BUS Shield init FAIL");

  startWiFi(); // Connect to the hotspot
}

// Code here runs repeatedly
void loop() {
  if (Serial.available() > 0) {
        switch(Serial.read()) {
        case '1': // Start
            running = true;
            Serial.println("Loop initialized");
            break;
        case '0': // Stop
            running = false;
            Serial.println("Loop stopped");
            break;
        }
  }

  if (running) {  
    // Execute anything only if message received
    if (CAN_MSGAVAIL == CAN.checkReceive()){
      printCanbus();
      delay(500);
      json();
      delay(1500);  
    }
  }
}

// Connect to ssidAP hotspot, print MAC and IP
void startWiFi() 
{
  WiFi.mode(WIFI_STA); /* Choose mode WIFI_AP (hotspot), WIFI_STA (client), 
                       or WIFI_AP_STA (both modes simultaneously). */
                       
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD); // Connect to ssidAP using passAP password

  Serial.println();
  Serial.print("Connecting to "); Serial.print(WIFI_SSID);

  // Wait until connection establishes
  while(WiFi.status() != WL_CONNECTED){ /* Function returns one of the following connection statuses:

                                        WL_CONNECTED after successful connection is established
                                        WL_NO_SSID_AVAIL in case configured SSID cannot be reached
                                        WL_CONNECT_FAILED if password is incorrect
                                        WL_IDLE_STATUS when Wi-Fi is in process of changing between statuses
                                        WL_DISCONNECTED if module is not configured in station mode */
    delay(500);
    Serial.print(".");
  }
  
  if (WiFi.status() == WL_CONNECTED)  {
    Serial.println("Connected!");

    
    uint8_t macAddr[6]; /* WiFi.macAddress(mac) function should be provided with mac 
                        that is a pointer to memory location (an uint8_t array the size of 6 elements) 
                        to save the mac address. The same pointer value is returned by the function itself. 
                         
                        There is optional version of this function available. Instead of the pointer, 
                        it returns a formatted String that contains the same mac address.
                        
                        if (WiFi.status() == WL_CONNECTED) {
                          Serial.printf("Connected, mac address: %s\n", WiFi.macAddress().c_str());
                        } */
    WiFi.macAddress(macAddr); /* Get mac while in 'client' mode. 
                              Use WiFi.softAPmacAddress(mac) if in 'hotspot' mode. */
    Serial.printf("Mac address: %02x:%02x:%02x:%02x:%02x:%02x\n", macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5]);

    dev_name = WiFi.macAddress().c_str(); // Set device name to be the MAC adress

    // Get and print IP while in 'client' mode. Use WiFi.softAPIP() if in 'hotspot' mode
    Serial.print("IP address: "); Serial.println(WiFi.localIP());
  } 
  else {
    Serial.print("Unsuccessfully, bcz status is: ");
    Serial.println(WiFi.status());
  } 
}

// Print received message
void printCanbus()
{
  CAN.readMsgBuf(&len, buff); // Read message with length of 'len' and store it in 'buff'
  canId = CAN.getCanId(); // Get the CAN ID of the "send" node (id represents where the data come from)

  //Serial.println("Some message received from ID:" + String(canId));
  Serial.print("A message received from ID (HEX): ");
  Serial.print(canId, HEX);
  Serial.println();
  Serial.print("The message: ");

  // Print the message
  for (int i = 0; i < len; i++){
    Serial.print(buff[i], HEX);
    Serial.print(" ");
  }
  Serial.println();
}

// JSON serialization and sending POST request
void json()
{
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& doc = jsonBuffer.createObject();
  
  doc["device_name"] = dev_name; // MAC adress
  doc["time"] = ts++; // Epoch timestamp
  doc["id"] = String(canId, HEX); // CANid in hexadecimal representation
  String myString = "";
  for (int i = 0; i < len; ++i){
    myString += " " + String(buff[i],HEX);
  }
  myString.remove(0,1);
  doc["data"] = myString; // Basically, the data, that sender sent

  // Print doc to Serial
  doc.printTo(Serial);

  // Store JSON in data
  String data = "";
  doc.printTo(data);
     
  // Sending JSON with POST
  WiFiClient client; /* Declaring a client that will be contacting the host (server).
                        Use WiFiClient class to create TCP connections. */

  Serial.print("Connecting to "); Serial.print(host); Serial.print(":"); Serial.print(port); // Print host:port

  // Check if connection is possible and do smth if it is
  if (client.connect(host, port)) { // Start client connection. Checks for connection
    Serial.println(" ... Connected");

    Serial.println("Sending a POST request");

    // POST
    client.print("POST "); client.print(POST_URL); client.println(" HTTP/1.1");
    client.print("Host: "); client.println(host);
    //client.println("User-Agent: Arduino/1.0");
    client.println("User-Agent: NodeMCU/ESP8266");
    client.println("Connection: close");
    client.println("Content-type: application/json"); // Only if POSTing JSON
    client.print("Content-Length: "); client.println(data.length());
    client.println();
    client.println(data); 
    Serial.println("Response:");

    // Read all the lines of the reply from server and print them to Serial
    while (client.connected() || client.available()) { /* Connected or data available.
                                                        Should check client.connected() since we sent some data
                                                      
                                                        client.available() Returns the number of bytes 
                                                        available for reading (that is, the amount of data 
                                                        that has been written to the client by the server 
                                                        it is connected to).
  
                                                        client.connected() Returns true if the client is connected, 
                                                        false if not. Note that a client is considered connected 
                                                        if the connection has been closed 
                                                        but there is still unread data. */
      if (client.available()) {
        //String line = client.readStringUntil('\n');
        //Serial.println(line);
        
        char ch = static_cast<char>(client.read());
        Serial.print(ch);
      }
    } 
 
    client.stop();
    Serial.println();
    Serial.println("Disconnected. Connection closed");
  }
  else { // If no client connected
    Serial.println(" ... Connection failed!");
    client.stop();
  }
}
