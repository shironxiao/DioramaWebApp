import React, { useState } from 'react';
import { Upload, Mic, Music, Play, Pause, Trash2 } from 'lucide-react';
import './Audio.css';

export default function Audio({ appState, setAppState, showToast }) {
  const [recordings, setRecordings] = useState([]);
  const [isRecording, setIsRecording] = useState(false);
  const [playingId, setPlayingId] = useState(null);

  const handleUploadAudio = (e) => {
    const file = e.target.files && e.target.files[0];
    if (file) {
      const newRec = {
        id: Date.now(),
        name: file.name,
        date: new Date().toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' }),
        url: URL.createObjectURL(file)
      };
      setRecordings((prev) => [newRec, ...prev]);
      showToast(`Uploaded sound track: ${file.name}`);
    }
  };

  const toggleRecord = () => {
    if (!isRecording) {
      setIsRecording(true);
      showToast('Recording started... Speak or play music');
      setTimeout(() => {
        setIsRecording(false);
        const newRec = {
          id: Date.now(),
          name: `Voice Memo ${recordings.length + 1}`,
          date: new Date().toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' }),
          url: '#'
        };
        setRecordings((prev) => [newRec, ...prev]);
        showToast('Recorded new audio memo!');
      }, 3500);
    } else {
      setIsRecording(false);
    }
  };

  const deleteRecording = (id) => {
    setRecordings((prev) => prev.filter((r) => r.id !== id));
    showToast('Deleted audio recording');
  };

  return (
    <div className="page-container">
      {/* Header */}
      <div className="page-header-text">
        <div className="section-breadcrumb audio-breadcrumb">LIBRARY / AUDIO</div>
        <h1 className="page-main-title">Audio</h1>
        <p className="page-subtitle">Bring a soundtrack to your little world.</p>
      </div>

      {/* Card 1: Add Audio */}
      <div className="control-card">
        <div className="card-header">
          <div className="card-title-group">
            <div className="icon-badge purple">
              <Upload size={22} />
            </div>
            <h3 className="card-title">Add audio</h3>
          </div>
        </div>

        <div className="audio-actions-row">
          <label className="btn-audio-upload">
            <Upload size={18} />
            <span>Upload audio</span>
            <input type="file" accept="audio/*" className="hidden-file-input" onChange={handleUploadAudio} />
          </label>

          <button className={`btn-audio-record ${isRecording ? 'recording' : ''}`} onClick={toggleRecord}>
            <Mic size={18} />
            <span>{isRecording ? 'Recording...' : 'Record audio'}</span>
          </button>
        </div>
      </div>

      {/* Section 2: Your recordings */}
      <div className="section-block mt-4">
        <h2 className="section-block-title">Your recordings</h2>

        {recordings.length === 0 ? (
          /* Empty state matching screenshot */
          <div className="control-card empty-audio-card">
            <div className="card-title-group">
              <div className="icon-badge purple">
                <Music size={22} />
              </div>
              <h3 className="card-title">Nothing here yet</h3>
            </div>
            <p className="empty-subtext">Upload or record a sound to get started.</p>
          </div>
        ) : (
          <div className="recordings-list">
            {recordings.map((rec) => (
              <div key={rec.id} className="control-card recording-item">
                <div className="rec-info-group">
                  <div className="icon-badge purple small">
                    <Music size={18} />
                  </div>
                  <div className="title-stack">
                    <h4 className="rec-name">{rec.name}</h4>
                    <span className="rec-date">{rec.date}</span>
                  </div>
                </div>

                <div className="rec-actions">
                  <button
                    className="audio-play-circle-btn purple-play"
                    onClick={() => {
                      const nextPlaying = playingId === rec.id ? null : rec.id;
                      setPlayingId(nextPlaying);
                      showToast(nextPlaying ? `Playing ${rec.name}` : 'Audio stopped');
                    }}
                  >
                    {playingId === rec.id ? <Pause size={16} /> : <Play size={16} className="play-icon-offset" />}
                  </button>

                  <button className="btn-delete" onClick={() => deleteRecording(rec.id)}>
                    <Trash2 size={16} />
                  </button>
                </div>
              </div>
            ))}
          </div>
        )}
      </div>
    </div>
  );
}
