import React, { useState } from 'react';
import { Waves, Droplet } from 'lucide-react';
import DioramaCanvas from '../../components/DioramaCanvas';
import './Fountain.css';

export default function Fountain({ appState, setAppState, showToast }) {
  const { fountainOn, fountainStrength, fountainAuxStrength, fountainForceSensorOn, fountainPattern, fountainColor } = appState;
  const [strengthTarget, setStrengthTarget] = useState('both'); // 'both', 'left', 'right'
  const [sliderValue, setSliderValue] = useState(fountainStrength);

  // Apply Strength to Target Handler
  const applyStrengthToTarget = (target) => {
    setAppState((prev) => {
      const next = { ...prev };
      if (target === 'both') {
        next.fountainStrength = sliderValue;
        next.fountainAuxStrength = sliderValue;
      } else if (target === 'left') {
        next.fountainStrength = sliderValue;
      } else if (target === 'right') {
        next.fountainAuxStrength = sliderValue;
      }
      return next;
    });
    if (target === 'both') {
      showToast(`✨ Applied ${sliderValue}% water strength to BOTH fountains!`);
    } else if (target === 'left') {
      showToast(`✨ Applied ${sliderValue}% water strength to Left Fountain!`);
    } else if (target === 'right') {
      showToast(`✨ Applied ${sliderValue}% water strength to Right Fountain!`);
    }
  };

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

      {/* Water Strength Card with target selection + Force Sensor */}
      <div className="control-card">
        <div className="card-header">
          <div className="card-title-group">
            <div className="icon-badge teal">
              <Droplet size={22} />
            </div>
            <div className="title-stack">
              <h3 className="card-title">Water strength</h3>
              <span className="card-status-subtext">
                {fountainForceSensorOn
                  ? 'Driven by Force Sensor'
                  : `Active: ${strengthTarget === 'both' ? 'Both Fountains' : strengthTarget === 'left' ? 'Left Fountain' : 'Right Fountain'}`}
              </span>
            </div>
          </div>
          <div className="strength-value-label">
            <span className="font-bold text-lg">
              {strengthTarget === 'both'
                ? `${fountainStrength}% / ${fountainAuxStrength}%`
                : strengthTarget === 'left'
                ? `${fountainStrength}%`
                : `${fountainAuxStrength}%`}
            </span>
          </div>
        </div>

        {/* Slider */}
        <div className="card-slider-group mb-2">
          <input
            type="range"
            min="0"
            max="100"
            value={
              strengthTarget === 'both'
                ? sliderValue
                : strengthTarget === 'left'
                ? fountainStrength
                : fountainAuxStrength
            }
            disabled={fountainForceSensorOn}
            className="custom-range-slider teal-range"
            onChange={(e) => {
              const val = Number(e.target.value);
              setSliderValue(val);
              setAppState((prev) => {
                const next = { ...prev };
                if (strengthTarget === 'both') {
                  next.fountainStrength = val;
                  next.fountainAuxStrength = val;
                } else if (strengthTarget === 'left') {
                  next.fountainStrength = val;
                } else if (strengthTarget === 'right') {
                  next.fountainAuxStrength = val;
                }
                return next;
              });
            }}
          />
        </div>

        {/* Target Selection & Apply Buttons */}
        <div className="apply-targets-section mt-3">
          <span className="apply-targets-header">APPLY STRENGTH TO:</span>
          <div className="apply-targets-grid">
            <button
              type="button"
              className={`btn-target-apply ${strengthTarget === 'both' ? 'active-target-btn' : ''}`}
              onClick={() => {
                setStrengthTarget('both');
                applyStrengthToTarget('both');
              }}
            >
              <span className="target-title">Both Fountains</span>
              <span className="target-sub">{fountainStrength}% &amp; {fountainAuxStrength}%</span>
            </button>

            <button
              type="button"
              className={`btn-target-apply ${strengthTarget === 'left' ? 'active-target-btn' : ''}`}
              onClick={() => {
                setStrengthTarget('left');
                applyStrengthToTarget('left');
              }}
            >
              <span className="target-title">Left Fountain</span>
              <span className="target-sub">{fountainStrength}%</span>
            </button>

            <button
              type="button"
              className={`btn-target-apply ${strengthTarget === 'right' ? 'active-target-btn' : ''}`}
              onClick={() => {
                setStrengthTarget('right');
                applyStrengthToTarget('right');
              }}
            >
              <span className="target-title">Right Fountain</span>
              <span className="target-sub">{fountainAuxStrength}%</span>
            </button>
          </div>
        </div>

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
