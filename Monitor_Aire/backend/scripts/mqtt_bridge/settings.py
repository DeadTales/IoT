"""Constantes de transporte y contrato de mediciones."""

RAW_TOPIC_FILTER = "dispositivo/+/crudo"
MESSAGE_QUEUE_SIZE = 100
PUBLISH_TIMEOUT_SECONDS = 10
MQTT_KEEPALIVE_SECONDS = 60
CSV_COLUMNS = (
    "id", "device_id", "timestamp", "temperature", "humidity", "gas", "calibrate"
)
MEASUREMENT_FIELDS = (
    ("temperature", "DHT11", "°C"),
    ("humidity", "DHT11", "%RH"),
    ("gas", "MQ135", "ppm"),
)
