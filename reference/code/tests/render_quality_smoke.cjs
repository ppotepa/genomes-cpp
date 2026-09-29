'use strict';
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

const listeners = new Map();
const values = new Map();
const window = {
  addEventListener(type, fn) { if (!listeners.has(type)) listeners.set(type, new Set()); listeners.get(type).add(fn); },
  removeEventListener(type, fn) { listeners.get(type)?.delete(fn); },
  dispatchEvent(event) { for (const fn of listeners.get(event.type) || []) fn(event); }
};
class CustomEvent { constructor(type, init = {}) { this.type = type; this.detail = init.detail; } }
const context = {
  window, CustomEvent,
  localStorage: { getItem: key => values.get(key) ?? null, setItem: (key, value) => values.set(key, value) },
  Object, Math, Number, RangeError
};
vm.runInNewContext(fs.readFileSync(path.join(__dirname, '..', 'js', 'config.js'), 'utf8'), context);
const quality = window.RTS.RenderQuality;

assert.equal(quality.get(), 'balanced');
assert.equal(quality.ratio(1920, 1080, 2, 'map'), 1.5);
assert.equal(quality.ratio(1920, 1080, 2, 'preview'), 1.3888888888888888);
let changes = 0;
const remove = quality.onChange(profile => { assert.equal(profile, 'low'); changes++; });
quality.set('low');
assert.equal(changes, 1);
assert.equal(quality.get(), 'low');
assert.equal(quality.ratio(4000, 3000, 3, 'map'), 0.5);
assert.equal(quality.ratio(4000, 3000, 3, 'preview'), Math.sqrt(2000000 / 12000000));
assert.throws(() => quality.set('ultra'), RangeError);
remove();
window.dispatchEvent(new CustomEvent('genomes-render-quality-change', { detail: 'high' }));
assert.equal(changes, 1);

for (const [file, needle] of [
  ['js/game.js', "RenderQuality.ratio(innerWidth,innerHeight,window.devicePixelRatio||1,'map')"],
  ['js/ui/unitPreview.js', "RenderQuality.ratio(r.width,r.height,window.devicePixelRatio||1,'preview')"],
  ['js/environment/environmentLab.js', "RenderQuality.ratio(w,h,devicePixelRatio||1,'preview')"]
]) assert.ok(fs.readFileSync(path.join(__dirname, '..', file), 'utf8').includes(needle), `${file} must use the shared bounded profile`);
assert.match(fs.readFileSync(path.join(__dirname, '..', 'index.html'), 'utf8'), /id="renderQuality"[\s\S]*?value="high"/);
assert.match(fs.readFileSync(path.join(__dirname, '..', 'environment.html'), 'utf8'), /id="renderQuality"[\s\S]*?value="high"/);

console.log('render_quality_smoke: PASS');
