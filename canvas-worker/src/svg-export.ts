const SVG_NAMESPACE = "http://www.w3.org/2000/svg";
const XLINK_NAMESPACE = "http://www.w3.org/1999/xlink";
const XML_DECLARATION = '<?xml version="1.0" encoding="UTF-8"?>';

function escapeRegExp(value: string) {
  return value.replace(/[.*+?^${}()|[\]\\]/gu, "\\$&");
}

function rootAttribute(rootTag: string, name: string): string | undefined {
  const attribute = new RegExp(
    `(?:^|\\s)${escapeRegExp(name)}\\s*=\\s*(?:"([^"]*)"|'([^']*)')`,
    "u",
  ).exec(rootTag);
  return attribute?.[1] ?? attribute?.[2];
}

function numericLength(value: string | undefined): number | undefined {
  if (!value) return undefined;
  const parsed = Number.parseFloat(value);
  return Number.isFinite(parsed) && parsed > 0 ? parsed : undefined;
}

function validateViewBox(value: string) {
  const components = value
    .trim()
    .split(/[\s,]+/u)
    .map((component) => Number(component));
  const width = components[2];
  const height = components[3];
  if (
    components.length !== 4 ||
    components.some((component) => !Number.isFinite(component)) ||
    width === undefined ||
    height === undefined ||
    width <= 0 ||
    height <= 0
  ) {
    throw new Error("The SVG root has an invalid viewBox.");
  }
}

function findTagEnd(markup: string, start: number) {
  let quote = "";
  for (let index = start; index < markup.length; index += 1) {
    const character = markup[index];
    if (quote) {
      if (character === quote) quote = "";
    } else if (character === '"' || character === "'") {
      quote = character;
    } else if (character === ">") {
      return index;
    }
  }
  return -1;
}

/**
 * Checks the element nesting and document boundary produced by Skia's trusted
 * XML writer. This deliberately rejects DTDs: exported artwork does not need
 * them, and accepting external entities would make the file less portable.
 */
function validateXmlStructure(markup: string) {
  const stack: string[] = [];
  let rootSeen = false;
  let rootClosed = false;
  let position = 0;

  while (position < markup.length) {
    const tagStart = markup.indexOf("<", position);
    if (tagStart < 0) {
      if ((!rootSeen || rootClosed) && markup.slice(position).trim()) {
        throw new Error("The SVG document has content outside its root element.");
      }
      break;
    }
    if ((!rootSeen || rootClosed) && markup.slice(position, tagStart).trim()) {
      throw new Error("The SVG document has content outside its root element.");
    }

    if (markup.startsWith("<!--", tagStart)) {
      const end = markup.indexOf("-->", tagStart + 4);
      if (end < 0) throw new Error("The SVG document has an unterminated comment.");
      position = end + 3;
      continue;
    }
    if (markup.startsWith("<![CDATA[", tagStart)) {
      if (!rootSeen || rootClosed) {
        throw new Error("The SVG document has CDATA outside its root element.");
      }
      const end = markup.indexOf("]]>", tagStart + 9);
      if (end < 0) throw new Error("The SVG document has an unterminated CDATA section.");
      position = end + 3;
      continue;
    }
    if (markup.startsWith("<?", tagStart)) {
      const end = markup.indexOf("?>", tagStart + 2);
      if (end < 0) {
        throw new Error("The SVG document has an unterminated processing instruction.");
      }
      position = end + 2;
      continue;
    }
    if (markup.startsWith("<!", tagStart)) {
      throw new Error("SVG exports must not contain a document type declaration.");
    }

    const tagEnd = findTagEnd(markup, tagStart + 1);
    if (tagEnd < 0) throw new Error("The SVG document has an unterminated element.");
    const tagBody = markup.slice(tagStart + 1, tagEnd).trim();
    const closing = tagBody.startsWith("/");
    const selfClosing = !closing && tagBody.endsWith("/");
    const nameMatch = (closing ? tagBody.slice(1) : tagBody).match(/^([A-Za-z_][\w:.-]*)/u);
    const name = nameMatch?.[1];
    if (!name) throw new Error("The SVG document contains an invalid element.");

    if (closing) {
      if (stack.pop() !== name) {
        throw new Error(`The SVG document has a mismatched closing </${name}> element.`);
      }
      if (stack.length === 0) rootClosed = true;
    } else {
      if (rootClosed) {
        throw new Error("The SVG document contains more than one root element.");
      }
      if (!rootSeen) {
        if (name !== "svg") {
          throw new Error("The renderer output does not have an SVG root element.");
        }
        rootSeen = true;
      }
      if (!selfClosing) stack.push(name);
      else if (stack.length === 0) rootClosed = true;
    }
    position = tagEnd + 1;
  }

  if (!rootSeen || !rootClosed || stack.length > 0) {
    throw new Error("The renderer returned an incomplete SVG document.");
  }
}

export function finalizeSvgDocument(markup: string): string {
  if (!markup.trim()) {
    throw new Error("The renderer returned an empty SVG document.");
  }
  if (markup.includes("\0")) {
    throw new Error("The renderer returned an SVG document containing a null byte.");
  }

  const withoutDeclaration = markup.replace(/^\uFEFF?\s*<\?xml[^?]*\?>\s*/u, "").trim();
  validateXmlStructure(withoutDeclaration);

  const rootMatch = /<svg\b[^>]*>/u.exec(withoutDeclaration);
  if (!rootMatch) {
    throw new Error("The renderer output does not have an SVG root element.");
  }
  const rootTag = rootMatch[0];
  const additions: string[] = [];
  const namespace = rootAttribute(rootTag, "xmlns");
  if (namespace && namespace !== SVG_NAMESPACE) {
    throw new Error("The SVG root has an invalid XML namespace.");
  }
  if (!namespace) additions.push(`xmlns="${SVG_NAMESPACE}"`);

  const xlinkNamespace = rootAttribute(rootTag, "xmlns:xlink");
  if (xlinkNamespace && xlinkNamespace !== XLINK_NAMESPACE) {
    throw new Error("The SVG root has an invalid XLink namespace.");
  }
  if (withoutDeclaration.includes("xlink:") && !xlinkNamespace) {
    additions.push(`xmlns:xlink="${XLINK_NAMESPACE}"`);
  }
  if (!rootAttribute(rootTag, "version")) additions.push('version="1.1"');

  const width = numericLength(rootAttribute(rootTag, "width"));
  const height = numericLength(rootAttribute(rootTag, "height"));
  if (!width || !height) {
    throw new Error("The SVG root is missing valid width or height metadata.");
  }
  const viewBox = rootAttribute(rootTag, "viewBox");
  if (viewBox) validateViewBox(viewBox);
  else additions.push(`viewBox="0 0 ${width} ${height}"`);

  const normalizedRoot = additions.length
    ? rootTag.replace(/^<svg\b/u, `<svg ${additions.join(" ")}`)
    : rootTag;
  const normalized = `${withoutDeclaration.slice(0, rootMatch.index)}${normalizedRoot}${withoutDeclaration.slice(rootMatch.index + rootTag.length)}`;
  validateXmlStructure(normalized);

  return `${XML_DECLARATION}\n${normalized}\n`;
}

export function createSvgBlob(markup: string): Blob {
  return new Blob([finalizeSvgDocument(markup)], {
    type: "image/svg+xml;charset=utf-8",
  });
}
