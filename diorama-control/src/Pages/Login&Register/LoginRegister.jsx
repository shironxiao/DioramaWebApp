import React, { useState } from 'react';
import { Sparkles, Mail, Lock, Eye, EyeOff, User } from 'lucide-react';
import './LoginRegister.css';

export default function LoginRegister({ onLoginSuccess, showToast }) {
  const [isRegisterMode, setIsRegisterMode] = useState(false);

  // Form Fields State
  const [loginEmail, setLoginEmail] = useState('Ronald.sevilla@example.com');
  const [loginPassword, setLoginPassword] = useState('password123');
  const [showPassword, setShowPassword] = useState(false);
  const [rememberMe, setRememberMe] = useState(true);

  // Register Fields
  const [firstName, setFirstName] = useState('');
  const [lastName, setLastName] = useState('');
  const [regEmail, setRegEmail] = useState('');
  const [regPassword, setRegPassword] = useState('');
  const [confirmPassword, setConfirmPassword] = useState('');
  const [showRegPassword, setShowRegPassword] = useState(false);
  const [showConfirmPassword, setShowConfirmPassword] = useState(false);
  const [agreeTerms, setAgreeTerms] = useState(false);

  const handleLoginSubmit = (e) => {
    e.preventDefault();
    showToast('Logging in...');
    setTimeout(() => {
      onLoginSuccess('Ronald Sevilla');
      showToast('Welcome back, Ronald Sevilla!');
    }, 800);
  };

  const handleRegisterSubmit = (e) => {
    e.preventDefault();
    if (regPassword !== confirmPassword) {
      showToast('Error: Passwords do not match!');
      return;
    }
    if (!agreeTerms) {
      showToast('Please agree to the Terms of Service.');
      return;
    }
    showToast('Creating account...');
    setTimeout(() => {
      const name = `${firstName} ${lastName}`.trim() || 'New User';
      onLoginSuccess(name);
      showToast(`Account created successfully! Welcome, ${name}!`);
    }, 1000);
  };

  return (
    <div className="login-page-wrapper">
      <div className="login-card-container">
        {/* Brand Header */}
        <div className="login-brand-header">
          <div className="login-sparkle-badge">
            <Sparkles className="sparkle-icon" size={26} />
          </div>
          <h1 className="login-brand-title">My Diorama</h1>
          <p className="login-brand-subtitle">Control your little world.</p>
        </div>

        {/* Card Form Wrapper with Animated Mode Switching */}
        <div className="login-card-box">
          {!isRegisterMode ? (
            /* --- LOGIN FORM --- */
            <form className="login-form-content animate-slide-in" onSubmit={handleLoginSubmit}>
              <div className="input-field-group">
                <label className="input-label">Username / Email</label>
                <div className="input-with-icon">
                  <Mail className="field-icon" size={18} />
                  <input
                    type="email"
                    required
                    placeholder="you@example.com"
                    value={loginEmail}
                    onChange={(e) => setLoginEmail(e.target.value)}
                  />
                </div>
              </div>

              <div className="input-field-group">
                <label className="input-label">Password</label>
                <div className="input-with-icon">
                  <Lock className="field-icon" size={18} />
                  <input
                    type={showPassword ? 'text' : 'password'}
                    required
                    placeholder="••••••••"
                    value={loginPassword}
                    onChange={(e) => setLoginPassword(e.target.value)}
                  />
                  <button
                    type="button"
                    className="btn-eye-toggle"
                    onClick={() => setShowPassword(!showPassword)}
                  >
                    {showPassword ? <EyeOff size={18} /> : <Eye size={18} />}
                  </button>
                </div>
              </div>

              {/* Remember Me on Left, Forgot Password on Right */}
              <div className="checkbox-row space-between">
                <label className="custom-checkbox-label">
                  <input
                    type="checkbox"
                    checked={rememberMe}
                    onChange={(e) => setRememberMe(e.target.checked)}
                  />
                  <span>Remember me</span>
                </label>

                <a
                  href="#forgot"
                  className="forgot-link"
                  onClick={(e) => {
                    e.preventDefault();
                    showToast('Password reset link sent to email!');
                  }}
                >
                  Forgot password?
                </a>
              </div>

              <button type="submit" className="btn-sage-submit">
                Login
              </button>
            </form>
          ) : (
            /* --- REGISTER FORM --- */
            <form className="login-form-content animate-slide-in" onSubmit={handleRegisterSubmit}>
              {/* First Name & Last Name Grid */}
              <div className="name-inputs-row">
                <div className="input-field-group">
                  <label className="input-label">First Name</label>
                  <div className="input-with-icon">
                    <User className="field-icon" size={18} />
                    <input
                      type="text"
                      required
                      placeholder="Ronald"
                      value={firstName}
                      onChange={(e) => setFirstName(e.target.value)}
                    />
                  </div>
                </div>

                <div className="input-field-group">
                  <label className="input-label">Last Name</label>
                  <div className="input-with-icon">
                    <User className="field-icon" size={18} />
                    <input
                      type="text"
                      required
                      placeholder="Sevilla"
                      value={lastName}
                      onChange={(e) => setLastName(e.target.value)}
                    />
                  </div>
                </div>
              </div>

              {/* Email */}
              <div className="input-field-group">
                <label className="input-label">Email</label>
                <div className="input-with-icon">
                  <Mail className="field-icon" size={18} />
                  <input
                    type="email"
                    required
                    placeholder="you@example.com"
                    value={regEmail}
                    onChange={(e) => setRegEmail(e.target.value)}
                  />
                </div>
              </div>

              {/* Password */}
              <div className="input-field-group">
                <label className="input-label">Password</label>
                <div className="input-with-icon">
                  <Lock className="field-icon" size={18} />
                  <input
                    type={showRegPassword ? 'text' : 'password'}
                    required
                    placeholder="••••••••"
                    value={regPassword}
                    onChange={(e) => setRegPassword(e.target.value)}
                  />
                  <button
                    type="button"
                    className="btn-eye-toggle"
                    onClick={() => setShowRegPassword(!showRegPassword)}
                  >
                    {showRegPassword ? <EyeOff size={18} /> : <Eye size={18} />}
                  </button>
                </div>
              </div>

              {/* Confirm Password */}
              <div className="input-field-group">
                <label className="input-label">Confirm Password</label>
                <div className="input-with-icon">
                  <Lock className="field-icon" size={18} />
                  <input
                    type={showConfirmPassword ? 'text' : 'password'}
                    required
                    placeholder="••••••••"
                    value={confirmPassword}
                    onChange={(e) => setConfirmPassword(e.target.value)}
                  />
                  <button
                    type="button"
                    className="btn-eye-toggle"
                    onClick={() => setShowConfirmPassword(!showConfirmPassword)}
                  >
                    {showConfirmPassword ? <EyeOff size={18} /> : <Eye size={18} />}
                  </button>
                </div>
              </div>

              {/* Terms Checkbox */}
              <div className="checkbox-row">
                <label className="custom-checkbox-label text-sm">
                  <input
                    type="checkbox"
                    checked={agreeTerms}
                    onChange={(e) => setAgreeTerms(e.target.checked)}
                  />
                  <span>I agree to the Terms of Service & Privacy Policy</span>
                </label>
              </div>

              <button type="submit" className="btn-sage-submit">
                Create Account
              </button>
            </form>
          )}

          {/* Mode Switch Footer */}
          <div className="login-footer-switch">
            {!isRegisterMode ? (
              <span>
                Don't have an account?{' '}
                <button
                  type="button"
                  className="switch-link-btn"
                  onClick={() => setIsRegisterMode(true)}
                >
                  Create one
                </button>
              </span>
            ) : (
              <span>
                Already have an account?{' '}
                <button
                  type="button"
                  className="switch-link-btn"
                  onClick={() => setIsRegisterMode(false)}
                >
                  Log in
                </button>
              </span>
            )}
          </div>
        </div>
      </div>
    </div>
  );
}
