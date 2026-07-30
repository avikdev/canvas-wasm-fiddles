# Bootstrap

Bootstrap a turborepo at this project and also a C++ Bazel project in the repo.

- First bootstrap the C++ repo in the folder `cc-engine`. That has no relation with outside files, the turborepo. But it uses Bazel to build a wasm binary which will be used in a JS code.

- Study the external repo's code at `/Users/avikpaul/github-ujanxyz/cc-core/ujcore/wasm/...`, see the Bazel build file, how it builds a wasm binary. Similarly build a demo binary. No need tor pre-js / post-js hooks.

- The turbo repo does not have separate `apps` and `packages` groups, just top-level dirs for apps, packages, and the top level package json explicitly lists the child repos.

- Use bun as package manager, setup `biome` for lint and format. See the ext package file `/Users/avikpaul/github-oojanstudio/amberv1/package.json`

- These are the repos: (1) `canvas-worker`: TS code executed in web-worker context (2) A Svelte-5 PWA app (not SvelteKit), that has a long list of fiddles (start with one or two now). A fiddle is a code demonstration. User selects a fiddle from the left nav list, and it is render in the main panel.
Use shadcn-svelte and sleek IP for the navigation. Only two panel : left nav and main content.

#

- Are the cpp functions `waveSample`, `multiply`, `version` actually used in JS ? If not, remove them.

## Granular Bazel build rules.

Create separate Bazel build rules (e.g. `cc_library`) for every complete C++ lib (pair of .h and .cc files). For every C++ directory have a dedicated `BUILD.bazel` file there, don't directly include sources (`srcs`) from child or other dirs.

## Skia fiddle Optimize

The Skia fiddle is too slow. Let's measure if the slowness is due to the graphics computation of copy the buffer from existing surface to the offcreen canvas or to the final context.

Add timing computation in C++ for these core steps, and after every 10 steps, print in stdout (that'll come in the console) a combined line about these accumulated times in last 10 steps.

Question: Does it execute Skia's WebGPU backend like Canvaskit or the raster path ? Using WebGPU might need special emscripten build flags in bazel.

-----

The analysis has been running for a while now and its about time to wrap this up. If you're doing any deep analysis, finish the immediate search / step, try to leave the code in a functioning condition if you found one. Summary your findings in a separate MD file, so we can resume in future without wasting time on the things already explored.
