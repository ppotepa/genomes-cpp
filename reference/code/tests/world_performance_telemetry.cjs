'use strict';
const fs=require('node:fs'),assert=require('node:assert/strict'),path=require('node:path');
const source=fs.readFileSync(path.join(__dirname,'../js/game.js'),'utf8');for(const key of ['simulationMs','presentationMs','rendererMs','frameMs','renderer.info.render','runtimeStats'])assert(source.includes(key),key+' telemetry');assert(source.includes('_sceneStatsScratch'));console.log('PASS WorldPerformanceTelemetry contract');
