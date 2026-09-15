#include "html.h"

namespace html_content {
// ================= WEB ===================
const char Styles[] PROGMEM = R"rawliteral(<!DOCTYPE html><html lang='es'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>
<meta name='color-scheme' content='dark light'>
<title>Qymera</title>
<style>
:root{
--bg:#0d1117;--bg-2:#131922;--surface:#161d29;--surface-2:#1c2533;--surface-hover:#232f41;
--border:#2a3444;--text:#e7edf5;--text-muted:#94a3b8;
--accent:#3b82f6;--accent-hover:#60a5fa;--on-accent:#ffffff;
--success:#22c55e;--warning:#f59e0b;--danger:#ef4444;--info:#38bdf8;
--radius-sm:8px;--radius-md:12px;--radius-lg:18px;
--shadow-mild:0 1px 2px rgba(0,0,0,.22);--shadow-lg:0 16px 40px rgba(0,0,0,.40);
--font:system-ui,-apple-system,"Segoe UI",Roboto,Inter,"Helvetica Neue",Arial,sans-serif;
--mono:ui-monospace,SFMono-Regular,Menlo,Consolas,"Liberation Mono",monospace;
--card:var(--surface);--panel:var(--surface-2);
}
[data-theme="light"]{
--bg:#f3f5f9;--bg-2:#eef1f6;--surface:#ffffff;--surface-2:#f4f6fa;--surface-hover:#e9edf4;
--border:#d8dfe9;--text:#1a2433;--text-muted:#5c6a7d;
--accent:#2456c8;--accent-hover:#3b6ae0;--on-accent:#ffffff;
--success:#16a34a;--warning:#b45309;--danger:#dc2626;--info:#0ea5e9;
--shadow-lg:0 16px 40px rgba(15,23,42,.18);color-scheme:light;
}
[data-theme="forest"]{
--bg:#0d1610;--bg-2:#121f16;--surface:#16241b;--surface-2:#1c2d21;--surface-hover:#243829;
--border:#2a4431;--text:#e3efe6;--text-muted:#8fa895;
--accent:#2fbf71;--accent-hover:#4cd78c;--on-accent:#06230f;
--success:#4ade80;--warning:#fbbf24;--danger:#f87171;--info:#38bdf8;
}
[data-theme="sand"]{
--bg:#191410;--bg-2:#201a13;--surface:#262015;--surface-2:#2f291c;--surface-hover:#3a3324;
--border:#4a402b;--text:#f1ead9;--text-muted:#b4a887;
--accent:#dc9b4f;--accent-hover:#eab273;--on-accent:#241607;
--success:#7ee081;--warning:#f0bd5d;--danger:#ec6a5e;--info:#66b6e8;
}
*{box-sizing:border-box}
html{color-scheme:dark}
body{margin:0;background:var(--bg);color:var(--text);font-family:var(--font);font-size:14px;line-height:1.45;-webkit-font-smoothing:antialiased}
h1,h2,h3,h4{margin:0;font-weight:700;line-height:1.25}
a{color:var(--accent)}
button{font-family:var(--font)}
.app{display:grid;grid-template-columns:1fr;grid-template-rows:auto 1fr;min-height:100vh}
.topbar{display:flex;align-items:center;justify-content:space-between;gap:12px;padding:0 18px;height:58px;background:var(--surface);border-bottom:1px solid var(--border);position:sticky;top:0;z-index:50}
.brand{display:flex;align-items:center;gap:9px;font-size:17px;font-weight:800;letter-spacing:.2px}
.brand-sub{font-size:10px;font-weight:700;color:var(--accent);background:var(--surface-2);border:1px solid var(--border);padding:2px 7px;border-radius:999px}
.top-actions{display:flex;align-items:center;gap:14px}
.status-chip{display:inline-flex;align-items:center;gap:6px;font-size:12px;font-weight:600;color:var(--text-muted)}
.lang-switch{display:flex;border:1px solid var(--border);border-radius:999px;overflow:hidden;background:var(--surface-2)}
.langbtn{border:none;background:transparent;color:var(--text-muted);padding:4px 12px;font-size:12px;font-weight:800;letter-spacing:.4px;cursor:pointer;transition:background .15s,color .15s}
.langbtn:hover{background:var(--surface-hover);color:var(--text)}
.langbtn.active{background:var(--accent);color:var(--on-accent)}
#themePicker{display:flex;gap:7px}
.themeDot{width:20px;height:20px;border-radius:50%;cursor:pointer;border:2px solid var(--border);padding:0;transition:transform .15s,box-shadow .15s;background:var(--surface-2)}
.themeDot:hover{transform:scale(1.15)}
.themeDot:focus-visible{outline:2px solid var(--accent);outline-offset:2px}
.themeDot[data-bg="#414141"]{background:#414141}.themeDot[data-bg="#4c834e"]{background:#4c834e}.themeDot[data-bg="#c1af8d"]{background:#c1af8d}.themeDot[data-bg="#f7f2ff"]{background:#f7f2ff}
.nav{background:var(--surface);border-right:1px solid var(--border);padding:14px 10px;display:flex;flex-direction:column;gap:4px}
.navitem{display:flex;align-items:center;gap:10px;width:100%;padding:10px 12px;border:none;border-radius:var(--radius-sm);background:transparent;color:var(--text-muted);font-weight:600;font-size:13px;cursor:pointer;text-align:left;transition:background .15s,color .15s}
.navitem:hover{background:var(--surface-hover);color:var(--text)}
.navitem.active{background:var(--accent);color:var(--on-accent)}
.navitem svg{flex:0 0 auto}
.nav-sep{height:1px;background:var(--border);margin:6px 4px}
.main{padding:22px;width:100%;max-width:1280px}
.content{display:none}
.content.active{display:block}
.page-head{display:flex;align-items:flex-end;justify-content:space-between;gap:12px;margin-bottom:18px;flex-wrap:wrap}
.page-head h1{font-size:20px}
.page-sub{color:var(--text-muted);font-size:13px;margin:2px 0 0}
.btn{display:inline-flex;align-items:center;justify-content:center;gap:6px;padding:9px 14px;border-radius:var(--radius-sm);border:1px solid transparent;background:var(--surface-2);color:var(--text);font-weight:600;font-size:13px;cursor:pointer;transition:background .15s,border-color .15s}
.btn:hover{background:var(--surface-hover)}
.btn.primary{background:var(--accent);color:var(--on-accent)}
.btn.primary:hover{background:var(--accent-hover)}
.btn.ghost{background:transparent;border-color:var(--border)}
.btn.ghost:hover{background:var(--surface-hover)}
.btn.danger{background:var(--danger);color:#fff}
.btn.sm{padding:6px 10px;font-size:12px}
.chip{display:inline-flex;align-items:center;gap:5px;padding:2px 8px;border-radius:999px;font-size:10px;font-weight:700;letter-spacing:.6px;text-transform:uppercase;background:var(--surface-2);color:var(--text-muted);border:1px solid var(--border)}
.chip.ok{background:rgba(34,197,94,.14);color:var(--success);border-color:rgba(34,197,94,.35)}
.chip.danger{background:rgba(239,68,68,.14);color:var(--danger);border-color:rgba(239,68,68,.35)}
button.chip{cursor:pointer}
button.chip:hover{background:var(--surface-hover);color:var(--text)}
button.chip.ok:hover{background:rgba(34,197,94,.22);color:var(--success)}
button.chip:focus-visible{outline:2px solid var(--accent);outline-offset:2px}
.dot{width:8px;height:8px;border-radius:50%;display:inline-block;background:var(--text-muted);flex:0 0 auto}
.dot.ok{background:var(--success)}.dot.bad{background:var(--danger)}.dot.info{background:var(--info)}
.status{display:inline-flex;align-items:center;gap:5px;font-size:10px;font-weight:700;color:var(--text-muted);letter-spacing:.4px;text-transform:uppercase}
.metric{font-size:26px;font-weight:800;font-variant-numeric:tabular-nums;line-height:1.1;text-align:center}
.metric.sm{font-size:18px}
.metric .unit{font-size:15px;font-weight:700;color:var(--text-muted);margin-left:2px}
.eyebrow{font-size:10px;font-weight:700;letter-spacing:.8px;color:var(--accent);text-transform:uppercase}
.field{display:flex;flex-direction:column;gap:6px;margin-bottom:12px}
.field label{font-size:12px;font-weight:600;color:var(--text)}
.input,select,input[type=date],input[type=number],input[type=password]{width:100%;padding:9px 11px;border-radius:var(--radius-sm);border:1px solid var(--border);background:var(--bg-2);color:var(--text);font:inherit;font-size:13px}
.input:focus,select:focus,input[type=date]:focus,input[type=number]:focus,input[type=password]:focus{border-color:var(--accent);outline:none;box-shadow:0 0 0 3px rgba(59,130,246,.18)}
select option{background:var(--surface)}
.input.sm{width:auto;min-width:90px;padding:7px 9px}
.input-sm{min-width:0;padding:8px 11px;border-radius:var(--radius-sm);border:1px solid var(--border);background:var(--bg-2);color:var(--text);font:inherit;font-size:13px}
.input-sm:focus{border-color:var(--accent);outline:none;box-shadow:0 0 0 3px rgba(59,130,246,.18)}
.check{display:flex;align-items:center;gap:8px;font-size:13px;color:var(--text);cursor:pointer;padding:4px 0}
.check input[type=checkbox]{width:16px;height:16px;accent-color:var(--accent);cursor:pointer}
.field-row{display:flex;gap:8px;flex-wrap:wrap;align-items:center}
.btn-row{display:flex;gap:8px;flex-wrap:wrap;margin:4px 0 0}
.kv{display:flex;justify-content:space-between;gap:8px;font-size:13px;padding:3px 0}
.kv span{color:var(--text-muted)}




.switch{position:relative;display:inline-flex;align-items:center;width:46px;height:26px;flex:0 0 auto;cursor:pointer;border-radius:999px;background:var(--border);border:1px solid var(--border);padding:0;transition:background .2s}
.switch .knob{position:absolute;top:2px;left:2px;width:20px;height:20px;border-radius:50%;background:var(--text-muted);transition:transform .2s,background .2s;pointer-events:none}
.switch.on{background:var(--accent);border-color:var(--accent)}
.switch.on .knob{transform:translateX(20px);background:var(--on-accent)}
input[type=range]{-webkit-appearance:none;appearance:none;width:100%;height:8px;border-radius:999px;background:var(--border);outline:none;margin:12px 0;cursor:pointer;border:none}
input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;appearance:none;width:20px;height:20px;border-radius:50%;background:var(--accent);border:2px solid var(--surface);box-shadow:0 1px 4px rgba(0,0,0,.4);cursor:pointer}
input[type=range]::-moz-range-thumb{width:16px;height:16px;border-radius:50%;background:var(--accent);border:none;cursor:pointer}
.dash-stats{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:12px;margin-bottom:18px}
.stat-card{background:var(--surface);border:1px solid var(--border);border-radius:var(--radius-md);padding:14px 16px;display:flex;flex-direction:column;gap:3px;box-shadow:var(--shadow-mild)}
.stat-label{font-size:11px;font-weight:700;letter-spacing:.6px;text-transform:uppercase;color:var(--text-muted)}
.stat-value{font-size:26px;font-weight:800;font-variant-numeric:tabular-nums}
.stat-value.time{font-size:18px;align-self:center}
.section-title{font-size:12px;font-weight:700;letter-spacing:.8px;text-transform:uppercase;color:var(--text-muted);margin-bottom:10px}
.devices-desktop-layout{display:grid;gap:18px;align-items:start;grid-template-columns:1fr}
.devices-column{min-height:0;border:1px solid var(--border);border-radius:var(--radius-md);padding:14px;background:var(--surface-2);display:flex;flex-direction:column;gap:12px}
.devices-mobile-list{display:none}
.devices-actuator-grid,.devices-sensor-grid{display:grid;gap:12px;grid-template-columns:repeat(auto-fill,minmax(180px,1fr))}
@media (min-width:1100px){.devices-desktop-layout{grid-template-columns:minmax(280px,38%) minmax(440px,1fr)}}
@media (max-width:899px){.devices-mobile-list{display:grid;gap:12px}.devices-desktop-layout{display:none}}
.device-card{background:var(--surface);border:1px solid var(--border);border-radius:var(--radius-md);box-shadow:var(--shadow-mild);padding:14px;display:grid;grid-template-rows:auto 1fr auto;gap:8px}
.device-head{display:flex;align-items:center;justify-content:space-between;gap:8px}
.device-control{display:flex;align-items:center;justify-content:center;gap:10px}
.device-footer{display:flex;align-items:center;justify-content:flex-start;padding-top:4px}
.device-row{display:flex;align-items:center;justify-content:space-between;gap:10px}
.dimmer-row{display:flex;align-items:flex-end;justify-content:space-between;gap:10px;margin-top:2px}
.state-text{font-weight:800;font-size:13px;letter-spacing:.5px}
.state-text.on{color:var(--success)}.state-text.off{color:var(--text-muted)}
.value-lg{font-size:24px;font-weight:800;font-variant-numeric:tabular-nums;color:var(--accent)}
.settings-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(290px,1fr));gap:14px}
.settings-general{grid-column:1/-1}
.settings-row{display:grid;grid-template-columns:repeat(auto-fit,minmax(300px,1fr));gap:14px}
.settings-card{background:var(--surface);border:1px solid var(--border);border-radius:var(--radius-md);box-shadow:var(--shadow-mild);padding:16px;display:flex;flex-direction:column;gap:12px}
.settings-card h3{margin:2px 0 0;font-size:15px}

.settings-card-head{display:flex;justify-content:space-between;align-items:flex-start;gap:8px}

/* Settings sections with accordions */
.settings-section{margin-bottom:12px}
.accordion-header{display:flex;align-items:center;justify-content:space-between;width:100%;padding:10px 12px;background:var(--surface-2);border:1px solid var(--border);border-radius:var(--radius-md);cursor:pointer;font-weight:600;font-size:14px;color:var(--text);transition:background .15s}
.accordion-header:hover{background:var(--surface-hover)}
.accordion-header:focus{outline:2px solid var(--accent);outline-offset:2px}
.accordion-chevron{font-size:10px;transition:transform .2s;color:var(--text-muted);margin-right:8px;flex-shrink:0}
.accordion-header[aria-expanded="true"] .accordion-chevron{transform:rotate(180deg)}
.accordion-title{font-weight:600;color:var(--text)}
.accordion-count{font-size:12px;color:var(--text-muted);background:var(--bg-2);padding:2px 8px;border-radius:999px;font-weight:500}
.accordion-content{padding:8px 4px;overflow:hidden}
.settings-section .settings-grid{gap:10px;margin-top:4px}

/* Remote device groups */
.remote-device-group{background:var(--surface-2);border:1px solid var(--border);border-radius:var(--radius-md);padding:12px;margin-bottom:8px}
.remote-device-header{display:flex;align-items:center;justify-content:space-between;gap:12px;margin-bottom:8px;padding-bottom:8px;border-bottom:1px solid var(--border)}
.remote-device-name{font-weight:600;font-size:14px;color:var(--text);font-family:var(--mono)}
.remote-gui-link{flex-shrink:0;white-space:nowrap}
.remote-gui-disabled{font-size:12px;color:var(--text-muted);opacity:.6}
.remote-device-content{padding-top:4px}

.rule-list{display:flex;flex-direction:column;gap:12px}
.rule-card{background:var(--surface);border:1px solid var(--border);border-radius:var(--radius-md);box-shadow:var(--shadow-mild);padding:14px;display:flex;flex-direction:column;gap:8px}
.rule-head{display:flex;align-items:center;justify-content:space-between;gap:8px;flex-wrap:wrap}
.rule-info{display:flex;justify-content:space-between;gap:8px;font-size:13px}
.rule-info span{color:var(--text-muted)}
.rule-actions{display:flex;gap:8px}
.empty{display:flex;flex-direction:column;align-items:center;justify-content:center;gap:6px;padding:22px;margin:8px 0;color:var(--text-muted);text-align:center;font-size:13px;border:1px dashed var(--border);border-radius:var(--radius-md);background:var(--surface-2)}
.empty.sm{padding:12px;font-size:12px}
.d-none{display:none!important}
@keyframes modalIn{from{opacity:0}to{opacity:1}}
@keyframes fadeIn{from{opacity:0}to{opacity:1}}
@keyframes fadeOut{from{opacity:1}to{opacity:0}}
.modal{position:fixed;inset:0;display:none;align-items:center;justify-content:center;padding:16px;background:rgba(0,0,0,.55);z-index:1000}
.modal.open{display:flex}
.modal-content{background:var(--surface);border:1px solid var(--border);border-radius:var(--radius-lg);width:400px;max-width:94vw;max-height:88vh;overflow-y:auto;padding:18px;box-shadow:var(--shadow-lg);animation:modalIn .15s ease}

/* Logs */
.log-status{font-size:12px;color:var(--text-muted);margin-bottom:12px;min-height:16px}
.log-status.ok{color:var(--success)}
.log-status.error{color:var(--danger)}
.log-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(280px,1fr));gap:14px;align-items:start}
.log-panel{background:var(--surface);border:1px solid var(--border);border-radius:var(--radius-md);box-shadow:var(--shadow-mild);padding:12px;font-family:var(--mono);font-size:11px;max-height:60vh;overflow-y:auto;display:flex;flex-direction:column;gap:2px}
.log-panel h3{font-family:var(--font);font-size:12px;font-weight:700;color:var(--text-muted);letter-spacing:.6px;text-transform:uppercase;margin:0 0 6px;padding-bottom:6px;border-bottom:1px solid var(--border)}
.log-entry{display:flex;gap:6px;padding:2px 4px;border-radius:4px;line-height:1.4}
.log-entry .t{color:var(--text-muted);flex:0 0 auto}
.log-entry .l{font-weight:800;flex:0 0 auto;width:34px}
.log-entry .l.inf{color:var(--info)}
.log-entry .l.wrn{color:var(--warning)}
.log-entry .l.err{color:var(--danger)}
.log-entry .m{word-break:break-word}
.log-empty{color:var(--text-muted);font-family:var(--font);font-size:12px;padding:6px 4px}

/* Toast notifications */
#toast-container{position:fixed;bottom:20px;left:50%;transform:translateX(-50%);z-index:9999;display:flex;flex-direction:column;gap:4px}
.toast{padding:8px 12px;border-radius:var(--radius-sm);min-width:200px;box-shadow:var(--shadow-mild);animation:fadeIn .3s ease}
.toast.success{background:var(--success);color:var(--on-accent)}
.toast.error{background:var(--danger);color:var(--on-accent)}
.toast.warning{background:var(--warning);color:var(--on-accent)}
.toast.info{background:var(--info);color:var(--on-accent)}
.toast.fade-out{animation:fadeOut .3s ease forwards}
.modal-head{display:flex;justify-content:space-between;align-items:center;gap:8px;margin-bottom:12px}
.modal-head h2{margin:0;font-size:17px}
.icon-btn{border:none;background:var(--surface-2);color:var(--text-muted);width:30px;height:30px;border-radius:var(--radius-sm);cursor:pointer;font-size:16px;line-height:1}
.icon-btn:hover{background:var(--surface-hover);color:var(--text)}
.modal-foot{display:flex;justify-content:flex-end;padding-top:12px;margin-top:12px;border-top:1px solid var(--border)}
.wizard-step{display:flex;flex-direction:column;gap:12px}
.wizard-option{display:flex;gap:10px;align-items:flex-start;padding:11px 12px;border:2px solid var(--border);border-radius:var(--radius-md);cursor:pointer;background:var(--surface-2)}
.wizard-option:has(input:checked){border-color:var(--accent);background:var(--surface)}
.wizard-option input{margin-top:4px;accent-color:var(--accent)}
.wizard-option strong{display:block;font-size:13px}
.wizard-option small{display:block;color:var(--text-muted);font-size:12px;margin-top:2px}
.wizard-nav{display:flex;justify-content:space-between;gap:8px;border-top:1px solid var(--border);padding-top:12px;margin-top:4px}
.wizard-hint{font-size:12px;color:var(--text-muted)}
.cond-box,.action-box{border:1px solid var(--border);border-radius:var(--radius-md);padding:10px 12px;display:flex;flex-direction:column;gap:8px}
.small-note{font-size:11px;color:var(--text-muted)}
@media (min-width:900px){
.app{grid-template-columns:220px 1fr;grid-template-rows:auto 1fr}
.topbar{grid-column:1/-1}
.nav{grid-row:2;grid-column:1;min-height:calc(100vh - 58px);position:sticky;top:58px;align-self:start;height:calc(100vh - 58px)}
.main{grid-column:2;grid-row:2;padding:24px}
}
@media (max-width:899px){
.nav{position:fixed;left:0;right:0;bottom:0;top:auto;flex-direction:row;justify-content:space-around;gap:0;padding:6px 4px;border-top:1px solid var(--border);border-right:none;z-index:110;background:var(--surface);box-shadow:0 -4px 16px rgba(0,0,0,.25)}
.navitem{flex:1;flex-direction:column;gap:3px;padding:6px;font-size:10px;justify-content:center;text-align:center}
.navitem svg{width:20px;height:20px}
.nav-sep{display:none}
.main{padding:16px 14px 80px}
.topbar{height:54px;padding:0 14px}
}
@media (prefers-reduced-motion:reduce){*{animation:none!important;transition:none!important}}
</style></head><body>
)rawliteral";


const char Tabs[] PROGMEM = R"rawliteral(
<div class="app">
<header class="topbar">
  <div class="brand"><span>Qymera</span><span class="brand-sub" id="brandSub">1.0.0</span></div>
  <div class="top-actions">
<span class="status-chip" id="linkStatus"><span class="dot ok"></span><span data-i18n="chip.online">Online</span></span>
<span class="status-chip" id="systemHealth" title="System health status"></span>
<div id="themePicker" role="group" aria-label="Tema de color">
      <button type="button" class="themeDot" data-bg="#414141" aria-label="Tema oscuro"></button>
      <button type="button" class="themeDot" data-bg="#f7f2ff" aria-label="Tema claro"></button>
      <button type="button" class="themeDot" data-bg="#4c834e" aria-label="Tema forest"></button>
      <button type="button" class="themeDot" data-bg="#c1af8d" aria-label="Tema sand"></button>
    </div>
    <div class="lang-switch" role="group" aria-label="Idioma / Language">
      <button type="button" class="langbtn" data-lang="es" aria-label="Español">ES</button>
      <button type="button" class="langbtn" data-lang="en" aria-label="English">EN</button>
    </div>
  </div>
</header>
<nav class="nav" aria-label="Navegación principal">
<button type="button" class="navitem active" id="t_control" onclick="show('control')"><svg viewBox="0 0 24 24" width="18" height="18" fill="none" stroke="currentColor" stroke-width="1.8" aria-hidden="true"><rect x="3" y="3" width="7" height="7" rx="1"/><rect x="14" y="3" width="7" height="7" rx="1"/><rect x="3" y="14" width="7" height="7" rx="1"/><rect x="14" y="14" width="7" height="7" rx="1"/></svg><span data-i18n="nav.devices">Devices</span></button>
<button type="button" class="navitem" id="t_auto" onclick="show('auto')"><svg viewBox="0 0 24 24" width="18" height="18" fill="none" stroke="currentColor" stroke-width="1.8" aria-hidden="true"><path d="M13 2 3 14h7l-1 8 11-13h-7z" stroke-linejoin="round"/></svg><span data-i18n="nav.automations">Automations</span></button>
<span class="nav-sep"></span>
<button type="button" class="navitem" id="t_config" onclick="show('config')"><svg viewBox="0 0 24 24" width="18" height="18" fill="none" stroke="currentColor" stroke-width="1.8" aria-hidden="true"><circle cx="12" cy="12" r="3"/><path d="M19.4 15a1.7 1.7 0 0 0 .34 1.87l.06.06a2 2 0 1 1-2.83 2.83l-.06-.06a1.7 1.7 0 0 0-1.87-.34 1.7 1.7 0 0 0-1 1.55V21a2 2 0 1 1-4 0v-.09a1.7 1.7 0 0 0-1-1.55 1.7 1.7 0 0 0-1.87.34l-.06.06a2 2 0 1 1-2.83-2.83l.06-.06a1.7 1.7 0 0 0 .34-1.87 1.7 1.7 0 0 0-1.55-1H3a2 2 0 1 1 0-4h.09a1.7 1.7 0 0 0 1.55-1 1.7 1.7 0 0 0-.34-1.87l-.06-.06a2 2 0 1 1 2.83-2.83l.06.06a1.7 1.7 0 0 0 1.87.34h.01a1.7 1.7 0 0 0 1-1.55V3a2 2 0 1 1 4 0v.09a1.7 1.7 0 0 0 1 1.55 1.7 1.7 0 0 0 1.87-.34l.06-.06a2 2 0 1 1 2.83 2.83l-.06.06a1.7 1.7 0 0 0-.34 1.87v.01a1.7 1.7 0 0 0 1.55 1H21a2 2 0 1 1 0 4h-.09a1.7 1.7 0 0 0-1.55 1z" stroke-linejoin="round"/></svg><span data-i18n="nav.settings">Settings</span></button>
<button type="button" class="navitem" id="t_logs" onclick="show('logs')"><svg viewBox="0 0 24 24" width="18" height="18" fill="none" stroke="currentColor" stroke-width="1.8" aria-hidden="true"><path d="M8 6h13M8 12h13M8 18h13" stroke-linecap="round"/><circle cx="4" cy="6" r="1.2" fill="currentColor"/><circle cx="4" cy="12" r="1.2" fill="currentColor"/><circle cx="4" cy="18" r="1.2" fill="currentColor"/></svg><span data-i18n="nav.logs">Logs</span></button>
</nav>
<main class="main">
  <section id="control" class="view content">
    <div class="pages">
      <div class="page-head">
        <div><h1 data-i18n="page.devices">Devices</h1><p class="page-sub" data-i18n="page.devices.sub">Real-time views of actuators and sensors</p></div>
      </div>
      <div id="devices_cards" class="devices-dashboard">
        <div id="dash_stats"></div>
        <div class="search-box" style="margin:12px 0;display:flex;gap:8px;">
          <input type="text" id="devSearch" class="input-sm" placeholder="Buscar..." data-i18n-ph="dev.search" style="flex:1;" onkeydown="if(event.key==='Enter'){event.preventDefault();filterDevices();}">
          <button class="btn ghost sm" data-i18n="btn.filter" onclick="filterDevices()" style="min-width:60px;">Filtrar</button>
        </div>
        <div id="dash_lists"></div>
      </div>
    </div>
  </section>
  <section id="auto" class="view content">
    <div class="pages">
      <div class="page-head">
        <div><h1 data-i18n="page.auto">Automations</h1><p class="page-sub" data-i18n="page.auto.sub">Automation rules</p></div>
        <button class="btn primary" onclick="newRule()"><svg viewBox="0 0 24 24" width="16" height="16" fill="none" stroke="currentColor" stroke-width="2.2" aria-hidden="true"><path d="M12 5v14M5 12h14" stroke-linecap="round"/></svg><span data-i18n="btn.newRule">New rule</span></button>
      </div>
      <div id="auto_table"></div>
      <div class="empty d-none" id="auto_empty"><span data-i18n="rule.empty">There are no automation rules.</span><br><span data-i18n="rule.emptyHint">Create the first one to get started.</span></div>
    </div>
  </section>
<section id="config" class="view content">
    <div class="pages">
      <div class="page-head">
        <div><h1 data-i18n="page.config">Settings</h1><p class="page-sub" data-i18n="page.config.sub">Calibration, persistence and node configuration</p></div>
      </div>
      <div class="search-box" style="margin:8px 0;display:flex;gap:8px;">
        <input type="text" id="settingsSearch" class="input-sm" placeholder="Buscar config..." data-i18n-ph="cfg.search" style="flex:1;" onkeydown="if(event.key==='Enter'){event.preventDefault();filterSettings();}">
        <button class="btn ghost sm" data-i18n="btn.filter" onclick="filterSettings()" style="min-width:80px;">Filtrar</button>
      </div>
      <div id="cards"></div>
    </div>
  </section>
  <section id="logs" class="view content">
    <div class="pages">
      <div class="page-head">
        <div><h1 data-i18n="page.logs">Logs</h1><p class="page-sub" data-i18n="page.logs.sub">Auto-refresh every 2 s</p></div>
        <div class="btn-row">
          <button class="btn ghost" onclick="refreshLogs()"><span data-i18n="btn.refresh">Refresh</span></button>
          <button class="btn ghost" onclick="clearLogs()"><span data-i18n="btn.clear">Clear</span></button>
        </div>
      </div>
      <div class="log-status" id="logStatus"></div>
      <div class="log-grid">
        <div class="log-panel" id="log-core"><h3 data-i18n="log.core">Core</h3></div>
        <div class="log-panel" id="log-events"><h3 data-i18n="log.events">Events</h3></div>
        <div class="log-panel" id="log-sensors"><h3 data-i18n="log.sensors">Sensors / Warn / Error</h3></div>
      </div>
    </div>
  </section>
</main>
</div>
<script>
async function show(tab){
  window.activeTab = tab;
  document.querySelectorAll('.content').forEach(c=>c.classList.remove('active'));
  document.querySelectorAll('.navitem').forEach(t=>t.classList.remove('active'));
  const target = document.getElementById(tab);
  const navItem = document.getElementById('t_'+tab);
  if (target) target.classList.add('active');
  if (navItem) navItem.classList.add('active');
  localStorage.setItem('tab',tab);
  if(tab==='control') loadDevices();
  if(tab==='auto') loadRules();
  if (tab === 'config'){ await loadCalib(); await syncOtaCheckbox(); await fwRefresh(); }
  if (tab === 'logs'){ refreshLogs(); startLogAutoRefresh(); }
    else { stopLogAutoRefresh(); if (fwPoll) { clearInterval(fwPoll); fwPoll = null; } }
}

/* -------------------- I18N -------------------- */

const I18N = {
es: {
  nav:{
  devices:'Dispositivos', automations:'Automatizaciones', settings:'Ajustes', logs:'Registros'}, page:{
  devices:'Dispositivos', 'devices.sub':'Vistas de actuadores y sensores en tiempo real',
  auto:'Automatizaciones', 'auto.sub':'Reglas de automatización',
  config:'Ajustes', 'config.sub':'Calibración, persistencia y configuración del nodo',
  logs:'Registros', 'logs.sub':'Actualización automática cada 2 s'}, btn:{
  newRule:'Nueva regla', refresh:'Refrescar', clear:'Limpiar', filter:'Filtrar'}, log:{
  core:'Core', events:'Eventos', sensors:'Sensores / Aviso / Error',
  updated:'Actualizado: {time} · {n} registros', empty:'Sin registros',
  cleared:'Registros borrados', error:'No se pudieron cargar los registros',
  invalid:'Respuesta no válida del servidor', netError:'Error de conexión'}, stat:{
  actuators:'Actuadores', sensors:'Sensores', time:'Tiempo'}, chip:{
  enabled:'Habilitado', disabled:'Deshabilitado', online:'En línea'}, status:{
  local:'Local', remote:'Remoto', offline:'Desconectado'}, dev:{
  on:'ON', off:'OFF', drag:'Arrastra para ajustar nivel', search:'Buscar', noMatch:'Sin coincidencias'}, no:{
  actuators:'No hay actuadores', sensors:'No hay sensores'}, air:{
  good:'Bueno', warn:'Aviso', bad:'Malo'}, yn:{ yes:'Sí', no:'No', closed:'Cerrado', open:'Abierto'}, cal:{
  'btn.set0':'Fijar 0%', 'btn.set100':'Fijar 100%', 'btn.ref':'Fijar valor', 'btn.reset':'Resetear',
  'btn.fade':'Fijar fade', 'ph.ref':'Valor', 'ph.fade':'Fade (ms)', 'ph.pulse':'Tiempo de pulso (ms)',
  'valueReq':'Introduce un valor antes de fijar', 'check.persist':'Persistencia', 'check.pulse':'Modo pulso (ms)'}, timezone:{
  title:'Zona horaria'}, cfg:{
  title:'Configuración del nodo', broadcast:'Puerto broadcast', command:'Puerto comando',
  interval:'Intervalo de reporte', 'ph.broadcast':'Broadcast', 'ph.command':'Command',
  'ph.interval':'Intervalo (ms)', ota:'Arduino OTA', save:'Guardar', factory:'Restablecer a fábrica',
  local:'LOCAL', remote:'REMOTE', openGui:'Abrir GUI ↗', search:'Buscar configuración', noMatch:'Sin coincidencias'}, net:{
  title:'Red del nodo', ssid:'SSID', 'ssid.ph':'Nombre de la red', pass:'Contraseña',
  'pass.ph':'Contraseña WiFi', save:'Guardar y reiniciar',
  note:'Tras guardar, el dispositivo se reiniciará y se conectará a la nueva red.'}, alert:{
  saved:'Guardado', failed:'Fallo al guardar en el nodo', saveError:'No se pudo guardar (HTTP {status}{detail})',
  isVirtualNet:'Error de red: no se pudo contactar a {ip}',
  isVirtualHttp:'Error en {ip} (HTTP {status}{detail})', localError:'Error local (HTTP {status}{detail})',
  localNet:'Error de red al contactar este dispositivo'}, factory:{
  confirm:'¿Seguro? Esto borrará todos los ajustes y la información.', doing:'Reiniciando...',
  err:'Error enviando reset'}, ota:{ enableMsg:'El dispositivo se reiniciará para activar OTA. ¿Continuar?',
  disableMsg:'El dispositivo se reiniciará para desactivar OTA. ¿Continuar?'}, fw:{ title:'Actualización de firmware', current:'Versión actual', channel:'Canal', latest:'Última versión', check:'Buscar actualizaciones', update:'Actualizar Qymera', checking:'Buscando actualizaciones...', upToDate:'Ya tienes la última versión', downloading:'Descargando {p}%', installing:'Instalando... no apagues el dispositivo', rebooting:'Reiniciando...', error:'Error: {msg}'}, wiz:{
  titleNew:'Nueva regla de automatización', titleEdit:'Editar regla de automatización', cancel:'Cancelar',
  back:'Atrás', next:'Siguiente', save:'Guardar', type:'Tipo de regla',
  edge:'EDGE - Cambios de estado', 'edge.desc':'Se ejecuta cuando un sensor cambia de estado',
  thresh:'THRESHOLD - Valores límite', 'thresh.desc':'Se ejecuta cuando un valor cruza el umbral',
  time:'TIME - A una hora', 'time.desc':'Ejecuta a una hora fija dentro de un rango de fechas',
  intervalTxt:'INTERVAL - Cada X tiempo', 'interval.desc':'Ejecuta periódicamente cada intervalo',
  sensors:'Seleccionar sensores', hint:'Ctrl/Cmd + clic para seleccionar varios',
  noSensors:'Sin sensores', conditions:'Condiciones', rising:'Subida', falling:'Caída',
  dates:'Rango de fechas y hora', from:'Desde cuándo:', to:'Hasta cuándo:', runAt:'Hora de ejecución:',
  logic:'Lógica', all:'TODAS las condiciones (AND)', any:'AL MENOS UNA condición (OR)',
  actuators:'Actuadores', noActuators:'Sin actuadores', actions:'Acciones',
  intervalMs:'Intervalo (ms)', delayMs:'Delay (ms)', cooldownMs:'Cooldown (ms)',
  selectType:'Debes seleccionar un tipo de regla', needSensor:'Debes seleccionar al menos un sensor para este tipo de regla',
  needThreshold:'Debes completar el threshold para el sensor', threshRange:'El threshold debe estar entre -1000 y 10000',
  timeRange:'La hora debe estar entre 00:00 y 23:59', dateOrder:'La fecha "desde" no puede ser posterior a "hasta"',
  needActuator:'Debes seleccionar al menos un actuador', loadActions:'Error al cargar las acciones',
  loadActuator:'Error al cargar el actuador', levelDimmerOnly:'La acción LEVEL solo se puede usar en dimmers',
  levelRange:'El level debe estar entre 0 y 100', delayRange:'El delay debe estar entre 0 y 60000 ms',
  cooldownRange:'El cooldown debe estar entre 0 y 3600000 ms', intervalRange:'El intervalo debe estar entre 1000 y 3600000 ms',
  hourRange:'La hora debe estar entre 0 y 23', minRange:'Los minutos deben estar entre 0 y 59',
  levelActuator:'El level del actuador debe estar entre 0 y 100 (actual: {level})',
  saved:'Regla guardada correctamente', saveError:'Error al guardar:', connError:'Error de conexión:',
  deleteConfirm:'¿Eliminar la regla {id}?', edit:'Editar', delete:'Eliminar'}, rule:{
  sensors:'Sensor(es)', logic:'Lógica', actions:'Acciones',
  delay:'Delay / Cooldown', title:'Regla', empty:'No hay reglas de automatización.',
  emptyHint:'Crea la primera para empezar.'}
},
en: {
  nav:{
  devices:'Devices', automations:'Automations', settings:'Settings', logs:'Logs'}, page:{
  devices:'Devices', 'devices.sub':'Real-time views of actuators and sensors',
  auto:'Automations', 'auto.sub':'Automation rules',
  config:'Settings', 'config.sub':'Calibration, persistence and node configuration',
  logs:'Logs', 'logs.sub':'Auto-refresh every 2 s'}, btn:{
  newRule:'New rule', refresh:'Refresh', clear:'Clear', filter:'Filter'}, log:{
  core:'Core', events:'Events', sensors:'Sensors / Warn / Error',
  updated:'Updated: {time} · {n} entries', empty:'No entries',
  cleared:'Logs cleared', error:'Could not load the logs',
  invalid:'Invalid response from the server', netError:'Network error'}, stat:{
  actuators:'Actuators', sensors:'Sensors', time:'Time'}, chip:{
  enabled:'Enabled', disabled:'Disabled', online:'Online'}, status:{
  local:'Local', remote:'Remote', offline:'Offline'}, dev:{
  on:'ON', off:'OFF', drag:'Drag to adjust level', search:'Search', noMatch:'No matches'}, no:{
  actuators:'No actuators', sensors:'No sensors'}, air:{
  good:'Good', warn:'Warn', bad:'Bad'}, yn:{ yes:'Yes', no:'No', closed:'Closed', open:'Open'}, cal:{
  'btn.set0':'Set 0%', 'btn.set100':'Set 100%', 'btn.ref':'Set ref value', 'btn.reset':'Reset',
  'btn.fade':'Set fade', 'ph.ref':'Value', 'ph.fade':'Fade (ms)', 'ph.pulse':'Pulse time (ms)',
  'valueReq':'Enter a value before setting', 'check.persist':'Persistence', 'check.pulse':'Pulse mode (ms)'}, timezone:{
  title:'Time zone'}, cfg:{
  title:'Node configuration', broadcast:'Broadcast port', command:'Command port',
  interval:'Report interval', 'ph.broadcast':'Broadcast', 'ph.command':'Command',
  'ph.interval':'Interval (ms)', ota:'Arduino OTA', save:'Save', factory:'Factory reset',
  local:'LOCAL', remote:'REMOTE', openGui:'Open GUI ↗', search:'Search configuration', noMatch:'No matches'}, net:{
  title:'Node network', ssid:'SSID', 'ssid.ph':'Network name', pass:'Password',
  'pass.ph':'WiFi password', save:'Save & restart',
  note:'After saving, the device will restart and connect to the new network.'}, alert:{
  saved:'Saved', failed:'Failed to save on the node', saveError:'Could not save (HTTP {status}{detail})',
  isVirtualNet:'Network error: could not reach {ip}',
  isVirtualHttp:'Error in {ip} (HTTP {status}{detail})', localError:'Local error (HTTP {status}{detail})',
  localNet:'Network error contacting this device'}, factory:{
  confirm:'Are you sure? This will erase all settings and data.', doing:'Restarting...',
  err:'Error sending reset'}, ota:{ enableMsg:'The device will restart to enable OTA. Continue?',
  disableMsg:'The device will restart to disable OTA. Continue?'}, fw:{ title:'Firmware update', current:'Current version', channel:'Channel', latest:'Latest version', check:'Check for updates', update:'Update Qymera', checking:'Checking for updates...', upToDate:'You are up to date', downloading:'Downloading {p}%', installing:'Installing... do not power off', rebooting:'Rebooting...', error:'Error: {msg}'}, wiz:{
  titleNew:'New automation rule', titleEdit:'Edit automation rule', cancel:'Cancel',
  back:'Back', next:'Next', save:'Save', type:'Rule type',
  edge:'EDGE - State changes', 'edge.desc':'Runs when a sensor changes state',
  thresh:'THRESHOLD - Threshold values', 'thresh.desc':'Runs when a value crosses a threshold',
  time:'TIME - At a fixed time', 'time.desc':'Runs at a fixed time within a date range',
  intervalTxt:'INTERVAL - Every X time', 'interval.desc':'Runs periodically every interval',
  sensors:'Select sensors', hint:'Ctrl/Cmd + click to select several',
  noSensors:'No sensors', conditions:'Conditions', rising:'Rising', falling:'Falling',
  dates:'Date range and time', from:'From:', to:'Until:', runAt:'Execution time:',
  logic:'Logic', all:'ALL conditions (AND)', any:'AT LEAST ONE condition (OR)',
  actuators:'Actuators', noActuators:'No actuators', actions:'Actions',
  intervalMs:'Interval (ms)', delayMs:'Delay (ms)', cooldownMs:'Cooldown (ms)',
  selectType:'Please select a rule type', needSensor:'Select at least one sensor for this rule type',
  needThreshold:'You must fill in the threshold for the sensor', threshRange:'The threshold must be between -1000 and 10000',
  timeRange:'The time must be between 00:00 and 23:59', dateOrder:'The start date cannot be after the end date',
  needActuator:'Select at least one actuator', loadActions:'Error loading the actions',
  loadActuator:'Error loading the actuator', levelDimmerOnly:'The LEVEL action can only be used on dimmers',
  levelRange:'The level must be between 0 and 100', delayRange:'The delay must be between 0 and 60000 ms',
  cooldownRange:'Cooldown must be between 0 and 3600000 ms', intervalRange:'The interval must be between 1000 and 3600000 ms',
  hourRange:'Hour must be between 0 and 23', minRange:'Minutes must be between 0 and 59',
  levelActuator:'The actuator level must be between 0 and 100 (current: {level})',
  saved:'Rule saved successfully', saveError:'Save error:', connError:'Connection error:',
  deleteConfirm:'Delete rule {id}?', edit:'Edit', delete:'Delete'}, rule:{
  sensors:'Sensor(s)', logic:'Logic', actions:'Actions',
  delay:'Delay / Cooldown', title:'Rule', empty:'There are no automation rules.',
  emptyHint:'Create the first one to get started.'}
}
};

const SENSOR_LABEL = {
  1:['Luminosidad','Luminosity'], 2:['Humedad','Humidity'], 3:['Temperatura','Temperature'],
  4:['Presión','Pressure'], 5:['Nivel','Level'], 6:['Calidad del aire','Air quality'],
  7:['Lluvia','Rain'], 8:['Dimmer','Dimmer'], 9:['Relé','Relay'],
  10:['Hora','Time'], 11:['Personalizado','Custom'], 12:['Contacto','Contact']
};

let LANG = 'es';
try { LANG = localStorage.getItem('lang') || 'es'; } catch(e) {}

const I18N_FLAT = (() => {
  const flat = (d, prefix, out) => {
    for (const k of Object.keys(d)) {
      const key = prefix ? prefix + '.' + k : k;
      const v = d[k];
      if (v && typeof v === 'object') flat(v, key, out);
      else out[key] = v;
    }
    return out;
  };
  return { es: flat(I18N.es, '', {}), en: flat(I18N.en, '', {}) };
})();

function t(k) {
  const d = I18N_FLAT[LANG] || I18N_FLAT.es;
  let v = d[k];
  if (v == null) v = I18N_FLAT.es[k];
  return v == null ? k : v;
}

function tf(k, map) {
  let s = t(k);
  if (map) for (const m in map) s = s.split('{' + m + '}').join(map[m]);
  return s;
}

function sLabel(type) {
  const a = SENSOR_LABEL[type];
  return a ? a[LANG === 'en' ? 1 : 0] : 'GENERAL';
}
)rawliteral";


const char Rules[] PROGMEM = R"rawliteral(
function newRule(){
  loadSensorsAndActuators().then(()=>{
    document.getElementById('ruleModalTitle').textContent=t('wiz.titleNew');
    startWizard();
    document.getElementById('ruleModal').classList.add('open');
  });
}
)rawliteral";


const char CardsSettings[] PROGMEM = R"rawliteral(
function calHead(label, s, i) {
  return `<div class="settings-card-head">
    <div><span class="eyebrow">${label}</span><h3>${s.name}</h3></div>
    <button type="button" onclick='toggleMatterSwitch(${i},"${s.id}","${s.name}")' id="matterBtn${i}" data-name="${s.id}" class="chip ${s.avail?'ok':''}" aria-pressed="${s.avail?'true':'false'}">${s.avail ? t('chip.enabled') : t('chip.disabled')}</button>
  </div>`;
}

function sensorCalibCard(s, i, cfg) {
  const minMaxBtns = cfg.hasMinMax ? `
    <div class="field-row">
      <button class="btn ghost sm" onclick='setCalib(${i},"min","${s.name}")'>${t('cal.btn.set0')}</button>
      <button class="btn ghost sm" onclick='setCalib(${i},"max","${s.name}")'>${t('cal.btn.set100')}</button>
    </div>` : '';
  return `<div class="settings-card">
    ${calHead(cfg.label, s, i)}
    <div class="metric" id="v${i}">${cfg.format(s.value)}</div>
    <div class="field-row">
      <input id="ref${i}" class="input sm" placeholder="${t('cal.ph.ref')}">
      <button class="btn ghost sm" onclick='setCalib(${i},"ref","${s.name}")'>${t('cal.btn.ref')}</button>
    </div>
    ${minMaxBtns}
    <div class="btn-row">
      <button class="btn ghost sm" onclick='setCalib(${i},"res","${s.name}")'>${t('cal.btn.reset')}</button>
    </div>

  </div>`;
}

const cardRenderers = {

HUMI: (s, i) => sensorCalibCard(s, i, { label: sLabel(s.type), format: v => (v === 255 || v == null) ? 'N/A' : v + ' %', hasMinMax: true }),

LEVE: (s, i) => sensorCalibCard(s, i, { label: sLabel(s.type), format: v => (v === 255 || v == null) ? 'N/A' : v + ' %', hasMinMax: true }),

LUMI: (s, i) => sensorCalibCard(s, i, { label: sLabel(s.type), format: v => (v === 255 || v == null) ? 'N/A' : (v * 108.9432 / 7074).toFixed(0) + ' lx', hasMinMax: false }),

TEMP: (s, i) => sensorCalibCard(s, i, { label: sLabel(s.type), format: v => (v === 255 || v == null) ? 'N/A' : v.toFixed(2) + ' °C', hasMinMax: false }),

PRES: (s, i) => sensorCalibCard(s, i, { label: sLabel(s.type), format: v => (v === 255 || v == null) ? 'N/A' : v.toFixed(2) + ' kPa', hasMinMax: false }),

GENERIC: (s, i) => sensorCalibCard(s, i, { label: sLabel(s.type), format: v => (v === 255 || v == null) ? 'N/A' : Number(v).toFixed(2), hasMinMax: false }),

AIRQ: (s, i) => `<div class="settings-card">
  ${calHead(sLabel(s.type), s, i)}
  <div class="metric" id="v${i}">${s.value === 255 || s.value == null ? 'N/A' : s.value == 0 ? t('air.good') : s.value == 1 ? t('air.warn') : s.value == 2 ? t('air.bad') : 'N/A'}</div>

</div>`,

RAIN: (s, i) => `<div class="settings-card">
  ${calHead(sLabel(s.type), s, i)}
  <div class="metric" id="v${i}">${s.value === 255 || s.value == null ? 'N/A' : s.value ? t('yn.yes') : t('yn.no')}</div>

</div>`,

CONTACT: (s, i) => `<div class="settings-card">
  ${calHead(sLabel(s.type), s, i)}
  <div class="metric" id="v${i}">${s.value === 255 || s.value == null ? 'N/A' : s.state ? t('yn.closed') : t('yn.open')}</div>

</div>`,

DIMM: (s, i) => `<div class="settings-card">
  ${calHead(sLabel(s.type), s, i)}
  <div class="kv"><span>${t('cal.ph.fade')}</span><b id="v${i}">${s.fade}</b></div>
  <div class="field-row">
    <input id="ref${i}" class="input sm min-width-120" placeholder="${t('cal.ph.fade')}"">
    <button class="btn ghost sm" onclick='setCalib(${i},"fad","${s.name}")'>${t('cal.btn.fade')}</button>
  </div>

</div>`,

REL: (s, i) => `<div class="settings-card">
  ${calHead(sLabel(s.type), s, i)}
  <div class="field-row">
    <span>${t('cal.check.persist')}</span>
    <button type="button" class="switch ${s.persist?'on':''}" role="switch" aria-checked="${s.persist?'true':'false'}" aria-label="Persistencia ${s.name}" onclick="togglePersist(${i})"><span class="knob"></span></button>
    <span>${t('cal.check.pulse')}</span>
    <button type="button" class="switch ${s.pulse?'on':''}" role="switch" aria-checked="${s.pulse?'true':'false'}" aria-label="Modo pulso ${s.name}" onclick="togglePulse(${i})"><span class="knob"></span></button>
  </div>
  <div class="kv"><span>${t('cal.ph.pulse')}</span><b id="v${i}">${s.pulse_ms ?? 0}</b></div>
  <div class="field-row" id="pulseRow${i}" style="${s.pulse ? '' : 'display:none'}">
    <input id="ref${i}" class="input sm" placeholder="${t('cal.ph.pulse')}" value="${s.pulse_ms ?? ''}" onchange='setCalib(${i},"pulse","${s.name}",this.value)'>
  </div>
</div>`,

TIME: (s, i) => `<div class="settings-card">
  <div class="settings-card-head">
    <div><span class="eyebrow">${sLabel(s.type)}</span><h3>${t('timezone.title')}</h3></div>
  </div>
  <div class="metric sm" id="v${i}">--</div>
  <select onchange="setCalib(${i},'timezone','TIME',this.value)">
    <option value="-720" ${s.correction==-720?'selected':''}>UTC-12 (Baker Island)</option>
    <option value="-660" ${s.correction==-660?'selected':''}>UTC-11 (Samoa)</option>
    <option value="-600" ${s.correction==-600?'selected':''}>UTC-10 (Hawái)</option>
    <option value="-540" ${s.correction==-540?'selected':''}>UTC-9 (Alaska)</option>
    <option value="-480" ${s.correction==-480?'selected':''}>UTC-8 (Los Angeles)</option>
    <option value="-420" ${s.correction==-420?'selected':''}>UTC-7 (Denver)</option>
    <option value="-360" ${s.correction==-360?'selected':''}>UTC-6 (Ciudad de México)</option>
    <option value="-300" ${s.correction==-300?'selected':''}>UTC-5 (Bogotá / Lima)</option>
    <option value="-240" ${s.correction==-240?'selected':''}>UTC-4 (Santiago)</option>
    <option value="-180" ${s.correction==-180?'selected':''}>UTC-3 (Argentina)</option>
    <option value="-120" ${s.correction==-120?'selected':''}>UTC-2 (Atlántico Sur)</option>
    <option value="-60"  ${s.correction==-60?'selected':''}>UTC-1 (Azores)</option>
    <option value="0" ${s.correction==0?'selected':''}>UTC (Londres)</option>
    <option value="60"  ${s.correction==60?'selected':''}>UTC+1 (Madrid / París)</option>
    <option value="120" ${s.correction==120?'selected':''}>UTC+2 (Atenas)</option>
    <option value="180" ${s.correction==180?'selected':''}>UTC+3 (Moscú)</option>
    <option value="240" ${s.correction==240?'selected':''}>UTC+4 (Dubái)</option>
    <option value="300" ${s.correction==300?'selected':''}>UTC+5 (Karachi)</option>
    <option value="330" ${s.correction==330?'selected':''}>UTC+5:30 (India)</option>
    <option value="360" ${s.correction==360?'selected':''}>UTC+6 (Daca)</option>
    <option value="420" ${s.correction==420?'selected':''}>UTC+7 (Bangkok)</option>
    <option value="480" ${s.correction==480?'selected':''}>UTC+8 (Pekín)</option>
    <option value="540" ${s.correction==540?'selected':''}>UTC+9 (Tokio)</option>
    <option value="570" ${s.correction==570?'selected':''}>UTC+9:30 (Adelaida)</option>
    <option value="600" ${s.correction==600?'selected':''}>UTC+10 (Sídney)</option>
    <option value="660" ${s.correction==660?'selected':''}>UTC+11 (Islas Salomón)</option>
    <option value="720" ${s.correction==720?'selected':''}>UTC+12 (Auckland)</option>
    <option value="765" ${s.correction==765?'selected':''}>UTC+12:45 (Chatham)</option>
  </select>
</div>`,

DEFAULT: (s, i) => `<div class="settings-card">
  <div class="settings-card-head">
    <div><span class="eyebrow">GENERAL</span><h3>${t('cfg.title')}</h3></div>
  </div>
  <div class="kv"><span>${t('cfg.broadcast')}</span><b>${genset.broadcast_port}</b></div>
  <div class="kv"><span>${t('cfg.command')}</span><b>${genset.command_port}</b></div>
  <div class="kv"><span>${t('cfg.interval')}</span><b>${genset.report_interval} ms</b></div>
  <div class="field-row">
    <input id="broadcast_port" class="input sm" placeholder="${t('cfg.ph.broadcast')}">
    <input id="command_port" class="input sm" placeholder="${t('cfg.ph.command')}">
    <input id="ref${i}" class="input sm" placeholder="${t('cfg.ph.interval')}">
  </div>
  <label class="check"><input type="checkbox" id="otaChk" onchange="toggleOta(this.checked)"> ${t('cfg.ota')}</label>
  <div class="btn-row">
    <button class="btn primary" onclick="setPort(${i})">${t('cfg.save')}</button>
    <button class="btn danger" onclick="factoryReset()">${t('cfg.factory')}</button>
  </div>
</div>`
};
)rawliteral";


const char DeviceCards[] PROGMEM = R"rawliteral(

const SensorType = Object.freeze({
  SENSOR_NONE: 0,
  SENSOR_LUMI: 1,
  SENSOR_HUMI: 2,
  SENSOR_TEMP: 3,
  SENSOR_PRESS: 4,
  SENSOR_LEVEL: 5,
  SENSOR_AIRQ: 6,
  SENSOR_RAIN: 7,
  TYPE_DIMMER: 8,
  TYPE_RELAY: 9,
  SENSOR_TIME: 10,
  SENSOR_GENERIC: 11,
  SENSOR_CONTACT: 12
});

const TYPE_ORDER = {
  [SensorType.SENSOR_TIME]: 0,
  [SensorType.TYPE_RELAY]: 10,
  [SensorType.TYPE_DIMMER]: 11,
  [SensorType.SENSOR_TEMP]: 20,
  [SensorType.SENSOR_HUMI]: 21,
  [SensorType.SENSOR_PRESS]: 22,
  [SensorType.SENSOR_LUMI]: 23,
  [SensorType.SENSOR_LEVEL]: 24,
  [SensorType.SENSOR_AIRQ]: 25,
  [SensorType.SENSOR_RAIN]: 26,
  [SensorType.SENSOR_CONTACT]: 27,
  [SensorType.SENSOR_GENERIC]: 28
};

function visualSort(devices) {
  return [...devices].sort((a, b) => {
    const orderA = TYPE_ORDER[a.type] ?? 999;
    const orderB = TYPE_ORDER[b.type] ?? 999;
    if (orderA !== orderB) {
      return orderA - orderB;
    }
    return (a.name || '').localeCompare(b.name || '');
  });
}

function formatTime(s) {
  if (!s) return 'N/A';
  const pad = n => String(n).padStart(2, '0');
  if (s.value == null) return 'N/A';
  const offsetMin = s.correction ?? 0;
  const t = new Date((s.value + offsetMin * 60) * 1000);
  return `${pad(t.getUTCHours())}:${pad(t.getUTCMinutes())}:${pad(t.getUTCSeconds())} - ${t.getUTCFullYear()}-${pad(t.getUTCMonth()+1)}-${pad(t.getUTCDate())}`;
}

function formatSensorValue(type, value, state){
  if (value === 255 || value == null) return 'N/A';
  switch(type){
    case SensorType.SENSOR_TEMP: return value.toFixed(2) + ' °C';
    case SensorType.SENSOR_HUMI: return value.toFixed(0) + ' %';
    case SensorType.SENSOR_PRESS: return value.toFixed(0) + ' kPa';
    case SensorType.SENSOR_LEVEL: return value.toFixed(0) + ' %';
    case SensorType.SENSOR_LUMI: return (value * 108.9432 / 7074).toFixed(0) + ' lx';
    case SensorType.SENSOR_AIRQ: return value == 0 ? t('air.good') : value == 1 ? t('air.warn') : value == 2 ? t('air.bad') : 'N/A';
    case SensorType.SENSOR_RAIN: return value ? t('yn.yes') : t('yn.no');
    case SensorType.SENSOR_CONTACT: return state ? t('yn.closed') : t('yn.open');
    case SensorType.SENSOR_GENERIC: return Number(value).toFixed(2);
    default: return value ?? '-';
  }
}

function devStatus(s) {
  if (!s || s.local) return '<span class="status"><span class="dot ok"></span>' + t('status.local') + '</span>';
  return (s.age_ms != null && s.age_ms <= 30000)
    ? '<span class="status"><span class="dot info"></span>' + t('status.remote') + '</span>'
    : '<span class="status"><span class="dot bad"></span>' + t('status.offline') + '</span>';
}

function deviceCard(name, value, id, state, fade, type, sensor = null) {
  if (type === SensorType.SENSOR_TIME) return '';
  if (type === SensorType.TYPE_RELAY) {
    const pulse = sensor && sensor.pulse;
    const persist = sensor && sensor.persist;
    const isPulseActive = pulse && state;
    const displayState = isPulseActive ? 'PULSE' : (state ? t('dev.on') : t('dev.off'));
    const stateClass = isPulseActive ? 'pulse' : (state ? 'on' : 'off');
    return `<div class="device-card" data-name="${name}" data-type="${type}">
      <div class="device-head">
        <span class="chip neutral">RELAY</span>
        <span class="device-name">${name}</span>
      </div>
      <div class="device-control">
        <button type="button" class="switch ${state?'on':'off'}" role="switch" aria-checked="${state}" aria-label="Alternar relé ${name}" onclick="toggleDevice(${id})"><span class="knob"></span></button>
        <span class="state-text ${stateClass}" id="dev_state_${id}">${displayState}</span>
      </div>
      <div class="device-footer">
        ${devStatus(sensor)}
      </div>
    </div>`;
  }
if (type === SensorType.TYPE_DIMMER) {
    const displayValue = state ? value : 0;
    return `<div class="device-card dimmer" data-name="${name}" data-type="${type}">
      <div class="device-head">
        <span class="chip neutral">DIMMER</span>
        <span class="device-name">${name}</span>
      </div>
      <div class="dimmer-row">
        <span class="value-lg"><b id="dev_val_${id}">${displayValue}</b><span class="unit">%</span></span>
        <button type="button" class="switch ${state?'on':''}" role="switch" aria-checked="${state?state:false}" aria-label="Alternar dimmer ${name}" onclick="toggleDevice(${id})"><span class="knob"></span></button>
      </div>
      <input type="range" min="0" max="100" name="${name}" value="${displayValue}" id="slider_${id}" oninput="onDimmerInput(${id}, this.value)" onchange="onDimmerChange(${id}, this.value)" aria-label="Nivel ${name}">
      <div class="device-row">${devStatus(sensor)}<span class="small-note">${t('dev.drag')}</span></div>
    </div>`;
  }
  if (type >= 1 && type <= 7 || type === 11 || type === 12) {
    const dv = (value === 255 || value == null) ? 'N/A' : formatSensorValue(type, value, state);
    return `<div class="device-card sensor" data-name="${name}" data-type="${type}">
      <div class="device-head">
        <span class="chip neutral">${sLabel(type)}</span>
        <span class="device-name">${name}</span>
      </div>
      <div class="metric" id="dev_${id}">${dv}</div>
      <div class="device-row">${devStatus(sensor)}</div>
    </div>`;
  }
  return `<div class="device-card" data-name="${name}" data-type="${type}">
    <div class="device-head"><span class="device-name">${name}</span></div>
    <div class="metric" id="dev_${id}">${value}</div>
  </div>`;
}
)rawliteral";


const char JS[] PROGMEM = R"rawliteral(

/* -------------------- DEVICES -------------------- */

var calibPromise = null;
let sensors = [];

// Toast notifications
function showToast(type, message, duration = 3000) {
  let container = document.getElementById('toast-container');
  if (!container) {
    container = document.createElement('div');
    container.id = 'toast-container';
    document.body.appendChild(container);
  }
  const toast = document.createElement('div');
  toast.className = 'toast ' + type;
  toast.textContent = message;
  container.appendChild(toast);
  setTimeout(() => {
    toast.classList.add('fade-out');
    setTimeout(() => {
      if (toast.parentNode) toast.parentNode.removeChild(toast);
    }, 300);
  }, duration);
}

/* -------------------- DEVICES -------------------- */

async function getCalib(force = false) {
  if (!force && Array.isArray(sensors) && sensors.length) {
    return sensors;
  }
  if (calibPromise) {
    return calibPromise;
  }
  calibPromise = (async () => {
    const r = await fetch('/calib');
    if (!r.ok) {
      throw new Error(`GET /calib failed: ${r.status}`);
    }
    const data = await r.json();
    sensors = Array.isArray(data) ? data : [];
    return sensors;
  })().finally(() => {
    calibPromise = null;
  });
  return calibPromise;
}

function isDeviceVisible(s) {
  if (!s) return false;
  if (s.id === 0 || s.id == null) return false;
  if (s.type === undefined || s.type === SensorType.SENSOR_NONE) return false;
  if (!(s.type in TYPE_ORDER)) return false;
  if (s.local === true) return true;
  return typeof s.age_ms === 'number' && s.age_ms <= 30000;
}

let devSearchTerm = '';
let settingsSearchTerm = '';

async function loadDevices() {
  try {
    const data = (await getCalib(true)).filter(isDeviceVisible);
    let mobile = '';
    let actuators = '';
    let deviceSensors = '';
    let actCount = 0;
    let sensCount = 0;
    visualSort(data).forEach((s, i) => {
      const card = deviceCard(s.name, s.value, s.id, s.state, s.fade, s.type, s);
      mobile += card;
      if (s.type === SensorType.TYPE_RELAY || s.type === SensorType.TYPE_DIMMER) {
        actuators += card;
        actCount++;
      } else if (s.type !== SensorType.SENSOR_TIME) {
        deviceSensors += card;
        sensCount++;
      }
    });
    const now = new Date();
    const clock = String(now.getHours()).padStart(2,'0') + ':' + String(now.getMinutes()).padStart(2,'0') + ':' + String(now.getSeconds()).padStart(2,'0') + ' - ' + String(now.getUTCFullYear()) + '-' + String(now.getUTCMonth() + 1).padStart(2, '0') + '-' + String(now.getUTCDate()).padStart(2, '0');
    const healthStatus = document.getElementById('systemHealth');
    if (healthStatus && sensors && sensors.length > 0) {
      const visibleSensors = sensors.filter(s => s.local || (!s.local && s.age_ms && s.age_ms <= 30000));
      if (visibleSensors.length > 0) {
        healthStatus.title = `${visibleSensors.length} sensors`;
        healthStatus.innerHTML = `<span class="dot ok"></span>${visibleSensors.length}`;
      } else {
        healthStatus.title = 'No sensors';
        healthStatus.innerHTML = '<span class="dot bad"></span>0';
      }
    }
    document.getElementById('dash_stats').innerHTML = `
      <div class="dash-stats">
        <div class="stat-card"><span class="stat-label">${t('stat.actuators')}</span><span class="stat-value">${actCount}</span></div>
        <div class="stat-card"><span class="stat-label">${t('stat.sensors')}</span><span class="stat-value">${sensCount}</span></div>
        <div class="stat-card"><span class="stat-label">${t('stat.time')}</span><span class="stat-value time">${clock}</span></div>
      </div>
    `;
    document.getElementById('dash_lists').innerHTML = `
      <div class="devices-mobile-list">${mobile}</div>
      <div class="devices-desktop-layout">
        <div class="devices-column devices-actuators">
          <div class="section-title">${t('stat.actuators')}</div>
          <div class="devices-actuator-grid">${actuators || "<div class='empty sm'>" + t('no.actuators') + "</div>"}</div>
        </div>
        <div class="devices-column devices-sensors">
          <div class="section-title">${t('stat.sensors')}</div>
          <div class="devices-sensor-grid">${deviceSensors || "<div class='empty sm'>" + t('no.sensors') + "</div>"}</div>
        </div>
      </div>
    `;
    if (devSearchTerm.trim() !== '') applyDeviceFilter(devSearchTerm);
  } catch (e) {
    console.log('loadDevices err', e);
  }
}

/* -------------------- CALIB -------------------- */

function sensorCardRenderer(s) {
  const renderer = TYPE_RENDERERS[s.type];
  if (!renderer) {
    console.warn(`[calib] tipo desconocido ${s.type} para '${s.name}', omitido`);
    return null;
  }
  return renderer;
}

const TYPE_RENDERERS = {
  [SensorType.TYPE_RELAY]: cardRenderers.REL,
  [SensorType.TYPE_DIMMER]: cardRenderers.DIMM,
  [SensorType.SENSOR_TEMP]: cardRenderers.TEMP,
  [SensorType.SENSOR_LUMI]: cardRenderers.LUMI,
  [SensorType.SENSOR_PRESS]: cardRenderers.PRES,
  [SensorType.SENSOR_RAIN]: cardRenderers.RAIN,
  [SensorType.SENSOR_AIRQ]: cardRenderers.AIRQ,
  [SensorType.SENSOR_LEVEL]: cardRenderers.LEVE,
  [SensorType.SENSOR_GENERIC]: cardRenderers.GENERIC,
  [SensorType.SENSOR_CONTACT]: cardRenderers.CONTACT,
  [SensorType.SENSOR_HUMI]: cardRenderers.HUMI,
  [SensorType.SENSOR_TIME]: cardRenderers.TIME
};

async function loadCalib() {
  try {
    const data = await getCalib(true);

    // Separate local and remote entries
    const localEntries = [];
    const remoteByDevice = {};

    data.forEach((s, i) => {
      if (s.local) {
        localEntries.push({ s, i });
      } else {
        const duid = s.device_uid;
        if (!remoteByDevice[duid]) {
          remoteByDevice[duid] = {
            device_uid: duid,
            device_ip: s.ip,
            name: s.name,
            entries: []
          };
        }
        remoteByDevice[duid].entries.push({ s, i });
      }
    });

    let html = `<div class='settings-grid'>`;

    // LOCAL section
    if (localEntries.length > 0) {
      html += `
<div class="settings-section">
  <button type="button" class="accordion-header" onclick="toggleAccordion(this)">
    <span class="accordion-chevron">▼</span>
    <span class="accordion-title">${t('cfg.local')}</span>
    <span class="accordion-count">${localEntries.length}</span>
  </button>
  <div class="accordion-content">
    <div class="settings-grid">`;
      localEntries.forEach(({ s, i }) => {
        const render = sensorCardRenderer(s);
        if (render) html += render(s, i);
      });
      html += `</div></div></div>`;
    }

    // REMOTE section
    const remoteDevices = Object.values(remoteByDevice);
    if (remoteDevices.length > 0) {
      html += `
<div class="settings-section">
  <button type="button" class="accordion-header" onclick="toggleAccordion(this)">
    <span class="accordion-chevron">▼</span>
    <span class="accordion-title">${t('cfg.remote')}</span>
    <span class="accordion-count">${remoteDevices.length}</span>
  </button>
  <div class="accordion-content">`;

      remoteDevices.forEach(device => {
        const ip = device.device_ip;
        const hasIp = ip && ip !== '0.0.0.0';
        const openGuiLink = hasIp
          ? `<a href="http://${device.device_ip}/" target="_blank" rel="noopener noreferrer" class="btn ghost sm remote-gui-link">${t('cfg.openGui')}</a>`
          : `<span class="remote-gui-disabled">${t('cfg.openGui')}</span>`;

        html += `
<div class="remote-device-group">
  <div class="remote-device-header">
    <span class="remote-device-name">Qymera ${device.device_uid.toString(16).toUpperCase().padStart(8, '0')}</span>
    ${openGuiLink}
  </div>
  <div class="remote-device-content">
    <div class="settings-grid">`;

        device.entries.forEach(({ s, i }) => {
          const render = sensorCardRenderer(s);
          if (render) html += render(s, i);
        });

        html += `</div></div></div>`;
      });

      html += `</div></div>`;
    }

    // General settings (network, etc.)
    html += `
      <div class="settings-general">
        <div class="settings-row">
          <div class="settings-card">
            <div class="settings-card-head">
              <div><span class="eyebrow">NETWORK</span><h3>${t('net.title')}</h3></div>
            </div>
            <form action="/save" method="post">
              <div class="field">
                <label for="ssid">${t('net.ssid')}</label>
                <input id="ssid" class="input" name="ssid" autocomplete="off" placeholder="${t('net.ssid.ph')}" required>
              </div>
              <div class="field">
                <label for="pass">${t('net.pass')}</label>
                <input id="pass" class="input" name="pass" type="password" autocomplete="off" placeholder="${t('net.pass.ph')}" required>
              </div>
              <button type="submit" class="btn primary">${t('net.save')}</button>
            </form>
            <p class="small-note" style="margin:12px 0 0">${t('net.note')}</p>
          </div>
          <div class="settings-card" id="fwCard">
            <div class="settings-card-head">
              <div><span class="eyebrow">FIRMWARE</span><h3>${t('fw.title')}</h3></div>
            </div>
            <div class="kv"><span>${t('fw.current')}</span><b><span id="fwCurrent">—</span></b></div>
            <div class="kv"><span>${t('fw.channel')}</span><b><span id="fwChannel">—</span></b></div>
            <div class="kv"><span>${t('fw.latest')}</span><b><span id="fwLatest">—</span></b></div>
            <div id="fwError" class="small-note" style="display:none;color:#d64545"></div>
            <div id="fwProgressWrap" style="display:none">
              <div style="height:8px;background:var(--border);border-radius:999px;overflow:hidden">
                <div id="fwProgress" style="height:100%;width:0%;background:var(--accent);transition:width .3s"></div>
              </div>
              <div id="fwProgressLabel" class="small-note" style="margin-top:6px"></div>
            </div>
            <div class="btn-row">
              <button class="btn primary" id="fwCheckBtn" onclick="fwCheck()">${t('fw.check')}</button>
              <button class="btn primary" id="fwUpdateBtn" style="display:none" onclick="fwUpdate()">${t('fw.update')}</button>
            </div>
          </div>
          ${cardRenderers.DEFAULT(
            { value: 0, min: 0, max: 0 },
            data.length
          )}
        </div>
      </div>
    `;
    html += "</div>";
    document.getElementById('cards').innerHTML = html;
    if (settingsSearchTerm.trim() !== '') applySettingsFilter(settingsSearchTerm);

    // Initialize accordions
    document.querySelectorAll('.accordion-header').forEach(btn => {
      const content = btn.nextElementSibling;
      if (content) content.style.display = 'none';
    });
  } catch (e) {
    console.log('loadCalib err', e);
  }
}

async function updateSettingsValues() {
  try {
    const data = await getCalib(false);
    data.forEach((s, i) => {
      const el = document.getElementById(`v${i}`);
      if (!el) return;
      if (s.type === SensorType.TYPE_DIMMER) el.innerText = s.fade;
      else if (s.type === SensorType.TYPE_RELAY) el.innerText = s.pulse_ms ?? 0;
      else if (s.type === SensorType.SENSOR_TIME) el.innerHTML = s ? formatTime(s) : 'N/A';
      else el.innerText = formatSensorValue(s.type, s.value, s.state);
    });
  } catch (e) {
    console.log('updateSettingsValues err', e);
  }
}

/* -------------------- REMOTE ACTIONS -------------------- */

async function isVirtual(id, path, body = null) {
  const sensor = sensors.find(s => s.id == id);
  if (!sensor || sensor.local || sensor.type === SensorType.SENSOR_TIME)
    return { handled:false, ok:false };
  let res;
  try {
    const ctrl = new AbortController();
    const timer = setTimeout(() => ctrl.abort(), 5000);
    res = await fetch(`http://${sensor.ip}${path}`, {
      method:'POST',
      headers:{ 'Content-Type': 'application/x-www-form-urlencoded' },
      body,
      signal: ctrl.signal
    });
    clearTimeout(timer);
  } catch (e) {
    showToast('error', tf('alert.isVirtualNet', { ip: sensor.ip }));
    return { handled:true, ok:false };
  }
  if (!res.ok) {
    let detail = '';
    try { detail = await res.text(); } catch(_) {}
    showToast('error', tf('alert.isVirtualHttp', { ip: sensor.ip, status: res.status, detail: detail ? ': ' + detail : '' }));
    return { handled:true, ok:false };
  }
  return { handled:true, ok:true };
}

/* -------------------- ACTIONS -------------------- */

async function toggleMatterSwitch(i, id, name) {
  const btn = document.getElementById(`matterBtn${i}`);
  const wasOn = btn.classList.contains('ok');
  const on = !wasOn;
  btn.classList.toggle('ok', on);
  btn.setAttribute('aria-pressed', on ? 'true' : 'false');
  btn.textContent = on ? t('chip.enabled') : t('chip.disabled');
  const body =
    `id=${encodeURIComponent(id)}` +
    `&type=avail` +
    `&ref=${encodeURIComponent(on ? 1 : 0)}`;
  const r = await isVirtual(id, '/calib/set', body);
  if (r.handled) {
    if (!r.ok) {
      btn.classList.toggle('ok', wasOn);
      btn.setAttribute('aria-pressed', wasOn ? 'true' : 'false');
      btn.textContent = wasOn ? t('chip.enabled') : t('chip.disabled');
      return;
    }
    const sensor = sensors.find(s => s.id == id);
    if (sensor) sensor.avail = on;
    return;
  }
  try {
    const res = await fetch('/calib/set', {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body
    });
    if (!res.ok) {
      let detail = '';
      try { detail = await res.text(); } catch(_) {}
      showToast('error', tf('alert.localError', { status: res.status, detail: detail ? ': ' + detail : '' }));
      btn.classList.toggle('ok', wasOn);
      btn.setAttribute('aria-pressed', wasOn ? 'true' : 'false');
      btn.textContent = wasOn ? t('chip.enabled') : t('chip.disabled');
      return;
    }
    showToast('success', t('chip.enabled'));
    const sensor = sensors.find(s => s.id == id);
    if (sensor) sensor.avail = on;
  } catch (e) {
    showToast('error', t('alert.localNet'));
  }
}

async function setPort(i) {
  const params = new URLSearchParams();
  const b = document.getElementById('broadcast_port').value.trim();
  const c = document.getElementById('command_port').value.trim();
  const r = document.getElementById(`ref${i}`).value.trim();
  if (b) params.append('broadcast', b);
  if (c) params.append('command', c);
  if (r) params.append('interval', r);
  const res = await fetch('/genset/save', {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: params.toString()
  });
  if (!res.ok) {
    let detail = '';
    try { detail = await res.text(); } catch(_) {}
    showToast('error', tf('alert.saveError', { status: res.status, detail: detail ? ': ' + detail : '' }));
    return;
  }
  showToast('success', t('alert.saved'));
}

async function setCalib(i, type, name, refOverride = null) {
  const sensor = sensors[i];
  if (!sensor)
    return false;
  const ref = refOverride !== null
    ? String(refOverride)
    : (document.getElementById(`ref${i}`)?.value ?? '').trim();
  // "Set ref value" requires an explicit value; the quick min/max buttons
  // ("Set 0%"/"Set 100%") intentionally send none so the node uses the live
  // raw reading as the reference.
  if (type === 'ref' && ref === '') {
    showToast('error', t('cal.valueReq'));
    return false;
  }
  let body =
    `id=${encodeURIComponent(sensor.id)}` +
    `&type=${encodeURIComponent(type)}`;
  if (ref !== '')
    body += `&ref=${encodeURIComponent(ref)}`;
  const r = await isVirtual(sensor.id, '/calib/set', body);
  if (r.handled)
    return r.ok;
  try {
    const res = await fetch('/calib/set', {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body
    });
    if (!res.ok) {
      let detail = '';
      try { detail = await res.text(); } catch(_) {}
      showToast('error', tf('alert.localError', { status: res.status, detail: detail ? ': ' + detail : '' }));
      return false;
    }
    return true;
  } catch (e) {
    showToast('error', t('alert.localNet'));
    return false;
  }
}

function toggleAccordion(btn) {
  const content = btn.nextElementSibling;
  const isExpanded = btn.getAttribute('aria-expanded') === 'true';
  btn.setAttribute('aria-expanded', !isExpanded);
  if (isExpanded) {
    content.style.display = 'none';
  } else {
    content.style.display = 'block';
  }
}

function updateRelayUI(i){
  const s = sensors[i];
  if(!s) return;
  const persistBtn = document.querySelector(`button[onclick*="togglePersist(${i})"]`);
  if(persistBtn){
    persistBtn.classList.toggle('on', !!s.persist);
    persistBtn.setAttribute('aria-checked', s.persist ? 'true' : 'false');
  }
  const pulseBtn = document.querySelector(`button[onclick*="togglePulse(${i})"]`);
  if(pulseBtn){
    pulseBtn.classList.toggle('on', !!s.pulse);
    pulseBtn.setAttribute('aria-checked', s.pulse ? 'true' : 'false');
  }
  const pulseRow = document.getElementById('pulseRow' + i);
  if(pulseRow) pulseRow.style.display = s.pulse ? '' : 'none';
}

function setPersistence(i, value) {
  return setCalib(i, 'persist', null, value ? '1' : '0');
}

function togglePersist(i) {
  const sensor = sensors[i];
  if (!sensor) return;
  const finish = (ok) => {
    if (!ok) {
      showToast('error', t('alert.failed'));
      return;
    }
    sensor.persist = !sensor.persist;
    if (sensor.persist) sensor.pulse = false;
    updateRelayUI(i);
  };
  // If pulse is active, disable it first (mutual exclusion)
  if (sensor.pulse) {
    setCalib(i, 'pulse', null, '0').then(pulseOk => {
      if (!pulseOk) {
        showToast('error', t('alert.failed'));
        return;
      }
      setPersistence(i, !sensor.persist).then(finish).catch(() => showToast('error', t('alert.failed')));
    }).catch(() => showToast('error', t('alert.failed')));
    return;
  }
  // Pulse not active, just toggle persist
  setPersistence(i, !sensor.persist).then(finish).catch(() => showToast('error', t('alert.failed')));
}

function togglePulse(i) {
  const sensor = sensors[i];
  if (!sensor) return;
  const finish = (ok) => {
    if (!ok) {
      showToast('error', t('alert.failed'));
      return;
    }
    sensor.pulse = true;
    sensor.persist = false;
    updateRelayUI(i);
  };
  // If pulse is already active, turn it off
  if (sensor.pulse) {
    setCalib(i, 'pulse', null, '0').then(ok => {
      if (!ok) {
        showToast('error', t('alert.failed'));
        return;
      }
      sensor.pulse = false;
      updateRelayUI(i);
    }).catch(() => showToast('error', t('alert.failed')));
    return;
  }
  // If persist is active, disable it first (mutual exclusion)
  if (sensor.persist) {
    setPersistence(i, false).then(persistOk => {
      if (!persistOk) {
        showToast('error', t('alert.failed'));
        return;
      }
      setCalib(i, 'pulse', null, '1').then(finish).catch(() => showToast('error', t('alert.failed')));
    }).catch(() => showToast('error', t('alert.failed')));
    return;
  }
  // Persist not active, just enable pulse
  setCalib(i, 'pulse', null, '1').then(finish).catch(() => showToast('error', t('alert.failed')));
}

async function toggleDevice(id) {
  const body = 'id=' + encodeURIComponent(id);
  const r = await isVirtual(id, '/toggle', body);
  if (r.handled) {
    if (r.ok) setTimeout(loadDevices, 100);
    return;
  }
  try {
    const res = await fetch('/toggle', {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body
    });
    if (!res.ok) {
      let detail = '';
      try { detail = await res.text(); } catch(_) {}
      showToast('error', tf('alert.localError', { status: res.status, detail: detail ? ': ' + detail : '' }));
      return;
    }
    setTimeout(loadDevices, 100);
  } catch (e) {
    showToast('error', t('alert.localNet'));
  }
}

function onDimmerInput(id, value) {
  const el = document.getElementById('dev_val_' + id);
  if (el) el.innerText = value;
  const sl = document.getElementById('slider_' + id);
  if (sl) {
    const pct = Number(value) || 0;
    sl.style.background = `linear-gradient(to right,var(--accent) ${pct}%,var(--border) ${pct}%)`;
  }
}

const dimmerTimeouts = {};

function onDimmerChange(id, value) {
  if (dimmerTimeouts[id]) {
    clearTimeout(dimmerTimeouts[id]);
  }
  dimmerTimeouts[id] = setTimeout(() => {
    sendDimmer(id, value);
  }, 120);
}

async function sendDimmer(id, value) {
  const body = `id=${id}&value=${value}`;
  try {
    const r = await isVirtual(id, '/dimmer', body);
    if (r.handled) {
      if (r.ok) setTimeout(loadDevices, 100);
      return;
    }
    const res = await fetch('/dimmer', {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body
    });
    if (!res.ok) {
      let detail = '';
      try { detail = await res.text(); } catch(_) {}
      showToast('error', tf('alert.localError', { status: res.status, detail: detail ? ': ' + detail : '' }));
      return;
    }
    setTimeout(loadDevices, 100);
  } catch (e) {
    console.log('sendDimmer err', e);
  }
}

function factoryReset() {
  if (!confirm(t('factory.confirm'))) return;
  fetch('/factory', { method: 'POST' })
    .then(() => showToast('success', t('factory.doing')))
    .catch(() => showToast('error', t('factory.err')));
}

async function toggleOta(enabled) {
  if (!confirm(tf(enabled ? 'ota.enableMsg' : 'ota.disableMsg'))) return;
  try {
    await fetch('/ota/toggle?enabled=' + (enabled ? 1 : 0));
    showToast('success', t(enabled ? 'ota.enableMsg' : 'ota.disableMsg'));
  } catch(e) {
    showToast('error', t('alert.localNet'));
  }
}

async function syncOtaCheckbox() {
  for (let i = 0; i < 10; i++) {
    const el = document.getElementById('otaChk');
    if (el) {
      try {
        const r = await fetch('/ota/status');
        const j = await r.json();
        el.checked = j.ota === 1;
        el.style.display = 'none';
        el.offsetHeight;
        el.style.display = '';
      } catch(e) {
        console.log('syncOta err', e);
      }
      return;
    }
    await new Promise(r => setTimeout(r, 100));
  }
}


/* -------------------- WEB OTA (FIRMWARE) -------------------- */

let fwPoll = null;

async function fwRefresh() {
  let j = null;
  try {
    const r = await fetch('/firmware');
    j = await r.json();
  } catch (e) { return; }
  const sub = document.getElementById('brandSub');
  if (sub && j.version) sub.innerText = j.version;
  const card = document.getElementById('fwCard');
  if (!card) return;
  const cur = document.getElementById('fwCurrent');
  const lat = document.getElementById('fwLatest');
  const cha = document.getElementById('fwChannel');
  const err = document.getElementById('fwError');
  const wrap = document.getElementById('fwProgressWrap');
  const bar = document.getElementById('fwProgress');
  const lbl = document.getElementById('fwProgressLabel');
  const checkBtn = document.getElementById('fwCheckBtn');
  const updBtn = document.getElementById('fwUpdateBtn');
  if (cur) cur.innerText = j.version || '-';
  if (cha) cha.innerText = j.channel || '-';
  if (lat) lat.innerText = j.latest || '-';
  if (err) {
    err.style.display = j.error ? '' : 'none';
    err.innerText = j.error ? tf('fw.error', { msg: j.error }) : '';
  }
  if (updBtn) updBtn.style.display = (j.state === 'ready' && j.available) ? '' : 'none';
  const busy = j.state === 'checking' || j.state === 'downloading' || j.state === 'installing';
  if (checkBtn) {
    checkBtn.disabled = busy;
    checkBtn.innerText = (j.state === 'checking') ? t('fw.checking') : t('fw.check');
  }
  if (wrap && bar && lbl) {
    const show = j.state === 'checking' || j.state === 'downloading' || j.state === 'installing';
    wrap.style.display = show ? '' : 'none';
    bar.style.width = (j.progress || 0) + '%';
    if (j.state === 'downloading') lbl.innerText = tf('fw.downloading', { p: j.progress });
    else if (j.state === 'installing') lbl.innerText = t('fw.installing');
    else if (j.state === 'checking') lbl.innerText = t('fw.checking');
  }
  if (busy) {
    if (!fwPoll) fwPoll = setInterval(fwRefresh, 700);
  } else {
    if (fwPoll) { clearInterval(fwPoll); fwPoll = null; }
  }
}

async function fwCheck() {
  if (fwPoll) return;
  try {
    const r = await fetch('/firmware/check');
    const j = await r.json();
    if (j.ok) fwRefresh();
  } catch (e) { showToast('error', t('alert.localNet')); }
}

async function fwUpdate() {
  try {
    const r = await fetch('/firmware/update', { method: 'POST' });
    const j = await r.json();
    if (!j.ok) { showToast('error', t('alert.localNet')); return; }
    fwRefresh();
  } catch (e) { showToast('error', t('alert.localNet')); }
}
/* -------------------- THEMES -------------------- */

const THEME_MAP = {
  '#414141':'dark', '#f7f2ff':'light', '#4c834e':'forest', '#c1af8d':'sand'
};

function setBackground(color){
  const theme = THEME_MAP[color] || 'dark';
  document.documentElement.setAttribute('data-theme', theme);
  localStorage.setItem('bgColor', color);
  localStorage.setItem('theme', theme);
}

document.querySelectorAll('.themeDot').forEach(dot =>
  dot.onclick = () => setBackground(dot.dataset.bg)
);

const savedBg = localStorage.getItem('bgColor');

if(savedBg){
  setBackground(savedBg);
}

/* -------------------- LOGS -------------------- */

var logTimer = null;

function showLogStatus(kind, msg){
  const el = document.getElementById('logStatus');
  if(!el) return;
  el.className = 'log-status' + (kind ? ' ' + kind : '');
  el.textContent = msg;
}

async function refreshLogs(){
  const coreEl = document.getElementById('log-core');
  const evntEl = document.getElementById('log-events');
  const sensEl = document.getElementById('log-sensors');
  const scrollPositions = {
    core: coreEl ? coreEl.scrollTop : 0,
    evnt: evntEl ? evntEl.scrollTop : 0,
    sens: sensEl ? sensEl.scrollTop : 0
  };
  try {
    const r = await fetch('/logs', { cache: 'no-store' });
    if(!r.ok) {
      showLogStatus('error', t('log.error'));
      return;
    }
    let logs;
    try {
      logs = await r.json();
    } catch(e) {
      showLogStatus('error', t('log.invalid'));
      return;
    }
    if(!Array.isArray(logs)) logs = [];
    renderLogs(logs);
    const now = new Date();
    showLogStatus('ok', tf('log.updated', {
      time: String(now.getUTCHours()).padStart(2,'0') + ':' +
            String(now.getUTCMinutes()).padStart(2,'0') + ':' +
            String(now.getUTCSeconds()).padStart(2,'0'),
      n: logs.length
    }));
    if (coreEl) coreEl.scrollTop = scrollPositions.core;
    if (evntEl) evntEl.scrollTop = scrollPositions.evnt;
    if (sensEl) sensEl.scrollTop = scrollPositions.sens;
  } catch(e){
    showLogStatus('error', t('log.netError'));
  }
}

function clearLogs(){
  fetch('/logs/clear', {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' }
  }).then(r => {
    if(!r.ok) throw new Error('HTTP ' + r.status);
    showToast('success', t('log.cleared'));
    refreshLogs();
  }).catch(() => showToast('error', t('alert.localNet')));
}

const LOG_PANELS = ['CORE', 'EVNT', 'SENS'];

function renderLogs(logs){
  const panelEls = {
    CORE: document.getElementById('log-core'),
    EVNT: document.getElementById('log-events'),
    SENS: document.getElementById('log-sensors')
  };
  LOG_PANELS.forEach(k => {
    const p = panelEls[k];
    if(!p) return;
    p.querySelectorAll('.log-entry, .log-empty').forEach(n => n.remove());
  });
  const counts = { CORE: 0, EVNT: 0, SENS: 0 };
  logs.forEach(e => {
    if(!e || typeof e !== 'object') return;
    const layer = e.l === 'CORE' ? 'CORE' : e.l === 'EVNT' ? 'EVNT' : 'SENS';
    if (counts[layer] >= 200) return;
    const panel = panelEls[layer];
    if(!panel) return;
    counts[layer]++;
    const el = document.createElement('div');
    el.className = 'log-entry';
    const timeEl = document.createElement('span');
    timeEl.className = 't';
    timeEl.textContent = e.t != null ? String(e.t) : '';
    const level = e.v != null ? String(e.v) : 'INF';
    const levelEl = document.createElement('span');
    levelEl.className = 'l ' + level.toLowerCase();
    levelEl.textContent = level;
    const msgEl = document.createElement('span');
    msgEl.className = 'm';
    msgEl.textContent = e.m != null ? String(e.m) : '';
    el.appendChild(timeEl);
    el.appendChild(levelEl);
    el.appendChild(msgEl);
    panel.appendChild(el);
  });
  LOG_PANELS.forEach(k => {
    const p = panelEls[k];
    if(!p || counts[k] > 0) return;
    const emptyEl = document.createElement('div');
    emptyEl.className = 'log-empty';
    emptyEl.textContent = t('log.empty');
    p.appendChild(emptyEl);
  });
}

function startLogAutoRefresh(){
  if(logTimer) clearInterval(logTimer);
  if (document.visibilityState === 'hidden') return;
  if (window.activeTab !== 'logs') return;
  logTimer = setInterval(refreshLogs, 2000);
}

function stopLogAutoRefresh(){
  if(logTimer){ clearInterval(logTimer); logTimer = null; }
}

)rawliteral";


const char AutoWizJS[] PROGMEM = R"rawliteral(

/*--------------------------------------------------- WIZARD AUTOMATIONS ------------------------------------------------------------------------*/

let wizard={step:0,data:{sensors:[],actuators:[],type:0,logic:1,delay:0,cooldown:0,interval:0,actions:[],levels:[],conditions:{},time_hour:0,time_minute:0,date_start:'',date_end:''}};
function sensorByIndex(index){
  return sensors.find(s => s.index === index);
}

async function loadSensorsAndActuators(){
  await getCalib(false);
}

function startWizard(edit=-1){
  wizard={step:0,data:{sensors:[],actuators:[],type:0,logic:1,delay:0,cooldown:0,interval:0,actions:[],levels:[],conditions:{},time_hour:0,time_minute:0,date_start:'',date_end:''}};
  if(edit>=0 && window.rules && window.rules[edit]) {
    const rule = window.rules[edit];
    wizard.data = {
      id: rule.id,
      sensors: rule.sensors || [],
      actuators: rule.actuators || [],
      type: rule.type || 0,
      logic: rule.logical_and ? 1 : 0,
      delay: rule.delay_ms || 0,
      cooldown: rule.cooldown_ms || 0,
      interval: rule.interval_ms || 0,
      actions: Array.isArray(rule.actions) ? [...rule.actions] : [rule.actions || 0],
      levels: Array.isArray(rule.levels) ? [...rule.levels] : [rule.levels || 0],
      conditions: {},
      time_hour: Math.floor((rule.time_s || 0) / 3600),
      time_minute: Math.floor(((rule.time_s || 0) % 3600) / 60),
      date_start: `${rule.year_start}-${String(rule.month_start).padStart(2,'0')}-${String(rule.day_start).padStart(2,'0')}`,
      date_end: `${rule.year_end}-${String(rule.month_end).padStart(2,'0')}-${String(rule.day_end).padStart(2,'0')}`
    };

    if(rule.cmp && rule.threshold) {
      rule.sensors.forEach((sensorIdx, posIdx) => {
        wizard.data.conditions[sensorIdx] = {
          cmp: rule.cmp[posIdx] || 0,
          threshold: rule.threshold[posIdx] || 0
        };
      });
    }
  }
  showStep(0);
}

function getRelevantSteps(){
  const steps = [0];

  if(wizard.data.type === 0 || wizard.data.type === 1) {
    steps.push(1, 2);
    if(wizard.data.sensors.length > 1) {
      steps.push(3);
    }
  }

  if(wizard.data.type === 2) {
    steps.push(3);
  }

  steps.push(4, 5);

  if(wizard.data.type === 0 || wizard.data.type === 1) {
    steps.push(6);
  } else if(wizard.data.type === 3) {
    steps.push(6);
  }

  return steps;
}

function showStep(n){
  const steps = getRelevantSteps();
  if(n >= steps.length) return;

  wizard.step = n;
  const stepNum = steps[n];

  let content = '';

  /* ================= STEP 0 ================= */
  if(stepNum === 0) {
    content = `<div class="wizard-step">
      <h3>${t('wiz.type')}</h3>
       ${[0,1,2,3].map(ti=>{
         const cfg = [
           {txt:t('wiz.edge'),desc:t('wiz.edge.desc')},
           {txt:t('wiz.thresh'),desc:t('wiz.thresh.desc')},
           {txt:t('wiz.time'),desc:t('wiz.time.desc')},
           {txt:t('wiz.intervalTxt'),desc:t('wiz.interval.desc')}
         ][ti];
         return `
         <label class="wizard-option">
           <input type="radio" name="type" value="${ti}" ${wizard.data.type===ti?'checked':''}>
           <div>
             <strong>${cfg.txt}</strong>
             <small>${cfg.desc}</small>
           </div>
         </label>`;
       }).join('')}
    </div>`;
  }

  /* ================= STEP 1 ================= */
  else if(stepNum === 1) {
    content = `<div class="wizard-step">
      <h3>${t('wiz.sensors')}</h3>
      <select id="sensorList" multiple size="5" class="input"></select>
      <span class="wizard-hint">${t('wiz.hint')}</span>
    </div>`;
  }

  /* ================= STEP 2 ================= */
  else if(stepNum === 2) {

    if(wizard.data.sensors.length === 0){
      content = `<h3>${t('wiz.noSensors')}</h3>`;
    } else {

      content = `<div class="wizard-step"><h3>${t('wiz.conditions')}</h3>`;

      wizard.data.sensors.forEach(sIdx=>{
        const s = sensorByIndex(sIdx);
        if(!s) return;
        const cond = wizard.data.conditions[sIdx] || {cmp:0,threshold:0};

        const val = formatSensorValue(s.type, s.value, s.state);

        content += `
        <div class="cond-box">
          <div class="rule-info"><b>${s.name}</b><span>${val}</span></div>
          ${wizard.data.type === 0 ? `
            <select id="cmp_${sIdx}">
              <option value="0" ${cond.cmp===0?'selected':''}>${t('wiz.rising')}</option>
              <option value="1" ${cond.cmp===1?'selected':''}>${t('wiz.falling')}</option>
            </select>
          ` : `
            <div class="field-row">
              <select id="cmp_${sIdx}" class="input sm input-full">
                <option value="0" ${cond.cmp===0?'selected':''}>&gt; </option>
                <option value="1" ${cond.cmp===1?'selected':''}>&lt; </option>
                <option value="2" ${cond.cmp===2?'selected':''}>= </option>
              </select>
              <input id="thresh_${sIdx}" class="input sm" style="flex:1.4" type="number" step="any" value="${cond.threshold}">
            </div>
          `}
        </div>`;
      });
      content += '</div>';
    }
  }

  /* ================= STEP 3 ================= */
   else if(stepNum === 3) {

     if(wizard.data.type === 2) {
       content = `<div class="wizard-step">
         <h3>${t('wiz.dates')}</h3>
         <div class="field">
           <label for="date_start">${t('wiz.from')}</label>
           <input id="date_start" type="date" value="${wizard.data.date_start}">
         </div>
         <div class="field">
           <label for="date_end">${t('wiz.to')}</label>
           <input id="date_end" type="date" value="${wizard.data.date_end}">
         </div>
         <div class="field">
           <label>${t('wiz.runAt')}</label>
           <div class="field-row">
<input id="time_hour" class="input sm input-full" type="number" min="0" max="23" value="${wizard.data.time_hour}" placeholder="Hs">
            <input id="time_minute" class="input sm input-full" type="number" min="0" max="59" value="${wizard.data.time_minute}" placeholder="Min">
           </div>
         </div>
       </div>`;
     } else if(wizard.data.sensors.length > 1) {
       content = `<div class="wizard-step">
         <h3>${t('wiz.logic')}</h3>
         <label class="wizard-option"><input type="radio" name="logic" value="1" ${wizard.data.logic===1?'checked':''}><div><strong>${t('wiz.all')}</strong></div></label>
         <label class="wizard-option"><input type="radio" name="logic" value="0" ${wizard.data.logic===0?'checked':''}><div><strong>${t('wiz.any')}</strong></div></label>
       </div>`;
     }
   }

  /* ================= STEP 4 ================= */
  else if(stepNum === 4) {
    content = `<div class="wizard-step">
      <h3>${t('wiz.actuators')}</h3>
      <select id="actuatorList" multiple size="5" class="input"></select>
      <span class="wizard-hint">${t('wiz.hint')}</span>
    </div>`;
  }

  /* ================= STEP 5 ================= */
  else if(stepNum === 5) {

    if(wizard.data.actuators.length === 0){
      content = `<h3>${t('wiz.noActuators')}</h3>`;
    } else {

      content = `<div class="wizard-step"><h3>${t('wiz.actions')}</h3>`;

      wizard.data.actuators.forEach((aIdx,i)=>{
        const a = sensorByIndex(aIdx);
        if(!a) return;
        const action = wizard.data.actions[i] || 0;
        const level = wizard.data.levels[i] || 0;

        const state = (a.type === 9)
          ? (a.state ? 'ON' : 'OFF')
          : (a.type === 8)
            ? `${a.value}%`
            : '-';

        content += `
        <div class="action-box">
          <div class="rule-info"><b>${a.name}</b><span>${state}</span></div>
          <div class="field-row">
            <select id="action_${i}" class="input sm input-full">
              <option value="0" ${action===0?'selected':''}>ON</option>
              <option value="1" ${action===1?'selected':''}>OFF</option>
              <option value="2" ${action===2?'selected':''}>TOGGLE</option>
              ${a.type===8?`<option value="3" ${action===3?'selected':''}>LEVEL</option>`:''}
            </select>
            ${a.type===8?`<input id="level_${i}" class="input sm input-full" type="number" min="0" max="100" value="${level}"${action===3?'':` class="d-none"`}>`
:''}
          </div>
        </div>`;
      });

      setTimeout(() => setupActionListeners(), 0);
      content += '</div>';
    }
  }

  /* ================= STEP 6 ================= */
  else if(stepNum === 6) {

    content = `<div class="wizard-step">`;

    if(wizard.data.type === 3){
      content += `<div class="field">
        <label for="interval">${t('wiz.intervalMs')}</label>
        <input id="interval" class="input" type="number" value="${wizard.data.interval||1000}">
      </div>`;
    }

    content += `
      <div class="field">
        <label for="delay">${t('wiz.delayMs')}</label>
        <input id="delay" class="input" type="number" value="${wizard.data.delay}">
      </div>
      <div class="field">
        <label for="cooldown">${t('wiz.cooldownMs')}</label>
        <input id="cooldown" class="input" type="number" value="${wizard.data.cooldown}">
      </div>
    </div>`;
  }

  /* ================= RENDER ================= */

  let html = `<div>${content}</div>`;
   html += `
  <div class="wizard-nav">
    ${n>0?'<button class="btn ghost" onclick="prevStep()">' + t('wiz.back') + '</button>':''}
    ${n<steps.length-1?'<button class="btn primary" onclick="nextStep()">' + t('wiz.next') + '</button>':'<button class="btn primary" onclick="finishWizard()">' + t('wiz.save') + '</button>'}
  </div>`;

  document.getElementById('wizardContent').innerHTML = html;

  if(stepNum===1) populateSensors();
  if(stepNum===4) populateActuators();
}

function setupActionListeners(){
  wizard.data.actuators.forEach((aIdx, aPos) => {
    const actuator = sensorByIndex(aIdx);
    if(!actuator) return;
    const isDimmer = actuator.type === 8;

    if(isDimmer) {
      const actionSelect = document.getElementById(`action_${aPos}`);
      const levelInput = document.getElementById(`level_${aPos}`);

      if(actionSelect) {
        const updateLevelVisibility = () => {
          if(levelInput) {
            levelInput.style.display = actionSelect.value === '3' ? 'inline-block' : 'none';
          }
        };
        updateLevelVisibility();
        actionSelect.addEventListener('change', updateLevelVisibility);
      }
    }
  });
}

function validateStep(stepNum) {
  if(stepNum === 0) {
    const typeRadio = document.querySelector('input[name="type"]:checked');
    if(!typeRadio) {
      showToast('error', t('wiz.selectType'));
      return false;
    }
    return true;
  }

  if(stepNum === 1) {
    if((wizard.data.type === 0 || wizard.data.type === 1) && wizard.data.sensors.length === 0) {
      showToast('error', t('wiz.needSensor'));
      return false;
    }
    return true;
  }

  if(stepNum === 2) {
    if(wizard.data.type === 0 || wizard.data.type === 1) {
      for(let sIdx of wizard.data.sensors) {
        const cmpSelect = document.getElementById(`cmp_${sIdx}`);
        const threshInput = document.getElementById(`thresh_${sIdx}`);

        if(wizard.data.type === 1) { // THRESHOLD
          if(!threshInput || threshInput.value === '') {
            showToast('error', t('wiz.needThreshold'));
            return false;
          }
          const threshVal = parseFloat(threshInput.value);
          if(isNaN(threshVal) || threshVal < -1000 || threshVal > 10000) {
            showToast('error', t('wiz.threshRange'));
            return false;
          }
        }
      }
    }
    return true;
  }

  if(stepNum === 3) {
    if(wizard.data.type === 2) { // TIME
      const dateStartEl = document.getElementById('date_start');
      const dateEndEl = document.getElementById('date_end');
      const timeHourEl = document.getElementById('time_hour');
      const timeMinEl = document.getElementById('time_minute');

      const hour = parseInt(timeHourEl.value) || 0;
      const min = parseInt(timeMinEl.value) || 0;

      if(hour < 0 || hour > 23 || min < 0 || min > 59) {
        showToast('error', t('wiz.timeRange'));
        return false;
      }

      if(dateStartEl.value && dateEndEl.value) {
        const start = new Date(dateStartEl.value);
        const end = new Date(dateEndEl.value);
        if(start > end) {
          showToast('error', t('wiz.dateOrder'));
          return false;
        }
      }
    }
    return true;
  }

  if(stepNum === 4) {
    if(wizard.data.actuators.length === 0) {
      showToast('error', t('wiz.needActuator'));
      return false;
    }
    return true;
  }

  if(stepNum === 5) {
    for(let aPos = 0; aPos < wizard.data.actuators.length; aPos++) {
      const actionSelect = document.getElementById(`action_${aPos}`);
      const levelInput = document.getElementById(`level_${aPos}`);

      if(!actionSelect) {
        showToast('error', t('wiz.loadActions'));
        return false;
      }

      const action = parseInt(actionSelect.value);
      const aIdx = wizard.data.actuators[aPos];
      const actuator = sensorByIndex(aIdx);
      if(!actuator) {
        showToast('error', t('wiz.loadActuator'));
        return false;
      }

      if(action === 3 && actuator.type !== 8) {
        showToast('error', t('wiz.levelDimmerOnly'));
        return false;
      }

      if(action === 3 && levelInput) {
        const level = parseInt(levelInput.value);
        if(isNaN(level) || level < 0 || level > 100) {
          showToast('error', t('wiz.levelRange'));
          return false;
        }
      }
    }
    return true;
  }

  if(stepNum === 6) {
    const delayEl = document.getElementById('delay');
    const cooldownEl = document.getElementById('cooldown');
    const intervalEl = document.getElementById('interval');

    const delay = parseInt(delayEl.value) || 0;
    const cooldown = parseInt(cooldownEl.value) || 0;

    if(delay < 0 || delay > 60000) {
      showToast('error', t('wiz.delayRange'));
      return false;
    }

    if(cooldown < 0 || cooldown > 3600000) {
      showToast('error', t('wiz.cooldownRange'));
      return false;
    }

    if(wizard.data.type === 3) {
      const interval = parseInt(intervalEl.value) || 0;
      if(interval < 1000 || interval > 3600000) {
        showToast('error', t('wiz.intervalRange'));
        return false;
      }
    }

    return true;
  }

  return true;
}

function nextStep(){
  const steps = getRelevantSteps();
  const stepNum = steps[wizard.step];

  if(stepNum === 1){
    wizard.data.sensors = [...document.querySelectorAll('#sensorList option:checked')]
      .map(o => parseInt(o.value));
  }

  if(stepNum === 4){
    wizard.data.actuators = [...document.querySelectorAll('#actuatorList option:checked')]
      .map(o => parseInt(o.value));
  }

  if(!validateStep(stepNum)) {
    return;
  }

  if(stepNum === 0) {
    const typeRadio = document.querySelector('input[name="type"]:checked');
    if(typeRadio) wizard.data.type = parseInt(typeRadio.value);
  }
  else if(stepNum === 1) {
    wizard.data.sensors = [...document.querySelectorAll('#sensorList option:checked')].map(o=>parseInt(o.value));
  }
  else if(stepNum === 2) {
    wizard.data.sensors.forEach(sIdx => {
      const cmpSelect = document.getElementById(`cmp_${sIdx}`);
      const threshInput = document.getElementById(`thresh_${sIdx}`);
      if(cmpSelect){
        wizard.data.conditions[sIdx] = {
          cmp: parseInt(cmpSelect.value),
          threshold: threshInput ? parseFloat(threshInput.value) || 0 : 0
        };
      }
    });
  }
  else if(stepNum === 3) {
    if(wizard.data.type === 2) {
      wizard.data.date_start = document.getElementById('date_start').value || '';
      wizard.data.date_end = document.getElementById('date_end').value || '';
      wizard.data.time_hour = parseInt(document.getElementById('time_hour').value) || 0;
      wizard.data.time_minute = parseInt(document.getElementById('time_minute').value) || 0;
    } else if(wizard.data.sensors.length > 1) {
      const logicRadio = document.querySelector('input[name="logic"]:checked');
      if(logicRadio) wizard.data.logic = parseInt(logicRadio.value);
    }
  }
  else if(stepNum === 4) {
    const newActuators = [...document.querySelectorAll('#actuatorList option:checked')].map(o=>parseInt(o.value));

    if(JSON.stringify(newActuators) !== JSON.stringify(wizard.data.actuators)) {
      wizard.data.actuators = newActuators;
      wizard.data.actions = wizard.data.actuators.map(() => 0);
      wizard.data.levels = wizard.data.actuators.map(() => 0);
    } else {
      wizard.data.actuators = newActuators;
    }
  }
  else if(stepNum === 5) {
    wizard.data.actions = [];
    wizard.data.levels = [];

    wizard.data.actuators.forEach((aIdx, aPos) => {
      const actionSelect = document.getElementById(`action_${aPos}`);
      const levelInput = document.getElementById(`level_${aPos}`);

      const action = actionSelect ? parseInt(actionSelect.value) : 0;
      let level = 0;

      if(levelInput) {
        const levelValue = levelInput.value;
        level = levelValue && levelValue.trim() !== '' ? parseInt(levelValue) : 0;
      }

      wizard.data.actions[aPos] = action;
      wizard.data.levels[aPos] = level;
    });
  }
  else if(stepNum === 6) {
    wizard.data.delay = parseInt(document.getElementById('delay').value) || 0;
    wizard.data.cooldown = parseInt(document.getElementById('cooldown').value) || 0;
    if(wizard.data.type === 3) {
      wizard.data.interval = parseInt(document.getElementById('interval').value) || 1000;
    }
  }

  showStep(wizard.step + 1);
}

function prevStep(){
  if(wizard.step > 0) showStep(wizard.step - 1);
}

function populateSensors(){
  const sel = document.getElementById('sensorList');

  let filtered = sensors;

  if(wizard.data.type === 0) {
    filtered = sensors.filter((s,i) => [7, 6, 9, 12].includes(s.type));
  } else if(wizard.data.type === 1) {
    filtered = sensors.filter((s,i) => [1, 2, 3, 4, 5, 11].includes(s.type));
  }

  sel.innerHTML = filtered.map((s,i)=>{
    const origIdx = s.index;
    return `<option value="${origIdx}">[${origIdx}] ${s.name}</option>`;
  }).join('');

  if(wizard.data.sensors && wizard.data.sensors.length > 0) {
    document.querySelectorAll('#sensorList option').forEach(o=>{
      if(wizard.data.sensors.includes(parseInt(o.value))) o.selected=true;
    });
  }
}

function populateActuators(){
  const sel = document.getElementById('actuatorList');
  const actuators = sensors.reduce((acc,s,i)=>{
    if(s.type===9 || s.type===8) acc.push({...s, idx:s.index});
    return acc;
  },[]);

  sel.innerHTML = actuators.map(s=>`<option value="${s.idx}">[${s.idx}] ${s.name}</option>`).join('');

  if(wizard.data.actuators && wizard.data.actuators.length > 0) {
    document.querySelectorAll('#actuatorList option').forEach(o=>{
      if(wizard.data.actuators.includes(parseInt(o.value))) o.selected=true;
    });
  }
}

async function finishWizard(){
  const steps = getRelevantSteps();
  const stepNum = steps[wizard.step];

  if(wizard.data.type === 2 && stepNum === 3) {
    wizard.data.date_start = document.getElementById('date_start').value || '';
    wizard.data.date_end = document.getElementById('date_end').value || '';
    wizard.data.time_hour = parseInt(document.getElementById('time_hour').value) || 0;
    wizard.data.time_minute = parseInt(document.getElementById('time_minute').value) || 0;
  }
  else if(stepNum === 5) {
    wizard.data.actions = [];
    wizard.data.levels = [];

    wizard.data.actuators.forEach((aIdx, aPos) => {
      const actionSelect = document.getElementById(`action_${aPos}`);
      const levelInput = document.getElementById(`level_${aPos}`);

      let action = 0;
      if(actionSelect) {
        action = parseInt(actionSelect.value);
      }

      let level = 0;
      if(levelInput && levelInput.style.display !== 'none') {
        const levelValue = levelInput.value;
        level = levelValue && levelValue.trim() !== '' ? parseInt(levelValue) : 0;
      }

      wizard.data.actions.push(action);
      wizard.data.levels.push(level);
    });
  }
  else if(stepNum === 6) {
    wizard.data.delay = parseInt(document.getElementById('delay').value) || 0;
    wizard.data.cooldown = parseInt(document.getElementById('cooldown').value) || 0;
    if(wizard.data.type === 3) {
      wizard.data.interval = parseInt(document.getElementById('interval').value) || 1000;
    }
  }

  // ========== VALIDACIONES FRONTEND ==========

  if(wizard.data.actuators.length === 0) {
    showToast('error', t('wiz.needActuator'));
    return;
  }

  if((wizard.data.type === 0 || wizard.data.type === 1) && wizard.data.sensors.length === 0) {
    showToast('error', t('wiz.needSensor'));
    return;
  }

  if(wizard.data.type === 2) {
    if(wizard.data.time_hour < 0 || wizard.data.time_hour > 23) {
      showToast('error', t('wiz.hourRange'));
      return;
    }
    if(wizard.data.time_minute < 0 || wizard.data.time_minute > 59) {
      showToast('error', t('wiz.minRange'));
      return;
    }
  }

  if(wizard.data.type === 2) {
    if(wizard.data.date_start && wizard.data.date_end) {
      const dateStart = new Date(wizard.data.date_start);
      const dateEnd = new Date(wizard.data.date_end);
      if(dateStart > dateEnd) {
        showToast('error', t('wiz.dateOrder'));
        return;
      }
    }
  }

  if(wizard.data.delay < 0 || wizard.data.delay > 60000) {
    showToast('error', t('wiz.delayRange'));
    return;
  }

  if(wizard.data.cooldown < 0 || wizard.data.cooldown > 3600000) {
    showToast('error', t('wiz.cooldownRange'));
    return;
  }

  if(wizard.data.type === 3) {
    if(wizard.data.interval < 1000 || wizard.data.interval > 3600000) {
      showToast('error', t('wiz.intervalRange'));
      return;
    }
  }

  let wizardValid = true;
  wizard.data.actuators.forEach((aIdx, aPos) => {
    if(!wizardValid) return;
    const level = wizard.data.levels[aPos] || 0;
    if(level < 0 || level > 100) {
      showToast('error', tf('wiz.levelActuator', { level }));
      wizardValid = false;
    }
  });

  if(wizardValid) {
    wizard.data.actuators.forEach((aIdx, aPos) => {
      if(!wizardValid) return;
      const action = wizard.data.actions[aPos];
      const actuator = sensorByIndex(aIdx);
      if(!actuator) return;
      if(action === 3 && actuator.type !== 8) {
        showToast('error', t('wiz.levelDimmerOnly'));
        wizardValid = false;
      }
    });
  }

  if(!wizardValid) return;

  let cmps = [], thresholds = [];
  wizard.data.sensors.forEach(sIdx => {
    const cond = wizard.data.conditions[sIdx] || {cmp: 0, threshold: 0};
    cmps.push(cond.cmp);
    thresholds.push(cond.threshold);
  });

  while(wizard.data.levels.length < wizard.data.actuators.length) {
    wizard.data.levels.push(0);
  }

  let time_s = wizard.data.type === 2 ? wizard.data.time_hour * 3600 + wizard.data.time_minute * 60 : 0;

  let year_start = 0, month_start = 0, day_start = 0;
  let year_end = 0, month_end = 0, day_end = 0;

  if(wizard.data.type === 2) {
    if(wizard.data.date_start) {
      const [y, m, d] = wizard.data.date_start.split('-');
      year_start = parseInt(y);
      month_start = parseInt(m);
      day_start = parseInt(d);
    }
    if(wizard.data.date_end) {
      const [y, m, d] = wizard.data.date_end.split('-');
      year_end = parseInt(y);
      month_end = parseInt(m);
      day_end = parseInt(d);
    }
  }

  const params = new URLSearchParams();

  params.append('id', wizard.data.id ?? -1);
  params.append('sensors', wizard.data.sensors.join(','));
  params.append('actuators', wizard.data.actuators.join(','));
  params.append('type', wizard.data.type);
  params.append('logic', wizard.data.logic);
  params.append('delay', wizard.data.delay);
  params.append('cooldown', wizard.data.cooldown);
  params.append('interval', wizard.data.interval || 0);
  params.append('actions', wizard.data.actions.join(','));
  params.append('levels', wizard.data.levels.join(','));
  params.append('cmp', cmps.join(','));
  params.append('threshold', thresholds.join(','));
  params.append('time_s', time_s);
  params.append('year_start', year_start);
  params.append('month_start', month_start);
  params.append('day_start', day_start);
  params.append('year_end', year_end);
  params.append('month_end', month_end);
  params.append('day_end', day_end);

  try {
    const res = await fetch('/rules/set', {
      method:'POST',
      headers:{'Content-Type':'application/x-www-form-urlencoded'},
      body:params.toString()
    });

    if(res.ok) {
      showToast('success', t('wiz.saved'));
      closeRule();
      loadRules();
    } else {
      const errMsg = await res.text();
      showToast('error', t('wiz.saveError') + ' ' + errMsg);
    }
  } catch(e) {
    showToast('error', t('wiz.connError') + ' ' + e.message);
  }
}

function editRule(i){
  loadSensorsAndActuators().then(()=>{
    document.getElementById('ruleModalTitle').textContent=t('wiz.titleEdit');
    startWizard(i);
    document.getElementById('ruleModal').classList.add('open');
  });
}

function closeRule(){
  const modal = document.getElementById('ruleModal');
  if (modal) modal.classList.remove('open');
}

async function deleteRule(i){
  if(!confirm(tf('wiz.deleteConfirm', { id: i }))) return;
  await fetch('/rules/delete',{
    method:'POST',
    headers:{'Content-Type':'application/x-www-form-urlencoded'},
    body:`id=${i}`
  });
  loadRules();
}

async function loadRules(){
  try {
    const res = await fetch('/rules');
    if(!res.ok) return;
    const rules = await res.json();
    window.rules = rules;

    const table = document.getElementById("auto_table");
    const empty = document.getElementById("auto_empty");

    if(!rules || rules.length === 0){
      table.innerHTML = '';
      if(empty) empty.style.display = 'flex';
      return;
    }
    if(empty) empty.style.display = 'none';

    const typeName = ['EDGE','THRESHOLD','TIME','INTERVAL'];
    const actionName = a => ['ON','OFF','TOGGLE','LEVEL'][a] != null ? ['ON','OFF','TOGGLE','LEVEL'][a] : a;
    const fmtActions = r => (r.actuators || []).map((a, idx) => {
      const act = actionName(r.actions && r.actions[idx]);
      return '#' + a + ': ' + act + (act === 'LEVEL' && (r.levels && r.levels[idx] != null) ? ' ' + r.levels[idx] + '%' : '');
    }).join(', ') || '-';
    let html = '<div class="rule-list">' + rules.map(r=>`
      <div class="rule-card">
        <div class="rule-head">
          <span class="chip neutral">${typeName[r.type] || 'RULE'} #${r.id}</span>
          <div class="rule-actions">
            <button class="btn ghost sm" onclick="editRule(${r.id})">${t('wiz.edit')}</button>
            <button class="btn danger sm" onclick="deleteRule(${r.id})">${t('wiz.delete')}</button>
          </div>
        </div>
        <div class="rule-info"><span>${t('rule.sensors')}</span><b>${(r.sensors || []).join(", ") || '-'}</b></div>
        <div class="rule-info"><span>${t('rule.logic')}</span><b>${r.logical_and ? 'AND' : 'OR'}</b></div>
        <div class="rule-info"><span>${t('rule.actions')}</span><b>${fmtActions(r)}</b></div>
        <div class="rule-info"><span>${t('rule.delay')}</span><b>${r.delay_ms} / ${r.cooldown_ms} ms</b></div>
      </div>`).join('') + '</div>';
    table.innerHTML = html;
  } catch(e) {
    console.log('loadRules error', e);
  }
}

loadRules();
loadDevices();
loadCalib();
let pollIntervalRef = setInterval(() => {
  if(document.visibilityState !== 'hidden') {
    const tab = window.activeTab;
    if (tab === 'config') {
      updateSettingsValues();
    } else if (tab === 'control') {
      loadDevices();
    }
  }
}, 5000);

(function modalInit(){
  const modal = document.getElementById('ruleModal');
  if(!modal) return;
  modal.addEventListener('click', e => {
    if(e.target === modal) closeRule();
  });
  document.addEventListener('keydown', e => {
    if(e.key === 'Escape' && modal.classList.contains('open')) closeRule();
  });
})

document.addEventListener('visibilitychange', () => {
  if (document.visibilityState === 'hidden') {
    stopLogAutoRefresh();
  } else if (window.activeTab === 'logs') {
    startLogAutoRefresh();
  }
});

function filterDevices() {
  const term = document.getElementById('devSearch')?.value?.toLowerCase() || '';
  devSearchTerm = term;
  applyDeviceFilter(term);
}

function applyDeviceFilter(term) {
  const cards = document.querySelectorAll('.device-card');
  let visibleCount = 0;
  cards.forEach(card => {
    const name = card.querySelector('.device-name')?.textContent?.toLowerCase() || '';
    const type = card.querySelector('.chip')?.textContent?.toLowerCase() || '';
    const show = name.includes(term) || type.includes(term);
    card.style.display = show ? '' : 'none';
    if (show) visibleCount++;
  });
  // Update empty state - scoped to devices section
  const devicesSection = document.getElementById('devices_cards');
  if (visibleCount === 0 && term) {
    if (!devicesSection.querySelector('.empty.sm')) {
      const div = document.createElement('div');
      div.className = 'empty sm';
      div.textContent = t('dev.noMatch');
      devicesSection.prepend(div);
    }
  } else {
    const existing = devicesSection.querySelector('.empty.sm');
    if (existing) existing.remove();
  }
}

function filterSettings() {
  const term = document.getElementById('settingsSearch')?.value?.toLowerCase() || '';
  settingsSearchTerm = term;
  applySettingsFilter(term);
}

function applySettingsFilter(term) {
  const cards = document.querySelectorAll('.settings-card');
  let visibleCount = 0;
  cards.forEach(card => {
    const name = card.querySelector('.settings-card-head h3')?.textContent?.toLowerCase() || '';
    const show = name.includes(term);
    card.style.display = show ? '' : 'none';
    if (show) visibleCount++;
  });
  // Update empty state - scoped to settings section
  const settingsSection = document.getElementById('cards');
  if (visibleCount === 0 && term) {
    if (!settingsSection.querySelector('.empty.sm')) {
      const div = document.createElement('div');
      div.className = 'empty sm';
      div.textContent = t('cfg.noMatch');
      settingsSection.prepend(div);
    }
  } else {
    const existing = settingsSection.querySelector('.empty.sm');
    if (existing) existing.remove();
  }
}

(function languageSelector() {
/* -------------------- LANGUAGE SELECTOR -------------------- */

function setLang(l) {
  LANG = (l === 'en') ? 'en' : 'es';
  try { localStorage.setItem('lang', LANG); } catch(e) {}
  document.querySelectorAll('[data-i18n]').forEach(el => {
    el.textContent = t(el.getAttribute('data-i18n'));
  });
  document.querySelectorAll('[data-i18n-ph]').forEach(el => {
    el.placeholder = t(el.getAttribute('data-i18n-ph')) + '...';
  });
document.querySelectorAll('.langbtn').forEach(btn => {
    btn.classList.toggle('active', btn.dataset.lang === LANG);
  });
  const tab = window.activeTab || 'control';
  if (tab === 'control') loadDevices();
  else if (tab === 'auto') loadRules();
  else if (tab === 'config') { loadCalib(); syncOtaCheckbox(); }
  else if (tab === 'logs') refreshLogs();
}

document.querySelectorAll('.langbtn').forEach(btn => {
  btn.addEventListener('click', () => setLang(btn.dataset.lang || 'es'));
});

setLang(LANG);
})(); /* -------------------- LANGUAGE SELECTOR -------------------- */
// Initialize active tab once, after all scripts are parsed (single init flow)
(() => {
  const valid = ['control', 'auto', 'config', 'logs'];
  const tab = valid.indexOf(window.startupTab) !== -1
    ? window.startupTab
    : (valid.indexOf(localStorage.getItem('tab')) !== -1
      ? localStorage.getItem('tab')
      : 'control');
  show(tab);
})();
</script>
<div id="ruleModal" class="modal" role="dialog" aria-modal="true" aria-labelledby="ruleModalTitle">
  <div class="modal-content">
    <div class="modal-head">
      <h2 id="ruleModalTitle" data-i18n="wiz.titleNew">Nueva regla de automatización</h2>
      <button type="button" class="icon-btn" onclick="closeRule()" aria-label="Cerrar">×</button>
    </div>
    <div id="wizardContent"></div>
    <div class="modal-foot">
      <button class="btn ghost sm" onclick="closeRule()" data-i18n="wiz.cancel">Cancelar</button>
    </div>
  </div>
</div>
</body>
</html>
)rawliteral";


}