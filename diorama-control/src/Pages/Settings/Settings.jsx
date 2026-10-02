import React, { useState } from 'react';
import { Wifi, Cpu, SlidersHorizontal, User, RotateCcw, LogOut } from 'lucide-react';
import './Settings.css';

export default function SettingsPage({ appState, setAppState, showToast, onLogout }) {
  const [controlSource, setControlSource] = useState('Web App');
  const [isConnected, setIsConnected] = useState(false);

  const handleReconnect = () => {
    showToast('Attempting device pairing...');
    setTimeout(() => {
      setIsConnected(true);
      showToast('Device connected successfully!');
    }, 1500);
  };

  const handleReset = (type) => {
    if (type === 'lighting') {
      setAppState((prev) => ({
        ...prev,
        lightsOn: true,
        brightness: 75,
        lightingMode: 'Sound Reactive',
        soundReactiveOn: true
      }));
      showToast('Reset Lighting controls to default.');
    } else if (type === 'fountain') {
      setAppState((prev) => ({
        ...prev,
        fountainOn: true,
        fountainStrength: 100
      }));
      showToast('Reset Fountain controls to default.');
    } else if (type === 'colors') {
      setAppState((prev) => ({
        ...prev,
        fountainColor: '#77898D',
        circleColor: '#D4B78C'
      }));
      showToast('Reset Colors to default.');
    } else if (type === 'all') {
      setAppState({
        lightsOn: true,
        brightness: 75,
        lightingMode: 'Sound Reactive',
        soundReactiveOn: true,
        micSensitivity: 50,
        reactionIntensity: 65,
        autoDimming: true,
        simulatedLight: 72,

        fountainOn: true,
        fountainStrength: 100,
        fountainColor: '#77898D',

        audioPlaying: false,
        audioTrack: 'No audio selected',
        volume: 70,

        circleColor: '#D4B78C'
      });
      showToast('Reset ALL controls to factory defaults!');
    }
  };

  return (
    <div className="page-container">
      {/* Header */}
      <div className="page-header-text">
        <div className="section-breadcrumb settings-breadcrumb">PREFERENCES</div>
        <h1 className="page-main-title">Settings & Sensors</h1>
        <p className="page-subtitle">The little details behind your little world.</p>
      </div>

      {/* Grid of 4 Cards: Connection, Sensors, Control Source, Account */}
      <div className="settings-grid">
        {/* 1. Connection Card */}
        <div className="control-card">
          <div className="card-header">
            <div className="card-title-group">
              <div className="icon-badge sage">
                <Wifi size={22} />
              </div>
              <h3 className="card-title">Connection</h3>
            </div>
          </div>

          <div className="connection-status-row">
            <span className={`status-gold-dot ${isConnected ? 'connected' : ''}`}></span>
            <span className="status-gold-text">
              {isConnected ? 'Connected • Live Hardware' : 'Not connected · Preview mode'}
            </span>
          </div>

          <p className="card-description-subtext">
            Physical device pairing is not configured yet. Controls currently update this preview.
          </p>

          <button className="btn-card-action" onClick={handleReconnect}>
            {isConnected ? 'Disconnect' : 'Reconnect'}
          </button>
        </div>

        {/* 2. Sensors Card */}
        <div className="control-card">
          <div className="card-header">
            <div className="card-title-group">
              <div className="icon-badge sage">
                <Cpu size={22} />
              </div>
              <h3 className="card-title">Sensors</h3>
            </div>
          </div>

          <div className="sensor-items-list">
            <div className="sensor-item-row">
              <span className="sensor-name">Ambient light sensor</span>
              <span className="sensor-val gold">
                <span className="dot gold-dot"></span> Simulated · {appState.simulatedLight}%
              </span>
            </div>

            <div className="sensor-item-row">
              <span className="sensor-name">Electret microphone</span>
              <span className="sensor-val gold">
                <span className="dot gold-dot"></span> Browser
              </span>
            </div>

            <div className="sensor-item-row">
              <span className="sensor-name">Color sensor</span>
              <span className="sensor-val muted">
                <span className="dot muted-dot"></span> Not linked
              </span>
            </div>
          </div>
        </div>

        {/* 3. Control Source Card */}
        <div className="control-card">
          <div className="card-header">
            <div className="card-title-group">
              <div className="icon-badge sage">
                <SlidersHorizontal size={22} />
              </div>
              <h3 className="card-title">Control source</h3>
            </div>
          </div>

          <div className="source-buttons-row">
            {['Web App', 'Onboard Interface', 'Sensor'].map((src) => (
              <button
                key={src}
                className={`source-pill-btn ${controlSource === src ? 'active' : ''}`}
                onClick={() => {
                  setControlSource(src);
                  showToast(`Control source switched to ${src}`);
                }}
              >
                {src}
              </button>
            ))}
          </div>

          <p className="card-description-subtext mt-2">
            Source selection is saved; hardware switching requires a device connection.
          </p>
        </div>

        {/* 4. Account Card */}
        <div className="control-card">
          <div className="card-header">
            <div className="card-title-group">
              <div className="icon-badge sage">
                <User size={22} />
              </div>
              <h3 className="card-title">Account</h3>
            </div>
          </div>

          <div className="account-user-name">Ronald Sevilla</div>

          <button className="btn-logout" onClick={onLogout}>
            <LogOut size={16} />
            <span>Logout</span>
          </button>
        </div>
      </div>

      {/* 5. System Card (Full Width) */}
      <div className="control-card full-width-card">
        <div className="card-header">
          <div className="card-title-group">
            <div className="icon-badge sage">
              <RotateCcw size={22} />
            </div>
            <h3 className="card-title">System</h3>
          </div>
        </div>

        <div className="system-actions-grid">
          <button className="btn-system-reset" onClick={() => handleReset('lighting')}>
            Reset Lighting
          </button>
          <button className="btn-system-reset" onClick={() => handleReset('fountain')}>
            Reset Fountain
          </button>
          <button className="btn-system-reset" onClick={() => handleReset('colors')}>
            Reset Colors
          </button>
          <button className="btn-system-reset highlight" onClick={() => handleReset('all')}>
            Reset All Controls
          </button>
        </div>
      </div>
    </div>
  );
}
