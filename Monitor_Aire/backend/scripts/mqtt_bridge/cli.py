"""Convierte lecturas de dispositivo/+/crudo a dispositivo/<id>/tratado."""

from __future__ import annotations

import argparse
import logging

from .bridge import CsvMqttBridge
from .settings import MQTT_KEEPALIVE_SECONDS


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="localhost")
    parser.add_argument("--port", type=int, default=1883)
    return parser.parse_args()


def main() -> None:
    import paho.mqtt.client as mqtt

    arguments = parse_arguments()
    logging.basicConfig(level=logging.INFO)
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)

    bridge = CsvMqttBridge(client)
    bridge.start()
    client.connect(arguments.host, arguments.port, MQTT_KEEPALIVE_SECONDS)

    try:
        client.loop_forever()
    except KeyboardInterrupt:
        client.disconnect()
