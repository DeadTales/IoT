# Frontend del Monitor de Aire

El panel Svelte muestra el estado de los dispositivos, las últimas mediciones, las tendencias y el historial.
Estos notebooks explican el código de `frontend/` y las tecnologías que se ejecutan en el navegador.

| Notebook | Contenido |
| --- | --- |
| [00 · Ejecución y herramientas](00_ejecucion_y_herramientas.ipynb) | Node.js, npm, Vite, arranque, compilación y vista previa. |
| [01 · Arquitectura y reactividad](01_arquitectura_y_reactividad.ipynb) | Componentes, runas de Svelte 5, estado, diseño adaptable y accesibilidad. |
| [02 · HTTP, sesión y tiempo real](02_http_sesion_y_tiempo_real.ipynb) | Proxy de Vite, autenticación por cookie, EventSource, snapshots y reconexión. |
| [03 · Series, historial y CSV](03_series_historial_y_csv.ipynb) | Chart.js, intervalos y huecos, filtros, paginación y descarga. |
| [04 · Diagnóstico y pruebas](04_diagnostico_y_pruebas.ipynb) | Pruebas manuales, DevTools y fallas habituales. |

Los diagramas están incluidos en los notebooks. Los fragmentos de terminal y JavaScript pueden leerse
sin iniciar un servidor ni ejecutar celdas.
