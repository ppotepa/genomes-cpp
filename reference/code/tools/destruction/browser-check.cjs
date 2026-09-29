// Real-browser acceptance: current model identity, geometry, pixels, and editor lifecycle.
const fs = require('node:fs'), path = require('node:path'), http = require('node:http');
const assert = require('node:assert/strict');
const {chromium} = require(process.env.DESTRUCTION_PLAYWRIGHT || 'playwright');
const root = path.resolve(__dirname, '../..');
const output = path.resolve(process.env.BUILDING_LAB_ARTIFACTS || path.join(require('node:os').tmpdir(), 'genomes-building-lab-browser'));
const results = [], errors = [], external = [], captured = new Set();
let browser, server, page;
async function check(name, fn) {
  try { const detail = await fn(); results.push({name, passed:true, detail}); console.log('PASS ' + name); }
  catch (error) { results.push({name, passed:false, error:error.message}); console.error('FAIL ' + name + ': ' + error.message); }
}
async function settle() {
  await page.waitForFunction(() => document.querySelector('#viewport').getAttribute('aria-busy') === 'false');
  await page.evaluate(() => new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve))));
}
async function configure(values) {
  // Batch form values, then use the actual Generate button; do not call the generator directly.
  const before = await page.locator('#viewport').getAttribute('data-generation');
  await page.evaluate(values => {
    const preset = document.getElementById('presetId');
    if (values.presetId && preset.value !== values.presetId) { preset.value = values.presetId; preset.dispatchEvent(new Event('change')); }
    for (const [id,value] of Object.entries(values)) {
      const el = document.getElementById(id);
      if (el.type === 'checkbox') el.checked = value;
      else { el.value = String(value); if (el.value !== String(value)) throw Error('Unavailable option ' + id + '=' + value); }
    }
  }, values);
  await page.click('#generate'); await settle();
  const state = await page.evaluate(() => ({error:document.querySelector('#error').hidden ? null : document.querySelector('#error').textContent, generation:document.querySelector('#viewport').dataset.generation, spec:BuildingLab.currentBuilding?.plan.spec}));
  assert.equal(state.error, null, state.error); assert.notEqual(state.generation, before, 'The last manifest cannot stand in for a new generation');
  if (values.seed !== undefined) assert.equal(state.spec.seed, Number(values.seed));
  return state.spec;
}
async function snapshot(name) {
  await settle(); const file = path.join(output, name + '.png');
  await page.locator('#viewport').screenshot({path:file}); captured.add(name); return file;
}
async function main() {
  fs.mkdirSync(output, {recursive:true});
  // Preserve the preceding visual capture for inspection across renderer changes.
  for (const name of ['visual-matrix','roof-gable','shape-rect']) {
    const source = path.join(output,name+'.png'), baseline = path.join(output,'baseline-'+name+'.png');
    if (fs.existsSync(source) && !fs.existsSync(baseline)) fs.copyFileSync(source,baseline);
  }
  server = http.createServer((req,res) => {
    let file;
    try { file = path.resolve(root, '.' + decodeURIComponent(new URL(req.url, 'http://localhost').pathname)); }
    catch { res.writeHead(400).end(); return; }
    if (!file.startsWith(root + path.sep)) { res.writeHead(403).end(); return; }
    if (req.url === '/favicon.ico') { res.writeHead(204).end(); return; }
    fs.readFile(file, (error,data) => {
      if (error) { res.writeHead(404).end(); return; }
      res.setHeader('Content-Type', ({'.html':'text/html; charset=utf-8','.js':'text/javascript','.css':'text/css','.wasm':'application/wasm','.json':'application/json'})[path.extname(file)] || 'application/octet-stream');
      res.end(data);
    });
  });
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  const origin = 'http://127.0.0.1:' + server.address().port;
  browser = await chromium.launch({channel:process.env.BUILDING_LAB_BROWSER || 'chrome', headless:true, args:['--use-angle=swiftshader','--disable-gpu','--disable-dev-shm-usage']});
  const context = await browser.newContext({viewport:{width:1440,height:1000}, deviceScaleFactor:1, permissions:['clipboard-read','clipboard-write']});
  page = await context.newPage(); page.setDefaultTimeout(60000);
  page.on('pageerror', e => errors.push(e.message));
  page.on('console', e => { if (e.type() === 'error' || e.type() === 'warning' && /shader.*(error|fail)|compile.*fail|linkProgram/i.test(e.text())) errors.push(e.text()); });
  await page.route('**/*', route => { if (route.request().url().startsWith(origin)) return route.continue(); external.push(route.request().url()); return route.abort(); });
  await page.goto(origin + '/building-lab/index.html');
  await page.waitForFunction(() => window.BuildingLab?.currentBuilding || document.querySelector('#error')?.hidden === false);
  await check('initial local render', async () => {
    await settle(); assert.equal(await page.locator('#error').isVisible(), false, await page.locator('#error').textContent());
    assert.equal(await page.evaluate(() => BuildingLab.currentBuilding.exportManifest().schema), 'rts.building-manifest/6');
    assert.deepEqual(external, []); return snapshot('initial');
  });
  await check('preset constraints and floors 1–5', async () => {
    const actual = await page.evaluate(() => {
      const floors = [...document.querySelector('#floors').options].map(o => o.value);
      const presets = RTS.ProceduralBuildingGenerator.presets;
      const result = [];
      for (const presetId of Object.keys(presets)) {
        const select = document.querySelector('#presetId'); select.value = presetId; select.dispatchEvent(new Event('change'));
        for (const [id,key] of Object.entries({shape:'footprints',roomProgram:'programs',roof:'roofs',stairType:'stairs',structure:'structures'})) {
          const values = [...document.getElementById(id).options].map(o => o.value).filter(v => v !== 'AUTO');
          if (JSON.stringify(values) !== JSON.stringify(presets[presetId][key].filter(v => v !== 'EXPLICIT'))) result.push(presetId + ':' + id);
        }
      }
      return {floors,result};
    });
    assert.deepEqual(actual.floors, ['AUTO','1','2','3','4','5']); assert.deepEqual(actual.result, []); await settle();
  });
  for (const [shape,preset,roof] of [['RECT','FAMILY_HOUSE','GABLE'],['L','OFFICE','FLAT'],['T','FARMHOUSE','GABLE'],['U','OFFICE','FLAT'],['H','OFFICE','FLAT'],['COURTYARD','MULTI_FAMILY','FLAT'],['COMPOSITE','FARMHOUSE','GABLE']]) {
    await check('render ' + shape, async () => {
      const spec = await configure({presetId:preset,seed:2026,shape,roof,floors:'2',width:24,depth:22,roomProgram:'AUTO',stairType:'AUTO',structure:'AUTO'});
      assert.equal(spec.footprintFamily, shape); assert.equal(spec.roofFamily, roof);
      const detail = await page.evaluate(() => {
        const b = BuildingLab.currentBuilding, meshes = b.components.filter(c => c.mesh.isMesh);
        let invalid = 0; for (const c of meshes) for (const value of c.mesh.geometry.attributes.position.array) if (!Number.isFinite(value)) invalid++;
        return {seed:b.manifest.seed, components:meshes.length, visible:meshes.filter(c => c.mesh.visible).length, invalid, navmesh:!!b.plan.navigation.navmesh, roofFaces:b.plan.roof.faces.length};
      });
      assert.equal(detail.seed, 2026); assert(detail.components > 20); assert(detail.visible > 20); assert.equal(detail.invalid,0); assert(detail.navmesh); assert(detail.roofFaces > 0);
      return {...detail, screenshot:await snapshot('shape-' + shape.toLowerCase())};
    });
  }
  await check('roof visibility changes geometry and rendered pixels', async () => {
    await configure({presetId:'FAMILY_HOUSE',shape:'RECT',roof:'GABLE',seed:4201,floors:'2',width:16,depth:14});
    const before = await page.locator('#viewport canvas').screenshot();
    await page.uncheck('#showRoof'); await settle();
    assert.equal(await page.evaluate(() => BuildingLab.currentBuilding.components.filter(c => ['roof','roof-detail','gable'].includes(c.category) && c.mesh.visible).length), 0);
    const after = await page.locator('#viewport canvas').screenshot(); assert(!before.equals(after), 'Roof toggle must change pixels');
    await page.check('#showRoof'); await settle();
    assert(await page.evaluate(() => BuildingLab.currentBuilding.components.some(c => c.category === 'roof' && c.mesh.visible)));
    return snapshot('roof-on');
  });
  await check('facade, joinery and roof covering contract', async () => {
    await configure({seed:4202,style:'RUSTIC',windowType:'SASH',doorType:'DOUBLE',roofCovering:'thatch'});
    const result = await page.evaluate(() => { const b = BuildingLab.currentBuilding; return {facade:b.plan.spec.facade, covering:b.plan.spec.roofCovering, openings:b.plan.openings.length, categories:[...new Set(b.components.map(c => c.category))]}; });
    for (const [key,value] of Object.entries({style:'RUSTIC',windowType:'SASH',doorType:'DOUBLE'})) assert.equal(result.facade[key],value); assert.equal(result.covering,'thatch');
    assert(result.openings > 0); assert(result.categories.some(c => /window|glass|frame/.test(c)), 'Window openings need rendered joinery'); assert(result.categories.some(c => /door/.test(c)), 'Doors need rendered geometry');
    const before = await page.locator('#viewport canvas').screenshot();
    await configure({seed:4202,roofCovering:'metal'}); const after = await page.locator('#viewport canvas').screenshot();
    assert(!before.equals(after), 'Roof covering must change visible rendering'); return snapshot('facade-metal');
  });
  await check('features produce real components', async () => {
    await configure({presetId:'FAMILY_HOUSE',shape:'RECT',roof:'GABLE',width:16,depth:14,floors:'2',seed:42,featureChimney:false,featurePorch:false,featureBalcony:false,featureDormer:false});
    const before = await page.evaluate(() => BuildingLab.currentBuilding.components.length);
    await configure({seed:42,featureChimney:true,featurePorch:true,featureBalcony:true,featureDormer:true});
    const result = await page.evaluate(() => { const b = BuildingLab.currentBuilding; return {count:b.components.length, features:b.plan.features.map(f => ({id:f.id,type:f.type,count:b.components.filter(c => c.sourceId === f.id && c.mesh.geometry?.attributes.position.count > 0).length})), sources:b.components.filter(c => c.sourceId?.startsWith('feature/')).length}; });
    assert(result.count > before); assert.equal(result.features.length,4); assert(result.sources >= 4); assert(result.features.every(f => f.count > 0), JSON.stringify(result)); return {...result,screenshot:await snapshot('features')};
  });
  await check('cutaway, floor plan and isolate survive regeneration', async () => {
    await page.check('#cutaway'); await page.check('#isolateFloor'); await page.selectOption('#floorPlanFloor','0'); await page.check('#showNav');
    await configure({seed:4204});
    const result = await page.evaluate(() => { const b = BuildingLab.currentBuilding; return {cutaway:b.cutaway,floor:b.visibleFloor,nav:b.navOverlay.visible,upper:b.components.filter(c => c.floor > 0 && c.mesh.visible).length,clipped:b.components.some(c => c.wallId && !c.mesh.visible && !c.destroyed),floors:document.querySelector('#floorPlanFloor').options.length}; });
    assert.equal(result.cutaway,true); assert.equal(result.floor,0); assert.equal(result.nav,true); assert.equal(result.upper,0); assert.equal(result.clipped,true); assert.equal(result.floors,2);
    const screenshot = await snapshot('cutaway-floor-1'); await page.uncheck('#cutaway'); await page.uncheck('#isolateFloor'); await page.uncheck('#showNav'); return {result,screenshot};
  });
  await check('floor isolation removes floating upper features', async () => {
    await page.check('#isolateFloor'); await page.selectOption('#floorPlanFloor','0');
    try {
      const floating = await page.evaluate(() => {
        const b = BuildingLab.currentBuilding, ceiling = b.plan.spec.storeys.floorHeight;
        b.root.updateMatrixWorld(true);
        return b.components.filter(c => c.mesh.visible && c.sourceId?.startsWith('feature/') && new THREE.Box3().setFromObject(c.mesh).min.y >= ceiling+.01).map(c => c.id);
      });
      assert.deepEqual(floating, [], 'Upper-storey feature geometry must not float above the isolated floor');
    } finally { await page.uncheck('#isolateFloor'); }
  });
  await check('floor selection and furniture toggle', async () => {
    await page.check('#isolateFloor'); await page.selectOption('#floorPlanFloor','1');
    assert.equal(await page.evaluate(() => BuildingLab.currentBuilding.visibleFloor),1);
    assert.match(await page.locator('#floorPlanSummary').textContent(),/Floor 2/);
    await page.uncheck('#isolateFloor');
    assert(await page.evaluate(() => BuildingLab.currentBuilding.components.some(c => c.category === 'furniture')));
    await page.uncheck('#showFurniture'); await settle();
    assert.equal(await page.evaluate(() => BuildingLab.currentBuilding.components.filter(c => c.category === 'furniture').length),0);
    await page.check('#showFurniture'); await settle();
    assert(await page.evaluate(() => BuildingLab.currentBuilding.components.some(c => c.category === 'furniture')));
  });
  await check('damage refresh and reset', async () => {
    const initial = await page.evaluate(() => BuildingLab.currentBuilding.components.map(c => c.health));
    for (let i=0;i<4;i++) await page.click('#breach');
    const result = await page.evaluate(() => { const b = BuildingLab.currentBuilding; return {health:b.components.map(c => c.health),damage:JSON.parse(document.querySelector('#manifest').textContent).damage,readout:document.querySelector('#componentReadout').textContent,total:b.components.length,destroyed:b.components.filter(c => c.destroyed).length}; });
    assert.notDeepEqual(result.health,initial); assert(result.destroyed > 0); assert.equal(result.damage.destroyed,result.destroyed); assert.equal(result.readout,(result.total-result.destroyed)+' / '+result.total);
    await page.click('#resetDamage'); assert.deepEqual(await page.evaluate(() => BuildingLab.currentBuilding.components.map(c => c.health)),initial); return result.damage;
  });
  await check('locks and keyboard', async () => {
    for (const id of ['lockTopology','lockEnvelope','lockFacade','lockConstruction','lockRoof']) await page.check('#'+id);
    const before = await page.evaluate(() => BuildingLab.currentBuilding.plan.spec);
    await page.evaluate(() => document.activeElement.blur()); await page.keyboard.press('n'); await settle();
    const after = await page.evaluate(() => BuildingLab.currentBuilding.plan.spec);
    assert.notEqual(after.seed,before.seed);
    for (const key of ['presetId','footprintFamily','roomProgram','width','depth','storeys','facade','material','structuralSystem','stairFamily','roofFamily','roofCovering','features']) assert.deepEqual(after[key],before[key],key);
    await page.keyboard.press('c'); assert.equal(await page.isChecked('#cutaway'),true); await page.keyboard.press('c');
    await page.focus('#seed'); await page.keyboard.press('r'); assert.equal(await page.evaluate(() => BuildingLab.currentBuilding.plan.spec.seed),after.seed);
    for (const id of ['lockTopology','lockEnvelope','lockFacade','lockConstruction','lockRoof']) await page.uncheck('#'+id);
  });
  await check('partial lock and explicit controls constrain AUTO presets', async () => {
    await configure({presetId:'AUTO',seed:300,shape:'RECT',roof:'GABLE',roomProgram:'AUTO',stairType:'AUTO',structure:'AUTO'});
    await page.check('#lockConstruction');
    const before = await page.evaluate(() => BuildingLab.currentBuilding.plan.spec);
    for (let i=0;i<3;i++) {
      await page.click('#randomize'); await settle();
      const after = await page.evaluate(() => BuildingLab.currentBuilding.plan.spec);
      assert.equal(await page.isVisible('#error'),false,await page.locator('#error').textContent());
      assert.equal(after.structuralSystem,before.structuralSystem); assert.equal(after.stairFamily,before.stairFamily);
      assert.equal(after.roofFamily,'GABLE'); assert.equal(after.footprintFamily,'RECT');
    }
    await page.uncheck('#lockConstruction');
  });
  await check('initial damage is deterministic and reset preserves it', async () => {
    await configure({presetId:'FAMILY_HOUSE',roomProgram:'AUTO',stairType:'AUTO',structure:'AUTO',seed:4210,damagePercent:35});
    const initial = await page.evaluate(() => BuildingLab.currentBuilding.components.map(c => c.health));
    assert(initial.some(h => h === 0));
    assert.equal(await page.evaluate(() => BuildingLab.currentBuilding.manifest.damage.achievedPercent),35);
    await page.click('#breach'); await page.click('#resetDamage');
    assert.deepEqual(await page.evaluate(() => BuildingLab.currentBuilding.components.map(c => c.health)),initial);
    await configure({seed:4210,damagePercent:35}); assert.deepEqual(await page.evaluate(() => BuildingLab.currentBuilding.components.map(c => c.health)),initial);
    await configure({seed:4210,damagePercent:0});
  });
  await check('failed generation preserves last model and diagnoses seed', async () => {
    const before = await page.evaluate(() => { window.__previousBuilding = BuildingLab.currentBuilding; return BuildingLab.currentBuilding.plan.spec.seed; });
    await page.fill('#width','1'); await page.click('#generate'); await settle();
    assert.equal(await page.evaluate(() => BuildingLab.currentBuilding === window.__previousBuilding && !BuildingLab.currentBuilding.disposed),true);
    assert.match(await page.locator('#error').textContent(),new RegExp('previous successful seed: '+before));
    await page.fill('#width','16'); await page.click('#generate'); await settle(); assert.equal(await page.isVisible('#error'),false);
  });
  await check('copy manifest', async () => {
    await page.click('#copyManifest'); const copied = await page.evaluate(() => navigator.clipboard.readText());
    assert.deepEqual(JSON.parse(copied),await page.evaluate(() => BuildingLab.currentBuilding.exportManifest()));
  });
  for (const [roof,preset,shape] of [['GABLE','FAMILY_HOUSE','RECT'],['HIP','FAMILY_HOUSE','RECT'],['HALF_HIP','FAMILY_HOUSE','RECT'],['COMPOUND_GABLE','FAMILY_HOUSE','L'],['SHED','OFFICE','RECT'],['FLAT','OFFICE','L'],['MANSARD','MULTI_FAMILY','RECT']]) {
    await check('roof inspection ' + roof, async () => {
      await configure({presetId:preset,shape,roof,roomProgram:'AUTO',stairType:'AUTO',structure:'AUTO',style:'AUTO',windowType:'AUTO',doorType:'AUTO',roofCovering:'AUTO',width:roof === 'MANSARD' ? 24 : 16,depth:roof === 'MANSARD' ? 22 : 14,floors:'2',seed:42,featureChimney:true,featurePorch:true,featureBalcony:true,featureDormer:roof !== 'FLAT'});
      const box = await page.locator('#viewport canvas').boundingBox();
      await page.mouse.move(box.x+box.width/2,box.y+box.height/2); await page.mouse.down();
      await page.mouse.move(box.x+box.width/2,box.y+box.height/2+90,{steps:12}); await page.mouse.up();
      await page.evaluate(() => new Promise(resolve => { let frames=90; function frame() { if (--frames) requestAnimationFrame(frame); else resolve(); } requestAnimationFrame(frame); }));
      const actual = await page.evaluate(() => { const b=BuildingLab.currentBuilding; return {seed:b.plan.spec.seed,roof:b.plan.spec.roofFamily,features:b.plan.features.map(f => f.type)}; });
      assert.equal(actual.seed,42); assert.equal(actual.roof,roof); return {...actual,screenshot:await snapshot('roof-'+roof.toLowerCase())};
    });
  }
  await check('visual inspection matrix', async () => {
    const names = ['shape-rect','shape-l','shape-t','shape-u','shape-h','shape-courtyard','shape-composite','features','cutaway-floor-1','roof-gable','roof-hip','roof-half_hip','roof-compound_gable','roof-shed','roof-flat','roof-mansard'];
    assert(names.every(name => captured.has(name)), 'Every matrix image must come from this run');
    const cards = names.map(name => '<figure><figcaption>'+name+'</figcaption><img src="data:image/png;base64,'+fs.readFileSync(path.join(output,name+'.png')).toString('base64')+'"></figure>');
    const matrix = await context.newPage();
    try {
      await matrix.setViewportSize({width:1500,height:1000});
      await matrix.setContent('<style>body{margin:0;padding:16px;background:#142329;color:#eef4ed;font:18px system-ui}main{display:grid;grid-template-columns:repeat(4,1fr);gap:12px}figure{margin:0}figcaption{padding:8px}img{width:100%;display:block}</style><h1>Building Lab · shape and roof inspection</h1><main>'+cards.join('')+'</main>');
      await matrix.evaluate(() => Promise.all([...document.images].map(img => img.decode())));
      const file=path.join(output,'visual-matrix.png'); await matrix.screenshot({path:file,fullPage:true}); return file;
    } finally { await matrix.close(); }
  });
  await check('before and after renderer comparison', async () => {
    const files = ['baseline-shape-rect','shape-rect','baseline-roof-gable','roof-gable'];
    if (!files.every(name => fs.existsSync(path.join(output,name+'.png')))) return 'No previous capture exists on this machine; current matrix is available.';
    const comparison = await context.newPage();
    try {
      await comparison.setViewportSize({width:1400,height:1000});
      const cards=files.map((name,i) => '<figure><figcaption>'+(i%2?'Current renderer':'Previous capture')+' · '+name+'</figcaption><img src="data:image/png;base64,'+fs.readFileSync(path.join(output,name+'.png')).toString('base64')+'"></figure>');
      await comparison.setContent('<style>body{margin:0;padding:16px;background:#142329;color:#eef4ed;font:18px system-ui}main{display:grid;grid-template-columns:repeat(2,1fr);gap:12px}figure{margin:0}figcaption{padding:8px}img{width:100%;display:block}</style><h1>Building Lab · previous and current captures</h1><main>'+cards.join('')+'</main>');
      await comparison.evaluate(() => Promise.all([...document.images].map(img => img.decode())));
      const file=path.join(output,'renderer-comparison.png'); await comparison.screenshot({path:file,fullPage:true}); return file;
    } finally { await comparison.close(); }
  });
  await check('destruction demo launch reset return and reopen', async () => {
    await configure({presetId:'RURAL_SHED',shape:'RECT',roof:'GABLE',floors:'1',width:6,depth:5,roomProgram:'AUTO',stairType:'AUTO',structure:'AUTO',featureChimney:false,featurePorch:false,featureBalcony:false,featureDormer:false,seed:777});
    for (let i=0;i<2;i++) {
      await page.click('#destructionDemo'); await page.waitForFunction(() => RTS.DestructionDemo.diagnostics?.ready);
      assert.equal(await page.locator('.destruction-demo canvas').count(),1);
      await page.locator('.destruction-demo [data-id="reset"]').click(); await page.waitForFunction(() => RTS.DestructionDemo.diagnostics?.ready && !RTS.DestructionDemo.diagnostics.preparing);
      const diagnostic = await page.evaluate(() => RTS.DestructionDemo.diagnostics); assert.equal(diagnostic.shots,0); assert.equal(diagnostic.projectiles,0);
      if (!i) await page.screenshot({path:path.join(output,'destruction-demo.png')});
      await page.locator('.destruction-demo [data-id="close"]').click(); await page.waitForFunction(() => !RTS.DestructionDemo.active);
      assert.equal(await page.locator('#viewport canvas').count(),1); assert.equal(await page.locator('.destruction-demo').count(),0);
      assert.equal(await page.evaluate(() => BuildingLab.currentBuilding.disposed),false);
    }
    return snapshot('after-demo');
  });
  await check('browser has no runtime or network errors', async () => { assert.deepEqual(errors,[]); assert.deepEqual(external,[]); });
}
main().catch(error => { results.push({name:'suite',passed:false,error:error.stack}); console.error(error); }).finally(async () => {
  if (page && results.some(r => !r.passed)) await page.screenshot({path:path.join(output,'failure.png')}).catch(() => {});
  await browser?.close(); if (server) await new Promise(resolve => server.close(resolve));
  fs.mkdirSync(output,{recursive:true}); fs.writeFileSync(path.join(output,'report.json'),JSON.stringify({results,errors,external},null,2)+'\n');
  console.log('Artifacts: '+output); if (results.some(r => !r.passed)) process.exitCode = 1;
});
