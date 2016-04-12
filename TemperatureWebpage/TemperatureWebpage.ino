#include <TimeLib.h>
#include <Wire.h>
#include <DS1307RTC.h>

#include <SPI.h>
#include <SD.h>

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include "ClockSync.h"
#include "webpage.h"

//WiFi credentials
const char* ssid = "****";
const char* password = "****";

const String url = "/rest/data/";
const char* host = "http://eskclimate.azurewebsites.net";


String timeZoneIds [] = {"America/New_York", "Europe/London", "Europe/Paris", "Australia/Sydney"};
ClockSync clockSync("en", "EN", "dd.MM.yyyy", 4, timeZoneIds);
const int CurrentTimezone = 2;

const int MaxNumberOfLines = 64;

File myFile;
//File logFile;
byte lastHour = 0;
String fileName = "th.csv";
String logFileName = "log.txt";
bool isSDcard = false;
String temperature;
String humidity;

ESP8266WebServer server(80);


void writeLogEntry(String data)
{
  myFile = SD.open(logFileName, FILE_WRITE);
  Serial.println("open log file");
  if (myFile)
  {
    String content = getDateTime();
    content += "\t";
    content += data;
    myFile.println(content);
    Serial.println(content);
    myFile.close();
    Serial.println("OK");
  }
}

void writeDataOnServer()
{
  writeLogEntry(F("connecting to server"));
  Serial.print(F("connecting to "));
  Serial.println(host);

  // Use WiFiClient class to create TCP connections
  WiFiClient client;
  const int httpPort = 80;
  if (!client.connect(host, httpPort)) {
    Serial.println(F("connection failed"));
     writeLogEntry(F("connection failed"));
    return;
  }

  Serial.print("Requesting URL: ");
  Serial.println(url);

  String request = "{\"sensorId\":1,\"temperature\":" + temperature + ",\"humidity\":" + humidity + "}\r\n\r\n";
  Serial.println("Request: " + request);
  // This will send the request to the server
  client.print("POST " + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "Content-Length: " + String(request.length()) + "\r\n" +
               "Content-Type: application/json\r\n" +
               "Connection: close\r\n\r\n");

  client.println(request);

  int timeout = millis() + 5000;
  while (client.available() == 0) {
    if (timeout - millis() < 0) {
      Serial.println(">>> Client Timeout !");
      client.stop();
      writeLogEntry("Client Timeout");
      return;
    }
  }

  // Read all the lines of the reply from server and print them to Serial
  while (client.available()) {
    String line = client.readStringUntil('\r');
    Serial.print(line);
  }

  Serial.println();
  Serial.println("closing connection");
  writeLogEntry("closing connection");
}

void readI2C()
{
  Wire.requestFrom(9, 10);    // request 2 bytes from slave device #8
  String i2cResult;
  while (Wire.available())
  { // slave may send less than requested
    i2cResult += Wire.read();
  }
  temperature = i2cResult.substring(0, 2);
  temperature += ".";
  temperature += i2cResult.substring(2, 3);

  humidity = i2cResult.substring(3, 5);
  humidity += ".";
  humidity += i2cResult.substring(5, 6);
}
String getDateTime()
{
  String content = String(year());
  content += "-";
  byte d = month();
  if (d < 10)
  {
    content += "0";
  }
  content += d;
  content += "-";
  d = day();
  if (d < 10)
  {
    content += "0";
  }
  content += d;
  content += " ";

  d = hour();
  if (d < 10)
  {
    content += "0";
  }
  content += d;
  content += ":";
  d = minute();
  if (d < 10)
  {
    content += "0";
  }
  content += d;
  content += ":";
  d = second();
  if (d < 10)
  {
    content += "0";
  }
  content += d;
  return content;
}

bool InitalizeSDcard()
{
  Serial.print("Initializing SD card...");

  if (!SD.begin(SS)) {
    Serial.println("initialization failed!");
    isSDcard = false;

  }
  else
  {
    Serial.println("initialization done.");
    isSDcard = true;
  }
  return isSDcard;
}

void timeUpdate()
{
  tmElements_t tm;
  clockSync.updateTime();
  delay(1000);
  tm = clockSync.getDateTime(CurrentTimezone);
  RTC.write(tm);
}

String readLastData()
{
  String rows;
  myFile = SD.open(fileName);
  int i, numberOfLines = 0;
  if (myFile)
  {
    // read from the file until there's nothing else in it:
    while (myFile.available())
    {
      myFile.readStringUntil('\n');
      numberOfLines++;
    }

    // close the file:
    myFile.close();

    myFile = SD.open(fileName);
    while (myFile.available())
    {
      if (numberOfLines - MaxNumberOfLines >= i)
      {
        myFile.readStringUntil('\n');
      }
      else
      {
        rows += myFile.readStringUntil('\n');
      }
      i++;
    }
    myFile.close();
  }
  return rows;
}
void handleRawData()
{
  String content = RawDataDataPageHeader;

  content += TableHeader;
  content += readLastData();
  content += "</table></body></html>";

  server.send(200, "text/html", content);
}

void handleMeasuredData()
{
  String rows;
  String content = MeasuredDataPageHeader;

  content += TableHeader;
  content += readLastData();
  content += "</table></body></html>";

  server.send(200, "text/html", content);
}

//root page can be accessed only if authentification is ok
void handleRoot() {
  Serial.println("Enter handleRoot");
  //  Serial.println(h);
  long rssi = WiFi.RSSI();

  tmElements_t tm;
  RTC.read(tm);

  String header;

  String content = PageHeader;
  if (server.hasHeader("User-Agent")) {
    content += "<table><tr><td><b>The user agent used is: </b></td><td>" + server.header("User-Agent") + "</td></tr><tr><td> </td></tr>";
  }
  readI2C();
  setSyncProvider(RTC.get);
  content += "<tr><td><b>Wifi strenght:</b></td><td>";
  content += rssi;
  content += " dB</td></tr><tr><td><b>Temperature senzor:</b></td><td>";
  content += temperature;
  content += " C</td></tr>";
  content += "<tr><td><b>Humidity senzor:</b></td><td>";
  content += humidity;
  content += " %</td></tr>";
  content += "<tr><td><b>Time:</b></td><td>";
  content += getDateTime();
  content += "</td></tr></table>";
  if (isSDcard)
    content += "<br><p>SD card is initialized. <a href=\"/measuredData\">Measured data</a>";
  content += "</p></body></html>";
  server.send(200, "text/html", content);
}

//no need authentification
void handleNotFound()
{
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void handleLogData()
{
  String content = PageHeader;
  myFile = SD.open(logFileName);
  if (myFile)
  {
    while (myFile.available())
    {
      content += myFile.readStringUntil('\n');
      content += "<br>";
    }
    // close the file:
    myFile.close();
  }
  content += "</body></html>";
  server.send(200, "text/html", content);
}

void setup(void) {
  Serial.begin(115200);
  Serial.println();
  Wire.begin(8);                // join i2c bus with address #8
  Serial.println("SENSOR ACTIVE");
  delay(50);

  Serial.print("Initializing SD card...");
  String ssidString = "";
  String passwordString = "";
  if (InitalizeSDcard())
  {
    bool isSsid = true;

    myFile = SD.open("wifi");

    if (myFile)
    {
      while (myFile.available())
      {
        char currentChar = (char)myFile.read();

        if (currentChar == '\n')
        {
          isSsid = false;
        }
        else
        {
          if (isSsid)
          {
            ssidString += currentChar;
          }
          else
          {
            passwordString += currentChar;
          }
        }
      }
      char s[ssidString.length()];
      char p[passwordString.length()];
      ssidString.toCharArray(s, ssidString.length());
      passwordString.toCharArray(p, passwordString.length() + 1);

      myFile.close();
      WiFi.begin(s, p);
    }
    else
    {
      Serial.println("Wifi file not found");
      ssidString = ssid;
      WiFi.begin(ssid, password);
    }
  }
  else
  {
    Serial.println("Wifi file not found");
    ssidString = ssid;
    WiFi.begin(ssid, password);
  }

  Serial.println();
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssidString);

  byte i = 0;
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
    if (i > 30)
    {
      Serial.println();
      Serial.println("Connection failed");
      writeLogEntry("Connection failed");
      return;
    }
    i++;
  }
  Serial.println();
  Serial.print("Connected to ");
  Serial.println(ssidString);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  //writeLogEntry("IP address: " + WiFi.localIP());
  timeUpdate();

  server.on("/", handleRoot);
  server.on("/measuredData", handleMeasuredData);
  server.on("/rawData", handleRawData);
  server.on("/log", handleLogData);

  server.onNotFound(handleNotFound);
  //here the list of headers to be recorded
  const char * headerkeys[] = {"User-Agent", "Cookie"} ;
  size_t headerkeyssize = sizeof(headerkeys) / sizeof(char*);
  //ask server to track these headers
  server.collectHeaders(headerkeys, headerkeyssize );
  server.begin();
  Serial.println("HTTP server started");
  writeLogEntry("HTTP server started");
  Serial.println(getDateTime());
}

void loop(void) {
  server.handleClient();
  setSyncProvider(RTC.get);
  byte hours = hour();
  if (minute() == 45 && hours != lastHour)
  {
    lastHour = hours;
    if (!isSDcard)
    {
      if (!InitalizeSDcard())
        return;
    }
    writeLogEntry("Reading I2C");
    readI2C();

    myFile = SD.open(fileName, FILE_WRITE);

    if (myFile)
    {
      writeLogEntry("Open log file");
      String content = "<tr><td>";
      content += getDateTime();
      content += "</td><td>";
      content += temperature;
      content += "</td><td>";
      content += humidity;
      content += "</td></tr>";
      myFile.println(content);
      myFile.close();
      Serial.println(content);
    }
    else
    {
      // if the file didn't open, print an error:
      Serial.println("error opening file");
      writeLogEntry("error opening file");
    }
    writeDataOnServer();
  }
}

