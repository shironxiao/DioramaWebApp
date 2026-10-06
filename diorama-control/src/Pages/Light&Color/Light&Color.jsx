import React, { useEffect, useState } from 'react';
import {
  Lightbulb,
  Mic,
  Palette,
  Sun,
  ChevronDown,
  ChevronUp,
  Check
} from 'lucide-react';
import DioramaCanvas from '../../components/DioramaCanvas';
import ColorPicker from '../../components/ColorPicker';
import { getLiveSensors } from '../../services/esp32Api';
import './Light&Color.css';

const LIGHTING_MODES = [
  {
    id: 'Basic',
    name: 'Basic',
    desc: 'Simple, steady warm lighting',
    icon: Lightbulb
  },
  {
    id: 'Colorful',
    name: 'Colorful',
    desc: 'Pick & apply custom colors to each RGB element',
    icon: Palette
  },
  {
    id: 'Sound Reactive',
    name: 'Sound Reactive',
    desc: 'Lights & inner circle react to sound sensor',
    icon: Mic
  },
  {
    id: 'Color Adaptive',
    name: 'Color Adaptive',
    desc: 'Follows ambient light sensor automatically',
    icon: Sun
  }
];

export default function LightAndColor({ appState, setAppState, showToast }) {
  const {
    lightsOn, brightness, lightingMode, soundReactiveOn,
    reactionIntensity, circleColor, fountainColor, fountainAuxColor, autoDimming
  } = appState;
  const isColorAdaptive = lightingMode === 'Color Adaptive';

  const [currentColor, setCurrentColor] = useState(circleColor || '#D4B78C');
  const [isColorsOpen, setIsColorsOpen] = useState(false);
  const [ambientTelemetry, setAmbientTelemetry] = useState({ connected: false, lux: null, brightness: null });

  useEffect(() => {
    if (!isColorAdaptive) return undefined;
    let active = true;
    const refreshAmbient = async () => {
      const data = await getLiveSensors();
      if (active) {
        setAmbientTelemetry(data?.ambientLight ?? { connected: false, lux: null, brightness: null });
      }
    };
    refreshAmbient();
    const interval = setInterval(refreshAmbient, 3000);
    return () => {
      active = false;
      clearInterval(interval);
    };
  }, [isColorAdaptive]);

  // Helper for color conversions
  const hexToRgb = (hex) => {
    const h = (hex || '#D4B78C').replace('#', '');
    const n = parseInt(h, 16);
    return { r: (n >> 16) & 255, g: (n >> 8) & 255, b: n & 255 };
  };
  const rgbToHex = (r, g, b) => {
    const c = v => Math.max(0, Math.min(255, v)).toString(16).padStart(2, '0');
    return `#${c(r)}${c(g)}${c(b)}`;
  };

  const activeRgb = hexToRgb(currentColor);

  const handlePickerChange = (newRgb) => {
    setCurrentColor(rgbToHex(newRgb.r, newRgb.g, newRgb.b));
  };

  // Mode Selection Handler
  const handleSelectMode = (modeId) => {
    if (modeId === 'Basic') {
      setAppState((prev) => ({
        ...prev,
        lightingMode: 'Basic',
        soundReactiveOn: false,
        ambientSensorOn: false,
        circleColor: '#D4B78C',
        fountainColor: '#77898D',
        fountainAuxColor: '#3B9DB3'
      }));
    } else if (modeId === 'Colorful') {
      setAppState((prev) => ({ ...prev, lightingMode: 'Colorful', soundReactiveOn: false, ambientSensorOn: false }));
      setIsColorsOpen(false);
    } else if (modeId === 'Sound Reactive') {
      setAppState((prev) => ({ ...prev, lightingMode: 'Sound Reactive', soundReactiveOn: true, ambientSensorOn: false }));
      showToast('🎵 Sound Reactive: All RGB lights, fountain & center circle react to sound!');
    } else if (modeId === 'Color Adaptive') {
      setAppState((prev) => ({ ...prev, lightingMode: 'Color Adaptive', soundReactiveOn: false, ambientSensorOn: true }));
      showToast('☀️ Color Adaptive: Ambient light sensor automatically active');
    }
  };

  // Apply Color to Target Handler: 'all', 'left_fountain', 'right_fountain', 'center'
  const applyColorToTarget = (target) => {
    let toastMsg = '';
    setAppState((prev) => {
      const next = { ...prev };
      if (target === 'all') {
        next.circleColor = currentColor;
        next.fountainColor = currentColor;
        next.fountainAuxColor = currentColor;
        toastMsg = `✨ Applied ${currentColor.toUpperCase()} to ALL!`;
      } else if (target === 'left_fountain') {
        next.fountainColor = currentColor;
        toastMsg = `✨ Applied ${currentColor.toUpperCase()} to Left Fountain!`;
      } else if (target === 'right_fountain') {
        next.fountainAuxColor = currentColor;
        toastMsg = `✨ Applied ${currentColor.toUpperCase()} to Right Fountain!`;
      } else if (target === 'center') {
        next.circleColor = currentColor;
        toastMsg = `✨ Applied ${currentColor.toUpperCase()} to the Center!`;
      }
      return next;
    });
    showToast(toastMsg);
  };

  return (
    <div className="page-container compact-for-mobile">
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
        lightingMode={lightingMode}
        soundReactiveOn={soundReactiveOn}
        fountainOn={appState.fountainOn}
        fountainStrength={appState.fountainStrength}
        fountainAuxStrength={appState.fountainAuxStrength}
        fountainForceSensorOn={appState.fountainForceSensorOn}
        fountainPattern={appState.fountainPattern}
        fountainColor={fountainColor}
        fountainAuxColor={appState.fountainAuxColor}
        circleColor={circleColor}
        autoDimming={autoDimming}
        audioPlaying={appState.audioPlaying}
        onGateClick={() => showToast('✨ Diorama Gate Tapped!')}
      />

      {/* SECTION: 4-GRID LIGHTING MODES */}
      <div className="section-block">
        <div className="section-header-row">
          <h2 className="section-block-title">Lighting Modes</h2>
          <span className="section-sub-badge">{lightingMode}</span>
        </div>

        <div className="modes-grid">
          {LIGHTING_MODES.map((mode) => {
            const IconComponent = mode.icon;
            const isSelected = lightingMode === mode.id;
            return (
              <div
                key={mode.id}
                className={`mode-card ${isSelected ? 'selected' : ''}`}
                onClick={() => handleSelectMode(mode.id)}
                role="button"
                tabIndex={0}
              >
                <div className="mode-card-header">
                  <div className={`icon-badge ${isSelected ? 'amber-active' : 'amber'}`}>
                    <IconComponent size={20} />
                  </div>
                  {isSelected && (
                    <div className="mode-active-pill">
                      <Check size={14} />
                      <span>Active</span>
                    </div>
                  )}
                </div>
                <div className="mode-card-body">
                  <h4 className="mode-name">{mode.name}</h4>
                  <p className="mode-desc">{mode.desc}</p>
                </div>
              </div>
            );
          })}
        </div>

        {/* Sound Reactive Sub-Control (when Sound Reactive is active) */}
        {lightingMode === 'Sound Reactive' && (
          <div className="control-card mt-2">
            <div className="card-header">
              <div className="card-title-group">
                <div className="icon-badge amber">
                  <Mic size={20} />
                </div>
                <div className="title-stack">
                  <h3 className="card-title">Sound-reactive lighting</h3>
                  <span className="card-status-subtext">{soundReactiveOn ? 'Active — all RGB elements shift with sound' : 'Off'}</span>
                </div>
              </div>
              <label className="toggle-switch">
                <input
                  type="checkbox"
                  checked={soundReactiveOn}
                  onChange={(e) => {
                    setAppState((prev) => ({ ...prev, soundReactiveOn: e.target.checked }));
                  }}
                />
                <span className="slider round amber-toggle"></span>
              </label>
            </div>
            <div className="card-slider-group gap-3">
              <div className="slider-label-row">
                <span>Reaction intensity</span>
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
        )}

        {/* Colorful Mode: Color Control & Detection Sub-Card */}
        {lightingMode === 'Colorful' && (
          <div className={`control-card mt-2 ${isColorsOpen ? 'expanded' : 'collapsed'} color-collapsible-card`}>
            <div
              className="card-header color-accordion-header"
              onClick={() => setIsColorsOpen(!isColorsOpen)}
            >
              <div className="card-title-group">
                <div className="icon-badge amber">
                  <Palette size={20} />
                </div>
                <div className="title-stack">
                  <div className="title-with-swatch">
                    <h3 className="card-title">Color Control</h3>
                    <span
                      className="color-mini-swatch"
                      style={{ backgroundColor: currentColor }}
                      title={currentColor}
                    />
                    <span className="hex-mini-tag">{(currentColor || '').toUpperCase()}</span>
                  </div>
                  <span className="card-status-subtext">
                    {isColorsOpen ? 'Pick a color and apply it to a lighting zone' : 'Tap to open color tools'}
                  </span>
                </div>
              </div>
              <div className="header-right-action">
                <div className={`dropdown-toggle-pill ${isColorsOpen ? 'active' : ''}`}>
                  <span>{isColorsOpen ? 'Close' : 'Choose'}</span>
                  {isColorsOpen ? <ChevronUp size={16} /> : <ChevronDown size={16} />}
                </div>
              </div>
            </div>

            {isColorsOpen && (
              <div className="color-collapsible-body">
                <ColorPicker rgb={activeRgb} onChange={handlePickerChange} />

                <div className="color-card-footer" style={{ marginTop: 14 }}>
                  <div className="hex-display-group">
                    <span className="hex-label">HEX: </span>
                    <span className="hex-display">{(currentColor || '').toUpperCase()}</span>
                  </div>
                </div>

                <div className="apply-targets-section">
                  <span className="apply-targets-header">APPLY COLOR TO:</span>
                  <div className="apply-targets-grid">
                    <button className="btn-target-apply" onClick={() => applyColorToTarget('all')}>
                      <span className="target-title">ALL</span>
                    </button>
                    <button className="btn-target-apply" onClick={() => applyColorToTarget('left_fountain')}>
                      <span className="target-title">Left Fountain</span>
                    </button>
                    <button className="btn-target-apply" onClick={() => applyColorToTarget('right_fountain')}>
                      <span className="target-title">Right Fountain</span>
                    </button>
                    <button className="btn-target-apply" onClick={() => applyColorToTarget('center')}>
                      <span className="target-title">The Center</span>
                    </button>
                  </div>
                </div>
              </div>
            )}
          </div>
        )}

        {/* Color Adaptive: Ambient Sensor Status Banner */}
        {isColorAdaptive && (
          <div className="control-card mt-2 ambient-status-card">
            <div className="card-header">
              <div className="card-title-group">
                <div className="icon-badge amber">
                  <Sun size={20} />
                </div>
                <div className="title-stack">
                  <h3 className="card-title">Ambient Light Sensor</h3>
                  <span className="card-status-subtext" style={{ color: ambientTelemetry.connected ? '#4ade80' : '#f87171' }}>
                    {ambientTelemetry.connected
                      ? `● ${Number(ambientTelemetry.lux).toFixed(1)} lx — output ${ambientTelemetry.brightness}%`
                      : '● VEML7700 offline — adaptive dimming paused'}
                  </span>
                </div>
              </div>
              <span className="sensor-scanning-pill" style={{
                background: ambientTelemetry.connected ? 'rgba(74,222,128,0.15)' : 'rgba(248,113,113,0.15)',
                color: ambientTelemetry.connected ? '#4ade80' : '#f87171',
                border: `1px solid ${ambientTelemetry.connected ? 'rgba(74,222,128,0.3)' : 'rgba(248,113,113,0.3)'}`
              }}>
                <span className="scanning-dot" style={{ background: ambientTelemetry.connected ? '#4ade80' : '#f87171' }}></span>
                {ambientTelemetry.connected ? 'VEML7700 ON' : 'Sensor OFF'}
              </span>
            </div>
            <p className="card-description-subtext mt-1">
              {ambientTelemetry.connected
                ? 'Brightness responds to measured ambient illuminance from the VEML7700.'
                : 'Reconnect the VEML7700 on SDA GPIO 32 and SCL GPIO 33 to resume automatic dimming.'}
            </p>
          </div>
        )}
      </div>


    </div>
  );
}
