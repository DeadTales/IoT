# Prueba aislada ST7789

La prueba conserva los pines configurados en `include/TFT_Setup.h`, el bus SPI y su frecuencia.
No utiliza WiFi, MQTT, DHT11, MQ135, LittleFS ni NVS. No modifica el CSV ni la configuracion guardada.
El firmware normal sigue siendo el entorno predeterminado de PlatformIO.

Para cargar solamente la prueba:

```bash
pio run -e display-test -t upload
pio device monitor -e display-test
```

Debe alternar rojo, verde, azul y blanco cada dos segundos, con texto negro.
El cambio de color es intencional en esta prueba. Serial imprime el color y el tiempo encendido.
En orientacion horizontal se esperan dimensiones logicas de 320 x 240.

- Si hay colores y texto estable, la pantalla funciona con esta configuracion. Hay que comparar
  el comportamiento con la aplicacion completa, incluyendo alimentacion al conectar WiFi/sensores.
- Si Serial sigue contando tiempo sin reinicios, pero solo hay luz, verificar señales, continuidad,
  controlador real del modulo y alimentacion. Terminar `display.init()` no confirma recepcion fisica:
  esta pantalla no tiene MISO para leer una respuesta.
- Si se repite todo el arranque sin tocar RESET ni volver a abrir el monitor, hay reinicios que
  deben investigarse. Probar cable/fuente USB estables y, con la alimentacion retirada, desconectar
  temporalmente los sensores y repetir la prueba permite separar sus efectos.

Para volver al monitor de aire:

```bash
pio run -e esp32-s3-devkitc-1 -t upload
```

No se requiere `uploadfs` ni borrar toda la flash para esta prueba.

## Variante FSPI a 5 MHz

Si la pantalla sigue sin imagen, esta variante cambia el bus a FSPI y reduce el reloj a 5 MHz.
Conserva SDA=13, SCL=12, CS=10, DC=9 y RST=8. El firmware normal usa HSPI a 20 MHz; esta variante es solo de diagnostico.
La seleccion FSPI es explicita para que TFT_eSPI use el puerto correspondiente en ESP32-S3.

```bash
pio run -e display-test-fspi -t upload
pio device monitor -e display-test-fspi
```

Serial debe mostrar `PRUEBA TFT`, `SPI=5000000 Hz` y `Prueba con bus FSPI`.
Si aparece `Evento: MQTT conectado`, sigue cargado el firmware principal y no la prueba aislada.
Informa si ves colores/texto y copia la salida Serial de esta variante antes de cambiar otras opciones.
