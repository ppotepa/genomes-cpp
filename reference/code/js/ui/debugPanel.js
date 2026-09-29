(function(){
  'use strict';
  const R=window.RTS,$=id=>document.getElementById(id);
  const GROUPS=[
    ['BODY',[
      ['heightGene','Height'],['speedGene','Speed'],['body.frameGene','Frame'],['body.massGene','Mass'],['body.musculatureGene','Musculature'],['body.adiposityGene','Adiposity'],
      ['body.shoulderBreadthGene','Shoulders'],['body.hipBreadthGene','Hips'],['body.torsoLegRatioGene','Torso / legs'],['body.armLengthGene','Arm length'],['body.legLengthGene','Leg length'],
      ['body.chestDepthGene','Chest depth'],['body.waistWidthGene','Waist'],['body.limbThicknessGene','Limb thickness'],['body.neckThicknessGene','Neck'],
      ['body.headScaleGene','Head scale'],['body.handScaleGene','Hands'],['body.footScaleGene','Feet'],['body.skinToneGene','Skin tone']
    ]],
    ['HEAD',[
      ['face.headWidthGene','Head width'],['face.headDepthGene','Head depth'],['face.headLengthGene','Head length'],['face.foreheadWidthGene','Forehead width'],['face.foreheadSlopeGene','Forehead slope'],['face.templeWidthGene','Temples'],['face.browRidgeGene','Brow ridge'],
      ['face.jawWidthGene','Jaw width'],['face.jawLengthGene','Jaw length'],['face.jawAngleGene','Jaw angle'],['face.chinWidthGene','Chin width'],['face.chinHeightGene','Chin height'],['face.chinProjectionGene','Chin projection'],
      ['face.cheekboneWidthGene','Cheekbones'],['face.cheekboneHeightGene','Cheek height'],['face.cheekFullnessGene','Cheek fullness'],['face.midfaceProjectionGene','Midface projection']
    ]],
    ['EYES / BROWS',[
      ['face.eyeSpacingGene','Eye spacing'],['face.eyeWidthGene','Eye width'],['face.eyeHeightGene','Eye height'],['face.eyeRoundnessGene','Eye roundness'],['face.eyeDepthGene','Eye depth'],['face.eyeTiltGene','Eye tilt'],['face.eyeVerticalGene','Eye vertical'],['face.eyeColorGene','Eye colour'],
      ['face.browHeightGene','Brow height'],['face.browThicknessGene','Brow thickness'],['face.browTiltGene','Brow tilt'],['face.browSpacingGene','Brow spacing']
    ]],
    ['NOSE',[
      ['face.noseWidthGene','Nose width'],['face.noseLengthGene','Nose length'],['face.noseProjectionGene','Nose projection'],['face.noseBridgeGene','Bridge'],['face.noseTipWidthGene','Tip width'],['face.noseTipRotationGene','Tip rotation'],['face.nostrilWidthGene','Nostrils']
    ]],
    ['MOUTH / EARS',[
      ['face.mouthWidthGene','Mouth width'],['face.upperLipGene','Upper lip'],['face.lowerLipGene','Lower lip'],['face.mouthHeightGene','Mouth height'],['face.earSizeGene','Ear size'],['face.earAngleGene','Ear angle']
    ]],
    ['HAIR',[
      ['face.hairColorGene','Hair colour'],['face.hairBrightnessGene','Hair brightness'],['face.hairStyleGene','Hair style'],['face.hairDensityGene','Hair density'],['face.hairThicknessGene','Hair thickness'],['face.hairVolumeGene','Hair volume'],['face.hairlineGene','Hairline'],['face.templeRecessionGene','Temple recession'],['face.widowPeakGene','Widow peak']
    ]],
    ['NEUTRAL / MICRO',[
      ['face.restingEyeOpennessGene','Resting eye open'],['face.restingBrowGene','Resting brow'],['face.restingMouthGene','Resting mouth'],['face.eyeHeightAsymmetryGene','Eye asymmetry'],['face.browHeightAsymmetryGene','Brow asymmetry'],['face.mouthCornerAsymmetryGene','Mouth asymmetry'],['face.earAsymmetryGene','Ear asymmetry'],
      ['face.blinkRateGene','Blink rate'],['face.blinkSpeedGene','Blink speed'],['face.gazeRestlessnessGene','Gaze restlessness'],['face.expressionScaleGene','Expression scale'],['face.eyeExpressionGene','Eye expression'],['face.mouthExpressionGene','Mouth expression'],['face.browExpressionGene','Brow expression']
    ]]
  ];

  const rowNodes=new WeakMap();
  function rows(id,items){
    const dl=$(id);let nodes=rowNodes.get(dl);
    if(!nodes){nodes=[];for(let i=0;i+1<dl.children.length;i+=2)nodes.push([dl.children[i],dl.children[i+1]]);rowNodes.set(dl,nodes);}
    while(nodes.length>items.length){const pair=nodes.pop();pair[0].remove();pair[1].remove();}
    for(let i=0;i<items.length;i++){
      let pair=nodes[i];if(!pair){const dt=document.createElement('dt'),dd=document.createElement('dd');dl.append(dt,dd);pair=nodes[i]=[dt,dd];}
      const key=String(items[i][0]),value=String(items[i][1]);if(pair[0].textContent!==key)pair[0].textContent=key;if(pair[1].textContent!==value)pair[1].textContent=value;
    }
  }
  function setValue(el,value){value=String(value);if(el.value!==value)el.value=value;}
  function setText(el,value){value=String(value);if(el.textContent!==value)el.textContent=value;}
  function getPath(obj,path){return path.split('.').reduce((a,k)=>a&&a[k],obj);}

  R.DebugPanel=class DebugPanel{
    constructor(game){
      this.game=game;this.modal=$('unitsModal');this.preview=null;this.isOpen=false;this.activeTab='model';this.lastFocus=null;this.overrides={};this.variation=1;this.geneControls={};this._listeners=[];this._disposed=false;
      this.buildGenomeEditor();this.equipmentEditor=new R.EquipmentEditor(this);this.weaponEditor=new R.WeaponEditor(this);$('openUnits').disabled=false;
       this.listen($('grid5'),'change',e=>{if(game.terrain)game.terrain.setSmallGridVisible(e.target.checked);});this.listen($('grid100'),'change',e=>{if(game.terrain)game.terrain.setBigGridVisible(e.target.checked);});
      this.listen($('openUnits'),'click',()=>this.open());this.listen($('closeUnits'),'click',()=>this.close());this.listen(this.modal,'click',e=>{if(e.target.dataset.dismiss)this.close();});
      this.listen(document,'keydown',e=>{if(!this.isOpen)return;if(e.key==='Escape'){this.close();e.preventDefault();}if(e.key==='Tab'){const list=Array.from(this.modal.querySelectorAll('button:not([disabled]),select,input,summary')).filter(el=>el.getClientRects().length),first=list[0],last=list[list.length-1];if(e.shiftKey&&document.activeElement===first){last.focus();e.preventDefault();}else if(!e.shiftKey&&document.activeElement===last){first.focus();e.preventDefault();}}});
      document.querySelectorAll('[data-tab]').forEach(b=>this.listen(b,'click',()=>this.tab(b.dataset.tab)));
      this.listen($('labSide'),'change',()=>this.rebuild());this.listen($('labSeed'),'change',()=>{this.overrides={};this.equipmentEditor.resetUnitSeed(Number($('labSeed').value));this.rebuild();});
      this.listen($('labState'),'change',()=>{if(this.preview){this.preview.setState($('labState').value);this.refresh();}});
      for(const [id,key,label]of [['postureAmount','crouch','postureRequestLabel'],['locomotionSpeed','speedMps','locomotionSpeedLabel']]) {
        this.listen($(id),'input',()=>{
          if(!this.preview||!this.preview.unit)return;
          const value=Number($(id).value);$(label).value=value.toFixed(2)+(key==='speedMps'?' m/s':'');
          this.preview.setLocomotion({[key]:value});this.refresh();
        });
      }
      this.listen($('labExpression'),'change',()=>{if(this.preview){this.preview.setExpression($('labExpression').value,Number($('expressionIntensity').value));this.refresh();}});
      this.listen($('expressionIntensity'),'input',()=>{const v=Number($('expressionIntensity').value);$('expressionIntensityLabel').value=v.toFixed(2);if(this.preview){this.preview.setExpression($('labExpression').value,v);this.refresh();}});
      this.listen($('labCamera'),'change',()=>{if(this.preview)this.preview.setView($('labCamera').value);});
      for(const [id,key]of[['showSurface','surface'],['showWire','wire'],['showBones','bones'],['showColliders','colliders'],['showBounds','bounds']])this.listen($(id),'change',e=>{if(this.preview){this.preview.settings[key]=e.target.checked;this.preview.updateSettings();}});
      this.listen($('pauseAnimation'),'change',e=>{if(this.preview)this.preview.setPaused(e.target.checked);});this.listen($('cyclePhase'),'input',e=>{if(this.preview){$('pauseAnimation').checked=true;this.preview.seek(Number(e.target.value));this.refresh();}});
      this.listen($('genomeVariation'),'input',e=>{this.variation=Number(e.target.value);$('genomeVariationLabel').value=this.variation.toFixed(2);});
      this.listen($('genomeVariation'),'change',()=>this.rebuild());this.listen($('resetGenome'),'click',()=>{this.overrides={};this.variation=1;$('genomeVariation').value='1';$('genomeVariationLabel').value='1.00';this.rebuild();});
    }
    listen(target,type,handler,options){target.addEventListener(type,handler,options);this._listeners.push([target,type,handler,options]);return handler;}
    buildGenomeEditor(){
      const host=$('genomeEditor');
      for(const [group,genes]of GROUPS){const section=document.createElement('section');section.className='genome-group';const h=document.createElement('h3');h.textContent=group;section.appendChild(h);
        for(const [path,label]of genes){const row=document.createElement('div');row.className='gene-row';const l=document.createElement('label'),out=document.createElement('output'),input=document.createElement('input');l.textContent=label;out.value='0.500';input.type='range';input.min='0';input.max='1';input.step='.001';input.value='.5';input.dataset.gene=path;
          this.listen(input,'input',()=>{out.value=Number(input.value).toFixed(3);});this.listen(input,'change',()=>{this.overrides[path]=Number(input.value);row.classList.add('overridden');this.rebuild();});
          row.append(l,out,input);section.appendChild(row);this.geneControls[path]={input,out,row};}
        host.appendChild(section);
      }
    }
    syncGenomeEditor(){if(!this.preview||!this.preview.unit)return;const genome=this.preview.unit.genome;for(const path in this.geneControls){const c=this.geneControls[path],v=getPath(genome,path);if(Number.isFinite(v)&&document.activeElement!==c.input){setValue(c.input,v);setValue(c.out,v.toFixed(3));}c.row.classList.toggle('overridden',Object.prototype.hasOwnProperty.call(this.overrides,path));}}
    message(text){const el=$('labMessage');setText(el,text);const hidden=!text;if(el.hidden!==hidden)el.hidden=hidden;}
    open(){this.lastFocus=document.activeElement;this.modal.hidden=false;this.isOpen=true;this.game.controls.enabled=false;this.game.clock.reset();try{if(!this.preview){this.preview=new R.UnitPreview($('unitPreview'),this.game.sides);this.preview.onReadout=()=>this.refresh();this.rebuild();}this.preview.setVisible(true);requestAnimationFrame(()=>this.preview.resize());$('closeUnits').focus();}catch(error){console.error(error);this.message('Błąd podglądu: '+error.message);}}
    close(){this.modal.hidden=true;this.isOpen=false;if(this.preview)this.preview.setVisible(false);this.game.controls.enabled=true;this.game.clock.reset();if(this.lastFocus)this.lastFocus.focus();}
    tab(name){this.activeTab=name;document.querySelectorAll('[data-tab]').forEach(b=>b.setAttribute('aria-selected',String(b.dataset.tab===name)));document.querySelectorAll('[data-page]').forEach(p=>p.hidden=p.dataset.page!==name);this.refresh();}
    rebuild(){
      if(!this.preview)return;const seed=Number($('labSeed').value);if($('labSeed').value.trim()===''||!Number.isInteger(seed)||seed<0||seed>4294967295){this.message('Seed musi być liczbą całkowitą z zakresu 0–4294967295.');return;}
      const first=!this.preview.unit,side=$('labSide').value,state=$('labState').value,expression=$('labExpression').value,intensity=Number($('expressionIntensity').value),genomeOptions={variation:this.variation,overrides:{...this.overrides}},equipmentOptions=this.equipmentEditor.options;
      this.message('Generowanie podglądu…');
      this.preview.generateAsync(side,seed,state,expression,intensity,genomeOptions,equipmentOptions).then(unit=>{
        if(!this.preview||this.preview.unit!==unit)return;
        this.equipmentEditor.sync(unit.equipment);this.preview.setPaused($('pauseAnimation').checked);if(first)this.preview.setView($('labCamera').value);this.message('');this.refresh();
      }).catch(error=>{if(error&&error.name==='AbortError')return;console.error(error);this.message('Błąd generowania: '+error.message);});
    }
    refresh(){
      const p=this.preview;if(!p||!p.unit)return;const u=p.unit,g=u.genome,f=u.phenotype,a=u.animator,v=p.validation,b=f.body;
      if(this.activeTab==='equipment')this.weaponEditor.sync();
      const loc=u.locomotion,biped=R.PostureProfile.isBiped(a.state);
      for(const id of ['postureAmount','locomotionSpeed']){const el=$(id),disabled=!biped;if(el.disabled!==disabled)el.disabled=disabled;}
      const speed=$('locomotionSpeed'),speedMax=String(f.runSpeed);if(speed.max!==speedMax)speed.max=speedMax;
      if(document.activeElement!==$('postureAmount'))setValue($('postureAmount'),loc.requestedCrouch);
      if(document.activeElement!==$('locomotionSpeed'))setValue($('locomotionSpeed'),loc.requestedSpeedMps);
      setValue($('postureRequestLabel'),loc.requestedCrouch.toFixed(2));
      setValue($('locomotionSpeedLabel'),loc.requestedSpeedMps.toFixed(2)+' m/s');
      if(this.activeTab==='animation')rows('locomotionData',[
        ['Sterowanie',biped?(loc.custom?'własne parametry':'preset'):'osobna rodzina podparcia'],
        ['Obniżenie żądane / osiągnięte',biped?loc.requestedCrouch.toFixed(3)+' / '+loc.actualCrouch.toFixed(3):'—'],
        ['Prędkość żądana / faktyczna',biped?loc.requestedSpeedMps.toFixed(2)+' / '+a.motion.speed.toFixed(2)+' m/s':'—'],
        ['Limit prędkości',biped?R.PostureProfile.speedLimit(loc.actualCrouch,f).toFixed(2)+' m/s':'—'],
        ['Udział biegu',biped?loc.runWeight.toFixed(3):'—'],
        ['Udział sprintu',biped?loc.sprintWeight.toFixed(3):'—'],
        ['Pełny cykl',biped?loc.gait.cycleM.toFixed(3)+' m':'—'],
        ['Ograniczenie',biped?(loc.limitReason||'—'):'wybierz IDLE / WALK / RUN / CROUCH'],
        ['Przestawienia / awaryjne kotwice',(a.metrics.replants||0)+' / '+(a.metrics.reanchors||0)]
      ]);
      if(this.activeTab==='model'){
        rows('modelData' ,[['Strona',u.side.id],['Seed',u.seed],['heightGene',g.heightGene.toFixed(5)],['speedGene',g.speedGene.toFixed(5)],['Wzrost',f.height.toFixed(3)+' m'],['Barki',b.shoulderWidthScale.toFixed(3)+'×'],['Klatka',b.chestWidthScale.toFixed(3)+'×'],['Talia',b.waistWidthScale.toFixed(3)+'×'],['Biodra',b.hipWidthScale.toFixed(3)+'×'],['Ramiona',b.armThicknessScale.toFixed(3)+'×'],['Nogi',b.legThicknessScale.toFixed(3)+'×'],['Mnożnik prędkości',f.speedMultiplier.toFixed(3)]]);
        rows('geometryData',[['SkinnedMesh (ciało + gear)',v.meshes+(u.model.gearMesh?1:0)],['Wierzchołki',v.vertices.toLocaleString('pl-PL')],['Trójkąty',v.triangles.toLocaleString('pl-PL')],['Regiony powierzchni',v.materialGroups],['Trójkąty wyposażenia',u.model.gear?u.model.gear.triangles.toLocaleString('pl-PL'):0],['Gen. wersja',R.Config.GENERATOR_VERSION]]);
      }
      if(this.activeTab==='skeleton'){
        if(p.settings.bounds)a.poseBounds(p.bounds);
        rows('skeletonData',[['Kości',v.bones],['Wagi skinningu',v.ok?'poprawne':'BŁĄD'],['Kontrola anatomii',v.anatomyOK?'poprawna':'sprawdź'],['Zmodyfikowane relacje',v.adjustments],['Błąd sumy wag',v.maxWeightError.toExponential(2)],['Zmiana długości kości',u.rig.maxLengthError().toExponential(2)+' m'],['Bryły / stawy',u.physicsSchema.bodies.length+' / '+u.physicsSchema.joints.length],['Właściciel pozy',a.poseOwner],['Aktywna fizyka','nie'],['Błąd transformacji',R.RagdollSchema.roundtripError(u.rig,u.physicsSchema).toExponential(2)]]);
        setText($('bonesTree'),u.rig.anatomy.order.map(name=>{let n=name,depth=0;while(u.rig.anatomy.parents[n]){depth++;n=u.rig.anatomy.parents[n];}return'  '.repeat(depth)+name;}).join('\n'));
      }
      if(this.activeTab==='animation'){
        const animationRows=[['Stan',a.state],['Przejście',a.transition?'trwa':'—'],['Chód',f.walkSpeed.toFixed(2)+' m/s'],['Bieg',f.runSpeed.toFixed(2)+' m/s'],['Chód obniżony',f.crouchSpeed.toFixed(2)+' m/s'],['Czołganie',f.proneSpeed.toFixed(2)+' m/s'],['Faza',a.phase.toFixed(3)],['Stały krok','1/60 s'],['Kontakt L',a.metrics.footContact[0]],['Kontakt R',a.metrics.footContact[1]],['Dłoń L',a.metrics.handContact[0]],['Dłoń R',a.metrics.handContact[1]],['Prześwit buta L',(a.metrics.minBoot[0]*1000).toFixed(1)+' mm'],['Prześwit buta R',(a.metrics.minBoot[1]*1000).toFixed(1)+' mm'],['Siedzisko',a.metrics.seat],['Błąd celu IK',(a.metrics.maxIKError*1000).toFixed(1)+' mm']];
        if(p.settings.bounds){const size=a.poseBounds(p.bounds).getSize(p.size);animationRows.push(['Obwiednia X',size.x.toFixed(3)+' m'],['Obwiednia Y',size.y.toFixed(3)+' m'],['Obwiednia Z',size.z.toFixed(3)+' m']);}
        else animationRows.push(['Obwiednia','włącz „Obwiednia aktualnej pozy”, aby mierzyć']);
        rows('animationData',animationRows);
      }
      if(this.activeTab==='face'){
        const fg=g.face,ff=f.face,fa=u.faceAnimator,hex=x=>'#'+Number(x).toString(16).padStart(6,'0');
        const faceRows=[['headWidthGene',fg.headWidthGene.toFixed(4)],['jawWidthGene',fg.jawWidthGene.toFixed(4)],['eyeSpacingGene',fg.eyeSpacingGene.toFixed(4)],['eyeWidthGene',fg.eyeWidthGene.toFixed(4)],['eyeHeightGene',fg.eyeHeightGene.toFixed(4)],['noseWidthGene',fg.noseWidthGene.toFixed(4)],['hairStyleGene',fg.hairStyleGene.toFixed(4)],['Rozstaw oczu',(u.model.anatomy.faceLayout.eyes.L.x*f.height*2*1000).toFixed(0)+' mm'],['Oko W/H',ff.eyeWidthScale.toFixed(2)+' / '+ff.eyeHeightScale.toFixed(2)],['Szczęka',ff.jawWidthScale.toFixed(3)+'×'],['Kolor oczu',hex(ff.eyeColor)],['Kolor włosów',hex(ff.hairColor)],['Fryzura',R.InfantryHair.names[ff.hairStyle]],['Hair volume',(ff.hairVolume*f.height*1000).toFixed(0)+' mm'],['Neutral eye open',ff.neutralEyeOpen.toFixed(3)],['Dopasowania anatomii',u.model.anatomy.faceLayout.adjustments.length],['Oczodoły','otwory w skórze'],['Głowa / szyja','wspólna powierzchnia']];
        for(const item of u.model.anatomy.faceLayout.adjustments)faceRows.push([item.key,item.requested.toFixed(4)+' → '+item.resolved.toFixed(4)+' × H']);rows('faceData',faceRows);
        rows('faceRuntimeData',[['Mimika',fa.state],['Intensywność',fa.intensity.toFixed(2)],['Otwarcie oczu',fa.metrics.eyeOpen.toFixed(3)],['Blink',fa.metrics.blink.toFixed(3)],['Gaze yaw',(fa.metrics.gazeYaw*180/Math.PI).toFixed(1)+'°'],['Gaze pitch',(fa.metrics.gazePitch*180/Math.PI).toFixed(1)+'°']]);
      }
      if(this.activeTab==='animation'){
        if(document.activeElement!==$('cyclePhase'))setValue($('cyclePhase'),a.phase);setValue($('phaseLabel'),a.phase.toFixed(3));
      }
      if(this.activeTab==='genome')this.syncGenomeEditor();
    }
    frame(now){if(this.preview)this.preview.frame(now);}
    dispose(){
      if(this._disposed)return;this._disposed=true;
      for(const [target,type,handler,options] of this._listeners)target.removeEventListener(type,handler,options);
      this._listeners.length=0;
      if(this.weaponEditor)this.weaponEditor.dispose();
      if(this.equipmentEditor)this.equipmentEditor.dispose();
      if(this.preview){this.preview.dispose();this.preview=null;}
      $('openUnits').disabled=true;
    }
  };
})();
