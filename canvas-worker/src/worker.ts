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

type WebGpuDevice = object;

interface WebGpuAdapter {
  requestDevice(): Promise<WebGpuDevice>;
}

interface WebGpu {
  requestAdapter(): Promise<WebGpuAdapter | null>;
  getPreferredCanvasFormat(): string;
}

interface WebGpuCanvasContext {
  configure(configuration: {
    device: WebGpuDevice;
    format: string;
    alphaMode: "premultiplied";
  }): void;
}

type WebGpuOffscreenCanvas = OffscreenCanvas & {
  __webgpuFormat?: string;
};

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

async function createWasmModule(fiddle: FiddleId, canvas: OffscreenCanvas) {
  let preinitializedWebGPUDevice: WebGpuDevice | undefined;

  if (fiddle === "skia-pulse") {
    const gpu = (navigator as WorkerNavigator & { gpu?: WebGpu }).gpu;
    if (!gpu) {
      throw new Error("WebGPU is unavailable in this worker.");
    }

    const adapter = await gpu.requestAdapter();
    if (!adapter) {
      throw new Error("WebGPU could not find a compatible GPU adapter.");
    }

    const device = await adapter.requestDevice();
    preinitializedWebGPUDevice = device;
    const canvasContext = canvas.getContext("webgpu") as unknown as
      | WebGpuCanvasContext
      | null;
    if (!canvasContext) {
      throw new Error("OffscreenCanvas could not create a WebGPU context.");
    }
    const canvasFormat = gpu.getPreferredCanvasFormat();
    canvasContext.configure({
      device,
      format: canvasFormat,
      alphaMode: "premultiplied",
    });
    (canvas as WebGpuOffscreenCanvas).__webgpuFormat = canvasFormat;
    reportLog("WebGPU device and OffscreenCanvas context configured.");
  }

  return CreateCanvasDemoModule({
    print: (message) => reportLog(message),
    printErr: (message) => self.postMessage({ type: "error", message }),
    preinitializedWebGPUDevice,
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

    const wasmModule = await createWasmModule(selectedFiddle, message.canvas);
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
