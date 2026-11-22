// Canales del dispositivo y sus colores en la gráfica.
export const metrics = [
  { key: 'temperature', label: 'Temperatura', unit: '°C', sensor: 'DHT11', color: '#ff9e64' },
  { key: 'humidity', label: 'Humedad', unit: '%RH', sensor: 'DHT11', color: '#7dcfff' },
  { key: 'gas', label: 'Gas', unit: 'ppm', sensor: 'MQ135', color: '#bb9af7' },
];

export const periods = [
  { key: 'day', label: 'Día', span: 'Últimas 24 horas' },
  { key: 'week', label: 'Semana', span: 'Últimos 7 días' },
  { key: 'month', label: 'Mes', span: 'Últimos 30 días' },
];

// La hora se formatea en Ciudad de México aunque el navegador use otra zona.
const dateFormats = Object.fromEntries(
  Object.entries({
    date: { day: '2-digit', month: 'short', year: 'numeric' },
    time: { hour: '2-digit', minute: '2-digit', second: '2-digit', hourCycle: 'h23' },
    day: { day: 'numeric', month: 'short' },
    hour: { hour: '2-digit', minute: '2-digit', hourCycle: 'h23' },
  }).map(([key, options]) => [
    key,
    new Intl.DateTimeFormat('es-MX', {
      timeZone: 'America/Mexico_City',
      ...options,
    }),
  ]),
);

const decimalFormat = new Intl.NumberFormat('es-MX', {
  minimumFractionDigits: 1,
  maximumFractionDigits: 1,
});

const gasFormat = new Intl.NumberFormat('es-MX', { maximumFractionDigits: 1 });
export const integerFormat = new Intl.NumberFormat('es-MX');

export function formatValue(value, metric) {
  if (value == null) return '—';
  return (metric.key === 'gas' ? gasFormat : decimalFormat).format(value);
}

export function formatDate(date, type = 'date') {
  return dateFormats[type].format(new Date(date));
}
