import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const dataDir = path.resolve(__dirname, '../Diorama/data');
const outputFile = path.resolve(__dirname, '../Diorama/webapp_embed.h');

function toCArray(buffer) {
  const hex = [];
  for (let i = 0; i < buffer.length; i++) {
    hex.push('0x' + buffer[i].toString(16).padStart(2, '0'));
  }
  const lines = [];
  for (let i = 0; i < hex.length; i += 16) {
    lines.push('  ' + hex.slice(i, i + 16).join(', '));
  }
  return lines.join(',\n');
}

function getMimeType(filename) {
  if (filename.endsWith('.html') || filename.endsWith('.html.gz')) return 'text/html';
  if (filename.endsWith('.css') || filename.endsWith('.css.gz')) return 'text/css';
  if (filename.endsWith('.js') || filename.endsWith('.js.gz')) return 'application/javascript';
  if (filename.endsWith('.svg') || filename.endsWith('.svg.gz')) return 'image/svg+xml';
  if (filename.endsWith('.png') || filename.endsWith('.png.gz')) return 'image/png';
  if (filename.endsWith('.stl') || filename.endsWith('.stl.gz')) return 'model/stl';
  if (filename.endsWith('.ico') || filename.endsWith('.ico.gz')) return 'image/x-icon';
  if (filename.endsWith('.json') || filename.endsWith('.json.gz')) return 'application/json';
  return 'text/plain';
}

function scanFiles(dir, base = '') {
  let results = [];
  if (!fs.existsSync(dir)) return results;
  const entries = fs.readdirSync(dir, { withFileTypes: true });
  for (const entry of entries) {
    const fullPath = path.join(dir, entry.name);
    const relPath = path.join(base, entry.name).replace(/\\/g, '/');
    if (entry.isDirectory()) {
      results = results.concat(scanFiles(fullPath, relPath));
    } else if (entry.isFile() && entry.name.endsWith('.gz')) {
      results.push({ fullPath, relPath: '/' + relPath.replace(/\.gz$/, ''), gzName: entry.name });
    }
  }
  return results;
}

const files = scanFiles(dataDir);

let headerContent = `// Auto-generated web app bundle for ESP32
#pragma once
#include <Arduino.h>
#include <WebServer.h>

`;

let fileEntries = [];

files.forEach((file, index) => {
  const varName = `WEB_ASSET_${index}`;
  const data = fs.readFileSync(file.fullPath);
  const mime = getMimeType(file.relPath);

  headerContent += `// ${file.relPath} (${(data.length / 1024).toFixed(1)} KB gzipped)\n`;
  headerContent += `const uint32_t ${varName}_LEN = ${data.length};\n`;
  headerContent += `const uint8_t ${varName}[] PROGMEM = {\n${toCArray(data)}\n};\n\n`;

  fileEntries.push({
    uri: file.relPath,
    varName,
    mime
  });
});

headerContent += `// Check and serve embedded static files
inline bool serveEmbeddedWebApp(WebServer &srv) {
  String uri = srv.uri();
  if (uri == "/") uri = "/index.html";

`;

fileEntries.forEach((entry) => {
  headerContent += `  if (uri == "${entry.uri}") {\n`;
  headerContent += `    srv.sendHeader("Content-Encoding", "gzip");\n`;
  headerContent += `    srv.send_P(200, "${entry.mime}", (const char*)${entry.varName}, ${entry.varName}_LEN);\n`;
  headerContent += `    return true;\n`;
  headerContent += `  }\n`;
});

// SPA fallback: serve index.html for non-api routes
const indexEntry = fileEntries.find(e => e.uri === '/index.html');
if (indexEntry) {
  headerContent += `
  // SPA fallback for client-side routing
  if (!uri.startsWith("/api/")) {
    srv.sendHeader("Content-Encoding", "gzip");
    srv.send_P(200, "text/html", (const char*)${indexEntry.varName}, ${indexEntry.varName}_LEN);\n`;
  headerContent += `    return true;\n  }\n`;
}

headerContent += `
  return false;
}
`;

fs.writeFileSync(outputFile, headerContent);
console.log(`Generated ${outputFile} with ${files.length} embedded web assets.`);
