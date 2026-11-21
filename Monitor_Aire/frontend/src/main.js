import { mount } from 'svelte';
import '@fontsource-variable/space-grotesk';
import '@fontsource/ibm-plex-mono/latin-400.css';
import App from './App.svelte';
import './style.css';

mount(App, { target: document.getElementById('app') });
