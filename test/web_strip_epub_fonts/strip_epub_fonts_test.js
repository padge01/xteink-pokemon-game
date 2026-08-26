const assert = require("node:assert/strict");
const childProcess = require("node:child_process");
const fs = require("node:fs");
const path = require("node:path");
const test = require("node:test");
const vm = require("node:vm");
const zlib = require("node:zlib");

const repoRoot = path.join(__dirname, "..", "..");
const helperPath = path.join(repoRoot, "web", "lib", "strip-epub-fonts.js");
assert.ok(fs.existsSync(helperPath), "production embedded-font helper is missing");

const context = { TextEncoder };
vm.runInNewContext(fs.readFileSync(helperPath, "utf8"), context, { filename: helperPath });
for (const name of [
  "collectFontPaths",
  "stripFontFaceRules",
  "stripFontManifestItems",
  "stripFontEncryptionEntries",
]) {
  assert.equal(typeof context[name], "function", `production helper must define ${name}()`);
}

function makeZip(paths) {
  const files = Object.fromEntries(paths.map((entry) => [entry.path, { size: entry.size || 0 }]));
  return {
    files,
    forEach(callback) {
      for (const filePath of Object.keys(files)) callback(filePath, files[filePath]);
    },
  };
}

function runWebGenerator() {
  const script = path.join(repoRoot, "scripts", "build_web.py");
  const candidates = process.env.PYTHON
    ? [[process.env.PYTHON, []]]
    : process.platform === "win32"
      ? [
          ["python", []],
          ["py", ["-3"]],
        ]
      : [
          ["python3", []],
          ["python", []],
        ];

  for (const [command, prefix] of candidates) {
    const result = childProcess.spawnSync(command, [...prefix, script], { cwd: repoRoot, encoding: "utf8" });
    if (result.error?.code === "ENOENT") continue;
    assert.equal(result.status, 0, result.stderr || result.stdout);
    return;
  }

  assert.fail("Python 3 is required to run scripts/build_web.py");
}

function readGeneratedPage(identifier) {
  const header = fs.readFileSync(
    path.join(repoRoot, "src", "network", "html", `${identifier}.generated.h`),
    "utf8",
  );
  const compressed = Buffer.from(
    Array.from(header.matchAll(/0x([0-9a-f]{2})/gi), (match) => Number.parseInt(match[1], 16)),
  );
  return zlib.gunzipSync(compressed).toString("utf8");
}

test("finds embedded fonts by extension and OPF media type", () => {
  const zip = makeZip([
    { path: "OPS/fonts/Main.TTF" },
    { path: "OPS/fonts/Display Font.bin" },
    { path: "OPS/images/cover.jpg" },
  ]);
  const opf = `
    <opf:package xmlns:opf="http://www.idpf.org/2007/opf">
      <opf:manifest>
        <opf:item id="main" href="fonts/Main.TTF" media-type="application/octet-stream"/>
        <opf:item id="display" href="fonts/Display%20Font.bin" media-type="application/vnd.ms-opentype"/>
        <opf:item id="missing" href="fonts/Missing.bin" media-type="font/woff2"/>
        <opf:item id="cover" href="images/cover.jpg" media-type="image/jpeg"/>
      </opf:manifest>
    </opf:package>`;

  assert.deepEqual(
    Array.from(context.collectFontPaths(zip, opf, "OPS/content.opf")).sort(),
    ["OPS/fonts/Display Font.bin", "OPS/fonts/Main.TTF"],
  );
});

test("strips font-face rules while preserving surrounding CSS", () => {
  const input = `
    @font-face /* embedded */ {
      font-family: "Reader";
      src: url(data:font/woff2;base64,AA==) format("woff2");
    }
    body { font-family: "Reader", serif; }
    @FONT-FACE{font-family:Other;src:url(../fonts/other.otf)}
    p { margin: 0; }
  `;

  const result = context.stripFontFaceRules(input);

  assert.equal(result.count, 2);
  assert.doesNotMatch(result.css, /@font-face/i);
  assert.match(result.css, /body \{ font-family: "Reader", serif; \}/);
  assert.match(result.css, /p \{ margin: 0; \}/);
});

test("removes percent-encoded font entries from the OPF manifest", () => {
  const opf = `
    <package><manifest>
      <item id='font' href='fonts/Display%20Font.bin' media-type='application/vnd.ms-opentype'/>
      <item id="cover" href="images/cover.jpg" media-type="image/jpeg"/>
    </manifest></package>`;
  const fontPaths = new Set(["OPS/fonts/Display Font.bin"]);

  const result = context.stripFontManifestItems(opf, "OPS", fontPaths);

  assert.equal(result.count, 1);
  assert.doesNotMatch(result.xml, /id=['"]font['"]/);
  assert.match(result.xml, /id="cover"/);
});

test("removes only matching font-obfuscation entries from encryption.xml", () => {
  const xml = `<?xml version="1.0"?>
    <enc:encryption xmlns:enc="urn:oasis:names:tc:opendocument:xmlns:container"
                    xmlns:xenc="http://www.w3.org/2001/04/xmlenc#">
      <xenc:EncryptedData><xenc:CipherData><xenc:CipherReference URI="OPS/fonts/Display%20Font.otf"/></xenc:CipherData></xenc:EncryptedData>
      <xenc:EncryptedData><xenc:CipherData><xenc:CipherReference URI="OPS/chapters/secret.xhtml"/></xenc:CipherData></xenc:EncryptedData>
    </enc:encryption>`;

  const result = context.stripFontEncryptionEntries(xml, new Set(["OPS/fonts/Display Font.otf"]));

  assert.equal(result.modified, true);
  assert.equal(result.dropFile, undefined);
  assert.doesNotMatch(result.xml, /Display%20Font/);
  assert.match(result.xml, /secret\.xhtml/);
});

test("drops encryption.xml when it contained only removed font entries", () => {
  const xml = `<encryption><EncryptedData><CipherData><CipherReference URI="/OPS/fonts/main.otf"/></CipherData></EncryptedData></encryption>`;

  const result = context.stripFontEncryptionEntries(xml, new Set(["OPS/fonts/main.otf"]));

  assert.deepEqual({ ...result }, { dropFile: true });
});

test("the generated Files page includes every font-removal integration point", () => {
  runWebGenerator();

  const html = readGeneratedPage("FilesPageHtml");
  assert.match(html, /function collectFontPaths\(zip, opfContent, opfPath\)/);
  assert.match(html, /const fontPaths = collectFontPaths\(zip, opfContent, opfPath\)/);
  assert.match(html, /fixOPF\(t, opfContent, opfDir, splitImages, fontPaths\)/);
  assert.match(html, /if \(fontPaths\.has\(path\)\)/);
  assert.match(html, /stripFontFaceRules\(t\)/);
  assert.match(html, /stripFontEncryptionEntries\(t, fontPaths\)/);

  for (const identifier of ["HomePageHtml", "SettingsPageHtml", "FontsPageHtml"]) {
    assert.doesNotMatch(readGeneratedPage(identifier), /function collectFontPaths\(/);
  }
});
