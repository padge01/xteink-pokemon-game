export async function beginX3Flash({
  CrossPointFlasher,
  downloadFirmware,
  validateFirmwareImage,
  onStepChange,
  onProgress,
}) {
  if (
    !CrossPointFlasher?.requestPort ||
    typeof downloadFirmware !== 'function' ||
    typeof validateFirmwareImage !== 'function'
  ) {
    throw new Error('The X3 flasher is not ready.');
  }

  // This must be the first asynchronous browser action. WebSerial requires
  // requestPort() to be called directly from the user's click.
  const port = await CrossPointFlasher.requestPort();
  const firmware = await downloadFirmware();

  if (!(firmware instanceof Uint8Array)) {
    throw new Error('Firmware download did not return binary data.');
  }

  await validateFirmwareImage(firmware);

  const flasher = new CrossPointFlasher(port, {
    expectedChip: 'ESP32-C3',
    deviceName: 'Xteink X3',
  });

  let matchedLayout;
  await flasher.connect();
  try {
    ({ matchedLayout } = await flasher.readPartitionTable());
  } finally {
    await flasher.disconnect(true);
  }

  if (matchedLayout !== 'X3') {
    throw new Error('The connected device is not an Xteink X3. Flashing was stopped before writing.');
  }

  return flasher.flashFirmware(firmware, {
    skipReset: true,
    onStepChange,
    onProgress,
  });
}
