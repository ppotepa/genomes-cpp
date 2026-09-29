(function () {
  'use strict';
  const R = window.RTS;
  // Visual/gameplay-laboratory values, not specifications of real weapons.
  // A slot is inventory ownership; family/profile describes an action's grip.
  const entries = {
    knife: {family:'ONE_HANDED', kind:'knife', profiles:['1H'], length:.25, draw:1.00, holster:1.05},
    grenade: {family:'ONE_HANDED', kind:'grenade', profiles:['1H'], length:.105, draw:.95, holster:1.00},
    sidearm: {family:'ONE_HANDED', kind:'pistol', profiles:['1H','2H'], length:.215, draw:1.05, holster:1.12, firearm:true, interval:.23, kick:.20},
    carbine: {family:'TWO_HANDED', kind:'long', profiles:['2H'], length:.70, draw:1.38, holster:1.45, firearm:true, interval:.17, kick:.23},
    rifle: {family:'TWO_HANDED', kind:'long', profiles:['2H'], length:.84, draw:1.45, holster:1.50, firearm:true, interval:.19, kick:.25},
    marksman_rifle: {family:'TWO_HANDED', kind:'long', profiles:['2H'], length:.96, draw:1.48, holster:1.53, firearm:true, interval:.36, kick:.29},
    support_gun: {family:'TWO_HANDED', kind:'long', profiles:['2H'], length:.94, draw:1.55, holster:1.60, firearm:true, interval:.15, kick:.22},
    heavy_support_gun: {family:'TWO_HANDED', kind:'long', profiles:['2H'], length:1.00, draw:1.60, holster:1.65, firearm:true, interval:.18, kick:.28}
  };
  for (const [id,d] of Object.entries(entries)) entries[id]=Object.freeze({id,firearm:false,...d,profiles:Object.freeze(d.profiles)});
  R.WeaponCatalog=Object.freeze({
    entries:Object.freeze(entries),
    get(id){return entries[id]||null;},
    grip(def,requested='AUTO') {
      if(!def) return null;
      if(requested==='AUTO')return def.profiles[0];
      if(!def.profiles.includes(requested))throw new RangeError('Ten przedmiot nie obsługuje chwytu '+requested+'.');
      return requested;
    }
  });
})();
