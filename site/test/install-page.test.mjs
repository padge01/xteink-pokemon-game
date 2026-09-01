import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import test from 'node:test';

const installerSource = readFileSync(
  new URL('../src/pages/install.astro', import.meta.url),
  'utf8',
);

test('X3 installer offers Wi-Fi and SD-card flows without USB flashing', () => {
  assert.match(installerSource, /Wi-Fi update/i);
  assert.match(installerSource, /SD card reader/i);
  assert.doesNotMatch(installerSource, /Flash over USB/i);
  assert.doesNotMatch(installerSource, /navigator\.serial/i);
  assert.doesNotMatch(installerSource, /x3-flasher/i);
});
