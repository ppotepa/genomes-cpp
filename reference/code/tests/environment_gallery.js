(function(){
  const R=window.RTS,width=1440,height=990,cols=5,rows=4,cellW=width/cols,cellH=(height-50)/rows;
  const renderer=new THREE.WebGLRenderer({antialias:true,preserveDrawingBuffer:true});renderer.setSize(width,height);renderer.outputEncoding=THREE.sRGBEncoding;renderer.toneMapping=THREE.ACESFilmicToneMapping;renderer.setScissorTest(true);
  const host=document.createElement('div');Object.assign(host.style,{position:'fixed',inset:'0',zIndex:'1000',background:'#c8d0c1'});host.appendChild(renderer.domElement);document.body.appendChild(host);
  const heading=document.createElement('div');heading.textContent='GENOMES / 20 PROCEDURALNYCH ROŚLIN · LATO · SEED 1337 · SYLWETKI DOPASOWANE DO KADRÓW';Object.assign(heading.style,{position:'absolute',top:'0',left:'0',padding:'15px 20px',color:'#263b2b',font:'13px system-ui',background:'#d8e0d0',width:'100%'});host.appendChild(heading);
  const results=[];
  Object.values(R.EnvironmentCatalog.plants).forEach((s,i)=>{
    const scene=new THREE.Scene();scene.background=new THREE.Color(i%2?0xc6cebd:0xd2d8c9);scene.add(new THREE.HemisphereLight(0xe5eee2,0x756c4a,1.1));const sun=new THREE.DirectionalLight(0xffedd3,1.8);sun.position.set(-25,45,30);scene.add(sun);
    const model=R.PlantGenerator.create(R.PlantGenome.create(s.id,1337));scene.add(model.root);const box=new THREE.Box3().setFromObject(model.root),size=box.getSize(new THREE.Vector3()),center=box.getCenter(new THREE.Vector3()),extent=Math.max(size.x,size.z,size.y)*1.18;
    const camera=new THREE.OrthographicCamera(-extent*.5*cellW/cellH,extent*.5*cellW/cellH,extent*.5,-extent*.5,.01,200);camera.position.copy(center).add(new THREE.Vector3(30,8,45));camera.lookAt(center);
    const x=i%cols*cellW,y=Math.floor(i/cols)*cellH+50;renderer.setViewport(x,height-y-cellH,cellW,cellH);renderer.setScissor(x,height-y-cellH,cellW,cellH);renderer.render(scene,camera);
    const label=document.createElement('div');label.textContent=(i+1)+'. '+s.name+' · '+size.y.toFixed(1)+' m';Object.assign(label.style,{position:'absolute',left:(x+10)+'px',top:(y+8)+'px',font:'bold 12px system-ui',color:'#263b2b'});host.appendChild(label);results.push({species:s.id,height:size.y,...model.stats});model.dispose();
  });
  return {results};
})();
