(function () {
  'use strict';
  const R = window.RTS = window.RTS || {};
  R.Config = Object.freeze({
    VERSION: '0.22.0', GENERATOR_VERSION: 'building-6.0.0',
    MAP_SIZE: 2000, HALF_MAP: 1000, SECTOR_SIZE: 5, BIG_SECTOR_SIZE: 100,
    TERRAIN_SEGMENTS: 300, TERRAIN_SEED: 1337,
    FIXED_DT: 1 / 60, MAX_STEPS: 8, MAX_FRAME_DT: 0.25,
    SIDES: Object.freeze({ SIDE_A: 0x596745, SIDE_B: 0x475f78 }),
    CAMERA: Object.freeze({ FOV: 48, NEAR: 0.12, FAR: 6500, MIN_DISTANCE: 2, MAX_DISTANCE: 3200 }),
    WORLD_BUILDING_RUNTIME: Object.freeze({
      proxyEnabled: true, lazyDamage: true, chunkSize: 2, farDistance: 220, worldDistance: 70,
      buildingSectorSize: 64, maxActiveDamageChunksPerBuilding: 96, maxDetailedBuildings: 8
    }),
    INFANTRY: Object.freeze({
      MIN_HEIGHT: 1.60, MAX_HEIGHT: 1.95,
      WALK_BASE_SPEED: 1.4, RUN_BASE_SPEED: 4.2, PRONE_BASE_SPEED: 0.55, CROUCH_BASE_SPEED: 0.75,
      MIN_SPEED_MULTIPLIER: 0.88, MAX_SPEED_MULTIPLIER: 1.12,
      WALK_CYCLE_HEIGHTS: 0.72, RUN_CYCLE_HEIGHTS: 1.20, CRAWL_CYCLE_HEIGHTS: 0.34,
      WALK_DUTY: 0.62, RUN_DUTY: 0.42,
      CROUCH_CYCLE_LEGS: 0.75, CROUCH_DUTY: 0.70,
      CRAWL_CYCLE_LEGS: 0.56, CRAWL_FOOT_DUTY: 0.62, CRAWL_HAND_DUTY: 0.66,
      HEIGHT_TOLERANCE: 0.005, CONTACT_CLEARANCE: 0.0015
    })
  });
  // Render profiles cap framebuffer work without changing procedural content or LOD.
  const renderProfiles = Object.freeze({
    low: Object.freeze({ label: 'Oszczędny', mapRatio: 1, mapPixels: 3000000, previewRatio: 1, previewPixels: 2000000 }),
    balanced: Object.freeze({ label: 'Zrównoważony', mapRatio: 1.5, mapPixels: 8000000, previewRatio: 1.5, previewPixels: 4000000 }),
    high: Object.freeze({ label: 'Wysoka jakość', mapRatio: 2, mapPixels: 12000000, previewRatio: 2, previewPixels: 6000000 })
  });
  const renderQualityKey = 'genomes.renderQuality';
  const renderQualityEvent = 'genomes-render-quality-change';
  function readRenderProfile() {
    try { const value = localStorage.getItem(renderQualityKey); return Object.hasOwn(renderProfiles, value) ? value : 'balanced'; }
    catch (_) { return 'balanced'; }
  }
  R.RenderQuality = Object.freeze({
    profiles: renderProfiles,
    get: readRenderProfile,
    set(value) {
      if (!Object.hasOwn(renderProfiles, value)) throw new RangeError('Unknown render quality profile');
      try { localStorage.setItem(renderQualityKey, value); } catch (_) { /* Storage may be disabled. */ }
      window.dispatchEvent(new CustomEvent(renderQualityEvent, { detail: value }));
      return value;
    },
    ratio(width, height, deviceRatio, target = 'map') {
      const p = renderProfiles[readRenderProfile()];
      const maxRatio = target === 'preview' ? p.previewRatio : p.mapRatio;
      const maxPixels = target === 'preview' ? p.previewPixels : p.mapPixels;
      const pixels = Math.max(1, Number(width) * Number(height));
      const dpr = Number.isFinite(Number(deviceRatio)) && Number(deviceRatio) > 0 ? Number(deviceRatio) : 1;
      return Math.min(dpr, maxRatio, Math.sqrt(maxPixels / pixels));
    },
    onChange(callback) {
      const listener = event => callback(event.detail || readRenderProfile());
      const storageListener = event => { if (event.key === renderQualityKey) callback(readRenderProfile()); };
      window.addEventListener(renderQualityEvent, listener);
      window.addEventListener('storage', storageListener);
      return () => { window.removeEventListener(renderQualityEvent, listener); window.removeEventListener('storage', storageListener); };
    }
  });
  R.UnitCategory = Object.freeze({ VEHICULAR: 'VEHICULAR', NON_VEHICULAR: 'NON_VEHICULAR' });
  R.UnitType = Object.freeze({ INFANTRY: 'INFANTRY' });
  R.AnimationState = Object.freeze({ REST:'REST', IDLE:'IDLE', WALK:'WALK', RUN:'RUN', CROUCH:'CROUCH', CROUCH_WALK:'CROUCH_WALK', SITTING:'SITTING', PRONE:'PRONE', PRONE_MOVE:'PRONE_MOVE' });
  R.ExpressionState = Object.freeze({ NEUTRAL:'NEUTRAL', ALERT:'ALERT', FEAR:'FEAR', ANGER:'ANGER', PAIN:'PAIN', FATIGUE:'FATIGUE', EYES_CLOSED:'EYES_CLOSED' });
  R.Math = {
    clamp: (v, lo, hi) => Math.max(lo, Math.min(hi, v)),
    mix: (a, b, t) => a + (b - a) * t,
    smooth: t => { t = Math.max(0, Math.min(1, t)); return t * t * (3 - 2 * t); },
    angle: a => Math.atan2(Math.sin(a), Math.cos(a))
  };
})();
