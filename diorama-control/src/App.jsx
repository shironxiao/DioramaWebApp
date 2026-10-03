import React, { useState } from 'react';
import Navigation from './components/Navigation';
import Home from './Pages/Home/Home';
import LightAndColor from './Pages/Light&Color/Light&Color';
import Fountain from './Pages/Fountain/Fountain';
import Audio from './Pages/Audio/Audio';
import SettingsPage from './Pages/Settings/Settings';
import LoginRegister from './Pages/Login&Register/LoginRegister';
import AboutPage from './Pages/About/About';
import {
  sendLightControl,
  sendFountainControl,
  sendColorControl,
  sendGateControl,
  sendModeControl
} from './services/esp32Api';
import './App.css';

function App() {
  const [activeTab, setActiveTab] = useState('home');
  const [toastMessage, setToastMessage] = useState(null);

  // Persistent login — read saved session from localStorage
  const [isLoggedIn, setIsLoggedIn] = useState(() => {
    return localStorage.getItem('diorama_logged_in') === 'true';
  });
  const [user, setUser] = useState(() => {
    const saved = localStorage.getItem('diorama_user');
    return saved ? JSON.parse(saved) : { name: '' };
  });

  // Shared application state
  const [appState, setAppState] = useState({
    lightsOn: true,
    brightness: 75,
    lightingMode: 'Color Adaptive',
    soundReactiveOn: true,
    micSensitivity: 50,
    reactionIntensity: 65,
    autoDimming: true,
    simulatedLight: 72,

    fountainOn: true,
    fountainStrength: 100,
    fountainAuxStrength: 75,
    fountainForceSensorOn: false,

    fountainColor: '#77898D',
    fountainAuxColor: '#3B9DB3',

    audioPlaying: false,
    audioTrack: 'No audio selected',
    volume: 70,

    circleColor: '#D4B78C',
    colorSensorTarget: 'All', // 'All', 'F1', 'F2', 'Center'
    rfidGateEnabled: true,
    gateOpen: false,
    plazaRotationMode: 'Sensor'
  });

  // Wrapper for updating state and syncing with physical ESP32
  const updateAppState = (updater) => {
    setAppState((prev) => {
      const next = typeof updater === 'function' ? updater(prev) : { ...prev, ...updater };
      
      if (next.lightsOn !== prev.lightsOn || next.brightness !== prev.brightness) {
        sendLightControl(next.lightsOn, next.brightness);
      }
      if (
        next.fountainOn !== prev.fountainOn ||
        next.fountainStrength !== prev.fountainStrength ||
        next.fountainAuxStrength !== prev.fountainAuxStrength
      ) {
        sendFountainControl(next.fountainOn, next.fountainStrength, next.fountainAuxStrength);
      }
      if (next.fountainColor !== prev.fountainColor) {
        sendColorControl(next.fountainColor);
      }
      if (next.gateOpen !== prev.gateOpen) {
        sendGateControl(next.gateOpen);
      }
      if (next.lightingMode !== prev.lightingMode) {
        sendModeControl(next.lightingMode);
      }

      return next;
    });
  };

  const showToast = (msg) => {
    setToastMessage(msg);
    setTimeout(() => setToastMessage(null), 3000);
  };

  const handleLogout = () => {
    localStorage.removeItem('diorama_logged_in');
    localStorage.removeItem('diorama_user');
    setIsLoggedIn(false);
    setUser({ name: '' });
    showToast('Logged out of Silvestre del Moro Park');
  };

  const handleLoginSuccess = (userName) => {
    localStorage.setItem('diorama_logged_in', 'true');
    localStorage.setItem('diorama_user', JSON.stringify({ name: userName }));
    setUser({ name: userName });
    setIsLoggedIn(true);
    setActiveTab('home');
  };

  if (!isLoggedIn) {
    return (
      <>
        {toastMessage && (
          <div className="toast-banner fixed-toast">
            <span>{toastMessage}</span>
          </div>
        )}
        <LoginRegister onLoginSuccess={handleLoginSuccess} showToast={showToast} />
      </>
    );
  }

  return (
    <div className="diorama-app-container">
      {/* Navbar Header & Mobile Navigation */}
      <Navigation
        activeTab={activeTab}
        setActiveTab={(tab) => {
          setActiveTab(tab);
          window.scrollTo({ top: 0, behavior: 'smooth' });
        }}
        onSettingsClick={() => {
          setActiveTab('settings');
          window.scrollTo({ top: 0, behavior: 'smooth' });
        }}
      />

      {/* Main Viewport */}
      <main className="main-viewport">
        <div className="content-max-width">
          {toastMessage && (
            <div className="toast-banner">
              <span>{toastMessage}</span>
            </div>
          )}

          {activeTab === 'home' && (
            <Home
              appState={appState}
              setAppState={updateAppState}
              setActiveTab={setActiveTab}
              showToast={showToast}
            />
          )}

          {activeTab === 'lights' && (
            <LightAndColor
              appState={appState}
              setAppState={updateAppState}
              showToast={showToast}
            />
          )}

          {activeTab === 'fountain' && (
            <Fountain
              appState={appState}
              setAppState={updateAppState}
              showToast={showToast}
            />
          )}

          {activeTab === 'audio' && (
            <Audio
              appState={appState}
              setAppState={updateAppState}
              showToast={showToast}
            />
          )}

          {activeTab === 'settings' && (
            <SettingsPage
              appState={appState}
              setAppState={updateAppState}
              showToast={showToast}
              onLogout={handleLogout}
            />
          )}

          {activeTab === 'about' && (
            <AboutPage setActiveTab={setActiveTab} />
          )}
        </div>
      </main>
    </div>
  );
}

export default App;
