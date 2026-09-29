(function () {
  'use strict';
  BuildingLab.createFloorPlan = function () {
    const canvas = document.getElementById('floorPlan'), select = document.getElementById('floorPlanFloor');
    let current = null, selected = 0;
    function draw(building = current, floor = selected) {
      current = building; selected = floor;
      if (!building) return;
      const width = Math.max(220, canvas.clientWidth), height = 220, ratio = Math.min(devicePixelRatio || 1, 2);
      canvas.width = width * ratio; canvas.height = height * ratio;
      const ctx = canvas.getContext('2d'); ctx.scale(ratio, ratio); ctx.clearRect(0,0,width,height);
      const plan = building.plan, storey = plan.storeys[floor]; if (!storey) return;
      const b = RTS.Buildings.Polygon.bounds(storey.polygon), scale = Math.min((width-28)/(b[1]-b[0]),(height-28)/(b[3]-b[2]));
      const point = p => [(p[0]-(b[0]+b[1])/2)*scale+width/2,(p[1]-(b[2]+b[3])/2)*scale+height/2];
      function polygon(poly, fill) {
        ctx.beginPath(); for (const ring of [poly.outer,...(poly.holes || [])]) { ring.forEach((p,i) => { const q = point(p); i ? ctx.lineTo(...q) : ctx.moveTo(...q); }); ctx.closePath(); }
        ctx.fillStyle = fill; ctx.fill('evenodd'); ctx.strokeStyle = '#bdd2cd'; ctx.lineWidth = 1; ctx.stroke();
      }
      polygon(storey.polygon, '#253f45');
      const rooms = plan.rooms.filter(r => r.floor === floor);
      for (const room of rooms) polygon(room.polygon, '#45625e');
      for (const wall of plan.walls.filter(w => w.floor === floor)) {
        ctx.beginPath(); ctx.moveTo(...point(wall.edge.a)); ctx.lineTo(...point(wall.edge.b)); ctx.strokeStyle = '#dae7e0'; ctx.lineWidth = 2; ctx.stroke();
        for (const opening of wall.openings || []) {
          const t = opening.t ?? opening.center/wall.length, half = opening.width/(2*wall.length);
          const along = u => point([wall.edge.a[0]+(wall.edge.b[0]-wall.edge.a[0])*u,wall.edge.a[1]+(wall.edge.b[1]-wall.edge.a[1])*u]);
          ctx.beginPath(); ctx.moveTo(...along(t-half)); ctx.lineTo(...along(t+half)); ctx.strokeStyle = opening.kind === 'window' ? '#68ceec' : '#edb769'; ctx.lineWidth = 3; ctx.stroke();
        }
      }
      for (const stair of plan.stairs.filter(s => s.floor === floor || s.fromFloor === floor)) for (const flight of stair.flights) {
        ctx.beginPath(); ctx.moveTo(...point([flight.start[0],flight.start[2]])); ctx.lineTo(...point([flight.end[0],flight.end[2]])); ctx.strokeStyle = '#76efb1'; ctx.lineWidth = 3; ctx.stroke();
      }
      document.getElementById('floorPlanSummary').textContent = 'Floor ' + (floor+1) + ' · ' + rooms.length + ' rooms · blue: windows · amber: doors';
    }
    new ResizeObserver(() => draw()).observe(canvas);
    return {draw, sync(building) { const floor = Math.min(Number(select.value) || 0, building.plan.storeys.length-1); select.replaceChildren(...building.plan.storeys.map((s,i) => new Option('Floor ' + (i+1), String(i)))); select.value = String(floor); }};
  };
})();
