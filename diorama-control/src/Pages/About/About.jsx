import React from 'react';
import { MapPin, Landmark, Cpu, Droplets, Lightbulb, Music, KeyRound, RotateCcw } from 'lucide-react';
import './About.css';

const features = [
  {
    icon: <Lightbulb size={20} />,
    title: 'Adaptive Lighting',
    desc: 'RGB LED lights with multiple modes — basic, colorful, sound-reactive, and ambient-adaptive via color sensor.'
  },
  {
    icon: <Droplets size={20} />,
    title: 'Dual Water Fountain',
    desc: 'Main and auxiliary spouts with adjustable strength. Force sensor support for pressure-reactive control.'
  },
  {
    icon: <RotateCcw size={20} />,
    title: 'Rotating Inner Plaza',
    desc: 'The central colonnade plaza can rotate continuously, on demand, or only when the mic detects sound.'
  },
  {
    icon: <Music size={20} />,
    title: 'Ambient Audio',
    desc: 'Pre-recorded soundscapes play directly into the diorama, bringing the miniature world to life.'
  },
  {
    icon: <KeyRound size={20} />,
    title: 'RFID Gate Access',
    desc: 'The front gate opens and closes via RFID reader, simulating real park entry control.'
  },
  {
    icon: <Cpu size={20} />,
    title: 'ESP32 Sensor Hub',
    desc: 'Powered by an ESP32 microcontroller integrating RGB, electret mic, ambient light, color, and force sensors.'
  }
];

export default function AboutPage({ setActiveTab }) {
  return (
    <div className="page-container about-page">

      {/* Hero */}
      <div className="about-hero">
        <div className="about-hero-badge">ABOUT THIS DIORAMA</div>
        <h1 className="about-title">Silvestre del Moro Park</h1>
        <div className="about-location-row">
          <MapPin size={16} className="about-pin-icon" />
          <span>Sta. Elena, Camarines Norte, Philippines</span>
        </div>
        <p className="about-tagline">
          A miniature park brought to life — lights, water, sound, and sensors working in harmony
          to recreate the iconic Silvestre del Moro Park in stunning detail.
        </p>
      </div>

      {/* Description Card */}
      <div className="control-card about-desc-card">
        <div className="card-header">
          <div className="card-title-group">
            <div className="icon-badge sage">
              <Landmark size={22} />
            </div>
            <div className="title-stack">
              <h3 className="card-title">About the Park</h3>
              <span className="card-status-subtext">Sta. Elena, Camarines Norte</span>
            </div>
          </div>
        </div>
        <p className="about-body-text">
          Silvestre del Moro Park is a beloved public landmark in Sta. Elena, Camarines Norte, Philippines.
          Named in honor of a distinguished local figure, the park serves as a gathering place for the
          community — featuring lush greenery, a central water fountain, a decorated walking plaza, and
          ornamental archway gates that welcome visitors.
        </p>
        <p className="about-body-text mt-3">
          This diorama project faithfully recreates the park in miniature scale, complete with working
          LED lighting, real water flow, ambient audio, and sensor-driven automation — controlled entirely
          through this web application and an onboard ESP32 interface.
        </p>
      </div>

      {/* Features Grid */}
      <div className="section-block">
        <h2 className="section-block-title">Diorama Features</h2>
        <div className="about-features-grid">
          {features.map((f, i) => (
            <div key={i} className="about-feature-card">
              <div className="about-feature-icon">{f.icon}</div>
              <div>
                <h4 className="about-feature-title">{f.title}</h4>
                <p className="about-feature-desc">{f.desc}</p>
              </div>
            </div>
          ))}
        </div>
      </div>

      {/* Tech Stack */}
      <div className="control-card">
        <div className="card-header">
          <div className="card-title-group">
            <div className="icon-badge sage">
              <Cpu size={22} />
            </div>
            <h3 className="card-title">Hardware & Technology</h3>
          </div>
        </div>
        <div className="about-tech-grid">
          {[
            ['Microcontroller', 'ESP32'],
            ['Lighting', 'RGB LED Modules'],
            ['Sound', 'Electret Microphone Sensor'],
            ['Ambient Light', 'LDR / Ambient Light Sensor'],
            ['Color Detection', 'TCS3200 Color Sensor'],
            ['Water Pump', 'Submersible DC Pump'],
            ['Pressure', 'Force Sensor (FSR)'],
            ['Gate Control', 'RFID RC522 Reader'],
            ['Control App', 'React + Vite (Web)'],
          ].map(([label, value]) => (
            <div key={label} className="about-tech-row">
              <span className="about-tech-label">{label}</span>
              <span className="about-tech-value">{value}</span>
            </div>
          ))}
        </div>
      </div>

      {/* Footer CTA */}
      <div className="about-footer-cta">
        <p>Explore and control your little world.</p>
        <button className="btn-about-cta" onClick={() => setActiveTab('home')}>
          Go to Dashboard
        </button>
      </div>

    </div>
  );
}
