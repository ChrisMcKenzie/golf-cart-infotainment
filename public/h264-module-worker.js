// H264 Decoder Module Worker (ES6 modules supported)
import TinyH264 from './TinyH264.js';
import TinyH264Decoder from './TinyH264Decoder.js';

let decoder = null;
let isReady = false;

// Initialize the decoder
async function initDecoder() {
  try {
    console.log('Initializing H264 decoder...');

    // Initialize TinyH264 module
    const tinyH264Module = await TinyH264();
    console.log('TinyH264 module loaded');

    // Create decoder instance
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

console.log('H264 Module Worker loaded');
