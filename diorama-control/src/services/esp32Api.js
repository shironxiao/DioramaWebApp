// ESP32 Hardware Communication Service

let esp32Ip = localStorage.getItem('esp32_ip') || '192.168.4.1';

export const getEsp32Ip = () => esp32Ip;

export const setEsp32Ip = (ip) => {
  esp32Ip = ip.replace(/^https?:\/\//, '').replace(/\/.*$/, '');
  localStorage.setItem('esp32_ip', esp32Ip);
};

// Returns true only when the page is actually served FROM the ESP32 itself.
// Opening the Vite dev server on a LAN IP must NOT count as self-hosted.
const isSelfHosted = () => window.location.hostname === esp32Ip;

// Build a full URL to the ESP32 — always absolute unless truly self-hosted.
const espUrl = (path) =>
  isSelfHosted() ? path : `http://${esp32Ip}${path}`;

// Generic GET command sender
const sendEspCommand = async (endpoint, params = {}) => {
  const qs = new URLSearchParams(params).toString();
  const url = espUrl(endpoint) + (qs ? `?${qs}` : '');
  try {
    const ctrl = new AbortController();
    const tid  = setTimeout(() => ctrl.abort(), 3000);
    const res  = await fetch(url, { method: 'GET', signal: ctrl.signal, mode: 'cors' });
    clearTimeout(tid);
    if (!res.ok) { console.warn(`ESP32 ${endpoint} → ${res.status}`); return false; }
    return true;
  } catch (e) {
    console.warn(`ESP32 command failed (${url}):`, e.message);
    return false;
  }
};

// Generic GET that returns parsed JSON (or null on failure)
const fetchEsp = async (endpoint, timeoutMs = 2500) => {
  const url = espUrl(endpoint);
  try {
    const ctrl = new AbortController();
    const tid  = setTimeout(() => ctrl.abort(), timeoutMs);
    const res  = await fetch(url, { signal: ctrl.signal, mode: 'cors' });
    clearTimeout(tid);
    if (!res.ok) return null;
    return await res.json();
  } catch {
    return null;
  }
};

// ── Hardware command wrappers ─────────────────────────────────────────────────

export const sendLightControl = (isOn, brightness = 75) =>
  sendEspCommand('/api/light', { state: isOn ? 'on' : 'off', brightness });

export const sendFountainControl = (isOn, leftStrength = 100, rightStrength = 75) =>
  sendEspCommand('/api/fountain', {
    state: isOn ? 'on' : 'off',
    strength: leftStrength,
    auxStrength: rightStrength,
  });

export const sendColorControl = (hexColor, target = 'all') => {
  const h = hexColor.replace('#', '');
  const r = parseInt(h.substring(0, 2), 16) || 0;
  const g = parseInt(h.substring(2, 4), 16) || 0;
  const b = parseInt(h.substring(4, 6), 16) || 0;
  return sendEspCommand('/api/color', { r, g, b, target });
};

export const sendGateControl = (isOpen) =>
  sendEspCommand('/api/gate', { state: isOpen ? 'open' : 'closed' });

export const sendModeControl = (mode) =>
  sendEspCommand('/api/mode', { mode });

export const sendVolumeControl = (volume) =>
  sendEspCommand('/api/audio/volume', { volume });

export const sendSoundReactive = (isOn, intensity = 65) =>
  sendEspCommand('/api/sound-reactive', { state: isOn ? 'on' : 'off', intensity });

export const sendForceSensorControl = (isOn) =>
  sendEspCommand('/api/force-sensor', { state: isOn ? 'on' : 'off' });

export const sendControlSource = (source) =>
  sendEspCommand('/api/control-source', { source });

// ── State polling ─────────────────────────────────────────────────────────────

/**
 * Poll full diorama state from ESP32 (used by App.jsx every 3 s).
 * Returns parsed JSON or null.
 */
export const getEsp32State = () => fetchEsp('/api/state', 2500);

export const getGateStatus = async () => {
  const data = await fetchEsp('/api/gate/status', 2000);
  return data && typeof data.open === 'boolean' ? data.open : null;
};

// ── Color sensor ──────────────────────────────────────────────────────────────

export const scanColorSensor = async () => {
  const data = await fetchEsp('/api/color-sensor', 4000);
  if (!data || data.r == null) return null;
  const hex = (v) => Math.max(0, Math.min(255, Number(v))).toString(16).padStart(2, '0');
  return `#${hex(data.r)}${hex(data.g)}${hex(data.b)}`;
};

// ── Audio ─────────────────────────────────────────────────────────────────────

export const getAudioFiles = async () => {
  const data = await fetchEsp('/api/audio/files', 3000);
  return Array.isArray(data?.files) ? data.files : [];
};

export const sendAudioPlay  = (filename) => sendEspCommand('/api/audio/play',  { file: filename });
export const sendAudioPause = ()          => sendEspCommand('/api/audio/pause');
export const sendAudioStop  = ()          => sendEspCommand('/api/audio/stop');

// ── Sensor reads ──────────────────────────────────────────────────────────────

export const getSoundLevel = async () => {
  const data = await fetchEsp('/api/sound', 2000);
  if (!data) return { detected: false, level: 0 };
  return {
    detected: !!data.detected,
    level: Math.min(1023, Math.max(0, Number(data.level) || 0)),
  };
};

export const getForceLevel = async () => {
  const data = await fetchEsp('/api/force', 2000);
  if (!data) return null;
  return {
    active: !!data.active,
    level: Math.min(1023, Math.max(0, Number(data.level ?? data.force) || 0)),
  };
};

export const getLiveSensors = () => fetchEsp('/api/sensors', 2500);
