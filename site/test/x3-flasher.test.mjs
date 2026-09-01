import assert from 'node:assert/strict';
import test from 'node:test';

import { beginX3Flash } from '../src/lib/installer/x3-flasher.mjs';

test('requests the serial port before awaiting the firmware download', async () => {
  const events = [];
  const firmware = new Uint8Array([0xe9, 0, 0, 0]);
  let receivedOptions;

  class FakeFlasher {
    static requestPort() {
      events.push('port');
      return Promise.resolve({ id: 'x3' });
    }

    constructor(port, options) {
      assert.deepEqual(port, { id: 'x3' });
      receivedOptions = options;
    }

    async connect() {
      events.push('connect');
    }

    async readPartitionTable() {
      events.push('layout');
      return { matchedLayout: 'X3' };
    }

    async disconnect(skipReset) {
      events.push('disconnect');
      assert.equal(skipReset, true);
    }

    async flashFirmware(receivedFirmware, options) {
      events.push('flash');
      assert.equal(receivedFirmware, firmware);
      assert.equal(options.skipReset, true);
      return { partition: 'app1', success: true };
    }
  }

  const result = await beginX3Flash({
    CrossPointFlasher: FakeFlasher,
    downloadFirmware: async () => {
      events.push('download');
      return firmware;
    },
    validateFirmwareImage: async (receivedFirmware) => {
      events.push('validate');
      assert.equal(receivedFirmware, firmware);
    },
  });

  assert.deepEqual(events, ['port', 'download', 'validate', 'connect', 'layout', 'disconnect', 'flash']);
  assert.deepEqual(receivedOptions, {
    expectedChip: 'ESP32-C3',
    deviceName: 'Xteink X3',
  });
  assert.deepEqual(result, { partition: 'app1', success: true });
});

test('rejects a firmware download that is not a Uint8Array', async () => {
  class FakeFlasher {
    static requestPort() {
      return Promise.resolve({});
    }
  }

  await assert.rejects(
    () => beginX3Flash({
      CrossPointFlasher: FakeFlasher,
      downloadFirmware: async () => null,
      validateFirmwareImage: async () => {},
    }),
    /firmware download did not return binary data/i,
  );
});

test('rejects a non-X3 ESP32-C3 layout before firmware writing begins', async () => {
  const events = [];

  class FakeFlasher {
    static requestPort() {
      return Promise.resolve({});
    }

    async connect() {
      events.push('connect');
    }

    async readPartitionTable() {
      events.push('layout');
      return { matchedLayout: 'X4' };
    }

    async disconnect(skipReset) {
      events.push('disconnect');
      assert.equal(skipReset, true);
    }

    async flashFirmware() {
      events.push('write');
    }
  }

  await assert.rejects(
    () => beginX3Flash({
      CrossPointFlasher: FakeFlasher,
      downloadFirmware: async () => new Uint8Array([0xe9]),
      validateFirmwareImage: async () => {},
    }),
    /not an Xteink X3/i,
  );

  assert.deepEqual(events, ['connect', 'layout', 'disconnect']);
});
