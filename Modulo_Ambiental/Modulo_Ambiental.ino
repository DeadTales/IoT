// Estimacion de PPM basada en una curva de CO2.
// MQ_REF_PPM es una concentracion SUPUESTA para el aire de referencia.
// El MQ135 responde a varios gases: no es una medicion selectiva de CO2.
// Tomar referencia solo con el sensor estabilizado. RESET toma una nueva.

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <MQUnifiedsensor.h>
#include <math.h>

#define PIN_DHT 12
#define PIN_MQ135 10
#define SDA 8
#define SCL 9
#define DHT_TYPE DHT11

#define TEXT_TEMP "Temp: "
#define TEXT_HUM "Hum: "
#define TEXT_GAS "Gas: "

#define UNIT_TEMP "C"
#define UNIT_HUM "%RH"
#define UNIT_GAS "PPM"

#define MQ_VCC 5.0f
#define MQ_RL 2.0f       // Resistencia 202 del modulo: 2 kOhm.
#define MQ_R_SUP 10.0f   // Divisor externo: AO hacia GPIO10.
#define MQ_R_INF 10.0f   // Divisor externo: GPIO10 hacia GND.
#define MQ_REF_SAMPLES 30
#define MQ_REF_PPM 450.0f  // SUPUESTO para la referencia, no medido.
#define MQ_A 110.47f      // Curva aproximada de CO2 del ejemplo MQUnifiedsensor.
#define MQ_B -2.862f

unsigned long lastPeriod = 0;
const unsigned long period = 2000;

float mqReference = 0;
float mqR0 = 0;
float mqReferenceSum = 0;
int mqReferenceCount = 0;
String mqStatus = "Tomando referencia";

DHT dht11(PIN_DHT, DHT_TYPE);
MQUnifiedsensor mq135("ESP32-S3", MQ_VCC, 12, PIN_MQ135, "MQ-135");
LiquidCrystal_I2C lcd(0x27, 20, 4);

void printText(LiquidCrystal_I2C& lcd, int col = 0, int row = 0, String text = "");
String formatTwoDigits(float value);
String formatSixDigits(float value);
void setupMQ135();
float readMQ135(float temp, float hum);
float correctionMQ135(float temp, float hum);

void setup() {
  Serial.begin(115200);
  Wire.begin(SDA, SCL);

  lcd.init();
  lcd.backlight();

  printText(lcd, 0, 0, TEXT_TEMP);
  printText(lcd, 0, 1, TEXT_HUM);
  printText(lcd, 0, 2, TEXT_GAS);

  dht11.begin();
  setupMQ135();
}

void loop() {
  unsigned long presentPeriod = millis();

  if (presentPeriod - lastPeriod >= period) {
    lastPeriod = presentPeriod;
    int contCol = 0;

    float temp = dht11.readTemperature();
    float hum = dht11.readHumidity();
    float gas = readMQ135(temp, hum);

    String textTemp = formatTwoDigits(temp) + UNIT_TEMP;
    String textHum = formatTwoDigits(hum) + UNIT_HUM;
    String textGas = formatSixDigits(gas) + UNIT_GAS;

    printText(lcd, strlen(TEXT_TEMP) + 1, contCol++, textTemp);
    printText(lcd, strlen(TEXT_HUM) + 1, contCol++, textHum);
    printText(lcd, strlen(TEXT_GAS) + 1, contCol, textGas);
    printText(lcd, 0, 3, mqStatus);
  }
}

void setupMQ135() {
  mq135.init();
  analogReadResolution(12);
  analogSetPinAttenuation(PIN_MQ135, ADC_11db);

  float rlDivider = MQ_R_SUP + MQ_R_INF;
  float rlEffective = (MQ_RL * rlDivider) / (MQ_RL + rlDivider);
  mq135.setRL(rlEffective);
  mq135.setRegressionMethod(1);
  mq135.setA(MQ_A);
  mq135.setB(MQ_B);
}

float correctionMQ135(float temp, float hum) {
  // Modelo ambiental empirico aproximado de la biblioteca MQ135 de GeorgK.
  return 0.00035f * temp * temp - 0.02718f * temp + 1.39538f
         - 0.0018f * (hum - 33.0f);
}

float readMQ135(float temp, float hum) {
  if (!isfinite(temp) || !isfinite(hum) || hum < 0 || hum > 100) {
    mqStatus = "Error DHT11";
    return NAN;
  }

  float voltage = 0;
  for (int i = 0; i < 32; i++) {
    voltage += analogReadMilliVolts(PIN_MQ135);
    delay(2);
  }
  voltage /= 32000.0f;

  float voltageAO = voltage * (MQ_R_SUP + MQ_R_INF) / MQ_R_INF;
  if (voltage < 0.15f || voltage >= 3.0f || voltageAO >= MQ_VCC) {
    mqStatus = "Error lectura ADC";
    return NAN;
  }

  mq135.externalADCUpdate(voltageAO);
  // No llamar mq135.update(): sobrescribiria el voltaje reconstruido.
  float rs = mq135.getRS();
  float factor = correctionMQ135(temp, hum);

  if (!isfinite(rs) || rs <= 0 || !isfinite(factor) || factor <= 0) {
    mqStatus = "Error calculo MQ135";
    return NAN;
  }

  float correctedRs = rs / factor;

  // Promedia 30 lecturas compensadas en el ambiente de referencia.
  // Se vuelve a tomar en cada encendido o RESET.
  if (mqReferenceCount < MQ_REF_SAMPLES) {
    mqReferenceSum += correctedRs;
    mqReferenceCount++;
    mqStatus = "Referencia: " + String(mqReferenceCount) + "/" + String(MQ_REF_SAMPLES);

    if (mqReferenceCount < MQ_REF_SAMPLES) return NAN;

    mqReference = mqReferenceSum / MQ_REF_SAMPLES;
    // PPM = A * (Rs corregida / R0)^B.
    // Despeja R0 suponiendo MQ_REF_PPM durante la referencia.
    mqR0 = mqReference / powf(MQ_REF_PPM / MQ_A, 1.0f / MQ_B);

    Serial.print("R0 estimada (kOhm): ");
    Serial.println(mqR0, 4);
    Serial.print("PPM SUPUESTAS para la referencia: ");
    Serial.println(MQ_REF_PPM, 1);
  }

  if (!isfinite(mqR0) || mqR0 <= 0) {
    mqStatus = "Error referencia R0";
    return NAN;
  }

  // readSensor() usa Rs sin compensar. Aplicar R0 * factor equivale a
  // calcular la relacion (Rs / factor) / R0, sin cambiar la biblioteca.
  mq135.setR0(mqR0 * factor);
  float ppm = mq135.readSensor();

  if (!isfinite(ppm) || ppm <= 0) {
    mqStatus = "Error calculo PPM";
    return NAN;
  }

  mqStatus = "PPM estimadas";
  return ppm;
}

void printText(LiquidCrystal_I2C& lcd, int col, int row, String text) {
  text = text.substring(0, 20 - col);
  lcd.setCursor(col, row);
  lcd.print(text);

  // Borra los caracteres que sobren de la lectura anterior.
  for (int i = col + text.length(); i < 20; i++) lcd.print(' ');
}

String formatTwoDigits(float value) {
  if (!isfinite(value)) return "--";

  char buffer[24];
  snprintf(buffer, sizeof(buffer), "%02.2f", value);
  return String(buffer);
}

String formatSixDigits(float value) {
  if (!isfinite(value)) return "--";

  char buffer[24];
  snprintf(buffer, sizeof(buffer), "%06.2f", value);
  return String(buffer);
}
