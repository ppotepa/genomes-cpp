(function () {
  'use strict';
  const $ = id => document.getElementById(id), view = BuildingLab.createView();
  const generator = new RTS.ProceduralBuildingGenerator();
  let building = null, request = 0;
  const initialDamage = new WeakMap();
  Object.defineProperty(BuildingLab, 'currentBuilding', {get:() => building});
  const forms = BuildingLab.createForms(() => building), floorPlan = BuildingLab.createFloorPlan();
  const demo = RTS.DestructionDemo.button($('generate').parentElement, () => building ? {
    renderer:view.renderer, controls:view.controls, target:RTS.DestructionAdapters.building(building),
    resume:() => { view.resize(); applyVisibility(); refresh(); }
  } : null);
  demo.id = 'destructionDemo'; demo.disabled = true;
  function applyInitialDamage(target, percent) {
    initialDamage.set(target, percent);
    if (!percent) return;
    // Stable spatial distribution, weighted by surface area, with a partial final component.
    const rank = id => { let h = target.plan.spec.seed; for (const c of id) h = Math.imul(h ^ c.charCodeAt(0), 16777619) >>> 0; return h; };
    const components = target.components.slice().sort((a,b) => rank(a.id)-rank(b.id));
    const total = components.reduce((sum,c) => sum + (c.surfaceArea || 1),0);
    let remaining = total * percent / 100;
    for (const c of components) {
      const area = c.surfaceArea || 1, loss = Math.min(area,remaining);
      c.health = c.maxHealth * (1-loss/area); c.destroyed = c.health <= 1e-8;
      remaining = Math.max(0,remaining-loss);
    }
    Object.assign(target.manifest.damage, {damagePercent:percent,achievedPercent:percent,destroyed:components.filter(c => c.destroyed).length,total:components.length});
  }
  function diagnose(error, options) {
    const detail = [error.message, error.code && 'Code: ' + error.code, error.stage && 'Stage: ' + error.stage,
      error.failures?.length && 'Failures: ' + JSON.stringify(error.failures),
      'Requested seed: ' + (options?.seed ?? $('seed').value),
      building && 'Showing previous successful seed: ' + building.plan.spec.seed].filter(Boolean);
    $('error').textContent = detail.join('\n'); $('error').hidden = false;
  }
  function applyVisibility() {
    if (!building) return;
    building.setCutaway($('cutaway').checked); building.setRoofVisible($('showRoof').checked); building.setNavVisible($('showNav').checked);
    const floor = Number($('floorPlanFloor').value);
    building.setVisibleFloor($('isolateFloor').checked ? floor : null);
    floorPlan.draw(building, floor);
  }
  function refresh() {
    if (!building) return;
    const p = building.plan, m = building.exportManifest(), s = p.spec;
    const readouts = {
      building:s.presetId, structure:s.structuralSystem + ' / ' + s.material,
      style:[s.facade?.style,s.facade?.windowType,s.facade?.doorType].filter(Boolean).join(' / '),
      size:s.width.toFixed(1) + ' × ' + s.depth.toFixed(1) + ' m',
      floor:p.storeys.length + ' × ' + s.storeys.floorHeight.toFixed(2) + ' m',
      roof:s.roofFamily + ' / ' + (s.roofCovering || 'AUTO'), material:s.material,
      window:p.openings.filter(x => x.kind === 'window').length, entrance:p.openings.filter(x => x.kind === 'door').length,
      stair:p.stairs.length, space:p.rooms.length + ' / ' + p.connections.length, wall:p.walls.length + ' / ' + p.openings.length,
      damage:(m.damage.achievedPercent ?? m.damage.damagePercent ?? 0) + '% / ' + (m.damage.destroyed || 0) + ' destroyed',
      component:building.components.filter(c => !c.destroyed).length + ' / ' + building.components.length
    };
    for (const [id, value] of Object.entries(readouts)) $(id + 'Readout').textContent = String(value);
    $('manifest').textContent = JSON.stringify(m, null, 2);
    $('generationStatus').textContent = 'Generated seed ' + s.seed + ' · ' + s.footprintFamily + ' · ' + p.features.length + ' features';
    floorPlan.draw(building, Number($('floorPlanFloor').value));
  }
  async function generate() {
    if (RTS.DestructionDemo.active) return;
    const serial = ++request, previous = building; let candidate, options;
    $('generationStatus').textContent = 'Generating…'; $('viewport').setAttribute('aria-busy','true');
    try {
      options = forms.options(); await RTS.Buildings.initializeBuildingBackends();
      if (serial !== request) return;
      candidate = generator.generate(options); applyInitialDamage(candidate, options.damagePercent || 0);
      view.add(candidate.root); view.fit(candidate);
      building = candidate; floorPlan.sync(building); applyVisibility(); refresh(); forms.captureLocks(building.plan.spec);
      if (previous) { view.remove(previous.root); previous.dispose(); }
      $('error').hidden = true; $('error').textContent = ''; demo.disabled = false;
      $('viewport').dataset.generation = String(serial);
    } catch (error) {
      building = previous;
      if (candidate) { view.remove(candidate.root); candidate.dispose(); }
      if (building) { floorPlan.sync(building); applyVisibility(); view.fit(building); refresh(); }
      diagnose(error, options); $('generationStatus').textContent = 'Generation failed' + (building ? ' · retained seed ' + building.plan.spec.seed : '');
    } finally { if (serial === request) $('viewport').setAttribute('aria-busy','false'); }
  }
  function randomize() {
    const value = new Uint32Array(1); crypto.getRandomValues(value);
    $('seed').value = String(value[0]); forms.constrain(); generate();
  }
  function damage(reset) {
    if (!building) return;
    if (reset) { building.resetDamage(); applyInitialDamage(building, initialDamage.get(building) || 0); }
    else building.breachFront();
    applyVisibility(); refresh();
  }
  $('generate').addEventListener('click', generate); $('randomize').addEventListener('click', randomize);
  for (const id of ['showRoof','showNav','cutaway','floorPlanFloor','isolateFloor']) $(id).addEventListener('change', applyVisibility);
  $('breach').addEventListener('click', () => damage(false)); $('resetDamage').addEventListener('click', () => damage(true));
  $('copyManifest').addEventListener('click', async () => {
    if (!building) return;
    try { await navigator.clipboard.writeText(JSON.stringify(building.exportManifest(), null, 2)); $('copyStatus').textContent = 'Manifest copied.'; }
    catch { $('copyStatus').textContent = 'Clipboard unavailable. Select and copy the JSON below.'; const range = document.createRange(); range.selectNodeContents($('manifest')); const selection = getSelection(); selection.removeAllRanges(); selection.addRange(range); }
  });
  for (const id of ['seed','presetId','roomProgram','shape','roof','roofCovering','style','windowType','doorType','stairType','structure','material','floors','width','depth','floorHeight','showFurniture','featureChimney','featurePorch','featureBalcony','featureDormer','damagePercent']) $(id).addEventListener('change', generate);
  window.addEventListener('keydown', event => {
    if (RTS.DestructionDemo.active || event.repeat || event.ctrlKey || event.metaKey || event.altKey || event.target.closest('input,select,textarea,button,[contenteditable="true"]')) return;
    const actions = {r:generate,n:randomize,c:() => { $('cutaway').checked = !$('cutaway').checked; applyVisibility(); },b:() => damage(false)};
    if (actions[event.key.toLowerCase()]) { event.preventDefault(); actions[event.key.toLowerCase()](); }
  });
  generate();
})();
