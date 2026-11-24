const assert = require('node:assert/strict');
const { readFileSync } = require('node:fs');
const path = require('node:path');
const test = require('node:test');
const vm = require('node:vm');

const source = readFileSync(path.join(__dirname, '../../src/core/audio/backend_web.js'), 'utf8');

async function prepare() {
  let processor;
  let instance;
  let closed = false;
  const blobs = new Map();
  const context = {
    Blob, Uint32Array, Float32Array, Atomics, console,
    URL: {
      createObjectURL(blob) { blobs.set('module', blob); return 'module'; },
      revokeObjectURL(url) { blobs.delete(url); },
    },
    AudioContext: class {
      destination = {};
      audioWorklet = {
        async addModule(url) {
          vm.runInNewContext(await blobs.get(url).text(), {
            AudioWorkletProcessor: class {}, Uint32Array, Float32Array, Atomics,
            registerProcessor(name, implementation) { processor = implementation; },
          });
        },
      };
      async resume() {}
      async close() { closed = true; }
    },
    AudioWorkletNode: class {
      constructor(context, name, options) { instance = new processor(options); }
      connect() {}
      disconnect() {}
    },
  };
  const audio = vm.runInNewContext(source + '\nWebAudio;', context);
  const backend = await audio.prepare();
  assert.equal(await audio.prepare(), backend);
  assert.equal(blobs.size, 0);
  const capacity = 256;
  const memory = new SharedArrayBuffer(8 + capacity * 8);
  const state = new Uint32Array(memory, 0, 2);
  const pcm = new Float32Array(memory, 8, capacity * 2);
  assert.equal(backend.start(memory, 0, 8, capacity), true);
  return { backend, state, pcm, capacity, processor: instance, closed: () => closed };
}

function process(output) {
  const channels = [new Float32Array(128), new Float32Array(128)];
  channels[0].fill(99);
  channels[1].fill(99);
  assert.equal(output.processor.process([], [channels]), true);
  return channels;
}

test('worklet consumes stereo frames across the ring boundary', async () => {
  const output = await prepare();
  output.state[0] = 254;
  output.state[1] = 258;
  for (let i = 0; i < 4; i++) {
    const at = ((254 + i) % output.capacity) * 2;
    output.pcm[at] = i + 1;
    output.pcm[at + 1] = -i - 1;
  }
  const channels = process(output);
  assert.deepEqual([...channels[0].slice(0, 4)], [1, 2, 3, 4]);
  assert.deepEqual([...channels[1].slice(0, 4)], [-1, -2, -3, -4]);
  assert.ok(channels[0].slice(4).every(sample => sample === 0));
  assert.equal(output.state[0], 258);
  await output.backend.stop();
});

test('worklet preserves frames when counters wrap at 32 bits', async () => {
  const output = await prepare();
  output.state[0] = 0xfffffffe;
  output.state[1] = 2;
  for (let i = 0; i < 4; i++) {
    const at = ((0xfffffffe + i) % output.capacity) * 2;
    output.pcm[at] = i + 1;
  }
  assert.deepEqual([...process(output)[0].slice(0, 4)], [1, 2, 3, 4]);
  assert.equal(output.state[0], 2);
  await output.backend.stop();
});

test('an underrun emits silence without advancing past queued frames', async () => {
  const output = await prepare();
  assert.ok(process(output)[0].every(sample => sample === 0));
  assert.equal(output.state[0], 0);
  output.pcm[0] = 0.5;
  Atomics.store(output.state, 1, 1);
  assert.equal(process(output)[0][0], 0.5);
  assert.equal(output.state[0], 1);
  await output.backend.stop();
});

test('shared memory is released only after the audio context closes', async () => {
  const output = await prepare();
  let released = false;
  output.backend.release({ _AudioWeb_Release(state, pcm) {
    assert.equal(output.closed(), true);
    assert.equal(state, 0);
    assert.equal(pcm, 8);
    released = true;
  } }, 0, 8);
  await new Promise(resolve => setImmediate(resolve));
  assert.equal(released, true);
});
