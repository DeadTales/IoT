<script>
  import { Eye, EyeOff } from '@lucide/svelte';
  import Brand from '../components/Brand.svelte';

  let { onlogin, pending, error } = $props();
  let username = $state('');
  let password = $state('');
  let passwordVisible = $state(false);

  // El componente padre valida las credenciales. Mientras responde, evitamos otro envío.
  function submit(event) {
    event.preventDefault();

    if (!pending) {
      onlogin(username.trim(), password);
    }
  }

  function togglePasswordVisibility() {
    passwordVisible = !passwordVisible;
  }
</script>

<main class="login-shell">
  <header class="login-header">
    <Brand />
  </header>

  <div class="login-deck">
    <!-- Formulario de acceso y estado del envío. -->
    <section class="login-access" aria-labelledby="login-title">
      <div class="login-form-wrap">
        <h1 id="login-title">Inicia sesión</h1>

        <form onsubmit={submit} aria-busy={pending}>
          <div class="login-field">
            <label for="login-username">Usuario</label>
            <input
              id="login-username"
              bind:value={username}
              required
              autocomplete="username"
              maxlength="64"
              autocapitalize="off"
              spellcheck="false"
            />
          </div>

          <div class="login-field">
            <label for="login-password">Contraseña</label>
            <div class="password-field">
              <input
                id="login-password"
                bind:value={password}
                required
                autocomplete="current-password"
                maxlength="256"
                type={passwordVisible ? 'text' : 'password'}
              />
              <button
                class="password-toggle"
                type="button"
                aria-label={passwordVisible ? 'Ocultar contraseña' : 'Mostrar contraseña'}
                onclick={togglePasswordVisibility}
              >
                {#if passwordVisible}
                  <EyeOff size={19} />
                {:else}
                  <Eye size={19} />
                {/if}
              </button>
            </div>
          </div>

          {#if error}
            <p class="login-error" role="alert">{error}</p>
          {/if}

          <button class="submit-button" type="submit" disabled={pending}>
            {pending ? 'Validando…' : 'Iniciar sesión'}
          </button>
        </form>
      </div>
    </section>

    <!-- Los colores de cada canal coinciden con los de sus gráficas. -->
    <section class="station" aria-labelledby="station-title">
      <h2 id="station-title">Estación ambiental</h2>

      <div class="signal-map">
        <div class="sensor-stack">
          <div class="sensor">
            <p>DHT11</p>
            <dl>
              <div class="temperature">
                <dt>Temperatura</dt>
                <dd>°C</dd>
              </div>
              <div class="humidity">
                <dt>Humedad</dt>
                <dd>%RH</dd>
              </div>
            </dl>
          </div>
          <div class="sensor">
            <p>MQ135</p>
            <dl>
              <div class="gas">
                <dt>Gas</dt>
                <dd>ppm</dd>
              </div>
            </dl>
          </div>
        </div>

        <div class="traces" aria-hidden="true">
          <span class="trace temperature"></span>
          <span class="trace humidity"></span>
          <span class="trace gas"></span>
        </div>

        <div class="processor">
          <strong>ESP32-S3</strong>
          <span>Wi-Fi · MQTT</span>
        </div>
      </div>
    </section>
  </div>
</main>

<style>
  /* Distribución general de la pantalla de acceso. */
  .login-shell {
    display: grid;
    align-content: center;
    width: min(72rem, calc(100% - 3rem));
    min-height: 100vh;
    min-height: 100svh;
    margin-inline: auto;
    padding-block: 2.75rem;
    color: var(--ink);
  }

  .login-header {
    margin-bottom: 1.1rem;
  }

  .login-deck {
    display: grid;
    grid-template-areas: 'station access';
    grid-template-columns: minmax(0, 1.12fr) minmax(0, 0.88fr);
    align-items: stretch;
    border: 1px solid var(--line);
  }

  /* Placa, pistas y sensores de la estación. */
  .station {
    grid-area: station;
    position: relative;
    isolation: isolate;
    display: grid;
    grid-template-rows: auto 1fr;
    align-items: center;
    gap: clamp(2rem, 5vw, 3.4rem);
    min-width: 0;
    min-height: 31rem;
    padding: clamp(1.6rem, 4.2vw, 3.9rem);
    overflow: hidden;
    color: var(--instrument-text);
    background-color: var(--instrument);
    background-image: radial-gradient(var(--instrument-line) 1px, transparent 1px);
    background-size: 17px 17px;
    background-position: center;
  }

  .station::before,
  .station::after {
    position: absolute;
    z-index: -1;
    width: 1.15rem;
    height: 1.15rem;
    border: 1px solid var(--line);
    content: '';
    pointer-events: none;
  }

  .station::before {
    top: 13px;
    left: 13px;
    border-right: 0;
    border-bottom: 0;
  }

  .station::after {
    right: 13px;
    bottom: 13px;
    border-top: 0;
    border-left: 0;
  }

  .station h2 {
    justify-self: start;
    max-width: 10ch;
    margin: 0;
    padding-right: 0.2em;
    color: var(--instrument-text);
    background: var(--instrument);
    font-size: clamp(2.35rem, 4.4vw, 4.5rem);
    font-weight: 640;
    line-height: 1;
    letter-spacing: -0.065em;
  }

  .signal-map {
    --sensor-caption-height: 2rem;
    --sensor-row-height: 1.85rem;
    --sensor-gap: 0.75rem;
    display: grid;
    grid-template-columns: minmax(8.2rem, 1fr) minmax(2rem, 0.42fr) minmax(6.6rem, 0.78fr);
    align-items: center;
    width: 100%;
    max-width: 28rem;
    font-family: var(--mono);
  }

  .sensor-stack {
    display: grid;
    gap: var(--sensor-gap);
  }

  .sensor {
    border: 1px solid var(--line);
    background: var(--instrument);
  }

  .sensor p {
    display: flex;
    align-items: center;
    height: var(--sensor-caption-height);
    margin: 0;
    padding-inline: 0.6rem;
    border-bottom: 1px solid var(--instrument-line);
    color: var(--instrument-muted);
    font-size: 0.8125rem;
    line-height: 1.1;
  }

  .sensor dl {
    display: grid;
    margin: 0;
    padding-inline: 0.6rem;
  }

  .sensor dl div {
    display: flex;
    justify-content: space-between;
    gap: 0.35rem;
    align-items: center;
    min-height: var(--sensor-row-height);
    font-size: clamp(0.76rem, 1.28vw, 0.91rem);
  }

  .sensor dt {
    color: var(--instrument-text);
  }

  .sensor dd {
    margin: 0;
    color: var(--channel);
    white-space: nowrap;
  }

  .temperature {
    --channel: var(--signal-temperature);
  }

  .humidity {
    --channel: var(--signal-humidity);
  }

  .gas {
    --channel: var(--signal-gas);
  }

  .traces {
    position: relative;
    align-self: stretch;
  }

  .trace {
    position: absolute;
    left: 0;
    right: 0;
    height: 1px;
    background: var(--channel);
  }

  /* Alinea cada pista con el centro de la fila que representa su lectura. */
  .trace.temperature {
    top: calc(1px + var(--sensor-caption-height) + var(--sensor-row-height) / 2);
  }
  .trace.humidity {
    top: calc(1px + var(--sensor-caption-height) + var(--sensor-row-height) * 1.5);
  }
  .trace.gas {
    top: calc(
      3px + var(--sensor-caption-height) * 2 + var(--sensor-row-height) * 2.5 + var(--sensor-gap)
    );
  }

  .trace::before {
    position: absolute;
    top: -3px;
    left: -3px;
    width: 7px;
    height: 7px;
    border: 1px solid var(--channel);
    border-radius: 50%;
    background: var(--instrument);
    content: '';
  }

  .processor {
    position: relative;
    display: grid;
    place-content: center;
    gap: 0.45rem;
    min-height: 8.85rem;
    padding: 0.6rem;
    border: 1px solid var(--accent);
    background: var(--paper);
    color: var(--accent);
    text-align: center;
  }

  .processor strong {
    font-size: clamp(0.95rem, 2.15vw, 1.35rem);
    font-weight: 600;
    letter-spacing: -0.04em;
    white-space: nowrap;
  }

  .processor span {
    color: var(--muted);
    font-size: 0.75rem;
    white-space: nowrap;
  }

  .processor::before,
  .processor::after {
    position: absolute;
    right: 15%;
    left: 15%;
    height: 7px;
    background: repeating-linear-gradient(90deg, var(--accent) 0 2px, transparent 2px 8px);
    content: '';
  }

  .processor::before {
    top: -8px;
  }
  .processor::after {
    bottom: -8px;
  }

  /* Formulario y controles de acceso. */
  .login-access {
    grid-area: access;
    display: grid;
    place-items: center;
    min-width: 0;
    padding: clamp(1.8rem, 5.4vw, 4.8rem);
    background: var(--paper);
  }

  .login-form-wrap {
    width: 100%;
    max-width: 22rem;
  }

  .login-form-wrap h1 {
    margin: 0 0 clamp(1.8rem, 4.1vw, 2.65rem);
    font-size: clamp(1.9rem, 4.25vw, 3.3rem);
    font-weight: 650;
    line-height: 1.02;
    letter-spacing: -0.06em;
  }

  form {
    display: grid;
    gap: 1.15rem;
  }

  .login-field {
    display: grid;
    gap: 0.48rem;
  }

  .login-field label {
    font-size: 0.9375rem;
    font-weight: 580;
  }

  .login-field input {
    width: 100%;
    min-height: 3.1rem;
    padding: 0.65rem 0.75rem;
    border: 1px solid var(--line);
    border-radius: 0;
    background: var(--page);
    color: var(--ink);
    font-size: 1rem;
  }

  .password-field {
    position: relative;
  }

  .password-field input {
    padding-right: 3.2rem;
  }

  .password-toggle {
    position: absolute;
    top: 1px;
    right: 1px;
    bottom: 1px;
    display: grid;
    place-items: center;
    width: 2.85rem;
    padding: 0;
    border: 1px solid transparent;
    border-left-color: var(--line);
    background: var(--paper);
    color: var(--muted);
  }

  .password-toggle:hover {
    background: var(--surface-hover);
    color: var(--ink);
  }

  .login-field input:focus-visible,
  .password-toggle:focus-visible,
  .submit-button:focus-visible {
    outline: 2px solid var(--accent);
    outline-offset: 2px;
  }

  .login-error {
    padding: 0.7rem 0.8rem;
    border-left: 3px solid var(--error-stroke);
    background: var(--error-surface);
    color: var(--error-text);
    font-size: 0.9375rem;
  }

  .submit-button {
    min-height: 3.1rem;
    padding: 0.62rem 0.9rem;
    border: 1px solid var(--ink);
    border-radius: 0;
    background: var(--accent);
    color: var(--accent-ink);
    font-weight: 660;
  }

  .submit-button:hover:not(:disabled) {
    background: var(--accent-hover);
  }

  .submit-button:disabled {
    background: var(--page);
    border-color: var(--line);
    color: var(--muted);
  }

  /* El formulario aparece primero cuando los paneles se apilan. */
  @media (max-width: 48rem) {
    .login-shell {
      width: min(32rem, calc(100% - 2rem));
      padding-block: 1.4rem 2rem;
    }

    .login-deck {
      grid-template-areas: 'access' 'station';
      grid-template-columns: minmax(0, 1fr);
    }

    .station {
      min-height: 0;
      gap: 1.35rem;
      padding: 1.65rem clamp(0.8rem, 5vw, 2rem);
    }

    .station h2 {
      max-width: none;
      font-size: clamp(1.4rem, 6.1vw, 2.2rem);
      letter-spacing: -0.045em;
    }

    .login-access {
      padding: 1.75rem clamp(1rem, 5vw, 2.25rem) 2rem;
    }
  }

  @media (max-width: 22rem) {
    .signal-map {
      grid-template-columns: minmax(7.75rem, 1fr) minmax(1rem, 0.34fr) minmax(5.6rem, 0.72fr);
    }
    .sensor p,
    .sensor dl {
      padding-inline: 0.4rem;
    }
  }
</style>
