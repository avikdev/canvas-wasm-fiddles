## Font loading

Attempt this if you can, if not feasible skip this.
But first complete all the above changes, do testing and handoff and then start this.

Try to load the 2 fonts from the public dirs in the web-worker using fetch (maybe bloc or file asset), pass to C++ wasm. It can be native JS object, but figue out in which JS format is most suitable for this.

In C++ side, setup a global Skia FontManager. Maybe use a singleton C++ class for that (use anonymous static method to initialize the singleton in .cc file). It can receive the font file, and load it so that the font can be used in text rendering. Here some font init logic involved, figure out the right way. if needed consult Skia repo and example code in the official Skia repo. I think you might need the paragraph related extra Bazel deps.

Use these 2 fonts:
- IBM Plex Mono
- Public Sans
You'll find the font files in an external repo here:

`/Users/avikpaul/github-oojanstudio/amberv1/packages/fecore/src/lib/assets/fonts/`

Copy the necessary font files to the public dir of the vite app, so that they can be loaded via a fetch call from the app. You should copy at least the regular files.

Finally after external font loading is correctly setup in the fiddle, cycle these 2 external fonts and the default / fallback font, total 3 fonts. This change can also be aligned with alignment change.

The animation is too fast, to make it less dramatic change the width between 75% and 40%
