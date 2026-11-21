# Backend de Monitor de aire

El backend de Python recibe las mediciones del broker, las guarda en PostgreSQL y expone la API que usa el panel. Los notebooks explican la ejecución y las decisiones del código con diagramas y ejercicios de lectura.

| Notebook | Contenido |
| --- | --- |
| [00 · Instalar, configurar y arrancar](00_ejecucion_y_entorno.ipynb) | Entorno virtual, PostgreSQL, pgAdmin, archivo de configuración, cuenta del panel y pruebas de arranque. |
| [01 · Arquitectura Python y FastAPI](01_arquitectura_fastapi.ipynb) | HTTP, ASGI, Uvicorn, FastAPI, Starlette, Pydantic, ciclo de vida y concurrencia. |
| [02 · PostgreSQL y persistencia](02_postgresql_y_persistencia.ipynb) | Tablas, SQL, tipos, índices, transacciones, migración y conexiones reutilizadas. |
| [03 · API y autenticación](03_api_y_autenticacion.ipynb) | Rutas, códigos HTTP, hash Argon2, JWT, cookies y verificación de sesión. |
| [04 · Ingestión, tiempo real e historial](04_ingestion_tiempo_real_e_historial.ipynb) | Validación MQTT, trabajo en segundo plano, SSE, agregación temporal, paginación y CSV. |

Los comandos están documentados para ejecutarse desde la raíz `ESP32`, salvo donde se indique otra carpeta. Los diagramas están incluidos en los notebooks y los ejemplos en bloques de código se ejecutan manualmente.
