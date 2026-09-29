(function () {
  'use strict';
  const R=window.RTS,$=id=>document.getElementById(id);
  R.EquipmentEditor=class EquipmentEditor {
    constructor(debug){
      this.debug=debug;this.controls={};this._listeners=[];this._generated=[];this._disposed=false;
      this.options={loadout:'RIFLEMAN',seed:R.EquipmentSeed.fromUnit(1001),overrides:{},palette:'side',wear:.25};
      for(const profile of Object.values(R.InfantryLoadouts)){
        const o=document.createElement('option');o.value=profile.id;o.textContent=profile.label;$('gearLoadout').appendChild(o);this._generated.push(o);
      }
      const groups={};
      for(const [slot,schema]of Object.entries(R.EquipmentSlots)){
        if(!groups[schema.group]){
          const d=document.createElement('details');d.className='gear-group';d.open=['GŁOWA','TUŁÓW'].includes(schema.group);
          const s=document.createElement('summary');s.textContent=schema.group;d.appendChild(s);$('equipmentEditor').appendChild(d);this._generated.push(d);groups[schema.group]=d;
        }
        const label=document.createElement('label');label.className='field gear-field';
        const caption=document.createElement('span');caption.textContent=schema.label;const select=document.createElement('select');select.id='gearSlot_'+slot;
        if(!schema.required){const o=document.createElement('option');o.value='';o.textContent='Brak';select.appendChild(o);}
        for(const item of R.EquipmentCatalog.forSlot(slot)){
          const o=document.createElement('option');o.value=item.id;o.textContent=item.label;select.appendChild(o);
        }
        this.listen(select,'change',()=>this.change(eq=>eq.withSlot(slot,select.value||null)));
        label.append(caption,select);groups[schema.group].appendChild(label);this.controls[slot]={select,label};
      }
      this.listen($('gearLoadout'),'change',()=>this.change(eq=>eq.clone({loadout:$('gearLoadout').value,overrides:{}})));
      this.listen($('gearSeed'),'change',()=>{
        const raw=$('gearSeed').value,seed=Number(raw);
        if(raw.trim()===''||!Number.isInteger(seed)||seed<0||seed>4294967295){debug.message('Seed wyposażenia: liczba całkowita 0–4294967295.');return;}
        this.change(eq=>eq.clone({seed,overrides:{}}));
      });
      this.listen($('resetEquipment'),'click',()=>this.change(eq=>eq.clone({overrides:{}})));
      this.listen($('gearPalette'),'change',()=>this.change(eq=>eq.clone({palette:$('gearPalette').value})));
      this.listen($('gearWear'),'input',()=>{$('gearWearLabel').value=Number($('gearWear').value).toFixed(2);});
      this.listen($('gearWear'),'change',()=>this.change(eq=>eq.clone({wear:Number($('gearWear').value)})));
    }
    listen(target,type,handler,options){target.addEventListener(type,handler,options);this._listeners.push([target,type,handler,options]);return handler;}
    dispose(){
      if(this._disposed)return;this._disposed=true;
      for(const [target,type,handler,options] of this._listeners)target.removeEventListener(type,handler,options);
      this._listeners.length=0;
      for(const element of this._generated)element.remove();
      this._generated.length=0;this.controls={};$('equipmentData').replaceChildren();
    }
    resetUnitSeed(seed){if(Number.isInteger(seed)&&seed>=0&&seed<=4294967295)this.options={...this.options,seed:R.EquipmentSeed.fromUnit(seed),overrides:{}};}
    change(build){
      const preview=this.debug.preview;if(!preview||!preview.unit)return;
      try{
        const next=build(preview.unit.equipment);preview.setEquipment(next);this.sync(next);this.debug.message('');this.debug.refresh();
      }catch(error){console.error(error);this.debug.message('Wyposażenie: '+error.message);this.sync(preview.unit.equipment);}
    }
    sync(eq){
      if(!eq)return;
      this.options={loadout:eq.loadout,seed:eq.seed,overrides:{...eq.overrides},palette:eq.palette,wear:eq.wear};
      if(document.activeElement!==$('gearLoadout'))$('gearLoadout').value=eq.loadout;
      if(document.activeElement!==$('gearSeed'))$('gearSeed').value=String(eq.seed);
      $('gearPalette').value=eq.palette;
      if(document.activeElement!==$('gearWear'))$('gearWear').value=String(eq.wear);
      $('gearWearLabel').value=eq.wear.toFixed(2);
      const profile=R.InfantryLoadouts[eq.loadout];$('gearDescription').textContent=profile?profile.description:'';
      for(const [slot,c]of Object.entries(this.controls)){
        if(document.activeElement!==c.select)c.select.value=eq.slots[slot]?eq.slots[slot].definitionId:'';
        c.label.classList.toggle('overridden',Object.prototype.hasOwnProperty.call(eq.overrides,slot));
      }
      const unit=this.debug.preview?.unit,g=unit?.model.gear;
      $('equipmentData').replaceChildren();
      for(const [key,value]of [
        ['Profil',profile?.label||'—'],['Elementy',eq.count],['Masa zestawu',eq.totalWeightKg.toFixed(2)+' kg'],
        ['Ręcznie zmienione sloty',Object.keys(eq.overrides).length],['Geometria gear',g?g.triangles.toLocaleString('pl-PL')+' trójkątów':'—'],
        ['Generowanie','proceduralne / seed'],['Montaż','wspólny szkielet'],['Wpływ masy na ruch','nieaktywny']
      ]){const dt=document.createElement('dt'),dd=document.createElement('dd');dt.textContent=key;dd.textContent=String(value);$('equipmentData').append(dt,dd);}
    }
  };
})();
