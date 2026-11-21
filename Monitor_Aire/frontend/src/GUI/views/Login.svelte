<script>
  import { Eye, EyeOff } from '@lucide/svelte';
  import Brand from '../components/Brand.svelte';

  let { onlogin, pending, error } = $props();
  let username = $state('');
  let password = $state('');
  let passwordVisible = $state(false);

  function submit(event) {
    event.preventDefault();
    if (!pending) onlogin(username.trim(), password);
  }
</script>

<main class="login-shell">
  <header class="login-header">
    <Brand />
  </header>

  <div class="login-deck">
    <section class="login-access" aria-labelledby="login-title">
      <div class="login-form-wrap">
        <h1 id="login-title">Inicia sesión</h1>

        <form onsubmit={submit} aria-busy={pending}>
          <div class="login-field">
            <label for="login-username">Usuario</label>
            <input id="login-username" bind:value={username} required autocomplete="username" maxlength="64" autocapitalize="off" spellcheck="false" />
          </div>

          <div class="login-field">
            <label for="login-password">Contraseña</label>
            <div class="password-field">
              <input id="login-password" bind:value={password} required autocomplete="current-password" maxlength="256" type={passwordVisible ? 'text' : 'password'} />
              <button class="password-toggle" type="button" aria-label={passwordVisible ? 'Ocultar contraseña' : 'Mostrar contraseña'} onclick={() => passwordVisible = !passwordVisible}>
                {#if passwordVisible}<EyeOff size={19} />{:else}<Eye size={19} />{/if}
              </button>
            </div>
          </div>

          {#if error}<p class="login-error" role="alert">{error}</p>{/if}
          <button class="submit-button" type="submit" disabled={pending}>
            {pending ? 'Validando…' : 'Iniciar sesión'}
          </button>
        </form>
      </div>
    </section>

    <section class="station" aria-labelledby="station-title">
      <h2 id="station-title">Estación ambiental</h2>

      <div class="signal-map">
        <div class="sensor-stack">
          <div class="sensor">
            <p>DHT11</p>
            <dl>
              <div class="temperature"><dt>Temperatura</dt><dd>°C</dd></div>
              <div class="humidity"><dt>Humedad</dt><dd>%RH</dd></div>
            </dl>
          </div>
          <div class="sensor">
            <p>MQ135</p>
            <dl>
              <div class="gas"><dt>Gas</dt><dd>ppm</dd></div>
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
  .login-shell {
    --login-paper: #eceee7;
    --login-ink: #18211e;
    --login-board: #131b18;
    --login-lime: #d5ed77;
    --login-line: #acb4a8;
    --login-mono: 'IBM Plex Mono', ui-monospace, monospace;
    display: grid;
    align-content: center;
    width: min(72rem, calc(100% - 3rem));
    min-height: 100vh;
    min-height: 100svh;
    margin-inline: auto;
    padding-block: 2.75rem;
    color: var(--login-ink);
  }

  .login-header {
    margin-bottom: 1.1rem;
  }

  .login-deck {
    display: grid;
    grid-template-areas: 'station access';
    grid-template-columns: minmax(0, 1.12fr) minmax(0, .88fr);
    align-items: stretch;
    border: 1px solid var(--login-line);
  }

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
    color: var(--login-paper);
    background-color: var(--login-board);
    background-image: radial-gradient(#35443b 1px, transparent 1px);
    background-size: 17px 17px;
    background-position: center;
  }

  .station::before,
  .station::after {
    position: absolute;
    z-index: -1;
    width: 1.15rem;
    height: 1.15rem;
    border: 1px solid #7b9a67;
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
    padding-right: .2em;
    color: var(--login-paper);
    background: var(--login-board);
    font-size: clamp(2.35rem, 4.4vw, 4.5rem);
    font-weight: 640;
    line-height: 1;
    letter-spacing: -.065em;
  }

  .signal-map {
    --sensor-caption-height: 2rem;
    --sensor-row-height: 1.85rem;
    --sensor-gap: .75rem;
    display: grid;
    grid-template-columns: minmax(8.2rem, 1fr) minmax(2rem, .42fr) minmax(6.6rem, .78fr);
    align-items: center;
    width: 100%;
    max-width: 28rem;
    font-family: var(--login-mono);
  }

  .sensor-stack {
    display: grid;
    gap: var(--sensor-gap);
  }

  .sensor {
    border: 1px solid #4d5c50;
    background: var(--login-board);
  }

  .sensor p {
    display: flex;
    align-items: center;
    height: var(--sensor-caption-height);
    margin: 0;
    padding-inline: .6rem;
    border-bottom: 1px solid #38453b;
    color: #b4c4b5;
    font-size: .8125rem;
    line-height: 1.1;
  }

  .sensor dl {
    display: grid;
    margin: 0;
    padding-inline: .6rem;
  }

  .sensor dl div {
    display: flex;
    justify-content: space-between;
    gap: .35rem;
    align-items: center;
    min-height: var(--sensor-row-height);
    font-size: clamp(.76rem, 1.28vw, .91rem);
  }

  .sensor dt {
    color: var(--login-paper);
  }

  .sensor dd {
    margin: 0;
    color: var(--channel);
    white-space: nowrap;
  }

  .temperature { --channel: #f1ae83; }
  .humidity { --channel: #88d4c5; }
  .gas { --channel: var(--login-lime); }

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

  .trace.temperature { top: calc(1px + var(--sensor-caption-height) + var(--sensor-row-height) / 2); }
  .trace.humidity { top: calc(1px + var(--sensor-caption-height) + var(--sensor-row-height) * 1.5); }
  .trace.gas { top: calc(3px + var(--sensor-caption-height) * 2 + var(--sensor-row-height) * 2.5 + var(--sensor-gap)); }

  .trace::before {
    position: absolute;
    top: -3px;
    left: -3px;
    width: 7px;
    height: 7px;
    border: 1px solid var(--channel);
    border-radius: 50%;
    background: var(--login-board);
    content: '';
  }

  .processor {
    position: relative;
    display: grid;
    place-content: center;
    gap: .45rem;
    min-height: 8.85rem;
    padding: .6rem;
    border: 1px solid #789465;
    background: #222e23;
    color: var(--login-lime);
    text-align: center;
  }

  .processor strong {
    font-size: clamp(.95rem, 2.15vw, 1.35rem);
    font-weight: 600;
    letter-spacing: -.04em;
    white-space: nowrap;
  }

  .processor span {
    color: #ced8c8;
    font-size: .75rem;
    white-space: nowrap;
  }

  .processor::before,
  .processor::after {
    position: absolute;
    right: 15%;
    left: 15%;
    height: 7px;
    background: repeating-linear-gradient(90deg, #789465 0 2px, transparent 2px 8px);
    content: '';
  }

  .processor::before { top: -8px; }
  .processor::after { bottom: -8px; }

  .login-access {
    grid-area: access;
    display: grid;
    place-items: center;
    min-width: 0;
    padding: clamp(1.8rem, 5.4vw, 4.8rem);
    background: #f4f5ef;
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
    letter-spacing: -.06em;
  }

  form {
    display: grid;
    gap: 1.15rem;
  }

  .login-field {
    display: grid;
    gap: .48rem;
  }

  .login-field label {
    font-size: .9375rem;
    font-weight: 580;
  }

  .login-field input {
    width: 100%;
    min-height: 3.1rem;
    padding: .65rem .75rem;
    border: 1px solid var(--login-line);
    border-radius: 0;
    background: #fff;
    color: var(--login-ink);
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
    border-left-color: var(--login-line);
    background: #f4f5ef;
    color: #405049;
  }

  .password-toggle:hover {
    background: #e6ebdd;
    color: var(--login-ink);
  }

  .login-field input:focus-visible,
  .password-toggle:focus-visible,
  .submit-button:focus-visible {
    outline: 2px solid #4d7145;
    outline-offset: 2px;
  }

  .login-error {
    padding: .7rem .8rem;
    border-left: 3px solid #9d4340;
    background: #f4e6e2;
    color: #782f2b;
    font-size: .9375rem;
  }

  .submit-button {
    min-height: 3.1rem;
    padding: .62rem .9rem;
    border: 1px solid var(--login-ink);
    border-radius: 0;
    background: var(--login-lime);
    color: var(--login-ink);
    font-weight: 660;
  }

  .submit-button:hover:not(:disabled) { background: #c5e055; }

  .submit-button:disabled {
    background: #eceee7;
    border-color: var(--login-line);
    color: #56665a;
  }

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
      padding: 1.65rem clamp(.8rem, 5vw, 2rem);
    }

    .station h2 {
      max-width: none;
      font-size: clamp(1.4rem, 6.1vw, 2.2rem);
      letter-spacing: -.045em;
    }

    .login-access { padding: 1.75rem clamp(1rem, 5vw, 2.25rem) 2rem; }
  }

  @media (max-width: 22rem) {
    .signal-map { grid-template-columns: minmax(7.75rem, 1fr) minmax(1rem, .34fr) minmax(5.6rem, .72fr); }
    .sensor p, .sensor dl { padding-inline: .4rem; }
  }
</style>
