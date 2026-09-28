import React, { useState, useEffect, useRef } from 'react';
import * as THREE from 'three';
import { OrbitControls } from 'three/examples/jsm/controls/OrbitControls.js';
import {
  Lightbulb,
  Waves,
  Music,
  Sun,
  Home as HomeIcon,
  Palette,
  Volume2,
  Settings,
  Sparkles,
  ChevronRight,
  Play,
  Pause,
  SlidersHorizontal,
  Wifi,
  Radio
} from 'lucide-react';
import './Home.css';

export default function Home() {
  // Interactive control states matching the screenshot
  const [lightsOn, setLightsOn] = useState(true);
  const [lightsMode, setLightsMode] = useState('Sound Reactive');
  const [brightness, setBrightness] = useState(75);

  const [fountainOn, setFountainOn] = useState(true);
  const [fountainMode, setFountainMode] = useState('Pulsing');
  const [fountainStrength, setFountainStrength] = useState(100);

  const [audioPlaying, setAudioPlaying] = useState(false);
  const [audioTrack, setAudioTrack] = useState('No audio selected');
  const [volume, setVolume] = useState(70);

  const [ambientReading, setAmbientReading] = useState(72);
  const [autoDimming, setAutoDimming] = useState(true);

  // Navigation tab state
  const [activeTab, setActiveTab] = useState('home');

  // Interactive 3D Notification / Toast
  const [toastMessage, setToastMessage] = useState(null);

  const showToast = (msg) => {
    setToastMessage(msg);
    setTimeout(() => setToastMessage(null), 3000);
  };

  // 3D Canvas Ref
  const mountRef = useRef(null);
  const sceneRef = useRef(null);
  const lightsGroupRef = useRef(null);
  const fountainParticlesRef = useRef([]);
  const gateRef = useRef(null);

  // Initialize Three.js Scene
  useEffect(() => {
    const container = mountRef.current;
    if (!container) return;

    const width = container.clientWidth;
    const height = container.clientHeight;

    // Scene
    const scene = new THREE.Scene();
    scene.background = new THREE.Color(0xecf3ec);
    sceneRef.current = scene;

    // Camera
    const camera = new THREE.PerspectiveCamera(38, width / height, 0.1, 1000);
    camera.position.set(16, 15, 20);

    // Renderer
    const renderer = new THREE.WebGLRenderer({ antialias: true, alpha: true });
    renderer.setSize(width, height);
    renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
    renderer.shadowMap.enabled = true;
    renderer.shadowMap.type = THREE.PCFSoftShadowMap;
    renderer.toneMapping = THREE.ACESFilmicToneMapping;
    renderer.toneMappingExposure = 1.1;

    // Clear previous canvas if any
    container.innerHTML = '';
    container.appendChild(renderer.domElement);

    // Controls
    const controls = new OrbitControls(camera, renderer.domElement);
    controls.enableDamping = true;
    controls.dampingFactor = 0.05;
    controls.maxPolarAngle = Math.PI / 2 - 0.05; // Don't go below ground
    controls.minDistance = 10;
    controls.maxDistance = 40;
    controls.target.set(0, 2, 0);

    // Main Lighting Group
    const lightingGroup = new THREE.Group();
    scene.add(lightingGroup);

    // Ambient Light
    const ambientLight = new THREE.AmbientLight(0xfffaee, 0.9);
    lightingGroup.add(ambientLight);

    // Directional Sun Light
    const sunLight = new THREE.DirectionalLight(0xfff7e6, 1.4);
    sunLight.position.set(12, 22, 10);
    sunLight.castShadow = true;
    sunLight.shadow.mapSize.width = 1024;
    sunLight.shadow.mapSize.height = 1024;
    sunLight.shadow.camera.near = 0.5;
    sunLight.shadow.camera.far = 50;
    sunLight.shadow.camera.left = -12;
    sunLight.shadow.camera.right = 12;
    sunLight.shadow.camera.top = 12;
    sunLight.shadow.camera.bottom = -12;
    lightingGroup.add(sunLight);

    // Point lights for diorama lamps
    const pointLightsGroup = new THREE.Group();
    scene.add(pointLightsGroup);
    lightsGroupRef.current = pointLightsGroup;

    // --- DIORAMA BASE & STRUCTURE ---
    const dioramaGroup = new THREE.Group();
    scene.add(dioramaGroup);

    // 1. Sleek Black Base Pedestal
    const baseGeo = new THREE.BoxGeometry(14.2, 1.2, 14.2);
    const baseMat = new THREE.MeshStandardMaterial({
      color: 0x1a1c1e,
      roughness: 0.3,
      metalness: 0.8
    });
    const baseMesh = new THREE.Mesh(baseGeo, baseMat);
    baseMesh.position.y = -0.6;
    baseMesh.receiveShadow = true;
    dioramaGroup.add(baseMesh);

    // Corner metal accents
    const cornerGeo = new THREE.BoxGeometry(0.4, 1.3, 0.4);
    const cornerMat = new THREE.MeshStandardMaterial({ color: 0x333639, metalness: 0.9, roughness: 0.2 });
    [
      [-7.1, -7.1], [7.1, -7.1], [-7.1, 7.1], [7.1, 7.1]
    ].forEach(([cx, cz]) => {
      const corner = new THREE.Mesh(cornerGeo, cornerMat);
      corner.position.set(cx, -0.6, cz);
      dioramaGroup.add(corner);
    });

    // Side power indicator button
    const powerGeo = new THREE.CylinderGeometry(0.2, 0.2, 0.1, 16);
    const powerMat = new THREE.MeshBasicMaterial({ color: 0x569670 });
    const powerBtn = new THREE.Mesh(powerGeo, powerMat);
    powerBtn.rotation.z = Math.PI / 2;
    powerBtn.position.set(7.15, -0.5, 3);
    dioramaGroup.add(powerBtn);

    // 2. Main Sage Green Grass Platform
    const grassGeo = new THREE.BoxGeometry(13.6, 0.6, 13.6);
    const grassMat = new THREE.MeshStandardMaterial({
      color: 0xa9bc9d, // soft sage green matching screenshot
      roughness: 0.85,
      metalness: 0.05
    });
    const grassMesh = new THREE.Mesh(grassGeo, grassMat);
    grassMesh.position.y = 0.3;
    grassMesh.receiveShadow = true;
    dioramaGroup.add(grassMesh);

    // 3. Sand/Beige Cross Pathways
    const pathMat = new THREE.MeshStandardMaterial({
      color: 0xded7c8, // warm sandy beige
      roughness: 0.9,
    });
    const path1 = new THREE.Mesh(new THREE.BoxGeometry(13.5, 0.02, 2.2), pathMat);
    path1.position.y = 0.61;
    path1.receiveShadow = true;
    dioramaGroup.add(path1);

    const path2 = new THREE.Mesh(new THREE.BoxGeometry(2.2, 0.02, 13.5), pathMat);
    path2.position.y = 0.61;
    path2.receiveShadow = true;
    dioramaGroup.add(path2);

    // Central Circular Plaza
    const plazaGeo = new THREE.CylinderGeometry(2.4, 2.4, 0.03, 32);
    const plazaMat = new THREE.MeshStandardMaterial({ color: 0xd9cebe, roughness: 0.7 });
    const plaza = new THREE.Mesh(plazaGeo, plazaMat);
    plaza.position.y = 0.62;
    plaza.receiveShadow = true;
    dioramaGroup.add(plaza);

    const plazaRingGeo = new THREE.RingGeometry(2.2, 2.4, 32);
    const plazaRingMat = new THREE.MeshStandardMaterial({ color: 0xbdad99, side: THREE.DoubleSide });
    const plazaRing = new THREE.Mesh(plazaRingGeo, plazaRingMat);
    plazaRing.rotation.x = -Math.PI / 2;
    plazaRing.position.y = 0.635;
    dioramaGroup.add(plazaRing);

    // Carved Park Text ("SANTA ELENA") simulation on path
    const textMat = new THREE.MeshStandardMaterial({ color: 0x5a554c, roughness: 0.9 });
    for (let i = 0; i < 9; i++) {
      const letterTile = new THREE.Mesh(new THREE.BoxGeometry(0.18, 0.01, 0.18), textMat);
      letterTile.position.set(-1.8 + i * 0.4, 0.63, 2.6);
      letterTile.rotation.y = 0.1;
      dioramaGroup.add(letterTile);
    }

    // 4. Miniature Wooden Archways / Gates (Left & Right)
    const archMat = new THREE.MeshStandardMaterial({ color: 0x8c533c, roughness: 0.6 }); // terracotta brown wood
    const archRoofMat = new THREE.MeshStandardMaterial({ color: 0x543224, roughness: 0.5 });

    const createArch = (x, z, rotY, isInteractive = false) => {
      const archGroup = new THREE.Group();
      archGroup.position.set(x, 0.6, z);
      archGroup.rotation.y = rotY;

      // Pillar Left
      const p1 = new THREE.Mesh(new THREE.BoxGeometry(0.5, 3.2, 0.6), archMat);
      p1.position.set(-1.1, 1.6, 0);
      p1.castShadow = true;
      archGroup.add(p1);

      // Pillar Right
      const p2 = new THREE.Mesh(new THREE.BoxGeometry(0.5, 3.2, 0.6), archMat);
      p2.position.set(1.1, 1.6, 0);
      p2.castShadow = true;
      archGroup.add(p2);

      // Curved Arch Top
      const topBeam = new THREE.Mesh(new THREE.BoxGeometry(2.8, 0.6, 0.7), archRoofMat);
      topBeam.position.set(0, 3.2, 0);
      topBeam.castShadow = true;
      archGroup.add(topBeam);

      const archCurveGeo = new THREE.TorusGeometry(0.9, 0.22, 12, 24, Math.PI);
      const archCurve = new THREE.Mesh(archCurveGeo, archRoofMat);
      archCurve.position.set(0, 2.7, 0);
      archCurve.castShadow = true;
      archGroup.add(archCurve);

      if (isInteractive) gateRef.current = archGroup;
      return archGroup;
    };

    dioramaGroup.add(createArch(-6, 0, Math.PI / 2));
    dioramaGroup.add(createArch(6, 0, -Math.PI / 2, true));

    // 5. Low-Poly Evergreen Pine Trees
    const treeTrunkMat = new THREE.MeshStandardMaterial({ color: 0x4a3425, roughness: 0.9 });
    const treeFoliageMat = new THREE.MeshStandardMaterial({ color: 0x2b543e, roughness: 0.8, flatShading: true }); // dark pine green

    const createPineTree = (x, z, scale = 1) => {
      const tree = new THREE.Group();
      tree.position.set(x, 0.6, z);
      tree.scale.set(scale, scale, scale);

      // Trunk
      const trunk = new THREE.Mesh(new THREE.CylinderGeometry(0.2, 0.28, 1.2, 8), treeTrunkMat);
      trunk.position.y = 0.6;
      trunk.castShadow = true;
      tree.add(trunk);

      // 3 Cone Layers
      const cone1 = new THREE.Mesh(new THREE.ConeGeometry(1.4, 1.6, 7), treeFoliageMat);
      cone1.position.y = 1.6;
      cone1.castShadow = true;
      tree.add(cone1);

      const cone2 = new THREE.Mesh(new THREE.ConeGeometry(1.1, 1.4, 7), treeFoliageMat);
      cone2.position.y = 2.4;
      cone2.castShadow = true;
      tree.add(cone2);

      const cone3 = new THREE.Mesh(new THREE.ConeGeometry(0.75, 1.1, 7), treeFoliageMat);
      cone3.position.y = 3.1;
      cone3.castShadow = true;
      tree.add(cone3);

      return tree;
    };

    // Tree coordinates around park sections
    const treeCoords = [
      [-5, -4.5, 1.1], [-4, -2.5, 0.95], [-5.2, 3.5, 1.2], [-3.8, 5, 0.9],
      [4.8, -4, 1.15], [3.5, -5.2, 0.9], [5, 4.5, 1.2], [3.8, 2.5, 0.85],
      [-1.8, -5.2, 1.0], [1.8, -5.2, 0.95], [-2, 5.2, 1.1], [2, 5.2, 1.0]
    ];
    treeCoords.forEach(([tx, tz, ts]) => dioramaGroup.add(createPineTree(tx, tz, ts)));

    // 6. Fountains (Front & Back)
    const fountainBaseMat = new THREE.MeshStandardMaterial({ color: 0x7da4b3, roughness: 0.4 });
    const waterMat = new THREE.MeshStandardMaterial({
      color: 0x6bbcd9,
      roughness: 0.1,
      metalness: 0.1,
      transparent: true,
      opacity: 0.85
    });

    const createFountain = (x, z) => {
      const fountainGroup = new THREE.Group();
      fountainGroup.position.set(x, 0.6, z);

      // Outer Basin
      const basin = new THREE.Mesh(new THREE.CylinderGeometry(1.2, 1.0, 0.5, 16), fountainBaseMat);
      basin.position.y = 0.25;
      basin.castShadow = true;
      fountainGroup.add(basin);

      // Water Pool
      const pool = new THREE.Mesh(new THREE.CylinderGeometry(1.05, 1.05, 0.05, 16), waterMat);
      pool.position.y = 0.48;
      fountainGroup.add(pool);

      // Center Spout Pillar
      const pillar = new THREE.Mesh(new THREE.CylinderGeometry(0.2, 0.25, 1.4, 12), fountainBaseMat);
      pillar.position.y = 0.95;
      pillar.castShadow = true;
      fountainGroup.add(pillar);

      const topBowl = new THREE.Mesh(new THREE.CylinderGeometry(0.5, 0.3, 0.3, 12), fountainBaseMat);
      topBowl.position.y = 1.6;
      fountainGroup.add(topBowl);

      return fountainGroup;
    };

    dioramaGroup.add(createFountain(0, -3.8));
    dioramaGroup.add(createFountain(0, 3.8));

    // Animated Fountain Particle Droplets
    const particles = [];
    const particleGeo = new THREE.SphereGeometry(0.06, 6, 6);
    const particleMat = new THREE.MeshBasicMaterial({ color: 0x9ee7ff, transparent: true, opacity: 0.85 });

    const fountainCenters = [
      { x: 0, z: -3.8 },
      { x: 0, z: 3.8 }
    ];

    fountainCenters.forEach((fc) => {
      for (let p = 0; p < 24; p++) {
        const particle = new THREE.Mesh(particleGeo, particleMat);
        particle.position.set(
          fc.x + (Math.random() - 0.5) * 0.3,
          2.2 + Math.random() * 0.8,
          fc.z + (Math.random() - 0.5) * 0.3
        );
        particle.userData = {
          originX: fc.x,
          originZ: fc.z,
          vy: 0.03 + Math.random() * 0.04,
          vx: (Math.random() - 0.5) * 0.02,
          vz: (Math.random() - 0.5) * 0.02,
          life: Math.random()
        };
        dioramaGroup.add(particle);
        particles.push(particle);
      }
    });
    fountainParticlesRef.current = particles;

    // 7. Small Peg Figures in Central Plaza
    const figureColors = [0xf4d068, 0xef7a7a, 0x7ebcf0, 0xb88ce8, 0xfa9e5c, 0x78cb96];
    figureColors.forEach((color, i) => {
      const angle = (i / figureColors.length) * Math.PI * 2;
      const radius = 1.3;
      const fx = Math.cos(angle) * radius;
      const fz = Math.sin(angle) * radius;

      const figGroup = new THREE.Group();
      figGroup.position.set(fx, 0.6, fz);

      const figMat = new THREE.MeshStandardMaterial({ color, roughness: 0.4 });
      // Body
      const body = new THREE.Mesh(new THREE.CylinderGeometry(0.12, 0.18, 0.55, 12), figMat);
      body.position.y = 0.285;
      body.castShadow = true;
      figGroup.add(body);

      // Head
      const head = new THREE.Mesh(new THREE.SphereGeometry(0.14, 12, 12), figMat);
      head.position.y = 0.65;
      head.castShadow = true;
      figGroup.add(head);

      dioramaGroup.add(figGroup);
    });

    // 8. Warm Pathway Lamp Posts & Lights
    const lampMat = new THREE.MeshStandardMaterial({ color: 0x3d3b38, roughness: 0.5 });
    const bulbMat = new THREE.MeshBasicMaterial({ color: 0xfffae0 });

    const lampPositions = [
      [-2.8, -1.6], [2.8, -1.6], [-2.8, 1.6], [2.8, 1.6],
      [-1.6, -2.8], [1.6, -2.8], [-1.6, 2.8], [1.6, 2.8]
    ];

    lampPositions.forEach(([lx, lz]) => {
      const lamp = new THREE.Group();
      lamp.position.set(lx, 0.6, lz);

      const pole = new THREE.Mesh(new THREE.CylinderGeometry(0.04, 0.06, 0.7, 8), lampMat);
      pole.position.y = 0.35;
      pole.castShadow = true;
      lamp.add(pole);

      const bulb = new THREE.Mesh(new THREE.SphereGeometry(0.12, 10, 10), bulbMat);
      bulb.position.y = 0.75;
      lamp.add(bulb);

      // Point Light
      const pLight = new THREE.PointLight(0xffebaa, 0.8, 4.5);
      pLight.position.set(lx, 1.4, lz);
      pointLightsGroup.add(pLight);

      dioramaGroup.add(lamp);
    });

    // 9. Glass Enclosure Box (matching the clean transparent glass case in screenshot)
    const glassWallMat = new THREE.MeshPhysicalMaterial({
      color: 0xffffff,
      transmission: 0.92,
      opacity: 1,
      transparent: true,
      roughness: 0.05,
      ior: 1.4,
      thickness: 0.3,
      specularIntensity: 0.8
    });

    // Glass Cube Walls (Open top or glass frame)
    const glassContainer = new THREE.Group();

    const glassHeight = 8;
    const glassWidth = 13.8;

    // Front Glass Wall
    const glassFront = new THREE.Mesh(new THREE.BoxGeometry(glassWidth, glassHeight, 0.08), glassWallMat);
    glassFront.position.set(0, glassHeight / 2 + 0.6, glassWidth / 2);
    glassContainer.add(glassFront);

    // Back Glass Wall
    const glassBack = new THREE.Mesh(new THREE.BoxGeometry(glassWidth, glassHeight, 0.08), glassWallMat);
    glassBack.position.set(0, glassHeight / 2 + 0.6, -glassWidth / 2);
    glassContainer.add(glassBack);

    // Left Glass Wall
    const glassLeft = new THREE.Mesh(new THREE.BoxGeometry(0.08, glassHeight, glassWidth), glassWallMat);
    glassLeft.position.set(-glassWidth / 2, glassHeight / 2 + 0.6, 0);
    glassContainer.add(glassLeft);

    // Right Glass Wall
    const glassRight = new THREE.Mesh(new THREE.BoxGeometry(0.08, glassHeight, glassWidth), glassWallMat);
    glassRight.position.set(glassWidth / 2, glassHeight / 2 + 0.6, 0);
    glassContainer.add(glassRight);

    dioramaGroup.add(glassContainer);

    // Raycaster for user interactions (e.g. clicking 3D diorama elements)
    const raycaster = new THREE.Raycaster();
    const mouse = new THREE.Vector2();

    const handleClick = (event) => {
      const rect = renderer.domElement.getBoundingClientRect();
      mouse.x = ((event.clientX - rect.left) / rect.width) * 2 - 1;
      mouse.y = -((event.clientY - rect.top) / rect.height) * 2 + 1;

      raycaster.setFromCamera(mouse, camera);
      const intersects = raycaster.intersectObjects(dioramaGroup.children, true);

      if (intersects.length > 0) {
        showToast('✨ Interactive Diorama: Front Gate Tapped!');
        // Quick gate animation pulse
        if (gateRef.current) {
          const startRot = gateRef.current.rotation.y;
          let step = 0;
          const anim = setInterval(() => {
            step++;
            gateRef.current.rotation.y = startRot + Math.sin(step * 0.3) * 0.25;
            if (step > 20) {
              clearInterval(anim);
              gateRef.current.rotation.y = startRot;
            }
          }, 30);
        }
      }
    };

    renderer.domElement.addEventListener('click', handleClick);

    // Resize Handler
    const handleResize = () => {
      if (!container) return;
      const w = container.clientWidth;
      const h = container.clientHeight;
      camera.aspect = w / h;
      camera.updateProjectionMatrix();
      renderer.setSize(w, h);
    };
    window.addEventListener('resize', handleResize);

    // Animation Loop
    let animationFrameId;
    let clock = new THREE.Clock();

    const animate = () => {
      animationFrameId = requestAnimationFrame(animate);
      const time = clock.getElapsedTime();

      // Slow ambient diorama drift rotation if idle
      dioramaGroup.rotation.y = Math.sin(time * 0.15) * 0.05;

      // Fountain Animation
      if (fountainParticlesRef.current && fountainParticlesRef.current.length > 0) {
        const isFountainActive = fountainOn;
        const speedMult = (fountainStrength / 100) * (isFountainActive ? 1 : 0);

        fountainParticlesRef.current.forEach((p) => {
          if (!isFountainActive) {
            p.visible = false;
            return;
          }
          p.visible = true;

          p.position.y += p.userData.vy * speedMult;
          p.position.x += p.userData.vx * speedMult;
          p.position.z += p.userData.vz * speedMult;
          p.userData.vy -= 0.0015; // gravity

          if (p.position.y < 1.1) {
            // Reset particle
            p.position.set(
              p.userData.originX + (Math.random() - 0.5) * 0.2,
              1.7,
              p.userData.originZ + (Math.random() - 0.5) * 0.2
            );
            p.userData.vy = 0.035 + Math.random() * 0.03;
          }
        });
      }

      controls.update();
      renderer.render(scene, camera);
    };

    animate();

    // Cleanup
    return () => {
      window.removeEventListener('resize', handleResize);
      if (renderer.domElement) {
        renderer.domElement.removeEventListener('click', handleClick);
      }
      cancelAnimationFrame(animationFrameId);
      renderer.dispose();
    };
  }, []);

  // Sync Lights & Brightness state with 3D Scene Point Lights
  useEffect(() => {
    if (!lightsGroupRef.current) return;
    const targetIntensity = lightsOn ? (brightness / 100) * 1.5 : 0;
    lightsGroupRef.current.children.forEach((pl) => {
      pl.intensity = targetIntensity;
    });
  }, [lightsOn, brightness]);

  // Sync Ambient Dimming
  useEffect(() => {
    if (!sceneRef.current) return;
    const bgVal = autoDimming ? 0xecf3ec : 0xf7faf7;
    sceneRef.current.background = new THREE.Color(bgVal);
  }, [autoDimming]);

  return (
    <div className="diorama-app-container">
      {/* 1. TOP NAVBAR (For Web View - Classic Desktop Navigation) */}
      <header className="top-navbar-desktop">
        <div className="navbar-content">
          <div className="brand-logo">
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
              onClick={() => {
                setActiveTab('lights');
                showToast('Switched to Lights & Colors View');
              }}
            >
              <Lightbulb size={18} />
              <span>Lights & Colors</span>
            </button>
            <button
              className={`nav-item ${activeTab === 'fountain' ? 'active' : ''}`}
              onClick={() => {
                setActiveTab('fountain');
                showToast('Switched to Fountain View');
              }}
            >
              <Waves size={18} />
              <span>Fountain</span>
            </button>
            <button
              className={`nav-item ${activeTab === 'audio' ? 'active' : ''}`}
              onClick={() => {
                setActiveTab('audio');
                showToast('Switched to Audio Controls');
              }}
            >
              <Music size={18} />
              <span>Audio</span>
            </button>
          </nav>

          <div className="navbar-actions">
            <div className="status-pill">
              <span className="status-dot"></span>
              <span className="status-text">Connected</span>
            </div>
            <button
              className="icon-button settings-btn"
              title="Settings"
              onClick={() => showToast('Settings Opened')}
            >
              <Settings size={20} />
            </button>
          </div>
        </div>
      </header>

      {/* MOBILE TOP HEADER (Visible on Small Screens) */}
      <header className="mobile-header">
        <div className="mobile-brand">
          <Sparkles className="mobile-sparkle" size={20} />
          <span className="mobile-title">My Diorama</span>
        </div>
        <button
          className="mobile-settings-btn"
          onClick={() => showToast('Settings Opened')}
        >
          <Settings size={20} />
        </button>
      </header>

      {/* MAIN CONTENT AREA */}
      <main className="main-viewport">
        <div className="content-max-width">
          {/* Header Title Section */}
          <div className="page-header-text">
            <div className="section-tag">YOUR LITTLE WORLD</div>
            <h1 className="page-main-title">My Diorama</h1>
            <p className="page-subtitle">
              Watch your miniature world come alive as you tweak it.
            </p>
          </div>

          {/* Toast Notification Alert */}
          {toastMessage && (
            <div className="toast-banner">
              <span>{toastMessage}</span>
            </div>
          )}

          {/* 3D DIORAMA PREVIEW CARD */}
          <div className="diorama-preview-card">
            <div className="card-top-tag">YOUR LITTLE WORLD</div>
            <div className="canvas-wrapper" ref={mountRef}>
              {/* WebGL Canvas mounts here */}
            </div>
            <div className="canvas-overlay-instruction">
              Drag to rotate · Tap the front gate
            </div>
          </div>

          {/* INTERACTIVE CONTROLS GRID */}
          <div className="controls-grid">
            {/* 1. LIGHTS CARD */}
            <div className={`control-card ${lightsOn ? 'active-card' : ''}`}>
              <div className="card-header">
                <div className="card-title-group">
                  <div className="icon-badge green">
                    <Lightbulb size={22} />
                  </div>
                  <div className="title-stack">
                    <h3 className="card-title">Lights</h3>
                    <span className="card-status-subtext">
                      {lightsOn ? `On - ${lightsMode}` : 'Off'}
                    </span>
                  </div>
                </div>
                <label className="toggle-switch">
                  <input
                    type="checkbox"
                    checked={lightsOn}
                    onChange={(e) => setLightsOn(e.target.checked)}
                  />
                  <span className="slider round"></span>
                </label>
              </div>

              <div className="card-slider-group">
                <div className="slider-label-row">
                  <span>Brightness</span>
                  <span className="value-label">{brightness}%</span>
                </div>
                <input
                  type="range"
                  min="0"
                  max="100"
                  value={brightness}
                  disabled={!lightsOn}
                  className="custom-range-slider"
                  onChange={(e) => setBrightness(Number(e.target.value))}
                />
              </div>
            </div>

            {/* 2. FOUNTAIN CARD */}
            <div className={`control-card ${fountainOn ? 'active-card' : ''}`}>
              <div className="card-header">
                <div className="card-title-group">
                  <div className="icon-badge green">
                    <Waves size={22} />
                  </div>
                  <div className="title-stack">
                    <h3 className="card-title">Fountain</h3>
                    <span className="card-status-subtext">
                      {fountainOn ? `On - ${fountainMode}` : 'Off'}
                    </span>
                  </div>
                </div>
                <label className="toggle-switch">
                  <input
                    type="checkbox"
                    checked={fountainOn}
                    onChange={(e) => setFountainOn(e.target.checked)}
                  />
                  <span className="slider round"></span>
                </label>
              </div>

              <div className="card-slider-group">
                <div className="slider-label-row">
                  <span>Strength</span>
                  <span className="value-label">{fountainStrength}%</span>
                </div>
                <input
                  type="range"
                  min="0"
                  max="100"
                  value={fountainStrength}
                  disabled={!fountainOn}
                  className="custom-range-slider"
                  onChange={(e) => setFountainStrength(Number(e.target.value))}
                />
              </div>
            </div>

            {/* 3. AUDIO CARD */}
            <div className="control-card">
              <div className="card-header">
                <div className="card-title-group">
                  <div className="icon-badge green">
                    <Music size={22} />
                  </div>
                  <div className="title-stack">
                    <h3 className="card-title">Audio</h3>
                    <span className="card-status-subtext">{audioTrack}</span>
                  </div>
                </div>
                <button
                  className="audio-play-circle-btn"
                  onClick={() => {
                    const nextPlaying = !audioPlaying;
                    setAudioPlaying(nextPlaying);
                    setAudioTrack(nextPlaying ? 'Nature Ambient Stream' : 'No audio selected');
                    showToast(nextPlaying ? 'Playing Nature Ambient Stream' : 'Audio Paused');
                  }}
                >
                  {audioPlaying ? <Pause size={16} /> : <Play size={16} className="play-icon-offset" />}
                </button>
              </div>

              <div className="card-slider-group">
                <div className="slider-label-row">
                  <span>Volume</span>
                  <span className="value-label">{volume}%</span>
                </div>
                <input
                  type="range"
                  min="0"
                  max="100"
                  value={volume}
                  className="custom-range-slider"
                  onChange={(e) => setVolume(Number(e.target.value))}
                />
              </div>
            </div>

            {/* 4. AMBIENT LIGHT CARD */}
            <div className="control-card">
              <div className="card-header">
                <div className="card-title-group">
                  <div className="icon-badge green">
                    <Sun size={22} />
                  </div>
                  <div className="title-stack">
                    <h3 className="card-title">Ambient Light</h3>
                  </div>
                </div>
              </div>

              <div className="ambient-metric-row">
                <span className="metric-large">{ambientReading}%</span>
                <span className="metric-subtext">simulated reading</span>
              </div>

              <div className="card-footer-row">
                <span className="footer-label">Auto dimming</span>
                <label className="toggle-switch">
                  <input
                    type="checkbox"
                    checked={autoDimming}
                    onChange={(e) => setAutoDimming(e.target.checked)}
                  />
                  <span className="slider round"></span>
                </label>
              </div>
            </div>
          </div>

          {/* BOTTOM TIP BAR */}
          <div
            className="tip-banner-card"
            onClick={() => {
              setActiveTab('lights');
              showToast('Navigating to Lights & Colors');
            }}
          >
            <span>Tip: open Lights & Colors to change colors and modes</span>
            <ChevronRight size={20} className="tip-arrow" />
          </div>
        </div>
      </main>

      {/* MOBILE FLOATING BOTTOM NAVIGATION BAR (Matching screenshot floating pill bar) */}
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
        </div>
      </div>
    </div>
  );
}
