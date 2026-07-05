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
       user-select:none;-webkit-user-select:none;touch-action:none;overflow:hidden}
  #bar{display:flex;justify-content:space-between;align-items:center;padding:8px 12px}
  #stat{font-size:12px;color:#8c8;max-width:45%;overflow:hidden;white-space:nowrap}
  #stop{background:#c33;color:#fff;border:0;border-radius:8px;padding:12px 26px;
        font-size:17px;font-weight:700}
  #deck{display:flex;gap:12px;padding:0 12px;height:56vh}
  .pad{flex:1;background:#1c1c1c;border-radius:16px;position:relative;overflow:hidden}
  .knob{width:58px;height:58px;border-radius:50%;position:absolute;left:50%;top:50%;
        transform:translate(-50%,-50%);pointer-events:none}
  #dknob{background:#4a8}  #rknob{background:#48a}
  .lbl{position:absolute;font-size:11px;color:#666;pointer-events:none;font-weight:600}
  .cx{left:50%;transform:translateX(-50%)}  .cy{top:50%;transform:translateY(-50%)}
  #row{display:flex;align-items:center;gap:10px;padding:10px 14px;font-size:13px}
  #spd{flex:1}
  #opts{display:flex;gap:16px;padding:0 14px;font-size:12px;color:#999}
  details{padding:6px 14px;font-size:11px;color:#888}
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
<div id="row">speed <input type="range" id="spd" min="25" max="100" step="5"><b id="spdv"></b></div>
<div id="opts">
  <label><input type="checkbox" id="invS"> invert strafe</label>
  <label><input type="checkbox" id="invR"> invert rotate</label>
  <span style="margin-left:auto"><a href="/tune">tuning →</a></span>
</div>
<details><summary>gamepad</summary><pre id="gpdbg">none connected — pair a controller with this phone via Bluetooth</pre></details>
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
  S.vx=0;S.vy=0;S.w=0;activeUntil=Date.now()+1000;fetch('/cmd?vx=0&vy=0&w=0');});

// ---- Gamepad (browser-paired controller, e.g. Joy-Con) ----
let gpIdx=null,gpWas=false;
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

setInterval(async()=>{
  pollGamepad();
  const k=spd.value/100;
  const vx=Math.round(S.vx*k),vy=Math.round(S.vy*k),w=Math.round(S.w*k);
  const any=vx||vy||w;
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
