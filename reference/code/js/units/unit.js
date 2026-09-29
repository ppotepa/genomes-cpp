(function () {
  'use strict';
  const R=window.RTS;
  // Any future vehicle/non-vehicle supplies its own compatible slot schema.
  // Inventory has no dependency on humanoid bones; only its visual adapter does.
  R.Unit=class Unit {
    constructor({id,side,seed,category,type,slotSchema={},equipment=null,equipmentOptions={}}){
      this.id=id;this.side=side;this.seed=seed;this.category=category;this.type=type;
      this.equipment=equipment||new R.Equipment({unitSeed:seed,slotSchema,...equipmentOptions});
    }
  };
})();
