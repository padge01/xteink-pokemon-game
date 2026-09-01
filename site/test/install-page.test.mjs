import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import test from 'node:test';

const installPage = readFileSync(new URL('../install.html', import.meta.url), 'utf8');

test('full install tells users to extract the package before copying it', () => {
  assert.match(installPage, /Do not copy the ZIP file itself/i);
  assert.match(installPage, /update\.bin/i);
  assert.match(installPage, /\.crosspoint/i);
  assert.match(installPage, /SD-card root/i);
});

test('firmware-only updates do not claim that the X3 supports USB flashing', () => {
  assert.match(installPage, /Firmware-only Wi-Fi update/i);
  assert.doesNotMatch(installPage, /Flash over USB/i);
  assert.doesNotMatch(installPage, /navigator\.serial/i);
  assert.doesNotMatch(installPage, /x3-flasher/i);
});
