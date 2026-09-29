(function(){
'use strict';
const B=globalThis.RTS.Buildings;
B.AGENT_PROFILE=Object.freeze({radius:.28,standingHeight:1.76,shoulderWidth:.56,passageMargin:.05});
const room=(role,minArea,weight,exterior=true)=>Object.freeze({role,minArea,weight,exterior});
B.ROOM_PROGRAMS=Object.freeze({
  SMALL_HOME:Object.freeze({rooms:[room('LIVING',14,3),room('KITCHEN',7,1.5),room('BEDROOM',9,2),room('BATHROOM',3.5,1,false)]}),
  FAMILY_HOME:Object.freeze({rooms:[room('LIVING',17,3),room('KITCHEN_DINING',12,2),room('BEDROOM',10,2),room('BEDROOM',9,2),room('BATHROOM',4,1,false),room('UTILITY',3,1,false)]}),
  FARM_SERVICE_HOME:Object.freeze({rooms:[room('LIVING',15,2),room('KITCHEN_DINING',13,2),room('BEDROOM',10,2),room('BATHROOM',4,1,false),room('MUDROOM',5,1),room('PANTRY',3,1,false)]}),
  APARTMENT_UNIT:Object.freeze({rooms:[room('LIVING_KITCHEN',18,3),room('BEDROOM',10,2),room('BATHROOM',4,1,false)]}),
  CORRIDOR_APARTMENT_FLOOR:Object.freeze({twoLevel:true,rooms:[room('COMMON_CORRIDOR',10,1,false),room('APARTMENT_UNIT',24,4),room('APARTMENT_UNIT',24,4),room('SERVICE',4,1,false)]}),
  SMALL_OFFICE:Object.freeze({rooms:[room('OPEN_OFFICE',20,4),room('MEETING',9,2),room('WC',3,1,false),room('STORE',3,1,false)]}),
  OPEN_OFFICE:Object.freeze({rooms:[room('OPEN_OFFICE',35,6),room('MEETING',12,2),room('BREAK',8,1),room('WC',4,1,false),room('SERVICE',4,1,false)]}),
  WORKSHOP:Object.freeze({rooms:[room('WORKSHOP',40,8),room('OFFICE',8,1),room('STORE',8,2),room('WC',3,1,false)]}),
  BARN_STORAGE:Object.freeze({rooms:[room('STORAGE',12,8)]}),
  RETAIL_GROUND_FLOOR:Object.freeze({rooms:[room('SALES',30,6),room('STORE',8,2,false),room('STAFF',6,1),room('WC',3,1,false)]})
});
// Capacity policy is applied BEFORE searching, never as a failed-candidate fallback.
// Residential nominal areas describe comfortable rooms. Compact plans may use
// 50% of those areas (at least 2 m² per room); non-residential programs retain
// their published minima. A single-purpose rural shed requires 2 m² of storage.
B.resolveRoomProgram=function(spec,availableArea){
  const program=B.ROOM_PROGRAMS[spec.roomProgram];
  if(!program)throw new B.BuildingGenerationError('UNKNOWN_PROGRAM','program',spec,null,[{programId:spec.roomProgram}]);
  const residential=['SMALL_HOME','FAMILY_HOME','FARM_SERVICE_HOME','APARTMENT_UNIT'].includes(spec.roomProgram);
  const nominal=program.rooms.reduce((sum,r)=>sum+r.minArea,0);
  const shed=spec.presetId==='RURAL_SHED'&&spec.roomProgram==='BARN_STORAGE';
  const compact=program.rooms.map(r=>Math.max(2,r.minArea*.5)),compactArea=compact.reduce((s,a)=>s+a,0);
  const scale=residential?Math.min(1,Math.max(0,(availableArea-compactArea)/(nominal-compactArea))):1;
  const rooms=program.rooms.map((r,i)=>({...r,nominalMinArea:r.minArea,minArea:shed?2:residential?compact[i]+scale*(r.minArea-compact[i]):r.minArea}));
  return {...program,rooms,capacityRule:{id:shed?'SINGLE_PURPOSE_SHED_2M2':residential?'RESIDENTIAL_COMPACT_55_PERCENT':'NOMINAL_MINIMA',availableArea,nominalArea:nominal,requiredArea:rooms.reduce((s,r)=>s+r.minArea,0),interpolation:scale,compactArea:residential?compactArea:null}};
};
class RoomGraph{
  constructor(programId,floor,descriptors){this.schema='rts.room-graph/1';this.programId=programId;this.floor=floor;this.nodes=[{id:'outside',role:'OUTSIDE'},...descriptors.map((d,i)=>({id:`f${floor}/room-${i}`,role:d.role,minArea:d.minArea,exterior:d.exterior}))];this.edges=[];}
  connect(from,to,type='REQUIRED_TRAVERSABLE'){if(!this.edges.some(e=>e.from===from&&e.to===to&&e.type===type))this.edges.push({from,to,type});return this;}
  toJSON(){return {schema:this.schema,programId:this.programId,floor:this.floor,nodes:this.nodes,edges:this.edges};}
}
B.createRoomGraph=function(programId,floor=0){const program=B.ROOM_PROGRAMS[programId];if(!program)throw new TypeError('Unknown room program '+programId);const graph=new RoomGraph(programId,floor,program.rooms);if(graph.nodes[1]&&floor===0)graph.connect('outside',graph.nodes[1].id,'REQUIRED_REACHABLE');for(let i=2;i<graph.nodes.length;i++)graph.connect(graph.nodes[1].id,graph.nodes[i].id,'REQUIRED_REACHABLE');return graph;};
B.RoomGraph=RoomGraph;
})();
