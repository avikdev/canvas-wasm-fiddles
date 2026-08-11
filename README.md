# Canvas Wasm Fiddles

A Bun/Turbo workspace for small canvas experiments, backed by a standalone
C++/Bazel WebAssembly project.

## Workspace

- `fiddle-app` — Svelte 5 PWA with the fiddle browser.
- `canvas-worker` — TypeScript Web Worker that owns the canvas render loop.
- `cc-engine` — independent Bazel module that compiles a demo C++ API to Wasm.

```sh
bun install
bun run dev
```

## Deploy

### Vercel

Vercel deployments are built locally and uploaded as prebuilt output; the
repository does not need to be connected to Vercel.

```sh
bunx vercel pull --yes --environment=production
bunx vercel build --prod
bunx vercel deploy --prebuilt --prod
```

The Vercel build uses `/` as the application base, which is appropriate for a
Vercel URL or a custom domain such as `fiddles.example.com`. See the
[Vercel deployment guide](docs/vercel-deploy.md) for initial project setup,
secrets, DNS, and subsequent deployments.

### GitHub Pages

The production build uses `/canvas-wasm-fiddles/` as its Vite base path, so it
can coexist with other GitHub Pages projects at:

`https://avikdev.github.io/canvas-wasm-fiddles/`

Build and publish the app to the `gh-pages` branch:

```sh
bun install
bun run deploy
```

In the repository's GitHub settings, open **Pages**, choose **Deploy from a
branch**, and select the `gh-pages` branch with the `/ (root)` folder. The
deploy command builds the full workspace, adds `.nojekyll`, and publishes
`fiddle-app/dist` to that branch. Git must be authenticated for the `origin`
remote before running it.

## Build the standalone Wasm separately

```sh
./scripts/wasm-build-and-export.sh
```

## About the fiddles

- [Text Reflow](docs/about-fiddles.md#text-reflow)
- [Text Cutting](docs/about-fiddles.md#text-cutting)
- [Text Tracing](docs/about-fiddles.md#text-tracing)
- [Text Morphing](docs/about-fiddles.md#text-morphing)
- [Curve Interpolate](docs/about-fiddles.md#curve-interpolate)
- [Envelope Distort](docs/about-fiddles.md#env-distort)
- [Mesh Warp](docs/about-fiddles.md#mesh-warp)
- [Swirl Deform](docs/about-fiddles.md#swirl-deform)
- [Noise Deform](docs/about-fiddles.md#noise-deform)
- [Pucker and Bloat](docs/about-fiddles.md#pucker-bloat)
- [Shape intersection](docs/about-fiddles.md#shape-intersection)
- [Contour Lines](docs/about-fiddles.md#contour-lines)
- [Contour 2: Composite Field](docs/about-fiddles.md#contour-composite)
- [SkSL Shader](docs/about-fiddles.md#sksl-shader)
- [Scene Benchmark (WebGL)](docs/about-fiddles.md#scene-benchmark-webgl)
- [Scene Benchmark (CPU)](docs/about-fiddles.md#scene-benchmark-cpu)
