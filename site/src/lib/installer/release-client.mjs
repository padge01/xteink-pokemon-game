const RELEASES_API_URL =
  'https://api.github.com/repos/padge01/xteink-pokemon-game/releases';

export function selectReleaseAssets(release) {
  const assets = release?.assets ?? [];
  const firmware = assets.find((asset) => /^xteink-pokemon-x3-v.+\.bin$/.test(asset.name));
  const checksum = assets.find((asset) => asset.name === 'SHA256SUMS');

  if (!firmware || !checksum) {
    throw new Error('Release is missing the X3 firmware or checksum.');
  }

  return {
    firmwareUrl: firmware.browser_download_url,
    checksumUrl: checksum.browser_download_url,
    version: String(release.tag_name ?? '').replace(/^v/, ''),
  };
}

export async function fetchLatestRelease(fetchImpl = fetch) {
  return fetchRelease('', fetchImpl);
}

export async function fetchRelease(tag = '', fetchImpl = fetch) {
  const releasePath = tag ? `/tags/${encodeURIComponent(tag)}` : '/latest';
  const response = await fetchImpl(`${RELEASES_API_URL}${releasePath}`, {
    headers: { Accept: 'application/vnd.github+json' },
  });

  if (!response.ok) {
    throw new Error(`Release lookup failed (${response.status}).`);
  }

  return selectReleaseAssets(await response.json());
}

export async function verifyReleaseChecksum(firmware, checksumText, subtle = globalThis.crypto?.subtle) {
  if (!(firmware instanceof Uint8Array) || !subtle) {
    throw new Error('Firmware checksum verification is unavailable.');
  }

  const published = String(checksumText).match(/^([0-9a-f]{64})\s{2,}\S+$/im)?.[1]?.toLowerCase();
  if (!published) {
    throw new Error('Release checksum file is invalid.');
  }

  const digest = new Uint8Array(await subtle.digest('SHA-256', firmware));
  const actual = Array.from(digest, (byte) => byte.toString(16).padStart(2, '0')).join('');

  if (actual !== published) {
    throw new Error('Firmware checksum does not match the published release.');
  }
}
