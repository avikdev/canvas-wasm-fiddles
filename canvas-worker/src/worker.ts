/// <reference lib="webworker" />

import type { CanvasWorkerMessage, FiddleId } from "./index";
import CreateCanvasDemoModule, { type FiddleManager } from "./wasm/demo.js";

let fiddleManager: FiddleManager | undefined;
let selectedFiddle: FiddleId = "orbital-bloom";
let width = 1;
let height = 1;
let dpr = 1;
let animationFrame = 0;
let previousFrameTime: number | undefined;

const wasmModulePromise = CreateCanvasDemoModule({
  print: (message) => console.log(`[cc-engine] ${message}`),
  printErr: (message) => console.error(`[cc-engine] ${message}`),
});

function render(now: number) {
  const deltaSeconds =
    previousFrameTime === undefined ? 0 : Math.min((now - previousFrameTime) / 1000, 0.1);
  previousFrameTime = now;
  fiddleManager?.tick(deltaSeconds);
  animationFrame = requestAnimationFrame(render);
}

self.onmessage = async (event: MessageEvent<CanvasWorkerMessage>) => {
  const message = event.data;

  if (message.type === "init") {
    selectedFiddle = message.fiddle;
    width = message.width;
    height = message.height;
    dpr = message.dpr;

    const wasmModule = await wasmModulePromise;
    fiddleManager?.delete();
    fiddleManager = new wasmModule.FiddleManager(message.canvas);
    fiddleManager.resize(width, height, dpr);
    if (selectedFiddle !== "orbital-bloom") {
      fiddleManager.selectFiddle(selectedFiddle);
    }

    previousFrameTime = undefined;
    cancelAnimationFrame(animationFrame);
    animationFrame = requestAnimationFrame(render);
    return;
  }

  if (message.type === "resize") {
    width = message.width;
    height = message.height;
    dpr = message.dpr;
    fiddleManager?.resize(width, height, dpr);
    return;
  }

  selectedFiddle = message.fiddle;
  fiddleManager?.selectFiddle(selectedFiddle);
};
