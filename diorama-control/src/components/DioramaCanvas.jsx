import React, { useEffect, useRef } from 'react';
import * as THREE from 'three';
import { OrbitControls } from 'three/examples/jsm/controls/OrbitControls.js';
import { STLLoader } from 'three/examples/jsm/loaders/STLLoader.js';

export default function DioramaCanvas({
  lightsOn = true,
  brightness = 75,
  fountainOn = true,
  fountainStrength = 100,
  fountainPattern = 'Pulsing',
  fountainColor = '#6bbcd9',
  circleColor = '#d9cebe',
  autoDimming = true,
  onGateClick = () => {}
}) {
  const mountRef = useRef(null);
  const sceneRef = useRef(null);
  const lightsGroupRef = useRef(null);
  const fountainParticlesRef = useRef([]);
  const plazaMeshRef = useRef(null);
  const fountainMeshRef = useRef(null);
  const customStlMeshRef = useRef(null);
  const gateRef = useRef(null);

  useEffect(() => {
    const container = mountRef.current;
    if (!container) return;

    const width = container.clientWidth;
    const height = container.clientHeight;

    // Scene setup
    const scene = new THREE.Scene();
    scene.background = new THREE.Color(autoDimming ? 0xecf3ec : 0xf7faf7);
    sceneRef.current = scene;

    // Camera
    const camera = new THREE.PerspectiveCamera(38, width / height, 0.1, 1000);
    camera.position.set(16, 15, 20);

    // Renderer
    const renderer = new THREE.WebGLRenderer({ antialias: true, alpha: true });
    renderer.setSize(width, height);
    renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
    renderer.shadowMap.enabled = true;
    renderer.shadowMap.type = THREE.PCFShadowMap;
    renderer.toneMapping = THREE.ACESFilmicToneMapping;
    renderer.toneMappingExposure = 1.1;

    container.innerHTML = '';
    container.appendChild(renderer.domElement);

    // Orbit Controls
    const controls = new OrbitControls(camera, renderer.domElement);
    controls.enableDamping = true;
    controls.dampingFactor = 0.05;
    controls.maxPolarAngle = Math.PI / 2 - 0.05;
    controls.minDistance = 10;
    controls.maxDistance = 40;
    controls.target.set(0, 2, 0);

    // Main Lighting Group
    const lightingGroup = new THREE.Group();
    scene.add(lightingGroup);

    const ambientLight = new THREE.AmbientLight(0xfffaee, 0.9);
    lightingGroup.add(ambientLight);

    const sunLight = new THREE.DirectionalLight(0xfff7e6, 1.4);
    sunLight.position.set(12, 22, 10);
    sunLight.castShadow = true;
    sunLight.shadow.mapSize.width = 1024;
    sunLight.shadow.mapSize.height = 1024;
    sunLight.shadow.camera.near = 0.5;
    sunLight.shadow.camera.far = 50;
    lightingGroup.add(sunLight);

    // Point lights group
    const pointLightsGroup = new THREE.Group();
    scene.add(pointLightsGroup);
    lightsGroupRef.current = pointLightsGroup;

    // --- DIORAMA GROUP ---
    const dioramaGroup = new THREE.Group();
    scene.add(dioramaGroup);

    // 1. Black Base
    const baseGeo = new THREE.BoxGeometry(14.2, 1.2, 14.2);
    const baseMat = new THREE.MeshStandardMaterial({ color: 0x1a1c1e, roughness: 0.3, metalness: 0.8 });
    const baseMesh = new THREE.Mesh(baseGeo, baseMat);
    baseMesh.position.y = -0.6;
    baseMesh.receiveShadow = true;
    dioramaGroup.add(baseMesh);

    // Base Corners
    const cornerGeo = new THREE.BoxGeometry(0.4, 1.3, 0.4);
    const cornerMat = new THREE.MeshStandardMaterial({ color: 0x333639, metalness: 0.9, roughness: 0.2 });
    [[-7.1, -7.1], [7.1, -7.1], [-7.1, 7.1], [7.1, 7.1]].forEach(([cx, cz]) => {
      const corner = new THREE.Mesh(cornerGeo, cornerMat);
      corner.position.set(cx, -0.6, cz);
      dioramaGroup.add(corner);
    });

    // 2. Sage Green Ground
    const grassGeo = new THREE.BoxGeometry(13.6, 0.6, 13.6);
    const grassMat = new THREE.MeshStandardMaterial({ color: 0xa9bc9d, roughness: 0.85 });
    const grassMesh = new THREE.Mesh(grassGeo, grassMat);
    grassMesh.position.y = 0.3;
    grassMesh.receiveShadow = true;
    dioramaGroup.add(grassMesh);

    // 3. Sandy Cross Paths
    const pathMat = new THREE.MeshStandardMaterial({ color: 0xded7c8, roughness: 0.9 });
    const path1 = new THREE.Mesh(new THREE.BoxGeometry(13.5, 0.02, 2.2), pathMat);
    path1.position.y = 0.61;
    path1.receiveShadow = true;
    dioramaGroup.add(path1);

    const path2 = new THREE.Mesh(new THREE.BoxGeometry(2.2, 0.02, 13.5), pathMat);
    path2.position.y = 0.61;
    path2.receiveShadow = true;
    dioramaGroup.add(path2);

    // Central Circle Plaza
    const plazaGeo = new THREE.CylinderGeometry(2.4, 2.4, 0.03, 32);
    const plazaMat = new THREE.MeshStandardMaterial({ color: new THREE.Color(circleColor), roughness: 0.7 });
    const plaza = new THREE.Mesh(plazaGeo, plazaMat);
    plaza.position.y = 0.62;
    plaza.receiveShadow = true;
    dioramaGroup.add(plaza);
    plazaMeshRef.current = plaza;

    // 4. Wooden Arch Gates
    const archMat = new THREE.MeshStandardMaterial({ color: 0x8c533c, roughness: 0.6 });
    const archRoofMat = new THREE.MeshStandardMaterial({ color: 0x543224, roughness: 0.5 });

    const createArch = (x, z, rotY, isInteractive = false) => {
      const archGroup = new THREE.Group();
      archGroup.position.set(x, 0.6, z);
      archGroup.rotation.y = rotY;

      const p1 = new THREE.Mesh(new THREE.BoxGeometry(0.5, 3.2, 0.6), archMat);
      p1.position.set(-1.1, 1.6, 0);
      p1.castShadow = true;
      archGroup.add(p1);

      const p2 = new THREE.Mesh(new THREE.BoxGeometry(0.5, 3.2, 0.6), archMat);
      p2.position.set(1.1, 1.6, 0);
      p2.castShadow = true;
      archGroup.add(p2);

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

    // 5. Pine Trees
    const treeTrunkMat = new THREE.MeshStandardMaterial({ color: 0x4a3425, roughness: 0.9 });
    const treeFoliageMat = new THREE.MeshStandardMaterial({ color: 0x2b543e, roughness: 0.8, flatShading: true });

    const createPineTree = (x, z, scale = 1) => {
      const tree = new THREE.Group();
      tree.position.set(x, 0.6, z);
      tree.scale.set(scale, scale, scale);

      const trunk = new THREE.Mesh(new THREE.CylinderGeometry(0.2, 0.28, 1.2, 8), treeTrunkMat);
      trunk.position.y = 0.6;
      trunk.castShadow = true;
      tree.add(trunk);

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

    [
      [-5, -4.5, 1.1], [-4, -2.5, 0.95], [-5.2, 3.5, 1.2], [-3.8, 5, 0.9],
      [4.8, -4, 1.15], [3.5, -5.2, 0.9], [5, 4.5, 1.2], [3.8, 2.5, 0.85],
      [-1.8, -5.2, 1.0], [1.8, -5.2, 0.95], [-2, 5.2, 1.1], [2, 5.2, 1.0]
    ].forEach(([tx, tz, ts]) => dioramaGroup.add(createPineTree(tx, tz, ts)));

    // 6. Fountains
    const fountainBaseMat = new THREE.MeshStandardMaterial({ color: 0x7da4b3, roughness: 0.4 });
    const waterMat = new THREE.MeshStandardMaterial({
      color: new THREE.Color(fountainColor),
      roughness: 0.1,
      transparent: true,
      opacity: 0.85
    });

    const createFountain = (x, z) => {
      const fountainGroup = new THREE.Group();
      fountainGroup.position.set(x, 0.6, z);

      const basin = new THREE.Mesh(new THREE.CylinderGeometry(1.2, 1.0, 0.5, 16), fountainBaseMat);
      basin.position.y = 0.25;
      basin.castShadow = true;
      fountainGroup.add(basin);

      const pool = new THREE.Mesh(new THREE.CylinderGeometry(1.05, 1.05, 0.05, 16), waterMat);
      pool.position.y = 0.48;
      fountainMeshRef.current = pool;
      fountainGroup.add(pool);

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

    // --- LOAD CLASSMATE'S CUSTOM 3D STL MODEL ---
    const stlLoader = new STLLoader();
    stlLoader.load(
      '/fountainFinal.stl',
      (geometry) => {
        geometry.computeVertexNormals();
        geometry.center();

        geometry.computeBoundingBox();
        const bbox = geometry.boundingBox;
        const sizeY = bbox.max.y - bbox.min.y;
        const desiredHeight = 2.4;
        const scaleFactor = desiredHeight / (sizeY || 1);

        const stlMat = new THREE.MeshStandardMaterial({
          color: new THREE.Color(fountainColor),
          roughness: 0.3,
          metalness: 0.2
        });

        const stlMesh = new THREE.Mesh(geometry, stlMat);
        stlMesh.rotation.x = -Math.PI / 2;
        stlMesh.scale.set(scaleFactor, scaleFactor, scaleFactor);
        stlMesh.position.set(0, 1.8, 0);
        stlMesh.castShadow = true;
        stlMesh.receiveShadow = true;

        customStlMeshRef.current = stlMesh;
        dioramaGroup.add(stlMesh);
      },
      undefined,
      (err) => {
        console.log('STL load notice:', err);
      }
    );

    // Particles
    const particles = [];
    const particleGeo = new THREE.SphereGeometry(0.06, 6, 6);
    const particleMat = new THREE.MeshBasicMaterial({
      color: new THREE.Color(fountainColor),
      transparent: true,
      opacity: 0.85
    });

    [{ x: 0, z: -3.8 }, { x: 0, z: 3.8 }, { x: 0, z: 0 }].forEach((fc) => {
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
          vz: (Math.random() - 0.5) * 0.02
        };
        dioramaGroup.add(particle);
        particles.push(particle);
      }
    });
    fountainParticlesRef.current = particles;

    // 7. Small Peg Figures
    [0xf4d068, 0xef7a7a, 0x7ebcf0, 0xb88ce8, 0xfa9e5c, 0x78cb96].forEach((color, i) => {
      const angle = (i / 6) * Math.PI * 2;
      const fx = Math.cos(angle) * 1.8;
      const fz = Math.sin(angle) * 1.8;
      const figGroup = new THREE.Group();
      figGroup.position.set(fx, 0.6, fz);
      const figMat = new THREE.MeshStandardMaterial({ color, roughness: 0.4 });
      const body = new THREE.Mesh(new THREE.CylinderGeometry(0.12, 0.18, 0.55, 12), figMat);
      body.position.y = 0.285;
      figGroup.add(body);
      const head = new THREE.Mesh(new THREE.SphereGeometry(0.14, 12, 12), figMat);
      head.position.y = 0.65;
      figGroup.add(head);
      dioramaGroup.add(figGroup);
    });

    // 8. Lamp Posts
    const lampMat = new THREE.MeshStandardMaterial({ color: 0x3d3b38 });
    const bulbMat = new THREE.MeshBasicMaterial({ color: 0xfffae0 });
    [
      [-2.8, -1.6], [2.8, -1.6], [-2.8, 1.6], [2.8, 1.6],
      [-1.6, -2.8], [1.6, -2.8], [-1.6, 2.8], [1.6, 2.8]
    ].forEach(([lx, lz]) => {
      const lamp = new THREE.Group();
      lamp.position.set(lx, 0.6, lz);
      const pole = new THREE.Mesh(new THREE.CylinderGeometry(0.04, 0.06, 0.7, 8), lampMat);
      pole.position.y = 0.35;
      lamp.add(pole);
      const bulb = new THREE.Mesh(new THREE.SphereGeometry(0.12, 10, 10), bulbMat);
      bulb.position.y = 0.75;
      lamp.add(bulb);

      const pLight = new THREE.PointLight(0xffebaa, (brightness / 100) * 1.5, 4.5);
      pLight.position.set(lx, 1.4, lz);
      pointLightsGroup.add(pLight);
      dioramaGroup.add(lamp);
    });

    // 9. Glass Container
    const glassWallMat = new THREE.MeshPhysicalMaterial({
      color: 0xffffff,
      transmission: 0.92,
      opacity: 1,
      transparent: true,
      roughness: 0.05,
      ior: 1.4,
      thickness: 0.3
    });
    const glassWidth = 13.8;
    const glassHeight = 8;
    const glassFront = new THREE.Mesh(new THREE.BoxGeometry(glassWidth, glassHeight, 0.08), glassWallMat);
    glassFront.position.set(0, glassHeight / 2 + 0.6, glassWidth / 2);
    dioramaGroup.add(glassFront);
    const glassBack = new THREE.Mesh(new THREE.BoxGeometry(glassWidth, glassHeight, 0.08), glassWallMat);
    glassBack.position.set(0, glassHeight / 2 + 0.6, -glassWidth / 2);
    dioramaGroup.add(glassBack);
    const glassLeft = new THREE.Mesh(new THREE.BoxGeometry(0.08, glassHeight, glassWidth), glassWallMat);
    glassLeft.position.set(-glassWidth / 2, glassHeight / 2 + 0.6, 0);
    dioramaGroup.add(glassLeft);
    const glassRight = new THREE.Mesh(new THREE.BoxGeometry(0.08, glassHeight, glassWidth), glassWallMat);
    glassRight.position.set(glassWidth / 2, glassHeight / 2 + 0.6, 0);
    dioramaGroup.add(glassRight);

    // Raycaster interaction
    const raycaster = new THREE.Raycaster();
    const mouse = new THREE.Vector2();

    const handleClick = (e) => {
      const rect = renderer.domElement.getBoundingClientRect();
      mouse.x = ((e.clientX - rect.left) / rect.width) * 2 - 1;
      mouse.y = -((e.clientY - rect.top) / rect.height) * 2 + 1;
      raycaster.setFromCamera(mouse, camera);
      const intersects = raycaster.intersectObjects(dioramaGroup.children, true);
      if (intersects.length > 0) {
        onGateClick();
        if (gateRef.current) {
          const start = gateRef.current.rotation.y;
          let s = 0;
          const anim = setInterval(() => {
            s++;
            gateRef.current.rotation.y = start + Math.sin(s * 0.3) * 0.25;
            if (s > 20) {
              clearInterval(anim);
              gateRef.current.rotation.y = start;
            }
          }, 30);
        }
      }
    };
    renderer.domElement.addEventListener('click', handleClick);

    const handleResize = () => {
      if (!container) return;
      const w = container.clientWidth;
      const h = container.clientHeight;
      camera.aspect = w / h;
      camera.updateProjectionMatrix();
      renderer.setSize(w, h);
    };
    window.addEventListener('resize', handleResize);

    let frameId;
    const startAnimTime = performance.now();

    const animate = () => {
      frameId = requestAnimationFrame(animate);
      const time = (performance.now() - startAnimTime) * 0.001;

      dioramaGroup.rotation.y = Math.sin(time * 0.15) * 0.05;

      // Particle physics & pattern modulation
      if (fountainParticlesRef.current) {
        let patternMult = 1;
        if (fountainPattern === 'Pulsing') patternMult = (Math.sin(time * 3) + 1.2) * 0.6;
        else if (fountainPattern === 'Wave') patternMult = (Math.sin(time * 5) + 1.5) * 0.5;
        else if (fountainPattern === 'Alternating') patternMult = (Math.cos(time * 2) + 1.2) * 0.6;

        const speed = (fountainStrength / 100) * (fountainOn ? 1 : 0) * patternMult;

        fountainParticlesRef.current.forEach((p) => {
          if (!fountainOn) {
            p.visible = false;
            return;
          }
          p.visible = true;
          p.position.y += p.userData.vy * speed;
          p.position.x += p.userData.vx * speed;
          p.position.z += p.userData.vz * speed;
          p.userData.vy -= 0.0015;

          if (p.position.y < 1.1) {
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

    return () => {
      window.removeEventListener('resize', handleResize);
      if (renderer.domElement) renderer.domElement.removeEventListener('click', handleClick);
      cancelAnimationFrame(frameId);
      renderer.dispose();
    };
  }, []);

  // Update light intensity
  useEffect(() => {
    if (!lightsGroupRef.current) return;
    const target = lightsOn ? (brightness / 100) * 1.5 : 0;
    lightsGroupRef.current.children.forEach((l) => (l.intensity = target));
  }, [lightsOn, brightness]);

  // Update Plaza Color
  useEffect(() => {
    if (plazaMeshRef.current) {
      plazaMeshRef.current.material.color.set(circleColor);
    }
  }, [circleColor]);

  // Update Fountain Water Color & Custom STL Mesh color
  useEffect(() => {
    if (fountainMeshRef.current) {
      fountainMeshRef.current.material.color.set(fountainColor);
    }
    if (customStlMeshRef.current) {
      customStlMeshRef.current.material.color.set(fountainColor);
    }
  }, [fountainColor]);

  return (
    <div className="diorama-preview-card">
      <div className="card-top-tag">YOUR LITTLE WORLD</div>
      <div className="canvas-wrapper" ref={mountRef}></div>
      <div className="canvas-overlay-instruction">Live 3D preview · drag to rotate</div>
    </div>
  );
}
