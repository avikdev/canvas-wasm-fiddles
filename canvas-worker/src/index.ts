export type CanvasWorkerMessage =
  | {
      type: "init";
      canvas: OffscreenCanvas;
      fiddle: string;
      assetBaseUrl: string;
      width: number;
      height: number;
      dpr: number;
      paused: boolean;
    }
  | {
      type: "resize";
      width: number;
      height: number;
      dpr: number;
    }
  | {
      type: "select";
      fiddle: string;
    }
  | {
      type: "animation";
      paused: boolean;
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
