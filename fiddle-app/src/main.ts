import { registerSW } from "virtual:pwa-register";
import { mount } from "svelte";
import App from "./App.svelte";
import "./app.css";

registerSW({ immediate: true });

const app = mount(App, {
  target: document.getElementById("app") as HTMLElement,
});

export default app;
