"""API HTTP y transmisiones en vivo del monitor de aire."""

import asyncio
import json
from contextlib import asynccontextmanager
from datetime import date, datetime, time, timedelta, timezone
from pathlib import Path
from typing import Annotated, Literal
from zoneinfo import ZoneInfo

from fastapi import APIRouter, Depends, FastAPI, HTTPException, Query, Request
from fastapi.concurrency import run_in_threadpool
from fastapi.encoders import jsonable_encoder
from fastapi.responses import StreamingResponse
from fastapi.staticfiles import StaticFiles

from . import auth, db
from .mqtt_ingest import MqttIngest

# Los filtros por fecha toman la zona local y PostgreSQL conserva los instantes.
LOCAL_TIME = ZoneInfo("America/Mexico_City")
PERIODS = {
    "day": (timedelta(days=1), timedelta(minutes=15)),
    "week": (timedelta(days=7), timedelta(hours=4)),
    "month": (timedelta(days=30), timedelta(days=1)),
}
DIST = Path(__file__).resolve().parents[1] / "frontend" / "dist"
DeviceId = Annotated[str, Query(min_length=1, max_length=128)]


class LiveState:
    """Estado del broker y de los dispositivos compartido por los clientes SSE."""

    def __init__(self):
        self.broker_connected = False
        self.statuses: dict[str, str] = {}
        self.changed = asyncio.Event()

    def notify(self):
        # Cada cambio despierta a quienes esperan el evento anterior.
        self.changed.set()
        self.changed = asyncio.Event()

    def set_broker(self, connected):
        if self.broker_connected != connected:
            self.broker_connected = connected
            self.statuses.clear()
            self.notify()

    def set_status(self, device, status):
        if self.statuses.get(device) != status:
            self.statuses[device] = status
            self.notify()

    def snapshot(self, readings):
        return {
            "broker_connected": self.broker_connected,
            "devices": [
                {
                    "device_id": device,
                    "status": self.statuses.get(device),
                    "latest": (
                        readings[device] if device in readings and readings[device]["id"] else None
                    ),
                }
                for device in sorted(readings.keys() | self.statuses.keys())
            ],
        }


@asynccontextmanager
async def lifespan(app: FastAPI):
    """Gestiona las conexiones de PostgreSQL y el cliente MQTT durante la ejecución."""
    app.state.auth = auth.AuthSettings.load()
    pool = await run_in_threadpool(db.open_pool)
    try:
        await run_in_threadpool(pool.open, wait=True)
        app.state.pool = pool
        live = app.state.live = LiveState()
        loop = asyncio.get_running_loop()
        # El hilo MQTT entrega los cambios al bucle de FastAPI de forma segura.
        ingest = MqttIngest(
            pool,
            on_broker=lambda connected: loop.call_soon_threadsafe(live.set_broker, connected),
            on_status=lambda device, status: loop.call_soon_threadsafe(
                live.set_status, device, status
            ),
            on_reading=lambda: loop.call_soon_threadsafe(live.notify),
        )
        ingest.start()
        try:
            yield
        finally:
            await run_in_threadpool(ingest.stop)
    finally:
        await run_in_threadpool(pool.close)


# Rutas de sesión y consultas de mediciones protegidas.
api = APIRouter(prefix="/api")
api.include_router(auth.router)
readings_api = APIRouter(dependencies=[Depends(auth.session_required)])


# El cliente recibe el estado inicial y los cambios mientras su sesión siga vigente.
@api.get("/live")
async def live_feed(
    request: Request,
    session: Annotated[auth.Session, Depends(auth.session_required)],
):
    live = request.app.state.live

    async def events():
        needs_snapshot = True
        while datetime.now(timezone.utc) < session.expires:
            if needs_snapshot:
                # Guarda el evento para detectar cambios durante la consulta.
                changed = live.changed
                readings = await run_in_threadpool(db.latest_by_device, request.app.state.pool)
                snapshot = json.dumps(jsonable_encoder(live.snapshot(readings)), ensure_ascii=False)
                yield f"event: snapshot\ndata: {snapshot}\n\n"

            remaining = (session.expires - datetime.now(timezone.utc)).total_seconds()
            try:
                await asyncio.wait_for(changed.wait(), timeout=min(15, remaining))
                needs_snapshot = True
            except TimeoutError:
                needs_snapshot = False
                yield ": heartbeat\n\n"

    return StreamingResponse(
        events(),
        media_type="text/event-stream",
        headers={"X-Accel-Buffering": "no"},
    )


# Consultas de gráficas y del historial exportable.
@readings_api.get("/series")
def series(
    request: Request,
    device_id: DeviceId,
    period: Literal["day", "week", "month"] = "day",
):
    window, step = PERIODS[period]
    end = datetime.now(timezone.utc)
    start = end - window
    return {
        "start": start,
        "end": end,
        "step_seconds": int(step.total_seconds()),
        "points": db.get_series(request.app.state.pool, device_id, start, end, step),
    }


def history_filters(
    device_id: DeviceId,
    start: date | None = None,
    end: date | None = None,
) -> dict:
    """Incluye todo el día final usando la medianoche siguiente como límite exclusivo."""
    if (start and end and start > end) or end == date.max:
        raise HTTPException(status_code=422, detail="El rango de fechas es inválido")
    return {
        "device": device_id,
        "start": datetime.combine(start, time.min, LOCAL_TIME) if start else None,
        "end": datetime.combine(end + timedelta(days=1), time.min, LOCAL_TIME) if end else None,
    }


@readings_api.get("/history")
def history(
    request: Request,
    filters: Annotated[dict, Depends(history_filters)],
    page: Annotated[int, Query(ge=1, le=2_147_483_647)] = 1,
    per_page: Annotated[int, Query(ge=1, le=100)] = 20,
):
    return db.get_history(request.app.state.pool, filters, page, per_page)


@readings_api.get("/history.csv")
def history_csv(request: Request, filters: Annotated[dict, Depends(history_filters)]):
    return StreamingResponse(
        db.export_history(request.app.state.pool, filters),
        media_type="text/csv",
        headers={"Content-Disposition": 'attachment; filename="monitor-aire.csv"'},
    )


api.include_router(readings_api)
app = FastAPI(title="Monitor de aire", lifespan=lifespan)
app.include_router(api)


@app.middleware("http")
async def prevent_api_caching(request: Request, call_next):
    response = await call_next(request)
    if request.url.path.startswith("/api/"):
        response.headers["Cache-Control"] = "no-store"
    return response


# Monta el frontend al final para dar prioridad a las rutas de la API.
if DIST.is_dir():
    app.mount("/", StaticFiles(directory=DIST, html=True), name="dashboard")
