(function () {
  'use strict';
  // Resolve every local asset from the page URL. The supported launch mode is
  // HTTP(S), which gives WASM, iframe and storage one stable origin.
  const page=document.baseURI||location.href;
  const base=new URL('./',page).href;
  const testing=document.documentElement.dataset.mode==='tests';
  const status=document.getElementById('bootStatus');
  const local=[
    'js/config.js','js/core/seededRandom.js','js/core/fixedClock.js','js/noise.js','js/terrain.js',
    'js/world/side.js','js/equipment/equipmentCatalog.js','js/weapons/weaponCatalog.js','js/equipment/infantryLoadouts.js','js/equipment/equipment.js','js/units/unit.js','js/units/infantryGenome.js','js/rendering/faceAnatomy.js','js/weapons/handRig.js','js/rendering/infantryRig.js',
    'js/rendering/surfaceBuilder.js','js/equipment/equipmentFit.js','js/equipment/gearGeometry.js','js/equipment/infantryGear.js','js/rendering/infantryHair.js','js/rendering/infantryFace.js','js/rendering/infantrySurface.js',
    'js/rendering/infantryMaterials.js','js/rendering/appearanceCache.js','js/rendering/infantryFactory.js','js/physics/ragdollSchema.js',
    'js/animation/twoBoneIK.js','js/animation/gaitProfile.js','js/animation/groundContact.js','js/animation/postureProfile.js','js/animation/locomotionController.js','js/animation/bipedFeet.js','js/animation/infantryAnimator.js','js/animation/faceAnimator.js','js/weapons/weaponGeometry.js','js/weapons/weaponHandlingProfiles.js','js/weapons/weaponController.js','js/units/infantryUnit.js',
    'js/rendering/unitDiagnostics.js','js/environment/plantCatalog.js','js/environment/plantGenerator.js','js/environment/rockGenerator.js',
    'js/vendor/buildings-6.0.0.js','js/buildings/buildingCatalog.js','js/buildings/buildingRandom.js','js/buildings/buildingBackends.js','js/buildings/buildingPolygon.js','js/buildings/buildingSpec.js','js/buildings/buildingFootprints.js','js/buildings/buildingPrograms.js','js/buildings/buildingSolver.js','js/buildings/buildingAssemblies.js','js/buildings/buildingPlan.js','js/buildings/buildingInstance.js','js/buildings/buildingGeometry.js','js/buildings/buildingCollisionProxy.js','js/buildings/buildingWorldRepresentation.js','js/buildings/buildingRuntime.js','js/buildings/buildingDamageChunkManager.js','js/buildings/buildingDamageChunks.js','js/buildings/buildingSpatialIndex.js','js/buildings/buildingRepresentationManager.js','js/buildings/proceduralBuildingGenerator.js',
    'js/world/lodHysteresis.js','js/world/unitPresentationScheduler.js','js/world/settlementGenerator.js','js/world/fenceGenerator.js','js/world/worldShadowPolicy.js','js/rendering/worldShadowPolicy.js','js/world/buildingSpatialIndex.js','js/world/worldDestructionHost.js','js/world/worldBallisticsBridge.js','js/world/battlefield.js',
    'js/destruction/catalogs.js','js/destruction/solid.js','js/destruction/impactProfiles.js','js/destruction/materialImpact.js','js/destruction/rubbleField.js','js/destruction/projectileMath.js','js/destruction/projectileState.js','js/destruction/impactSolver.js','js/destruction/surfaceDamage.js','js/destruction/materialModel.js','js/destruction/projectileTrace.js','js/destruction/ballistics.js','js/destruction/physics.js','js/destruction/adapters.js','js/destruction/effects.js','js/destruction/demo.js',
    'js/ai/aiModelRegistry.js','js/ai/simpleCombatModel.js','js/ai/battlefieldAIManager.js','js/combat/infantryDamageRuntime.js'
  ];
  const loadedScripts=new Map();
  function script(url){
    const key=String(url),existing=loadedScripts.get(key);
    if(existing)return existing;
    const promise=new Promise((resolve,reject)=>{
      const s=document.createElement('script');s.src=key;s.async=false;
      s.onload=resolve;s.onerror=()=>{s.remove();reject(new Error('Nie można załadować: '+key));};document.head.appendChild(s);
    });
    loadedScripts.set(key,promise);
    promise.catch(()=>{if(loadedScripts.get(key)===promise)loadedScripts.delete(key);});
    return promise;
  }
  let rapierReady=null;
  function rapierModule(){
    // The generated local bundle has appeared in all of these shapes over
    // time: the module itself, an ES-module namespace, or a compatibility
    // namespace containing the actual module. Keep one canonical object for
    // every consumer after resolving it once.
    const exported=globalThis.RapierCompat||globalThis.Rapier||globalThis.RAPIER;
    const candidates=[exported,exported?.default,exported?.RapierCompat,exported?.default?.RapierCompat];
    const module=candidates.find(value=>value&&typeof value.init==='function'&&typeof value.World==='function')||
      candidates.find(value=>value&&typeof value.init==='function');
    if(module&&module!==exported)globalThis.RapierCompat=module;
    return module;
  }
  function validateRapier(P){
    const missing=[];
    const required=[['World',P?.World],['RigidBodyDesc',P?.RigidBodyDesc],['ColliderDesc',P?.ColliderDesc],
      ['RigidBodyDesc.kinematicPositionBased',P?.RigidBodyDesc?.kinematicPositionBased],
      ['ColliderDesc.cuboid',P?.ColliderDesc?.cuboid],['ColliderDesc.capsule',P?.ColliderDesc?.capsule]];
    for(const [name,value] of required)if(typeof value!=='function')missing.push(name);
    if(missing.length)throw new Error('Lokalny Rapier nie zakończył inicjalizacji WASM. Brak: '+missing.join(', ')+'.');
    return P;
  }
  function preloadLocalPhysics(){
    if(!rapierReady)rapierReady=(async()=>{
      if(!rapierModule())await script(new URL('js/vendor/rapier-0.19.3.js',base).href);
      if(!globalThis.DestructionConvexFaces)await script(new URL('js/vendor/convex-hull-r128.js',base).href);
      const P=rapierModule();
      if(!P||typeof P.init!=='function')throw new Error('Lokalny RapierCompat nie jest dostępny.');
      await P.init();
      return validateRapier(P);
    })().catch(error=>{rapierReady=null;throw error;});
    return rapierReady;
  }
  globalThis.RapierReady=preloadLocalPhysics;
  async function dependency(name,paths,verify){
    for(const p of paths){try{await script(p);if(verify())return;}catch(e){console.warn(name+': próba kolejnego CDN.');}}
    throw new Error('Brak biblioteki '+name+'. Potrzebne jest połączenie z internetem i dostęp do CDN (jsDelivr lub unpkg).');
  }
  async function boot(){
    await dependency('Three.js r128',[
      new URL('tools/destruction/node_modules/three/build/three.min.js',base).href,
      'https://cdn.jsdelivr.net/npm/three@0.128.0/build/three.min.js',
      'https://unpkg.com/three@0.128.0/build/three.min.js'],()=>window.THREE&&THREE.REVISION==='128');
    await dependency('OrbitControls r128',[
      new URL('tools/destruction/node_modules/three/examples/js/controls/OrbitControls.js',base).href,
      'https://cdn.jsdelivr.net/npm/three@0.128.0/examples/js/controls/OrbitControls.js',
      'https://unpkg.com/three@0.128.0/examples/js/controls/OrbitControls.js'],()=>typeof THREE.OrbitControls==='function');
    await preloadLocalPhysics();
    if(status)status.textContent='Generowanie świata i modelu…';
    for(const p of local)await script(new URL(p,base).href);
    await RTS.Buildings.initializeBuildingBackends();
    if(!testing){
      document.title='Genomes — Infantry Lab '+RTS.Config.VERSION;
      document.querySelectorAll('.version').forEach(el=>el.textContent=RTS.Config.VERSION);
      document.querySelectorAll('.generator-version').forEach(el=>el.textContent=RTS.Config.GENERATOR_VERSION);
    }
    if(testing){await script(new URL('tests/acceptance.js',base).href);await RTS.runAcceptanceTests();}
    else {
      for(const p of ['js/ui/unitPreview.js','js/ui/equipmentEditor.js','js/ui/weaponEditor.js','js/ui/debugPanel.js','js/game.js'])await script(new URL(p,base).href);
      RTS.game=new RTS.Game();
    }
    if(status)status.hidden=true;
  }
  boot().catch(error=>{
    console.error(error);if(status){status.hidden=false;status.className='error';status.textContent='Nie udało się uruchomić prototypu.\n'+error.message+'\n\nRozpakuj cały ZIP i otwórz index.html. Nie trzeba wyłączać zabezpieczeń przeglądarki. Szczegóły: docs/00_START.txt.';}
  });
})();
