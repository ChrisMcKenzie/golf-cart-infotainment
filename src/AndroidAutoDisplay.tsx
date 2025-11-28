import { useEffect, useRef, useState } from "react";
import { invoke } from "@tauri-apps/api/core";
import { listen, UnlistenFn } from "@tauri-apps/api/event";

interface VideoFramePayload {
  data: string; // base64 encoded video data
  width: number;
  height: number;
  format: string; // "h264" or "rgb24"
}

interface AndroidAutoDisplayProps {
  isConnected: boolean;
}

export default function AndroidAutoDisplay({ isConnected }: AndroidAutoDisplayProps) {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const [isStreaming, setIsStreaming] = useState(false);
  const [frameCount, setFrameCount] = useState(0);
  const [fps, setFps] = useState(0);
  const [decoderReady, setDecoderReady] = useState(false);
  const unlistenRef = useRef<UnlistenFn | null>(null);
  const lastFrameTimeRef = useRef<number>(Date.now());
  const fpsIntervalRef = useRef<number | null>(null);
  const workerRef = useRef<Worker | null>(null);

  // Initialize H264 decoder worker
  useEffect(() => {
    try {
      const worker = new Worker('/h264-module-worker.js', { type: 'module' });

      worker.onmessage = (e) => {
        const { type, width, height, data, error } = e.data;

        switch (type) {
          case 'ready':
            console.log('H264 decoder worker ready');
            setDecoderReady(true);
            break;

          case 'decoded':
            // Render the decoded YUV frame
            renderYUVFrame(new Uint8Array(data), width, height);
            break;

          case 'error':
            console.error('Worker error:', error);
            break;
        }
      };

      worker.onerror = (error) => {
        console.error('Worker error:', error);
        setDecoderReady(false);
      };

      workerRef.current = worker;

      // Send init message to worker
      worker.postMessage({ type: 'init' });

      return () => {
        if (workerRef.current) {
          workerRef.current.postMessage({ type: 'release' });
          workerRef.current.terminate();
          workerRef.current = null;
        }
      };
    } catch (error) {
      console.error('Failed to create worker:', error);
    }
  }, []);

  // Start/stop video streaming based on connection status
  useEffect(() => {
    if (isConnected && !isStreaming) {
      startVideoStream();
    } else if (!isConnected && isStreaming) {
      stopVideoStream();
    }

    return () => {
      if (isStreaming) {
        stopVideoStream();
      }
    };
  }, [isConnected]);

  const startVideoStream = async () => {
    try {
      console.log("Starting video stream...");

      // Start the video streaming task in Rust
      await invoke("start_video_stream");

      // Listen for video frame events
      const unlisten = await listen<VideoFramePayload>("video-frame", (event) => {
        renderFrame(event.payload);
        setFrameCount((prev) => prev + 1);
      });

      unlistenRef.current = unlisten;
      setIsStreaming(true);

      // Set up FPS counter
      fpsIntervalRef.current = window.setInterval(() => {
        const now = Date.now();
        const elapsed = (now - lastFrameTimeRef.current) / 1000;
        if (elapsed > 0) {
          setFps(Math.round(frameCount / elapsed));
          setFrameCount(0);
          lastFrameTimeRef.current = now;
        }
      }, 1000);

      console.log("Video stream started successfully");
    } catch (error) {
      console.error("Failed to start video stream:", error);
    }
  };

  const stopVideoStream = async () => {
    try {
      console.log("Stopping video stream...");

      // Stop the video streaming task in Rust
      await invoke("stop_video_stream");

      // Unlisten from video frame events
      if (unlistenRef.current) {
        unlistenRef.current();
        unlistenRef.current = null;
      }

      // Clear FPS interval
      if (fpsIntervalRef.current) {
        clearInterval(fpsIntervalRef.current);
        fpsIntervalRef.current = null;
      }

      setIsStreaming(false);
      setFrameCount(0);
      setFps(0);

      // Clear canvas
      const canvas = canvasRef.current;
      if (canvas) {
        const ctx = canvas.getContext("2d");
        if (ctx) {
          ctx.clearRect(0, 0, canvas.width, canvas.height);
        }
      }

      console.log("Video stream stopped");
    } catch (error) {
      console.error("Failed to stop video stream:", error);
    }
  };

  const renderYUVFrame = (yuvData: Uint8Array, width: number, height: number) => {
    const canvas = canvasRef.current;
    if (!canvas) return;

    const ctx = canvas.getContext("2d");
    if (!ctx) return;

    // Resize canvas if needed
    if (canvas.width !== width || canvas.height !== height) {
      canvas.width = width;
      canvas.height = height;
      console.log(`Canvas resized to ${width}x${height}`);
    }

    // Create ImageData for rendering
    const imageData = ctx.createImageData(width, height);
    const pixels = imageData.data;

    // Convert YUV420 to RGB
    const ySize = width * height;
    const uvSize = ySize / 4;

    for (let i = 0; i < ySize; i++) {
      const y = yuvData[i];
      const uvIndex = Math.floor(i / 4);
      const u = yuvData[ySize + uvIndex] - 128;
      const v = yuvData[ySize + uvSize + uvIndex] - 128;

      // YUV to RGB conversion
      const r = y + 1.402 * v;
      const g = y - 0.344136 * u - 0.714136 * v;
      const b = y + 1.772 * u;

      const idx = i * 4;
      pixels[idx] = Math.max(0, Math.min(255, r));
      pixels[idx + 1] = Math.max(0, Math.min(255, g));
      pixels[idx + 2] = Math.max(0, Math.min(255, b));
      pixels[idx + 3] = 255;
    }

    ctx.putImageData(imageData, 0, 0);
  };

  const renderFrame = (frame: VideoFramePayload) => {
    if (frame.format === "rgb24") {
      // Render RGB24 frames
      const canvas = canvasRef.current;
      if (!canvas) return;

      const ctx = canvas.getContext("2d");
      if (!ctx) return;

      try {
        if (canvas.width !== frame.width || canvas.height !== frame.height) {
          canvas.width = frame.width;
          canvas.height = frame.height;
        }

        const binaryString = atob(frame.data);
        const rgbData = new Uint8Array(binaryString.length);
        for (let i = 0; i < binaryString.length; i++) {
          rgbData[i] = binaryString.charCodeAt(i);
        }

        const imageData = ctx.createImageData(frame.width, frame.height);
        const pixels = imageData.data;

        for (let i = 0, j = 0; i < rgbData.length; i += 3, j += 4) {
          pixels[j] = rgbData[i];
          pixels[j + 1] = rgbData[i + 1];
          pixels[j + 2] = rgbData[i + 2];
          pixels[j + 3] = 255;
        }

        ctx.putImageData(imageData, 0, 0);
      } catch (error) {
        console.error("Failed to render RGB frame:", error);
      }
    } else if (frame.format === "h264") {
      // Send H264 frame to worker for decoding
      const worker = workerRef.current;
      if (!worker || !decoderReady) {
        console.warn("H264 decoder worker not ready");
        return;
      }

      try {
        // Decode base64 to binary H264 data
        const binaryString = atob(frame.data);
        const h264Data = new Uint8Array(binaryString.length);
        for (let i = 0; i < binaryString.length; i++) {
          h264Data[i] = binaryString.charCodeAt(i);
        }

        // Send to worker for decoding
        worker.postMessage({
          type: 'decode',
          data: h264Data.buffer
        }, [h264Data.buffer]); // Transfer ownership
      } catch (error) {
        console.error("Failed to send H264 frame to worker:", error);
      }
    }
  };

  return (
    <div style={{
      position: "relative",
      width: "100%",
      height: "100%",
      backgroundColor: "#000",
      borderRadius: "8px",
      overflow: "hidden",
      display: "flex",
      alignItems: "center",
      justifyContent: "center",
    }}>
      <canvas
        ref={canvasRef}
        style={{
          maxWidth: "100%",
          maxHeight: "100%",
          objectFit: "contain",
        }}
      />

      {!isConnected && (
        <div style={{
          position: "absolute",
          top: "50%",
          left: "50%",
          transform: "translate(-50%, -50%)",
          color: "#fff",
          textAlign: "center",
          fontSize: "1.2em",
          opacity: 0.7,
        }}>
          <p>Waiting for Android Auto connection...</p>
          <p style={{ fontSize: "0.8em", marginTop: "10px" }}>
            Connect your phone via USB
          </p>
          {!decoderReady && (
            <p style={{ fontSize: "0.7em", marginTop: "10px", color: "#ff0" }}>
              ⚠️ H264 decoder initializing...
            </p>
          )}
        </div>
      )}

      {isConnected && !decoderReady && (
        <div style={{
          position: "absolute",
          top: "10px",
          left: "10px",
          backgroundColor: "rgba(255, 165, 0, 0.7)",
          color: "#000",
          padding: "5px 10px",
          borderRadius: "4px",
          fontSize: "0.8em",
          fontFamily: "monospace",
        }}>
          Decoder initializing...
        </div>
      )}

      {isStreaming && (
        <div style={{
          position: "absolute",
          top: "10px",
          right: "10px",
          backgroundColor: "rgba(0, 0, 0, 0.7)",
          color: "#0f0",
          padding: "5px 10px",
          borderRadius: "4px",
          fontSize: "0.8em",
          fontFamily: "monospace",
        }}>
          {fps} FPS
        </div>
      )}
    </div>
  );
}
