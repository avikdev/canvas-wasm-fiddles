export type CanvasWorkerMessage =
  | {
      type: "init";
      canvas: OffscreenCanvas;
      fiddle?: string;
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
    }
  | {
      type: "input";
      key: string;
      value: string;
    }
  | {
      type: "image-input";
      key: string;
      imageId: string;
      bytes: ArrayBuffer;
    }
  | {
      type: "export-svg";
      requestId: number;
    };

export type CanvasWorkerStatus =
  | {
      type: "ready";
    }
  | {
      type: "initialization-error";
      message: string;
    }
  | {
      type: "first-frame";
    }
  | {
      type: "controls";
      controls: FiddleControlDefinition[];
    }
  | {
      type: "log";
      message: string;
    }
  | {
      type: "error";
      message: string;
    }
  | {
      type: "svg-capability";
      writable: boolean;
    }
  | {
      type: "svg-export";
      requestId: number;
      blob?: Blob;
      error?: string;
    };

export type FiddleControlType = "bool" | "option" | "range" | "text" | "para" | "image";

export type FiddleControlDefinition = {
  key: string;
  title: string;
  type: FiddleControlType;
  defaultValue: string;
  options: string[];
  min: number;
  max: number;
  step: number;
};
