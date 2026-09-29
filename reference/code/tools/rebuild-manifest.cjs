'use strict';
// Source-package inventory; excludes repository metadata and generated caches.
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const root = path.resolve(__dirname, '..');
const excluded = new Set(['.git', '.codegraph', 'node_modules', '__pycache__', '.DS_Store', 'Thumbs.db']);
function inventory(directory = '') {
  return fs.readdirSync(path.join(root, directory), { withFileTypes: true }).flatMap(entry => {
    const file = directory ? `${directory}/${entry.name}` : entry.name;
    if (excluded.has(entry.name) || file === 'tests/out' || file === 'manifest.sha256' || file.endsWith('.log') || entry.isSymbolicLink()) return [];
    return entry.isDirectory() ? inventory(file) : [file];
  });
}
const files = inventory().sort();
const lines = files.map(file => `${crypto.createHash('sha256').update(fs.readFileSync(path.join(root, file))).digest('hex')}  ${file}`);
fs.writeFileSync(path.join(root, 'manifest.sha256'), lines.join('\n') + '\n');
console.log(`Manifest rebuilt: ${files.length} files.`);
