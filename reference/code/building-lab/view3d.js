(function(){
  'use strict';
  window.BuildingLab=window.BuildingLab||{};
  BuildingLab.createView=function(){
  const $=id=>document.getElementById(id);
  const scene=new THREE.Scene();scene.background=new THREE.Color(0x9fb2ba);scene.fog=new THREE.Fog(0x9fb2ba,28,70);
  const host=$('viewport');
  const renderer=new THREE.WebGLRenderer({antialias:true,powerPreference:'high-performance'});
  renderer.outputEncoding=THREE.sRGBEncoding;renderer.toneMapping=THREE.ACESFilmicToneMapping;renderer.toneMappingExposure=1.04;
  renderer.shadowMap.enabled=true;renderer.shadowMap.type=THREE.PCFSoftShadowMap;renderer.setPixelRatio(Math.min(devicePixelRatio||1,1.6));host.appendChild(renderer.domElement);
  const camera=new THREE.PerspectiveCamera(43,1,.08,120);camera.position.set(15,11,17);
  const controls=new THREE.OrbitControls(camera,renderer.domElement);controls.enableDamping=true;controls.dampingFactor=.075;controls.target.set(0,2.4,0);controls.maxPolarAngle=Math.PI*.49;controls.minDistance=4;controls.maxDistance=45;
  scene.add(new THREE.HemisphereLight(0xd8e7ed,0x615f50,1.05));
  const sun=new THREE.DirectionalLight(0xffead2,2.15);sun.position.set(-10,18,12);sun.castShadow=true;sun.shadow.mapSize.set(2048,2048);sun.shadow.camera.left=-18;sun.shadow.camera.right=18;sun.shadow.camera.top=18;sun.shadow.camera.bottom=-18;sun.shadow.camera.near=.5;sun.shadow.camera.far=60;sun.shadow.normalBias=.018;scene.add(sun);
  const ground=new THREE.Mesh(new THREE.PlaneGeometry(70,70),new THREE.MeshStandardMaterial({color:0x737b68,roughness:1}));ground.rotation.x=-Math.PI/2;ground.position.y=-.23;ground.receiveShadow=true;scene.add(ground);
  let grid=new THREE.GridHelper(70,70,0x5f6a63,0x7f8a80);grid.position.y=-.215;grid.material.transparent=true;grid.material.opacity=.42;scene.add(grid);


  function fit(building){
    const spec=building.plan.spec,footprint=building.plan.footprint.polygon,b=RTS.Buildings.Polygon.bounds(footprint),actualWidth=b[1]-b[0],actualDepth=b[3]-b[2],height=spec.storeys.count*spec.storeys.floorHeight;controls.target.set(0,Math.max(1.6,height*.42),0);
    const dist=Math.max(actualWidth,actualDepth,height*.82)*1.35;camera.far=Math.max(120,dist*2.1);camera.updateProjectionMatrix();controls.maxDistance=Math.max(45,dist*1.7);
    const groundSize=Math.max(70,Math.ceil(Math.max(actualWidth,actualDepth)*2/10)*10);ground.geometry.dispose();ground.geometry=new THREE.PlaneGeometry(groundSize,groundSize);
    scene.remove(grid);grid.geometry.dispose();for(const material of Array.isArray(grid.material)?grid.material:[grid.material])material.dispose();grid=new THREE.GridHelper(groundSize,groundSize,0x5f6a63,0x7f8a80);grid.position.y=-.215;grid.material.transparent=true;grid.material.opacity=.42;scene.add(grid);
    const shadowExtent=Math.max(18,actualWidth*.68,actualDepth*.68,height*.62);sun.shadow.camera.left=-shadowExtent;sun.shadow.camera.right=shadowExtent;sun.shadow.camera.top=shadowExtent;sun.shadow.camera.bottom=-shadowExtent;sun.shadow.camera.far=Math.max(60,height+dist);sun.shadow.camera.updateProjectionMatrix();
    camera.position.set(dist*.78,height*.72+4,dist);controls.update();
  }
  function resize(){const r=host.getBoundingClientRect();camera.aspect=Math.max(1,r.width)/Math.max(1,r.height);camera.updateProjectionMatrix();renderer.setSize(r.width,r.height,false);}
  new ResizeObserver(resize).observe(host);resize();
  function frame(now){requestAnimationFrame(frame);if(RTS.DestructionDemo.frame(renderer,now))return;controls.update();renderer.render(scene,camera);}frame();
  return {renderer,controls,resize,add:root=>scene.add(root),remove:root=>scene.remove(root),fit};
  };
})();
