/// <reference lib="webworker" />

import type { CanvasWorkerMessage } from "./index";
import CreateCanvasDemoModule, { type FiddleManager } from "./wasm/demo.js";

let fiddleManager: FiddleManager | undefined;
let selectedFiddle = "ribbon-field";
let width = 1;
let height = 1;
let dpr = 1;
let assetBaseUrl = "/";
let animationFrame = 0;
let animationPaused = false;
let previousFrameTime: number | undefined;
let didReportFirstFrame = false;

const externalFonts = [
  {
    id: "ibm-plex-mono",
    label: "IBM Plex Mono",
    path: "fonts/ibm-plex-mono/ibm-plex-mono-regular.woff2",
  },
  {
    id: "public-sans",
    label: "Public Sans",
    path: "fonts/public-sans/public-sans-regular.woff2",
  },
] as const;

const externalImages = [
  {
    id: "/images/demoimage-01.jpg",
    label: "Demo image 01",
    path: "images/demoimage-01.jpg",
  },
] as const;

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

function reportSvgCapability() {
  self.postMessage({
    type: "svg-capability",
    writable: fiddleManager?.isSvgWritable() ?? false,
  });
}

self.addEventListener("error", (event) => reportError(event.error ?? event.message));
self.addEventListener("unhandledrejection", (event) => reportError(event.reason));

async function createWasmModule() {
  return CreateCanvasDemoModule({
    print: (message) => reportLog(message),
    printErr: (message) => self.postMessage({ type: "error", message }),
  });
}

async function loadExternalFonts(wasmModule: Awaited<ReturnType<typeof createWasmModule>>) {
  await Promise.all(
    externalFonts.map(async (font) => {
      try {
        const response = await fetch(`${assetBaseUrl}${font.path}`);
        if (!response.ok) {
          throw new Error(`${response.status} ${response.statusText}`);
        }
        const bytes = new Uint8Array(await response.arrayBuffer());
        if (!wasmModule.loadFont(font.id, bytes)) {
          throw new Error("Skia could not decode the font data.");
        }
        reportLog(`Loaded external font ${font.label} (${bytes.byteLength} bytes).`);
      } catch (error) {
        reportError(new Error(`Could not load ${font.label}: ${describeError(error)}`));
      }
    }),
  );
}

async function loadExternalImages(wasmModule: Awaited<ReturnType<typeof createWasmModule>>) {
  await Promise.all(
    externalImages.map(async (image) => {
      try {
        const response = await fetch(`${assetBaseUrl}${image.path}`);
        if (!response.ok) {
          throw new Error(`${response.status} ${response.statusText}`);
        }
        const bytes = new Uint8Array(await response.arrayBuffer());
        if (!wasmModule.loadImage(image.id, bytes)) {
          throw new Error("Skia could not decode the image data.");
        }
        reportLog(`Loaded image ${image.label} (${bytes.byteLength} bytes).`);
      } catch (error) {
        reportError(new Error(`Could not load ${image.label}: ${describeError(error)}`));
      }
    }),
  );
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
  if (!animationPaused) {
    animationFrame = requestAnimationFrame(render);
  }
}

self.onmessage = async (event: MessageEvent<CanvasWorkerMessage>) => {
  const message = event.data;

  if (message.type === "init") {
    selectedFiddle = message.fiddle;
    assetBaseUrl = message.assetBaseUrl;
    width = message.width;
    height = message.height;
    dpr = message.dpr;
    animationPaused = message.paused;

    const wasmModule = await createWasmModule();
    await Promise.all([loadExternalFonts(wasmModule), loadExternalImages(wasmModule)]);
    fiddleManager?.delete();
    fiddleManager = new wasmModule.FiddleManager(message.canvas, selectedFiddle);
    fiddleManager.resize(width, height, dpr);
    self.postMessage({ type: "ready" });
    reportSvgCapability();

    previousFrameTime = undefined;
    didReportFirstFrame = false;
    cancelAnimationFrame(animationFrame);
    // Draw one initialized frame even when the fiddle starts paused. The
    // render callback will only schedule another frame when animation is on.
    animationFrame = requestAnimationFrame(render);
    return;
  }

  if (message.type === "resize") {
    width = message.width;
    height = message.height;
    dpr = message.dpr;
    fiddleManager?.resize(width, height, dpr);
    if (animationPaused) {
      fiddleManager?.tick(0);
    }
    return;
  }

  if (message.type === "animation") {
    if (animationPaused === message.paused) {
      return;
    }
    animationPaused = message.paused;
    cancelAnimationFrame(animationFrame);
    if (!animationPaused) {
      previousFrameTime = undefined;
      animationFrame = requestAnimationFrame(render);
    }
    return;
  }

  if (message.type === "select") {
    selectedFiddle = message.fiddle;
    const didSelect = fiddleManager?.selectFiddle(selectedFiddle);
    reportLog(`Selected ${selectedFiddle}: ${String(didSelect)}`);
    reportSvgCapability();
    return;
  }

  if (message.type === "export-svg") {
    try {
      if (!fiddleManager) {
        throw new Error("The renderer is not ready.");
      }
      if (!animationPaused) {
        throw new Error("Pause the animation before saving an SVG.");
      }
      if (!fiddleManager.isSvgWritable()) {
        throw new Error("This fiddle cannot be represented as SVG.");
      }
      const svg = fiddleManager.exportSvg();
      if (!svg) {
        throw new Error("Skia could not generate the SVG frame.");
      }
      self.postMessage({ type: "svg-export", requestId: message.requestId, svg });
    } catch (error) {
      self.postMessage({
        type: "svg-export",
        requestId: message.requestId,
        error: describeError(error),
      });
    }
  }
};
