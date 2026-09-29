(function () {
  'use strict';
  const R=window.RTS,$=id=>document.getElementById(id);
  const rowNodes=new WeakMap();
  function setValue(el,value){value=String(value);if(el.value!==value)el.value=value;}
  function setDisabled(el,value){value=!!value;if(el.disabled!==value)el.disabled=value;}
  function setHidden(el,value){value=!!value;if(el.hidden!==value)el.hidden=value;}
  function syncRows(host,items){
    let nodes=rowNodes.get(host);if(!nodes){nodes=[];for(let i=0;i+1<host.children.length;i+=2)nodes.push([host.children[i],host.children[i+1]]);rowNodes.set(host,nodes);}
    while(nodes.length>items.length){const pair=nodes.pop();pair[0].remove();pair[1].remove();}
    for(let i=0;i<items.length;i++){
      let pair=nodes[i];if(!pair){const dt=document.createElement('dt'),dd=document.createElement('dd');host.append(dt,dd);pair=nodes[i]=[dt,dd];}
      const label=String(items[i][0]),value=String(items[i][1]);if(pair[0].textContent!==label)pair[0].textContent=label;if(pair[1].textContent!==value)pair[1].textContent=value;
    }
  }
  R.WeaponEditor=class WeaponEditor {
    constructor(debug) {
      this.debug=debug;this._listeners=[];this._disposed=false;
      const tab=document.createElement('button');tab.type='button';tab.id='tabWeapons';tab.dataset.tab='weapons';tab.setAttribute('role','tab');tab.setAttribute('aria-selected','false');tab.textContent='Broń';
      document.querySelector('.tabs').appendChild(tab);
      const page=document.createElement('section');page.dataset.page='weapons';page.hidden=true;page.setAttribute('role','tabpanel');page.setAttribute('aria-labelledby','tabWeapons');
      page.innerHTML=`<h2>Przedmiot w dłoniach</h2>
        <label class="field">Posiadana broń<select id="weaponSlot"></select></label>
        <button type="button" id="weaponAddPistol">Dodaj pistolet do wyposażenia</button>
        <label class="field">Chwyt<select id="weaponGrip"><option value="AUTO">Auto</option><option value="1H">Jedna ręka</option><option value="2H">Dwie ręce</option></select></label>
        <div class="weapon-actions"><button type="button" id="weaponDraw">Wyjmij</button><button type="button" id="weaponHolster">Schowaj</button></div>
        <label class="field">Podniesienie <output id="weaponReadyValue">0.75</output><input id="weaponReady" type="range" min="0" max="1" step=".01" value=".75"></label>
        <label class="field">Kierunek lewo / prawo <output id="weaponYawValue">0°</output><input id="weaponYaw" type="range" min="-48" max="48" step="1" value="0"></label>
        <label class="field">Kierunek góra / dół <output id="weaponPitchValue">0°</output><input id="weaponPitch" type="range" min="-28" max="28" step="1" value="0"></label>
        <div class="weapon-actions"><button type="button" id="weaponFire">Strzał</button><button type="button" id="weaponTrigger">Przytrzymaj ogień</button></div>
        <h2>Podgląd akcji w pauzie</h2>
        <label class="field">Akcja<select id="weaponAction"><option value="DRAW">Wyjmowanie</option><option value="HOLSTER">Chowanie</option><option value="SHOT">Odrzut — podgląd</option></select></label>
        <label class="field">Faza akcji <output id="weaponPhaseValue">0.000</output><input id="weaponPhase" type="range" min="0" max="1" step=".001" value="0"></label>
        <dl id="weaponData" class="readout"></dl>
        <p class="note">Listę posiadanych przedmiotów zmieniasz w Ekwipunek → Broń. Nóż i granat: wyjmowanie, trzymanie, chowanie. Strzały są efektami prezentacyjnymi, bez pocisków i obrażeń. Kamera nie steruje kierunkiem broni.</p>`;
      document.querySelector('.inspector-scroll').appendChild(page);
      this.listen($('weaponSlot'),'change',()=>this.change(w=>w.select($('weaponSlot').value||null)));
      this.listen($('weaponAddPistol'),'click',()=>{
        const p=debug.preview;if(!p?.unit)return;
        try {
          p.setEquipment(p.unit.equipment.withSlot('secondaryWeapon','sidearm'));
          p.unit.weapons.select('secondaryWeapon');
          debug.equipmentEditor?.sync(p.unit.equipment);debug.refresh();this.sync();
        } catch(e){debug.message(e.message);}
      });
      this.listen($('weaponGrip'),'change',()=>this.change(w=>{w.setGrip($('weaponGrip').value);if(debug.preview.paused)w.snapGrip();}));
      this.listen($('weaponReady'),'input',()=>this.change(w=>{w.setReadiness(Number($('weaponReady').value));if(debug.preview.paused){w.readiness=w.requestedReadiness;w.readySpring.x=w.readiness;w.readySpring.v=0;}}));
      for(const id of ['weaponYaw','weaponPitch'])this.listen($(id),'input',()=>this.change(w=>{w.setAim(Number($('weaponYaw').value)*Math.PI/180,Number($('weaponPitch').value)*Math.PI/180);if(debug.preview.paused){w.aimYaw=w.yaw;w.aimPitch=w.pitch;}}));
      this.listen($('weaponDraw'),'click',()=>this.play(w=>w.draw()));
      this.listen($('weaponHolster'),'click',()=>this.play(w=>w.holster()));
      this.listen($('weaponFire'),'click',()=>this.play(w=>w.fire()));
      const trigger=$('weaponTrigger');
      this.listen(trigger,'pointerdown',e=>{if(trigger.disabled)return;e.preventDefault();trigger.setPointerCapture(e.pointerId);this.play(w=>w.setTrigger(true));});
      const release=()=>{if(debug.preview?.unit)debug.preview.unit.weapons.setTrigger(false);};
      for(const event of ['pointerup','pointercancel','lostpointercapture'])this.listen(trigger,event,release);
      this.listen(trigger,'keydown',e=>{if((e.code==='Space'||e.code==='Enter')&&!e.repeat&&!trigger.disabled){e.preventDefault();this.play(w=>w.setTrigger(true));}});
      this.listen(trigger,'keyup',e=>{if(e.code==='Space'||e.code==='Enter'){e.preventDefault();release();}});
      this.listen(trigger,'blur',release);
      this.listen(window,'blur',release);this.listen(document,'visibilitychange',()=>{if(document.hidden)release();});
      this.listen($('weaponPhase'),'input',()=>{
        const p=debug.preview;if(!p?.unit)return;
        try {p.setPaused(true);$('pauseAnimation').checked=true;p.unit.weapons.seekAction($('weaponAction').value,Number($('weaponPhase').value));p.unit.seek(p.unit.animator.phase);p.unit.render(1);debug.refresh();}
        catch(e){debug.message(e.message);}
      });
    }
    listen(target,type,handler,options){target.addEventListener(type,handler,options);this._listeners.push([target,type,handler,options]);return handler;}
    dispose(){
      if(this._disposed)return;this._disposed=true;
      if(this.debug.preview?.unit)this.debug.preview.unit.weapons.setTrigger(false);
      for(const [target,type,handler,options] of this._listeners)target.removeEventListener(type,handler,options);
      this._listeners.length=0;
      const tab=$('tabWeapons'),page=document.querySelector('[data-page="weapons"]');if(tab)tab.remove();if(page)page.remove();
    }
    change(fn) {
      const p=this.debug.preview;if(!p?.unit)return;
      try {fn(p.unit.weapons);if(p.paused){p.unit.seek(p.unit.animator.phase);p.unit.render(1);}this.debug.message('');this.sync();}
      catch(e){this.debug.message(e.message);}
    }
    play(fn){const p=this.debug.preview;if(!p?.unit)return;p.setPaused(false);$('pauseAnimation').checked=false;this.change(fn);}
    sync() {
      const p=this.debug.preview;if(!p?.unit)return;const w=p.unit.weapons,select=$('weaponSlot');
      const signature=Object.entries(w.instances).map(([s,i])=>s+':'+i.item.definitionId).join('|');
      if(this.signature!==signature){
        this.signature=signature;select.replaceChildren();const none=document.createElement('option');none.value='';none.textContent='Brak';select.appendChild(none);
        for(const [slot,item]of Object.entries(w.instances)){const o=document.createElement('option');o.value=slot;o.textContent=R.EquipmentCatalog.get(item.item.definitionId).label;select.appendChild(o);}
      }
      setValue(select,w.selectedSlot||'');const item=w.active||w.selected;
      setHidden($('weaponAddPistol'),!!w.instances.secondaryWeapon);
      for(const o of $('weaponGrip').options)setDisabled(o,o.value!=='AUTO'&&(!item||!item.def.profiles.includes(o.value)));
      setValue($('weaponGrip'),w.requestedGrip);
      const values=[['weaponReady','weaponReadyValue',w.requestedReadiness,w.requestedReadiness.toFixed(2)],['weaponYaw','weaponYawValue',w.yaw*180/Math.PI,(w.yaw*180/Math.PI).toFixed(0)+'°'],['weaponPitch','weaponPitchValue',w.pitch*180/Math.PI,(w.pitch*180/Math.PI).toFixed(0)+'°']];
      for(const [id,label,value,text]of values){if(document.activeElement!==$(id))setValue($(id),value);setValue($(label),text);}
      setDisabled($('weaponDraw'),!w.selected||w.busy||w.state==='HELD');setDisabled($('weaponHolster'),!w.active);
      const canFire=w.canFire();setDisabled($('weaponFire'),!canFire);setDisabled($('weaponTrigger'),!canFire);
      setDisabled($('weaponPhase'),!w.selected);
      for(const o of $('weaponAction').options)if(o.value==='SHOT')setDisabled(o,!item?.def.firearm);
      if($('weaponAction').value==='SHOT'&&!item?.def.firearm)setValue($('weaponAction'),'DRAW');
      if(document.activeElement!==$('weaponPhase'))setValue($('weaponPhase'),w.phase);
      setValue($('weaponPhaseValue'),w.phase.toFixed(3));
      syncRows($('weaponData'),[['Stan',w.state],['Status',w.metrics.status],['Rodzina',item?.def.family||'—'],['Chwyt',w.grip||'—'],['Profil ruchu',w.metrics.handlingProfile||'—'],['Udział podparcia 2H',(w.metrics.supportWeight||0).toFixed(2)],['Ręka L / R',w.metrics.owners.join(' / ')],['Rzeczywiste podniesienie',w.readiness.toFixed(2)],['Błąd chwytu',(w.metrics.gripError*1000).toFixed(1)+' mm'],['Strzały (zdarzenia)',w.shotId]]);
    }
  };
})();
