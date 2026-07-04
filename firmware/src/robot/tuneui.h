#pragma once
#include <Arduino.h>

// Motor tuning page, served by the robot at http://192.168.4.1 (AP: cartbot-robot).
// Four motor rows with +/- duty steppers, applied RAW (no deadband remap) at
// 10Hz while non-zero — the number on screen when the wheel starts turning is
// that motor's true deadband.
inline const char kTuneHtml[] PROGMEM = R"HTML(<!DOCTYPE html>
<html><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<title>cart-bot tuning</title>
<style>
  body{margin:0;font-family:system-ui;background:#111;color:#eee;user-select:none;-webkit-user-select:none}
  #bar{display:flex;justify-content:space-between;align-items:center;padding:10px 14px}
  #stat{font-size:13px;color:#8c8}
  #stopall{background:#c33;color:#fff;border:0;border-radius:8px;padding:12px 24px;font-size:16px;font-weight:700}
  .motor{display:flex;align-items:center;gap:8px;padding:10px 14px;border-top:1px solid #2a2a2a}
  .name{width:36px;font-weight:700;font-size:18px}
  .val{width:64px;text-align:center;font-size:22px;font-variant-numeric:tabular-nums}
  .val.on{color:#4a8}
  button.st{background:#333;color:#eee;border:0;border-radius:8px;padding:14px 0;font-size:16px;flex:1}
  button.z{background:#555}
  #note{padding:10px 14px;font-size:12px;color:#888;line-height:1.5}
</style></head><body>
<div id="bar"><b>motor tuning (raw duty)</b><span id="stat">–</span><button id="stopall">ALL STOP</button></div>
<div id="rows"></div>
<div id="note">Step a motor up until the wheel just starts turning — that duty is its
deadband. Values apply raw (no remap) while non-zero; releasing to 0 stops it.
Watchdog stops everything if this page closes.</div>
<div id="bar" style="border-top:2px solid #2a2a2a"><b>RL pin-role finder</b>
  <span id="rlstat">idle</span><button id="rlstop" style="background:#c33;color:#fff;border:0;border-radius:8px;padding:12px 24px;font-size:16px;font-weight:700">STOP</button></div>
<div id="rlcfgs" style="display:flex;flex-wrap:wrap;gap:8px;padding:10px 14px"></div>
<div style="display:flex;gap:12px;padding:0 14px 10px">
  <button id="rlf" class="st" style="flex:1;background:#274;color:#eee;border:0;border-radius:8px;padding:16px 0;font-size:17px;font-weight:700">FWD ▲</button>
  <button id="rlr" class="st" style="flex:1;background:#247;color:#eee;border:0;border-radius:8px;padding:16px 0;font-size:17px;font-weight:700">REV ▼</button>
</div>
<div id="note">Pick a config, then FWD / REV at your own pace. The config where BOTH
buttons move the wheel is the true pin-role mapping. Auto-stops 1.5s after
the page stops talking.</div>
<script>
const M=['fl','fr','rl','rr'];
const duty={fl:0,fr:0,rl:0,rr:0};
let activeUntil=0;
const rows=document.getElementById('rows');
for(const m of M){
  const r=document.createElement('div');r.className='motor';
  r.innerHTML=`<span class="name">${m.toUpperCase()}</span>
    <button class="st" data-m="${m}" data-d="-25">−25</button>
    <button class="st" data-m="${m}" data-d="-5">−5</button>
    <span class="val" id="v-${m}">0</span>
    <button class="st" data-m="${m}" data-d="5">+5</button>
    <button class="st" data-m="${m}" data-d="25">+25</button>
    <button class="st z" data-m="${m}" data-d="z">0</button>`;
  rows.appendChild(r);
}
rows.addEventListener('click',e=>{
  const b=e.target.closest('button');if(!b)return;
  const m=b.dataset.m;
  if(b.dataset.d==='z')duty[m]=0;
  else duty[m]=Math.max(-255,Math.min(255,duty[m]+parseInt(b.dataset.d)));
  const el=document.getElementById('v-'+m);
  el.textContent=duty[m];el.classList.toggle('on',duty[m]!==0);
  activeUntil=Date.now()+1500;
});
document.getElementById('stopall').addEventListener('click',()=>{
  for(const m of M){duty[m]=0;const el=document.getElementById('v-'+m);
    el.textContent='0';el.classList.remove('on');}
  activeUntil=Date.now()+1500;
});
// ---- RL pin-role finder ----
const CFGS=[[14,22,23],[14,23,22],[22,14,23],[22,23,14],[23,14,22],[23,22,14]];
let rlCfg=0,rlDir='s';
const cfgWrap=document.getElementById('rlcfgs');
CFGS.forEach((c,i)=>{
  const b=document.createElement('button');b.className='st';
  b.style.cssText='flex:1 1 30%;padding:10px 4px;font-size:12px';
  b.innerHTML=`<b>${i+1}</b><br>in1=${c[0]} in2=${c[1]}<br>en=${c[2]}`;
  b.addEventListener('click',()=>{
    rlCfg=i+1;rlDir='s';fetch('/rl?dir=s');
    [...cfgWrap.children].forEach((x,j)=>x.style.background=j===i?'#4a8':'#333');
  });
  cfgWrap.appendChild(b);
});
document.getElementById('rlf').addEventListener('click',()=>{if(rlCfg)rlDir='f';});
document.getElementById('rlr').addEventListener('click',()=>{if(rlCfg)rlDir='r';});
document.getElementById('rlstop').addEventListener('click',()=>{rlDir='s';fetch('/rl?dir=s');});
setInterval(async()=>{
  if(rlDir==='s')return;
  try{
    const r=await fetch(`/rl?cfg=${rlCfg}&dir=${rlDir}&duty=200`);
    document.getElementById('rlstat').textContent=await r.text();
  }catch(e){document.getElementById('rlstat').textContent='link lost';}
},300);

setInterval(async()=>{
  const any=M.some(m=>duty[m]!==0);
  if(!any&&Date.now()>activeUntil)return;   // go silent so ESP-NOW can drive
  try{
    const q=M.map(m=>`${m}=${duty[m]}`).join('&');
    const r=await fetch('/raw?'+q);
    document.getElementById('stat').textContent=await r.text();
    document.getElementById('stat').style.color='#8c8';
  }catch(e){
    document.getElementById('stat').textContent='link lost';
    document.getElementById('stat').style.color='#c66';
  }
},100);
</script></body></html>)HTML";
