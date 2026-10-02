import React from 'react';
import { Sparkles, Settings, Home as HomeIcon, Lightbulb, Waves, Music, Info } from 'lucide-react';

export default function Navigation({ activeTab, setActiveTab, onSettingsClick = () => {} }) {
  return (
    <>
      {/* 1. TOP NAVBAR FOR DESKTOP WEB VIEW */}
      <header className="top-navbar-desktop">
        <div className="navbar-content">
          <div className="brand-logo" onClick={() => setActiveTab('home')}>
            <div className="brand-icon-wrapper">
              <Sparkles className="brand-icon" />
            </div>
            <span className="brand-name">My Diorama</span>
          </div>

          <nav className="desktop-menu-links">
            <button
              className={`nav-item ${activeTab === 'home' ? 'active' : ''}`}
              onClick={() => setActiveTab('home')}
            >
              <HomeIcon size={18} />
              <span>Home</span>
            </button>

            <button
              className={`nav-item ${activeTab === 'lights' ? 'active' : ''}`}
              onClick={() => setActiveTab('lights')}
            >
              <Lightbulb size={18} />
              <span>Lights & Colors</span>
            </button>

            <button
              className={`nav-item ${activeTab === 'fountain' ? 'active' : ''}`}
              onClick={() => setActiveTab('fountain')}
            >
              <Waves size={18} />
              <span>Fountain</span>
            </button>

            <button
              className={`nav-item ${activeTab === 'audio' ? 'active' : ''}`}
              onClick={() => setActiveTab('audio')}
            >
              <Music size={18} />
              <span>Audio</span>
            </button>

            <button
              className={`nav-item ${activeTab === 'about' ? 'active' : ''}`}
              onClick={() => setActiveTab('about')}
            >
              <Info size={18} />
              <span>About</span>
            </button>
          </nav>

          <div className="navbar-actions">
            <div className="status-pill">
              <span className="status-dot"></span>
              <span className="status-text">Connected</span>
            </div>
            <button
              className={`icon-button settings-btn ${activeTab === 'settings' ? 'active' : ''}`}
              title="Settings & Sensors"
              onClick={onSettingsClick}
            >
              <Settings size={20} />
            </button>
          </div>
        </div>
      </header>

      {/* 2. MOBILE TOP HEADER */}
      <header className="mobile-header">
        <div className="mobile-brand" onClick={() => setActiveTab('home')}>
          <Sparkles className="mobile-sparkle" size={20} />
          <span className="mobile-title">My Diorama</span>
        </div>
        <button className="mobile-settings-btn" onClick={onSettingsClick}>
          <Settings size={20} />
        </button>
      </header>

      {/* 3. MOBILE FLOATING BOTTOM NAV BAR */}
      <div className="mobile-bottom-nav-container">
        <div className="mobile-pill-nav">
          <button
            className={`pill-nav-item ${activeTab === 'home' ? 'active' : ''}`}
            onClick={() => setActiveTab('home')}
          >
            <HomeIcon size={20} />
            <span>Home</span>
          </button>
          <button
            className={`pill-nav-item ${activeTab === 'lights' ? 'active' : ''}`}
            onClick={() => setActiveTab('lights')}
          >
            <Lightbulb size={20} />
            <span>Lights & Colors</span>
          </button>
          <button
            className={`pill-nav-item ${activeTab === 'fountain' ? 'active' : ''}`}
            onClick={() => setActiveTab('fountain')}
          >
            <Waves size={20} />
            <span>Fountain</span>
          </button>
          <button
            className={`pill-nav-item ${activeTab === 'audio' ? 'active' : ''}`}
            onClick={() => setActiveTab('audio')}
          >
            <Music size={20} />
            <span>Audio</span>
          </button>
          <button
            className={`pill-nav-item ${activeTab === 'about' ? 'active' : ''}`}
            onClick={() => setActiveTab('about')}
          >
            <Info size={20} />
            <span>About</span>
          </button>
        </div>
      </div>
    </>
  );
}
