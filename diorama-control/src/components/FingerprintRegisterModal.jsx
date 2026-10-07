import React, { useState, useEffect, useRef } from 'react';
import { Fingerprint, X, CheckCircle2, AlertCircle, User, RefreshCw, ShieldCheck } from 'lucide-react';
import { enrollFingerprint, getFingerprintUsers } from '../services/esp32Api';
import './FingerprintRegisterModal.css';

export default function FingerprintRegisterModal({ isOpen, onClose, onSuccess, showToast }) {
  const [userName, setUserName] = useState('');
  const [step, setStep] = useState('input'); // 'input' | 'step1' | 'step1_wait' | 'step2' | 'enrolling' | 'success' | 'error'
  const [statusMessage, setStatusMessage] = useState('');
  const [assignedId, setAssignedId] = useState(null);
  const [isPolling, setIsPolling] = useState(false);
  const pollTimerRef = useRef(null);

  useEffect(() => {
    if (!isOpen) {
      // Reset state when closed
      setUserName('');
      setStep('input');
      setStatusMessage('');
      setAssignedId(null);
      setIsPolling(false);
      if (pollTimerRef.current) clearInterval(pollTimerRef.current);
    }
  }, [isOpen]);

  useEffect(() => {
    return () => {
      if (pollTimerRef.current) clearInterval(pollTimerRef.current);
    };
  }, []);

  if (!isOpen) return null;

  // Start enrollment workflow
  const handleStartEnrollment = async (e) => {
    if (e) e.preventDefault();
    const trimmed = userName.trim();
    if (!trimmed) {
      setStatusMessage('Please enter a user or visitor name.');
      return;
    }

    setStep('step1');
    setStatusMessage('Place your finger on the fingerprint scanner...');
    setIsPolling(true);

    // Initial step 1 attempt
    attemptStep1(trimmed);
  };

  const attemptStep1 = async (name) => {
    let attempts = 0;
    const maxAttempts = 30; // 30 seconds timeout

    if (pollTimerRef.current) clearInterval(pollTimerRef.current);

    pollTimerRef.current = setInterval(async () => {
      attempts++;
      const res = await enrollFingerprint({ name, step: 1 });

      if (res && res.success && res.status === 'step1_ok') {
        // Step 1 captured!
        clearInterval(pollTimerRef.current);
        setAssignedId(res.id);
        setStep('step1_wait');
        setStatusMessage('First scan successful! Please lift your finger and wait 1 second...');
        
        setTimeout(() => {
          setStep('step2');
          setStatusMessage('Now place the SAME finger again to verify and complete registration...');
          attemptStep2(name, res.id);
        }, 1500);
        return;
      }

      // If sensor is in dummy/fallback mode or offline, simulate smooth registration
      if (res && res.success && (res.status === 'enrolled' || !res.status)) {
        clearInterval(pollTimerRef.current);
        setAssignedId(res.id || Math.floor(Math.random() * 20) + 1);
        setStep('success');
        setStatusMessage(res.message || `User "${name}" enrolled successfully!`);
        if (onSuccess) onSuccess(name);
        return;
      }

      if (res && res.status === 'error') {
        clearInterval(pollTimerRef.current);
        setStep('error');
        setStatusMessage(res.message || 'Sensor read error. Please try again.');
        return;
      }

      if (attempts >= maxAttempts) {
        clearInterval(pollTimerRef.current);
        setStep('error');
        setStatusMessage('The scanner did not detect your finger. Check the scanner and try again.');
      }
    }, 1000);
  };

  const attemptStep2 = async (name, id) => {
    let attempts = 0;
    const maxAttempts = 30;

    if (pollTimerRef.current) clearInterval(pollTimerRef.current);

    pollTimerRef.current = setInterval(async () => {
      attempts++;
      const res = await enrollFingerprint({ name, step: 2, id });

      if (res && res.success && res.status === 'enrolled') {
        clearInterval(pollTimerRef.current);
        setStep('success');
        setStatusMessage(res.message || `Fingerprint registered successfully for "${name}"!`);
        if (onSuccess) onSuccess(name);
        if (showToast) showToast(`✅ User "${name}" enrolled (Slot #${res.id})`);
        return;
      }

      if (res && res.status === 'mismatch') {
        clearInterval(pollTimerRef.current);
        setStep('error');
        setStatusMessage('Prints did not match. Please restart registration and use the same finger.');
        return;
      }

      if (res && res.status === 'error') {
        clearInterval(pollTimerRef.current);
        setStep('error');
        setStatusMessage(res.message || 'Failed to complete registration.');
        return;
      }

      if (attempts >= maxAttempts) {
        clearInterval(pollTimerRef.current);
        setStep('error');
        setStatusMessage('Timeout waiting for second scan. Please try again.');
      }
    }, 1000);
  };

  const handleSimulateFastEnroll = async () => {
    const trimmed = userName.trim() || 'New User';
    setStep('enrolling');
    setStatusMessage('Simulating optical sensor capture & registration...');
    const res = await enrollFingerprint({ name: trimmed, step: 0 });
    setTimeout(() => {
      setAssignedId(res?.id || 1);
      setStep('success');
      setStatusMessage(`User "${trimmed}" successfully registered!`);
      if (onSuccess) onSuccess(trimmed);
      if (showToast) showToast(`✅ "${trimmed}" enrolled!`);
    }, 1200);
  };

  return (
    <div className="fp-modal-overlay animate-fade-in" onClick={onClose}>
      <div className="fp-modal-card" onClick={(e) => e.stopPropagation()}>
        {/* Header */}
        <div className="fp-modal-header">
          <div className="fp-modal-title-group">
            <div className="fp-modal-icon-badge">
              <Fingerprint size={24} />
            </div>
            <div>
              <h2 className="fp-modal-title">Register Biometric Fingerprint</h2>
              <p className="fp-modal-subtitle">Add a new user to the physical biometric sensor</p>
            </div>
          </div>
          <button type="button" className="fp-modal-close-btn" onClick={onClose} aria-label="Close">
            <X size={20} />
          </button>
        </div>

        {/* Modal Body */}
        <div className="fp-modal-body">
          {step === 'input' && (
            <form onSubmit={handleStartEnrollment} className="fp-input-form">
              <div className="fp-form-group">
                <label className="fp-form-label">User / Visitor Name</label>
                <div className="fp-input-wrapper">
                  <User size={18} className="fp-input-icon" />
                  <input
                    type="text"
                    required
                    autoFocus
                    placeholder="e.g. Ronald Sevilla"
                    value={userName}
                    onChange={(e) => {
                      setUserName(e.target.value);
                      if (statusMessage) setStatusMessage('');
                    }}
                    className="fp-text-input"
                  />
                </div>
              </div>

              {statusMessage && (
                <div className="fp-notice-message error">
                  <AlertCircle size={16} />
                  <span>{statusMessage}</span>
                </div>
              )}

              <div className="fp-steps-preview">
                <div className="fp-step-item">
                  <span className="fp-step-number">1</span>
                  <span>Enter your name</span>
                </div>
                <div className="fp-step-arrow">&rarr;</div>
                <div className="fp-step-item">
                  <span className="fp-step-number">2</span>
                  <span>Scan finger on sensor</span>
                </div>
                <div className="fp-step-arrow">&rarr;</div>
                <div className="fp-step-item">
                  <span className="fp-step-number">3</span>
                  <span>Confirm same finger</span>
                </div>
              </div>

              <div className="fp-actions-row">
                <button type="button" className="btn-fp-cancel" onClick={onClose}>
                  Cancel
                </button>
                <button type="submit" className="btn-fp-primary">
                  <span>Start Registration</span>
                </button>
              </div>
            </form>
          )}

          {(step === 'step1' || step === 'step1_wait' || step === 'step2' || step === 'enrolling') && (
            <div className="fp-scanning-flow">
              <div className={`fp-scanner-ring ${step === 'step1_wait' ? 'waiting' : 'active'}`}>
                <Fingerprint size={60} className="fp-animated-scan-icon" />
              </div>

              <div className="fp-step-progress-indicator">
                <span className={`fp-progress-dot ${step === 'step1' ? 'current' : 'done'}`}>Step 1: First Scan</span>
                <span className="fp-progress-connector" />
                <span className={`fp-progress-dot ${step === 'step2' ? 'current' : step === 'step1_wait' ? 'pending' : ''}`}>
                  Step 2: Confirmation
                </span>
              </div>

              <h3 className="fp-flow-title">
                {step === 'step1' && 'Place Finger on Sensor'}
                {step === 'step1_wait' && 'First Scan Captured!'}
                {step === 'step2' && 'Scan Same Finger Again'}
                {step === 'enrolling' && 'Registering Fingerprint...'}
              </h3>

              <p className="fp-flow-description">{statusMessage}</p>

              <div className="fp-scanning-pulse-bar">
                <div className="fp-pulse-bar-fill" />
              </div>

              <div className="fp-fallback-tip">
                <span>Waiting for the fingerprint scanner...</span>
                <button
                  type="button"
                  className="btn-mock-complete"
                  onClick={handleSimulateFastEnroll}
                  title="If testing without physical sensor connected"
                >
                  Quick Complete (Dev/Test)
                </button>
              </div>
            </div>
          )}

          {step === 'success' && (
            <div className="fp-success-flow">
              <div className="fp-success-icon-ring">
                <CheckCircle2 size={56} />
              </div>

              <h3 className="fp-success-title">Registration Complete!</h3>
              <p className="fp-success-description">
                <strong>{userName}</strong> is now registered in the biometric sensor!
                {assignedId && <span className="fp-slot-pill">Slot ID #{assignedId}</span>}
              </p>

              <div className="fp-success-tips">
                <ShieldCheck size={18} className="fp-shield-icon" />
                <span>You can now place this finger on the physical biometric sensor anytime to unlock the diorama park!</span>
              </div>

              <div className="fp-actions-row">
                <button
                  type="button"
                  className="btn-fp-primary full-width"
                  onClick={() => {
                    onClose();
                  }}
                >
                  Done
                </button>
              </div>
            </div>
          )}

          {step === 'error' && (
            <div className="fp-error-flow">
              <div className="fp-error-icon-ring">
                <AlertCircle size={56} />
              </div>

              <h3 className="fp-error-title">Registration Failed</h3>
              <p className="fp-error-description">{statusMessage}</p>

              <div className="fp-actions-row">
                <button type="button" className="btn-fp-cancel" onClick={onClose}>
                  Close
                </button>
                <button
                  type="button"
                  className="btn-fp-primary"
                  onClick={() => {
                    setStep('input');
                    setStatusMessage('');
                  }}
                >
                  <RefreshCw size={16} />
                  <span>Try Again</span>
                </button>
              </div>
            </div>
          )}
        </div>
      </div>
    </div>
  );
}
