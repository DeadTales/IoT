#include <ArduinoJson.h>
#include <LittleFS.h>
#include <SPI.h>
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

void setup() {
  Serial.begin(115200);

  while(!Serial) delay(10);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("Iniciando...");

  if(!LittleFS.begin(true)){
    Serial.println("Error al inicializar LittleFS");
    tft.setTextColor(TFT_RED);
    tft.println("Error LittleFS");
    return;
  }

  File file = LittleFile.open("/config.json");

  if (!file){
    Serial.println("Error al abrir el archivo 'config.json'");
    tft.setTextColor(TFT_RED);
    tft.println("Falta 'config.json'");
  }

  JsonDocument doc;
}

void loop() {
  // put your main code here, to run repeatedly:

}
