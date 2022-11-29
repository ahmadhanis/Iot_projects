/*
   This ESP32 code is created by esp32io.com

   This ESP32 code is released in the public domain

   For more detail (instruction and wiring diagram), visit https://esp32io.com/tutorials/esp32-rfid-nfc
*/

#include <SPI.h>
#include <MFRC522.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SS_PIN  5  // ESP32 pin GIOP5 
#define RST_PIN 27 // ESP32 pin GIOP27 
#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 64  // OLED display height, in pixels
#define OLED_RESET    -1  // Reset pin # (or -1 if sharing reset pin)

MFRC522 mfrc522(SS_PIN, RST_PIN);   // Create MFRC522 instance
MFRC522::MIFARE_Key key;
String content = "";
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);


void setup() {
  Serial.begin(115200);
  SPI.begin(); // init SPI bus
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;); // Don't proceed, loop forever
  }

  display.display();
  delay(2);
  display.clearDisplay();
  display.fillScreen(WHITE);
  drawScreen(" SMART LAB SYSTEM", "", "   Setup", "Please wait...", "Starting up...");
  delay(3000);
  mfrc522.PCD_Init(); // init MFRC522
  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }
  Serial.println("Tap an RFID/NFC tag on the RFID-RC522 reader");
  drawScreen(" SMART LAB SYSTEM", "", "READY", "", "Completed");
  delay(3000);
  drawScreen(" SMART LAB SYSTEM", "", "READY", "", "Present Card");
}

void loop() {
  if ( ! mfrc522.PICC_IsNewCardPresent()) {
    delay(50);
    return;
  }
  // Select one of the cards
  if ( ! mfrc522.PICC_ReadCardSerial()) {
    delay(50);
    return;
  }
  content = "";
  for (byte i = 0; i < mfrc522.uid.size; i++)
  {
    content.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? "0" : ""));
    content.concat(String(mfrc522.uid.uidByte[i], HEX));
  }
  content.toUpperCase();
  Serial.println("Card read:" + content);
  drawScreen(" SMART LAB SYSTEM", "", "CARD ID", content, "");
}


void drawScreen(String a, String b, String c, String d, String e)
{
  display.display();
  delay(10);
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(15, 5);
  display.print(a);
  display.setCursor(0, 20);
  display.print(b);
  display.setCursor(0, 30);
  display.print(c);
  display.setCursor(0, 40);
  display.print(d);
  display.setCursor(0, 50);
  display.print(e);
  display.display();
}
