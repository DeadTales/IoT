// Conserva el código HTTP para que cada vista decida cómo responder.
export class ApiError extends Error {
  constructor(status) {
    super(`HTTP ${status}`);
    this.status = status;
  }
}

// Rutas de mismo origen para compartir la cookie de sesión.
export async function api(path, options) {
  const response = await fetch(`/api${path}`, options);
  if (!response.ok) throw new ApiError(response.status);
  if (response.status !== 204) return response.json();
}

export function postJson(path, payload) {
  return api(path, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(payload),
  });
}

// El listado y la descarga CSV comparten los filtros de fecha.
export function historyQuery(device, filters) {
  const query = new URLSearchParams({ device_id: device });
  if (filters.start) query.set('start', filters.start);
  if (filters.end) query.set('end', filters.end);
  return query;
}
