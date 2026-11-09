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

// Hardware y tiempos de operacion.
constexpr uint8_t PIN_DHT = 4;
constexpr uint8_t PIN_MQ135 = 5;
constexpr unsigned long SAMPLE_INTERVAL_MS = 2000;
constexpr unsigned long CONNECTION_RETRY_INTERVAL_MS = 5000;
constexpr unsigned long CSV_RETRY_INTERVAL_MS = 5000;
constexpr size_t MIN_FREE_STORAGE_BYTES = 8192;
constexpr unsigned int ADC_SAMPLE_COUNT = 32;
constexpr unsigned int REFERENCE_SAMPLE_COUNT = 30;
constexpr float MQ_SUPPLY_VOLTAGE = 5.0f;
constexpr float MQ_REFERENCE_PPM = 450.0f;
constexpr float MQ_CURVE_A = 110.47f;
constexpr float MQ_CURVE_B = -2.862f;
constexpr char DEVICE_ID[] = "ESP32_S3_0001";
constexpr char CSV_PATH[] = "/pending.csv";
constexpr char CSV_HEADER[] = "id,device_id,timestamp,temperature,humidity,gas,calibrate\n";

// Perifericos y configuracion.
TFT_eSPI display;
DHT dhtSensor(PIN_DHT, DHT11);
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
JsonDocument configuration;
Preferences queuePreferences;

// Topics especificos de este dispositivo.
const String deviceTopicPrefix = String("dispositivo/") + DEVICE_ID;
const String rawTopic = deviceTopicPrefix + "/crudo";
const String processedTopic = deviceTopicPrefix + "/tratado";
const String acknowledgmentTopic = deviceTopicPrefix + "/ack";
const String statusTopic = deviceTopicPrefix + "/estado";

// Estado de conexion, reenvio y referencia del sensor.
bool isInitialized = false;
bool wasMqttConnected = false;
unsigned long lastSampleAt = 0;
unsigned long lastConnectionAttemptAt = 0;
unsigned long lastReplayAt = 0;
size_t confirmedCsvOffset = 0;
size_t pendingCsvEndOffset = 0;
String pendingRecordId;
float referenceResistanceSum = 0;
float referenceResistance = 0;
unsigned referenceCount = 0;
String gasSensorStatus = "Tomando referencia";

// Prototipos: todas las implementaciones estan debajo de loop().
bool loadConfiguration();
void maintainMqttConnection();
void onMqttMessage(char *topic, byte *payload, unsigned int length);
void sampleAndPublish();
void updateDisplay(float temperature, float humidity, float gasPpm);
float readGasPpm(float temperature, float humidity);
String getCurrentTimestamp();
String formatCsvNumber(float value);
void buildMeasurementJson(JsonDocument &document, const String &timestamp, float temperature,
                          float humidity, float gasPpm);
void saveMeasurementCsv(const String &timestamp, float temperature, float humidity, float gasPpm);
void publishPendingCsv();

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

    // No formatear automaticamente: conserva los registros ante errores.
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

    isInitialized = true;
}

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
    delay(2);
}

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
    // Persistir el avance solo tras la confirmacion del conversor.
    if (recordId == pendingRecordId &&
        queuePreferences.putULong("offset", pendingCsvEndOffset) == sizeof(uint32_t)) {
        confirmedCsvOffset = pendingCsvEndOffset;
        pendingRecordId = "";
    }
}

String getCurrentTimestamp() {
    time_t now = time(nullptr);
    if (now < 1700000000) {
        return "";
    }
    struct tm localTime;
    localtime_r(&now, &localTime);
    char buffer[40];
    strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S-06:00", &localTime);
    return String(buffer);
}

String formatCsvNumber(float value) {
    return isfinite(value) ? String(value, 2) : "";
}

void buildMeasurementJson(JsonDocument &document, const String &timestamp, float temperature,
                          float humidity, float gasPpm) {
    document["device_id"] = DEVICE_ID;
    if (timestamp.isEmpty()) {
        document["timestamp"] = nullptr;
    } else {
        document["timestamp"] = timestamp;
    }
    JsonObject measurements = document["measurements"].to<JsonObject>();
    const char *names[] = {"temperature", "humidity", "gas"};
    const char *units[] = {"°C", "%RH", "ppm"};
    float values[] = {temperature, humidity, gasPpm};
    for (int i = 0; i < 3; ++i) {
        JsonObject measurement = measurements[names[i]].to<JsonObject>();
        measurement["sensor"] = i == 2 ? "MQ135" : "DHT11";
        if (isfinite(values[i])) {
            measurement["value"] = values[i];
        } else {
            measurement["value"] = nullptr;
        }
        measurement["unit"] = units[i];
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
    if (!mqttClient.connected() || !mqttClient.publish(processedTopic.c_str(), json.c_str())) {
        saveMeasurementCsv(timestamp, temperature, humidity, gasPpm);
    }

    updateDisplay(temperature, humidity, gasPpm);
}

void updateDisplay(float temperature, float humidity, float gasPpm) {
    const String lines[] = {
        "Temp: " + formatCsvNumber(temperature) + " C",
        "Hum: " + formatCsvNumber(humidity) + " %RH",
        "Gas: " + formatCsvNumber(gasPpm) + " ppm",
        gasSensorStatus,
        mqttClient.connected() ? "MQTT conectado" : "MQTT desconectado",
    };
    constexpr size_t LINE_COUNT = sizeof(lines) / sizeof(lines[0]);
    static String previousLines[LINE_COUNT];

    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.setTextDatum(TL_DATUM);
    display.setTextPadding(display.width());
    const int lineHeight = display.fontHeight();

    for (size_t line = 0; line < LINE_COUNT; ++line) {
        if (lines[line] == previousLines[line]) {
            continue;
        }

        // El fondo y el padding borran el texto anterior sin limpiar toda la pantalla.
        display.drawString(lines[line], 0, line * lineHeight);
        previousLines[line] = lines[line];
    }
}

float readGasPpm(float temperature, float humidity) {
    if (!isfinite(temperature) || !isfinite(humidity) || humidity < 0 || humidity > 100) {
        return NAN;
    }
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
    // Estimar R0 a partir de 30 muestras y una referencia supuesta de 450 ppm.
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
    snprintf(recordId, sizeof(recordId), "%08lx%08lx", (unsigned long)esp_random(),
             (unsigned long)esp_random());
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

    // El archivo se borra al confirmar todas sus filas; no se reescribe por cada ACK.
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
        file.readStringUntil('\n'); // Omitir la cabecera una sola vez.
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