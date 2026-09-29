'use strict';
const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict'),path=require('node:path');
const context={RTS:{Config:{WORLD_BUILDING_RUNTIME:{buildingSectorSize:64}}},globalThis:null};context.globalThis=context;vm.runInNewContext(fs.readFileSync(path.join(__dirname,'../js/buildings/buildingSpatialIndex.js'),'utf8'),context);
const index=new context.RTS.BuildingSpatialIndex(64),a={id:'a',root:{position:{x:100,z:0},rotation:{y:Math.PI/180*37}},representation:{_bounds:[-5,5,-5,5]},queryLogicalHit(){return [{t:.4,id:'hit'}]}};index.add(a);assert.equal(index.querySegment([90,0,0],[110,0,0]).length,1);assert.equal(index.stats().buildings,1);index.remove(a);assert.equal(index.stats().buildings,0);console.log('PASS BuildingSpatialIndex');
