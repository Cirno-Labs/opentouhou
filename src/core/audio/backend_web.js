const WebAudio = (() => {
  let ready = null;

  function processorModule() {
    class Output extends AudioWorkletProcessor {
      constructor(options) {
        super();
        const { memory, state, pcm, capacity } = options.processorOptions;
        this.state = new Uint32Array(memory, state, 2);
        this.pcm = new Float32Array(memory, pcm, capacity * 2);
        this.capacity = capacity;
      }

      process(inputs, outputs) {
        const left = outputs[0][0];
        const right = outputs[0][1];
        const read = Atomics.load(this.state, 0);
        const write = Atomics.load(this.state, 1);
        const count = Math.min((write - read) >>> 0, left.length);
        for (let i = 0; i < count; i++) {
          const at = ((read + i) % this.capacity) * 2;
          left[i] = this.pcm[at];
          right[i] = this.pcm[at + 1];
        }
        left.fill(0, count);
        right.fill(0, count);
        Atomics.store(this.state, 0, (read + count) >>> 0);
        return true;
      }
    }
    registerProcessor('opentouhou-output', Output);
  }

  async function create(frames) {
    const context = new AudioContext({ sampleRate: 48000, latencyHint: 'interactive' });
    let url = null;
    try {
      if (!context.audioWorklet) {
        throw new Error('This browser does not support AudioWorklet audio output.');
      }
      const resumed = context.resume();
      const source = '(' + processorModule.toString() + ')();';
      url = URL.createObjectURL(new Blob([source], { type: 'text/javascript' }));
      await context.audioWorklet.addModule(url);
      await resumed;
      let node = null;
      return {
        targetFrames: Math.max(128, Math.min(16384, Number(frames) || 2048)) | 0,
        start(memory, state, pcm, capacity) {
          try {
            node = new AudioWorkletNode(context, 'opentouhou-output', {
              numberOfInputs: 0,
              numberOfOutputs: 1,
              outputChannelCount: [2],
              processorOptions: { memory, state, pcm, capacity },
            });
            node.connect(context.destination);
            return true;
          } catch (error) {
            console.error('Cannot start audio output:', error);
            return false;
          }
        },
        release(module, state, pcm) {
          this.stop().then(() => module['_AudioWeb_Release'](state, pcm))
            .catch(error => console.error('Cannot close audio output:', error));
        },
        async stop() {
          if (node) {
            node.disconnect();
            node = null;
          }
          await context.close();
          ready = null;
        },
      };
    } catch (error) {
      await context.close();
      ready = null;
      throw error;
    } finally {
      if (url !== null) {
        URL.revokeObjectURL(url);
      }
    }
  }

  return {
    prepare(frames = 2048) {
      ready ??= create(frames);
      return ready;
    },
  };
})();
