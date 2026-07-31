import { svelte } from "@sveltejs/vite-plugin-svelte";
import tailwindcss from "@tailwindcss/vite";
import { defineConfig } from "vite";
import { VitePWA } from "vite-plugin-pwa";

const repositoryBase = "/canvas-wasm-fiddles/";

export default defineConfig({
  base: repositoryBase,
  plugins: [
    tailwindcss(),
    svelte(),
    VitePWA({
      registerType: "autoUpdate",
      includeAssets: ["icon.svg", "fonts/**/*.woff2"],
      workbox: {
        maximumFileSizeToCacheInBytes: 8 * 1024 * 1024,
      },
      manifest: {
        name: "Canvas Wasm Fiddles",
        short_name: "Canvas Fiddles",
        description: "A pocket gallery of worker-powered canvas experiments.",
        theme_color: "#17201c",
        background_color: "#f6f3ea",
        display: "standalone",
        start_url: repositoryBase,
        scope: repositoryBase,
        icons: [
          {
            src: `${repositoryBase}icon.svg`,
            sizes: "any",
            type: "image/svg+xml",
            purpose: "any maskable",
          },
        ],
      },
    }),
  ],
});
