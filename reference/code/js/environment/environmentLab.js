(function(){
  'use strict';
  const base=new URL('../../',document.currentScript.src),$=id=>document.getElementById(id);
  const script=url=>new Promise((resolve,reject)=>{const el=document.createElement('script');el.src=url;el.onload=resolve;el.onerror=()=>reject(Error('Nie udało się załadować '+url));document.head.appendChild(el);});
  async function dependency(paths,verify){for(const p of paths){try{await script(p);if(verify())return;}catch(e){/* Try the alternate CDN. */}}throw Error('Brak biblioteki 3D. Sprawdź połączenie z internetem.');}
  async function boot(){
    await dependency(['https://cdn.jsdelivr.net/npm/three@0.128.0/build/three.min.js','https://unpkg.com/three@0.128.0/build/three.min.js'],()=>window.THREE);
    await dependency(['https://cdn.jsdelivr.net/npm/three@0.128.0/examples/js/controls/OrbitControls.js','https://unpkg.com/three@0.128.0/examples/js/controls/OrbitControls.js'],()=>THREE.OrbitControls);
    for(const path of ['js/config.js','js/core/seededRandom.js','js/environment/plantCatalog.js','js/environment/plantGenerator.js','js/environment/rockGenerator.js','js/destruction/catalogs.js','js/destruction/solid.js','js/destruction/impactProfiles.js','js/destruction/materialImpact.js','js/destruction/rubbleField.js','js/destruction/projectileState.js','js/destruction/impactSolver.js','js/destruction/surfaceDamage.js','js/destruction/materialModel.js','js/destruction/projectileTrace.js','js/destruction/ballistics.js','js/destruction/physics.js','js/destruction/adapters.js','js/destruction/effects.js','js/destruction/demo.js'])await script(new URL(path,base).href);
    const R=window.RTS,scene=new THREE.Scene();scene.background=new THREE.Color(0xc5cebd);
    const renderer=new THREE.WebGLRenderer({antialias:true});let pixelRatio=0;renderer.outputEncoding=THREE.sRGBEncoding;renderer.toneMapping=THREE.ACESFilmicToneMapping;renderer.shadowMap.enabled=true;renderer.shadowMap.type=THREE.PCFSoftShadowMap;$('viewport').appendChild(renderer.domElement);
    const camera=new THREE.PerspectiveCamera(42,1,.05,500),controls=new THREE.OrbitControls(camera,renderer.domElement);controls.enableDamping=true;controls.maxPolarAngle=Math.PI*.495;
    scene.add(new THREE.HemisphereLight(0xe5eee2,0x756c4a,1.1));const sun=new THREE.DirectionalLight(0xffedd3,1.8);sun.position.set(-25,45,30);sun.castShadow=true;sun.shadow.mapSize.set(2048,2048);Object.assign(sun.shadow.camera,{left:-35,right:35,top:35,bottom:-35,near:1,far:120});sun.shadow.normalBias=.035;scene.add(sun);
    const floor=new THREE.Mesh(new THREE.PlaneGeometry(160,160),new THREE.MeshStandardMaterial({color:0x939a76,roughness:1}));floor.rotation.x=-Math.PI/2;floor.position.y=-.025;floor.receiveShadow=true;scene.add(floor);const grid=new THREE.GridHelper(80,80,0x7d896e,0xa3af96);grid.position.y=-.012;scene.add(grid);
    const category=new URLSearchParams(location.search).get('category')==='rocks'?'rocks':'plants',rocks=category==='rocks';
    const catalog=rocks?R.RockCatalog:R.EnvironmentCatalog.plants,genomeFactory=rocks?R.RockGenome:R.PlantGenome,generator=rocks?R.RockGenerator:R.PlantGenerator;
    for(const cat of R.EnvironmentCatalog.categories){const b=document.createElement('button');b.textContent=cat.name+(cat.available?'':' · w planie');b.disabled=!cat.available;if(cat.id===category)b.setAttribute('aria-current','page');b.onclick=()=>{location.href='environment.html?category='+cat.id;};$('categories').appendChild(b);}
    for(const s of Object.values(catalog)){const o=document.createElement('option');o.value=s.id;o.textContent=s.name;$('species').appendChild(o);}
    if(rocks){$("species").closest("label").firstChild.textContent="Typ";document.querySelector('header > span').textContent='Kamienie i skały · 8 typów';document.querySelector('.toolbar > span').textContent='Podgląd skały';$('viewport').setAttribute('aria-label','Interaktywny podgląd skały 3D');$('species').closest('details').querySelector('summary').textContent='Skała';$('season').closest('details').hidden=true;}
    const labels=rocks?{size:'Rozmiar',elongation:'Wydłużenie',flattening:'Spłaszczenie',roughness:'Nieregularność',roundness:'Zaokrąglenie',layering:'Warstwy',moss:'Mech'}:{stature:'Wysokość',spread:'Szerokość korony',branching:'Rozgałęzienia',crookedness:'Krzywizna',girth:'Grubość pnia',damage:'Ułamane gałęzie',foliage:'Gęstość liści',leafSize:'Wielkość liści',fruiting:'Owocowanie'};
    let genome,model,frameId=0,dirty=true,skeletonCache=null,skeletonCacheKey='',observer=null,disposed=false,rebuildTimer=0,sliderNeedsRebuild=false,activeBuild=null;
    const listeners=[];
    function listen(target,type,handler,options){target.addEventListener(type,handler,options);listeners.push([target,type,handler,options]);return handler;}
    function invalidate(){if(disposed)return;dirty=true;if(!frameId&&!document.hidden)frameId=requestAnimationFrame(loop);}
    listen(controls,'change',invalidate);
    const onVisibility=()=>{if(!document.hidden)invalidate();};listen(document,'visibilitychange',onVisibility);
    function frame(){const box=new THREE.Box3().setFromObject(model.root),size=box.getSize(new THREE.Vector3()),center=box.getCenter(new THREE.Vector3()),extent=Math.max(size.y,size.x,size.z,1);controls.target.copy(center);camera.position.copy(center).add(new THREE.Vector3(extent*.95,extent*.35,extent*1.65));controls.minDistance=.4;controls.maxDistance=extent*7;controls.update();invalidate();}
    function yieldBuildFrame(){return new Promise(resolve=>requestAnimationFrame(resolve));}
    async function rebuild(fit=false,detailOverride=null){
      if(disposed)return null;
      if(activeBuild)activeBuild.cancelled=true;
      const ticket={cancelled:false};activeBuild=ticket;
      const age=Number($('age').value),input=genomeFactory.create(genome.species,genome.seed,genome.genes),season=$('season').value,detail=detailOverride||$('detail').value;
      const key=[input.species,input.seed,age,input.genes.stature,input.genes.spread,input.genes.branching,input.genes.crookedness,input.genes.girth,input.genes.damage].join('|');
      const skeleton=key===skeletonCacheKey?skeletonCache:null;
      const check=()=>{if(disposed||ticket.cancelled||activeBuild!==ticket){const error=new Error('Plant preview build cancelled.');error.name='AbortError';throw error;}};
      try{
        const next=await generator.createAsync(input,{season,age,detail,skeleton,_sharedDeterministicSkeleton:!!skeleton},yieldBuildFrame,check,4);
        check();
        const previous=model;model=next;skeletonCache=next.skeleton;skeletonCacheKey=key;scene.add(next.root);
        if(previous)previous.dispose();
        $('ageValue').textContent=Math.round(age*100)+'%';$('latin').textContent=catalog[input.species].latin||catalog[input.species].name;const s=next.stats;$('stats').textContent=s.triangles.toLocaleString('pl-PL')+' trójkątów'+(rocks?'':' · '+s.leaves+' liści / pęczków · '+s.fruits+' owoców')+' · '+s.drawCalls+' draw calls';$('message').textContent='';if(fit)frame();window.environmentLab.model=next;window.environmentLab.genome=genome;
        invalidate();return next;
      }catch(error){if(error.name!=='AbortError'&&!disposed)$('message').textContent=error.message;return null;}
      finally{if(activeBuild===ticket)activeBuild=null;}
    }
    function scheduleSliderPreview(geneKey){
      // Fruit probability is not part of world-detail geometry.
      if(geneKey==='fruiting'&&$('detail').value==='world')return;
      sliderNeedsRebuild=true;
      if(rebuildTimer)clearTimeout(rebuildTimer);
      rebuildTimer=setTimeout(()=>{rebuildTimer=0;if(disposed)return;rebuild(false,'world');},180);
    }
    function finishSliderEdit(){
      if(rebuildTimer){clearTimeout(rebuildTimer);rebuildTimer=0;}
      if(!disposed&&sliderNeedsRebuild){sliderNeedsRebuild=false;rebuild();}
    }
    function geneUI(){for(const key of Object.keys(labels)){$(key).value=genome.genes[key];$(key+'Value').textContent=Math.round(genome.genes[key]*100)+'%';}}
    function fresh(){genome=genomeFactory.create($('species').value,Number($('seed').value));$('seed').value=genome.seed;geneUI();rebuild(true);}
    for(const [key,label] of Object.entries(labels)){const row=document.createElement('label');row.textContent=label;const output=document.createElement('output');output.id=key+'Value';row.appendChild(output);const slider=document.createElement('input');slider.type='range';slider.id=key;slider.min=0;slider.max=1;slider.step=.01;listen(slider,'input',()=>{genome.genes[key]=Number(slider.value);output.textContent=Math.round(slider.value*100)+'%';scheduleSliderPreview(key);});listen(slider,'change',finishSliderEdit);row.appendChild(slider);$('genes').appendChild(row);}
    function dispose(){if(disposed)return;disposed=true;if(activeBuild){activeBuild.cancelled=true;activeBuild=null;}if(rebuildTimer)clearTimeout(rebuildTimer);rebuildTimer=0;sliderNeedsRebuild=false;if(frameId)cancelAnimationFrame(frameId);frameId=0;if(observer)observer.disconnect();for(const [target,type,handler,options] of listeners)target.removeEventListener(type,handler,options);listeners.length=0;for(const element of document.querySelectorAll('#species,#seed,#reset,#randomize,#season,#detail,#renderQuality,#age,#frame,#export,#import,#genes input')){element.onchange=null;element.onclick=null;element.oninput=null;}if(model){model.dispose();model=null;}controls.dispose();floor.geometry.dispose();floor.material.dispose();grid.geometry.dispose();grid.material.dispose();renderer.dispose();renderer.domElement.remove();if(window.environmentLab===lab)window.environmentLab=null;}
    const lab={scene,camera,renderer,controls,rebuild,frame,dispose,get genome(){return genome;},set genome(value){genome=value;}};
    window.environmentLab=lab;
    R.DestructionDemo.button(document.querySelector('.toolbar'),()=>model?{renderer,controls,target:rocks?R.DestructionAdapters.rock(model):R.DestructionAdapters.plant(model),suspend:invalidate,resume:()=>{resize();invalidate();}}:null);
    listen($('species'),'change',fresh);listen($('seed'),'change',fresh);listen($('reset'),'click',fresh);listen($('randomize'),'click',()=>{$('seed').value=crypto.getRandomValues(new Uint32Array(1))[0];fresh();});listen($('season'),'change',()=>rebuild());listen($('detail'),'change',()=>rebuild());listen($('age'),'input',()=>{$('ageValue').textContent=Math.round(Number($('age').value)*100)+'%';scheduleSliderPreview();});listen($('age'),'change',finishSliderEdit);listen($('frame'),'click',frame);
    listen($('export'),'click',()=>{const blob=new Blob([JSON.stringify({genome,state:model.state},null,2)],{type:'application/json'}),url=URL.createObjectURL(blob),a=document.createElement('a');a.href=url;a.download=genome.species+'-'+genome.seed+'.json';a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);});
    $('import').onchange=async()=>{try{const file=$('import').files[0];if(!file)return;if(file.size>50000)throw Error('Plik DNA jest za duży.');const data=JSON.parse(await file.text()),dna=data.genome;if(!dna||dna.version!==genomeFactory.version||!catalog[dna.species]||!Number.isInteger(dna.seed)||dna.seed<0||dna.seed>4294967295||!dna.genes)throw Error('Nieprawidłowy format lub wersja DNA.');for(const key of Object.keys(labels).filter(key=>!(key==="girth"||key==="damage")||dna.genes[key]!==undefined))if(!Number.isFinite(dna.genes[key])||dna.genes[key]<0||dna.genes[key]>1)throw Error('Nieprawidłowy gen: '+key);genome=genomeFactory.create(dna.species,dna.seed,dna.genes);$('species').value=genome.species;$('seed').value=genome.seed;if(data.state){$('season').value=['spring','summer','autumn','winter'].includes(data.state.season)?data.state.season:'summer';$('age').value=Number.isFinite(data.state.age)?Math.max(.05,Math.min(1,data.state.age)):1;$('detail').value=data.state.detail==='world'?'world':'high';}geneUI();rebuild(true);}catch(e){$('message').textContent=e.message;}finally{$('import').value='';}};
    $('renderQuality').value=R.RenderQuality.get();listen($('renderQuality'),'change',()=>R.RenderQuality.set($('renderQuality').value));listen(window,'genomes-render-quality-change',()=>{$('renderQuality').value=R.RenderQuality.get();resize();});
    const resize=()=>{const el=$('viewport'),w=Math.max(1,el.clientWidth),h=Math.max(1,el.clientHeight),ratio=R.RenderQuality.ratio(w,h,devicePixelRatio||1,'preview');if(Math.abs(ratio-pixelRatio)>1e-4){pixelRatio=ratio;renderer.setPixelRatio(ratio);}renderer.setSize(w,h);camera.aspect=w/h;camera.updateProjectionMatrix();invalidate();};observer=new ResizeObserver(resize);observer.observe($('viewport'));resize();fresh();
    function loop(now){frameId=0;if(disposed||document.hidden)return;if(R.DestructionDemo.frame(renderer,now)){frameId=requestAnimationFrame(loop);return;}controls.update();if(dirty){renderer.render(scene,camera);dirty=false;}}
    listen(window,'pagehide',dispose,{once:true});
  }
  boot().catch(e=>{$('message').textContent=e.message;console.error(e);});
})();
