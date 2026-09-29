(function () {
  'use strict';
  const R=window.RTS;
  function hash(seed,text) {
    let h=(2166136261^(seed>>>0))>>>0;
    for(let i=0;i<text.length;i++)h=Math.imul(h^text.charCodeAt(i),16777619)>>>0;
    h=Math.imul(h^(h>>>16),0x7feb352d);h=Math.imul(h^(h>>>15),0x846ca68b);
    return (h^(h>>>16))>>>0;
  }
  function validSeed(value){if(!Number.isInteger(value)||value<0||value>4294967295)throw new RangeError('Seed wyposażenia: wymagane 0–4294967295.');return value>>>0;}
  R.EquipmentSeed=Object.freeze({hash,fromUnit:seed=>hash(seed,'EQUIPMENT/v1')});
  R.Equipment=class Equipment {
    constructor({unitSeed=0,seed=R.EquipmentSeed.fromUnit(unitSeed),slotSchema={},loadout=null,overrides={},palette='side',wear=.25}={}){
      this.seed=validSeed(seed);this.slotSchema=slotSchema;this.loadout=loadout;this.overrides={...overrides};this.palette=palette;this.wear=R.Math.clamp(Number(wear)||0,0,1);
      this.slots={};this.revision=0;this.resolve();
    }
    resolve(){
      const profile=this.loadout?R.InfantryLoadouts[this.loadout]:null;
      if(this.loadout&&!profile)throw new Error('Nieznany profil wyposażenia: '+this.loadout);
      const next={};
      for(const [slot,schema]of Object.entries(this.slotSchema)){
        const choice=profile&&profile.slots[slot];
        const rng=new R.SeededRandom(hash(this.seed,'slot/'+slot));
        let id=Array.isArray(choice)?choice[Math.floor(rng.next()*choice.length)]:(choice||null);
        if(Object.prototype.hasOwnProperty.call(this.overrides,slot))id=this.overrides[slot];
        id=id||schema.required||null;
        if(id){const def=R.EquipmentCatalog.get(id);if(!def.slots.includes(slot))throw new Error(def.label+' nie pasuje do slotu '+slot);
          const variantSeed=hash(this.seed,slot+'/'+id),v=new R.SeededRandom(variantSeed);
          next[slot]=Object.freeze({definitionId:id,slot,seed:variantSeed,variant:Object.freeze({size:.965+v.next()*.07,shade:.92+v.next()*.13,detail:v.next()}),weightKg:def.weightKg});
        }else next[slot]=null;
      }
      for(const slot of Object.keys(this.overrides))if(!this.slotSchema[slot])throw new Error('Nieobsługiwany slot '+slot);
      this.slots=Object.freeze(next);this.revision++;
    }
    definition(slot){const item=this.slots[slot];return item?R.EquipmentCatalog.get(item.definitionId):null;}
    get totalWeightKg(){return Object.values(this.slots).reduce((sum,item)=>sum+(item?item.weightKg:0),0);}
    get count(){return Object.values(this.slots).filter(Boolean).length;}
    clone(changes={}){return new R.Equipment({seed:this.seed,slotSchema:this.slotSchema,loadout:this.loadout,overrides:this.overrides,palette:this.palette,wear:this.wear,...changes});}
    withSlot(slot,id){if(!this.slotSchema[slot])throw new Error('Nieobsługiwany slot '+slot);return this.clone({overrides:{...this.overrides,[slot]:id||null}});}
    toJSON(){return {version:1,seed:this.seed,loadout:this.loadout,palette:this.palette,wear:this.wear,overrides:{...this.overrides},slots:Object.fromEntries(Object.entries(this.slots).map(([s,i])=>[s,i?i.definitionId:null]))};}
  };
})();
