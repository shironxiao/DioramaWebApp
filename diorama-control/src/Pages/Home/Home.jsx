import React from 'react';
import {
  Lightbulb,
  Waves,
  Music,
  Sun,
  Play,
  Pause,
  ChevronRight,
  KeyRound
} from 'lucide-react';
import DioramaCanvas from '../../components/DioramaCanvas';
import './Home.css';

export default function Home({ appState, setAppState, setActiveTab, showToast }) {
  const {
    lightsOn, brightness, lightingMode,
    fountainOn, fountainStrength, fountainColor,
    audioPlaying, audioTrack, volume,
    simulatedLight, autoDimming, circleColor,
    rfidGateEnabled, gateOpen
  } = appState;

  return (
    <div className="page-container">
      {/* Header Section */}
      <div className="page-header-text">
        <div className="section-tag">YOUR LITTLE WORLD</div>
        <h1 className="page-main-title">My Diorama</h1>
        <p className="page-subtitle">
          Watch your miniature world come alive as you tweak it.
        </p>
      </div>

      {/* Interactive 3D Diorama Canvas */}
      <DioramaCanvas
        lightsOn={lightsOn}
        brightness={brightness}
        fountainOn={fountainOn}
        fountainStrength={fountainStrength}
        fountainColor={fountainColor}
        circleColor={circleColor}
        autoDimming={autoDimming}
        audioPlaying={audioPlaying}
        plazaRotationMode={appState.plazaRotationMode}
        onGateClick={() => showToast('✨ Interactive Diorama: Front Gate Tapped!')}
      />

      {/* 4 Control Cards Grid */}
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

          <div className="card-slider-group">
            <div className="slider-label-row">
              <span>Strength</span>
              <span className="value-label">{fountainStrength}%</span>
            </div>
            <input
              type="range"
              min="0"
              max="100"
              value={fountainStrength}
              disabled={!fountainOn}
              className="custom-range-slider"
              onChange={(e) => setAppState((prev) => ({ ...prev, fountainStrength: Number(e.target.value) }))}
            />
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

        {/* 4. AMBIENT LIGHT CARD */}
        <div className="control-card">
          <div className="card-header">
            <div className="card-title-group" onClick={() => setActiveTab('lights')}>
              <div className="icon-badge green">
                <Sun size={22} />
              </div>
              <div className="title-stack">
                <h3 className="card-title">Ambient Light</h3>
              </div>
            </div>
          </div>

          <div className="ambient-metric-row">
            <span className="metric-large">{simulatedLight}%</span>
            <span className="metric-subtext">simulated reading</span>
          </div>

          <div className="card-footer-row">
            <span className="footer-label">Auto dimming</span>
            <label className="toggle-switch">
              <input
                type="checkbox"
                checked={autoDimming}
                onChange={(e) => setAppState((prev) => ({ ...prev, autoDimming: e.target.checked }))}
              />
              <span className="slider round"></span>
            </label>
          </div>
        </div>

        {/* 5. GATE ACCESS CARD */}
        <div className="control-card">
          <div className="card-header">
            <div className="card-title-group">
              <div className="icon-badge green">
                <KeyRound size={22} />
              </div>
              <div className="title-stack">
                <h3 className="card-title">Gate Access</h3>
                <span className="card-status-subtext">
                  {rfidGateEnabled ? 'RFID Active' : 'RFID Disabled'}
                </span>
              </div>
            </div>
          </div>
          
          <div className="card-footer-row border-none">
            <span className="footer-label">Enable RFID Reader</span>
            <label className="toggle-switch">
              <input
                type="checkbox"
                checked={rfidGateEnabled}
                onChange={(e) => {
                  const next = e.target.checked;
                  setAppState((prev) => ({ ...prev, rfidGateEnabled: next }));
                  showToast(next ? 'RFID Access Enabled' : 'RFID Access Disabled');
                }}
              />
              <span className="slider round"></span>
            </label>
          </div>

          <div className="card-footer-row border-t pt-2 mt-2">
            <span className="footer-label">Gate Status</span>
            <button 
              className={`btn-outline ${gateOpen ? 'active' : ''}`}
              onClick={() => {
                const next = !gateOpen;
                setAppState((prev) => ({ ...prev, gateOpen: next }));
                showToast(next ? 'Gate Opened Manually' : 'Gate Closed Manually');
              }}
            >
              {gateOpen ? 'Open' : 'Closed'}
            </button>
          </div>
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
        <span>Tip: open Lights & Colors to change colors and modes</span>
        <ChevronRight size={20} className="tip-arrow" />
      </div>
    </div>
  );
}
