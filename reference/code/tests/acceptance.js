(function () {
  'use strict';
  const R=window.RTS;
  R.runAcceptanceTests=async function () {
    // Loader enters this function after all production modules have loaded.
    if(!R.CoreChecks)await new Promise((resolve,reject)=>{const s=document.createElement('script');s.src=new URL('check_core.js',location.href).href;s.onload=resolve;s.onerror=reject;document.head.appendChild(s);});
    const report={version:R.Config.VERSION,environment:'Browser + actual Three.js '+THREE.REVISION,userAgent:navigator.userAgent,results:[]};
    const side=new R.Side('SIDE_A',R.Config.SIDES.SIDE_A);
    const test=async(name,fn)=>{
      const result={name,ok:false};try{result.details=await fn();result.ok=true;}catch(e){result.error=e.message;console.error(e);}
      report.results.push(result);const row=document.createElement('tr');
      for(const value of [result.ok?'PASS':'FAIL',name,result.ok?JSON.stringify(result.details):result.error]){const cell=document.createElement('td');cell.textContent=value;row.appendChild(cell);}
      row.className=result.ok?'pass':'fail';document.getElementById('results').appendChild(row);
      await new Promise(resolve=>setTimeout(resolve,0));
    };
    for(const seed of [0,1001,1003,2002,2003,63])await test('Anatomia / oczy / zamknięcie: seed '+seed,()=>{
      const model=R.InfantryFactory.createModel(side,R.InfantryGenome.express(R.InfantryGenome.create(seed)),'world');
      try{return R.CoreChecks.verify(model);}finally{model.dispose();}
    });
    await test('Różnorodność = 0',()=>{const g=R.InfantryGenome.applyVariation(R.InfantryGenome.create(1003),0);R.CoreChecks.assert(g.heightGene===.5&&Object.values(g.face).every(v=>v===.5),'Niezerowa różnorodność');return true;});
    await test('Nowe postawy: orientacje i brak rozciągania kości',()=>{
      const unit=new R.InfantryUnit({id:'POSTURES',side,seed:2003,detail:'world'}),states=[];
      try {
        for(const state of ['SITTING','CROUCH','CROUCH_WALK','PRONE','PRONE_MOVE']) {
          unit.animator.setState(state,true);unit.seek(.25);
          for(const b of unit.rig.bones) R.CoreChecks.assert(b.matrixWorld.elements.every(Number.isFinite),'Niepoprawna transformacja '+b.name);
          R.CoreChecks.assert(unit.rig.maxLengthError()<1e-5,'Rozciągnięcie kości');
          states.push(state);
        }
        return {states,note:'Próbki geometryczne, nie ocena płynności.'};
      } finally {unit.dispose();}
    });
    await test('Ciągła postawa / zachowanie fazy',()=>{
      const unit=new R.InfantryUnit({id:'CONTINUOUS',side,seed:1003,detail:'world'});
      try{
        const depths=[];
        for(const crouch of [0,.25,.5,.75,1]){
          unit.setLocomotion({crouch,speedMps:0});unit.seek(.31);
          depths.push(unit.rig.modelPoint('head',new THREE.Vector3()).y);
          R.CoreChecks.assert(Math.abs(unit.animator.phase-.31)<1e-8,'Reset fazy');
          R.CoreChecks.assert(unit.rig.maxLengthError()<1e-5,'Rozciągnięcie kości');
        }
        for(let i=1;i<depths.length;i++)R.CoreChecks.assert(depths[i]<depths[i-1],'Brak ciągłego obniżenia');
        return {headPivotHeights:depths};
      }finally{unit.dispose();}
    });
    await test('Sprint: zakres ruchu, przejścia i kontakty w świecie',async()=>{
      if(!R.SprintChecks)await new Promise((resolve,reject)=>{const s=document.createElement('script');s.src=new URL('sprint_checks.js',location.href).href;s.onload=resolve;s.onerror=reject;document.head.appendChild(s);});
      const checks=R.SprintChecks.run();
      const failed=checks.filter(c=>!c.ok);
      R.CoreChecks.assert(failed.length===0,failed.map(c=>c.name+': '+c.error).join('; '));
      return checks;
    });
    await test('WebGL: skinning + morphs + mimika',()=>{
      const renderer=new THREE.WebGLRenderer({antialias:true,preserveDrawingBuffer:true});renderer.setSize(640,420);renderer.outputEncoding=THREE.sRGBEncoding;
      document.getElementById('gpuPreview').appendChild(renderer.domElement);
      const scene=new THREE.Scene();scene.background=new THREE.Color(0x253038);scene.add(new THREE.HemisphereLight(0xffffff,0x504c43,.85));
      const sun=new THREE.DirectionalLight(0xfff2dc,1.3);sun.position.set(-2,4,5);scene.add(sun);
      const unit=new R.InfantryUnit({id:'GPU',side,seed:2003,state:'CROUCH',detail:'high'});scene.add(unit.root);unit.seek(.25);
      const head=unit.rig.byName.head.getWorldPosition(new THREE.Vector3()),camera=new THREE.PerspectiveCamera(34,640/420,.01,15);
      camera.position.copy(head).add(new THREE.Vector3(.13,.04,1.05));camera.lookAt(head.clone().add(new THREE.Vector3(0,.025,.015)));
      const target=new THREE.WebGLRenderTarget(128,128),pixels=new Uint8Array(128*128*4),draws=[];
      for(const expression of ['NEUTRAL','FEAR','EYES_CLOSED']){
        unit.setExpression(expression);unit.seek(.25);unit.render(1);renderer.setRenderTarget(target);renderer.render(scene,camera);renderer.readRenderTargetPixels(target,0,0,128,128,pixels);
        let visible=0;for(let i=0;i<pixels.length;i+=4)if(pixels[i]>75)visible++;
        R.CoreChecks.assert(visible>30,'Brak widocznego modelu po renderze '+expression);
        R.CoreChecks.assert(renderer.getContext().getError()===0,'Błąd WebGL '+expression);
        draws.push({expression,visiblePixels:visible});
      }
      renderer.setRenderTarget(null);unit.setExpression('NEUTRAL');unit.seek(.25);unit.render(1);renderer.render(scene,camera);target.dispose();
      window.addEventListener('pagehide',()=>{unit.dispose();renderer.dispose();},{once:true});
      return {draws,note:'Kontrola wykonania renderu, nie ocena estetyki ani wszystkich kombinacji genów.'};
    });
    report.passed=report.results.filter(x=>x.ok).length;report.failed=report.results.length-report.passed;
    document.getElementById('testSummary').textContent=report.passed+' poprawnych / '+report.failed+' błędów';
    document.getElementById('jsonReport').textContent=JSON.stringify(report,null,2);
    window.RTS_TEST_REPORT=report;
  };
})();
