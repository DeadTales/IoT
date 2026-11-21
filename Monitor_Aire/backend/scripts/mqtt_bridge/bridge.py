"""Recepcion MQTT, procesamiento en segundo plano y confirmaciones."""

from __future__ import annotations

import csv
import logging
import queue
import threading
from typing import TYPE_CHECKING

from .converter import convert_csv_message
from .models import ConvertedReading
from .settings import MESSAGE_QUEUE_SIZE, PUBLISH_TIMEOUT_SECONDS, RAW_TOPIC_FILTER

if TYPE_CHECKING:
    import paho.mqtt.client as mqtt

LOGGER = logging.getLogger(__name__)


class CsvMqttBridge:
    """Procesa mensajes fuera del hilo MQTT para poder esperar el PUBACK."""

    def __init__(self, client: mqtt.Client) -> None:
        self.client = client
        self.messages: queue.Queue[tuple[str, bytes]] = queue.Queue(
            maxsize=MESSAGE_QUEUE_SIZE
        )
        self.client.on_connect = self.on_connect
        self.client.on_message = self.on_message

    def on_connect(self, client, userdata, flags, reason_code, properties) -> None:
        """Restablece la suscripcion al conectar o reconectar al broker."""
        if reason_code != 0:
            LOGGER.error("Conexion rechazada: %s", reason_code)
            return

        client.subscribe(RAW_TOPIC_FILTER, qos=1)
        LOGGER.info("Conectado; escuchando %s", RAW_TOPIC_FILTER)

    def on_message(self, client, userdata, message) -> None:
        """Encola el CSV sin bloquear el procesamiento de paquetes MQTT."""
        try:
            self.messages.put_nowait((message.topic, message.payload))
        except queue.Full:
            LOGGER.warning("Cola llena; el ESP32 reintentara sin recibir ACK")

    def publish_reading(self, reading: ConvertedReading) -> None:
        """Confirma al ESP32 solo despues del PUBACK del JSON tratado."""
        topic_prefix = f"dispositivo/{reading.device_id}"
        publication = self.client.publish(
            f"{topic_prefix}/tratado", reading.json_payload, qos=1
        )
        publication.wait_for_publish(timeout=PUBLISH_TIMEOUT_SECONDS)

        if not publication.is_published():
            LOGGER.warning("Sin confirmacion del broker; CSV pendiente")
            return

        self.client.publish(f"{topic_prefix}/ack", reading.record_id, qos=1)

    def process_messages(self) -> None:
        """Mantiene pendientes en el ESP32 las lecturas que no se procesen."""
        while True:
            topic, payload = self.messages.get()
            try:
                reading = convert_csv_message(topic, payload)
                self.publish_reading(reading)
            except (ValueError, KeyError, UnicodeError, RuntimeError, csv.Error) as error:
                LOGGER.error("CSV no procesado: %s", error)
            finally:
                self.messages.task_done()

    def start(self) -> None:
        worker = threading.Thread(
            target=self.process_messages,
            name="csv-message-worker",
            daemon=True,
        )
        worker.start()
