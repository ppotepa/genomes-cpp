(function () {
  'use strict';
  const R=window.RTS;
  function sceneGeometryBufferBytes(scene,state,additionalScene=null){
    const stack=state.stack,geometries=state.geometries,buffers=state.buffers;stack.length=0;geometries.clear();buffers.clear();stack.push(scene);if(additionalScene)stack.push(additionalScene);
    let bytes=0;
    const add=attribute=>{const array=attribute&&attribute.array;if(!array||!array.buffer||buffers.has(array.buffer))return;buffers.add(array.buffer);bytes+=array.buffer.byteLength;};
    while(stack.length){const node=stack.pop();if(!node)continue;
      // InstancedMesh keeps per-instance transforms/colors outside its shared
      // geometry attributes. Count those CPU buffers as scene geometry data.
      add(node.instanceMatrix);add(node.instanceColor);
      if(node.geometry&&!geometries.has(node.geometry)){
        const geometry=node.geometry;geometries.add(geometry);
        if(geometry.index)add(geometry.index);
        const attributes=geometry.attributes||{};
        for(const name in attributes)if(Object.prototype.hasOwnProperty.call(attributes,name))add(attributes[name]);
        const morphAttributes=geometry.morphAttributes||{};
        for(const name in morphAttributes)if(Object.prototype.hasOwnProperty.call(morphAttributes,name)){const targets=morphAttributes[name];for(let i=0;i<targets.length;i++)add(targets[i]);}
      }
      const children=node.children||[];for(let i=0;i<children.length;i++)stack.push(children[i]);
    }
    return bytes;
  }
  function formatBytes(bytes){return bytes>=1048576?(bytes/1048576).toFixed(1)+' MiB':bytes>=1024?(bytes/1024).toFixed(1)+' KiB':bytes+' B';}
  function sceneObjectStats(scene,state){const stack=state.stack;stack.length=0;stack.push(scene);let objects=0,meshes=0,visibleMeshes=0,shadowCasters=0,instancedMeshes=0;while(stack.length){const node=stack.pop();if(!node)continue;objects++;if(node.isMesh||node.isInstancedMesh){meshes++;if(node.visible)visibleMeshes++;if(node.castShadow&&node.visible)shadowCasters++;if(node.isInstancedMesh)instancedMeshes++;}for(const child of node.children||[])stack.push(child);}state.objects=objects;state.meshes=meshes;state.visibleMeshes=visibleMeshes;state.shadowCasters=shadowCasters;state.instancedMeshes=instancedMeshes;return state;}
  R.Game=class Game {
    constructor(){
      this.scene=new THREE.Scene();this.scene.background=new THREE.Color(0xb0c5cf);this.scene.fog=new THREE.FogExp2(0xb0c5cf,.0003);
      const C=R.Config.CAMERA;this.camera=new THREE.PerspectiveCamera(C.FOV,innerWidth/innerHeight,C.NEAR,C.FAR);
      this.renderer=new THREE.WebGLRenderer({antialias:true,powerPreference:'high-performance'});
      this.renderer.outputEncoding=THREE.sRGBEncoding;this.renderer.toneMapping=THREE.ACESFilmicToneMapping;this.renderer.toneMappingExposure=1;
      this._renderPixelRatio=0;this.updateRenderScale();
      this._removeQualityListener=R.RenderQuality.onChange(()=>this.updateRenderScale());
      this.renderer.shadowMap.enabled=true;this.renderer.shadowMap.type=THREE.PCFSoftShadowMap;
      document.getElementById('app').appendChild(this.renderer.domElement);
      this.controls=new THREE.OrbitControls(this.camera,this.renderer.domElement);this.controls.enableDamping=true;this.controls.dampingFactor=.075;
      this.controls.screenSpacePanning=false;this.controls.minDistance=C.MIN_DISTANCE;this.controls.maxDistance=C.MAX_DISTANCE;this.controls.maxPolarAngle=Math.PI*.486;
      this.scene.add(new THREE.HemisphereLight(0xd9ecf2,0x6c684f,.80));
      // Keep application startup light: terrain and units are built only by START NEW.
      this.terrain=null;
      const y=0,sun=new THREE.DirectionalLight(0xffefd8,1.7);
      this.sun=sun;this.battlefield=null;this.starting=false;
      this._lodBuildUnit=null;this._lodBuildPromise=null;
      sun.position.set(-30,y+55,35);sun.target.position.set(0,y,0);this.scene.add(sun,sun.target);sun.castShadow=true;
      sun.shadow.mapSize.set(2048,2048);sun.shadow.normalBias=.012;sun.shadow.bias=-.00015;
      this._shadowDirection=new THREE.Vector3(-30,55,35).normalize();this._shadowExtent=0;this._shadowTarget=new THREE.Vector3(Infinity,Infinity,Infinity);
      // Cienie obejmują wyłącznie pole testowe jednostek, nie 4 km² terenu.
      this.sides={};for(const id of Object.keys(R.Config.SIDES))this.sides[id]=new R.Side(id,R.Config.SIDES[id]);
      this.units=[];this.camera.position.set(20,y+15,26);this.controls.target.set(0,y+.8,0);this.controls.update();this.updateShadowRegion(true);
      this.lastLOD=0;this.lastCacheStats=0;this._memoryHighWater={heapBytes:0,geometries:0,textures:0,geometryBufferBytes:0};this._sceneMemoryScratch={stack:[],geometries:new Set(),buffers:new Set()};this._completionReported=false;this._lodFrustum=new THREE.Frustum();this._lodViewProjection=new THREE.Matrix4();this._lodUnitSphere=new THREE.Sphere(new THREE.Vector3(),3);this._lodCameraState=new Float64Array(36);this._lodCameraStateReady=false;this._lodCameraParent=null;this.clock=new R.FixedClock();this.debug=new R.DebugPanel(this);this.running=true;
      this.startButton=document.getElementById('startNew');this.startButton.disabled=false;this.onStart=()=>{document.getElementById('worldConfig').hidden=false;};this.startButton.addEventListener('click',this.onStart);
      const configDefaults=R.WorldGeneration.defaults;this.worldConfig={};for(const [key,id] of Object.entries({seed:'worldSeed',size:'worldMapSize',preset:'worldPreset',vegetation:'worldVegetation',buildings:'worldBuildings',fencedParcels:'worldFences'}))this.worldConfig[key]=document.getElementById(id);
      try{const saved=JSON.parse(localStorage.getItem('genomes.world-generation-1')||'null');const settings=R.WorldGeneration.normalize(saved||configDefaults);for(const key in this.worldConfig){if(this.worldConfig[key])this.worldConfig[key].value=key==='vegetation'||key==='buildings'||key==='fencedParcels'?Math.round(settings[key]*100):settings[key];}}catch{}
      const updateWorldLabels=()=>{for(const [key,id] of Object.entries({vegetation:'worldVegetationValue',buildings:'worldBuildingsValue',fencedParcels:'worldFencesValue'})){const out=document.getElementById(id),field=this.worldConfig[key];if(out&&field)out.textContent=field.value+'%';}};updateWorldLabels();
      for(const field of Object.values(this.worldConfig))if(field)field.addEventListener('input',updateWorldLabels);
      document.getElementById('randomSeed').addEventListener('click',()=>{this.worldConfig.seed.value=crypto.getRandomValues(new Uint32Array(1))[0];});
      document.getElementById('worldConfigForm').addEventListener('submit',event=>{event.preventDefault();const config=R.WorldGeneration.normalize({seed:this.worldConfig.seed.value,size:this.worldConfig.size.value,preset:this.worldConfig.preset.value,vegetation:Number(this.worldConfig.vegetation.value)/100,buildings:Number(this.worldConfig.buildings.value)/100,fencedParcels:Number(this.worldConfig.fencedParcels.value)/100});try{localStorage.setItem('genomes.world-generation-1',JSON.stringify(config));}catch{}document.getElementById('worldConfig').hidden=true;this.startNew(config.seed,config);});
      this.qualitySelect=document.getElementById('renderQuality');this.qualitySelect.value=R.RenderQuality.get();this.onQualityChange=()=>R.RenderQuality.set(this.qualitySelect.value);this.qualitySelect.addEventListener('change',this.onQualityChange);
      const rockPlacementDefaults=R.Battlefield.rockPlacementDefaults||{};this.rockSettingListeners=[];
      try{const saved=JSON.parse(localStorage.getItem('genomes.rockPlacement.v1')||'null'),normalized=R.Battlefield.normalizeRockSettings?R.Battlefield.normalizeRockSettings(saved):saved;if(normalized)for(const [key,value] of Object.entries(normalized)){const field=document.getElementById('rock'+key[0].toUpperCase()+key.slice(1));if(field)field.value=value;}}catch{}
      for(const key of Object.keys(rockPlacementDefaults)){const field=document.getElementById('rock'+key[0].toUpperCase()+key.slice(1));if(!field)continue;const output=document.getElementById(field.id+'Value'),update=()=>{if(output)output.textContent=Math.round(Number(field.value)*100)+'%';try{const values={};for(const name of Object.keys(rockPlacementDefaults)){const setting=document.getElementById('rock'+name[0].toUpperCase()+name.slice(1));if(setting)values[name]=Number(setting.value);}localStorage.setItem('genomes.rockPlacement.v1',JSON.stringify(values));}catch{}};field.addEventListener('input',update);field.addEventListener('change',update);this.rockSettingListeners.push([field,update]);update();}
      document.getElementById('scenarioStatus').textContent='Wybierz konfigurację świata.';
      document.getElementById('worldSize').textContent='Świat nieutworzony · plan: 600 × 600 m';
      this.resize=()=>{this.camera.aspect=innerWidth/innerHeight;this.camera.updateProjectionMatrix();this.updateRenderScale();};
      this.visibility=()=>{this.clock.reset();if(this.debug.preview)this.debug.preview.clock.reset();};
      window.addEventListener('resize',this.resize);document.addEventListener('visibilitychange',this.visibility);
      this.loop=this.loop.bind(this);this.frameId=requestAnimationFrame(this.loop);
      window.addEventListener('pagehide',()=>this.dispose(),{once:true});
    }
    updateRenderScale(){
      const ratio=R.RenderQuality.ratio(innerWidth,innerHeight,window.devicePixelRatio||1,'map');
      if(Math.abs(ratio-this._renderPixelRatio)>1e-4){this._renderPixelRatio=ratio;this.renderer.setPixelRatio(ratio);}
      this.renderer.setSize(innerWidth,innerHeight);
    }

    updateShadowRegion(force=false){
      const target=this.controls.target,distance=this.camera.position.distanceTo(target);
      const extent=distance<48?24:distance<110?55:110;
      const moved=this._shadowTarget.distanceToSquared(target)>.01;
      if(!force&&extent===this._shadowExtent&&!moved)return;
      this._shadowExtent=extent;this._shadowTarget.copy(target);
      const lightDistance=Math.max(70,extent*2);
      this.sun.target.position.copy(target);
      this.sun.position.copy(target).addScaledVector(this._shadowDirection,lightDistance);
      Object.assign(this.sun.shadow.camera,{left:-extent,right:extent,top:extent,bottom:-extent,near:.5,far:lightDistance+extent*2});
      this.sun.shadow.camera.updateProjectionMatrix();
    }

    updateLODFrustum(){
      const camera=this.camera,state=this._lodCameraState,projection=camera.projectionMatrix.elements;
      let changed=!this._lodCameraStateReady||camera.parent!==this._lodCameraParent||!!camera.parent;
      let i=0;const p=camera.position,q=camera.quaternion,up=camera.up;
      const x=p.x,y=p.y,z=p.z;
      if(state[i]!==x)changed=true;state[i++]=x;if(state[i]!==y)changed=true;state[i++]=y;if(state[i]!==z)changed=true;state[i++]=z;
      const qx=q.x,qy=q.y,qz=q.z,qw=q.w;
      if(state[i]!==qx)changed=true;state[i++]=qx;if(state[i]!==qy)changed=true;state[i++]=qy;if(state[i]!==qz)changed=true;state[i++]=qz;if(state[i]!==qw)changed=true;state[i++]=qw;
      const ux=up.x,uy=up.y,uz=up.z;
      if(state[i]!==ux)changed=true;state[i++]=ux;if(state[i]!==uy)changed=true;state[i++]=uy;if(state[i]!==uz)changed=true;state[i++]=uz;
      for(let j=0;j<16;j++){const value=projection[j];if(state[i]!==value)changed=true;state[i++]=value;}
      this._lodCameraParent=camera.parent;this._lodCameraStateReady=true;
      if(!changed)return false;
      camera.updateMatrixWorld();
      this._lodViewProjection.multiplyMatrices(camera.projectionMatrix,camera.matrixWorldInverse);
      this._lodFrustum.setFromProjectionMatrix(this._lodViewProjection);
      return true;
    }

    async startNew(seed=crypto.getRandomValues(new Uint32Array(1))[0],generation={}){
      if(this.starting||!this.running)return;this.starting=true;this.startButton.disabled=true;this._completionReported=false;
      const status=document.getElementById('scenarioStatus'),rockSettings={},rockDefaults=R.Battlefield.rockPlacementDefaults||{};for(const key of Object.keys(rockDefaults)){const field=document.getElementById('rock'+key[0].toUpperCase()+key.slice(1));rockSettings[key]=field?Number(field.value):rockDefaults[key];}let candidate=null;const controller=new AbortController();this.startController=controller;
      const previous=this.battlefield,previousUnits=this.units,previousTerrain=this.terrain;
      const previousCameraPosition=this.camera.position.clone(),previousTarget=this.controls.target.clone(),previousMaxDistance=this.controls.maxDistance;
      const previousStatus=status.textContent;
      status.textContent='Generowanie terenu, roślin i skał…';
      let failure=null;
      try{
        await globalThis.RapierReady?.();
        candidate=new R.Battlefield(seed,this.sides,rockSettings,generation);
        // Drop only inactive appearance LRU entries before staging a second
        // world. Active geometry stays reference-counted for rollback; stale
        // retained variants must not inflate the build's memory peak.
        R.AppearanceCache.clear();
        await new Promise(resolve=>requestAnimationFrame(resolve));
        let lastBuildMemorySample=-Infinity;
        const sampleBuildMemory=(force=false)=>{
          if(!document.getElementById('performancePanel').open)return;
          const now=performance.now();if(!force&&now-lastBuildMemorySample<500)return;lastBuildMemorySample=now;
          const bytes=sceneGeometryBufferBytes(this.scene,this._sceneMemoryScratch,candidate.root);
          this._memoryHighWater.geometryBufferBytes=Math.max(this._memoryHighWater.geometryBufferBytes,bytes);
        };
        await candidate.build((count,stage)=>{
          status.textContent=stage==='terrain'?'Generowanie terenu…':stage==='trees'
            ?'Budowanie roślin: '+count+' / '+R.Battlefield.vegetationPrototypeCount:stage==='rocks'?'Budowanie skał: '+count+' / 8':stage==='buildings'?'Budowanie osady: '+count+' / '+(candidate.plan?.buildings.length||6):'Tworzenie armii: '+count+' / 50';
          sampleBuildMemory(stage==='army'&&count===50);
        },controller.signal);
        sampleBuildMemory(true);
        if(!this.running||controller.signal.aborted){candidate.dispose();return;}
        if(this.debug.isOpen)this.debug.close();
        // Install and attach the complete candidate before releasing the old
        // world. If a setup operation throws, the catch path can still restore
        // the exact previous simulation instead of trying to rebuild it.
        this.battlefield=candidate;this.terrain=candidate.terrain;this.units=candidate.units;this.scene.add(candidate.root);this.units.forEach(u=>u.side.addUnit(u));
        this.terrain.setSmallGridVisible(document.getElementById('grid5').checked);this.terrain.setBigGridVisible(document.getElementById('grid100').checked);
        const y=this.terrain.getHeightAt(0,0);this.controls.target.set(0,y,0);this.camera.position.set(115,y+145,155);this.controls.maxDistance=340;this.controls.update();
        this.updateShadowRegion(true);
        this.clock.reset();document.getElementById('worldSize').textContent=candidate.generation.size+' × '+candidate.generation.size+' m · '+candidate.buildings.length+' budynków · 1 jednostka = 1 metr';status.textContent='25 vs 25 · osada · marsz · seed '+candidate.seed;
        if(previous)previous.dispose();else{previousUnits.forEach(u=>u.dispose());if(previousTerrain)previousTerrain.dispose();}
      }catch(error){
        failure=error;
        if(candidate&&this.battlefield===candidate){
          candidate.units?.forEach(unit=>unit.side.removeUnit(unit));
          candidate.dispose?.();this.scene.remove(candidate.root);
          this.battlefield=previous;this.units=previousUnits;this.terrain=previousTerrain;
          if(previous&&previous.root.parent!==this.scene)this.scene.add(previous.root);
          this.camera.position.copy(previousCameraPosition);this.controls.target.copy(previousTarget);this.controls.maxDistance=previousMaxDistance;this.controls.update();
          this.updateShadowRegion(true);
        }else if(candidate)candidate.dispose?.();
        if(error.name!=='AbortError'){console.error(error);status.textContent='Nie udało się utworzyć świata: '+error.message;}
      }
      finally{
        if(this.startController===controller)this.startController=null;
        this.starting=false;
        this.startButton.disabled=!this.running;
        if(!this.running)status.textContent='Gra została zamknięta.';
        else if(failure?.name==='AbortError')status.textContent=previousStatus;
      }
    }
    loop(now){
      if(!this.running)return;this.frameId=requestAnimationFrame(this.loop);if(document.hidden)return;const frameStart=performance.now();this._frameTelemetry=this._frameTelemetry||{simulationMs:0,presentationMs:0,rendererMs:0,frameMs:0,frames:0};
      if(this.debug.isOpen){this.debug.frame(now);return;}
      if(this.starting){this.clock.reset();return;}
      const simulationStart=performance.now();const alpha=this.clock.advance(now,dt=>{if(this.battlefield)this.battlefield.step(dt);else for(const u of this.units)u.step(dt,this.terrain);});this._frameTelemetry.simulationMs=performance.now()-simulationStart;
      if(now-this.lastLOD>100){
        this.lastLOD=now;
        this.updateLODFrustum();this.battlefield?.updateBuildingRepresentations(this.camera);
        this.updateShadowRegion();
        if(this.battlefield&&this.battlefield.arrived===50&&!this._completionReported){document.getElementById('scenarioStatus').textContent='25 vs 25 · armie na pozycjach · seed '+this.battlefield.seed;this._completionReported=true;}
        for(const u of this.units){
          const distanceSq=this.camera.position.distanceToSquared(u.position);
          this._lodUnitSphere.center.set(u.position.x,u.position.y+u.animator.H*.5,u.position.z);
          this._lodUnitSphere.radius=Math.max(3,u.animator.H*1.45);
          const inView=this._lodFrustum.intersectsSphere(this._lodUnitSphere);
          let desiredDetail=u.model.detail;
          if(inView){
            if(distanceSq<100)desiredDetail='high';
            else if(distanceSq>4225)desiredDetail='far';
            else if(distanceSq<3025&&u.model.detail==='far')desiredDetail='world';
            else if(distanceSq>225&&u.model.detail==='high')desiredDetail='world';
          }
          u._lodDesiredDetail=desiredDetail;u._lodDetailDistanceSq=distanceSq;
          const faceInterval=inView?(distanceSq<100?0:distanceSq>225?.10:1/30):.20;
          const animationInterval=inView?(distanceSq<225?0:distanceSq<1600?1/30:1/15):.20;
          u.setVisualCadence(animationInterval,faceInterval);
        }
        // Desired LOD and visibility are sampled every 100 ms above, so use
        // that same cadence to select the next rebuild instead of rescanning
        // the entire army on every rendered frame.
        let detailCandidate=null,nearestDetailSq=Infinity;
        for(const u of this.units)if(u._lodDesiredDetail&&u._lodDesiredDetail!==u.model.detail&&u._lodDetailDistanceSq<nearestDetailSq){detailCandidate=u;nearestDetailSq=u._lodDetailDistanceSq;}
        if(detailCandidate&&!this._lodBuildPromise){
          const unit=detailCandidate,target=unit._lodDesiredDetail;this._lodBuildUnit=unit;
          const yieldFrame=()=>new Promise(resolve=>requestAnimationFrame(resolve));
          const stillNeeded=()=>{if(!this.running||this.starting||this._lodBuildUnit!==unit||unit.model.disposed||unit._lodDesiredDetail!==target){const error=new Error('LOD build is no longer needed.');error.name='AbortError';throw error;}};
          const job=unit.setDetailAsync(target,yieldFrame,stillNeeded,4);
          this._lodBuildPromise=job.catch(error=>{if(error.name!=='AbortError')console.error(error);}).finally(()=>{
            if(this._lodBuildUnit===unit){this._lodBuildUnit=null;this._lodBuildPromise=null;}
          });
        }
      }
      if(document.getElementById('performancePanel').open&&now-this.lastCacheStats>500){
        this.lastCacheStats=now;
        const cache=R.AppearanceCache.stats(),signals=cache.keyStats.slice(0,3).map(item=>item.detail+': '+item.id+' · '+item.hits+' trafień / '+item.misses+' chybień · '+item.evictions+' ewikcji / '+item.hitsAfterEviction+' powrotów').join('\n');
        const memory=typeof performance!=='undefined'?performance.memory:null,usedHeap=memory&&Number.isFinite(memory.usedJSHeapSize)?memory.usedJSHeapSize:0;
        const renderMemory=this.renderer.info.memory,geometryBufferBytes=sceneGeometryBufferBytes(this.scene,this._sceneMemoryScratch);
        this._memoryHighWater.heapBytes=Math.max(this._memoryHighWater.heapBytes,usedHeap);
        this._memoryHighWater.geometries=Math.max(this._memoryHighWater.geometries,renderMemory.geometries||0);
        this._memoryHighWater.textures=Math.max(this._memoryHighWater.textures,renderMemory.textures||0);
        this._memoryHighWater.geometryBufferBytes=Math.max(this._memoryHighWater.geometryBufferBytes,geometryBufferBytes);
        const heapText=usedHeap?'JS heap Chromium: '+(usedHeap/1048576).toFixed(1)+' / '+(this._memoryHighWater.heapBytes/1048576).toFixed(1)+' MiB teraz / szczyt zaobserwowany':'JS heap: API pomiaru niedostępne';
        const renderText='Three.js zasoby: '+(renderMemory.geometries||0)+' / '+this._memoryHighWater.geometries+' geometrii, '+(renderMemory.textures||0)+' / '+this._memoryHighWater.textures+' tekstur teraz / szczyt (liczby obiektów, nie bajty GPU)';
        const activePeak=Number.isFinite(cache.peakActiveBytes)?cache.peakActiveBytes:cache.activeBytes;
        document.getElementById('cacheStats').textContent='Bufory geometrii CPU: '+(cache.activeBytes/1048576).toFixed(1)+' aktywne / '+(activePeak/1048576).toFixed(1)+' MiB szczyt appearance + '+(cache.inactiveBytes/1048576).toFixed(1)+' cache MiB (limit cache '+(cache.limit/1048576).toFixed(0)+' MiB) · '+cache.entries+' wpisów · '+cache.hits+' trafień\n'+heapText+'\n'+renderText+'\nNajczęściej tracone klucze (id skrócone):\n'+(signals||'Brak danych o użyciu');
        document.getElementById('cacheStats').textContent+='\nBufory geometrii aktywnej sceny: '+formatBytes(geometryBufferBytes)+' / '+formatBytes(this._memoryHighWater.geometryBufferBytes)+' teraz / szczyt (unikalne ArrayBuffer CPU; nie bajty GPU)';
        const plantMemory=R.PlantGenerator&&R.PlantGenerator.memoryStats?R.PlantGenerator.memoryStats():null;
        if(plantMemory)document.getElementById('cacheStats').textContent+='\nZ tego: geometria roślin w cache: '+formatBytes(plantMemory.retainedGeometryBytes)+' / '+formatBytes(plantMemory.peakRetainedGeometryBytes)+' teraz / szczyt (unikalne bufory; podzbiór aktywnej sceny) · '+plantMemory.worldEntries+' warianty LOD, '+plantMemory.timberEntries+' pnie';
        const sceneScratch=sceneObjectStats(this.scene,this._sceneStatsScratch),renderStats=this.renderer.info.render||{};const runtimeStats=this.battlefield?.runtimeStats?.();const t=this._frameTelemetry;document.getElementById('cacheStats').textContent+='\nKlatka ms: symulacja '+t.simulationMs.toFixed(2)+' · prezentacja '+t.presentationMs.toFixed(2)+' · submit renderera '+t.rendererMs.toFixed(2)+' · całość '+t.frameMs.toFixed(2)+'\nRender: draw calls '+(renderStats.calls||0)+' · trójkąty '+(renderStats.triangles||0)+' · linie '+(renderStats.lines||0)+' · punkty '+(renderStats.points||0)+' · obiekty '+sceneScratch.objects+' · meshe '+sceneScratch.meshes+' ('+sceneScratch.visibleMeshes+' widocznych, '+sceneScratch.instancedMeshes+' instanced) · shadow casters '+sceneScratch.shadowCasters+'\nŚwiat budynków: '+(runtimeStats?JSON.stringify(runtimeStats.buildings):'brak')+'\nDestrukcja świata: '+(runtimeStats?JSON.stringify(runtimeStats.destruction):'brak');
      }
      const presentationStart=performance.now();for(const u of this.units)u.render(alpha);this._frameTelemetry.presentationMs=performance.now()-presentationStart;
      this.controls.update();const rendererStart=performance.now();this.renderer.render(this.scene,this.camera);this._frameTelemetry.rendererMs=performance.now()-rendererStart;this._frameTelemetry.frameMs=performance.now()-frameStart;this._frameTelemetry.frames++;
    }
    dispose(){
      if(!this.running)return;this.running=false;if(this.startController)this.startController.abort();cancelAnimationFrame(this.frameId);window.removeEventListener('resize',this.resize);document.removeEventListener('visibilitychange',this.visibility);
      for(const [field,handler] of this.rockSettingListeners){field.removeEventListener('input',handler);field.removeEventListener('change',handler);}this.rockSettingListeners.length=0;
      this.startButton.removeEventListener('click',this.onStart);this.qualitySelect.removeEventListener('change',this.onQualityChange);if(this._removeQualityListener)this._removeQualityListener();this.debug.dispose();if(this.battlefield)this.battlefield.dispose();else{this.units.forEach(u=>u.dispose());if(this.terrain)this.terrain.dispose();}R.AppearanceCache.clear();this.controls.dispose();this.renderer.dispose();R.InfantryMaterials.dispose();R.InfantryGear.dispose();
    }
  };
})();
