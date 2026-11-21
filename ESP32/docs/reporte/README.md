# Reporte de la práctica MonitorAire

- [Reporte en PDF](reporte_monitor_aire.pdf): 19 páginas, tres diagramas vectoriales, correspondencia OSI/TCP-IP, resultados, ventajas y desventajas, fundamento teórico y análisis del código.
- [Fuente editable LaTeX](reporte_monitor_aire.tex).

El reporte describe la configuración principal HSPI a 20 MHz y distingue resultados observados de verificaciones pendientes. No incluye credenciales.

Para regenerar desde la raíz del proyecto, con XeLaTeX, TikZ y las fuentes Liberation instaladas:

```bash
xelatex -interaction=nonstopmode -halt-on-error -output-directory=/tmp docs/reporte/reporte_monitor_aire.tex
xelatex -interaction=nonstopmode -halt-on-error -output-directory=/tmp docs/reporte/reporte_monitor_aire.tex
cp /tmp/reporte_monitor_aire.pdf docs/reporte/reporte_monitor_aire.pdf
```

Las dos pasadas actualizan índice, numeración y referencias. Los archivos auxiliares quedan en `/tmp`.

Los diagramas usan tarjetas con espacios interiores explícitos, rutas separadas para datos y ACK, agrupaciones por equipo y paneles independientes para mensajes actuales e históricos. Se revisaron los renders de las páginas de diagramas para evitar solapes y texto fuera de los bloques.
