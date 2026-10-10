import React from 'react';
import { Waves, Droplet } from 'lucide-react';
import DioramaCanvas from '../../components/DioramaCanvas';
import './Fountain.css';

export default function Fountain({ appState, setAppState, showToast }) {
  const { fountainOn, fountainStrength, fountainAuxStrength, fountainForceSensorOn, fountainPattern, fountainColor } = appState;

  return (
    <div className="page-container">
      {/* Header */}
      <div className="page-header-text">
        <div className="section-breadcrumb fountain-breadcrumb">CONTROLS / 02</div>
        <h1 className="page-main-title">Fountain</h1>
        <p className="page-subtitle">Let the water find its rhythm.</p>
      </div>

      {/* 3D Diorama Canvas */}
      <DioramaCanvas
        lightsOn={appState.lightsOn}
        brightness={appState.brightness}
        lightingMode={appState.lightingMode}
        soundReactiveOn={appState.soundReactiveOn}
        fountainOn={fountainOn}
        fountainStrength={fountainStrength}
        fountainAuxStrength={fountainAuxStrength}
        fountainForceSensorOn={fountainForceSensorOn}
        fountainPattern={fountainPattern}
        fountainColor={fountainColor}
        circleColor={appState.circleColor}
        autoDimming={appState.autoDimming}
        audioPlaying={appState.audioPlaying}
        onGateClick={() => showToast('✨ Diorama Gate Tapped!')}
      />

      {/* Shared pump speed */}
      <div className="control-card">
        <div className="card-header">
          <div className="card-title-group">
            <div className="icon-badge teal">
              <Droplet size={22} />
            </div>
            <div className="title-stack">
              <h3 className="card-title">Shared pump speed</h3>
              <span className="card-status-subtext">
                {fountainForceSensorOn
                  ? 'Driven by Force Sensor'
                  : 'Both pumps run together'}
              </span>
            </div>
          </div>
          <div className="strength-value-label">
            <span className="font-bold text-lg">
              {fountainStrength}%
            </span>
          </div>
        </div>

        {/* Slider */}
        <div className="card-slider-group mb-2">
          <input
            type="range"
            min="0"
            max="100"
            value={fountainStrength}
            disabled={fountainForceSensorOn}
            className="custom-range-slider teal-range"
            onChange={(e) => {
              const val = Number(e.target.value);
              setAppState((prev) => ({
                ...prev,
                fountainStrength: val,
                fountainAuxStrength: val
              }));
            }}
          />
        </div>
        <p className="text-xs text-muted mt-2">One speed setting is shared by both TB6612FNG pump channels.</p>

        {/* Force Sensor Toggle Row */}
        <div className="card-footer-row border-t pt-3 mt-3">
          <div className="title-stack">
            <span className="footer-label font-bold">Force Sensor Control</span>
            <span className="text-xs text-muted">
              {fountainForceSensorOn ? 'Overrides sliders with pressure' : 'Enable FSR pressure sensor'}
            </span>
          </div>
          <label className="toggle-switch">
            <input
              type="checkbox"
              checked={fountainForceSensorOn}
              disabled={!fountainOn}
              onChange={(e) => {
                const on = e.target.checked;
                setAppState((prev) => ({ ...prev, fountainForceSensorOn: on }));
                showToast(on ? '⚡ Force Sensor ON: Pressure controls fountain strength' : 'Force Sensor OFF: Sliders restored');
              }}
            />
            <span className="slider round teal-toggle"></span>
          </label>
        </div>

        {/* Force Sensor Active Status */}
        {fountainForceSensorOn && (
          <div className="force-sensor-status-box mt-3">
            <div className="force-status-header">
              <span className="force-status-tag">
                <span className="scanning-dot green"></span> SENSOR ACTIVE
              </span>
              <span className="force-val-label">Auto Flow Control</span>
            </div>
            <p className="force-status-desc">
              Physical force sensor is active. Pressing the FSR sensor dynamically controls fountain spray height.
            </p>
            <div className="force-meter-bar-wrap">
              <div className="force-meter-bar-fill"></div>
            </div>
          </div>
        )}
      </div>
    </div>
  );
}
