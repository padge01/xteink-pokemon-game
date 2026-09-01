const RELEASES_API_URL =
  'https://api.github.com/repos/padge01/xteink-pokemon-game/releases';

export function selectReleaseAssets(release) {
  const assets = release?.assets ?? [];
  const fullInstall = assets.find((asset) => /^xteink-pokemon-x3-full-v.+\.zip$/.test(asset.name));
  const firmware = assets.find((asset) => /^xteink-pokemon-x3-firmware-v.+\.bin$/.test(asset.name))
    ?? assets.find((asset) => /^xteink-pokemon-x3-v.+\.bin$/.test(asset.name));
  const checksum = assets.find((asset) => asset.name === 'SHA256SUMS.txt')
    ?? assets.find((asset) => asset.name === 'SHA256SUMS');

  if (!firmware || !checksum) {
    throw new Error('Release is missing the X3 firmware or checksum.');
  }

  return {
    fullInstallUrl: fullInstall?.browser_download_url ?? '',
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

  if (response.ok) {
    return selectReleaseAssets(await response.json());
  }

  if (tag || response.status !== 404) {
    throw new Error(`Release lookup failed (${response.status}).`);
  }

  const releasesResponse = await fetchImpl(`${RELEASES_API_URL}?per_page=20`, {
    headers: { Accept: 'application/vnd.github+json' },
  });
  if (!releasesResponse.ok) {
    throw new Error(`Release lookup failed (${releasesResponse.status}).`);
  }

  const releases = await releasesResponse.json();
  const published = releases.find((release) => !release.draft);
  if (!published) {
    throw new Error('No published release is available.');
  }
  return selectReleaseAssets(published);
}

export async function verifyReleaseChecksum(
  firmware,
  checksumText,
  filename,
  subtle = globalThis.crypto?.subtle,
) {
  if (!(firmware instanceof Uint8Array) || !subtle) {
    throw new Error('Firmware checksum verification is unavailable.');
  }

  const published = String(checksumText)
    .split(/\r?\n/)
    .map((line) => line.match(/^([0-9a-f]{64})\s{2,}(.+)$/i))
    .find((match) => match?.[2] === filename)?.[1]?.toLowerCase();
  if (!published) {
    throw new Error(`Release checksum is missing for ${filename}.`);
  }

  const digest = new Uint8Array(await subtle.digest('SHA-256', firmware));
  const actual = Array.from(digest, (byte) => byte.toString(16).padStart(2, '0')).join('');

  if (actual !== published) {
    throw new Error('Firmware checksum does not match the published release.');
  }
}
