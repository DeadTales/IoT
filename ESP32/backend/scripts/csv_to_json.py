"""Punto de entrada del conversor CSV a JSON por MQTT."""

if __package__:
    from .mqtt_bridge.cli import main
else:
    from mqtt_bridge.cli import main


if __name__ == "__main__":
    main()
