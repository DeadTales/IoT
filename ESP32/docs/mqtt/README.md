# Comunicación MQTT

MQTT transporta la telemetría del ESP32 entre clientes conectados a Mosquitto. La placa publica muestras y estados, y el consumidor Python valida las lecturas. El recorrido cubre desde la conexión TCP hasta las confirmaciones de cada fila pendiente.

## Recorrido principal

| Notebook | Contenido |
| --- | --- |
| [00 · MQTT en MonitorAire](00_indice_mqtt.ipynb) | Arquitectura y mapa de lectura. |
| [01 · Modelo pub/sub y protocolo MQTT](01_modelo_pubsub_y_protocolo.ipynb) | Temas, filtros, paquetes, QoS, keepalive, sesiones y Last Will. |
| [02 · Mosquitto y la red local](02_broker_mosquitto_y_red.ipynb) | Configuración LAN, direcciones, puertos, alcance de red y laboratorio con Mosquitto. |
| [03 · Topics y contrato de datos](03_topics_y_contrato_de_datos.ipynb) | JSON, CSV, identificadores, validación y uso de timestamps. |
| [04 · Ingestión, reintentos y diagnóstico](04_ingesta_reintentos_y_diagnostico.ipynb) | Hilos Paho, confirmaciones, deduplicación y laboratorio del conversor. |

## Recorrido complementario en Python

- [06 · Bibliotecas, tipos y conversión CSV a JSON](06_python_conversion_y_sintaxis.ipynb).
- [07 · Cliente MQTT, hilos, callbacks y ACK](07_python_mqtt_clase_hilos_y_ack.ipynb).
- [09 · Módulos, responsabilidades y contratos](09_python_modular.ipynb).
- [10 · Prácticas sin broker](10_python_pruebas_modulares.ipynb).

Los notebooks incluyen diagramas incrustados. Los laboratorios usan ejemplos sintéticos en un broker separado que escucha en `127.0.0.1:18883`.
