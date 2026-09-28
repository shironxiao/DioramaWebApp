import React, { useState } from 'react';
import { Lightbulb, Mic, Sun, Palette, Sparkles } from 'lucide-react';
import DioramaCanvas from '../../components/DioramaCanvas';
import './Light&Color.css';

export default function LightAndColor({ appState, setAppState, showToast }) {
  const {
    lightsOn, brightness, lightingMode, soundReactiveOn, micSensitivity,
    reactionIntensity, autoDimming, simulatedLight, circleColor, fountainColor
  } = appState;

  // Local state for color RGB inputs
  const [circleRgb, setCircleRgb] = useState({ r: 212, g: 183, b: 140 });
  const [fountainRgb, setFountainRgb] = useState({ r: 119, g: 137, b: 141 });
  const [micActive, setMicActive] = useState(false);

  const hexFromRgb = (r, g, b) => {
    const toHex = (c) => Math.max(0, Math.min(255, c)).toString(16).padStart(2, '0');
    return `#${toHex(r)}${toHex(g)}${toHex(b)}`;
  };

  const applyCircleColor = () => {
    const hex = hexFromRgb(circleRgb.r, circleRgb.g, circleRgb.b);
    setAppState((prev) => ({ ...prev, circleColor: hex }));
    showToast(`Applied central circle color: ${hex.toUpperCase()}`);
  };

  const applyFountainColor = () => {
    const hex = hexFromRgb(fountainRgb.r, fountainRgb.g, fountainRgb.b);
    setAppState((prev) => ({ ...prev, fountainColor: hex }));
    showToast(`Applied fountain color: ${hex.toUpperCase()}`);
  };

  return (
    <div className="page-container">
      {/* Header */}
      <div className="page-header-text">
        <div className="section-breadcrumb">CONTROLS / 01</div>
        <h1 className="page-main-title">Lights & Colors</h1>
        <p className="page-subtitle">Set the mood and color of your little world.</p>
      </div>

      {/* 3D Diorama Preview */}
      <DioramaCanvas
        lightsOn={lightsOn}
        brightness={brightness}
        fountainOn={appState.fountainOn}
        fountainStrength={appState.fountainStrength}
        fountainPattern={appState.fountainPattern}
        fountainColor={fountainColor}
        circleColor={circleColor}
        autoDimming={autoDimming}
        onGateClick={() => showToast('✨ Diorama Gate Tapped!')}
      />

      {/* SECTION 1: LIGHTING */}
      <div className="section-block">
        <h2 className="section-block-title">Lighting</h2>
        <div className="controls-grid">
          {/* Master Lighting */}
          <div className="control-card">
            <div className="card-header">
              <div className="card-title-group">
                <div className="icon-badge amber">
                  <Lightbulb size={22} />
                </div>
                <div className="title-stack">
                  <h3 className="card-title">Master lighting</h3>
                  <span className="card-status-subtext">{lightsOn ? 'Lights on' : 'Lights off'}</span>
                </div>
              </div>
              <label className="toggle-switch">
                <input
                  type="checkbox"
                  checked={lightsOn}
                  onChange={(e) => setAppState((prev) => ({ ...prev, lightsOn: e.target.checked }))}
                />
                <span className="slider round amber-toggle"></span>
              </label>
            </div>
          </div>

          {/* Brightness */}
          <div className="control-card">
            <div className="card-header">
              <div className="card-title-group">
                <div className="icon-badge amber">
                  <Lightbulb size={22} />
                </div>
                <div className="title-stack">
                  <h3 className="card-title">Brightness</h3>
                </div>
              </div>
              <span className="value-label font-bold">{brightness}%</span>
            </div>
            <div className="card-slider-group">
              <input
                type="range"
                min="0"
                max="100"
                value={brightness}
                disabled={!lightsOn}
                className="custom-range-slider amber-range"
                onChange={(e) => setAppState((prev) => ({ ...prev, brightness: Number(e.target.value) }))}
              />
            </div>
          </div>
        </div>
      </div>

      {/* SECTION 2: LIGHTING MODES */}
      <div className="section-block">
        <h2 className="section-block-title">Lighting modes</h2>
        <div className="modes-grid">
          <div
            className={`mode-card ${lightingMode === 'Basic' ? 'selected' : ''}`}
            onClick={() => setAppState((prev) => ({ ...prev, lightingMode: 'Basic' }))}
          >
            <h4 className="mode-name">Basic</h4>
            <p className="mode-desc">Simple, steady lighting</p>
          </div>

          <div
            className={`mode-card ${lightingMode === 'Colorful' ? 'selected' : ''}`}
            onClick={() => setAppState((prev) => ({ ...prev, lightingMode: 'Colorful' }))}
          >
            <h4 className="mode-name">Colorful</h4>
            <p className="mode-desc">A soft colored glow</p>
          </div>

          <div
            className={`mode-card ${lightingMode === 'Sound Reactive' ? 'selected' : ''}`}
            onClick={() => setAppState((prev) => ({ ...prev, lightingMode: 'Sound Reactive' }))}
          >
            <h4 className="mode-name">Sound Reactive</h4>
            <p className="mode-desc">Responds to microphone input</p>
          </div>

          <div
            className={`mode-card ${lightingMode === 'Sensor Controlled' ? 'selected' : ''}`}
            onClick={() => setAppState((prev) => ({ ...prev, lightingMode: 'Sensor Controlled' }))}
          >
            <h4 className="mode-name">Sensor Controlled</h4>
            <p className="mode-desc">Follows ambient light</p>
          </div>
        </div>

        {/* Sub Controls: Sound Reactive & Ambient Light */}
        <div className="controls-grid mt-4">
          <div className="control-card">
            <div className="card-header">
              <div className="card-title-group">
                <div className="icon-badge amber">
                  <Mic size={20} />
                </div>
                <div className="title-stack">
                  <h3 className="card-title">Sound-reactive lighting</h3>
                  <span className="card-status-subtext">{soundReactiveOn ? 'On' : 'Off'}</span>
                </div>
              </div>
              <label className="toggle-switch">
                <input
                  type="checkbox"
                  checked={soundReactiveOn}
                  onChange={(e) => setAppState((prev) => ({ ...prev, soundReactiveOn: e.target.checked }))}
                />
                <span className="slider round amber-toggle"></span>
              </label>
            </div>

            <div className="card-slider-group gap-3">
              <div className="slider-label-row">
                <span>Microphone sensitivity</span>
                <span className="value-label">{micSensitivity}%</span>
              </div>
              <input
                type="range"
                min="0"
                max="100"
                value={micSensitivity}
                className="custom-range-slider amber-range"
                onChange={(e) => setAppState((prev) => ({ ...prev, micSensitivity: Number(e.target.value) }))}
              />

              <div className="slider-label-row">
                <span>Light reaction intensity</span>
                <span className="value-label">{reactionIntensity}%</span>
              </div>
              <input
                type="range"
                min="0"
                max="100"
                value={reactionIntensity}
                className="custom-range-slider amber-range"
                onChange={(e) => setAppState((prev) => ({ ...prev, reactionIntensity: Number(e.target.value) }))}
              />
            </div>
          </div>

          {/* Ambient light control */}
          <div className="control-card">
            <div className="card-header">
              <div className="card-title-group">
                <div className="icon-badge amber">
                  <Sun size={20} />
                </div>
                <div className="title-stack">
                  <h3 className="card-title">Ambient light control</h3>
                </div>
              </div>
            </div>

            <div className="card-footer-row border-none pt-0">
              <span>Auto dimming</span>
              <label className="toggle-switch">
                <input
                  type="checkbox"
                  checked={autoDimming}
                  onChange={(e) => setAppState((prev) => ({ ...prev, autoDimming: e.target.checked }))}
                />
                <span className="slider round amber-toggle"></span>
              </label>
            </div>

            <div className="card-slider-group">
              <div className="slider-label-row">
                <span>Simulated room light</span>
                <span className="value-label">{simulatedLight}%</span>
              </div>
              <input
                type="range"
                min="0"
                max="100"
                value={simulatedLight}
                className="custom-range-slider amber-range"
                onChange={(e) => setAppState((prev) => ({ ...prev, simulatedLight: Number(e.target.value) }))}
              />
              <div className="range-sub-labels">
                <span>Bright</span>
                <span>Dark</span>
              </div>
            </div>
          </div>
        </div>
      </div>

      {/* SECTION 3: COLORS */}
      <div className="section-block">
        <h2 className="section-block-title">Colors</h2>
        <div className="colors-grid">
          {/* Central Circle Color */}
          <div className="control-card">
            <div className="card-header">
              <div className="card-title-group">
                <div className="icon-badge amber">
                  <Palette size={20} />
                </div>
                <h3 className="card-title">Central circle</h3>
              </div>
            </div>

            <div className="color-picker-flex">
              <div
                className="color-swatch-box"
                style={{ backgroundColor: hexFromRgb(circleRgb.r, circleRgb.g, circleRgb.b) }}
              ></div>
              <div className="rgb-inputs-group">
                <span className="rgb-label-title">RGB values</span>
                <div className="rgb-fields-row">
                  <div className="field">
                    <span>R</span>
                    <input
                      type="number"
                      value={circleRgb.r}
                      onChange={(e) => setCircleRgb({ ...circleRgb, r: Number(e.target.value) })}
                    />
                  </div>
                  <div className="field">
                    <span>G</span>
                    <input
                      type="number"
                      value={circleRgb.g}
                      onChange={(e) => setCircleRgb({ ...circleRgb, g: Number(e.target.value) })}
                    />
                  </div>
                  <div className="field">
                    <span>B</span>
                    <input
                      type="number"
                      value={circleRgb.b}
                      onChange={(e) => setCircleRgb({ ...circleRgb, b: Number(e.target.value) })}
                    />
                  </div>
                </div>
              </div>
            </div>

            <div className="color-card-footer">
              <span className="hex-display">{hexFromRgb(circleRgb.r, circleRgb.g, circleRgb.b).toUpperCase()}</span>
              <button className="btn-amber" onClick={applyCircleColor}>Apply color</button>
            </div>
          </div>

          {/* Fountain Color */}
          <div className="control-card">
            <div className="card-header">
              <div className="card-title-group">
                <div className="icon-badge amber">
                  <Palette size={20} />
                </div>
                <h3 className="card-title">Fountain</h3>
              </div>
            </div>

            <div className="color-picker-flex">
              <div
                className="color-swatch-box"
                style={{ backgroundColor: hexFromRgb(fountainRgb.r, fountainRgb.g, fountainRgb.b) }}
              ></div>
              <div className="rgb-inputs-group">
                <span className="rgb-label-title">RGB values</span>
                <div className="rgb-fields-row">
                  <div className="field">
                    <span>R</span>
                    <input
                      type="number"
                      value={fountainRgb.r}
                      onChange={(e) => setFountainRgb({ ...fountainRgb, r: Number(e.target.value) })}
                    />
                  </div>
                  <div className="field">
                    <span>G</span>
                    <input
                      type="number"
                      value={fountainRgb.g}
                      onChange={(e) => setFountainRgb({ ...fountainRgb, g: Number(e.target.value) })}
                    />
                  </div>
                  <div className="field">
                    <span>B</span>
                    <input
                      type="number"
                      value={fountainRgb.b}
                      onChange={(e) => setFountainRgb({ ...fountainRgb, b: Number(e.target.value) })}
                    />
                  </div>
                </div>
              </div>
            </div>

            <div className="color-card-footer">
              <span className="hex-display">{hexFromRgb(fountainRgb.r, fountainRgb.g, fountainRgb.b).toUpperCase()}</span>
              <button className="btn-amber" onClick={applyFountainColor}>Apply color</button>
            </div>
          </div>

          {/* Color Sensor */}
          <div className="control-card col-span-full">
            <div className="card-header">
              <div className="card-title-group">
                <div className="icon-badge amber">
                  <Sparkles size={20} />
                </div>
                <div className="title-stack">
                  <h3 className="card-title">Color sensor</h3>
                  <span className="card-status-subtext">Demo sample - No color sensor linked</span>
                </div>
              </div>
            </div>

            <div className="sensor-content-row">
              <div className="detected-color-info">
                <div className="color-swatch-box small" style={{ backgroundColor: '#D4B78C' }}></div>
                <div className="detected-text">
                  <span className="text-sm font-semibold">Detected color</span>
                  <span className="text-xs text-muted">#D4B78C - RGB 212, 183, 140</span>
                </div>
              </div>
              <button
                className="btn-amber"
                onClick={() => {
                  setAppState((prev) => ({ ...prev, circleColor: '#D4B78C' }));
                  showToast('Applied detected sensor color #D4B78C');
                }}
              >
                Use detected color
              </button>
            </div>
          </div>
        </div>
      </div>

      {/* SECTION 4: MICROPHONE */}
      <div className="section-block">
        <h2 className="section-block-title">Microphone</h2>
        <div className="control-card">
          <div className="card-header">
            <div className="card-title-group">
              <div className="icon-badge amber">
                <Mic size={20} />
              </div>
              <div className="title-stack">
                <h3 className="card-title">Microphone</h3>
                <span className="card-status-subtext">{micActive ? 'Microphone active' : 'Microphone off'}</span>
              </div>
            </div>
            <button
              className="btn-outline"
              onClick={() => {
                const next = !micActive;
                setMicActive(next);
                showToast(next ? 'Microphone Enabled' : 'Microphone Disabled');
              }}
            >
              {micActive ? 'Disable microphone' : 'Enable microphone'}
            </button>
          </div>

          <div className="sound-level-meter-wrapper">
            <div className="sound-level-labels">
              <span>Sound level</span>
              <span>{micActive ? '42%' : '0%'}</span>
            </div>
            <div className="level-bar-background">
              <div className="level-bar-fill" style={{ width: micActive ? '42%' : '0%' }}></div>
            </div>
            <div className="dots-meter-row">
              {Array.from({ length: 26 }).map((_, i) => (
                <span key={i} className={`meter-dot ${micActive && i < 11 ? 'active' : ''}`}></span>
              ))}
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}
