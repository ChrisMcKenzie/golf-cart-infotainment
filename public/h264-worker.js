// H264 Decoder Web Worker
importScripts('https://cdn.jsdelivr.net/npm/tinyh264@0.0.7/lib/TinyH264.js');

let decoder = null;
let decoderReady = false;

// Initialize the decoder
self.Module = {
  onRuntimeInitialized: function() {
    try {
      decoder = new self.H264bsdDecoder(self.Module);
      decoderReady = true;
      self.postMessage({ type: 'ready' });
      console.log('H264 decoder initialized in worker');
    } catch (e) {
      self.postMessage({ type: 'error', error: 'Failed to initialize decoder: ' + e.message });
    }
  }
};

// Handle messages from main thread
self.onmessage = function(e) {
  const { type, data } = e.data;

  switch (type) {
    case 'decode':
      if (!decoderReady || !decoder) {
        self.postMessage({ type: 'error', error: 'Decoder not ready' });
        return;
      }

      try {
        // Decode the H264 data
        const result = decoder.decode(data);

        if (result) {
          // Get the decoded picture
          const pic = decoder.nextPicture();

          if (pic) {
            // Send the YUV data back to main thread
            self.postMessage({
              type: 'decoded',
              width: pic.width,
              height: pic.height,
              data: pic.data.buffer
            }, [pic.data.buffer]); // Transfer ownership
          }
        }
      } catch (error) {
        self.postMessage({ type: 'error', error: 'Decode error: ' + error.message });
      }
      break;

    case 'release':
      if (decoder) {
        decoder.release();
        decoder = null;
        decoderReady = false;
      }
      break;
  }
};
