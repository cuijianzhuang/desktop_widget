#pragma once

// 后台管理页面 HTML — Apple Watch 同款 iOS 设计风格
// 直接存 Flash，ESP32 通过 MMU 缓存访问无需 PROGMEM
// 注：相邻原始字符串字面量在编译期拼接，逻辑上仍是单个 char 数组
static const char WEB_UI[] =

/* ── Part 1: HTML head + CSS ─────────────────────────────────────────────── */
R"__(<!DOCTYPE html>
<html lang="zh">
<head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<title>桌面摆件</title>
<style>
:root{--bg:#000;--s1:#1c1c1e;--s2:#2c2c2e;--sep:#38383a;--pri:#0a84ff;--red:#ff375f;--grn:#30d158;--ylw:#ffd60a;--txt:#fff;--sub:#8e8e93}
*,::before,::after{box-sizing:border-box;margin:0;padding:0}
html{-webkit-text-size-adjust:100%;color-scheme:dark}
body{background:var(--bg);color:var(--txt);font-family:-apple-system,system-ui,'SF Pro Text','PingFang SC',sans-serif;font-size:16px;line-height:1.4;min-height:100dvh;display:flex;flex-direction:column;-webkit-tap-highlight-color:transparent}

/* Nav + Segment */
.hdr{position:sticky;top:0;z-index:100}
.nav{background:rgba(28,28,30,.92);backdrop-filter:blur(20px) saturate(180%);-webkit-backdrop-filter:blur(20px) saturate(180%);border-bottom:.5px solid var(--sep);padding:14px 20px;padding-top:max(14px,env(safe-area-inset-top));display:flex;align-items:center;justify-content:space-between}
.nav h1{font-size:17px;font-weight:700;letter-spacing:-.02em}
.nav-r{display:flex;align-items:center;gap:7px}
.dot{width:9px;height:9px;border-radius:50%;background:var(--grn);flex-shrink:0;transition:background .4s}
.dot.off{background:var(--red)}
#nav-ip{font-size:12px;color:var(--sub)}

/* Segmented Control */
.seg-wrap{background:var(--bg);padding:10px 16px 12px}
.seg{display:flex;background:var(--s2);border-radius:12px;padding:3px;gap:2px}
.seg-btn{flex:1;padding:7px 0;border:none;border-radius:9px;background:transparent;color:var(--sub);font-size:11px;font-weight:600;cursor:pointer;transition:background .2s,color .2s,box-shadow .2s;line-height:1.3}
.seg-btn.on{background:var(--s1);color:var(--pri);box-shadow:0 1px 5px rgba(0,0,0,.6)}
.seg-btn:active{opacity:.7}

/* 主内容 */
main{flex:1;padding:4px 16px;padding-bottom:calc(40px + env(safe-area-inset-bottom));max-width:560px;width:100%;margin:0 auto}
.panel{display:none}.panel.on{display:block}

/* Section Header */
.sh{font-size:13px;color:var(--sub);margin:20px 4px 8px;font-weight:500;display:block}
.sh:first-child{margin-top:4px}

/* Grouped List */
.grp{background:var(--s1);border-radius:16px;overflow:hidden;margin-bottom:8px}
.row{display:flex;align-items:center;padding:0 16px;min-height:50px;gap:10px;border-bottom:.5px solid var(--sep)}
.row:last-child{border-bottom:none}
.row .k{font-size:15px;color:var(--txt);flex-shrink:0;padding:4px 0}
.row input[type=text],.row input[type=password],.row input[type=number]{flex:1;background:none;border:none;outline:none;text-align:right;color:var(--sub);font-size:15px;padding:13px 0;min-width:0}
.row select{flex:1;background:none;border:none;outline:none;color:var(--sub);font-size:15px;padding:13px 0;text-align:right;-webkit-appearance:none;cursor:pointer}
.row input::placeholder{color:var(--sep)}

/* Slider Row */
.row-col{flex-direction:column;align-items:stretch;gap:8px;padding:12px 16px}
.row-top{display:flex;justify-content:space-between;align-items:center}
.sk{font-size:15px;color:var(--txt)}.sv{font-size:15px;font-weight:700;color:var(--pri)}
input[type=range]{-webkit-appearance:none;appearance:none;width:100%;height:6px;border-radius:3px;background:var(--s2);outline:none;cursor:pointer}
input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:22px;height:22px;border-radius:50%;background:#fff;box-shadow:0 2px 8px rgba(0,0,0,.5)}
input[type=range]::-moz-range-thumb{width:22px;height:22px;border-radius:50%;background:#fff;border:none}

/* iOS Toggle */
.tog{position:relative;width:51px;height:31px;flex-shrink:0}
.tog input{display:none}
.tog-r{position:absolute;inset:0;background:var(--s2);border-radius:31px;cursor:pointer;transition:background .25s}
.tog-r::before{content:'';position:absolute;width:27px;height:27px;top:2px;left:2px;background:#fff;border-radius:50%;box-shadow:0 2px 6px rgba(0,0,0,.5);transition:transform .3s cubic-bezier(.34,1.4,.64,1)}
.tog input:checked+.tog-r{background:var(--grn)}
.tog input:checked+.tog-r::before{transform:translateX(20px)}

/* Warning Banner */
.warn{background:rgba(255,214,10,.07);border-left:3px solid var(--ylw);border-radius:0 12px 12px 0;padding:12px 14px;margin:4px 0 12px;font-size:13px;color:var(--txt);line-height:1.55}

/* Note text */
.note{font-size:13px;color:var(--sub);padding:10px 16px 12px;line-height:1.55}
.note a{color:var(--pri);text-decoration:none}

/* Buttons */
.btns{display:flex;flex-direction:column;gap:10px;margin-top:16px}
.btn{display:block;width:100%;padding:15px;border:none;border-radius:14px;font-size:17px;font-weight:600;cursor:pointer;transition:opacity .15s,transform .1s;letter-spacing:-.02em;text-align:center}
.btn:active{opacity:.65;transform:scale(.98)}
.btn-p{background:var(--pri);color:#fff}
.btn-s{background:var(--s1);color:var(--pri)}
.btn-r{background:var(--red);color:#fff}
.btn-w{background:var(--s2);color:var(--txt)}

/* OTA Progress */
.prog-wrap{background:var(--s2);border-radius:100px;height:8px;margin:16px 0 4px;display:none;overflow:hidden}
.prog-bar{height:100%;background:var(--pri);border-radius:100px;transition:width .3s;width:0}
.prog-pct{text-align:center;font-size:12px;color:var(--sub);display:none;margin-bottom:10px}

/* Status Grid */
.sg{display:grid;grid-template-columns:1fr 1fr;gap:10px}
.sc{background:var(--s1);border-radius:16px;padding:16px;overflow:hidden}
.sc-k{font-size:11px;color:var(--sub);text-transform:uppercase;letter-spacing:.06em;font-weight:600}
.sc-v{font-size:clamp(15px,4.5vw,22px);font-weight:700;margin-top:6px;color:var(--txt);line-height:1.1;word-break:break-all}
.sc-v.c-p{color:var(--pri)}.sc-v.c-g{color:var(--grn)}.sc-v.c-y{color:var(--ylw)}.sc-v.c-r{color:var(--red)}

/* Toast */
.toast-w{position:fixed;bottom:calc(28px + env(safe-area-inset-bottom));inset-inline:0;display:flex;justify-content:center;pointer-events:none;z-index:999}
#toast{background:rgba(44,44,46,.98);backdrop-filter:blur(16px);-webkit-backdrop-filter:blur(16px);border:.5px solid var(--sep);border-radius:20px;padding:13px 22px;font-size:14px;font-weight:500;max-width:calc(100vw - 40px);text-align:center;line-height:1.4;transform:translateY(80px) scale(.95);opacity:0;transition:transform .35s cubic-bezier(.34,1.4,.64,1),opacity .25s}
#toast.on{transform:none;opacity:1}
</style>
</head>
)__"

/* ── Part 2: Body HTML ────────────────────────────────────────────────────── */
R"__(<body>

<div class="hdr">
  <div class="nav">
    <h1>&#x231A; 桌面摆件</h1>
    <div class="nav-r">
      <span class="dot off" id="wd"></span>
      <span id="nav-ip">连接中...</span>
    </div>
  </div>
  <div class="seg-wrap">
    <div class="seg">
      <button class="seg-btn on"  onclick="ST(this,'net')">&#x1F4F6;<br>网络</button>
      <button class="seg-btn"     onclick="ST(this,'wx')">&#x1F324;<br>天气</button>
      <button class="seg-btn"     onclick="ST(this,'disp')">&#x1F5A5;<br>显示</button>
      <button class="seg-btn"     onclick="ST(this,'ota')">&#x1F4E6;<br>固件</button>
      <button class="seg-btn"     onclick="ST(this,'stat')">&#x1F4CA;<br>状态</button>
    </div>
  </div>
</div>

<main>

<!-- 网络 -->
<div class="panel on" id="p-net">
  <div class="warn">&#x26A0;&#xFE0F; 本设备芯片<strong>仅支持 2.4 GHz WiFi</strong>，请填写路由器 <strong>2.4G 频段</strong>的 SSID（常见为不带 "-5G" 后缀的名称）。</div>
  <span class="sh">WIFI</span>
  <div class="grp">
    <div class="row"><span class="k">网络名称</span><input id="c-ssid" type="text" placeholder="2.4G SSID"></div>
    <div class="row"><span class="k">密码</span><input id="c-pass" type="password" placeholder="&#x2022;&#x2022;&#x2022;&#x2022;&#x2022;&#x2022;&#x2022;&#x2022;"></div>
  </div>
  <div class="btns"><button class="btn btn-p" onclick="saveNet()">保存并重新连接</button></div>

  <span class="sh">时间服务器</span>
  <div class="grp">
    <div class="row"><span class="k">NTP 服务器</span><input id="c-ntp" type="text" placeholder="ntp.aliyun.com"></div>
    <div class="row"><span class="k">时区 UTC +</span><input id="c-tz" type="number" min="-12" max="14" placeholder="8"></div>
  </div>
  <div class="btns"><button class="btn btn-s" onclick="saveNTP()">保存</button></div>
</div>

<!-- 天气 -->
<div class="panel" id="p-wx">
  <span class="sh">OPENWEATHERMAP</span>
  <div class="grp">
    <div class="row"><span class="k">API Key</span><input id="c-key" type="text" placeholder="32位密钥"></div>
    <div class="row"><span class="k">城市 ID</span><input id="c-city" type="text" placeholder="1796236（上海）"></div>
    <div class="row"><span class="k">刷新间隔</span><input id="c-intv" type="number" min="5" max="360" placeholder="分钟"></div>
    <div class="row"><span class="k">温度单位</span>
      <select id="c-unit"><option value="metric">摄氏度 &deg;C</option><option value="imperial">华氏度 &deg;F</option></select>
    </div>
  </div>
  <div class="note">免费 Key 请前往 <a href="https://openweathermap.org/api" target="_blank">openweathermap.org</a> 注册。城市 ID 可在 <a href="https://openweathermap.org/find" target="_blank">城市搜索</a> 中查询。</div>
  <div class="btns"><button class="btn btn-p" onclick="saveWX()">保存</button></div>
</div>

<!-- 显示 -->
<div class="panel" id="p-disp">
  <span class="sh">屏幕</span>
  <div class="grp">
    <div class="row row-col">
      <div class="row-top"><span class="sk">亮度</span><span class="sv" id="bv">200</span></div>
      <input type="range" id="c-br" min="20" max="255" value="200" oninput="document.getElementById('bv').textContent=this.value">
    </div>
    <div class="row"><span class="k">开机默认</span>
      <select id="c-scr"><option value="0">时钟</option><option value="1">天气</option><option value="2">传感器</option></select>
    </div>
    <div class="row"><span class="k">自动息屏（秒）</span><input type="number" id="c-sleep" min="0" max="3600" step="30" placeholder="0 = 关闭"></div>
  </div>
  <span class="sh">交互</span>
  <div class="grp">
    <div class="row"><span class="k">摇一摇唤醒</span><label class="tog"><input type="checkbox" id="c-shake" checked><span class="tog-r"></span></label></div>
    <div class="row"><span class="k">整点屏幕提示</span><label class="tog"><input type="checkbox" id="c-hourly"><span class="tog-r"></span></label></div>
  </div>
  <div class="btns"><button class="btn btn-p" onclick="saveDisp()">保存</button></div>
</div>

<!-- OTA 固件 -->
<div class="panel" id="p-ota">
  <span class="sh">固件更新 OTA</span>
  <div class="grp">
    <div class="note">Arduino IDE &rarr; 项目 &rarr; 导出已编译的二进制文件，选择 <b>.bin</b> 上传。上传期间请勿断电，完成后自动重启。</div>
    <div class="row"><span class="k">固件文件</span>
      <input type="file" id="ota-f" accept=".bin" style="flex:1;background:none;border:none;color:var(--sub);font-size:13px;text-align:right;outline:none">
    </div>
  </div>
  <div class="prog-wrap" id="pbar-w"><div class="prog-bar" id="pbar"></div></div>
  <p class="prog-pct" id="pct"></p>
  <div class="btns"><button class="btn btn-p" onclick="doOTA()">开始上传</button></div>

  <span class="sh">设备维护</span>
  <div class="btns">
    <button class="btn btn-w" onclick="doRestart()">重启设备</button>
    <button class="btn btn-r" onclick="doReset()">恢复出厂设置</button>
  </div>
</div>

<!-- 状态 -->
<div class="panel" id="p-stat">
  <span class="sh">实时状态 <span id="stat-age" style="font-weight:400"></span></span>
  <div class="sg">
    <div class="sc"><div class="sc-k">IP 地址</div><div class="sc-v c-p" id="s-ip">--</div></div>
    <div class="sc"><div class="sc-k">WiFi 信号</div><div class="sc-v" id="s-rssi">--</div></div>
    <div class="sc"><div class="sc-k">运行时间</div><div class="sc-v" id="s-up">--</div></div>
    <div class="sc"><div class="sc-k">可用内存</div><div class="sc-v" id="s-heap">--</div></div>
    <div class="sc"><div class="sc-k">电池电量</div><div class="sc-v" id="s-batt">--</div></div>
    <div class="sc"><div class="sc-k">固件版本</div><div class="sc-v" id="s-ver">--</div></div>
  </div>
  <div class="btns" style="margin-top:16px"><button class="btn btn-s" onclick="fetchStat()">刷新状态</button></div>
</div>

</main>
)__"

/* ── Part 3: Toast + Script ──────────────────────────────────────────────── */
R"__(<div class="toast-w"><div id="toast"></div></div>

<script>
function ST(el,id){
  document.querySelectorAll('.seg-btn').forEach(b=>b.classList.remove('on'));
  document.querySelectorAll('.panel').forEach(p=>p.classList.remove('on'));
  el.classList.add('on');document.getElementById('p-'+id).classList.add('on');
  if(id==='stat')fetchStat();
}

var _tt;
function toast(msg,ok){
  if(ok===undefined)ok=true;
  var t=document.getElementById('toast');
  t.innerHTML=(ok?'<span style="color:var(--grn)">✓</span> ':'<span style="color:var(--red)">✕</span> ')+msg;
  t.classList.add('on');clearTimeout(_tt);
  _tt=setTimeout(function(){t.classList.remove('on')},4000);
}

var $=function(id){return document.getElementById(id)};
var sv=function(id,v){var e=$(id);if(e)e.value=v};
var sc=function(id,v){var e=$(id);if(e)e.checked=v};
var gv=function(id){var e=$(id);return e?e.value:''};
var gn=function(id){var e=$(id);return e?+e.value:0};
var gc=function(id){var e=$(id);return(e&&e.checked)?1:0};

function loadCfg(){
  fetch('/api/config').then(function(r){return r.json()}).then(function(c){
    sv('c-ssid',c.wifi_ssid||'');sv('c-pass',c.wifi_pass||'');
    sv('c-ntp',c.ntp1||'ntp.aliyun.com');sv('c-tz',c.tz_offset!=null?c.tz_offset:8);
    sv('c-key',c.owm_key||'');sv('c-city',c.city_id||'1796236');
    sv('c-intv',c.wx_interval||30);sv('c-unit',c.wx_units||'metric');
    var br=c.brightness||200;sv('c-br',br);$('bv').textContent=br;
    sv('c-scr',c.default_screen||0);
    sv('c-sleep',(c.auto_sleep_sec!=null)?c.auto_sleep_sec:0);
    sc('c-shake',c.wake_shake!==0);sc('c-hourly',!!c.hourly_notify);
  }).catch(function(e){console.warn('loadCfg:',e)});
}

function postCfg(body){
  return fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)}).then(function(r){return r.ok});
}

function saveNet(){
  var ssid=gv('c-ssid').trim();
  if(/5[Gg](?:Hz)?$|[-_.]5[Gg]$|5[Gg][-_.]/i.test(ssid))
    if(!confirm('该名称很像 5GHz 热点，确定仍要保存？'))return;
  postCfg({wifi_ssid:ssid,wifi_pass:gv('c-pass')}).then(function(ok){
    toast(ok?'已保存，设备正在重新连接 WiFi...':'保存失败',ok);
  }).catch(function(e){toast('请求错误: '+e.message,false)});
}
function saveNTP(){
  postCfg({ntp1:gv('c-ntp'),tz_offset:gn('c-tz')}).then(function(ok){
    toast(ok?'NTP 配置已保存':'保存失败',ok);
  }).catch(function(e){toast('请求错误: '+e.message,false)});
}
function saveWX(){
  postCfg({owm_key:gv('c-key'),city_id:gv('c-city'),wx_interval:gn('c-intv'),wx_units:gv('c-unit')}).then(function(ok){
    toast(ok?'天气配置已保存':'保存失败',ok);
  }).catch(function(e){toast('请求错误: '+e.message,false)});
}
function saveDisp(){
  postCfg({brightness:gn('c-br'),default_screen:gn('c-scr'),auto_sleep_sec:gn('c-sleep'),wake_shake:gc('c-shake'),hourly_notify:gc('c-hourly')}).then(function(ok){
    toast(ok?'显示配置已保存':'保存失败',ok);
  }).catch(function(e){toast('请求错误: '+e.message,false)});
}

function doOTA(){
  var f=$('ota-f').files[0];
  if(!f){toast('请先选择 .bin 文件',false);return}
  if(!f.name.endsWith('.bin')){toast('请选择 .bin 固件文件',false);return}
  var xhr=new XMLHttpRequest();xhr.open('POST','/update',true);
  xhr.upload.onprogress=function(e){
    if(!e.lengthComputable)return;
    var p=Math.round(e.loaded/e.total*100);
    $('pbar-w').style.display='block';$('pct').style.display='block';
    $('pbar').style.width=p+'%';
    $('pct').textContent=p+'%  ('+kb(e.loaded)+' / '+kb(e.total)+')';
  };
  xhr.onload=function(){return xhr.status===200?toast('更新成功！设备 3 秒后重启...'):toast('失败: '+xhr.responseText,false)};
  xhr.onerror=function(){toast('上传出错，请检查连接',false)};
  var fd=new FormData();fd.append('firmware',f);xhr.send(fd);
}
function kb(b){return(b/1024).toFixed(1)+' KB'}

function doRestart(){
  if(!confirm('确认重启设备？'))return;
  fetch('/api/restart',{method:'POST'}).catch(function(){});
  toast('设备正在重启，请稍候约 10 秒后刷新...');
}
function doReset(){
  if(!confirm('确认恢复出厂设置？所有配置将被清除！'))return;
  fetch('/api/reset',{method:'POST'}).catch(function(){});
  toast('已恢复出厂，设备正在重启...');
}

function fetchStat(){
  fetch('/api/status').then(function(r){return r.json()}).then(function(s){
    $('s-ip').textContent=s.ip||'--';
    $('s-rssi').innerHTML=rssiHtml(s.rssi,s.wifi_connected);
    $('s-up').textContent=fmtUp(s.uptime||0);
    $('s-heap').textContent=Math.round((s.free_heap||0)/1024)+' KB';
    var b=s.battery;
    $('s-batt').className='sc-v '+(b<0?'':(b<=20?'c-r':b<=50?'c-y':'c-g'));
    $('s-batt').textContent=b>=0?b+'%':'--';
    $('s-ver').textContent=s.version||'--';
    $('stat-age').textContent='&#xB7; 刚刚';
    var wd=$('wd'),ni=$('nav-ip');
    if(s.wifi_connected){wd.classList.remove('off');ni.textContent=s.ip}
    else{wd.classList.add('off');ni.textContent=s.ap_mode?'AP 配置模式':'未连接'}
  }).catch(function(){$('stat-age').textContent='&#xB7; 更新失败'});
}
function rssiHtml(rssi,conn){
  if(!conn)return '<span style="color:var(--sub)">离线</span>';
  var r=parseInt(rssi)||0,bars=r>-50?4:r>-60?3:r>-70?2:1;
  var col=bars>=3?'var(--grn)':bars===2?'var(--ylw)':'var(--red)';
  var filled='',empty='';
  for(var i=0;i<bars;i++)filled+='&#x25AE;';
  for(var i=0;i<4-bars;i++)empty+='&#x25AE;';
  return '<span style="color:'+col+';letter-spacing:2px">'+filled+'<span style="opacity:.2">'+empty+'</span></span><br><small style="color:var(--sub);font-size:12px">'+rssi+' dBm</small>';
}
function fmtUp(s){return Math.floor(s/3600)+'h '+Math.floor(s%3600/60)+'m '+s%60+'s'}

loadCfg();fetchStat();setInterval(fetchStat,8000);
</script>
</body>
</html>
)__";
