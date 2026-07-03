#pragma once
#include <Arduino.h>

// Single-page touch joystick, served from flash at "/".
// Left pad: drag = vx/vy. Right slider: rotate. Posts to /cmd at 10Hz.
inline const char kIndexHtml[] PROGMEM = R"HTML(<!DOCTYPE html>
<html><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<title>cart-bot</title>
<style>
  body{margin:0;font-family:system-ui;background:#111;color:#eee;
       user-select:none;-webkit-user-select:none;touch-action:none}
  #bar{display:flex;justify-content:space-between;padding:10px 14px;align-items:center}
  #stat{font-size:14px;color:#8c8}
  #stop{background:#c33;color:#fff;border:0;border-radius:8px;padding:10px 22px;
        font-size:16px;font-weight:700}
  #deck{display:flex;gap:16px;padding:0 14px;height:70vh;align-items:stretch}
  #pad{flex:2;background:#1c1c1c;border-radius:16px;position:relative}
  #knob{width:64px;height:64px;border-radius:50%;background:#4a8;position:absolute;
        transform:translate(-50%,-50%);pointer-events:none}
  #rotwrap{flex:1;background:#1c1c1c;border-radius:16px;position:relative}
  #rot{width:56px;height:56px;border-radius:50%;background:#48a;position:absolute;
       left:50%;transform:translate(-50%,-50%);pointer-events:none}
  #vals{padding:10px 14px;font-size:14px;color:#999}
</style></head><body>
<div id="bar"><b>cart-bot</b><span id="stat">–</span><button id="stop">STOP</button></div>
<div id="deck">
  <div id="pad"><div id="knob"></div></div>
  <div id="rotwrap"><div id="rot"></div></div>
</div>
<div id="vals">vx 0 · vy 0 · ω 0</div>
<script>
let vx=0,vy=0,w=0;
const pad=document.getElementById('pad'),knob=document.getElementById('knob');
const rw=document.getElementById('rotwrap'),rot=document.getElementById('rot');
function center(){
  const p=pad.getBoundingClientRect();knob.style.left=p.width/2+'px';knob.style.top=p.height/2+'px';
  const r=rw.getBoundingClientRect();rot.style.top=r.height/2+'px';
}
center();addEventListener('resize',center);
function padMove(e){
  const t=e.touches?e.touches[0]:e,r=pad.getBoundingClientRect();
  let x=(t.clientX-r.left)/r.width*2-1,y=(t.clientY-r.top)/r.height*2-1;
  x=Math.max(-1,Math.min(1,x));y=Math.max(-1,Math.min(1,y));
  vx=Math.round(-y*100);vy=Math.round(x*100);
  knob.style.left=(x+1)/2*r.width+'px';knob.style.top=(y+1)/2*r.height+'px';
}
function padEnd(){vx=0;vy=0;center();}
function rotMove(e){
  const t=e.touches?e.touches[0]:e,r=rw.getBoundingClientRect();
  let y=(t.clientY-r.top)/r.height*2-1;y=Math.max(-1,Math.min(1,y));
  // slider up = CCW (negative), down = CW — flip here if it feels backwards
  w=Math.round(y*100);
  rot.style.top=(y+1)/2*r.height+'px';
}
function rotEnd(){w=0;center();}
for(const[el,mv,end]of[[pad,padMove,padEnd],[rw,rotMove,rotEnd]]){
  el.addEventListener('touchstart',mv);el.addEventListener('touchmove',mv);
  el.addEventListener('touchend',end);
  el.addEventListener('mousedown',e=>{el.dataset.d=1;mv(e)});
  el.addEventListener('mousemove',e=>{if(el.dataset.d)mv(e)});
  addEventListener('mouseup',()=>{if(el.dataset.d){delete el.dataset.d;end()}});
}
document.getElementById('stop').addEventListener('click',()=>{vx=0;vy=0;w=0;center();fetch('/stop')});
setInterval(async()=>{
  document.getElementById('vals').textContent=`vx ${vx} · vy ${vy} · ω ${w}`;
  try{
    const r=await fetch(`/cmd?vx=${vx}&vy=${vy}&w=${w}`);
    document.getElementById('stat').textContent=await r.text();
    document.getElementById('stat').style.color='#8c8';
  }catch(e){
    document.getElementById('stat').textContent='link lost';
    document.getElementById('stat').style.color='#c66';
  }
},100);
</script></body></html>)HTML";
