(function () {
  "use strict";

  window.RTS = window.RTS || {};

  function SeededPerlin2D(seed) {
    this.perm = new Uint8Array(512);

    var random = new RTS.SeededRandom(seed);
    var p = new Uint8Array(256);
    var i;

    for (i = 0; i < 256; i++) {
      p[i] = i;
    }

    for (i = 255; i > 0; i--) {
      var j = Math.floor(random.next() * (i + 1));
      var tmp = p[i];
      p[i] = p[j];
      p[j] = tmp;
    }

    for (i = 0; i < 512; i++) {
      this.perm[i] = p[i & 255];
    }
  }

  function gradientNoise(hash,x,y){
    switch(hash&7){
      case 0:return x+y;
      case 1:return -x+y;
      case 2:return x-y;
      case 3:return -x-y;
      case 4:return x;
      case 5:return -x;
      case 6:return y;
      default:return -y;
    }
  }

  SeededPerlin2D.prototype.fade = function (t) {
    return t * t * t * (t * (t * 6 - 15) + 10);
  };

  SeededPerlin2D.prototype.lerp = function (a, b, t) {
    return a + t * (b - a);
  };

  SeededPerlin2D.prototype.grad = function (hash, x, y) {
    switch (hash & 7) {
      case 0: return x + y;
      case 1: return -x + y;
      case 2: return x - y;
      case 3: return -x - y;
      case 4: return x;
      case 5: return -x;
      case 6: return y;
      default: return -y;
    }
  };

  SeededPerlin2D.prototype.noise = function (x, y) {
    var floorX = Math.floor(x);
    var floorY = Math.floor(y);
    var xi = floorX & 255;
    var yi = floorY & 255;
    var xf = x - floorX;
    var yf = y - floorY;
    // Inline the fixed fade/lerp formulas used by this sampler. Terrain calls
    // noise many times per vertex; keeping these scalar operations local avoids
    // repeated prototype dispatch while preserving the original arithmetic.
    var u = xf * xf * xf * (xf * (xf * 6 - 15) + 10);
    var v = yf * yf * yf * (yf * (yf * 6 - 15) + 10);

    var aa = this.perm[this.perm[xi] + yi];
    var ab = this.perm[this.perm[xi] + yi + 1];
    var ba = this.perm[this.perm[xi + 1] + yi];
    var bb = this.perm[this.perm[xi + 1] + yi + 1];

    var gaa=gradientNoise(aa,xf,yf),gba=gradientNoise(ba,xf-1,yf),gab=gradientNoise(ab,xf,yf-1),gbb=gradientNoise(bb,xf-1,yf-1);
    var x1=gaa+u*(gba-gaa),x2=gab+u*(gbb-gab);

    return (x1+v*(x2-x1))*0.72;
  };

  RTS.SeededPerlin2D = SeededPerlin2D;
})();
