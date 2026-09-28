import React, { useState } from 'react';
import Navigation from './components/Navigation';
import Home from './Pages/Home/Home';
import LightAndColor from './Pages/Light&Color/Light&Color';
import Fountain from './Pages/Fountain/Fountain';
import Audio from './Pages/Audio/Audio';
import SettingsPage from './Pages/Settings/Settings';
import LoginRegister from './Pages/Login&Register/LoginRegister';
import './App.css';

function App() {
  const [activeTab, setActiveTab] = useState('home');
  const [isLoggedIn, setIsLoggedIn] = useState(true);
  const [user, setUser] = useState({ name: 'Ronald Sevilla' });
  const [toastMessage, setToastMessage] = useState(null);

  // Shared application state
  const [appState, setAppState] = useState({
    lightsOn: true,
    brightness: 75,
    lightingMode: 'Sound Reactive',
    soundReactiveOn: true,
    micSensitivity: 50,
    reactionIntensity: 65,
    autoDimming: true,
    simulatedLight: 72,

    fountainOn: true,
    fountainStrength: 100,
    fountainPattern: 'Pulsing',
    fountainColor: '#77898D',

    audioPlaying: false,
    audioTrack: 'No audio selected',
    volume: 70,

    circleColor: '#D4B78C'
  });

  const showToast = (msg) => {
    setToastMessage(msg);
    setTimeout(() => setToastMessage(null), 3000);
  };

  const handleLogout = () => {
    setIsLoggedIn(false);
    showToast('Logged out of My Diorama');
  };

  const handleLoginSuccess = (userName) => {
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
              setAppState={setAppState}
              setActiveTab={setActiveTab}
              showToast={showToast}
            />
          )}

          {activeTab === 'lights' && (
            <LightAndColor
              appState={appState}
              setAppState={setAppState}
              showToast={showToast}
            />
          )}

          {activeTab === 'fountain' && (
            <Fountain
              appState={appState}
              setAppState={setAppState}
              showToast={showToast}
            />
          )}

          {activeTab === 'audio' && (
            <Audio
              appState={appState}
              setAppState={setAppState}
              showToast={showToast}
            />
          )}

          {activeTab === 'settings' && (
            <SettingsPage
              appState={appState}
              setAppState={setAppState}
              showToast={showToast}
              onLogout={handleLogout}
            />
          )}
        </div>
      </main>
    </div>
  );
}

export default App;
