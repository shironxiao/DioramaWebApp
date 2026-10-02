import React from 'react';
import { Music, Play, Pause } from 'lucide-react';
import './Audio.css';

export default function Audio({ appState, setAppState, showToast }) {
  const { audioPlaying, audioTrack, volume } = appState;

  const tracks = [
    { id: 1, name: 'Nature Ambient Stream', artist: 'Built-in Audio' },
    { id: 2, name: 'Gentle Piano', artist: 'Built-in Audio' },
    { id: 3, name: 'Forest Birds', artist: 'Built-in Audio' },
  ];

  const handlePlayToggle = (trackName) => {
    if (audioTrack === trackName && audioPlaying) {
      setAppState(prev => ({ ...prev, audioPlaying: false }));
      showToast('Audio paused');
    } else {
      setAppState(prev => ({ ...prev, audioPlaying: true, audioTrack: trackName }));
      showToast(`Playing ${trackName}`);
    }
  };

  return (
    <div className="page-container">
      {/* Header */}
      <div className="page-header-text">
        <div className="section-breadcrumb audio-breadcrumb">LIBRARY / AUDIO</div>
        <h1 className="page-main-title">Audio</h1>
        <p className="page-subtitle">Bring a soundtrack to your little world.</p>
      </div>

      {/* Built-in Tracks Section */}
      <div className="section-block mt-4">
        <h2 className="section-block-title">Pre-recorded Tracks</h2>
        <div className="recordings-list">
          {tracks.map((track) => {
            const isPlaying = audioTrack === track.name && audioPlaying;
            return (
              <div key={track.id} className="control-card recording-item">
                <div className="rec-info-group">
                  <div className="icon-badge purple small">
                    <Music size={18} />
                  </div>
                  <div className="title-stack">
                    <h4 className="rec-name">{track.name}</h4>
                    <span className="rec-date">{track.artist}</span>
                  </div>
                </div>

                <div className="rec-actions">
                  <button
                    className="audio-play-circle-btn purple-play"
                    onClick={() => handlePlayToggle(track.name)}
                  >
                    {isPlaying ? <Pause size={16} /> : <Play size={16} className="play-icon-offset" />}
                  </button>
                </div>
              </div>
            );
          })}
        </div>
      </div>
    </div>
  );
}
