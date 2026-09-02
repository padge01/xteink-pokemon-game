// CrossInk renders EPUBs with built-in or SD-card fonts, so publisher-embedded
// font files and their references only increase optimized book size.
const FONT_EXT_REGEX = /\.(ttf|otf|woff2?|eot)$/i;
const FONT_MEDIA_TYPE_REGEX = /^(font\/|application\/(font-|x-font-|vnd\.ms-(opentype|fontobject)$))/i;

function decodeFontHref(href) {
  try {
    return decodeURIComponent(href);
  } catch (e) {
    return href;
  }
}

function normalizeEpubPath(path) {
  const parts = [];
  for (const part of path.replace(/^\/+/, "").split("/")) {
    if (!part || part === ".") continue;
    if (part === "..") parts.pop();
    else parts.push(part);
  }
  return parts.join("/");
}

function resolveFontHref(opfDir, href) {
  const decoded = decodeFontHref(href).split("#", 1)[0].split("?", 1)[0];
  return normalizeEpubPath(decoded.startsWith("/") ? decoded : `${opfDir ? `${opfDir}/` : ""}${decoded}`);
}

function xmlTagAttribute(tag, name) {
  const match = tag.match(new RegExp(`\\b${name}\\s*=\\s*(["'])([\\s\\S]*?)\\1`, "i"));
  return match ? match[2] : "";
}

function isFontMediaType(mediaType) {
  return FONT_MEDIA_TYPE_REGEX.test(mediaType.trim());
}

// Returns zip-root-relative font paths. Extension detection covers normal EPUBs;
// manifest media types also catch obfuscated fonts with unusual extensions.
function collectFontPaths(zip, opfContent, opfPath) {
  const fontPaths = new Set();
  zip.forEach((path, file) => {
    if (!file.dir && FONT_EXT_REGEX.test(path)) fontPaths.add(path);
  });

  if (!opfContent || !opfPath) return fontPaths;
  const opfDir = opfPath.includes("/") ? opfPath.substring(0, opfPath.lastIndexOf("/")) : "";
  const addManifestFont = (href, mediaType) => {
    if (!href || !isFontMediaType(mediaType)) return;
    const resolved = resolveFontHref(opfDir, href);
    if (Object.prototype.hasOwnProperty.call(zip.files, resolved)) fontPaths.add(resolved);
  };

  let parsed = false;
  try {
    const doc = new DOMParser().parseFromString(opfContent, "application/xml");
    if (!doc.querySelector("parsererror")) {
      parsed = true;
      for (const item of Array.from(doc.getElementsByTagNameNS("*", "item"))) {
        addManifestFont(item.getAttribute("href") || "", item.getAttribute("media-type") || "");
      }
    }
  } catch (e) {
    // The regex fallback below covers malformed OPF files and host-side tests.
  }

  if (!parsed) {
    for (const match of opfContent.matchAll(/<(?:[\w.-]+:)?item\b[^>]*\/?\s*>/gi)) {
      addManifestFont(xmlTagAttribute(match[0], "href"), xmlTagAttribute(match[0], "media-type"));
    }
  }
  return fontPaths;
}

// Also removes data-URI fonts, which may be much larger than the rest of a
// stylesheet even when the EPUB contains no separate font files.
function stripFontFaceRules(css) {
  let count = 0;
  const stripped = css.replace(/@font-face(?:\s|\/\*[\s\S]*?\*\/)*\{[^{}]*\}/gi, () => {
    count++;
    return "";
  });
  return { css: stripped, count };
}

function stripFontManifestItems(xmlText, opfDir, fontPaths) {
  let count = 0;
  const xml = xmlText.replace(
    /<(?:[\w.-]+:)?item\b[^>]*?(?:\/\s*>|>\s*<\/(?:[\w.-]+:)?item\s*>)/gi,
    (item) => {
      const href = xmlTagAttribute(item, "href");
      if (!href || !fontPaths.has(resolveFontHref(opfDir, href))) return item;
      count++;
      return "";
    },
  );
  return { xml, count };
}

function encryptionUriMatchesFont(uri, fontPaths) {
  return fontPaths.has(normalizeEpubPath(decodeFontHref(uri)));
}

// Removes font-obfuscation entries. Other encrypted resources are retained;
// the whole file is dropped only when no encryption entries remain.
function stripFontEncryptionEntries(xmlText, fontPaths) {
  try {
    const doc = new DOMParser().parseFromString(xmlText, "application/xml");
    if (!doc.querySelector("parsererror")) {
      let removed = 0;
      for (const entry of Array.from(doc.getElementsByTagNameNS("*", "EncryptedData"))) {
        const ref = entry.getElementsByTagNameNS("*", "CipherReference")[0];
        const uri = ref ? ref.getAttribute("URI") || "" : "";
        if (!encryptionUriMatchesFont(uri, fontPaths)) continue;
        entry.parentNode.removeChild(entry);
        removed++;
      }
      if (!removed) return { modified: false };
      const remaining =
        doc.getElementsByTagNameNS("*", "EncryptedData").length +
        doc.getElementsByTagNameNS("*", "EncryptedKey").length;
      if (remaining === 0) return { dropFile: true };
      return { modified: true, xml: safeSerialize(doc, xmlText) };
    }
  } catch (e) {
    // Fall through to a conservative string-based cleanup.
  }

  let removed = 0;
  const xml = xmlText.replace(
    /<(?:[\w.-]+:)?EncryptedData\b[^>]*>[\s\S]*?<\/(?:[\w.-]+:)?EncryptedData\s*>/gi,
    (entry) => {
      const ref = entry.match(/<(?:[\w.-]+:)?CipherReference\b[^>]*\/?\s*>/i);
      const uri = ref ? xmlTagAttribute(ref[0], "URI") : "";
      if (!uri || !encryptionUriMatchesFont(uri, fontPaths)) return entry;
      removed++;
      return "";
    },
  );
  if (!removed) return { modified: false };
  if (!/<(?:[\w.-]+:)?Encrypted(?:Data|Key)\b/i.test(xml)) return { dropFile: true };
  return { modified: true, xml };
}
