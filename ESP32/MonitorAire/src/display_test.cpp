#include <Arduino.h>
#include <TFT_eSPI.h>

constexpr unsigned long COLOR_INTERVAL_MS = 2000;
constexpr uint16_t TEST_COLORS[] = {TFT_RED, TFT_GREEN, TFT_BLUE, TFT_WHITE};
constexpr const char *COLOR_NAMES[] = {"ROJO", "VERDE", "AZUL", "BLANCO"};
constexpr size_t COLOR_COUNT = sizeof(TEST_COLORS) / sizeof(TEST_COLORS[0]);

TFT_eSPI display;
unsigned long lastColorAt = 0;
size_t colorIndex = 0;

void showTestColor(size_t index);

void setup() {
    Serial.begin(115200);
    Serial.println("PRUEBA TFT: sin WiFi, MQTT, sensores ni acceso a archivos");
    Serial.printf("MOSI=%d SCLK=%d CS=%d DC=%d RST=%d SPI=%d Hz\n", TFT_MOSI, TFT_SCLK, TFT_CS,
                  TFT_DC, TFT_RST, SPI_FREQUENCY);

#if defined(USE_FSPI_PORT)
    Serial.println("Prueba con bus FSPI");
#else
    Serial.println("Prueba con bus HSPI");
#endif
    display.init();
    display.setRotation(1);
    display.setTextSize(2);
    Serial.printf("TFT inicializada: %d x %d en orientacion horizontal\n", display.width(),
                  display.height());
    showTestColor(colorIndex);
    lastColorAt = millis();
}

void loop() {
    if (millis() - lastColorAt >= COLOR_INTERVAL_MS) {
        lastColorAt = millis();
        colorIndex = (colorIndex + 1) % COLOR_COUNT;
        showTestColor(colorIndex);
    }
    delay(10);
}

void showTestColor(size_t index) {
    display.fillScreen(TEST_COLORS[index]);
    display.setTextColor(TFT_BLACK, TEST_COLORS[index]);
    display.setCursor(10, 20);
    display.println("PRUEBA ST7789");
    display.println(COLOR_NAMES[index]);
    Serial.printf("Tiempo encendido: %lu ms; color: %s\n", millis(), COLOR_NAMES[index]);
}
