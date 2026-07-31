export interface FiddleManager {
  selectFiddle(key: string): boolean;
  resize(width: number, height: number, devicePixelRatio: number): void;
  tick(deltaSeconds: number): void;
  delete(): void;
}

export interface CanvasDemoModule {
  loadFont(fontId: string, bytes: Uint8Array): boolean;
  loadImage(imageId: string, bytes: Uint8Array): boolean;
  FiddleManager: {
    new (canvas: OffscreenCanvas, initialKey: string): FiddleManager;
  };
}

export interface CanvasDemoModuleOptions {
  print?(message: string): void;
  printErr?(message: string): void;
}

export default function CreateCanvasDemoModule(
  options?: CanvasDemoModuleOptions,
): Promise<CanvasDemoModule>;
