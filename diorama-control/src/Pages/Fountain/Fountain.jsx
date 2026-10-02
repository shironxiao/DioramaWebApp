import React, { useState } from 'react';
import { Waves, Droplet, Palette } from 'lucide-react';
import DioramaCanvas from '../../components/DioramaCanvas';
import './Fountain.css';

export default function Fountain({ appState, setAppState, showToast }) {
  const { fountainOn, fountainStrength, fountainAuxStrength, fountainForceSensorOn, fountainPattern, fountainColor } = appState;

  const [rgb, setRgb] = useState({ r: 119, g: 137, b: 141 });

  const hexFromRgb = (r, g, b) => {
    const toHex = (c) => Math.max(0, Math.min(255, c)).toString(16).padStart(2, '0');
    return `#${toHex(r)}${toHex(g)}${toHex(b)}`;
  };

  const applyFountainColor = () => {
    const hex = hexFromRgb(rgb.r, rgb.g, rgb.b);
    setAppState((prev) => ({ ...prev, fountainColor: hex }));
    showToast(`Applied fountain color: ${hex.toUpperCase()}`);
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
        fountainOn={fountainOn}
        fountainStrength={fountainStrength}
        fountainPattern={fountainPattern}
        fountainColor={fountainColor}
        circleColor={appState.circleColor}
        autoDimming={appState.autoDimming}
        audioPlaying={appState.audioPlaying}
        plazaRotationMode={appState.plazaRotationMode}
        onGateClick={() => showToast('✨ Diorama Gate Tapped!')}
      />

      {/* Top 2 Control Cards: Fountain Power & Water Strength */}
      <div className="controls-grid">
        {/* Fountain power */}
        <div className="control-card">
          <div className="card-header">
            <div className="card-title-group">
              <div className="icon-badge teal">
                <Waves size={22} />
              </div>
              <div className="title-stack">
                <h3 className="card-title">Fountain power</h3>
                <span className="card-status-subtext">{fountainOn ? 'Flowing' : 'Off'}</span>
              </div>
            </div>
            <label className="toggle-switch">
              <input
                type="checkbox"
                checked={fountainOn}
                onChange={(e) => setAppState((prev) => ({ ...prev, fountainOn: e.target.checked }))}
              />
              <span className="slider round teal-toggle"></span>
            </label>
          </div>
        </div>

        {/* Water strength */}
        <div className="control-card">
          <div className="card-header">
            <div className="card-title-group">
              <div className="icon-badge teal">
                <Droplet size={22} />
              </div>
              <div className="title-stack">
                <h3 className="card-title">Water strength</h3>
              </div>
            </div>
            <div className="strength-value-label">
              <span>Main</span>
              <span className="font-bold">{fountainStrength}%</span>
            </div>
          </div>

          <div className="card-slider-group mb-4">
            <input
              type="range"
              min="0"
              max="100"
              value={fountainStrength}
              disabled={!fountainOn || fountainForceSensorOn}
              className="custom-range-slider teal-range"
              onChange={(e) => setAppState((prev) => ({ ...prev, fountainStrength: Number(e.target.value) }))}
            />
          </div>
          
          <div className="strength-value-label px-5">
            <span className="text-sm font-medium">Aux Spout</span>
            <span className="font-bold text-sm float-right">{fountainAuxStrength}%</span>
          </div>
          <div className="card-slider-group mb-2">
            <input
              type="range"
              min="0"
              max="100"
              value={fountainAuxStrength}
              disabled={!fountainOn || fountainForceSensorOn}
              className="custom-range-slider teal-range"
              onChange={(e) => setAppState((prev) => ({ ...prev, fountainAuxStrength: Number(e.target.value) }))}
            />
          </div>
          
          <div className="card-footer-row border-t pt-2 mt-2">
            <div className="title-stack">
              <span className="footer-label font-bold">Force Sensor Control</span>
              <span className="text-xs text-muted">Overrides sliders</span>
            </div>
            <label className="toggle-switch">
              <input
                type="checkbox"
                checked={fountainForceSensorOn}
                disabled={!fountainOn}
                onChange={(e) => setAppState((prev) => ({ ...prev, fountainForceSensorOn: e.target.checked }))}
              />
              <span className="slider round teal-toggle"></span>
            </label>
          </div>
        </div>
      </div>



      {/* Fountain Color Card */}
      <div className="control-card">
        <div className="card-header">
          <div className="card-title-group">
            <div className="icon-badge teal">
              <Palette size={22} />
            </div>
            <h3 className="card-title">Fountain color</h3>
          </div>
        </div>

        <div className="color-picker-flex">
          <div
            className="color-swatch-box"
            style={{ backgroundColor: hexFromRgb(rgb.r, rgb.g, rgb.b) }}
          ></div>

          <div className="rgb-inputs-group">
            <span className="rgb-label-title">RGB values</span>
            <div className="rgb-fields-row">
              <div className="field">
                <span>R</span>
                <input
                  type="number"
                  value={rgb.r}
                  onChange={(e) => setRgb({ ...rgb, r: Number(e.target.value) })}
                />
              </div>
              <div className="field">
                <span>G</span>
                <input
                  type="number"
                  value={rgb.g}
                  onChange={(e) => setRgb({ ...rgb, g: Number(e.target.value) })}
                />
              </div>
              <div className="field">
                <span>B</span>
                <input
                  type="number"
                  value={rgb.b}
                  onChange={(e) => setRgb({ ...rgb, b: Number(e.target.value) })}
                />
              </div>
            </div>
          </div>
        </div>

        <div className="color-card-footer">
          <span className="hex-display">{hexFromRgb(rgb.r, rgb.g, rgb.b).toUpperCase()}</span>
          <button className="btn-teal" onClick={applyFountainColor}>Apply color</button>
        </div>
      </div>
    </div>
  );
}
