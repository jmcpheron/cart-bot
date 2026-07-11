#pragma once
#include <Arduino.h>

// Drive page, served by the robot at http://192.168.4.1/ (AP: cartbot-robot).
//
// Dual thumb-pads with REAL multi-touch: each pad claims one finger by its
// touch identifier and only follows that finger — strafing and rotating at
// the same time works (the old transmitter page read touches[0] everywhere,
// so the second control fought over the first finger).
//
// Gamepad API: pair a Bluetooth controller (e.g. a Joy-Con) with the PHONE,
// and this page reads it and relays — no firmware Bluetooth involved. The
// collapsible debug panel shows raw axes/buttons for adapting the mapping.
inline const char kDriveHtml[] PROGMEM = R"HTML(<!DOCTYPE html>
<html><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<title>cart-bot drive</title>
<style>
  body{margin:0;font-family:system-ui;background:#111;color:#eee;
       user-select:none;-webkit-user-select:none;overscroll-behavior-y:contain}
  #bar{display:flex;justify-content:space-between;align-items:center;padding:8px 12px}
  #stat{font-size:12px;color:#8c8;max-width:45%;overflow:hidden;white-space:nowrap}
  #stop{background:#c33;color:#fff;border:0;border-radius:8px;padding:12px 26px;
        font-size:17px;font-weight:700}
  #deck{display:flex;gap:12px;padding:0 12px;height:56vh}
  .pad{flex:1;background:#1c1c1c;border-radius:16px;position:relative;overflow:hidden;touch-action:none}
  .knob{width:58px;height:58px;border-radius:50%;position:absolute;left:50%;top:50%;
        transform:translate(-50%,-50%);pointer-events:none}
  #dknob{background:#4a8}  #rknob{background:#48a}
  .lbl{position:absolute;font-size:11px;color:#666;pointer-events:none;font-weight:600}
  .cx{left:50%;transform:translateX(-50%)}  .cy{top:50%;transform:translateY(-50%)}
  #gyro{display:flex;align-items:center;gap:10px;padding:8px 14px;font-size:13px;color:#bbb}
  #gneedle{display:inline-block;font-size:20px;color:#4a8;transition:transform .2s;line-height:1}
  #row{display:flex;align-items:center;gap:10px;padding:10px 14px;font-size:13px}
  #spd{flex:1}
  #opts{display:flex;gap:16px;padding:0 14px;font-size:12px;color:#999}
  details{padding:6px 14px;font-size:11px;color:#888}
  button.dt{background:#333;color:#eee;border:0;border-radius:8px;padding:10px 12px;font-size:13px}
  pre{white-space:pre-wrap;color:#7a7;margin:4px 0}
  a{color:#48a}
</style></head><body>
<div id="bar"><b>cart-bot</b><span id="stat">idle</span><button id="stop">STOP</button></div>
<div id="deck">
  <div class="pad" id="dpad">
    <span class="lbl cx" style="top:8px">▲ FWD</span>
    <span class="lbl cx" style="bottom:8px">▼ REV</span>
    <span class="lbl cy" style="left:8px">◀ STRAFE</span>
    <span class="lbl cy" style="right:8px">STRAFE ▶</span>
    <div class="knob" id="dknob"></div>
  </div>
  <div class="pad" id="rpad">
    <span class="lbl cy" style="left:10px;font-size:16px">⟲</span>
    <span class="lbl cy" style="right:10px;font-size:16px">⟳</span>
    <span class="lbl cx" style="top:8px">ROTATE</span>
    <div class="knob" id="rknob"></div>
  </div>
</div>
<div id="gyro">
  <span id="gneedle">▲</span>
  <b id="gyaw" style="min-width:52px">--</b>
  <span id="gdet" style="flex:1">gyro…</span>
  <button id="gzero" class="dt">yaw zero</button>
</div>
<details id="gsec"><summary>gyroscope</summary>
 <div style="display:flex;gap:14px;align-items:center;padding:8px 0;flex-wrap:wrap">
  <div style="position:relative;width:110px;height:110px;flex:none">
   <svg viewBox="-55 -55 110 110" style="position:absolute;inset:0">
    <circle r="52" fill="none" stroke="#333" stroke-width="2"/>
    <line y1="-52" y2="-46" stroke="#666" stroke-width="2"/>
    <line y1="46" y2="52" stroke="#444" stroke-width="2"/>
    <line x1="-52" x2="-46" stroke="#444" stroke-width="2"/>
    <line x1="46" x2="52" stroke="#444" stroke-width="2"/>
    <text y="-40" fill="#666" font-size="9" text-anchor="middle">0</text>
   </svg>
   <svg id="gcart" viewBox="-55 -55 110 110"
        style="position:absolute;inset:0;transition:transform .25s">
    <rect x="-18" y="-28" width="36" height="56" rx="5" fill="#1c1c1c"
          stroke="#4a8" stroke-width="2"/>
    <g fill="#333" stroke="#555" stroke-width="1">
     <rect x="-27" y="-26" width="9" height="16" rx="3"/>
     <rect x="18" y="-26" width="9" height="16" rx="3"/>
     <rect x="-27" y="10" width="9" height="16" rx="3"/>
     <rect x="18" y="10" width="9" height="16" rx="3"/>
    </g>
    <g stroke="#666" stroke-width="1.5">
     <line x1="-25" y1="-24" x2="-20" y2="-12"/>
     <line x1="25" y1="-24" x2="20" y2="-12"/>
     <line x1="-20" y1="12" x2="-25" y2="24"/>
     <line x1="20" y1="12" x2="25" y2="24"/>
    </g>
    <polygon points="0,-22 6,-10 -6,-10" fill="#4a8"/>
   </svg>
  </div>
  <div style="position:relative;width:64px;height:64px;flex:none;border-radius:50%;
       background:#1c1c1c;border:1px solid #333">
   <div style="position:absolute;left:50%;top:4px;bottom:4px;width:1px;background:#2a2a2a"></div>
   <div style="position:absolute;top:50%;left:4px;right:4px;height:1px;background:#2a2a2a"></div>
   <div style="position:absolute;left:50%;top:50%;width:22px;height:22px;border:1px solid #333;
        border-radius:50%;transform:translate(-50%,-50%)"></div>
   <div id="gdot" style="position:absolute;left:50%;top:50%;width:12px;height:12px;
        border-radius:50%;background:#8c8;transform:translate(-50%,-50%);transition:transform .25s"></div>
  </div>
  <div id="ggrid" style="flex:1;min-width:190px;display:grid;
       grid-template-columns:auto 1fr auto 1fr;gap:2px 10px;font-size:11px;color:#888"></div>
 </div>
</details>
<div id="row">speed <input type="range" id="spd" min="25" max="100" step="5"><b id="spdv"></b></div>
<div id="opts">
  <label><input type="checkbox" id="invS"> invert strafe</label>
  <label><input type="checkbox" id="invR"> invert rotate</label>
  <span style="margin-left:auto"><a href="/tune">tuning →</a></span>
</div>
<details id="csec"><summary>camera</summary>
  <div style="display:flex;gap:8px;align-items:center;padding:6px 0">
    <input id="chost" style="flex:1;background:#222;color:#eee;border:1px solid #444;border-radius:6px;padding:8px"
           placeholder="cartbot-cam.local or IP">
    <span id="cstat" style="color:#8c8;white-space:nowrap">closed</span>
  </div>
  <img id="cimg" alt="" style="width:100%;border-radius:12px;background:#000;min-height:120px">
</details>
<details><summary>gamepad</summary><pre id="gpdbg"></pre></details>
<details id="dsec" open><summary>dances</summary>
  <div style="display:flex;gap:8px;padding:10px 0 6px;align-items:center">
    <button id="dplay" style="flex:2;background:#274;color:#fff;border:0;border-radius:10px;padding:16px 0;font-size:17px;font-weight:700">▶ PLAY</button>
    <button id="dstop" style="flex:1;background:#c33;color:#fff;border:0;border-radius:10px;padding:16px 0;font-size:17px;font-weight:700">⏹</button>
    <button id="drec"  style="flex:2;background:#844;color:#fff;border:0;border-radius:10px;padding:16px 0;font-size:15px;font-weight:700">● record drive</button>
  </div>
  <div id="dmsg" style="font-size:13px;color:#8c8;padding:2px 0 8px">no steps yet — pick a preset, add steps, or record your driving</div>

  <div style="color:#bbb;font-size:12px;font-weight:600;padding:4px 0">PRESETS</div>
  <div id="dpresets" style="display:flex;flex-wrap:wrap;gap:6px;padding-bottom:8px"></div>

  <div style="color:#bbb;font-size:12px;font-weight:600;padding:4px 0">ADD STEPS
    <label style="float:right;font-weight:400">each step: <input id="ddur" type="number" value="1.0"
      step="0.1" min="0.1" max="15" style="width:52px;background:#222;color:#eee;border:1px solid #444"> s
      at the speed slider's %</label></div>
  <div style="display:flex;flex-wrap:wrap;gap:6px;padding-bottom:8px">
    <button class="dt" data-s="f">▲ FWD</button><button class="dt" data-s="b">▼ REV</button>
    <button class="dt" data-s="l">◀ STRAFE</button><button class="dt" data-s="r">STRAFE ▶</button>
    <button class="dt" data-s="ccw">⟲ ROTATE</button><button class="dt" data-s="cw">ROTATE ⟳</button>
    <button class="dt" data-s="p">⏸ PAUSE</button>
    <button class="dt" data-g="ccw">⟲ 90° gyro</button><button class="dt" data-g="cw">gyro 90° ⟳</button>
  </div>

  <div style="color:#bbb;font-size:12px;font-weight:600;padding:4px 0">PROGRAM
    <span style="float:right;font-weight:400">total <b id="dtot">0.0</b>s · tap a step to remove it</span></div>
  <div id="dsteps" style="font-size:14px;line-height:2.1;min-height:34px"></div>
  <div style="display:flex;gap:8px;padding:8px 0;align-items:center">
    <button id="dclr" class="dt">clear</button>
    <span style="margin-left:auto;color:#bbb;font-size:12px;font-weight:600">ROBOT FLASH</span>
    <select id="dslot" style="background:#222;color:#eee;border:1px solid #444;border-radius:6px;padding:8px"></select>
    <button id="dsave" class="dt">💾 save</button><button id="dload" class="dt">📂 load</button>
  </div>
</details>
<script>
const S={vx:0,vy:0,w:0};
let activeUntil=0;
const spd=document.getElementById('spd'),spdv=document.getElementById('spdv');
const invS=document.getElementById('invS'),invR=document.getElementById('invR');
spd.value=localStorage.speed||60; invS.checked=localStorage.invS==='1'; invR.checked=localStorage.invR==='1';
spdv.textContent=spd.value+'%';
spd.addEventListener('input',()=>{spdv.textContent=spd.value+'%';localStorage.speed=spd.value;});
invS.addEventListener('change',()=>localStorage.invS=invS.checked?'1':'0');
invR.addEventListener('change',()=>localStorage.invR=invR.checked?'1':'0');

// Pad with per-finger tracking: claims ONE touch by identifier.
function makePad(el,knob,move,end,lockY){
  let tid=null;
  const center=()=>{knob.style.left='50%';knob.style.top=lockY?'50%':'50%';};
  function apply(t){
    const r=el.getBoundingClientRect();
    let x=(t.clientX-r.left)/r.width*2-1, y=(t.clientY-r.top)/r.height*2-1;
    x=Math.max(-1,Math.min(1,x)); y=Math.max(-1,Math.min(1,y));
    if(lockY)y=0;
    knob.style.left=(x+1)/2*100+'%'; knob.style.top=(y+1)/2*100+'%';
    move(x,y); activeUntil=Date.now()+1000;
  }
  el.addEventListener('touchstart',e=>{e.preventDefault();
    if(tid===null){const t=e.changedTouches[0];tid=t.identifier;apply(t);}},{passive:false});
  addEventListener('touchmove',e=>{
    if(tid===null)return;
    for(const t of e.changedTouches)if(t.identifier===tid){e.preventDefault();apply(t);}},{passive:false});
  const release=e=>{
    if(tid===null)return;
    for(const t of e.changedTouches)if(t.identifier===tid){tid=null;center();end();activeUntil=Date.now()+1000;}};
  addEventListener('touchend',release);
  addEventListener('touchcancel',release);
  el.addEventListener('mousedown',e=>{el.dataset.d=1;apply(e);});
  addEventListener('mousemove',e=>{if(el.dataset.d)apply(e);});
  addEventListener('mouseup',()=>{if(el.dataset.d){delete el.dataset.d;center();end();activeUntil=Date.now()+1000;}});
  center();
}
makePad(document.getElementById('dpad'),document.getElementById('dknob'),
  (x,y)=>{S.vx=Math.round(-y*100);S.vy=Math.round(x*100*(invS.checked?-1:1));},
  ()=>{S.vx=0;S.vy=0;},false);
makePad(document.getElementById('rpad'),document.getElementById('rknob'),
  (x,_y)=>{S.w=Math.round(x*100*(invR.checked?-1:1));},
  ()=>{S.w=0;},true);

document.getElementById('stop').addEventListener('click',()=>{
  S.vx=0;S.vy=0;S.w=0;activeUntil=Date.now()+1000;
  danceStop();fetch('/cmd?vx=0&vy=0&w=0');});

// ---- Camera (overhead ESP32-CAM, only reachable when both are on the
// home LAN — not from the robot's own AP). Polls single /jpg frames instead
// of holding the MJPEG stream: kinder to the camera's limited TX budget,
// and one slow frame can't wedge a persistent connection. ----
const csec=document.getElementById('csec'),cimg=document.getElementById('cimg'),
      chost=document.getElementById('chost'),cstat=document.getElementById('cstat');
chost.value=localStorage.camhost||'cartbot-cam.local';
chost.addEventListener('change',()=>localStorage.camhost=chost.value);
let cbusy=false;
setInterval(async()=>{
  if(!csec.open||cbusy)return;
  cbusy=true;const t0=Date.now();
  try{
    const r=await fetch(`http://${chost.value}/jpg`,{cache:'no-store'});
    const b=await r.blob();
    const u=URL.createObjectURL(b);
    cimg.onload=()=>URL.revokeObjectURL(u);
    cimg.src=u;
    cstat.textContent=`${(b.size/1024).toFixed(0)}KB ${((Date.now()-t0)/1000).toFixed(1)}s`;
    cstat.style.color='#8c8';
  }catch(e){cstat.textContent='unreachable';cstat.style.color='#c66';}
  cbusy=false;
},700);

// ---- Gamepad (browser-paired controller, e.g. Joy-Con) ----
let gpIdx=null,gpWas=false;
const gpdbg=document.getElementById('gpdbg');
gpdbg.textContent=
  (navigator.getGamepads?'gamepad API: available':'gamepad API: NOT SUPPORTED by this browser')+
  (window.isSecureContext?'':'\nnote: page is plain http — some browsers (esp. Chrome) block gamepads here')+
  '\nnone connected — pair via Bluetooth, then PRESS ANY BUTTON on the controller to wake it up';
addEventListener('gamepadconnected',e=>{gpIdx=e.gamepad.index;});
addEventListener('gamepaddisconnected',e=>{if(gpIdx===e.gamepad.index)gpIdx=null;});
function pollGamepad(){
  if(gpIdx===null)return;
  const gp=navigator.getGamepads()[gpIdx]; if(!gp)return;
  const dz=v=>Math.abs(v)<0.15?0:v;
  let gx=dz(gp.axes[0]||0),gy=dz(gp.axes[1]||0),gr=dz(gp.axes[2]||0)||dz(gp.axes[3]||0);
  // dpad fallback (standard-mapping buttons 12-15) for controllers whose
  // stick reports as a hat switch (common for Joy-Con generic HID mode)
  const b=gp.buttons;
  if(!gx&&!gy&&b.length>=16){
    gy=(b[12]&&b[12].pressed?-1:0)+(b[13]&&b[13].pressed?1:0);
    gx=(b[14]&&b[14].pressed?-1:0)+(b[15]&&b[15].pressed?1:0);
  }
  if(!gr&&b.length>=8){gr=(b[4]&&b[4].pressed?-1:0)+(b[5]&&b[5].pressed?1:0);} // shoulders rotate
  if(gx||gy||gr){
    S.vx=Math.round(-gy*100);S.vy=Math.round(gx*100*(invS.checked?-1:1));
    S.w=Math.round(gr*100*(invR.checked?-1:1));
    activeUntil=Date.now()+1000;gpWas=true;
  } else if(gpWas){S.vx=0;S.vy=0;S.w=0;gpWas=false;activeUntil=Date.now()+1000;}
  const pressed=[...b.keys()].filter(i=>b[i].pressed).join(',');
  document.getElementById('gpdbg').textContent=
    gp.id+'\naxes: '+gp.axes.map(a=>a.toFixed(2)).join('  ')+'\npressed buttons: '+(pressed||'none');
}

// ---- Gyro strip + gyroscope panel: poll /imu (250ms while the panel is
// open, 500ms otherwise). Cart/needle rotate by CONTINUOUS yaw so the CSS
// transition never animates the long way around at the ±180° wrap. ----
const gneedle=document.getElementById('gneedle'),gyaw=document.getElementById('gyaw'),
      gdet=document.getElementById('gdet'),gsec=document.getElementById('gsec'),
      gcart=document.getElementById('gcart'),gdot=document.getElementById('gdot'),
      ggrid=document.getElementById('ggrid');
const gv={};
['yaw','turns','rate','pitch','roll','gx','gy','|a|','az','temp','bias','errs','state']
.forEach(l=>{
  const s=document.createElement('span');s.textContent=l;
  const b=document.createElement('b');
  b.style.cssText='color:#8c8;font-family:ui-monospace,monospace;font-weight:600';
  b.textContent='--';ggrid.append(s,b);gv[l]=b;
});
async function pollImu(){
  try{
    const d=await(await fetch('/imu?j=1')).json();
    if(d.ok){
      gneedle.style.transform=`rotate(${d.cont}deg)`;  // CW-positive, like the robot
      gneedle.style.color='#4a8';
      gyaw.textContent=d.yaw.toFixed(1)+'°';
      gdet.textContent=`${d.gz.toFixed(1)}°/s · ${d.hold?'HOLDING':d.still?'re-zeroing':'free'}`
        +(d.trim?` · trim ${d.trim}`:'')+(d.errs?` · errs ${d.errs}`:'');
      if(gsec.open){
        gcart.style.opacity=1;
        gcart.style.transform=`rotate(${d.cont}deg)`;
        // Bubble level: ±20° full scale. Roll moves the dot sideways,
        // nose-down moves it up (bubble convention).
        const cl=v=>Math.max(-1,Math.min(1,v/20));
        gdot.style.transform=`translate(-50%,-50%) translate(${cl(d.r)*24}px,${cl(-d.p)*24}px)`;
        gdot.style.background=(Math.abs(d.p)<3&&Math.abs(d.r)<3)?'#8c8':'#ca4';
        gv.yaw.textContent=d.yaw.toFixed(1)+'°';
        gv.turns.textContent=(d.cont/360).toFixed(2);
        gv.rate.textContent=d.gz.toFixed(1)+'°/s';
        gv.pitch.textContent=d.p.toFixed(1)+'°';
        gv.roll.textContent=d.r.toFixed(1)+'°';
        gv.gx.textContent=d.gx.toFixed(1)+'°/s';
        gv.gy.textContent=d.gy.toFixed(1)+'°/s';
        gv['|a|'].textContent=Math.hypot(d.ax,d.ay,d.az).toFixed(2)+'g';
        gv.az.textContent=d.az.toFixed(2)+'g';
        gv.temp.textContent=d.t.toFixed(1)+'°C';
        gv.bias.textContent=d.bias.toFixed(2);
        gv.errs.textContent=d.errs;gv.errs.style.color=d.errs?'#c66':'#8c8';
        gv.state.textContent=(d.hold?'HOLD':d.still?'still':'free')+(d.trim?` trim ${d.trim}`:'');
      }
    }else{
      gneedle.style.color='#555';gyaw.textContent='--';
      gdet.textContent='gyro offline — wiring? (serial: imu scan)';
      gcart.style.opacity=0.3;gdot.style.background='#555';
    }
  }catch(e){gdet.textContent='gyro: no link';}
}
async function imuLoop(){await pollImu();setTimeout(imuLoop,gsec.open?250:500);}
imuLoop();
document.getElementById('gzero').addEventListener('click',()=>fetch('/imu?zero=1&j=1').then(pollImu));

// ---- Dance: turtle editor + record/replay ----
const ICONS={f:'▲',b:'▼',l:'◀',r:'▶',ccw:'⟲',cw:'⟳',p:'⏸',rec:'●'};
let prog=[];try{prog=JSON.parse(localStorage.prog||'[]');}catch(e){}
let dancing=false,rec=false,recLog=[];
const dmsg=t=>{document.getElementById('dmsg').textContent=t;};

// Four built-in dances. "drift square" is the standard repeatability test:
// run it 3x from the same start mark and measure how far it drifts.
const PRESETS={
  'drift square':
    '50,0,0,1500;0,0,0,400;0,50,0,1500;0,0,0,400;-50,0,0,1500;0,0,0,400;0,-50,0,1500;0,0,0,400',
  // Gyro-measured square: same repeatability test, but the corners are
  // "t,deg,w" steps the robot ends by yaw angle, not by timer. Run 3x from
  // the same mark and compare against "drift square".
  'gyro square':
    '50,0,0,1500;0,0,0,300;t,90,40;0,0,0,300;50,0,0,1500;0,0,0,300;t,90,40;0,0,0,300;'+
    '50,0,0,1500;0,0,0,300;t,90,40;0,0,0,300;50,0,0,1500;0,0,0,300;t,90,40;0,0,0,300',
  'systems check':
    '40,0,0,1200;0,0,0,300;-40,0,0,1200;0,0,0,300;0,40,0,1200;0,0,0,300;0,-40,0,1200;0,0,0,300;0,0,40,1200;0,0,0,300;0,0,-40,1200',
  'diamond':
    '45,45,0,1200;0,0,0,300;-45,45,0,1200;0,0,0,300;-45,-45,0,1200;0,0,0,300;45,-45,0,1200;0,0,0,300',
  'shimmy spin':
    '0,45,0,300;0,-45,0,300;0,45,0,300;0,-45,0,300;0,0,0,300;0,0,60,1400;0,0,-60,1400;30,0,0,400;-30,0,0,400;0,0,70,900',
};
function textToProg(text){
  return text.split(';').map(t=>{
    t=t.trim();
    if(t[0]==='t'||t[0]==='T'){                    // gyro turn: "t,deg,w"
      const[,deg,w]=t.split(',').map(Number);
      return{turn:true,deg,w,icon:deg>=0?'⟳':'⟲'};
    }
    const[vx,vy,w,ms]=t.split(',').map(Number);
    return{vx,vy,w,ms,icon:iconFor(vx,vy,w)};
  });
}
function renderProg(){
  const el=document.getElementById('dsteps');
  el.innerHTML=prog.length?'':'<i style="color:#666">empty — pick a preset, add steps, or ● record</i>';
  prog.forEach((s,i)=>{
    const d=document.createElement('span');
    d.style.cssText='display:inline-block;background:#222;border-radius:6px;padding:3px 8px;margin:2px';
    d.textContent=s.turn?`${i+1}.${s.icon} ${Math.abs(s.deg)}°`
                        :`${i+1}.${s.icon} ${(s.ms/1000).toFixed(1)}s`;
    d.title=s.turn?`gyro turn ${s.deg}° at w${s.w}`:`vx${s.vx} vy${s.vy} w${s.w}`;
    d.addEventListener('click',()=>{prog.splice(i,1);renderProg();});
    el.appendChild(d);
  });
  // gyro turns have no fixed duration — they don't count toward the total
  document.getElementById('dtot').textContent=(prog.reduce((a,s)=>a+(s.turn?0:s.ms),0)/1000).toFixed(1);
  localStorage.prog=JSON.stringify(prog);
}
function iconFor(vx,vy,w){
  const ax=Math.abs(vx),ay=Math.abs(vy),aw=Math.abs(w);
  if(!ax&&!ay&&!aw)return ICONS.p;
  if(ax&&ay&&Math.abs(ax-ay)<20)return '◆';   // diagonal (both axes)
  if(ax>=ay&&ax>=aw)return vx>0?ICONS.f:ICONS.b;
  if(ay>=aw)return vy>0?ICONS.r:ICONS.l;
  return w>0?ICONS.cw:ICONS.ccw;
}
const presetWrap=document.getElementById('dpresets');
Object.keys(PRESETS).forEach(name=>{
  const b=document.createElement('button');
  b.className='dt';b.textContent=name;
  b.addEventListener('click',()=>{
    prog=textToProg(PRESETS[name]);renderProg();
    dmsg(`loaded preset "${name}" — press ▶ PLAY`);
  });
  presetWrap.appendChild(b);
});
document.querySelectorAll('.dt[data-s]').forEach(b=>b.addEventListener('click',()=>{
  const v=+spd.value,ms=Math.round(1000*Math.min(15,Math.max(0.1,parseFloat(document.getElementById('ddur').value)||1)));
  const map={f:[v,0,0],b:[-v,0,0],l:[0,-v,0],r:[0,v,0],ccw:[0,0,-v],cw:[0,0,v],p:[0,0,0]};
  const[vx,vy,w]=map[b.dataset.s];
  prog.push({vx,vy,w,ms,icon:ICONS[b.dataset.s]});renderProg();
}));
// Gyro-terminated 90° turns at the speed slider's % (firmware floors it at 10).
document.querySelectorAll('.dt[data-g]').forEach(b=>b.addEventListener('click',()=>{
  const deg=b.dataset.g==='cw'?90:-90;
  prog.push({turn:true,deg,w:+spd.value,icon:deg>=0?'⟳':'⟲'});renderProg();
}));
document.getElementById('dclr').addEventListener('click',()=>{prog=[];renderProg();});
const ser=()=>prog.map(s=>s.turn?`t,${s.deg},${s.w}`
                                :`${s.vx},${s.vy},${s.w},${s.ms}`).join(';');
document.getElementById('dplay').addEventListener('click',async()=>{
  if(!prog.length){dmsg('no steps');return;}
  try{
    const up=await fetch('/dance',{method:'POST',body:ser()});
    if(!up.ok){dmsg(await up.text());return;}
    const pl=await fetch('/dance/play');
    dmsg(await pl.text());dancing=pl.ok;
  }catch(e){dmsg('upload failed');}
});
async function danceStop(){dancing=false;try{await fetch('/dance/stop');}catch(e){}}
document.getElementById('dstop').addEventListener('click',()=>{danceStop();dmsg('stopped');});
document.getElementById('drec').addEventListener('click',()=>{
  rec=!rec;
  document.getElementById('drec').style.background=rec?'#c33':'#844';
  if(rec){recLog=[];dmsg('recording — drive with the pads, tap ● again to finish');}
  else{
    // compress ticks into steps: new step when velocity moves >10 on any axis
    const steps=[];let cur=null;
    for(const[vx,vy,w,t]of recLog){
      if(cur&&Math.abs(vx-cur.vx)<=10&&Math.abs(vy-cur.vy)<=10&&Math.abs(w-cur.w)<=10){cur.end=t;continue;}
      if(cur)steps.push(cur);
      cur={vx,vy,w,start:t,end:t};
    }
    if(cur)steps.push(cur);
    prog=steps.map(s=>({vx:s.vx,vy:s.vy,w:s.w,ms:Math.max(200,s.end-s.start),icon:iconFor(s.vx,s.vy,s.w)}))
              .filter(s=>s.ms>=200).slice(0,64);
    renderProg();dmsg(`recorded ${prog.length} steps — edit, then PLAY or save`);
  }
});
const slotSel=document.getElementById('dslot');
async function refreshSlots(){
  try{
    const t=await(await fetch('/dance/list')).text();
    slotSel.innerHTML='';
    t.split('|').forEach(e=>{
      const[i,name]=e.split(':');
      const o=document.createElement('option');o.value=i;
      o.textContent=`slot ${+i+1}${name?': '+name:' (empty)'}`;
      slotSel.appendChild(o);
    });
  }catch(e){}
}
refreshSlots();
document.getElementById('dsave').addEventListener('click',async()=>{
  if(!prog.length){dmsg('no steps');return;}
  const name=prompt('dance name?','dance');if(name===null)return;
  await fetch('/dance',{method:'POST',body:ser()});
  const r=await fetch(`/dance/save?slot=${slotSel.value}&name=${encodeURIComponent(name)}`);
  dmsg(await r.text());refreshSlots();
});
document.getElementById('dload').addEventListener('click',async()=>{
  const r=await fetch(`/dance/load?slot=${slotSel.value}`);
  if(!r.ok){dmsg(await r.text());return;}
  const[name,text]=(await r.text()).split('\n');
  prog=textToProg(text);
  renderProg();dmsg(`loaded "${name}" from robot flash — press ▶ PLAY`);
});
renderProg();

setInterval(async()=>{
  pollGamepad();
  const k=spd.value/100;
  const vx=Math.round(S.vx*k),vy=Math.round(S.vy*k),w=Math.round(S.w*k);
  const any=vx||vy||w;
  if(dancing){
    if(any){await danceStop();}                    // manual input preempts the dance
    else{
      try{
        const st=await(await fetch('/dance/status')).text();
        document.getElementById('stat').textContent=st;
        if(st==='idle')dancing=false;
      }catch(e){}
      return;                                       // robot feeds its own watchdog
    }
  }
  if(rec&&(any||recLog.length))recLog.push([vx,vy,w,Date.now()]);
  if(!any&&Date.now()>activeUntil){document.getElementById('stat').textContent='idle';return;}
  try{
    const r=await fetch(`/cmd?vx=${vx}&vy=${vy}&w=${w}`);
    document.getElementById('stat').textContent=await r.text();
    document.getElementById('stat').style.color='#8c8';
  }catch(e){
    document.getElementById('stat').textContent='LINK LOST';
    document.getElementById('stat').style.color='#c66';
  }
},66);
</script></body></html>)HTML";
