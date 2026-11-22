import { mount } from 'svelte';
import '@fontsource-variable/space-grotesk';
import '@fontsource/ibm-plex-mono/latin-400.css';
import App from './App.svelte';
import './style.css';

// Monta el panel en el contenedor definido por index.html.
mount(App, { target: document.getElementById('app') });
