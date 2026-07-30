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
let didReportFirstFrame = false;

function describeError(error: unknown) {
  if (error instanceof Error) {
    return `${error.name}: ${error.message}${error.stack ? `\n${error.stack}` : ""}`;
  }
  return String(error);
}

function reportError(error: unknown) {
  self.postMessage({ type: "error", message: describeError(error) });
}

function reportLog(message: string) {
  self.postMessage({ type: "log", message });
}

self.addEventListener("error", (event) => reportError(event.error ?? event.message));
self.addEventListener("unhandledrejection", (event) => reportError(event.reason));

async function createWasmModule() {
  return CreateCanvasDemoModule({
    print: (message) => reportLog(message),
    printErr: (message) => self.postMessage({ type: "error", message }),
  });
}

function render(now: number) {
  const deltaSeconds =
    previousFrameTime === undefined ? 0 : Math.min((now - previousFrameTime) / 1000, 0.1);
  previousFrameTime = now;
  try {
    fiddleManager?.tick(deltaSeconds);
    if (fiddleManager && !didReportFirstFrame) {
      didReportFirstFrame = true;
      reportLog("First animation frame rendered.");
    }
  } catch (error) {
    reportError(error);
    return;
  }
  animationFrame = requestAnimationFrame(render);
}

self.onmessage = async (event: MessageEvent<CanvasWorkerMessage>) => {
  const message = event.data;

  if (message.type === "init") {
    selectedFiddle = message.fiddle;
    width = message.width;
    height = message.height;
    dpr = message.dpr;

    const wasmModule = await createWasmModule();
    fiddleManager?.delete();
    fiddleManager = new wasmModule.FiddleManager(message.canvas, selectedFiddle);
    fiddleManager.resize(width, height, dpr);
    self.postMessage({ type: "ready" });

    previousFrameTime = undefined;
    didReportFirstFrame = false;
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
  const didSelect = fiddleManager?.selectFiddle(selectedFiddle);
  reportLog(`Selected ${selectedFiddle}: ${String(didSelect)}`);
};
