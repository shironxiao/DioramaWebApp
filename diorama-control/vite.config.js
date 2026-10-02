import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';
import fs from 'fs';
import path from 'path';
import zlib from 'zlib';

function gzipPlugin() {
  return {
    name: 'gzip-dist-files',
    closeBundle() {
      const outDir = 'D:/Diorama/data'; // Specify the output directory here babaguhin mo ito depende sa directory mo nun arduino mo
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
    outDir: 'D:/Diorama/data',
    emptyOutDir: true
  },
  server: {
    watch: {
      ignored: ['**/*.stl', '**/*.glb', '**/*.gltf']
    }
  }
});
