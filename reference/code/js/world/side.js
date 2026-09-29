(function () {
  'use strict';
  const R = window.RTS;
  R.Side = class Side {
    constructor(id, uniformColor) { this.id = id; this.uniformColor = uniformColor; this.units = []; }
    addUnit(unit) { if (!this.units.includes(unit)) this.units.push(unit); }
    removeUnit(unit) { const i = this.units.indexOf(unit); if (i >= 0) this.units.splice(i, 1); }
  };
})();
