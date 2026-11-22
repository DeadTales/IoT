<script>
  import { Download, RefreshCw } from '@lucide/svelte';
  import { api, historyQuery } from '../../models/api';
  import { formatDate, formatValue, integerFormat, metrics } from '../../models/telemetry';

  const PER_PAGE = 20;
  const EMPTY_FILTERS = { start: '', end: '', start_time: '', end_time: '' };
  let { deviceId, onexpire } = $props();
  // Separa los filtros editados de los que ya se aplicaron.
  let draft = $state({ ...EMPTY_FILTERS });
  let filters = $state({ ...EMPTY_FILTERS });
  let pagination = $state({ device: '', page: 1 });
  let revision = $state(0);

  let response = $state(null);
  let loading = $state(false);
  let requestError = $state('');
  const maxPage = $derived(Math.max(1, Math.ceil((response?.total ?? 0) / PER_PAGE)));
  const csvUrl = $derived(`/api/history.csv?${historyQuery(deviceId, filters)}`);
  const datesInvalid = $derived(Boolean(draft.start && draft.end && draft.start > draft.end));
  const filtersChanged = $derived(
    Object.keys(EMPTY_FILTERS).some((key) => draft[key] !== filters[key]),
  );
  const hasAppliedFilters = $derived(Object.values(filters).some(Boolean));
  const crossesMidnight = $derived(
    draft.start_time && draft.end_time && draft.start_time > draft.end_time,
  );

  // Renueva la página al cambiar el dispositivo o los filtros.
  $effect(() => {
    const id = deviceId;
    if (!id) return;
    void revision;
    const page = id === pagination.device ? pagination.page : 1;
    const query = historyQuery(id, filters);
    query.set('page', page);
    query.set('per_page', PER_PAGE);
    const controller = new AbortController();
    loading = true;
    requestError = '';
    api(`/history?${query}`, { signal: controller.signal })
      .then((result) => {
        if (controller.signal.aborted) return;
        const last = Math.max(1, Math.ceil(result.total / PER_PAGE));
        if (page > last) goToPage(last);
        else response = result;
      })
      .catch((error) => {
        if (controller.signal.aborted) return;
        if (error.status === 401) onexpire();
        else requestError = 'No se pudo consultar el historial.';
      })
      .finally(() => {
        if (!controller.signal.aborted) loading = false;
      });
    return () => controller.abort();
  });

  function goToPage(page) {
    pagination = { device: deviceId, page };
  }

  function applyFilters(event) {
    event.preventDefault();
    if (datesInvalid) return;
    filters = { ...draft };
    goToPage(1);
  }

  function clearFilters() {
    draft = { ...EMPTY_FILTERS };
    filters = { ...EMPTY_FILTERS };
    goToPage(1);
  }
</script>

<section class="history-panel" aria-label="Registros históricos">
  <div class="history-header">
    <p>Hora de Ciudad de México</p>
    {#if response?.total && !loading && !requestError && !filtersChanged && deviceId}
      <a class="button button-primary" href={csvUrl} download="monitor-aire.csv">
        <Download size={17} /> Exportar CSV
      </a>
    {:else}
      <button class="button button-primary" type="button" disabled>
        <Download size={17} /> Exportar CSV
      </button>
    {/if}
  </div>

  <!-- Filtros de fecha y hora compartidos con la exportación. -->
  <form class="history-filters" onsubmit={applyFilters}>
    <div class="history-filter-pair">
      <div class="history-filter-field">
        <label for="filter-start">Fecha inicial</label>
        <input id="filter-start" type="date" bind:value={draft.start} />
      </div>
      <div class="history-filter-field">
        <label for="filter-end">Fecha final</label>
        <input id="filter-end" type="date" bind:value={draft.end} />
      </div>
    </div>
    <div class="history-filter-pair">
      <div class="history-filter-field">
        <label for="filter-start-time">Hora inicial</label>
        <input
          id="filter-start-time"
          type="time"
          step="60"
          bind:value={draft.start_time}
          aria-describedby="history-hours-note"
        />
      </div>
      <div class="history-filter-field">
        <label for="filter-end-time">Hora final</label>
        <input
          id="filter-end-time"
          type="time"
          step="60"
          bind:value={draft.end_time}
          aria-describedby="history-hours-note"
        />
      </div>
    </div>
    <button
      class="button button-secondary"
      type="submit"
      disabled={datesInvalid || !deviceId || !filtersChanged}
    >
      Aplicar filtros
    </button>
    {#if filtersChanged || hasAppliedFilters}
      <button class="button button-text" type="button" onclick={clearFilters}>Limpiar</button>
    {/if}
    <p class="history-filter-note" id="history-hours-note">
      Las horas se aplican a cada día del periodo.{#if crossesMidnight}
        El rango cruza la medianoche.
      {/if}
    </p>
    {#if datesInvalid}<p class="form-error" role="alert">
        La fecha final debe ser igual o posterior a la fecha inicial.
      </p>{/if}
  </form>

  {#if loading && deviceId}
    <div class="table-empty" role="status"><strong>Cargando registros…</strong></div>
  {:else if requestError && deviceId}
    <div class="table-empty" role="status">
      <strong>{requestError}</strong><button
        class="button button-secondary"
        type="button"
        onclick={() => revision++}
      >
        <RefreshCw size={16} /> Reintentar
      </button>
    </div>
  {:else if !deviceId || !response?.rows.length}
    <div class="table-empty">
      <strong>No hay registros disponibles</strong>
      <p>
        {deviceId && hasAppliedFilters
          ? 'Prueba con otro rango de fechas u horas.'
          : 'Aquí aparecerán las lecturas con hora de medición.'}
      </p>
    </div>
  {:else}
    <div class="table-scroll">
      <table aria-label="Lecturas históricas del dispositivo">
        <thead>
          <tr>
            <th scope="col">Fecha y hora de medición</th>
            {#each metrics as metric (metric.key)}
              <th scope="col" class="numeric">{metric.label} <span>({metric.unit})</span></th>
            {/each}
            <th scope="col">Calibración de gas</th>
          </tr>
        </thead>
        <tbody>
          {#each response.rows as row (row.id)}
            {@const timestamp = row.measured_at}
            <tr>
              <td class="table-time">
                <time datetime={timestamp}>
                  <strong>{formatDate(timestamp)}</strong><span>
                    {formatDate(timestamp, 'time')}
                  </span>
                </time>
              </td>
              {#each metrics as metric (metric.key)}
                <td class="numeric">{formatValue(row[metric.key], metric)}</td>
              {/each}
              <td>
                <span class="table-pill" class:calibrated={row.calibrated && row.gas != null}>
                  {row.gas == null ? '—' : row.calibrated ? 'Calibrado' : 'Sin calibrar'}
                </span>
              </td>
            </tr>
          {/each}
        </tbody>
      </table>
    </div>
    <div class="table-pagination">
      <div>
        <p>
          Mostrando {integerFormat.format((response.page - 1) * PER_PAGE + 1)} a {integerFormat.format(
            (response.page - 1) * PER_PAGE + response.rows.length,
          )} de {integerFormat.format(response.total)} registros
        </p>
        <button
          class="icon-button"
          type="button"
          title="Actualizar registros"
          aria-label="Actualizar registros"
          onclick={() => revision++}
        >
          <RefreshCw size={17} />
        </button>
      </div>
      <nav aria-label="Páginas del historial">
        <button
          type="button"
          class="button button-secondary"
          onclick={() => goToPage(response.page - 1)}
          disabled={response.page <= 1}
        >
          Anterior
        </button>
        <span>{response.page} de {maxPage}</span>
        <button
          type="button"
          class="button button-secondary"
          onclick={() => goToPage(response.page + 1)}
          disabled={response.page >= maxPage}
        >
          Siguiente
        </button>
      </nav>
    </div>
  {/if}
</section>
