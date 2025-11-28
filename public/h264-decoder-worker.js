// H264 Decoder Web Worker using tinyh264
// Load TinyH264 library and decoder
importScripts('/TinyH264.js');
importScripts('/TinyH264Decoder.js');

let decoder = null;
let isReady = false;

// Initialize the decoder
async function initDecoder() {
  try {
    console.log('Initializing H264 decoder...');

    // Wait for TinyH264 module to initialize
    if (typeof TinyH264 === 'function') {
      const tinyH264Module = await TinyH264();
      console.log('TinyH264 module loaded');

      // Create decoder instance using TinyH264Decoder
      decoder = new TinyH264Decoder(tinyH264Module, (output, width, height) => {
        // Send decoded frame back to main thread
        self.postMessage({
          type: 'decoded',
          width: width,
          height: height,
          data: output.buffer
        }, [output.buffer]);
      });

      isReady = true;
      self.postMessage({ type: 'ready' });
      console.log('H264 decoder ready');
    } else {
      throw new Error('TinyH264 not loaded');
    }
  } catch (error) {
    console.error('Failed to initialize decoder:', error);
    self.postMessage({ type: 'error', error: error.message || 'Failed to initialize decoder' });
  }
}

// Handle messages
self.onmessage = function(e) {
  const { type, data } = e.data;

  if (type === 'init') {
    initDecoder();
    return;
  }

  if (!isReady || !decoder) {
    self.postMessage({ type: 'error', error: 'Decoder not ready' });
    return;
  }

  switch (type) {
    case 'decode':
      try {
        const h264Data = new Uint8Array(data);
        decoder.decode(h264Data);
      } catch (error) {
        self.postMessage({ type: 'error', error: 'Decode error: ' + error.message });
      }
      break;

    case 'release':
      if (decoder) {
        decoder.release();
        decoder = null;
        isReady = false;
      }
      break;
  }
};

console.log('H264 Worker loaded');
