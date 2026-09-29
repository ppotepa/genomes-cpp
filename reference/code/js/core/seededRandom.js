(function () {
  "use strict";

  window.RTS = window.RTS || {};

  function SeededRandom(seed) {
    this.state = seed >>> 0;
  }

  SeededRandom.prototype.next = function () {
    this.state += 0x6D2B79F5;

    var t = this.state;
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);

    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };

  RTS.SeededRandom = SeededRandom;
})();
