import { describe, expect, test } from "bun:test";
import { createSvgBlob, finalizeSvgDocument } from "../src/svg-export";

describe("SVG export finalization", () => {
  test("adds portable document metadata", () => {
    const svg = finalizeSvgDocument('<svg width="320" height="180"><path d="M0 0L10 10"/></svg>');

    expect(svg).toStartWith('<?xml version="1.0" encoding="UTF-8"?>\n');
    expect(svg).toContain('xmlns="http://www.w3.org/2000/svg"');
    expect(svg).toContain('version="1.1"');
    expect(svg).toContain('viewBox="0 0 320 180"');
    expect(svg).toEndWith("</svg>\n");
  });

  test("preserves valid root metadata and adds XLink when used", () => {
    const svg = finalizeSvgDocument(
      '<svg xmlns="http://www.w3.org/2000/svg" width="12" height="8" viewBox="1 2 3 4"><use xlink:href="#shape"/></svg>',
    );

    expect(svg).toContain('viewBox="1 2 3 4"');
    expect(svg).toContain('xmlns:xlink="http://www.w3.org/1999/xlink"');
  });

  test("creates a typed Blob in the worker-compatible helper", async () => {
    const blob = createSvgBlob('<svg width="10" height="20"></svg>');

    expect(blob).toBeInstanceOf(Blob);
    expect(blob.type).toBe("image/svg+xml;charset=utf-8");
    expect(await blob.text()).toContain('viewBox="0 0 10 20"');
  });

  test.each([
    ["empty output", ""],
    ["missing root", "<g></g>"],
    ["incomplete output", '<svg width="10" height="10">'],
    ["invalid namespace", '<svg xmlns="https://example.com/svg" width="10" height="10"></svg>'],
    ["invalid dimensions", '<svg width="0" height="10"></svg>'],
    ["invalid viewBox", '<svg width="10" height="10" viewBox="0 0 0 10"></svg>'],
    ["null byte", '<svg width="10" height="10">\0</svg>'],
  ])("rejects %s", (_name, markup) => {
    expect(() => finalizeSvgDocument(markup)).toThrow();
  });
});
