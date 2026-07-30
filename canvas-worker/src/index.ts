export const fiddleIds = [
  "orbital-bloom",
  "ribbon-field",
  "skia-webgl",
  "skia-cpu",
  "elastic-text",
] as const;

export type FiddleId = (typeof fiddleIds)[number];

export type CanvasWorkerMessage =
  | {
      type: "init";
      canvas: OffscreenCanvas;
      fiddle: FiddleId;
      width: number;
      height: number;
      dpr: number;
    }
  | {
      type: "resize";
      width: number;
      height: number;
      dpr: number;
    }
  | {
      type: "select";
      fiddle: FiddleId;
    };

export type CanvasWorkerStatus =
  | {
      type: "ready";
    }
  | {
      type: "log";
      message: string;
    }
  | {
      type: "error";
      message: string;
    };
