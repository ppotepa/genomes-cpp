(function(){
  'use strict';
  // Public world-layer alias. The implementation lives with building runtime
  // modules so logical plan queries and ownership remain colocated.
  const R=globalThis.RTS;if(R.BuildingSpatialIndex)R.WorldBuildingSpatialIndex=R.BuildingSpatialIndex;
})();
