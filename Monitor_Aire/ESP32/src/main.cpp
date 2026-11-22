#include <Arduino.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <math.h>
#include <time.h>

// Pines de los sensores en la placa ESP32-S3.
constexpr uint8_t PIN_DHT = 4;
constexpr uint8_t PIN_MQ135 = 5;

// Intervalos de trabajo y umbral para aceptar una fecha sincronizada por NTP.
constexpr unsigned long SAMPLE_INTERVAL_MS = 2000;
constexpr unsigned long CONNECTION_RETRY_INTERVAL_MS = 5000;
constexpr unsigned long CSV_RETRY_INTERVAL_MS = 5000;
constexpr time_t EARLIEST_VALID_TIME = 1700000000;

// Parámetros de muestreo y referencia del MQ135.
constexpr unsigned int ADC_SAMPLE_COUNT = 32;
constexpr unsigned int REFERENCE_SAMPLE_COUNT = 30;
constexpr float MQ_SUPPLY_VOLTAGE = 5.0f;
constexpr float MQ_REFERENCE_PPM = 450.0f;
constexpr float MQ_CURVE_A = 110.47f;
constexpr float MQ_CURVE_B = -2.862f;

// Identidad del dispositivo y formato del respaldo en LittleFS.
constexpr size_t MIN_FREE_STORAGE_BYTES = 8192;
constexpr char DEVICE_ID[] = "ESP32_S3_0001";
constexpr char CSV_PATH[] = "/pending.csv";
constexpr char CSV_HEADER[] = "id,device_id,timestamp,temperature,humidity,gas,calibrate\n";

// Periféricos, cliente MQTT y configuración guardada.
TFT_eSPI display;
DHT dhtSensor(PIN_DHT, DHT11);
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
JsonDocument configuration;
Preferences queuePreferences;

// Temas MQTT del dispositivo para lecturas, estado y confirmaciones.
const String deviceTopicPrefix = String("dispositivo/") + DEVICE_ID;
const String rawTopic = deviceTopicPrefix + "/crudo";
const String processedTopic = deviceTopicPrefix + "/tratado";
const String acknowledgmentTopic = deviceTopicPrefix + "/ack";
const String statusTopic = deviceTopicPrefix + "/estado";

// Estado del ciclo principal y plazos de reconexión y muestreo.
bool isInitialized = false;
bool wasMqttConnected = false;
unsigned long lastSampleAt = 0;
unsigned long lastConnectionAttemptAt = 0;
unsigned long lastReplayAt = 0;

// La cola distingue el desplazamiento confirmado del último registro enviado.
size_t confirmedCsvOffset = 0;
size_t pendingCsvEndOffset = 0;
String pendingRecordId;

// Referencia del MQ135 acumulada durante las primeras muestras válidas.
float referenceResistanceSum = 0;
float referenceResistance = 0;
unsigned referenceCount = 0;
String gasSensorStatus = "Preparando";

// Geometría, colores y estilos de las tarjetas de la pantalla TFT.
namespace {
constexpr int DISPLAY_MARGIN = 12;
constexpr int FIRST_CARD_Y = 66;
constexpr int CARD_GAP = 6;
constexpr int FOOTER_HEIGHT = 28;
constexpr int METRIC_LABEL_X = 62;
constexpr int VALUE_WIDTH = 76;

constexpr uint16_t rgb565(uint8_t red, uint8_t green, uint8_t blue) {
    return ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3);
}

constexpr uint16_t SCREEN_BG = rgb565(11, 19, 36);
constexpr uint16_t CARD_BG = rgb565(20, 34, 56);
constexpr uint16_t ICON_BG = rgb565(34, 52, 79);
constexpr uint16_t SECONDARY_TEXT = rgb565(165, 188, 216);
constexpr uint16_t MINT = rgb565(91, 231, 186);
constexpr uint16_t ORANGE = rgb565(255, 184, 115);
constexpr uint16_t BLUE = rgb565(120, 203, 255);
constexpr uint16_t LAVENDER = rgb565(188, 171, 255);

enum Metric { TEMPERATURE, HUMIDITY, GAS, METRIC_COUNT };

struct MetricStyle {
    const char *label;
    const char *unit;
    uint16_t color;
};

constexpr MetricStyle METRICS[METRIC_COUNT] = {
    {"Temperatura", "C", ORANGE},
    {"Humedad", "%RH", BLUE},
    {"Gas MQ135", "ppm", LAVENDER},
};

int cardHeight();
int cardCenter(int metric);
void drawMetricIcon(int metric, int x, int y);
void drawDisplay();
void updateClock();
void updateMqttStatus();
} // namespace

// Configuración y transporte. Las implementaciones siguen al ciclo de Arduino.
bool loadConfiguration();
void maintainMqttConnection();
void onMqttMessage(char *topic, byte *payload, unsigned int length);

// Fecha de la lectura, adquisición y representación de los datos.
bool getLocalReadingTime(time_t now, struct tm &localTime);
String getCurrentTimestamp();
float readGasPpm(float temperature, float humidity);
void buildMeasurementJson(JsonDocument &document, const String &timestamp, float temperature,
                          float humidity, float gasPpm);
void sampleAndPublish();
void updateDisplay(float temperature, float humidity, float gasPpm);

// Respaldo CSV y recuperación al restablecer la conexión.
String formatCsvNumber(float value);
void saveMeasurementCsv(const String &timestamp, float temperature, float humidity, float gasPpm);
void publishPendingCsv();

// Arranque de Arduino y recuperación de la posición confirmada de la cola.
void setup() {
    Serial.begin(115200);

    Serial.println("Iniciando pantalla ST7789...");
    Serial.printf("TFT: MOSI=%d, SCLK=%d, CS=%d, DC=%d, RST=%d\n", TFT_MOSI, TFT_SCLK, TFT_CS,
                  TFT_DC, TFT_RST);
    display.init();
    Serial.println("Inicializacion TFT terminada");
    display.setRotation(1);
    display.fillScreen(TFT_BLACK);
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.setTextSize(2);
    display.println("Iniciando...");

    // El montaje sin formateo conserva las lecturas pendientes si falla LittleFS.
    if (!LittleFS.begin(false)) {
        Serial.println("ERROR: no se pudo montar LittleFS");
        display.setTextColor(TFT_RED, TFT_BLACK);
        display.println("Error LittleFS");
        return;
    }
    if (!loadConfiguration()) {
        return;
    }

    if (!queuePreferences.begin("csv-queue", false)) {
        Serial.println("ERROR: no se pudo abrir el estado del CSV");
        return;
    }
    confirmedCsvOffset = queuePreferences.getULong("offset", 0);

    dhtSensor.begin();
    analogReadResolution(12);
    analogSetPinAttenuation(PIN_MQ135, ADC_11db);

    WiFi.mode(WIFI_STA);
    WiFi.begin(configuration["SSID"].as<const char *>(), configuration["PASS"].as<const char *>());
    configTime(-6 * 3600, 0, "pool.ntp.org", "time.nist.gov");

    mqttClient.setServer(configuration["MQTT_HOST"].as<const char *>(),
                         configuration["MQTT_PORT"] | 1883);
    mqttClient.setBufferSize(1024);
    mqttClient.setSocketTimeout(1);
    mqttClient.setCallback(onMqttMessage);

    drawDisplay();
    updateDisplay(NAN, NAN, NAN);
    isInitialized = true;
}

// El ciclo atiende MQTT y la pantalla entre cada toma de muestras.
void loop() {
    if (!isInitialized) {
        delay(10);
        return;
    }

    maintainMqttConnection();
    mqttClient.loop();

    if (millis() - lastSampleAt >= SAMPLE_INTERVAL_MS) {
        lastSampleAt = millis();
        sampleAndPublish();
    }

    publishPendingCsv();
    updateClock();
    updateMqttStatus();
    delay(2);
}

// Configuración almacenada en flash y manejo de la conexión MQTT.
bool loadConfiguration() {
    File file = LittleFS.open("/config.json", "r");
    if (!file || deserializeJson(configuration, file) ||
        !configuration["SSID"].is<const char *>() || !configuration["PASS"].is<const char *>() ||
        !configuration["MQTT_HOST"].is<const char *>()) {
        Serial.println("Revisar config.json: SSID, PASS, MQTT_HOST");
        display.println("Error config.json");
        return false;
    }
    return true;
}

void maintainMqttConnection() {
    const bool connected = mqttClient.connected();
    if (wasMqttConnected && !connected) {
        Serial.println("Evento: MQTT desconectado; guardando CSV");
    }
    wasMqttConnected = connected;
    if (connected || millis() - lastConnectionAttemptAt < CONNECTION_RETRY_INTERVAL_MS) {
        return;
    }
    lastConnectionAttemptAt = millis();
    if (WiFi.status() != WL_CONNECTED) {
        WiFi.reconnect();
        return;
    }
    if (mqttClient.connect(DEVICE_ID, statusTopic.c_str(), 1, true, "desconectado")) {
        if (!mqttClient.subscribe(acknowledgmentTopic.c_str(), 1)) {
            mqttClient.disconnect();
            return;
        }
        mqttClient.publish(statusTopic.c_str(), "conectado", true);
        Serial.println("Evento: MQTT conectado");
        wasMqttConnected = true;
    }
}

void onMqttMessage(char *topic, byte *payload, unsigned int length) {
    if (String(topic) != acknowledgmentTopic || pendingRecordId.isEmpty()) {
        return;
    }
    String recordId;
    for (unsigned int i = 0; i < length; ++i) {
        recordId += char(payload[i]);
    }
    // Tras el ACK, el cursor persistido permite continuar el envío después de un reinicio.
    if (recordId == pendingRecordId &&
        queuePreferences.putULong("offset", pendingCsvEndOffset) == sizeof(uint32_t)) {
        confirmedCsvOffset = pendingCsvEndOffset;
        pendingRecordId = "";
    }
}

// El timestamp queda vacío hasta que el reloj tiene una fecha válida.
String getCurrentTimestamp() {
    struct tm localTime;
    if (!getLocalReadingTime(time(nullptr), localTime)) {
        return "";
    }
    char buffer[40];
    strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S-06:00", &localTime);
    return String(buffer);
}

bool getLocalReadingTime(time_t now, struct tm &localTime) {
    return now >= EARLIEST_VALID_TIME && localtime_r(&now, &localTime) != nullptr;
}

// Contrato de telemetría seguido por el consumidor MQTT.
void buildMeasurementJson(JsonDocument &document, const String &timestamp, float temperature,
                          float humidity, float gasPpm) {
    document["device_id"] = DEVICE_ID;
    if (timestamp.isEmpty()) {
        document["timestamp"] = nullptr;
    } else {
        document["timestamp"] = timestamp;
    }
    JsonObject measurements = document["measurements"].to<JsonObject>();
    constexpr const char *names[METRIC_COUNT] = {"temperature", "humidity", "gas"};
    constexpr const char *units[METRIC_COUNT] = {"°C", "%RH", "ppm"};
    const float values[METRIC_COUNT] = {temperature, humidity, gasPpm};

    for (int metric = 0; metric < METRIC_COUNT; ++metric) {
        JsonObject measurement = measurements[names[metric]].to<JsonObject>();
        measurement["sensor"] = metric == GAS ? "MQ135" : "DHT11";
        if (isfinite(values[metric])) {
            measurement["value"] = values[metric];
        } else {
            measurement["value"] = nullptr;
        }
        measurement["unit"] = units[metric];
    }
    measurements["gas"]["calibrate"] = false;
}

void sampleAndPublish() {
    const float temperature = dhtSensor.readTemperature();
    const float humidity = dhtSensor.readHumidity();
    const float gasPpm = readGasPpm(temperature, humidity);
    const String timestamp = getCurrentTimestamp();

    JsonDocument document;
    buildMeasurementJson(document, timestamp, temperature, humidity, gasPpm);
    String json;
    serializeJson(document, json);

    // El respaldo local conserva la muestra cuando MQTT no puede enviarla.
    if (!mqttClient.connected() || !mqttClient.publish(processedTopic.c_str(), json.c_str())) {
        saveMeasurementCsv(timestamp, temperature, humidity, gasPpm);
    }

    updateDisplay(temperature, humidity, gasPpm);
}

// Actualización de las lecturas sin volver a dibujar las tarjetas completas.
void updateDisplay(float temperature, float humidity, float gasPpm) {
    static String previousValues[METRIC_COUNT];
    static String previousGasStatus;
    const float readings[METRIC_COUNT] = {temperature, humidity, gasPpm};
    const int valueRight = display.width() - DISPLAY_MARGIN - 50;

    display.startWrite();
    display.setTextColor(TFT_WHITE, CARD_BG);
    display.setTextDatum(MR_DATUM);
    display.setTextPadding(VALUE_WIDTH);
    for (int metric = 0; metric < METRIC_COUNT; ++metric) {
        const float reading = readings[metric];
        String value = "--";
        if (isfinite(reading)) {
            if (metric == GAS && reading >= 99999.5f) {
                char scientific[16];
                snprintf(scientific, sizeof(scientific), "%.0e", static_cast<double>(reading));
                value = scientific;
            } else {
                value = String(reading, metric == GAS ? 0 : 1);
            }
        }
        if (value != previousValues[metric]) {
            display.drawString(value, valueRight, cardCenter(metric), 4);
            previousValues[metric] = value;
        }
    }

    if (gasSensorStatus != previousGasStatus) {
        display.setTextColor(SECONDARY_TEXT, CARD_BG);
        display.setTextDatum(ML_DATUM);
        display.setTextPadding(valueRight - VALUE_WIDTH - METRIC_LABEL_X);
        display.drawString(gasSensorStatus, METRIC_LABEL_X, cardCenter(GAS) + 8, 2);
        previousGasStatus = gasSensorStatus;
    }
    display.endWrite();
}

namespace {
// Dibujo de las tarjetas, el reloj y el pie de estado MQTT.
int cardHeight() {
    return (display.height() - FIRST_CARD_Y - FOOTER_HEIGHT - 3 * CARD_GAP) / METRIC_COUNT;
}

int cardCenter(int metric) {
    return FIRST_CARD_Y + metric * (cardHeight() + CARD_GAP) + cardHeight() / 2;
}

void drawMetricIcon(int metric, int x, int y) {
    const uint16_t color = METRICS[metric].color;
    display.fillCircle(x, y, 15, ICON_BG);
    switch (metric) {
    case TEMPERATURE:
        display.fillRoundRect(x - 4, y - 11, 9, 19, 4, color);
        display.fillCircle(x, y + 6, 7, color);
        display.fillRoundRect(x - 2, y - 9, 5, 15, 2, ICON_BG);
        display.fillCircle(x, y + 6, 5, ICON_BG);
        display.drawFastVLine(x, y - 5, 11, color);
        display.fillCircle(x, y + 6, 3, color);
        break;
    case HUMIDITY:
        display.fillTriangle(x, y - 11, x - 7, y + 2, x + 7, y + 2, color);
        display.fillCircle(x, y + 2, 7, color);
        display.fillTriangle(x, y - 7, x - 5, y + 2, x + 5, y + 2, ICON_BG);
        display.fillCircle(x, y + 2, 5, ICON_BG);
        display.drawFastHLine(x - 2, y + 5, 3, color);
        break;
    case GAS:
        display.drawFastHLine(x - 9, y - 5, 16, color);
        display.drawFastHLine(x - 11, y, 22, color);
        display.drawFastHLine(x - 7, y + 5, 14, color);
        display.fillCircle(x + 7, y - 5, 2, color);
        display.fillCircle(x + 10, y, 2, color);
        display.fillCircle(x + 6, y + 5, 2, color);
        break;
    }
}

void drawDisplay() {
    const int panelWidth = display.width() - 2 * DISPLAY_MARGIN;
    const int right = display.width() - DISPLAY_MARGIN;
    const int clockX = right - 116;

    display.startWrite();
    display.fillScreen(SCREEN_BG);
    display.setTextSize(1);
    display.setTextPadding(0);
    display.setTextDatum(TL_DATUM);
    display.setTextColor(MINT, SCREEN_BG);
    display.drawString("MONITOR DE AIRE", DISPLAY_MARGIN, 13, 2);
    display.setTextColor(SECONDARY_TEXT, SCREEN_BG);
    display.drawString(DEVICE_ID, DISPLAY_MARGIN, 32, 2);

    display.drawCircle(clockX, 34, 10, MINT);
    display.drawFastVLine(clockX, 28, 7, MINT);
    display.drawLine(clockX, 34, clockX + 5, 37, MINT);
    display.fillCircle(clockX, 34, 1, MINT);
    display.drawFastHLine(DISPLAY_MARGIN, 56, panelWidth, ICON_BG);

    for (int metric = 0; metric < METRIC_COUNT; ++metric) {
        const int center = cardCenter(metric);
        display.fillRoundRect(DISPLAY_MARGIN, center - cardHeight() / 2, panelWidth, cardHeight(),
                              10, CARD_BG);
        drawMetricIcon(metric, DISPLAY_MARGIN + 24, center);
        display.setTextColor(TFT_WHITE, CARD_BG);
        display.setTextDatum(ML_DATUM);
        display.drawString(METRICS[metric].label, METRIC_LABEL_X, center - (metric == GAS ? 8 : 0),
                           2);

        const int unitRight = right - 12;
        display.setTextColor(METRICS[metric].color, CARD_BG);
        display.setTextDatum(MR_DATUM);
        display.drawString(METRICS[metric].unit, unitRight, center, 2);
        if (metric == TEMPERATURE) {
            display.drawCircle(unitRight - display.textWidth("C", 2) - 3, center - 4, 2,
                               METRICS[metric].color);
        }
    }

    display.drawFastHLine(DISPLAY_MARGIN, display.height() - FOOTER_HEIGHT, panelWidth, ICON_BG);
    display.endWrite();
}

void updateClock() {
    static time_t displayedSecond = -1;
    static bool wasSynced = false;
    const time_t now = time(nullptr);
    const time_t clockSecond = now >= EARLIEST_VALID_TIME ? now : 0;
    if (clockSecond == displayedSecond) {
        return;
    }

    struct tm localTime;
    char clock[] = "--:--:--";
    const bool synced = getLocalReadingTime(clockSecond, localTime);
    if (synced) {
        strftime(clock, sizeof(clock), "%H:%M:%S", &localTime);
    }
    const int right = display.width() - DISPLAY_MARGIN;

    display.startWrite();
    display.setTextDatum(TR_DATUM);
    display.setTextPadding(102);
    if (displayedSecond < 0 || synced != wasSynced) {
        display.setTextColor(SECONDARY_TEXT, SCREEN_BG);
        display.drawString(synced ? "HORA LOCAL" : "ESPERANDO NTP", right, 6, 2);
        wasSynced = synced;
    }
    display.setTextColor(TFT_WHITE, SCREEN_BG);
    display.drawString(clock, right, 23, 4);
    display.endWrite();
    displayedSecond = clockSecond;
}

void updateMqttStatus() {
    static bool drawn = false;
    static bool previousConnection = false;
    if (drawn && wasMqttConnected == previousConnection) {
        return;
    }

    const int center = display.height() - FOOTER_HEIGHT / 2;
    const uint16_t color = wasMqttConnected ? MINT : ORANGE;
    display.startWrite();
    display.fillCircle(DISPLAY_MARGIN + 5, center, 4, color);
    display.setTextColor(color, SCREEN_BG);
    display.setTextDatum(ML_DATUM);
    display.setTextPadding(display.width() - 2 * DISPLAY_MARGIN - 18);
    display.drawString(wasMqttConnected ? "Broker MQTT conectado" : "Broker MQTT desconectado",
                       DISPLAY_MARGIN + 18, center, 2);
    display.endWrite();
    previousConnection = wasMqttConnected;
    drawn = true;
}
} // namespace

// Compensación ambiental y estimación de gas a partir del voltaje del MQ135.
float readGasPpm(float temperature, float humidity) {
    if (!isfinite(temperature) || !isfinite(humidity) || humidity < 0 || humidity > 100) {
        return NAN;
    }

    // Promedia muestras del ADC y convierte el valor de milivoltios a voltios.
    float adcVoltage = 0;
    for (int i = 0; i < ADC_SAMPLE_COUNT; ++i) {
        adcVoltage += analogReadMilliVolts(PIN_MQ135);
        delay(2);
    }
    adcVoltage /= ADC_SAMPLE_COUNT * 1000.0f;
    const float sensorOutputVoltage = adcVoltage * 2; // Divisor externo 10k / 10k.
    if (adcVoltage < 0.15f || adcVoltage >= 3 || sensorOutputVoltage >= MQ_SUPPLY_VOLTAGE) {
        gasSensorStatus = "Error ADC";
        return NAN;
    }
    const float environmentalCorrection = 0.00035f * temperature * temperature -
                                          0.02718f * temperature + 1.39538f -
                                          0.0018f * (humidity - 33);
    // La carga efectiva incluye el divisor externo en paralelo con RL (2 kOhm).
    const float effectiveLoadResistance = (2.0f * 20.0f) / 22.0f;
    const float correctedResistance = (MQ_SUPPLY_VOLTAGE / sensorOutputVoltage - 1) *
                                      effectiveLoadResistance / environmentalCorrection;
    if (!isfinite(correctedResistance) || correctedResistance <= 0 ||
        environmentalCorrection <= 0) {
        return NAN;
    }

    // Estima R0 con 30 muestras y una referencia supuesta de 450 ppm.
    if (referenceCount < REFERENCE_SAMPLE_COUNT) {
        referenceResistanceSum += correctedResistance;
        ++referenceCount;
        gasSensorStatus =
            "Referencia " + String(referenceCount) + "/" + String(REFERENCE_SAMPLE_COUNT);
        if (referenceCount < REFERENCE_SAMPLE_COUNT) {
            return NAN;
        }
        referenceResistance = (referenceResistanceSum / REFERENCE_SAMPLE_COUNT) /
                              powf(MQ_REFERENCE_PPM / MQ_CURVE_A, 1.0f / MQ_CURVE_B);
    }
    const float ppm = MQ_CURVE_A * powf(correctedResistance / referenceResistance, MQ_CURVE_B);
    gasSensorStatus = "PPM estimadas";
    return isfinite(ppm) && ppm > 0 ? ppm : NAN;
}

// Respaldo CSV y reenvío de registros hasta recibir su ACK.
String formatCsvNumber(float value) {
    return isfinite(value) ? String(value, 2) : "";
}

void saveMeasurementCsv(const String &timestamp, float temperature, float humidity, float gasPpm) {
    if (LittleFS.totalBytes() - LittleFS.usedBytes() < MIN_FREE_STORAGE_BYTES) {
        Serial.println("ERROR: almacenamiento lleno; lectura no guardada");
        return;
    }

    const bool isNewFile = !LittleFS.exists(CSV_PATH);
    if (isNewFile) {
        if (queuePreferences.putULong("offset", 0) != sizeof(uint32_t)) {
            Serial.println("ERROR: no se pudo reiniciar el estado del CSV");
            return;
        }
        confirmedCsvOffset = 0;
    }

    char recordId[24];
    snprintf(recordId, sizeof(recordId), "%08lx%08lx", static_cast<unsigned long>(esp_random()),
             static_cast<unsigned long>(esp_random()));
    const String row = String(recordId) + "," + DEVICE_ID + "," + timestamp + "," +
                       formatCsvNumber(temperature) + "," + formatCsvNumber(humidity) + "," +
                       formatCsvNumber(gasPpm) + ",false\n";
    File file = LittleFS.open(CSV_PATH, "a");
    if (!file) {
        Serial.println("ERROR: no se pudo abrir el CSV");
        return;
    }

    const bool headerWritten = file.size() != 0 || file.print(CSV_HEADER) == strlen(CSV_HEADER);
    if (!headerWritten || file.print(row) != row.length()) {
        Serial.println("ERROR: escritura incompleta del CSV");
    }
    file.close();
}

void publishPendingCsv() {
    if (!mqttClient.connected() || millis() - lastReplayAt < CSV_RETRY_INTERVAL_MS) {
        return;
    }
    lastReplayAt = millis();

    File file = LittleFS.open(CSV_PATH, "r");
    if (!file) {
        return;
    }

    // Al confirmar todas las filas, el archivo completo puede borrarse.
    if (confirmedCsvOffset >= file.size()) {
        file.close();
        if (LittleFS.remove(CSV_PATH)) {
            confirmedCsvOffset = 0;
            pendingRecordId = "";
            Serial.println("CSV confirmado y eliminado");
        }
        return;
    }

    if (confirmedCsvOffset == 0) {
        file.readStringUntil('\n'); // La primera fila del archivo es la cabecera.
    } else if (!file.seek(confirmedCsvOffset)) {
        Serial.println("ERROR: no se pudo leer la posicion del CSV");
        return;
    }

    const String row = file.readStringUntil('\n');
    const int separator = row.indexOf(',');
    if (separator <= 0) {
        Serial.println("ERROR: fila CSV incompleta; archivo conservado");
        return;
    }

    pendingRecordId = row.substring(0, separator);
    pendingCsvEndOffset = file.position();
    const String payload = String(CSV_HEADER) + row + "\n";
    mqttClient.publish(rawTopic.c_str(), payload.c_str());
}
