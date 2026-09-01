import assert from 'node:assert/strict';
import test from 'node:test';

import {
  fetchLatestRelease,
  fetchRelease,
  selectReleaseAssets,
  verifyReleaseChecksum,
} from '../src/lib/installer/release-client.mjs';


test('selectReleaseAssets returns the full install, firmware, and checksum', () => {
  const result = selectReleaseAssets({
    tag_name: 'v1.0.0',
    assets: [
      {
        name: 'xteink-pokemon-x3-full-v1.0.0.zip',
        browser_download_url: 'https://example.test/full',
      },
      {
        name: 'xteink-pokemon-x3-firmware-v1.0.0.bin',
        browser_download_url: 'https://example.test/firmware',
      },
      { name: 'SHA256SUMS.txt', browser_download_url: 'https://example.test/checksums' },
    ],
  });

  assert.deepEqual(result, {
    fullInstallUrl: 'https://example.test/full',
    firmwareUrl: 'https://example.test/firmware',
    checksumUrl: 'https://example.test/checksums',
    version: '1.0.0',
  });
});

test('selectReleaseAssets keeps a legacy firmware-only release usable during rollout', () => {
  const result = selectReleaseAssets({
    tag_name: 'rc-0.1.0-209ad6f',
    assets: [
      {
        name: 'xteink-pokemon-x3-v0.1.0-RC.bin',
        browser_download_url: 'https://example.test/legacy-firmware',
      },
      { name: 'SHA256SUMS', browser_download_url: 'https://example.test/legacy-checksums' },
    ],
  });

  assert.deepEqual(result, {
    fullInstallUrl: '',
    firmwareUrl: 'https://example.test/legacy-firmware',
    checksumUrl: 'https://example.test/legacy-checksums',
    version: 'rc-0.1.0-209ad6f',
  });
});

test('selectReleaseAssets rejects a release without firmware and checksum files', () => {
  assert.throws(
    () => selectReleaseAssets({ tag_name: 'v1.0.0', assets: [] }),
    /release is missing the X3 firmware or checksum/i,
  );
});

test('fetchLatestRelease reports a failed GitHub response', async () => {
  const fetchImpl = async () => ({ ok: false, status: 503 });

  await assert.rejects(() => fetchLatestRelease(fetchImpl), /release lookup failed \(503\)/i);
});

test('fetchRelease loads an explicitly requested release tag', async () => {
  let requestedUrl;
  const fetchImpl = async (url) => {
    requestedUrl = url;
    return {
      ok: true,
      json: async () => ({
        tag_name: 'v0.1.0-rc.1',
        assets: [
          {
            name: 'xteink-pokemon-x3-full-v0.1.0-rc.1.zip',
            browser_download_url: 'https://example.test/full',
          },
          {
            name: 'xteink-pokemon-x3-firmware-v0.1.0-rc.1.bin',
            browser_download_url: 'https://example.test/firmware',
          },
          { name: 'SHA256SUMS.txt', browser_download_url: 'https://example.test/checksums' },
        ],
      }),
    };
  };

  const result = await fetchRelease('v0.1.0-rc.1', fetchImpl);

  assert.match(requestedUrl, /releases\/tags\/v0\.1\.0-rc\.1$/);
  assert.equal(result.version, '0.1.0-rc.1');
});

test('verifyReleaseChecksum accepts the published SHA-256 and rejects a mismatch', async () => {
  const firmware = new TextEncoder().encode('x3 firmware');
  const digest = 'd544bfb52dd10be2aef2aeca42757cef80809bb98fed83a24db44552bc32918a';

  await verifyReleaseChecksum(
    firmware,
    `${digest}  xteink-pokemon-x3-firmware-v1.0.0.bin\n`,
  );

  await assert.rejects(
    () => verifyReleaseChecksum(firmware, `${'0'.repeat(64)}  firmware.bin\n`),
    /checksum does not match/i,
  );
});
