'use strict';
const fs=require('node:fs'),assert=require('node:assert/strict'),path=require('node:path');
const source=fs.readFileSync(path.join(__dirname,'../js/buildings/buildingWorldRepresentation.js'),'utf8');assert(source.includes("drawCalls:this.detail==='WORLD'?Math.min(8"));assert(source.includes('furnitureMeshes:0'));console.log('PASS WorldRenderBudget contract');
