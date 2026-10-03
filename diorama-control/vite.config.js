import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';

// Arduino ESP32 LittleFS data folder
const arduinoDataPath =
  'C:\\Users\\Augorio Miguel\\OneDrive\\Documents\\Arduino\\Diorama\\data';

export default defineConfig({
  // Important for files hosted directly by ESP32
  base: './',

  plugins: [react()],

  build: {
    // Build directly into Arduino's data folder
    outDir: arduinoDataPath,

    // Allow Vite to clean the previous build
    emptyOutDir: true,

    // Keep normal files for now.
    // We can add gzip compression later after the basic setup works.
  },

  server: {
    watch: {
      // Prevent Vite from constantly watching large 3D model files
      ignored: [
        '**/*.stl',
        '**/*.glb',
        '**/*.gltf',
      ],
    },
  },
});