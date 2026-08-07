const BLOB_URL_LIFETIME_MS = 60_000;

export function downloadSvg(blob: Blob, filename: string) {
  if (!blob.type.toLowerCase().startsWith("image/svg+xml")) {
    throw new Error("The canvas worker returned a file with an invalid SVG media type.");
  }

  const url = URL.createObjectURL(blob);
  const anchor = document.createElement("a");
  anchor.href = url;
  anchor.download = filename;
  anchor.type = blob.type;
  anchor.rel = "noopener";
  anchor.hidden = true;
  document.body.append(anchor);
  anchor.click();
  anchor.remove();

  // Chrome reads blob URLs asynchronously when handing them to its download
  // manager. Keeping the URL alive also avoids truncated files on slower
  // mobile devices.
  window.setTimeout(() => URL.revokeObjectURL(url), BLOB_URL_LIFETIME_MS);
}
