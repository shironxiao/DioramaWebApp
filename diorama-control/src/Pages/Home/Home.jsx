import React, { useState } from 'react';
import {
  Lightbulb,
  Waves,
  Music,
  Play,
  Pause,
  ChevronRight,
  Fingerprint,
  Lock,
  Unlock
} from 'lucide-react';
import DioramaCanvas from '../../components/DioramaCanvas';
import './Home.css';

export default function Home({ appState, setAppState, setActiveTab, showToast }) {
  const {
    lightsOn, brightness, lightingMode,
    fountainOn, fountainStrength, fountainColor,
    audioPlaying, audioTrack, volume,
    autoDimming, circleColor, gateOpen
  } = appState;

  const [isScanning, setIsScanning] = useState(false);

  const handleBiometricScan = () => {
    if (isScanning) return;
    setIsScanning(true);
    showToast('🔍 Biometric sensor scanning fingerprint...');
    setTimeout(() => {
      const newGateState = !gateOpen;
      setAppState((prev) => ({ ...prev, gateOpen: newGateState }));
      setIsScanning(false);
      showToast(newGateState ? '✅ Fingerprint verified! Gate opened.' : '🔒 Fingerprint verified! Gate closed.');
    }, 1800);
  };

  return (
    <div className="page-container">
      {/* Header Section */}
      <div className="page-header-text">
        <div className="section-tag">STA. ELENA, CAMARINES NORTE</div>
        <h1 className="page-main-title">Silvestre del Moro Park</h1>
        <p className="page-subtitle">
          Control and explore the miniature recreation of this iconic local landmark.
        </p>
      </div>

      {/* Interactive 3D Diorama Canvas */}
      <DioramaCanvas
        lightsOn={lightsOn}
        brightness={brightness}
        lightingMode={lightingMode}
        soundReactiveOn={appState.soundReactiveOn}
        fountainOn={fountainOn}
        fountainStrength={fountainStrength}
        fountainAuxStrength={appState.fountainAuxStrength}
        fountainForceSensorOn={appState.fountainForceSensorOn}
        fountainColor={fountainColor}
        circleColor={circleColor}
        autoDimming={autoDimming}
        audioPlaying={audioPlaying}
        onGateClick={() => showToast('✨ Interactive Diorama: Front Gate Tapped!')}
      />

      {/* 3 Control Cards Grid */}
      <div className="controls-grid">
        {/* 1. LIGHTS CARD */}
        <div className={`control-card ${lightsOn ? 'active-card' : ''}`}>
          <div className="card-header">
            <div className="card-title-group" onClick={() => setActiveTab('lights')}>
              <div className="icon-badge green">
                <Lightbulb size={22} />
              </div>
              <div className="title-stack">
                <h3 className="card-title">Lights</h3>
                <span className="card-status-subtext">
                  {lightsOn ? `On - ${lightingMode}` : 'Off'}
                </span>
              </div>
            </div>
            <label className="toggle-switch">
              <input
                type="checkbox"
                checked={lightsOn}
                onChange={(e) => setAppState((prev) => ({ ...prev, lightsOn: e.target.checked }))}
              />
              <span className="slider round"></span>
            </label>
          </div>

          <div className="card-slider-group">
            <div className="slider-label-row">
              <span>Brightness</span>
              <span className="value-label">{brightness}%</span>
            </div>
            <input
              type="range"
              min="0"
              max="100"
              value={brightness}
              disabled={!lightsOn}
              className="custom-range-slider"
              onChange={(e) => setAppState((prev) => ({ ...prev, brightness: Number(e.target.value) }))}
            />
          </div>
        </div>

        {/* 2. FOUNTAIN CARD */}
        <div className={`control-card ${fountainOn ? 'active-card' : ''}`}>
          <div className="card-header">
            <div className="card-title-group" onClick={() => setActiveTab('fountain')}>
              <div className="icon-badge green">
                <Waves size={22} />
              </div>
              <div className="title-stack">
                <h3 className="card-title">Fountain</h3>
                <span className="card-status-subtext">
                  {fountainOn ? 'Flowing' : 'Off'}
                </span>
              </div>
            </div>
            <label className="toggle-switch">
              <input
                type="checkbox"
                checked={fountainOn}
                onChange={(e) => setAppState((prev) => ({ ...prev, fountainOn: e.target.checked }))}
              />
              <span className="slider round"></span>
            </label>
          </div>
        </div>

        {/* 3. AUDIO CARD */}
        <div className="control-card">
          <div className="card-header">
            <div className="card-title-group" onClick={() => setActiveTab('audio')}>
              <div className="icon-badge green">
                <Music size={22} />
              </div>
              <div className="title-stack">
                <h3 className="card-title">Audio</h3>
                <span className="card-status-subtext">{audioTrack}</span>
              </div>
            </div>
            <button
              className="audio-play-circle-btn"
              onClick={() => {
                const next = !audioPlaying;
                setAppState((prev) => ({
                  ...prev,
                  audioPlaying: next,
                  audioTrack: next ? 'Nature Ambient Stream' : 'No audio selected'
                }));
                showToast(next ? 'Playing Nature Ambient Stream' : 'Audio Paused');
              }}
            >
              {audioPlaying ? <Pause size={16} /> : <Play size={16} className="play-icon-offset" />}
            </button>
          </div>

          <div className="card-slider-group">
            <div className="slider-label-row">
              <span>Volume</span>
              <span className="value-label">{volume}%</span>
            </div>
            <input
              type="range"
              min="0"
              max="100"
              value={volume}
              className="custom-range-slider"
              onChange={(e) => setAppState((prev) => ({ ...prev, volume: Number(e.target.value) }))}
            />
          </div>
        </div>

        {/* 4. BIOMETRIC GATE CARD */}
        <div className={`control-card biometric-gate-card ${gateOpen ? 'gate-open-card' : ''} ${isScanning ? 'gate-scanning-card' : ''}`}>
          <div className="card-header">
            <div className="card-title-group">
              <div className={`icon-badge ${gateOpen ? 'gate-icon-open' : 'gate-icon-closed'}`}>
                {gateOpen ? <Unlock size={20} /> : <Lock size={20} />}
              </div>
              <div className="title-stack">
                <h3 className="card-title">Gate Access</h3>
                <div className={`gate-inline-pill ${gateOpen ? 'open' : 'closed'}`}>
                  <span className="pill-dot"></span>
                  <span>{isScanning ? 'Scanning...' : gateOpen ? 'Gate: OPEN' : 'Gate: CLOSED'}</span>
                </div>
              </div>
            </div>
          </div>

          {/* Biometric Scanner Button */}
          <button
            id="biometric-scan-btn"
            className={`biometric-scan-btn ${isScanning ? 'scanning' : ''} ${gateOpen ? 'gate-is-open' : ''}`}
            onClick={handleBiometricScan}
            disabled={isScanning}
            aria-label="Biometric fingerprint scanner to open or close the gate"
          >
            <div className="biometric-inner">
              <Fingerprint
                size={30}
                className={`fingerprint-icon ${isScanning ? 'fp-scanning' : ''}`}
                strokeWidth={1.75}
              />
              <span className="biometric-label">
                {isScanning
                  ? 'Verifying...'
                  : gateOpen
                  ? 'Tap to Close Gate'
                  : 'Tap to Open Gate'}
              </span>
            </div>
            {isScanning && <div className="biometric-scan-ring"></div>}
          </button>
        </div>
      </div>

      {/* Tip Banner */}
      <div
        className="tip-banner-card"
        onClick={() => {
          setActiveTab('lights');
          showToast('Navigating to Lights & Colors');
        }}
      >
        <span>Tip: open Lights &amp; Colors to change colors and modes</span>
        <ChevronRight size={20} className="tip-arrow" />
      </div>
    </div>
  );
}
