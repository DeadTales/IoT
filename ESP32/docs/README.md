# Documentación de MonitorAire

MonitorAire toma lecturas de temperatura, humedad y gas en el ESP32-S3, las transmite por MQTT y las presenta en un panel web. PostgreSQL conserva el historial y el backend controla el acceso. El panel muestra las mediciones recientes, grafica promedios por periodo y exporta las lecturas filtradas.

```text
                  MQTT                       MQTT
ESP32-S3 ───────────────────► Mosquitto ───────────────────► FastAPI ───► PostgreSQL
                                                               │
                                            HTTP y eventos SSE │
                                                               ▼
                                                          Panel Svelte
```

## Arranque

Sigue el [notebook de puesta en marcha](00_puesta_en_marcha.ipynb) para preparar PostgreSQL y Mosquitto, ejecutar el backend y elegir cómo servir la interfaz.

Con `backend/.env` configurado y los servicios activos, abre dos terminales. Desde la raíz `ESP32`, inicia el backend.

```bash
backend/.venv/bin/python -m uvicorn backend.app:app --host 127.0.0.1 --port 8000
```

En la segunda terminal, abre Vite en modo desarrollo.

```bash
cd frontend
npm ci
npm run dev -- --host 127.0.0.1
```

Entra en `http://127.0.0.1:5173`. `npm ci` se necesita en una instalación nueva o cuando cambia `package-lock.json`. Para servir desde FastAPI, compila con `npm run build` en `frontend` antes de arrancar Uvicorn y abre `http://127.0.0.1:8000`.

## Guías por servicio

| Área | Qué aprenderás |
|---|---|
| [Frontend](frontend/README.md) | Svelte, Vite, reactividad, conexión a la API, gráficas, historial y exportación. |
| [Backend](backend/README.md) | FastAPI, Python, PostgreSQL, autenticación, persistencia y transmisión en vivo. |
| [ESP32](esp32/README.md) | C++, PlatformIO, electrónica, sensores, pantalla, sincronización de hora y almacenamiento local. |
| [MQTT](mqtt/README.md) | Publicación/suscripción, Mosquitto, mensajes, confirmaciones, recuperación y diagnóstico. |

Abre los archivos `.ipynb` en VS Code con soporte de Jupyter o en JupyterLab. Los diagramas están adjuntos a los notebooks. Las celdas de Markdown se pueden leer sin ejecutar Python. Los notebooks con ejercicios identifican qué se ejecuta y qué queda como muestra.
