import fs from 'fs';
import path from 'path';
import zlib from 'zlib';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const targetDir = path.resolve(__dirname, '../Diorama/data');
const assetsDir = path.join(targetDir, 'assets');

// Clean orphaned files from previous builds
function cleanOldAssets() {
  const indexPath = path.join(targetDir, 'index.html');
  if (!fs.existsSync(indexPath) || !fs.existsSync(assetsDir)) return;
  const indexHtml = fs.readFileSync(indexPath, 'utf-8');

  const files = fs.readdirSync(assetsDir);
  for (const file of files) {
    const baseName = file.replace(/\.gz$/, '');
    if (!indexHtml.includes(baseName) && !file.startsWith('logo')) {
      const p = path.join(assetsDir, file);
      try {
        fs.unlinkSync(p);
        console.log(`Cleaned stale build file: ${file}`);
      } catch (err) {
        // ignore
      }
    }
  }
}

function compressDirectory(dir) {
  if (!fs.existsSync(dir)) return;
  const entries = fs.readdirSync(dir, { withFileTypes: true });

  for (const entry of entries) {
    const fullPath = path.join(dir, entry.name);
    if (entry.isDirectory()) {
      compressDirectory(fullPath);
    } else if (entry.isFile()) {
      if (entry.name.endsWith('.gz')) continue;
      
      const fileBuffer = fs.readFileSync(fullPath);
      const gzippedBuffer = zlib.gzipSync(fileBuffer, { level: 9 });
      const gzPath = fullPath + '.gz';
      
      fs.writeFileSync(gzPath, gzippedBuffer);
      console.log(`Compressed: ${entry.name} (${(fileBuffer.length / 1024).toFixed(1)} KB -> ${(gzippedBuffer.length / 1024).toFixed(1)} KB)`);
    }
  }
}

console.log(`Cleaning old assets in ${assetsDir}...`);
cleanOldAssets();
console.log(`Gzipping built files in ${targetDir}...`);
compressDirectory(targetDir);
console.log('Compression complete!');
