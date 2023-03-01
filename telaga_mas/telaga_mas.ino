#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include <Firebase_ESP_Client.h>
#include <dht.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ESP8266Ping.h>

#define API_KEY "AIzaSyDLhNBs1q-ICr4o4LnGxxTX--bUjj9NxV8"
#define DATABASE_URL "https://esp-firebase-demo-a15c2-default-rtdb.firebaseio.com/"
//#define LED D0            // Led in NodeMCU at pin GPIO16 (D0).
#define D2 4
//#define LED 4

char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
const long utcOffsetInSeconds = 28800;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);
int moderelay = 0;
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
dht DHT;
ESP8266WebServer server(80);
const char* remote_host = "google.com";
String ssid, pass, devid, content;

bool apmode = false;                              //Default AP mode status
const int relay = 5;
int count = 0;
bool signupOK = false;
double temp = 0.0, hum = 0.0;
unsigned long sendDataPrevMillis = 0;
int relaystatus = 0;
int prevrelaystatus = 0;
String curtime = "";
String time1 = "", time2 = "", time3 = "", time4 = "", time5 = "";
String lastupdate = "", timeupdate = "";
bool isconnected = false;
int countconn = 0;
int resetpin = 1;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);                           //Serial monitor
  Serial.println("SETUP");
  pinMode(relay, OUTPUT);
  pinMode(0, INPUT_PULLUP);
  Serial.println("AP 5 Second setup");
  delay(5000);
  resetpin = digitalRead(0);
  delay(100);
  readData();                                     //Check EEPROM for stored credentials
  delay(100);
  if (resetpin == 0) {
    ap_mode();
    Serial.println("Setup AP Mode");
  } else {
    //pinMode(LED, OUTPUT);
    //digitalWrite(LED, LOW);
    Serial.println("WiFi Mode");
    if (ssid.length() == 0) {                       //If not found set AP mode
      ap_mode();
    } else {
      if (testWifi()) {                             //If found try to connect to WiFi AP
        Serial.println("WiFi Connected!!!");
        apmode = false;
        config.api_key = API_KEY;
        config.database_url = DATABASE_URL;
        if (Firebase.signUp(&config, &auth, "", "")) {
          Serial.println("ok");
          signupOK = true;
        }
        else {
          Serial.printf("%s\n", config.signer.signupError.message.c_str());
        }
        config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h

        Firebase.begin(&config, &auth);
        Firebase.reconnectWiFi(true);
        delay(100);
        digitalWrite(relay, LOW); //off
        Serial.print("Setup relay off");
        timeClient.begin();
      }
      else {                                       //If failed set AP mode
        Serial.println("Unable to connect to wifi. delay (20s)");
        delay(30000);
        ESP.restart();
        //ap_mode();
      }
    }
  }
}

void relayOperation(bool st) {
  if (st) {
    digitalWrite(relay, HIGH); //on
    delay(120000); //let it on for 30 seconds
    digitalWrite(relay, LOW);
  }
}

void getDht() {
  int chk = DHT.read11(D2);
  Serial.print("Temperature = ");
  temp = DHT.temperature;
  Serial.println(DHT.temperature);
  hum = DHT.humidity;
  Serial.print("Humidity = ");
  Serial.println(DHT.humidity);
  yield();
  delay(1000);
}

void loop() {
  // put your main code here, to run repeatedly:
  if (apmode) {
    server.handleClient();                        //If apmode is true listen to incoming connection.
  } else {
    if (WiFi.status() == WL_CONNECTED) {          //If WiFi connected
      if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 15000 || sendDataPrevMillis == 0)) {
        if (Ping.ping(remote_host)) {
          isconnected  = true;
          Serial.println("Ping OK");
          countconn = 0;
        } else {
          Serial.println("Ping NOT OK");
          isconnected = false;
          countconn = countconn + 1;
        }
        if (isconnected) {
          sendDataPrevMillis = millis();
          sendDataPrevMillis = millis();
          timeClient.update();
          time_t epochTime = timeClient.getEpochTime();
          struct tm *ptm = gmtime ((time_t *)&epochTime);
          int monthDay = ptm->tm_mday;
          int currentMonth = ptm->tm_mon + 1;
          int currentYear = ptm->tm_year + 1900;
          lastupdate = String(monthDay) + "-" + String(currentMonth) + "-" + String(currentYear);
          timeupdate = String(timeClient.getHours()) + ":" + String(timeClient.getMinutes());
          sendDataPrevMillis = millis();
          // Write an Int number on the database path test/int
          getDht();
          Firebase.RTDB.setFloat(&fbdo, devid + "/temp", temp);
          Firebase.RTDB.setFloat(&fbdo, devid + "/humidity", hum);
          Firebase.RTDB.setString(&fbdo, devid + "/dtupdate", lastupdate);
          Firebase.RTDB.setString(&fbdo, devid + "/timeupdate", timeupdate);
          if (Firebase.RTDB.getInt(&fbdo, devid + "/mode")) {
            if (fbdo.dataType() == "int") {
              moderelay = fbdo.intData();
              Serial.print("MODE:");
              Serial.println(moderelay);
            }
          }
          if (moderelay == 0) { //non auto mode
            Serial.println("Manual Mode");
            if (Firebase.RTDB.getInt(&fbdo, devid + "/relay")) {
              if (fbdo.dataType() == "int") {
                relaystatus = fbdo.intData();
                Serial.print("RELAY STATUS:");
                Serial.println(relaystatus);
                if (relaystatus == 1) {
                  digitalWrite(relay, HIGH); //off
                } else {
                  digitalWrite(relay, LOW); //off
                }
              }
            }
          }
          if (moderelay == 1) {
            Serial.println("Auto Mode");
            timeClient.update();
            if (Firebase.RTDB.getString(&fbdo, devid + "/time1")) {
              if (fbdo.dataType() == "string") {
                time1 = fbdo.stringData();
                curtime = String(timeClient.getHours()) + ":" + String(timeClient.getMinutes());
                Serial.println(curtime);
                Serial.println(time1);
                if (curtime == time1 ) {
                  Serial.println("1:Relay ON");
                  relayOperation(true);
                }
              }
            }
            if (Firebase.RTDB.getString(&fbdo, devid + "/time2")) {
              if (fbdo.dataType() == "string") {
                time2 = fbdo.stringData();
                Serial.println(time2);
                curtime = String(timeClient.getHours()) + ":" + String(timeClient.getMinutes());
                // Serial.println(curtime);
                if (curtime == time2 ) {
                  Serial.println("2:Relay ON");
                  relayOperation(true);
                }
              }
            }
            if (Firebase.RTDB.getString(&fbdo, devid + "/time3")) {
              if (fbdo.dataType() == "string") {
                time3 = fbdo.stringData();
                Serial.println(time3);
                curtime = String(timeClient.getHours()) + ":" + String(timeClient.getMinutes());
                // Serial.println(curtime);
                if (curtime == time3 ) {
                  Serial.println("3:Relay ON");
                  relayOperation(true);
                }
              }
            }
            if (Firebase.RTDB.getString(&fbdo, devid + "/time4")) {
              if (fbdo.dataType() == "string") {
                time4 = fbdo.stringData();
                Serial.println(time4);
                curtime = String(timeClient.getHours()) + ":" + String(timeClient.getMinutes());
                // Serial.println(curtime);
                if (curtime == time4 ) {
                  Serial.println("4:Relay ON");
                  relayOperation(true);
                }
              }
            }
            if (Firebase.RTDB.getString(&fbdo, devid + "/time5")) {
              if (fbdo.dataType() == "string") {
                time5 = fbdo.stringData();
                Serial.println(time5);
                curtime = String(timeClient.getHours()) + ":" + String(timeClient.getMinutes());
                Serial.println(curtime);
                if (curtime == time5 ) {
                  Serial.println("5:Relay ON");
                  relayOperation(true);
                }
              }
            }
          }
        } else {
            Serial.println("Restart No Ping (20 times for 20s)");
            delay(20000);
            ESP.restart();
        }
      }
    } else {
      Serial.println("Restart No WiFi");
      delay(20000);
      ESP.restart();
    }
  }
}

void ap_mode() {                                  //Function to set as AP mode
  Serial.println("AP Mode. Please connect to http://192.168.4.1 to configure");
  apmode = true;
  const char* ssidap = "wifi-ap-ste";
  const char* passap = "";
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssidap, passap);
  Serial.println(WiFi.softAPIP());
  apmode = true;
  launchWeb(0);                                   //Create web server
}

void launchWeb(int webtype) {
  createWebServer(webtype);
  server.begin();
}

void createWebServer(int webtype) {             //Create web server HTML interface
  if (webtype == 0) { //AP mode
    server.on("/", []() {
      content = "<html><head><style>.button {background-color: #3CBC8D;";
      content += "color: white;padding: 5px 10px;text-align: center;text-decoration: none;";
      content += "display: inline-block;font-size: 14px;margin: 4px 2px;cursor: pointer;}";
      content += "input[type=text],[type=password]{width: 100%;padding: 5px 10px;";
      content += "margin: 5px 0;box-sizing: border-box;border: none;";
      content += "background-color: #3CBC8D;color: white;}</style></head><body>";
      content += "<h1>WIFI MANAGER</h1><br>";
      content += "<h3>Current Settings</h3>";
      content += "<table><tr><td><label> Device ID</label></td><td><label>" + devid + "</label></td></tr>";
      content += "<tr><td><label> WiFi SSID</label></td><td><label>" + ssid + "</label></td></tr>";
      content += "<tr><td><label> WiFi Pasword</label></td><td><label>" + pass + "</label></td></tr></table><br><br>";
      content += "<form method='get' action='setting'>";
      content += "<h3>New WiFi Settings</h3>";
      content += "<table><tr><td><label>WiFi SSID</label></td><td><input type='text' name = 'ssid' lenght=32 ></td></tr>";
      content += "<tr><td><label> WiFi Password</label></td><td><input type='password' name = 'password' lenght=32></td></tr>";
      content += "<tr><td><label>Device ID</label></td><td><input type='text' name = 'deviceid' lenght=32 ></td></tr>";
      content += "<tr><td></td><td><input class=button type='submit'></td></tr></table></form>";
      content += "</body></html>";
      server.send(200, "text/html", content);
    });
    server.on("/setting", []() {                  //Respond handler for form submit button
      String ssidw = server.arg("ssid");
      String passw = server.arg("password");
      String devid = server.arg("deviceid");
      Serial.println(devid);
      writeData(ssidw, passw, devid);                   //Store data from HTML form to EEPROM
      content = "Success..please reboot";
      server.send(200, "text/html", content);
      delay(2000);
      ESP.restart();
    });
    server.on("/clear", []() {                   //Clear data in EEPROM
      clearData();
      delay(2000);
      ESP.restart();
    });
  }
}

void readData() {                                 //Read from EEPROM memory
  EEPROM.begin(512);
  Serial.println("Reading From EEPROM..");
  for (int i = 0; i < 20; i++) {                  //Reading for SSID max for 20 characters long
    if (char(EEPROM.read(i)) > 0) {
      ssid += char(EEPROM.read(i));
      Serial.println(char(EEPROM.read(i)));
    }
  }
  Serial.println("Reading Wifi ssid: " + ssid);
  for (int i = 20; i < 40; i++) {                 //Reading for WiFi password max for 40 characters long
    if (char(EEPROM.read(i)) > 0) {
      pass += char(EEPROM.read(i));
      Serial.println(char(EEPROM.read(i)));
    }
  }
  Serial.println("Reading Wifi password: " + pass);
  for (int i = 40; i < 60; i++) {                 //Reading for SSID for 40 characters long
    if (char(EEPROM.read(i)) > 0) {
      devid += char(EEPROM.read(i));
      Serial.println(char(EEPROM.read(i)));
    }
  }
  Serial.println("Reading Dev ID: " + devid);

  EEPROM.end();
}

void writeData(String a, String b, String c) {             //Writing WiFi credentials to EEPROM
  Serial.println("Writing data...");
  clearData();                                    //Clear current EEPROM memory
  EEPROM.begin(512);
  Serial.println("Writing to EEPROM...");
  Serial.println("Writing Wifi ssid: " + a);
  for (int i = 0; i < 20; i++) {                  //Writing for SSID max for 20 characters long
    EEPROM.write(i, a[i]);
    Serial.println(a[i]);

  }
  Serial.println("Writing Wifi password: " + b);
  for (int i = 20; i < 40; i++) {                 //writing for password max for 40 characters long
    EEPROM.write(i, b[i - 20]);
    Serial.println(b[i]);

  }
  Serial.println("Writing Dev ID: " + c);
  for (int i = 40; i < 60; i++) {                 //writing for devid max for 40 characters long
    EEPROM.write(i, c[i - 40]);
    Serial.println(c[i]);
  }
  EEPROM.commit();
  EEPROM.end();
  Serial.println("Write Successfull");
}

void clearData() {                                //Clear current EEPROM memory function
  EEPROM.begin(512);
  Serial.println("Clearing EEPROM ");
  for (int i = 0; i < 512; i++) {
    Serial.print(".");
    EEPROM.write(i, 0);
  }
  EEPROM.commit();
  EEPROM.end();
}

boolean testWifi() {                            //Test WiFi connection function
  WiFi.softAPdisconnect();
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());
  int c = 0;
  while (c < 30) {
    if (WiFi.status() == WL_CONNECTED) {
      WiFi.setAutoReconnect(true);
      WiFi.persistent(true);
      Serial.println(WiFi.status());
      Serial.println(WiFi.localIP());
      delay(100);
      //digitalWrite(LED, HIGH);
      return true;
    }
    Serial.print(".");
    yield();
    delay(1000);
    c++;
  }
  //digitalWrite(LED, LOW);
  Serial.println("Connection time out...");
  return false;
}
