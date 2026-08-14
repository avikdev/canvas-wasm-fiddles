export interface FiddleManager {
  selectFiddle(key: string): boolean;
  isSvgWritable(): boolean;
  widgets(): EmbindVector<FiddleWidget>;
  setInput(name: string, value: string): boolean;
  exportSvg(): string;
  resize(width: number, height: number, devicePixelRatio: number): void;
  tick(deltaSeconds: number): void;
  delete(): void;
}

export interface EmbindVector<T> {
  size(): number;
  get(index: number): T | undefined;
  delete(): void;
}

export interface FiddleWidget {
  name: string;
  type: string;
  defaultValue: string;
  options: EmbindVector<string>;
  min: number;
  max: number;
  step: number;
}

export interface CanvasDemoModule {
  loadFont(fontId: string, bytes: Uint8Array): boolean;
  loadImageBitmap(imageId: string, bitmap: ImageBitmap): boolean;
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
