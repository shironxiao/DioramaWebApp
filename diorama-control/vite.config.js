import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';
import fs from 'fs';
import path from 'path';
import zlib from 'zlib';

// Pointing directly to your permanent Arduino Diorama sketch's 'data' folder
const arduinoDataPath = 'C:/Users/Augorio Miguel/OneDrive/Documents/Arduino/Diorama/data';

function gzipPlugin() {
  return {
    name: 'gzip-dist-files',
    closeBundle() {
      // Uses the path defined above
      const outDir = arduinoDataPath; 
      
      function compressDir(dir) {
        if (!fs.existsSync(dir)) return;
        const files = fs.readdirSync(dir);
        for (const file of files) {
          const filePath = path.join(dir, file);
          const stat = fs.statSync(filePath);
          if (stat.isDirectory()) {
            compressDir(filePath);
          } else if (
            (file.endsWith('.js') || file.endsWith('.css') || file.endsWith('.html') || file.endsWith('.stl') || file.endsWith('.svg')) &&
            !file.endsWith('.gz')
          ) {
            const content = fs.readFileSync(filePath);
            const gzipped = zlib.gzipSync(content, { level: 9 });
            fs.writeFileSync(filePath + '.gz', gzipped);
            // Delete the original uncompressed file — ESP32 only needs the .gz version
            fs.unlinkSync(filePath);
          }
        }
      }
      compressDir(outDir);
    }
  };
}

// https://vite.dev/config/
export default defineConfig({
  base: './',
  plugins: [react(), gzipPlugin()],
  build: {
    // Vite will automatically build and output straight into the Arduino data folder
    outDir: arduinoDataPath, 
    emptyOutDir: true // This will cleanly wipe the old build inside the data folder before putting in the new one
  },
  server: {
    watch: {
      ignored: ['**/*.stl', '**/*.glb', '**/*.gltf']
    }
  }
});