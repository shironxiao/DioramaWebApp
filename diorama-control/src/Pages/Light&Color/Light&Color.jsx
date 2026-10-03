import React, { useState } from 'react';
import {
  Lightbulb,
  Mic,
  Palette,
  Sparkles,
  Loader2,
  Sun,
  ChevronDown,
  ChevronUp,
  Check
} from 'lucide-react';
import DioramaCanvas from '../../components/DioramaCanvas';
import ColorPicker from '../../components/ColorPicker';
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
  const [isScanning, setIsScanning] = useState(false);
  const [isColorsOpen, setIsColorsOpen] = useState(false);

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

  // Scan Color Sensor Handler
  const handleScanColor = () => {
    if (isScanning) return;
    setIsScanning(true);
    showToast('🔍 TCS3200 Color Sensor scanning target object...');

    setTimeout(() => {
      const colors = ['#D4B78C', '#4A8C5F', '#3B9DB3', '#E06D53', '#8E67B8', '#E6A14A', '#00A896'];
      const scannedHex = colors[Math.floor(Math.random() * colors.length)];
      setCurrentColor(scannedHex);
      setIsScanning(false);
      showToast(`✨ Sensor scanned ${scannedHex}! Choose target below to apply.`);
    }, 1500);
  };

  // Apply Color to Target Handler: 'all', 'left_fountain', 'right_fountain', 'center'
  const applyColorToTarget = (target) => {
    setAppState((prev) => {
      const next = { ...prev };
      if (target === 'all') {
        next.circleColor = currentColor;
        next.fountainColor = currentColor;
        next.fountainAuxColor = currentColor;
        showToast(`✨ Applied ${currentColor.toUpperCase()} to ALL (Center + Left & Right Fountains)!`);
      } else if (target === 'left_fountain') {
        next.fountainColor = currentColor;
        showToast(`✨ Applied ${currentColor.toUpperCase()} to Left Fountain!`);
      } else if (target === 'right_fountain') {
        next.fountainAuxColor = currentColor;
        showToast(`✨ Applied ${currentColor.toUpperCase()} to Right Fountain!`);
      } else if (target === 'center') {
        next.circleColor = currentColor;
        showToast(`✨ Applied ${currentColor.toUpperCase()} to the Center!`);
      }
      return next;
    });
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
          <div className={`control-card mt-2 ${isScanning ? 'card-scanning' : ''} ${isColorsOpen ? 'expanded' : 'collapsed'} color-collapsible-card`}>
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
                    <h3 className="card-title">Color Control & Detection</h3>
                    <span
                      className="color-mini-swatch"
                      style={{ backgroundColor: currentColor }}
                      title={currentColor}
                    />
                    <span className="hex-mini-tag">{(currentColor || '').toUpperCase()}</span>
                  </div>
                  <span className="card-status-subtext">
                    {isColorsOpen ? (isScanning ? 'TCS3200 scanning...' : 'Pick a color or scan with sensor') : 'Tap to open color tools'}
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
                  <button
                    className={`btn-amber-sensor ${isScanning ? 'is-scanning' : ''}`}
                    onClick={(e) => { e.stopPropagation(); handleScanColor(); }}
                    disabled={isScanning}
                    title="Detect color with TCS3200 sensor"
                  >
                    {isScanning ? (
                      <><Loader2 size={16} className="spin-icon" /><span>Scanning Color...</span></>
                    ) : (
                      <><Sparkles size={16} /><span>Scan Color Sensor</span></>
                    )}
                  </button>
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
                  <span className="card-status-subtext" style={{ color: '#4ade80' }}>● Active — auto-adjusting brightness</span>
                </div>
              </div>
              <span className="sensor-scanning-pill" style={{ background: 'rgba(74,222,128,0.15)', color: '#4ade80', border: '1px solid rgba(74,222,128,0.3)' }}>
                <span className="scanning-dot" style={{ background: '#4ade80' }}></span> Sensor ON
              </span>
            </div>
            <p className="card-description-subtext mt-1">
              Following room brightness automatically via ambient sensor. No manual control needed.
            </p>
          </div>
        )}
      </div>


    </div>
  );
}
