type Gtag = (...args: unknown[]) => void;

declare global {
  interface Window {
    dataLayer: unknown[];
    gtag: Gtag;
  }
}

type SelectionMethod = "automatic" | "user";
type EngagementReason = "fiddle_change" | "hidden" | "page_exit";

type ActiveFiddle = {
  key: string;
  accumulatedMilliseconds: number;
  visibleSince?: number;
};

const measurementId = import.meta.env.VITE_GA_MEASUREMENT_ID?.trim();
const validMeasurementId = /^G-[A-Z0-9]+$/i.test(measurementId ?? "");
let initialized = false;
let activeFiddle: ActiveFiddle | undefined;

function analyticsEnabled() {
  return initialized && typeof window !== "undefined" && typeof window.gtag === "function";
}

function sendEvent(name: string, parameters: Record<string, string | number>) {
  if (!analyticsEnabled()) return;
  window.gtag("event", name, parameters);
}

function pauseActiveTimer() {
  if (!activeFiddle || activeFiddle.visibleSince === undefined) return;
  activeFiddle.accumulatedMilliseconds += performance.now() - activeFiddle.visibleSince;
  activeFiddle.visibleSince = undefined;
}

function resumeActiveTimer() {
  if (
    !activeFiddle ||
    activeFiddle.visibleSince !== undefined ||
    document.visibilityState !== "visible"
  )
    return;
  activeFiddle.visibleSince = performance.now();
}

function flushFiddleEngagement(reason: EngagementReason, endSession: boolean) {
  if (!activeFiddle) return;
  pauseActiveTimer();
  const duration = Math.round(activeFiddle.accumulatedMilliseconds);
  if (duration > 0) {
    sendEvent("fiddle_engagement", {
      fiddle_key: activeFiddle.key,
      engagement_reason: reason,
      engagement_time_msec: duration,
      fiddle_time_msec: duration,
    });
  }
  if (endSession) {
    activeFiddle = undefined;
  } else {
    activeFiddle.accumulatedMilliseconds = 0;
  }
}

function fiddlePagePath(fiddleKey: string) {
  const base = import.meta.env.BASE_URL.endsWith("/")
    ? import.meta.env.BASE_URL
    : `${import.meta.env.BASE_URL}/`;
  return `${base}fiddles/${encodeURIComponent(fiddleKey)}`;
}

export function initializeAnalytics() {
  if (initialized || !import.meta.env.PROD || !validMeasurementId || !measurementId) return;

  initialized = true;
  window.gtag("js", new Date());
  window.gtag("config", measurementId);

  const script = document.createElement("script");
  script.async = true;
  script.src = `https://www.googletagmanager.com/gtag/js?id=${encodeURIComponent(measurementId)}`;
  document.head.append(script);

  document.addEventListener("visibilitychange", () => {
    if (document.visibilityState === "hidden") {
      flushFiddleEngagement("hidden", false);
    } else {
      resumeActiveTimer();
    }
  });
  window.addEventListener("pagehide", () => flushFiddleEngagement("page_exit", false));
  window.addEventListener("pageshow", resumeActiveTimer);
}

export function trackFiddleView(
  fiddleKey: string,
  fiddleTitle: string,
  selectionMethod: SelectionMethod,
) {
  flushFiddleEngagement("fiddle_change", true);
  activeFiddle = {
    key: fiddleKey,
    accumulatedMilliseconds: 0,
    visibleSince: document.visibilityState === "visible" ? performance.now() : undefined,
  };

  const pagePath = fiddlePagePath(fiddleKey);
  sendEvent("page_view", {
    page_title: `${fiddleTitle} · Canvas Wasm Fiddles`,
    page_location: new URL(pagePath, window.location.origin).href,
    page_path: pagePath,
    fiddle_key: fiddleKey,
  });
  sendEvent("fiddle_view", {
    fiddle_key: fiddleKey,
    selection_method: selectionMethod,
  });
}

export function trackFiddleClick(fiddleKey: string) {
  sendEvent("fiddle_select", { fiddle_key: fiddleKey });
}

export function trackSvgSave(fiddleKey: string) {
  sendEvent("save_svg", { fiddle_key: fiddleKey });
}

export function trackSvgSaveFailure(fiddleKey: string) {
  sendEvent("save_svg_failure", { fiddle_key: fiddleKey });
}
