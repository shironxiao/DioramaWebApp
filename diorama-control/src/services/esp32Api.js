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

export const sendFountainControl = (isOn, strength = 100, pattern = 'Pulsing') => {
  return sendEspCommand('/api/fountain', {
    state: isOn ? 'on' : 'off',
    strength: strength,
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
