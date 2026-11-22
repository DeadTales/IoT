"""Constantes de transporte y contrato de mediciones."""

# Parámetros del cliente y límites de procesamiento.
RAW_TOPIC_FILTER = "dispositivo/+/crudo"
MESSAGE_QUEUE_SIZE = 100
PUBLISH_TIMEOUT_SECONDS = 10
MQTT_KEEPALIVE_SECONDS = 60

# Columnas en el orden enviado por el firmware.
CSV_COLUMNS = (
    "id",
    "device_id",
    "timestamp",
    "temperature",
    "humidity",
    "gas",
    "calibrate",
)

# Cada medición especifica su clave JSON, sensor y unidad.
MEASUREMENT_FIELDS = (
    ("temperature", "DHT11", "°C"),
    ("humidity", "DHT11", "%RH"),
    ("gas", "MQ135", "ppm"),
)
