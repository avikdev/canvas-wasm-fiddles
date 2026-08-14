import { registerSW } from "virtual:pwa-register";
import { mount } from "svelte";
import App from "./App.svelte";
import "./app.css";
import { initializeAnalytics } from "./lib/analytics";

registerSW({ immediate: true });
initializeAnalytics();

const app = mount(App, {
  target: document.getElementById("app") as HTMLElement,
});

export default app;
