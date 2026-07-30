export interface FiddleManager {
  selectFiddle(key: string): boolean;
  resize(width: number, height: number, devicePixelRatio: number): void;
  tick(deltaSeconds: number): void;
  delete(): void;
}

export interface CanvasDemoModule {
  FiddleManager: {
    new (canvas: OffscreenCanvas, initialKey: string): FiddleManager;
  };
}

export interface CanvasDemoModuleOptions {
  print?(message: string): void;
  printErr?(message: string): void;
  preinitializedWebGPUDevice?: unknown;
}

export default function CreateCanvasDemoModule(
  options?: CanvasDemoModuleOptions,
): Promise<CanvasDemoModule>;
