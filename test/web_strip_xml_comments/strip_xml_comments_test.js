const assert = require("node:assert/strict");
const childProcess = require("node:child_process");
const fs = require("node:fs");
const path = require("node:path");
const test = require("node:test");
const vm = require("node:vm");
const zlib = require("node:zlib");

const repoRoot = path.join(__dirname, "..", "..");
const helperPath = path.join(repoRoot, "web", "lib", "strip-xml-comments.js");
assert.ok(fs.existsSync(helperPath), "production stripComments helper is missing");

const context = { TextEncoder };
vm.runInNewContext(fs.readFileSync(helperPath, "utf8"), context, { filename: helperPath });
assert.equal(typeof context.stripComments, "function", "production helper must define stripComments(xml)");

const stripComments = context.stripComments;

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

test("removes complete comments before, inside, and after the root", () => {
  const result = stripComments("<!--a--><root>x<!--bc-->y</root><!--d-->");

  assert.equal(result.text, "<root>xy</root>");
  assert.equal(result.count, 3);
  assert.equal(result.bytes, 25);
});

test("counts removed comment bytes as UTF-8", () => {
  const result = stripComments("<root><!--é🙂--></root>");

  assert.equal(result.text, "<root></root>");
  assert.equal(result.count, 1);
  assert.equal(result.bytes, 13);
});

test("preserves comment-looking text in protected XML constructs", () => {
  const input =
    '<?keep <!-- pi -->?><!DOCTYPE root [<!ENTITY marker "<!-- entity -->">]>' +
    "<root><![CDATA[<!-- cdata -->]]><!--drop--><child/></root>";
  const result = stripComments(input);

  assert.equal(
    result.text,
    '<?keep <!-- pi -->?><!DOCTYPE root [<!ENTITY marker "<!-- entity -->">]>' +
      "<root><![CDATA[<!-- cdata -->]]><child/></root>",
  );
  assert.equal(result.count, 1);
  assert.equal(result.bytes, 11);
});

test("preserves DTD comments and processing instructions while tracking the internal subset", () => {
  const doctype =
    '<!DOCTYPE root [<!-- ] > --><!-- [ --><?keep ] > [ ?><!ENTITY marker "ok">]>';
  const result = stripComments(`${doctype}<root><!--drop--></root>`);

  assert.equal(result.text, `${doctype}<root></root>`);
  assert.equal(result.count, 1);
  assert.equal(result.bytes, 11);
});

test("preserves an unterminated DOCTYPE even when its DTD comments contain structural characters", () => {
  const input = "<!DOCTYPE root [<!-- ] > --><!-- still DTD -->";
  const result = stripComments(input);

  assert.equal(result.text, input);
  assert.equal(result.count, 0);
  assert.equal(result.bytes, 0);
});

test("keeps adjacent tags adjacent after comment removal", () => {
  const result = stripComments("<a/><!-- remove --><b/>");

  assert.equal(result.text, "<a/><b/>");
  assert.equal(result.count, 1);
  assert.equal(result.bytes, 15);
});

test("preserves the remaining source when a protected construct is unterminated", async (t) => {
  const cases = [
    "<root><!-- unfinished",
    "<root><![CDATA[<!-- keep",
    "<?instruction <!-- keep",
    '<!DOCTYPE root [<!ENTITY marker "<!-- keep -->">',
  ];

  for (const input of cases) {
    await t.test(input, () => {
      const result = stripComments(input);
      assert.equal(result.text, input);
      assert.equal(result.count, 0);
      assert.equal(result.bytes, 0);
    });
  }
});

test("the generated Files page includes and invokes the production helper", () => {
  runWebGenerator();

  const html = readGeneratedPage("FilesPageHtml");
  const stripCall = html.indexOf("const stripped = stripComments(t)");
  const scrubCall = html.indexOf("t = scrubEpubTextResource(xhtmlPath, t)");

  assert.match(html, /function stripComments\(xml\)/);
  assert.ok(stripCall >= 0, "generated Files page must invoke stripComments");
  assert.ok(scrubCall > stripCall, "generated Files page must strip comments before scrubbing XHTML");

  for (const identifier of ["HomePageHtml", "SettingsPageHtml", "FontsPageHtml"]) {
    assert.doesNotMatch(readGeneratedPage(identifier), /function stripComments\(xml\)/);
  }
});
