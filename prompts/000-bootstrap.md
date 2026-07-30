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

# C++ fiddle manager.

Now move the fiddle engine inside C++. Define a class (with proper header.h and source .cc files) named `FiddleManager`. It will support:
- Time tracking, increment time on every call.
- Change fiddle. Select by same key as before.

- Now the offscreen canvas is sent to the C++ wasm side. And C++ draws on it directly. Uses emscripten `val` getters to get the width, height, context etc from the canvas.

- Fiddles are derived classes from a base class `FiddleBase`, which are all registered in a registry. The class supports populating a canvas JS object (emscripten).

- Define a class `FiddleRegistry` for registering fiddles using a string key, and a creator function pointer.

- Worker now inits the fiddle manager in wasm at beginning / mount. Immediately activates the first fiddle. On change in selection, sends the new key.
