<script>
  import { ChevronDown, Cpu, LogOut, RefreshCw } from '@lucide/svelte';
  import Brand from '../components/Brand.svelte';
  import SeriesChart from '../components/SeriesChart.svelte';
  import { api } from '../../models/api';
  import { formatDate, formatValue, metrics, periods } from '../../models/telemetry';
  import HistoryTable from './HistoryTable.svelte';

  const RECONNECT_INTERVAL_MS = 3000;
  const SERIES_REFRESH_INTERVAL_MS = 30_000;

  let { user, onlogout, onexpire, loggingOut } = $props();

  // Estado de navegación, dispositivos y conexión en vivo.
  let view = $state('overview');
  let devices = $state([]);
  let selectedId = $state('');
  let brokerConnected = $state(false);
  let streamStatus = $state('connecting');
  let actionError = $state('');

  // Selección y resultado de la consulta histórica graficada.
  let metricKey = $state('temperature');
  let periodKey = $state('day');
  let series = $state(null);
  let seriesLoading = $state(false);
  let seriesError = $state('');
  let seriesRevision = $state(0);

  const device = $derived(devices.find(({ device_id }) => device_id === selectedId));
  const latest = $derived(device?.latest);
  const metric = $derived(metrics.find(({ key }) => key === metricKey));
  const period = $derived(periods.find(({ key }) => key === periodKey));
  const chartHasValues = $derived(series?.points.some((point) => point[metricKey] != null));
  const aggregation = $derived(
    series &&
      (periodKey === 'month'
        ? 'Promedio diario'
        : series.step_seconds >= 3600
          ? `Promedio cada ${series.step_seconds / 3600} h`
          : `Promedio cada ${series.step_seconds / 60} min`),
  );
  const lastEvent = $derived(latest?.measured_at ?? latest?.received_at);
  const brokerOnline = $derived(streamStatus === 'open' && brokerConnected);
  const deviceOnline = $derived(brokerOnline && device?.status === 'conectado');
  const brokerText = $derived(
    streamStatus === 'open'
      ? brokerConnected
        ? 'MQTT conectado'
        : 'MQTT desconectado'
      : streamStatus === 'connecting'
        ? 'Conectando al panel'
        : 'Reconectando al panel',
  );
  const deviceText = $derived(
    deviceOnline
      ? 'En línea'
      : brokerOnline && device?.status === 'desconectado'
        ? 'Fuera de línea'
        : 'Sin estado',
  );

  // SSE transporta las lecturas y los cambios de estado del broker.
  $effect(() => {
    let stream;
    let retry;
    let checking = false;
    let active = true;

    function connect() {
      stream = new EventSource('/api/live');
      stream.onopen = () => (streamStatus = 'open');
      stream.addEventListener('snapshot', ({ data }) => {
        const snapshot = JSON.parse(data);
        devices = snapshot.devices;
        brokerConnected = snapshot.broker_connected;
        if (!devices.some(({ device_id }) => device_id === selectedId)) {
          selectedId = devices[0]?.device_id ?? '';
        }
      });
      stream.onerror = async () => {
        streamStatus = 'reconnecting';
        if (checking) return;
        checking = true;
        try {
          await api('/auth/me');
        } catch (error) {
          if (error.status === 401 && active) {
            onexpire();
            return;
          }
        } finally {
          checking = false;
        }
        if (active && stream.readyState === EventSource.CLOSED)
          retry = setTimeout(connect, RECONNECT_INTERVAL_MS);
      };
    }

    connect();
    return () => {
      active = false;
      clearTimeout(retry);
      stream.close();
    };
  });

  // Cancela la consulta del dispositivo y periodo al cambiar de vista.
  $effect(() => {
    const id = selectedId;
    const key = periodKey;
    if (!id || view !== 'overview') return;
    void seriesRevision;
    const controller = new AbortController();
    const query = new URLSearchParams({ device_id: id, period: key });
    series = null;
    seriesError = '';
    seriesLoading = true;
    let pending = false;
    const update = async () => {
      if (pending) return;
      pending = true;
      try {
        const result = await api(`/series?${query}`, { signal: controller.signal });
        if (controller.signal.aborted) return;
        series = result;
        seriesError = '';
      } catch (error) {
        if (controller.signal.aborted) return;
        if (error.status === 401) onexpire();
        else seriesError = 'No se pudo consultar la gráfica.';
      } finally {
        pending = false;
        if (!controller.signal.aborted) seriesLoading = false;
      }
    };
    void update();
    const interval = setInterval(update, SERIES_REFRESH_INTERVAL_MS);
    return () => {
      clearInterval(interval);
      controller.abort();
    };
  });

  function selectView(next) {
    view = next;
    actionError = '';
    window.scrollTo(0, 0);
  }

  async function endSession() {
    actionError = '';
    try {
      await onlogout();
    } catch {
      actionError = 'No se pudo cerrar la sesión. Intenta nuevamente.';
    }
  }
</script>

<div class="dashboard-layout">
  <!-- Navegación principal y cierre de sesión. -->
  <header class="topbar">
    <div class="topbar-inner">
      <Brand />
      <nav class="main-nav" aria-label="Navegación principal">
        <button
          type="button"
          class:active={view === 'overview'}
          aria-current={view === 'overview' ? 'page' : undefined}
          onclick={() => selectView('overview')}
        >
          En vivo
        </button>
        <button
          type="button"
          class:active={view === 'history'}
          aria-current={view === 'history' ? 'page' : undefined}
          onclick={() => selectView('history')}
        >
          Historial
        </button>
      </nav>
      <div class="account">
        <span title={user.username}>{user.username}</span>
        <button
          class="icon-button"
          type="button"
          title="Cerrar sesión"
          aria-label="Cerrar sesión"
          onclick={endSession}
          disabled={loggingOut}
        >
          <LogOut size={17} />
        </button>
      </div>
    </div>
  </header>

  <main class="workspace">
    <header class="page-header">
      <div>
        <div class="broker-status" aria-live="polite">
          <span class="status-dot" class:online={brokerOnline} aria-hidden="true"></span>
          {brokerText}
        </div>
        <h1>{view === 'overview' ? 'Estación ambiental' : 'Historial de lecturas'}</h1>
      </div>
      <div class="device-control">
        <label for="device-select">Dispositivo</label>
        <div class="select-field">
          <Cpu size={18} class="select-icon" />
          <select id="device-select" bind:value={selectedId} disabled={!devices.length}>
            {#if !devices.length}
              <option value="">Sin dispositivos</option>
            {:else}
              {#each devices as item (item.device_id)}<option value={item.device_id}>
                  {item.device_id}
                </option>{/each}
            {/if}
          </select>
          <ChevronDown size={16} class="select-chevron" />
        </div>
        {#if device}
          <div class="device-status">
            <span class="status-dot" class:online={deviceOnline} aria-hidden="true"></span>
            {deviceText}
          </div>
        {/if}
      </div>
    </header>
    {#if actionError}<p class="form-error" role="alert">{actionError}</p>{/if}

    {#if view === 'overview'}
      <!-- Las lecturas actuales también seleccionan la variable de la gráfica. -->
      <div class="instrument" style:--trace={metric.color}>
        <section class="channels" aria-labelledby="live-label">
          <div class="channels-header">
            <h2 id="live-label">Lecturas actuales</h2>
            {#if lastEvent}
              <p>
                {latest.measured_at ? 'Última medición' : 'Última recepción'}
                <time datetime={lastEvent} title={formatDate(lastEvent)}>
                  {formatDate(lastEvent, 'time')}
                </time>
              </p>
            {:else}
              <p>{device ? 'Esperando una medición' : 'Esperando un dispositivo'}</p>
            {/if}
          </div>
          <div class="channel-list" role="group" aria-label="Variable de la gráfica">
            {#each metrics as item (item.key)}
              <button
                type="button"
                class="channel"
                class:active={metricKey === item.key}
                style:--channel={item.color}
                aria-pressed={metricKey === item.key}
                aria-controls="trend"
                onclick={() => (metricKey = item.key)}
              >
                <span class="channel-heading">
                  <span>{item.label}</span>
                  <span class="sensor-label">{item.sensor}</span>
                </span>
                <span class="channel-value">
                  {formatValue(latest?.[item.key], item)} <span>{item.unit}</span>
                </span>
                {#if item.key === 'gas' && latest?.gas != null}
                  <span class="channel-note">
                    {latest.calibrated ? 'Lectura calibrada' : 'Concentración estimada'}
                  </span>
                {/if}
              </button>
            {/each}
          </div>
        </section>

        <section class="trend" id="trend" aria-labelledby="trend-label">
          <div class="trend-header">
            <div>
              <h2 id="trend-label"><span aria-hidden="true"></span>{metric.label}</h2>
              <p>{period.span}{aggregation ? ` · ${aggregation}` : ''}</p>
            </div>
            <div class="period-control" role="group" aria-label="Periodo de la gráfica">
              {#each periods as item (item.key)}
                <button
                  type="button"
                  class:active={periodKey === item.key}
                  aria-pressed={periodKey === item.key}
                  onclick={() => (periodKey = item.key)}
                >
                  {item.label}
                </button>
              {/each}
            </div>
          </div>

          {#if seriesError && selectedId}
            <div class="chart-empty" role="status">
              <strong>{seriesError}</strong><button
                type="button"
                class="button button-secondary"
                onclick={() => (seriesRevision += 1)}
              >
                <RefreshCw size={16} /> Reintentar
              </button>
            </div>
          {:else if selectedId && seriesLoading}
            <div class="chart-empty" role="status"><strong>Cargando mediciones…</strong></div>
          {:else if selectedId && chartHasValues}
            <SeriesChart {series} period={periodKey} {metric} />
          {:else}
            <div class="chart-empty">
              <strong>{selectedId ? 'Sin lecturas en este periodo' : 'Aún no hay lecturas'}</strong>
              <p>
                {selectedId
                  ? 'Elige otro periodo o selecciona otra medición.'
                  : 'La gráfica se habilita cuando un dispositivo envía mediciones.'}
              </p>
            </div>
          {/if}
          <p class="trend-footer">Hora de Ciudad de México</p>
        </section>
      </div>
    {:else}
      <HistoryTable deviceId={selectedId} {onexpire} />
    {/if}
  </main>
</div>
