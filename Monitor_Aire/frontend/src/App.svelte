<script>
  import { onMount } from 'svelte';
  import { api, postJson } from './models/api';
  import Dashboard from './GUI/views/Dashboard.svelte';
  import Login from './GUI/views/Login.svelte';

  let user = $state(null);
  let checking = $state(true);
  let busy = $state(false);
  let authError = $state('');

  onMount(async () => {
    try {
      user = await api('/auth/me');
    } catch (error) {
      if (error.status !== 401) authError = 'No se pudo conectar al servidor. Intenta nuevamente.';
    } finally {
      checking = false;
    }
  });

  async function login(username, password) {
    busy = true;
    authError = '';
    try {
      user = await postJson('/auth/login', { username, password });
    } catch (error) {
      authError = error.status === 401 || error.status === 422
        ? 'Revisa el usuario y la contraseña.'
        : 'No se pudo iniciar sesión. Intenta nuevamente.';
    } finally {
      busy = false;
    }
  }

  async function logout() {
    busy = true;
    try {
      await api('/auth/logout', { method: 'POST' });
      user = null;
    } catch (error) {
      if (error.status === 401) user = null;
      else throw error;
    } finally {
      busy = false;
    }
  }

  function expireSession() {
    user = null;
    authError = 'Tu sesión venció. Inicia sesión nuevamente.';
  }
</script>

{#if checking}
  <main class="session-loading" aria-live="polite">Cargando monitor…</main>
{:else if user}
  <Dashboard {user} onlogout={logout} onexpire={expireSession} loggingOut={busy} />
{:else}
  <Login onlogin={login} pending={busy} error={authError} />
{/if}
