// ESP32 Hardware Communication Service

let esp32Ip = localStorage.getItem('esp32_ip') || '192.168.100.138';

export const getEsp32Ip = () => esp32Ip;

export const setEsp32Ip = (ip) => {
  esp32Ip = ip.replace(/^https?:\/\//, '').replace(/\/.*$/, '');
  localStorage.setItem('esp32_ip', esp32Ip);
};

// Generic fetch sender with fallback handling
const sendEspCommand = async (endpoint, params = {}) => {
  const queryString = new URLSearchParams(params).toString();
  // If app is served directly from ESP32, use relative URL; otherwise connect to esp32Ip
  const isSelfHosted = window.location.hostname === esp32Ip || (window.location.hostname !== 'localhost' && window.location.hostname !== '127.0.0.1');
  const url = isSelfHosted 
    ? `${endpoint}${queryString ? '?' + queryString : ''}`
    : `http://${esp32Ip}${endpoint}${queryString ? '?' + queryString : ''}`;

  try {
    const controller = new AbortController();
    const timeoutId = setTimeout(() => controller.abort(), 3000);

    const response = await fetch(url, {
      method: 'GET',
      signal: controller.signal,
      mode: 'cors'
    });
    clearTimeout(timeoutId);

    if (!response.ok) {
      console.warn(`ESP32 responded with status ${response.status}`);
      return false;
    }
    return true;
  } catch (error) {
    console.warn(`ESP32 reachability check failed (${url}):`, error.message);
    return false;
  }
};

// Hardware command wrappers
export const sendLightControl = (isOn, brightness = 75) => {
  return sendEspCommand('/api/light', {
    state: isOn ? 'on' : 'off',
    brightness: brightness
  });
};

export const sendFountainControl = (isOn, leftStrength = 100, rightStrength = 75, pattern = 'Pulsing') => {
  return sendEspCommand('/api/fountain', {
    state: isOn ? 'on' : 'off',
    strength: leftStrength,
    auxStrength: rightStrength,
    pattern: pattern
  });
};

export const sendColorControl = (hexColor) => {
  // Convert hex #RRGGBB to R, G, B ints
  const cleanHex = hexColor.replace('#', '');
  const r = parseInt(cleanHex.substring(0, 2), 16) || 0;
  const g = parseInt(cleanHex.substring(2, 4), 16) || 0;
  const b = parseInt(cleanHex.substring(4, 6), 16) || 0;

  return sendEspCommand('/api/color', { r, g, b });
};

export const sendGateControl = (isOpen) => {
  return sendEspCommand('/api/gate', {
    state: isOpen ? 'open' : 'closed'
  });
};

export const sendModeControl = (mode) => {
  return sendEspCommand('/api/mode', {
    mode: mode
  });
};

export const sendVolumeControl = (volume) => {
  return sendEspCommand('/api/audio/volume', { volume });
};

export const sendSoundReactive = (isOn, intensity = 65) => {
  return sendEspCommand('/api/sound-reactive', {
    state: isOn ? 'on' : 'off',
    intensity
  });
};

export const sendForceSensorControl = (isOn) => {
  return sendEspCommand('/api/force-sensor', {
    state: isOn ? 'on' : 'off'
  });
};

export const sendControlSource = (source) => {
  return sendEspCommand('/api/control-source', { source });
};

export const scanColorSensor = async () => {
  const isSelfHosted =
    window.location.hostname === esp32Ip ||
    (window.location.hostname !== 'localhost' && window.location.hostname !== '127.0.0.1');
  const url = isSelfHosted ? '/api/color-sensor' : `http://${esp32Ip}/api/color-sensor`;
  try {
    const controller = new AbortController();
    const timeoutId = setTimeout(() => controller.abort(), 4000);
    const response = await fetch(url, { signal: controller.signal, mode: 'cors' });
    clearTimeout(timeoutId);
    if (!response.ok) return null;
    const data = await response.json();
    // Expect ESP32: { "r": 180, "g": 120, "b": 60 }
    if (data.r == null) return null;
    const toHex = (v) => Math.max(0, Math.min(255, Number(v))).toString(16).padStart(2, '0');
    return `#${toHex(data.r)}${toHex(data.g)}${toHex(data.b)}`;
  } catch {
    return null;
  }
};

// ── Audio / SD-card commands ──────────────────────────────────────────────────

/**
 * Fetch the list of audio files stored on the SD card.
 * Expects ESP32 to return: { "files": ["001.mp3", "002.mp3", ...] }
 */
export const getAudioFiles = async () => {
  const isSelfHosted =
    window.location.hostname === esp32Ip ||
    (window.location.hostname !== 'localhost' && window.location.hostname !== '127.0.0.1');
  const url = isSelfHosted ? '/api/audio/files' : `http://${esp32Ip}/api/audio/files`;
  try {
    const controller = new AbortController();
    const timeoutId = setTimeout(() => controller.abort(), 3000);
    const response = await fetch(url, { signal: controller.signal, mode: 'cors' });
    clearTimeout(timeoutId);
    if (!response.ok) return [];
    const data = await response.json();
    return Array.isArray(data.files) ? data.files : [];
  } catch {
    return [];
  }
};

/**
 * Tell the ESP32 to play a specific track by filename.
 * Sends: GET /api/audio/play?file=<filename>
 */
export const sendAudioPlay = (filename) =>
  sendEspCommand('/api/audio/play', { file: filename });

/**
 * Tell the ESP32 to pause playback.
 */
export const sendAudioPause = () => sendEspCommand('/api/audio/pause');

/**
 * Tell the ESP32 to stop playback.
 */
export const sendAudioStop = () => sendEspCommand('/api/audio/stop');

// ── Sensor reads ──────────────────────────────────────────────────────────────

// Read sound sensor from ESP32: returns { detected: bool, level: 0-1023 }
export const getSoundLevel = async () => {
  const isSelfHosted = window.location.hostname === esp32Ip || (window.location.hostname !== 'localhost' && window.location.hostname !== '127.0.0.1');
  const url = isSelfHosted ? '/api/sound' : `http://${esp32Ip}/api/sound`;
  try {
    const controller = new AbortController();
    const timeoutId = setTimeout(() => controller.abort(), 2000);
    const response = await fetch(url, { signal: controller.signal, mode: 'cors' });
    clearTimeout(timeoutId);
    if (!response.ok) return { detected: false, level: 0 };
    const data = await response.json();
    // Expect ESP32 to return: { "detected": true/false, "level": 0-1023 }
    return {
      detected: !!data.detected,
      level: Math.min(1023, Math.max(0, Number(data.level) || 0))
    };
  } catch {
    return { detected: false, level: 0 };
  }
};

// Read force sensor from ESP32: returns { active: bool, level: 0-1023 }
export const getForceLevel = async () => {
  const isSelfHosted = window.location.hostname === esp32Ip || (window.location.hostname !== 'localhost' && window.location.hostname !== '127.0.0.1');
  const url = isSelfHosted ? '/api/force' : `http://${esp32Ip}/api/force`;
  try {
    const controller = new AbortController();
    const timeoutId = setTimeout(() => controller.abort(), 2000);
    const response = await fetch(url, { signal: controller.signal, mode: 'cors' });
    clearTimeout(timeoutId);
    if (!response.ok) return null;
    const data = await response.json();
    return {
      active: !!data.active,
      level: Math.min(1023, Math.max(0, Number(data.level ?? data.force) || 0))
    };
  } catch {
    return null;
  }
};
