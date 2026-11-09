# Monitor de aire ESP32-S3

Cada 2 segundos publica el JSON en `dispositivo/ESP32_S3_0001/tratado`.
Si MQTT esta desconectado o falla publish, agrega las lecturas a un unico archivo
en LittleFS (`/pending.csv`). Al reconectar publica una fila por mensaje en
`dispositivo/ESP32_S3_0001/crudo`. Python escucha `dispositivo/+/crudo`,
convierte al mismo JSON y publica en `dispositivo/<ID>/tratado` con QoS 1.
Tras la confirmacion del broker, Python publica el ID del registro en
`dispositivo/<ID>/ack`. El ESP32 guarda el avance confirmado en NVS para
continuar despues de reiniciar. Las filas confirmadas permanecen fisicamente
en el archivo hasta confirmar todas las filas; entonces se elimina el CSV
completo. Si se pierde el ACK se reintenta la misma fila.
Los reintentos pueden producir duplicados. Las publicaciones actuales usan
QoS 0 (limitacion de PubSubClient): publish exitoso no garantiza recepcion.
Las filas del historial se reenvian en el orden en que se guardaron; las
lecturas actuales siguen publicandose directamente mientras se envia el historial.

`dispositivo/<ID>/estado` conserva `conectado` y usa Last Will para indicar
`desconectado` tras perder la conexion. Serial informa las transiciones.
El comodin `+` se utiliza al suscribirse, nunca al publicar.

## Configuracion y ejecucion

1. En `data/config.json` configura SSID, PASS, MQTT_HOST y MQTT_PORT.
   PASS corresponde exclusivamente a la clave de tu red WiFi.
   MQTT_HOST contiene una IP de ejemplo: reemplazala por la IP LAN de tu PC.
   En ESP32, localhost apuntaria al propio ESP32.
2. Configura tu broker para aceptar conexiones anonimas desde la LAN.
   El firmware y Python se conectan sin usuario ni contrasena MQTT, mediante TCP sin TLS.
3. Ejecuta `pio run -t uploadfs` para cargar config.json y `pio run -t upload`
   para cargar firmware. Subir LittleFS de nuevo reemplaza los CSV existentes.
4. En la computadora del broker:

```bash
python -m venv .venv
source .venv/bin/activate
pip install -r scripts/requirements.txt
python scripts/csv_to_json.py --host localhost --port 1883
```

Para observar el JSON, suscribete a `dispositivo/+/tratado`.
Cambia DEVICE_ID en main.cpp para cada placa. No ejecutes multiples conversores
simultaneos sobre la misma suscripcion porque duplicarian las publicaciones.

## Pines

| Parte | Senal | GPIO ESP32-S3 |
|---|---|---|
| ST7789 | MOSI / SDA | 13 |
| ST7789 | SCLK / SCL | 12 |
| ST7789 | CS | 10 |
| ST7789 | DC | 9 |
| ST7789 | RST | 8 |
| ST7789 | MISO | Sin conectar (-1) |
| DHT11 | DATA | 4 |
| MQ135 | AO mediante divisor | 5 (ADC1) |

Los pines y opciones TFT originales estan copiados a `include/TFT_Setup.h`
y se cargan desde platformio.ini para conservarlos al reinstalar TFT_eSPI.
No se asigna GPIO de retroiluminacion; conserva tu conexion BL actual.
Pantalla: VCC segun tu modulo, GND comun. DHT11: VCC 3.3 V, GND comun,
y pull-up de DATA a 3.3 V si el modulo no lo incorpora.
MQ135: VCC 5 V y GND comun. AO -> resistencia 10 kOhm -> GPIO5;
GPIO5 -> resistencia 10 kOhm -> GND. No conectar AO de 5 V directo al GPIO.
La entrada digital DO del MQ135 queda sin conectar.

## Mediciones y limites

JSON: device_id, timestamp y measurements.temperature/humidity/gas;
cada medicion incluye sensor, value y unit. Gas incluye calibrate=false.
Los valores invalidos son null en JSON y campos vacios en CSV.
Se usa hora NTP con desplazamiento fijo UTC-06:00; antes de sincronizar,
timestamp es null. No se inventa una fecha para lecturas sin hora conocida.

La curva MQ135 conserva los parametros del codigo de referencia: RL 2 kOhm,
divisor 10k/10k, compensacion ambiental, 30 muestras de referencia y supuesto
de 450 ppm. Requiere sensor estabilizado antes de tomar referencia; se repite
al reiniciar. Son ppm estimadas, no una medicion selectiva de CO2 ni una
calibracion certificada, por lo que calibrate permanece false.

LittleFS no se formatea automaticamente. Si falta, inicializalo con uploadfs.
Cuando queda poco espacio se informa por Serial que no se guardo la lectura.
Una escritura interrumpida puede dejar una fila incompleta: el conversor la
rechaza sin confirmar y el archivo se conserva para revision. Los archivos
/queue de la version anterior no se eliminan ni se migran automaticamente.
Las funciones tienen prototipos antes de setup e implementaciones bajo loop.

El firmware principal usa HSPI a 20 MHz, con los pines de include/TFT_Setup.h.
Para cargar el monitor completo:

```bash
pio run -e esp32-s3-devkitc-1 -t upload
```
