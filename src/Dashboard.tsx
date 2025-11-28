import { useEffect, useState } from "react";
import { invoke } from "@tauri-apps/api/core";
import { IoPlayForwardOutline, IoPlayBackOutline } from "react-icons/io5";
import { PiHeadlights } from "react-icons/pi";
import { FaArrowLeft } from "react-icons/fa";
import { FaArrowRight } from "react-icons/fa";
import { TiBatteryFull } from "react-icons/ti";
import { GiGearStick } from "react-icons/gi";
import "./Dashboard.css";

interface HardwareStatus {
  battery_level: number;
  battery_voltage: number;
  drive_mode: "Park" | "Reverse" | "Neutral" | "Forward" | "Unknown";
  headlights_on: boolean;
  speed: number;
  signals: {
    left_turn: boolean;
    right_turn: boolean;
    hazard: boolean;
    brake: boolean;
  };
}

export default function Dashboard() {
  const [status, setStatus] = useState<HardwareStatus>({
    battery_level: 75.0,
    battery_voltage: 48.0,
    drive_mode: "Neutral",
    headlights_on: false,
    speed: 0.0,
    signals: {
      left_turn: false,
      right_turn: false,
      hazard: false,
      brake: false,
    },
  });
  const [openAutoRunning, setOpenAutoRunning] = useState(false);
  const [openAutoConnected, setOpenAutoConnected] = useState(false);
  const [currentTrack] = useState({
    artist: "Raquel Ramos",
    title: "I Want Your Man To Be My...",
  });
  const [isPlaying, setIsPlaying] = useState(false);
  const [volume, setVolume] = useState(50);

  useEffect(() => {
    // Poll hardware status every 100ms
    const updateStatus = async () => {
      try {
        const currentStatus = await invoke<HardwareStatus>("get_hardware_status");
        setStatus(currentStatus);
      } catch (error) {
        console.error("Error getting hardware status:", error);
      }
    };

    // Initial status fetch
    updateStatus();
    checkOpenAutoStatus();
    checkOpenAutoConnection();

    // Set up polling interval
    const interval = setInterval(updateStatus, 100);
    
    // Poll Android Auto connection status
    const connectionInterval = setInterval(checkOpenAutoConnection, 500);

    return () => {
      clearInterval(interval);
      clearInterval(connectionInterval);
    };
  }, []);

  const checkOpenAutoStatus = async () => {
    try {
      const running = await invoke<boolean>("is_openauto_running");
      setOpenAutoRunning(running);
    } catch (error) {
      console.error("Error checking OpenAuto status:", error);
    }
  };

  const checkOpenAutoConnection = async () => {
    try {
      const connected = await invoke<boolean>("is_openauto_connected");
      setOpenAutoConnected(connected);
    } catch (error) {
      console.error("Error checking OpenAuto connection:", error);
    }
  };

  const handleStartOpenAuto = async () => {
    try {
      await invoke("start_openauto");
      // Check status after a brief delay to verify it actually started
      setTimeout(async () => {
        const running = await invoke<boolean>("is_openauto_running");
        setOpenAutoRunning(running);
        if (!running) {
          alert("OpenAuto failed to start. Please check:\n- OpenAuto is installed\n- Check terminal logs for details");
        }
      }, 500);
      setOpenAutoRunning(true);
    } catch (error) {
      console.error("Error starting OpenAuto:", error);
      setOpenAutoRunning(false);
      const errorMsg = error instanceof Error ? error.message : String(error);
      alert(`Failed to start Android Auto: ${errorMsg}\n\nPlease ensure OpenAuto is installed and accessible.`);
    }
  };

  const handleStopOpenAuto = async () => {
    try {
      await invoke("stop_openauto");
      setOpenAutoRunning(false);
    } catch (error) {
      console.error("Error stopping OpenAuto:", error);
    }
  };

  const handleVolumeChange = async (newVolume: number) => {
    setVolume(newVolume);
    await invoke("set_audio_volume", { volume: newVolume / 100 });
  };


  return (
    <div className="music-dashboard">
      {/* Background Pattern */}
      <div className="background-pattern"></div>

      {/* Left Navigation Bar */}
      <nav className="side-nav">
        <div className="nav-icon active" title="Home">
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2">
            <path d="M3 9l9-7 9 7v11a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z"></path>
            <polyline points="9 22 9 12 15 12 15 22"></polyline>
          </svg>
        </div>
      </nav>

      {/* Main Content Area */}
      <main className="main-content-area">
        {/* Top Bar with Search */}
        <div className="top-bar">
          <div className="nav-arrows">
            <button className="nav-arrow">
              <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
                <polyline points="15 18 9 12 15 6"></polyline>
              </svg>
            </button>
            <button className="nav-arrow">
              <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
                <polyline points="9 18 15 12 9 6"></polyline>
              </svg>
            </button>
          </div>
          <div className="search-container">
            <input type="text" placeholder="Search." className="search-input" />
            <svg className="search-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2">
              <circle cx="11" cy="11" r="8"></circle>
              <path d="m21 21-4.35-4.35"></path>
            </svg>
          </div>
        </div>

        {/* Hardware Status Cards */}
        <div className="status-cards">
          <div className="status-card battery">
            <div className="status-icon battery-icon">
              <TiBatteryFull />
            </div>
            <div className="status-info">
              <div className="status-value">{status.battery_level.toFixed(0)}%</div>
              <div className="status-label">{status.battery_voltage.toFixed(1)}V</div>
            </div>
          </div>
          <div className={`status-card drive drive-${status.drive_mode.toLowerCase()}`}>
            <div className="status-icon drive-icon">
              <GiGearStick />
            </div>
            <div className="status-info">
              <div className="status-value">{status.drive_mode}</div>
              <div className="status-label">Drive Mode</div>
            </div>
          </div>
          <div className={`status-card headlights ${status.headlights_on ? "on" : ""}`}>
            <div className="status-icon headlight-icon">
              <PiHeadlights />
            </div>
            <div className="status-info">
              <div className="status-value">{status.headlights_on ? "ON" : "OFF"}</div>
              <div className="status-label">Headlights</div>
            </div>
          </div>
        </div>

        {/* Speedometer */}
        <div className="speedometer-container">
          <div className="speedometer">
            <div className="speedometer-label">MPH</div>
            <div className="speedometer-value">{status.speed.toFixed(0)}</div>
          </div>
        </div>

        {/* Android Auto Integration Area */}
        <section className="openauto-section">
          <div className="openauto-content">
            {openAutoRunning ? (
              <>
                <p>Android Auto {openAutoConnected ? "Connected" : "Waiting for Device..."}</p>
                {openAutoConnected && <p style={{fontSize: "0.8em", opacity: 0.7}}>USB device detected and connected</p>}
                <button className="openauto-btn stop" onClick={handleStopOpenAuto}>
                  Stop Android Auto
                </button>
              </>
            ) : (
              <>
                <p>Android Auto</p>
                <button className="openauto-btn start" onClick={handleStartOpenAuto}>
                  Start Android Auto
                </button>
              </>
            )}
          </div>
        </section>
      </main>

      {/* Bottom Playback Control Bar */}
      <div className="playback-bar">
        {/* <div className="track-info">
          <div className="track-artist">{currentTrack.artist}</div>
          <div className="track-title">{currentTrack.title}</div>
        </div> */}
        {/* <div className="playback-controls">
          <button className="control-btn">
            <IoPlayBackOutline height={16} width={16} />
          </button>
          <button className="control-btn play-pause" onClick={() => setIsPlaying(!isPlaying)}>
            {isPlaying ? (
              <svg viewBox="0 0 24 24" fill="currentColor">
                <path d="M6 4h4v16H6V4zm8 0h4v16h-4V4z" />
              </svg>
            ) : (
              <svg viewBox="0 0 24 24" fill="currentColor">
                <path d="M8 5v14l11-7z" />
              </svg>
            )}
          </button>
          <button className="control-btn">
            <IoPlayForwardOutline height={16} width={16} />
          </button>
        </div> */}
        {/* <div className="volume-controls">
          <button className="control-btn">
            <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2">
              <polygon points="11 5 6 9 2 9 2 15 6 15 11 19 11 5"></polygon>
              <path d="M19.07 4.93a10 10 0 0 1 0 14.14M15.54 8.46a5 5 0 0 1 0 7.07"></path>
            </svg>
          </button>
          <input
            type="range"
            min="0"
            max="100"
            value={volume}
            onChange={(e) => handleVolumeChange(Number(e.target.value))}
            className="volume-slider"
          />
        </div> */}
        <div className="signal-indicators">
          {status.signals.left_turn && <div className="signal left">
            <FaArrowLeft />
          </div>}
          {status.signals.right_turn && <div className="signal right">
            <FaArrowRight />
          </div>}
          {status.signals.hazard && <div className="signal hazard">⚠</div>}
          {status.signals.brake && <div className="signal brake">🛑</div>}
        </div>
      </div>
    </div>
  );
}
