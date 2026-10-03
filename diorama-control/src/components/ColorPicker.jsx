import React, { useCallback } from 'react';
import './ColorPicker.css';

// ── Helpers ──────────────────────────────────────────────────────────────────
function hexToRgb(hex) {
  const h = hex.replace('#', '');
  const bigint = parseInt(h.length === 3
    ? h.split('').map(c => c + c).join('')
    : h, 16);
  return { r: (bigint >> 16) & 255, g: (bigint >> 8) & 255, b: bigint & 255 };
}

function rgbToHex(r, g, b) {
  const c = v => Math.max(0, Math.min(255, v)).toString(16).padStart(2, '0');
  return `#${c(r)}${c(g)}${c(b)}`;
}

function rgbToHsl(r, g, b) {
  r /= 255; g /= 255; b /= 255;
  const max = Math.max(r, g, b), min = Math.min(r, g, b);
  let h, s, l = (max + min) / 2;
  if (max === min) { h = s = 0; }
  else {
    const d = max - min;
    s = l > 0.5 ? d / (2 - max - min) : d / (max + min);
    switch (max) {
      case r: h = ((g - b) / d + (g < b ? 6 : 0)) / 6; break;
      case g: h = ((b - r) / d + 2) / 6; break;
      default: h = ((r - g) / d + 4) / 6;
    }
  }
  return { h: Math.round(h * 360), s: Math.round(s * 100), l: Math.round(l * 100) };
}

function hslToRgb(h, s, l) {
  h /= 360; s /= 100; l /= 100;
  let r, g, b;
  if (s === 0) { r = g = b = l; }
  else {
    const hue2rgb = (p, q, t) => {
      if (t < 0) t += 1;
      if (t > 1) t -= 1;
      if (t < 1/6) return p + (q - p) * 6 * t;
      if (t < 1/2) return q;
      if (t < 2/3) return p + (q - p) * (2/3 - t) * 6;
      return p;
    };
    const q = l < 0.5 ? l * (1 + s) : l + s - l * s;
    const p = 2 * l - q;
    r = hue2rgb(p, q, h + 1/3);
    g = hue2rgb(p, q, h);
    b = hue2rgb(p, q, h - 1/3);
  }
  return { r: Math.round(r * 255), g: Math.round(g * 255), b: Math.round(b * 255) };
}

// ── Preset palette ────────────────────────────────────────────────────────────
const PRESETS = [
  '#FF4444', '#FF8C00', '#FFD700', '#7CFC00',
  '#00BFFF', '#8A2BE2', '#FF69B4', '#FFFFFF',
  '#00CED1', '#FF6347', '#4169E1', '#32CD32',
];

// ── Component ─────────────────────────────────────────────────────────────────
export default function ColorPicker({ rgb, onChange, accentClass = 'amber' }) {
  const { r, g, b } = rgb;
  const hex = rgbToHex(r, g, b);
  const hsl = rgbToHsl(r, g, b);

  // Native color wheel changed
  const onNativeChange = useCallback((e) => {
    onChange(hexToRgb(e.target.value));
  }, [onChange]);

  // Hue slider changed
  const onHueChange = useCallback((e) => {
    const newHsl = { ...hsl, h: Number(e.target.value) };
    onChange(hslToRgb(newHsl.h, newHsl.s, newHsl.l));
  }, [hsl, onChange]);

  // Saturation slider
  const onSatChange = useCallback((e) => {
    onChange(hslToRgb(hsl.h, Number(e.target.value), hsl.l));
  }, [hsl, onChange]);

  // RGB number inputs
  const onRgbChange = (channel, val) => {
    onChange({ r, g, b, [channel]: Math.max(0, Math.min(255, Number(val))) });
  };

  // Preset click
  const onPreset = (presetHex) => onChange(hexToRgb(presetHex));

  return (
    <div className="cpk-root">
      {/* Top row: native color wheel + swatch */}
      <div className="cpk-top-row">
        <div className="cpk-wheel-wrap">
          <input
            type="color"
            className="cpk-native-wheel"
            value={hex}
            onChange={onNativeChange}
            title="Open color wheel"
          />
          <div className="cpk-wheel-overlay" style={{ background: hex }} />
        </div>
        <div className="cpk-top-info">
          <span className="cpk-hex-label">HEX</span>
          <span className="cpk-hex-value">{hex.toUpperCase()}</span>
          <span className="cpk-rgb-badge">RGB {r}, {g}, {b}</span>
        </div>
      </div>

      {/* Hue slider */}
      <div className="cpk-slider-block">
        <div className="cpk-slider-header">
          <span className="cpk-slider-label">Hue</span>
          <span className="cpk-slider-val">{hsl.h}°</span>
        </div>
        <div className="cpk-hue-track">
          <input
            type="range"
            min="0"
            max="360"
            value={hsl.h}
            onChange={onHueChange}
            className="cpk-range cpk-hue-range"
          />
        </div>
      </div>

      {/* Saturation slider */}
      <div className="cpk-slider-block">
        <div className="cpk-slider-header">
          <span className="cpk-slider-label">Saturation</span>
          <span className="cpk-slider-val">{hsl.s}%</span>
        </div>
        <div className="cpk-sat-track" style={{ '--hue': hsl.h }}>
          <input
            type="range"
            min="0"
            max="100"
            value={hsl.s}
            onChange={onSatChange}
            className="cpk-range cpk-sat-range"
          />
        </div>
      </div>

      {/* RGB number fields */}
      <div className="cpk-rgb-row">
        {[['R', 'r', '#FF4444'], ['G', 'g', '#44CC44'], ['B', 'b', '#4488FF']].map(([label, ch, col]) => (
          <div key={ch} className="cpk-rgb-field">
            <span className="cpk-rgb-label" style={{ color: col }}>{label}</span>
            <input
              type="number"
              min="0"
              max="255"
              value={rgb[ch]}
              onChange={e => onRgbChange(ch, e.target.value)}
              className="cpk-rgb-input"
            />
          </div>
        ))}
      </div>

      {/* Preset swatches */}
      <div className="cpk-presets">
        {PRESETS.map(p => (
          <button
            key={p}
            className={`cpk-preset-dot ${p === hex.toUpperCase() || p.toLowerCase() === hex ? 'active' : ''}`}
            style={{ background: p }}
            title={p}
            onClick={() => onPreset(p)}
          />
        ))}
      </div>
    </div>
  );
}
