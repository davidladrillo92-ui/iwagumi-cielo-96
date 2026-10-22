#include <Arduino.h>
#include "cielo_ui.h"
const char CIELO_UI[] PROGMEM = R"CIELOUI_96(<!DOCTYPE html>
<html lang="es"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<meta name="theme-color" content="#101213"><meta name="apple-mobile-web-app-capable" content="yes"><meta name="apple-mobile-web-app-status-bar-style" content="black-translucent"><meta name="apple-mobile-web-app-title" content="Cielo"><meta name="description" content="IWAGUMI CIELO · Iluminación del acuario">
<link rel="manifest" href="/manifest.webmanifest"><link rel="apple-touch-icon" sizes="180x180" href="/icon-180.png"><link rel="icon" type="image/svg+xml" href="/icon.svg">
<title>CIELO 11 · Iwagumi en vivo</title>
<style>
:root{color-scheme:dark;--bg:#0c0d0e;--panel:#191b1e;--panel2:#222529;--line:#ffffff16;--text:#f4f2ee;--soft:#a8a9a7;--dim:#777a7d;--warm:#e7c4a7;--mint:#c7d6d0;font-family:-apple-system,BlinkMacSystemFont,'SF Pro Display',system-ui,Arial,sans-serif;font-synthesis:none}
*{box-sizing:border-box}html{height:100%;-webkit-tap-highlight-color:transparent}body{background:#050607;color:var(--text);margin:0;min-height:100%;-webkit-font-smoothing:antialiased}button,input{font:inherit}button{cursor:pointer;touch-action:manipulation}button:disabled{opacity:.36;cursor:default}button:focus-visible,input:focus-visible{outline:2px solid var(--warm);outline-offset:3px}.shell{position:relative;overflow:hidden;max-width:480px;margin:auto;background:var(--bg);min-height:100dvh;box-shadow:0 0 80px #0008;padding:calc(env(safe-area-inset-top) + 19px) 23px calc(112px + env(safe-area-inset-bottom))}
.top{display:flex;align-items:center;justify-content:space-between;margin-bottom:19px}.brand{font-size:14px;letter-spacing:.22em;font-weight:650;color:#f5f3ee}.iconbtn{width:43px;height:43px;display:grid;place-items:center;border-radius:50%;background:#ffffff08;border:1px solid #ffffff0e;color:var(--text)}.iconbtn svg{height:20px;width:20px}.heading{display:flex;justify-content:space-between;align-items:end;gap:12px;margin-bottom:12px}.eyebrow{font-size:11px;letter-spacing:.08em;color:var(--soft);font-weight:500}.h1{font-size:39px;letter-spacing:-.062em;font-weight:580;line-height:1.1;margin:5px 0 0}.status{display:inline-flex;align-items:center;gap:7px;font-size:11px;color:#ced7d0;padding-bottom:6px;white-space:nowrap}.status i{width:6px;height:6px;background:#91c2a4;border-radius:50%;box-shadow:0 0 12px #91c2a466}.status.offline{color:#aaa2a0}.status.offline i{background:#a1928a;box-shadow:none}
.hero{position:relative;width:100%;height:248px;border-radius:23px;overflow:hidden;background:#172126;border:1px solid #ffffff12;isolation:isolate;box-shadow:inset 0 1px #ffffff20}.hero-art{position:absolute;inset:0;width:100%;height:100%}.hero:after{content:'';position:absolute;inset:0;background:linear-gradient(180deg,transparent 38%,#06090b10 60%,#02060893 100%);pointer-events:none}.glow{position:absolute;inset:-40% -60%;background:radial-gradient(ellipse,var(--sky,#d9a382) 0%,transparent 58%);opacity:.28;filter:blur(32px);mix-blend-mode:screen;transition:background 1s ease}.hero-caption{position:absolute;z-index:2;bottom:22px;left:21px;right:21px;display:flex;justify-content:space-between;align-items:end}.hero-caption strong{font-size:21px;letter-spacing:-.045em;font-weight:560}.hero-caption span{display:block;margin-top:4px;color:#e5e8e5bd;font-size:11px}.hero-day{font-size:12px;font-variant-numeric:tabular-nums;color:#f9f4ee;opacity:.94;background:#ffffff18;border:1px solid #ffffff20;padding:8px 10px;border-radius:30px;backdrop-filter:blur(9px)}
.sectionhead{display:flex;align-items:center;justify-content:space-between;margin:17px 1px 9px}.sectionhead h2{margin:0;font-size:18px;font-weight:590;letter-spacing:-.035em}.minor{border:0;background:none;color:#b6b9b9;font-size:12px;padding:9px 0}.lamps{border-radius:22px;background:#1a1d20;border:1px solid #ffffff0e;overflow:hidden}.lamp{width:100%;padding:12px 17px 12px;display:flex;align-items:center;text-align:left;gap:13px;border:0;color:var(--text);background:none}.lamp+.lamp{border-top:1px solid #ffffff0e}.lamp:active{background:#ffffff09}.lamp-icon{width:41px;height:41px;display:grid;place-items:center;background:#292c30;border:1px solid #ffffff0b;border-radius:15px;flex-shrink:0}.lamp-icon svg{width:20px;height:20px;opacity:.86}.lamp-copy{flex:1;min-width:0}.lamp-copy strong{font-size:15px;font-weight:600;letter-spacing:-.017em}.lamp-copy small{display:block;margin-top:4px;color:#969c9f;font-size:12px}.lamp-side{text-align:right}.lamp-side b{font-size:22px;font-variant-numeric:tabular-nums;font-weight:500;letter-spacing:-.045em}.lamp-side small{display:block;color:#82888b;font-size:10px;margin-top:3px}.chevron{color:#60686e;margin-left:3px}
.scenes{display:grid;grid-template-columns:repeat(4,1fr);gap:8px;margin-top:4px}.scene{padding:0;border:0;background:none;color:#b7b6b6;font-size:11px;text-align:center}.orb{margin:0 auto 9px;width:min(57px,18vw);aspect-ratio:1;border-radius:50%;display:block;box-shadow:inset 0 2px 9px #ffffff55,inset 0 -8px 18px #0008,0 6px 13px #0008}.scene.active .orb{outline:1px solid #ede4d8;outline-offset:4px}.scene:nth-child(1) .orb{background:radial-gradient(circle at 26% 17%,#f5dedb,#b97886 55%,#643f61)}.scene:nth-child(2) .orb{background:radial-gradient(circle at 26% 17%,#fff4dc,#dfdfc7 54%,#879b9c)}.scene:nth-child(3) .orb{background:radial-gradient(circle at 26% 17%,#f7c588,#bf5a37 64%,#72354c)}.scene:nth-child(4) .orb{background:radial-gradient(circle at 25% 20%,#687fbd,#1a355d 55%,#131c31)}
.bottom{position:fixed;bottom:0;left:50%;transform:translateX(-50%);z-index:30;width:min(480px,100%);height:calc(81px + env(safe-area-inset-bottom));padding:10px 48px calc(7px + env(safe-area-inset-bottom));display:flex;justify-content:center;gap:42px;align-items:flex-start;background:linear-gradient(180deg,#141619f8,#151719);border-top:1px solid #ffffff12;backdrop-filter:blur(20px);-webkit-backdrop-filter:blur(20px)}.tab{display:grid;place-items:center;gap:6px;min-width:72px;border:0;color:#73787b;background:none;font-size:11px}.tab svg{width:21px;height:21px}.tab.active{color:#ece6df}.page{display:none}.page.active{display:block}.quiet{font-size:12px;color:#899094;line-height:1.5}.plot{width:100%;height:195px;display:block;overflow:visible}.plot-wrap{background:#191b1e;border:1px solid var(--line);padding:22px 13px 10px;border-radius:22px}.seg{display:flex;gap:3px;border-radius:12px;background:#25282c;padding:4px}.seg button{flex:1;min-width:0;border:0;background:none;border-radius:10px;color:#848b8f;font-size:12px;font-weight:550;padding:10px 4px}.seg button.active{background:#414348;color:#fff;box-shadow:0 2px 9px #0003}.plot-foot{display:flex;justify-content:space-between;color:#73797d;font-size:10px;margin:2px 5px 13px}.time-control{display:flex;align-items:center;justify-content:space-between;margin:23px 0 12px}.time-control strong{font-size:29px;font-weight:500;font-variant-numeric:tabular-nums;letter-spacing:-.04em}.time-control small{color:#a0a4a5;font-size:12px}input[type=range]{width:100%;accent-color:#dcc4ac;cursor:pointer}input[type=number]{color:var(--text);background:#26292e;border:1px solid #414448;border-radius:12px;width:100%;min-width:0;font-size:20px;font-variant-numeric:tabular-nums;padding:12px 5px;text-align:center}.pills{display:flex;gap:7px;flex-wrap:wrap}.pill{border:1px solid #ffffff1e;background:#26292c;color:#dfdedd;border-radius:99px;padding:10px 13px;font-size:12px}.pill.active{color:#191c1d;background:#ece5dd}.readout{margin-top:13px;display:flex;justify-content:space-between;align-items:center;color:#c8cbc9;font-size:12px}.btn{padding:14px 19px;min-height:48px;border-radius:14px;background:#eae6df;color:#171a1c;border:0;font-size:14px;font-weight:620}.btn.sub{background:#2a2e32;color:#ededed}.btn.danger{background:#2e2727;color:#efbeb6}.btn:disabled{opacity:.38}.btn.wide{width:100%}.actions{display:flex;gap:9px}.actions>*{flex:1}.panel{border-radius:21px;background:#1a1d20;border:1px solid var(--line);padding:17px}.note{font-size:11px;color:#82888b;line-height:1.45}.sheet-bg{position:fixed;z-index:50;inset:0;background:#000a;display:none;align-items:end;justify-content:center;backdrop-filter:blur(7px)}.sheet-bg.open{display:flex}.sheet{background:#202226;border-radius:28px 28px 0 0;width:min(480px,100%);max-height:calc(94dvh - env(safe-area-inset-top));padding:11px 22px calc(24px + env(safe-area-inset-bottom));overflow-y:auto;box-shadow:0 -20px 80px #0007;animation:up .21s ease}@keyframes up{from{transform:translateY(14px);opacity:.8}to{transform:translateY(0);opacity:1}}.handle{width:39px;height:5px;border-radius:30px;background:#74777a6b;margin:0 auto 23px}.sheet-title{font-size:26px;letter-spacing:-.05em;font-weight:600;margin:0}.sheet-sub{font-size:12px;color:#9c9da1;margin:6px 0 26px}.fields{display:grid;grid-template-columns:repeat(4,minmax(0,1fr));gap:9px;margin:18px 0 23px}.fields.three{grid-template-columns:repeat(3,minmax(0,1fr))}.field{min-width:0}.field label{display:block;color:#898d90;font-size:12px;margin:0 0 8px;text-align:center}.settings-row{padding:15px 1px;border-bottom:1px solid #ffffff13;display:flex;justify-content:space-between;align-items:center;gap:13px;color:#dfdfdf;font-size:14px}.settings-row small{font-size:12px;color:#888d91}.settings-row button{background:none;border:0;color:#dbd3c9;font-size:13px}.helpchip{font-size:12px;color:#adadae;background:#24272a;padding:12px 14px;border-radius:12px;line-height:1.45}.toast{position:fixed;z-index:90;bottom:calc(94px + env(safe-area-inset-bottom));left:50%;transform:translate(-50%,20px);opacity:0;transition:.25s;pointer-events:none;background:#383c40;color:#fff;box-shadow:0 6px 40px #0009;border-radius:16px;padding:12px 17px;font-size:12px;max-width:calc(100% - 45px);width:max-content;text-align:center}.toast.show{opacity:1;transform:translate(-50%,0)}.sr-only{position:absolute;width:1px;height:1px;overflow:hidden;clip:rect(0 0 0 0)}
@media(min-width:700px){.shell{margin:35px auto;min-height:850px;border-radius:26px}.bottom{bottom:0}}@media(max-width:360px){.shell{padding-left:16px;padding-right:16px}.hero{height:267px}.scenes{gap:4px}.fields{gap:4px}}
.wifiForm{display:flex;flex-direction:column;gap:13px;margin:18px 0}.wifiForm label{font-size:12px;color:var(--soft);display:flex;flex-direction:column;gap:7px}.wifiForm input{height:47px;border-radius:12px;border:1px solid var(--line);background:var(--panel2);color:var(--text);padding:0 14px;font-size:16px}.wifiPrimary,.wifiSecondary{width:100%;min-height:48px;border-radius:13px;border:1px solid var(--line);font-size:15px}.wifiPrimary{background:var(--text);color:var(--bg)}.wifiSecondary{background:var(--panel);color:var(--soft)}
.quietHead{display:flex;justify-content:flex-end;margin:0 0 11px}.information{cursor:default}.information .lamp-side small{display:flex;gap:7px;align-items:center;justify-content:flex-end}.color-dot{display:inline-block;width:9px;height:9px;border-radius:99px;border:1px solid #ffffff33;box-shadow:0 0 8px #ffffff1c}.homefoot{text-align:center;letter-spacing:.30em;color:#848885;font-size:9px;margin:26px 0 0}.editorActions{display:flex;gap:7px;flex-wrap:wrap;margin:11px 0}.editorActions button{background:var(--panel2);border:1px solid var(--line);border-radius:11px;color:var(--text);font-size:12px;padding:11px 13px}.minorActions button{background:transparent;color:var(--soft)}.curveHelper{font-size:12px;color:var(--dim);margin:7px 0 13px;min-height:16px}.pointSelect{display:flex;align-items:end;gap:10px;margin:18px 0}.pointSelect label{flex:1;font-size:12px;color:var(--soft)}.pointSelect select{width:100%;display:block;margin-top:7px;border:1px solid var(--line);background:var(--panel2);color:var(--text);height:45px;padding:0 8px;border-radius:11px}.pointSelect button{background:var(--panel2);border:1px solid var(--line);border-radius:11px;color:var(--text);height:45px;padding:0 14px}
 .home-lamps{margin-top:17px}
.pointTime{margin:17px 0 8px}.pointTime label{display:block;color:var(--soft);font-size:12px;margin-bottom:6px}.pointTime input{width:100%;max-width:100%;min-width:0;height:46px;background:var(--panel2);color:var(--text);border:1px solid var(--line);border-radius:12px;font-size:19px;letter-spacing:.03em;padding:0 13px}.pointTime+.fields{margin-top:16px}
.timeGrid{display:grid;grid-template-columns:1fr 1fr;gap:12px;margin:18px 0}
.timeGrid label{display:block;font-size:12px;color:#a6a8aa}
.timeGrid select{display:block;width:100%;min-height:52px;margin-top:8px;padding:12px 13px;border:1px solid #ffffff28;border-radius:13px;background:#2a2d31;color:#f6f3ef;font-size:18px;appearance:auto}
.editorHint{font-size:12px;line-height:1.5;color:#c4c3c1;background:#2c2e31;border-radius:14px;padding:12px 14px;margin-bottom:16px}
.livePanel{margin:17px 0;background:#212429;border:1px solid #ffffff18;border-radius:20px;padding:17px}
.livePanel strong{font-size:16px}.livePanel p{font-size:12px;color:#9da3a5;line-height:1.55}
.liveButtons{display:grid;grid-template-columns:1fr 1fr;gap:9px;margin-top:14px}
.liveButtons button{min-height:45px;border-radius:12px;border:1px solid #ffffff2b;background:#eae6df;color:#202124;font-size:13px;font-weight:600}
.liveButtons button:last-child{background:#303337;color:#eae6df}
.bottom{gap:9px;padding-left:9px;padding-right:9px}.tab{min-width:64px}
.labText{white-space:pre-wrap;line-height:1.58;font-size:12px;color:#d6d8d5;font-variant-numeric:tabular-nums}
.labDanger{border-left:3px solid #e6bb94;padding:11px 14px;background:#a87c3622;border-radius:8px;margin:10px 0}
.labButtons{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:8px;margin-top:12px}
.labButtons button{border:1px solid #ffffff1c;background:#2c3034;color:#e9e5df;border-radius:11px;min-height:46px;padding:9px;font-size:12px}
.labButtons button.danger{border-color:#cc927a60;color:#f2cbb7}
.hero-tint{position:absolute;inset:0;z-index:1;background:var(--ambient,#445a69);opacity:.18;mix-blend-mode:screen;pointer-events:none;transition:background 1.5s ease,opacity 1.5s ease}
.conn-tick{font-style:normal;display:inline-flex;align-items:center;justify-content:center;width:17px;height:17px;border-radius:50%;font-size:12px;color:#aeb5ad;background:#ffffff17;margin-left:5px;vertical-align:middle}
.conn-tick.ok{color:#112719;background:#8ddeaa;box-shadow:0 0 9px #56bd7775}
.conn-tick.wait{color:#eacda2;background:#94714350}
.lamp.information .lamp-copy strong{display:inline-flex;align-items:center}
#momentSub:empty{display:none}
.chart-legend{display:flex;flex-wrap:wrap;gap:7px 15px;color:#bdc5c4;font-size:12px;padding:9px 0 5px}
.chart-legend span{display:inline-flex;align-items:center;gap:6px}
.chart-legend i{width:11px;height:3px;display:inline-block;border-radius:3px}
.chart-inspector{font-size:12px;line-height:1.55;color:#dce2dc;background:#ffffff0b;border:1px solid #ffffff14;border-radius:12px;padding:9px 12px;margin:8px 0 6px}
.health-head{display:flex;justify-content:space-between;align-items:start;gap:14px}
.health-head strong{font-size:17px;letter-spacing:-.025em}
.health-head-note{font-size:11px;color:#8d9697;margin:5px 0}
.health-indicator{width:11px;height:11px;flex:none;border-radius:50%;background:#72797f;box-shadow:0 0 0 4px #ffffff08;margin-top:4px}
.health-indicator.good{background:#8bd9a9;box-shadow:0 0 0 4px #8bd9a921}
.health-indicator.warning{background:#e9b980;box-shadow:0 0 0 4px #e9b98022}
.health-indicator.error{background:#e58c86;box-shadow:0 0 0 4px #e58c8622}
.health-title{font-size:24px;font-weight:580;letter-spacing:-.045em;line-height:1.14;margin:18px 0 7px}
.health-explain{font-size:13px;color:#b7bebb;line-height:1.48;margin:0 0 17px}
.health-grid{display:grid;grid-template-columns:1fr 1fr;gap:9px}
.health-light{background:#ffffff09;border:1px solid #ffffff12;border-radius:15px;padding:13px 11px;min-width:0}
.health-light span,.health-stats span{display:block;font-size:11px;color:#929c9d}
.health-light strong{display:block;font-size:14px;margin-top:8px;line-height:1.3;font-weight:580}
.health-light small{display:block;font-size:11px;color:#a8afad;margin-top:5px;line-height:1.4}
.health-stats{display:grid;grid-template-columns:1fr 1fr;gap:13px 8px;padding:17px 3px 14px}
.health-stats strong{display:block;font-size:16px;font-weight:550;margin-top:5px;font-variant-numeric:tabular-nums}
.health-advice{font-size:12px;color:#d4d9d4;background:#ffffff0b;border-radius:13px;padding:12px 13px;line-height:1.6;margin:4px 0 12px}
.health-events{display:grid;gap:9px;margin-top:13px}
.health-event{border-bottom:1px solid #ffffff16;padding-bottom:8px;font-size:12px;line-height:1.4;color:#c4cbc8}
.health-event strong{display:block;color:#e5e8e5;margin-bottom:4px;font-size:12px}
.health-buttons{margin-top:18px}
.health-secondary{margin-top:8px}
.health-secondary button{background:#ffffff0d;color:#ccd1cd}
@media(max-width:360px){.health-title{font-size:21px}.health-light{padding:11px 8px}}

/* RC5 B6+B7: interaccion tactil, visualizacion honesta y VoiceOver. */
.confirmed-times{display:flex;flex-direction:column;gap:5px;background:#ffffff08;border-radius:12px;padding:10px 12px;margin-top:12px;font-size:11px;color:#c2cfca;font-variant-numeric:tabular-nums}
:root{--focus:#f6caa2}
button,select,input[type=checkbox]{touch-action:manipulation}
button{min-height:44px}
button:focus-visible,select:focus-visible,input:focus-visible,summary:focus-visible,svg:focus-visible{outline:3px solid var(--focus);outline-offset:3px}
.access-pill{margin-left:auto;margin-right:10px;border:1px solid #b5b9b950;color:#e0e7e0;background:#283130;border-radius:18px;padding:5px 11px;font-size:11px;min-height:36px}
.access-pill.admin{border-color:#8dc39a;color:#cef3d4}
.measurement-disclaimer{margin-top:12px;color:#aeb7b7;font-size:11px;line-height:1.55}
.chart-tools{display:flex;align-items:center;justify-content:space-between;gap:8px;flex-wrap:wrap;margin:-6px 0 7px}
.zoom-control{display:flex;gap:3px;background:#272b2e;border:1px solid #ffffff17;border-radius:11px;padding:3px;flex:1;max-width:290px}
.zoom-control button{min-width:45px;min-height:40px;flex:1;border:0;border-radius:8px;background:none;color:#c5cccd;font-size:13px;font-weight:600}
.zoom-control button.active{background:#d9c7b4;color:#11191b}
.compare-control,.chart-point-actions button{border:1px solid #ffffff32;background:#303638;color:#eaf1ee;border-radius:10px;padding:9px 11px;font-size:12px}
.compare-control[aria-pressed=true]{background:#314d49;border-color:#89c8b9}
.chart-zoom-note{color:#b7c0c0;font-size:11px;line-height:1.4;margin:2px 0 6px}
.chart-point-actions{display:flex;gap:8px;align-items:center;flex-wrap:wrap;padding:9px 0 2px}
.chart-point-actions span{font-size:11px;color:#a7b3b6;flex:1;min-width:115px;line-height:1.5}
.chart-point-actions button:disabled{opacity:.5}
.color-preview{display:flex;align-items:center;gap:9px;padding-top:8px;flex-wrap:wrap}
.color-preview-swatch{width:34px;height:27px;border:1px solid #ffffff30;border-radius:9px;display:inline-block;box-shadow:inset 0 0 7px #0006}
.color-preview p{font-size:11px;color:#afbaba;line-height:1.4;flex:1;min-width:160px;margin:0}
.pending-state{border:1px solid #ffffff25;background:#ffffff08;border-radius:12px;padding:10px 12px;font-size:12px;line-height:1.5;color:#dee4e1}
.pending-state.unsaved{border-color:#e4bd7d80;background:#a87c3820;color:#ffe2b2}
.access-summary{display:flex;gap:10px;align-items:center;justify-content:space-between;background:#20272a;border:1px solid #ffffff23;border-radius:16px;padding:14px;margin:11px 0 16px}
.access-summary strong{font-size:14px}.access-summary p{font-size:12px;line-height:1.45;color:#bac5c3;margin:5px 0 0}
.access-summary button{flex-shrink:0;border:1px solid #ffffff4d;border-radius:10px;background:#31433d;color:#e9eee9;padding:8px 12px}
.sheet-close{float:right;border:1px solid #ffffff25;border-radius:50%;background:#ffffff12;color:#ecefed;width:44px;height:44px;margin:-4px 0 7px 8px}
.chart-point{cursor:pointer;stroke:#111819;stroke-width:1.5;touch-action:manipulation}
.chart-point.selected{stroke:#fff7df;stroke-width:2.7}
@media(max-width:380px){.shell{padding-left:15px;padding-right:15px}.bottom{padding-left:17px;padding-right:17px;gap:12px;justify-content:space-around}.tab{min-width:67px}.plot-wrap{padding-left:10px;padding-right:10px}.chart-tools{gap:5px}.zoom-control button{min-width:37px}.access-summary{align-items:flex-start}}
@media(prefers-reduced-motion:reduce){*,*::before,*::after{transition-duration:.01ms!important;animation-duration:.01ms!important}}

.pointNudge{display:flex;align-items:center;gap:10px;margin:10px 0;flex-wrap:wrap;font-size:12px;color:#c8cdcc}
.pointNudge button{border:1px solid #ffffff44;background:#293036;color:#f3eee7;border-radius:9px;min-width:90px;padding:9px}
#modal .sheet{max-height:90dvh;overflow-y:auto;overscroll-behavior:contain}
#modal .sheet input,#modal .sheet select{min-height:44px}
/* SIMPLE1: intrinsic row height, bounded columns and short labels on iPhone. */
.top .iconbtn{margin-left:auto}
.home-lamps .lamp{display:grid;grid-template-columns:36px minmax(0,1fr) minmax(70px,95px);gap:10px;padding:17px 14px}
.home-lamps .lamp-icon{width:36px;height:36px;border-radius:12px}
.home-lamps .lamp-copy,.home-lamps .lamp-side{min-width:0}
.home-lamps .lamp-copy strong{display:flex;gap:3px;align-items:center;font-size:15px;line-height:1.35}
.home-lamps .lamp-copy small{font-size:12px;line-height:1.5;overflow-wrap:anywhere}
.home-lamps .lamp-side b{font-size:25px;line-height:1.3}
.home-lamps .lamp-side small{font-size:10px;line-height:1.5;white-space:nowrap}
.home-lamps .conn-tick{flex:none;margin-left:2px}
#chiInfo,#pezInfo{min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.report-action{grid-template-columns:1fr}
.report-action button:last-child{background:#eae6df;color:#202124}
@media(max-width:340px){.home-lamps .lamp{grid-template-columns:28px minmax(0,1fr) 78px;gap:7px;padding:15px 10px}.home-lamps .lamp-icon{width:28px;height:32px}.home-lamps .lamp-copy strong{font-size:14px}.home-lamps .lamp-side small{font-size:9px}}
</style>
</head><body>
<div class="shell"><header class="top"><div class="brand">C I E L O</div><button id="openSettings" class="iconbtn" aria-label="Ajustes"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7"><circle cx="12" cy="12" r="3"/><path d="M12 2v2m0 16v2M2 12h2m16 0h2M4.9 4.9l1.5 1.5m11.2 11.2 1.5 1.5M19.1 4.9l-1.5 1.5M6.4 17.6l-1.5 1.5"/></svg></button></header>
<main id="viewLight" class="page active"><div class="quietHead"><div class="status offline" id="status"><i></i><span id="statusText">Vista previa</span></div></div>
<section class="hero" aria-label="Paisaje del acuario"><div class="glow" id="glow"></div><svg class="hero-art" viewBox="0 0 360 306" preserveAspectRatio="xMidYMid slice" aria-hidden="true"><defs><linearGradient id="sea" x1="0" x2="0" y1="0" y2="1"><stop stop-color="#1f2d38"/><stop offset=".53" stop-color="#5c6974"/><stop offset="1" stop-color="#12232b"/></linearGradient><linearGradient id="stone" x1="0" x2="1"><stop stop-color="#1e2c30"/><stop offset=".56" stop-color="#657473"/><stop offset="1" stop-color="#222e32"/></linearGradient><linearGradient id="stone2" x1="0" x2="1"><stop stop-color="#334a4b"/><stop offset=".5" stop-color="#85918d"/><stop offset="1" stop-color="#2a393c"/></linearGradient><linearGradient id="floor" x1="0" x2="0" y1="0" y2="1"><stop stop-color="#254f40"/><stop offset="1" stop-color="#142e2a"/></linearGradient><filter id="blur"><feGaussianBlur stdDeviation="15"/></filter></defs><rect width="360" height="306" fill="url(#sea)"/><ellipse cx="206" cy="102" rx="154" ry="63" fill="#f0d3a0" opacity=".12" filter="url(#blur)"/><path d="M0 225C79 212 119 218 173 219c80-11 128-7 187 3v87H0Z" fill="url(#floor)"/><path d="M-20 250C40 231 67 240 101 226l38-80 18 71 37-81 39 97 48-32 58 50 42-4v65H-20Z" fill="#162b2d" opacity=".44"/><path d="M55 229l25-74 27-48 22 49-6 67z" fill="url(#stone)"/><path d="M81 165l26-58 22 49-16-9Z" fill="#c2b8ac" opacity=".30"/><path d="M116 232l26-107 27-52 25 88-3 69Z" fill="url(#stone2)"/><path d="M142 125l27-52 25 88-16-25-18 8Z" fill="#d6d5c4" opacity=".35"/><path d="M172 229l38-87 19 40 20 53Z" fill="url(#stone)"/><path d="M210 142l19 40-17-10Z" fill="#c5cfc2" opacity=".32"/><path d="M247 238l37-62 32 64Z" fill="url(#stone2)"/><path d="M0 235q35-13 73-5t68 7q68-5 93-6t126 11v65H0Z" fill="#2b593f"/><path d="M0 243Q68 230 127 247Q150 254 173 253Q187 249 212 245Q263 237 360 252v55H0Z" fill="#376746" opacity=".8"/><path d="M160 308Q158 280 185 253Q198 246 210 245Q199 265 218 307Z" fill="#c3c3a6"/><path d="M168 307Q167 280 188 261Q195 255 202 254Q192 282 204 307Z" fill="#e1d9bf" opacity=".84"/><g fill="none" stroke="#9aba8b" stroke-width="2.3" opacity=".42"><path d="M3 245q40-17 70-9t46 11M219 251q72-18 142 1M20 263q59-20 106-5M225 266q79-9 137 7"/></g><rect width="360" height="306" fill="url(#sea)" opacity=".05"/></svg><div class="hero-tint" id="heroTint"></div>
<div class="hero-caption"><div><strong id="moment">En calma</strong><span id="momentSub"></span></div><div class="hero-day" id="heroTime">22:20</div></div></section>
<section class="lamps home-lamps">
<div class="lamp information"><span class="lamp-icon"><svg viewBox="0 0 24 24" fill="none" stroke="#d6cbbf" stroke-width="1.6"><path d="M4 16h16M8 7l4-4 4 4M12 3v11m-8 5h16"/></svg></span><span class="lamp-copy"><strong>Chihiros <i class="conn-tick" id="chiLed" title="Sin respuesta BLE reciente">·</i></strong><small id="chiSub">AUTO</small></span><span class="lamp-side"><b id="chiPct">—</b><small><i class="color-dot" id="chiColor"></i><span id="chiInfo">Según programa</span></small></span></div>
<div class="lamp information"><span class="lamp-icon"><svg viewBox="0 0 24 24" fill="none" stroke="#b5c5d4" stroke-width="1.5"><circle cx="12" cy="12" r="7"/><path d="M6 15c4-3 7-3 12-6M5 11c5-3 8-2 14-2"/></svg></span><span class="lamp-copy"><strong>Pez Verde <i class="conn-tick" id="pezLed" title="Sin respuesta BLE reciente">·</i></strong><small id="pezSub">AUTO</small></span><span class="lamp-side"><b id="pezPct">—</b><small><i class="color-dot" id="pezColor"></i><span id="pezInfo">Según programa</span></small></span></div>
</section></main>
<main id="viewCurves" class="page"><div class="heading"><div><div class="eyebrow">24 HORAS</div><h1 class="h1">Curvas.</h1></div></div><div class="seg" id="curveSeg"><button data-curve="both" class="active">Juntas</button><button data-curve="chi">Chihiros</button><button data-curve="pez">Pez Verde</button></div><div class="plot-wrap" style="margin-top:15px"><div class="chart-tools"><div class="zoom-control" role="group" aria-label="Escala de tiempo del gráfico"><button data-zoom="1">1 h</button><button data-zoom="3">3 h</button><button data-zoom="6">6 h</button><button data-zoom="24" class="active" aria-pressed="true">24 h</button></div><button id="compareCurves" type="button" aria-pressed="false" class="compare-control">Comparar</button></div><p id="chartZoomNote" class="chart-zoom-note">Vista de 24 horas · pulsa un punto para inspeccionarlo</p><svg id="chart" class="plot" viewBox="0 0 338 195" aria-label="Curvas de iluminación: horas y porcentajes" role="img" tabindex="0"></svg><div id="chartLegend" class="chart-legend" aria-label="Leyenda de las curvas"></div><div id="chartInspector" class="chart-inspector" aria-live="polite">Selecciona una hora para consultar las dos luces.</div><div class="color-preview"><span id="colorPreviewPv" class="color-preview-swatch" role="img" aria-label="Color calculado de Pez Verde"></span><span id="colorPreviewChi" class="color-preview-swatch" role="img" aria-label="Color calculado de Chihiros"></span><p id="previewDisclaimer">Vista calculada: respeta el brillo de Pez Verde; no equivale a intensidad ni color ópticos medidos.</p></div><div class="plot-foot" id="chartAxisTimes"><span>00:00</span><span>06:00</span><span>12:00</span><span>18:00</span><span>24:00</span></div><div class="chart-point-actions"><button id="editSelectedPoint" type="button">Editar punto seleccionado</button><span id="selectedPointNote" aria-live="polite">Pulsa un círculo para escoger un punto.</span></div></div><div class="time-control"><strong id="scrubTime">22:20</strong><small id="scrubScene">Simulación de la curva</small></div><input id="scrub" type="range" min="0" max="1440" value="1340" step="1" aria-label="Hora de simulación"><div class="sectionhead"><h2>Programación</h2><button id="resetTime" class="minor">Volver a ahora</button></div><p class="note" id="scrubHint">Mover la hora cambia la vista previa de Luz, no envía órdenes a las pantallas.</p><p id="curvePending" class="pending-state" role="status" aria-live="polite">Sin cambios pendientes de guardar</p>
<div class="editorActions" role="group" aria-label="Editar y guardar curvas">
<button id="editCurve">Editar puntos</button><button id="undoCurve">Deshacer</button><button id="redoCurve">Rehacer</button><button id="saveCurve">Guardar</button><button id="loadProfile">Importar</button><button id="exportProfile">Exportar</button>
</div>
<details class="curveHelper" id="curvesAdvanced">
<summary style="cursor:pointer">Opciones avanzadas de curvas</summary>
<div class="editorActions minorActions"><button id="resetCurve">Restaurar originales</button></div>
</details>

<input type="file" accept=".json,application/json" id="importFile" hidden>
<div class="curveHelper" id="editMessage">Selecciona Chihiros o Pez Verde para editar.</div>
<div class="lamps"><button class="lamp" data-lamp="chi"><span class="lamp-icon">☀</span><span class="lamp-copy"><strong>Chihiros</strong><small id="curveChiRgb">R 0 · G 0 · B 0</small></span><span class="lamp-side"><b id="curveChiPct">0%</b></span><span class="chevron">›</span></button><button class="lamp" data-lamp="pez"><span class="lamp-icon">◐</span><span class="lamp-copy"><strong>Pez Verde</strong><small id="curvePezRgb">R 0 · G 0 · B 0</small></span><span class="lamp-side"><b id="curvePezPct">0%</b></span><span class="chevron">›</span></button></div></main>
<main id="viewLab" class="page">
<div class="heading"><div><h1 class="h1">Centro.</h1></div></div>
<section class="livePanel" id="cielo95Diagnostics" aria-label="Estado de las pantallas">
 <div class="health-head"><strong>Estado de CIELO</strong><span class="health-indicator" id="diagIndicator" aria-hidden="true"></span></div>
 <p class="health-title" id="diagState" role="status" aria-live="polite">Comprobando…</p>
 <div class="health-grid">
  <div class="health-light"><span>Pez Verde</span><strong id="diagPvStatus">Esperando</strong></div>
  <div class="health-light"><span>Chihiros</span><strong id="diagChStatus">Esperando</strong></div>
 </div>
 <div class="liveButtons report-action"><button id="diagDownload">Descargar informe</button></div>
</section>
<details class="livePanel"><summary><strong>Actualizaciones</strong></summary>
<section class="livePanel" id="legacyOta942">
<strong>Instalar actualización</strong>
<p id="otaInstalledVersion">Versión instalada: consultando...</p>



<label style="display:block;margin:14px 0 4px" for="otaFile">Archivo (.bin)</label>
<input id="otaFile" type="file" accept=".bin,application/octet-stream" style="width:100%;color:#e7ded4">
<div class="liveButtons" style="margin-top:12px"><button id="otaInstall" disabled>Instalar BIN</button></div>
<progress id="otaProgress" max="100" value="0" hidden style="width:100%;accent-color:#96d9ad;margin:12px 0 7px"></progress>
<p id="otaState">Comprobando actualización local...</p>

</section>
</details>
</main>
<nav class="bottom" aria-label="Secciones"><button class="tab active" data-view="light"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7"><path d="M6 17a9 9 0 1 1 12 0M9 20h6m-5 3h4"/></svg>Luz</button><button class="tab" data-view="curves"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7"><path d="M2 18L8 12l5 3 9-10M2 22h20"/></svg>Curvas</button><button class="tab" data-view="lab"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7"><path d="M9 3v6l-5 9a2 2 0 0 0 2 3h12a2 2 0 0 0 2-3l-5-9V3M8 3h8M7 15h10"/></svg>Centro</button></nav></div>
<div class="sheet-bg" id="modal"><section class="sheet" aria-modal="true" role="dialog" aria-label="Panel de CIELO" tabindex="-1"><div class="handle"></div><button id="sheetClose" type="button" class="sheet-close" aria-label="Cerrar panel">✕</button><div id="modalBody"></div></section></div><div id="toast" class="toast" role="status" aria-live="polite"></div>
<script>
'use strict';
const $=s=>document.getElementById(s),clamp=(v,a,b)=>Math.min(b,Math.max(a,v)),pad=n=>String(n).padStart(2,'0'),fmt=t=>(t===1440?'24:00':pad(Math.floor(t/60)%24)+':'+pad(Math.floor(t%60)));
const CHI=[[870,0,0,0],[875,1,1,1],[880,2,2,2],[890,7,7,7],[900,18,18,18],[910,30,29,28],[920,44,42,40],[930,57,54,51],[940,70,65,60],[1270,70,65,60],[1290,68,64,60],[1310,63,60,57],[1330,54,53,52],[1340,47,47,46],[1350,40,40,40],[1360,29,29,29],[1365,22,22,22],[1370,15,15,15],[1375,7,7,7],[1380,0,0,0],[1440,0,0,0]];
const PEZ=[[870,210,0,145,0],[875,225,130,145,1],[885,240,135,145,4],[895,250,130,120,9],[905,255,130,110,15],[915,255,145,110,22],[925,255,174,145,25],[935,240,238,230,20],[940,240,238,230,20],[1270,240,238,230,20],[1285,255,218,145,30],[1300,255,164,31,38],[1315,255,104,3,46],[1325,255,85,0,50],[1335,255,49,3,46],[1345,255,17,20,40],[1355,225,6,30,28],[1365,205,18,47,22],[1370,198,26,57,19],[1375,194,35,66,16],[1380,190,45,75,14],[1385,187,55,85,12],[1390,184,70,95,10],[1395,178,87,109,8],[1405,151,128,154,4],[1430,130,156,187,1],[1440,130,156,187,0]];
const state={chi:CHI.map(p=>p.slice()),pez:PEZ.map(p=>p.slice()),connected:false,status:null,view:'light',curve:'both',scrub:1340,preview:null,busy:false,updated:0,dirty:{chi:false,pez:false},profileGeneration:0,undoStack:[],redoStack:[],editWhich:'pez',editIndex:0,zoomHours:24,compare:false,baseline:null,selectedPoint:null};
let toastTimer;
function toast(s){$('toast').textContent=s;$('toast').classList.add('show');clearTimeout(toastTimer);toastTimer=setTimeout(()=>$('toast').classList.remove('show'),3600)}
function vAt(points,t){if(t<points[0][0]||t>=1440)return points[0].slice(1).map(()=>0);for(let i=0;i<points.length-1;i++){let a=points[i],b=points[i+1];if(t>b[0])continue;let f=(t-a[0])/(b[0]-a[0]);return a.slice(1).map((v,j)=>Math.round(v+(b[j+1]-v)*f))}return points.at(-1).slice(1)}
function scene(t){if(t<870)return ['Descanso nocturno','Luces en reposo'];if(t<935)return ['Amanecer','La luz despierta'];if(t<1270)return ['Luz natural','Pleno día'];if(t<1320)return ['Luz dorada','Tarde'];if(t<1380)return ['Atardecer','Última luz'];if(t<1440)return ['Descanso nocturno','Luces en reposo'];return ['Descanso nocturno','Luces en reposo']}
function rgb(r,g,b){return '#'+[r,g,b].map(x=>clamp(x,0,255).toString(16).padStart(2,'0')).join('')}
function nowMin(){let date=new Date();return date.getHours()*60+date.getMinutes()}
function currentClock(){
 // El ATOM es la referencia. Entre lecturas, su hora avanza localmente.
 const match=state.status?.clock?.match(/(\d\d):(\d\d):(\d\d)/);
 if(!match||!state.updated)return nowMin();
 const seconds=Number(match[1])*3600+Number(match[2])*60+Number(match[3]);
 const elapsed=Math.max(0,Math.floor((Date.now()-state.updated)/1000));
 return Math.floor(((seconds+elapsed)%86400)/60);
}
function isLive(){return (location.protocol==='https:'||location.protocol==='http:')&&location.hostname!=='localhost'&&location.hostname!=='127.0.0.1'||location.search.includes('live=1')}
// Local access: no login, stored key or session token.
try{localStorage.removeItem('cieloOtaKey')}catch(_){}
async function api(path,data){
 if(!isLive())throw Error('Vista previa, abre Cielo desde el ATOM');
 const opt={cache:'no-store',credentials:'same-origin'};
 if(data){opt.method='POST';opt.headers={'Content-Type':'application/x-www-form-urlencoded'};opt.body=new URLSearchParams(data);}
 const controller=globalThis.AbortController?new AbortController():null;
 const timer=controller?setTimeout(()=>controller.abort(),5000):null;
 if(controller)opt.signal=controller.signal;
 let r,body;try{r=await fetch(path,opt);body=await r.text()}finally{if(timer)clearTimeout(timer)}
 if(!r.ok)throw Error(body||('Error '+r.status));
 try{return JSON.parse(body)}catch{return body}
}
async function sync(){if(!isLive())return;try{const d=await api('/api/status');if(!d||!Array.isArray(d.rgb)||typeof d.ble!=='boolean')throw Error('Respuesta incompatible');state.connected=true;state.status=d;state.updated=Date.now();if(!state.dirty.pez&&!state.dirty.chi&&Number.isInteger(d.profileGeneration))state.profileGeneration=d.profileGeneration;setCadenceLabel(d.cadence);if(d.points&&!state.dirty.pez){let p=d.points.split(';').map(t=>t.split(',').map(Number));if(p.length>=2&&p.every((v,i)=>v.length===5&&v.every(Number.isFinite)&&(!i||v[0]>p[i-1][0])))state.pez=p;}if(d.chiPoints&&!state.dirty.chi){let p=d.chiPoints.split(';').map(t=>t.split(',').map(Number));if(p.length>=2&&p.every((v,i)=>v.length===4&&v.every(Number.isFinite)&&(!i||v[0]>p[i-1][0])))state.chi=p;}if(!state.dirty.pez&&!state.dirty.chi){state.baseline={pez:state.pez.map(p=>p.slice()),chi:state.chi.map(p=>p.slice())};}render()}catch(e){state.connected=false;render()}}
function hourLabel(t){return fmt(clamp(t,0,1440))}
function render(){
 const d=state.status,v4=d?.cieloV4||{},now=currentClock();
 const preview=state.preview!==null,when=preview?state.preview:now;
 const fresh=state.connected&&Date.now()-state.updated<20000;
 const chiPlan=vAt(state.chi,when),pvPlan=vAt(state.pez,when);
 const chiRecent=fresh&&!preview&&v4.chihiros_active&&v4.ch_online_recent&&Array.isArray(v4.ch_last_requested);
 const pvRecent=fresh&&!preview&&v4.pez_verde_active&&v4.pv_online_recent&&Array.isArray(v4.pv_last_requested);
 const chi=chiRecent?v4.ch_last_requested:chiPlan;
 const pez=pvRecent?v4.pv_last_requested:pvPlan;
 const moment=scene(when);
 $('status').classList.toggle('offline',!fresh);
 $('statusText').textContent=!fresh?'Sin conexión':preview?'Simulación':'ATOM conectado';
 $('moment').textContent=moment[0];
 $('momentSub').textContent=preview?'Vista previa · '+hourLabel(when):'';
 $('heroTime').textContent=hourLabel(when);
 const chiIsKnown=preview||!!(state.connected&&chiRecent);
 const pvIsKnown=preview||!!(state.connected&&pvRecent);
 const chiRgb=(chiIsKnown?chi:[0,0,0]).map(v=>Math.round(v*2.55));
 const pvRgb=pvIsKnown?pez.slice(0,3):[0,0,0];
 const dimmedPv=pvRgb.map(v=>Math.round(v*(pez[3]||0)/100));
 const mix=[0,1,2].map(k=>Math.round(.58*dimmedPv[k]+.42*chiRgb[k]));
 const strength=Math.max(...mix);
 const saturation=(Math.max(...pvRgb)-Math.min(...pvRgb))/255;
 const pezWeight=.52+.32*saturation;
 // Atardeceres cálidos: preservar el tono real de Pez Verde, sin grisarlo
 // por la mezcla con Chihiros. Si ambas están a cero, el fondo queda oscuro.
 const ambient=strength<5?'#13232e':
   rgb(...[0,1,2].map(k=>Math.round(dimmedPv[k]*pezWeight+chiRgb[k]*(1-pezWeight))));
 $('glow').style.setProperty('--sky',ambient);
 $('heroTint').style.setProperty('--ambient',ambient);
 $('heroTint').style.opacity=String(strength<5?.04:Math.min(.56,.12+strength/400));
 $('chiPct').textContent=chiIsKnown?Math.max(...chi)+'%':'—';
 $('pezPct').textContent=pvIsKnown?pez[3]+'%':'—';
 $('chiColor').style.background=rgb(...chiRgb);
 $('pezColor').style.background=rgb(...pvRgb);
 $('chiInfo').textContent=chiIsKnown?chi.join(' / '):'—';
 $('pezInfo').textContent=pvIsKnown?pvRgb.join(' / '):'—';
 const lampState=(active,recent,errors,reconnecting=false)=>preview?'Simulación':
  !fresh?'Sin comunicación':reconnecting||errors>0?'Reconectando…':
  recent?'Disponible':active?'Conectando…':v4.always_on?'Preparando…':'En pausa';
 $('chiSub').textContent=lampState(v4.chihiros_active,chiRecent,v4.ch_consecutive_errors||0,v4.ch_reconnect_pending);
 $('pezSub').textContent=lampState(v4.pez_verde_active,pvRecent,Math.max(v4.pv_consecutive_errors||0,v4.pv_off_failures||0));
 for(const [id,up] of [['chiLed',chiRecent&&!v4.ch_reconnect_pending&&!(v4.ch_consecutive_errors>0)],['pezLed',pvRecent&&!(v4.pv_consecutive_errors>0)&&!(v4.pv_off_failures>0)]]){
  const el=$(id);el.className='conn-tick'+(state.connected&&up&&!preview?' ok':' wait');
  el.textContent=state.connected&&up&&!preview?'✓':'·';
  el.title=up?'Respuesta Bluetooth reciente':'Esperando comunicación Bluetooth';
 }
 $('scrubHint').textContent=preview?'Simulación '+hourLabel(when)+' · no cambia las luces.':'Vista del momento actual.';
 const vv=vAt(state.chi,state.scrub),pp=vAt(state.pez,state.scrub);
 $('scrubTime').textContent=hourLabel(state.scrub);
 $('scrubScene').textContent=scene(state.scrub)[0];
 $('curveChiRgb').textContent='R '+vv[0]+' · G '+vv[1]+' · B '+vv[2];
 $('curveChiPct').textContent=Math.max(...vv)+'%';
 $('curvePezRgb').textContent='R '+pp[0]+' · G '+pp[1]+' · B '+pp[2];
 $('curvePezPct').textContent=pp[3]+'%';
 $('editMessage').textContent=state.curve==='both'?'Selecciona una luz. Añade o edita puntos y pulsa Guardar.':state.curve==='chi'?'Chihiros · Al guardar, CIELO aplicará los nuevos puntos durante su control 24 h.':'Pez Verde · Al guardar, CIELO aplicará los nuevos puntos en los siguientes envíos.';
 if($('stormStatus'))$('stormStatus').textContent=d?.storm?'Tormenta en marcha · '+(d.stormLeft||0)+' s restantes':d?.stormStatus||'Lista para iniciar';
 if($('stormMetrics'))$('stormMetrics').textContent='Chihiros · '+(d?.stormChiWrites||0)+' lotes | Pez Verde · '+(d?.stormPezWrites||0)+' lotes | Reconexiones CH · '+(d?.stormChiReconnections||0)+' | Fallos CH/PV · '+(d?.stormChiFailures||0)+'/'+(d?.stormPezFailures||0)+' | Separación BLE máxima CH · '+((d?.stormChiMaxGapMs||0)/1000).toFixed(1)+' s | Relámpagos · '+(d?.stormLightning||0)+'/2';
 if($('stormRestore'))$('stormRestore').textContent=d?.storm?'Prueba en curso; restauraciones aún no necesarias':d?.stormPendingRestore?'Restauración pendiente: Chihiros AUTO '+(d.stormChiAutoSent?'enviado':'pendiente')+' · Pez Verde '+(d.stormPezRestored?'orden normal enviada':'pendiente'):(d?.stormChiAutoSent||d?.stormPezRestored)?'Restauración software: CH AUTO '+(d.stormChiAutoSent?'enviado':'pendiente')+' · PV '+(d.stormPezRestored?'reenviado':'pendiente')+'. Comprueba físicamente ambas luces.':'Último informe disponible en el botón.';
 if($('stormOn'))$('stormOn').disabled=!!d?.storm||!!d?.chContinuous||!!d?.chActive||!state.connected||!d?.clockValid;
 if($('stormOff'))$('stormOff').disabled=!d?.storm;
 if(d?.storm){let flashes=Number(d?.stormLightning||0);if(flashes>lastStormAudio){if(stormSoundOn)stormThunder();lastStormAudio=flashes;}}
 else if(d?.stormLightning===0)lastStormAudio=0;
 if($('chi931Status'))$('chi931Status').textContent=d?.chi931Active?'Curva Aurora 9.3.1 guardada en ATOM · activa CIELO para verla.':'Curva anterior sin sobrescribir; puedes aplicar el nuevo diseño.';
 if($('chi931On'))$('chi931On').disabled=!!d?.chi931Active||!!d?.chContinuous||!!d?.chActive||!state.connected;
 if($('chi931Off'))$('chi931Off').disabled=!d?.chi931Backup||!!d?.chContinuous||!!d?.chActive||!state.connected;
 /* Estado ya disponible en v4. */
 if($('chNightStatus'))$('chNightStatus').textContent=
  v4.return_pending?'Recuperando My Chihiros…':
  v4.chihiros_active&&v4.pez_verde_active?'CIELO controla las dos luces 24 horas':
  v4.pez_verde_active?'Pez Verde en CIELO · Chihiros en My Chihiros':'Control detenido';
 if($('chNightMetrics'))$('chNightMetrics').textContent=
  'Pez Verde '+(v4.pv_gatt_ok||0)+' GATT · Chihiros '+(v4.ch_gatt_ok||0)+
  ' GATT · retornos '+(v4.return_ok||0)+' · errores '+(v4.return_failed||0);
 if($('chNightOn'))$('chNightOn').disabled=!state.connected||!d?.clockValid||!d?.ble||
  (v4.chihiros_active&&v4.pez_verde_active)||v4.return_pending;
 if($('chNightOff'))$('chNightOff').disabled=!state.connected||!!v4.return_pending;
 if($('pez94Status'))$('pez94Status').textContent=d?.pez94Active?'Colores Impacto activos (en este ATOM). Hay copia de seguridad.':'Colores actuales conservados. Activar solo bajo tu confirmación.';
 if($('pez94On'))$('pez94On').disabled=!!d?.pez94Active||!state.connected;
 if($('pez94Off'))$('pez94Off').disabled=!d?.pez94Backup||!state.connected;
 if($('pez93Status'))$('pez93Status').textContent=d?.pez931Active?'Aurora rosada 9.3.1 aplicada · anterior respaldada.':'Curva Pez Verde actual conservada; aplica el nuevo diseño cuando quieras.';
 if($('pez93On'))$('pez93On').disabled=!!d?.pez931Active||!state.connected;
 if($('pez93Off'))$('pez93Off').disabled=!d?.pez931Backup||!state.connected;
 if($('chLiveStatus'))$('chLiveStatus').textContent=d?.chContinuous?'PRUEBA '+(d.chTest||'?')+' en curso · RGB '+(d.chLiveRgb||[]).join('/')+' · '+(d.chLiveSecondsLeft||0)+' s restantes':d?.chActive?'Restaurando AUTO por Bluetooth: espera antes de empezar otra':(d?.chStatus?.includes('AUTO enviado por BLE')?'AUTO transmitido · verifica físicamente la luz':'Listo para la siguiente prueba, solo si la Chihiros está en AUTO');
 if($('chTestHistory'))$('chTestHistory').textContent=Array.isArray(d?.chReports)?d.chReports.map((s,i)=>s?(''+(i+1)+'. '+s):(''+(i+1)+'. Sin probar')).join('\n'):'Sin resultados';
 if($('chTestMetrics'))$('chTestMetrics').textContent=(d?.chLastSummary||'Sin ensayos')+' · Envíos: '+(d?.chTxCount||0)+' · Conexiones: '+(d?.chConnectCount||0)+' · Tramas: '+(d?.chFrameCount||0)+' · Fallos: '+(d?.chTestErrorCount||0)+' · Intervalo último/máximo: '+((d?.chGapMs||0)/1000).toFixed(1)+'/'+((d?.chGapMaxMs||0)/1000).toFixed(1)+' s · Envío último/máximo: '+((d?.chTxMs||0)/1000).toFixed(1)+'/'+((d?.chTxMaxMs||0)/1000).toFixed(1)+' s · Intentos AUTO: '+(d?.chAutoAttempts||0);
 document.querySelectorAll('[data-chi-test]').forEach(b=>b.disabled=!!d?.chContinuous||!!d?.chActive||!state.connected);
 if($('chLiveOff'))$('chLiveOff').disabled=!d?.chContinuous&&!d?.chActive;
 updateEditorStatus();
 drawChart();
}
function chartWindow(hours=state.zoomHours,center=state.scrub){
 const span=Math.min(1440,Math.max(60,Number(hours)*60));
 const begin=clamp(Math.round(center-span/2),0,1440-span);
 return {begin,end:begin+span,span};
}
function selectZoom(hours){
 if(![1,3,6,24].includes(hours))return;
 state.zoomHours=hours;document.querySelectorAll('[data-zoom]').forEach(b=>{
  const active=Number(b.dataset.zoom)===hours;b.classList.toggle('active',active);b.setAttribute('aria-pressed',String(active));
 });render();
}
function updateEditorStatus(){
 const edited=!!(state.dirty.pez||state.dirty.chi),n=$('curvePending');
 if(n){n.classList.toggle('unsaved',edited);n.textContent=edited?'CAMBIOS SIN GUARDAR · '+(state.dirty.pez?'Pez Verde':'')+(state.dirty.pez&&state.dirty.chi?' y ':'')+(state.dirty.chi?'Chihiros':'')+'. No se han enviado a las luces.':'Sin cambios pendientes de guardar en el ATOM.';}
 if($('undoCurve'))$('undoCurve').disabled=!state.undoStack.length;
 if($('redoCurve'))$('redoCurve').disabled=!state.redoStack.length;
 if($('saveCurve'))$('saveCurve').disabled=!edited||!state.connected;
 const sel=state.selectedPoint;
 if(sel&&!state[sel.which]?.[sel.index])state.selectedPoint=null;
 if($('editSelectedPoint'))$('editSelectedPoint').disabled=!sel;
 if($('selectedPointNote'))$('selectedPointNote').textContent=sel?`${sel.which==='chi'?'Chihiros':'Pez Verde'} · punto ${sel.index+1} · ${fmt(state[sel.which][sel.index]?.[0]??0)}`:'Pulsa un círculo para seleccionar un punto.';

}
function chartPointSelect(which,index){
 if(which!=='pez'&&which!=='chi')return;
 if(!Number.isInteger(index)||!state[which][index])return;
 state.selectedPoint={which,index};state.scrub=state[which][index][0];state.preview=state.scrub;
 $('scrub').value=state.scrub;render();
}
function drawChart(){
 const svg=$('chart'),{begin,end,span}=chartWindow();
 const left=29,right=331,top=24,bottom=174;
 const x=t=>left+(t-begin)/span*(right-left);
 const y=pct=>bottom-clamp(pct,0,100)/100*(bottom-top);
 const build=(points,channel,scale)=>{
  let path='',first=true;
  const step=Math.max(1,Math.round(span/180));
  for(let t=begin;t<=end;t=Math.min(end,t+step)){
   const values=vAt(points,t);
   const value=channel<0?Math.max(...values):values[channel]/scale*100;
   path+=(first?'M':'L')+x(t).toFixed(1)+' '+y(value).toFixed(1)+' ';
   first=false;if(t===end)break;
  }
  return path;
 };
 const path=(d,color,width,opacity,dash='')=>
   '<path d="'+d+'" fill="none" stroke="'+color+'" stroke-width="'+width+'" opacity="'+opacity+'" '+(dash?'stroke-dasharray="'+dash+'" ':'')+'stroke-linecap="round" stroke-linejoin="round"/>';
 let content='';
 for(const pct of [0,25,50,75,100]){
  const at=y(pct);
  content+='<path d="M'+left+' '+at+'H'+right+'" stroke="#ffffff20" stroke-dasharray="2 4"/>';
  content+='<text x="'+(left-5)+'" y="'+(at+3)+'" fill="#b6bdbf" font-size="9" text-anchor="end">'+pct+'</text>';
 }
 const legend=[];
 const pvShown=state.curve!=='chi',chShown=state.curve!=='pez';
 if(pvShown){
  if(state.compare&&state.baseline){content+=path(build(state.baseline.pez,3,100),'#e5b5a0',1.6,.43,'5 5');}
  content+=path(build(state.pez,3,100),'#e5b5a0',2.7,1);
  legend.push('<span><i style="background:#e5b5a0"></i>Pez Verde · brillo %</span>');
  if(state.curve==='pez')content+=path(build(state.pez,2,255),'#758ac0',1.4,.68);
 }
 if(chShown){
  if(state.compare&&state.baseline){content+=path(build(state.baseline.chi,-1,100),'#d6ded6',1.6,.42,'5 5');}
  content+=path(build(state.chi,-1,100),'#d6ded6',2.7,1);
  legend.push('<span><i style="background:#d6ded6"></i>Chihiros · intensidad RGB programada %</span>');
  if(state.curve==='chi')content+=path(build(state.chi,2,100),'#7694bc',1.4,.68);
 }
 if(state.compare&&state.baseline)legend.push('<span>Discontinuo: guardado · continuo: edición</span>');
 // Círculos interactivos para selección táctil y ratón. No envían BLE.
 for(const which of ['pez','chi']){
  if((which==='pez'&&!pvShown)||(which==='chi'&&!chShown))continue;
  for(let i=0;i<state[which].length;i++){
   const p=state[which][i];if(p[0]<begin||p[0]>end)continue;
   const val=which==='pez'?p[4]:Math.max(...p.slice(1));
   const selected=state.selectedPoint?.which===which&&state.selectedPoint.index===i;
   const cx=x(p[0]),cy=y(val),color=which==='pez'?'#eac6ae':'#e2efe3';
   content+='<circle class="chart-point'+(selected?' selected':'')+'" cx="'+cx.toFixed(1)+'" cy="'+cy.toFixed(1)+'" r="'+(selected?6.3:4.6)+'" fill="'+color+'" data-which="'+which+'" data-index="'+i+'"/>';
  }
 }
 if(state.scrub>=begin&&state.scrub<=end)content+='<path d="M'+x(state.scrub).toFixed(1)+' '+top+'V'+bottom+'" stroke="#ffffff9e" stroke-width="1" stroke-dasharray="3 4" pointer-events="none"/>';
 svg.innerHTML=content;
 $('chartLegend').innerHTML=legend.join('');
 const labels=[0,.25,.5,.75,1].map(f=>fmt(Math.round(begin+f*span)));
 $('chartAxisTimes').innerHTML=labels.map(t=>'<span>'+t+'</span>').join('');
 $('chartZoomNote').textContent='Zoom '+state.zoomHours+' h · '+fmt(begin)+' — '+fmt(end)+' · puntos visibles seleccionables';
 const pv=vAt(state.pez,state.scrub),ch=vAt(state.chi,state.scrub);
 $('chartInspector').textContent='HORA SIMULADA '+hourLabel(state.scrub)+' · PV RGB '+pv.slice(0,3).join('/')+' · brillo '+pv[3]+' % · CH RGB '+ch.join('/')+' % · Sin órdenes BLE';
 const scaledPv=pv.slice(0,3).map(c=>Math.round(c*pv[3]/100));
 $('colorPreviewPv').style.background=rgb(...scaledPv);
 $('colorPreviewPv').setAttribute('aria-label','Pez Verde: color RGB '+scaledPv.join(', ')+' calculado al brillo '+pv[3]+' %, no medido');
 $('colorPreviewChi').style.background=rgb(...ch.map(v=>Math.round(v*2.55)));
 $('colorPreviewChi').setAttribute('aria-label','Chihiros: RGB porcentual '+ch.join(', ')+' convertido para vista previa, no medido');
}
function nav(n){state.view=n;document.querySelectorAll('.page').forEach(e=>e.classList.toggle('active',e.id==='view'+(n==='light'?'Light':n==='curves'?'Curves':n==='lab'?'Lab':'Service')));document.querySelectorAll('[data-view]').forEach(b=>b.classList.toggle('active',b.dataset.view===n));render();if(n==='lab'){serviceRefresh();}}
let lastModalFocus=null;
function sheet(html){lastModalFocus=document.activeElement;$('modalBody').innerHTML=html;$('modal').classList.add('open');document.body.style.overflow='hidden';$('sheetClose').focus();}
function close(){if($('monitorToken'))$('monitorToken').textContent='';$('modal').classList.remove('open');document.body.style.overflow='';if(lastModalFocus?.focus)lastModalFocus.focus();lastModalFocus=null;}
function dialogKeydown(e){
 if(!$('modal').classList.contains('open')||e.key!=='Tab')return;
 const nodes=Array.from($('modal').querySelectorAll('button:not(:disabled),input:not(:disabled),select:not(:disabled),textarea:not(:disabled),summary,[tabindex="0"]')).filter(el=>el.getClientRects().length);
 if(!nodes.length)return;
 const first=nodes[0],last=nodes.at(-1);
 if(e.shiftKey&&document.activeElement===first){e.preventDefault();last.focus();}
 else if(!e.shiftKey&&document.activeElement===last){e.preventDefault();first.focus();}
}

function input(id,label,value,max){return `<div class="field"><label for="${id}">${label}</label><input type="number" id="${id}" inputmode="numeric" step="1" min="0" max="${max}" value="${value}"></div>`}
let picked='chi';
function lampSheet(which){picked=which;let arr=vAt(state[which],state.preview??currentClock());let isPez=which==='pez';let base=isPez?arr:[...arr];sheet(`<h2 class="sheet-title">${isPez?'Pez Verde':'Chihiros'}</h2><p class="sheet-sub">${isPez?'Luz ambiente':'Luz principal'} · ${isPez?'RGB 255':'RGB 100'}</p><div class="fields ${isPez?'':'three'}">${input('r','Rojo',base[0],isPez?255:100)}${input('g','Verde',base[1],isPez?255:100)}${input('b','Azul',base[2],isPez?255:100)}${isPez?input('bright','Brillo',base[3],100):''}</div><div class="actions"><button id="previewButton" class="btn">Probar ${isPez?'20':'8'} s</button><button id="restoreButton" class="btn sub">Ceder conexión</button></div><p class="note" style="margin-top:18px">${isPez?'No hay retorno automático en laboratorio: utiliza la aplicación Pez Verde.':'CIELO no restaura AUTO: libera la conexion y recupera tu curva desde My Chihiros.'}</p>`);$('previewButton').onclick=()=>preview(which);$('restoreButton').onclick=()=>restore(which)}
async function preview(which){if(state.busy)return;let isPez=which==='pez';let m=isPez?255:100,fields=['r','g','b',...(isPez?['bright']:[])];let a=fields.map(id=>{let v=$ (id).value.trim();return /^\d+$/.test(v)?Number(v):NaN});if(a.some((v,i)=>!Number.isInteger(v)||v<0||v>(i===3?100:m))){toast('Revisa los valores');return}if(!state.connected){toast('Conecta el iPhone a la red Bambu o CIELO96-CONFIG');return}if(!confirm(`Probar ${which==='chi'?'Chihiros':'Pez Verde'} manualmente. Primero autoriza envíos 90 s en Centro; después necesitarás recuperar tu programación desde su aplicación. ¿Continuar?`))return;state.busy=true;try{if(isPez)await api('/api/manual',{r:a[0],g:a[1],b:a[2],bright:a[3],lab_manual:'1'});else await api('/api/chihiros/manual',{r:a[0],g:a[1],b:a[2],lab_manual:'1'});toast('Prueba enviada');close();await sync()}catch(e){toast(e.message)}finally{state.busy=false}}
async function restore(which){
 if(!state.connected){toast('Sin conexión con ATOM');return}
 state.busy=true;
 try{
  await api('/api/ble/release',{device:which==='pez'?'pez_verde':'chihiros'});
  toast('Conexion liberada. Recupera tu programa desde la app original.');
  close();await sync()
 }catch(e){toast(e.message)}finally{state.busy=false}
}
async function restoreAll(){
 if(!state.connected){toast('Sin conexión');return}
 if(!confirm('Ceder conexiones Bluetooth sin cambiar la programacion. Despues usa My Chihiros para volver a su curva. ¿Continuar?'))return;
 try{
  await api('/api/lab/tx/disarm',{});
  await api('/api/ble/release',{device:'chihiros'});
  await api('/api/ble/release',{device:'pez_verde'});
  toast('Conexiones libres; ninguna orden AUTO enviada.');
  await sync()
 }catch(e){toast(e.message)}
}
function sceneChoose(name){let times={dawn:900,day:1100,dusk:1340,moon:1400};state.preview=times[name];state.scrub=times[name];$('scrub').value=state.scrub;document.querySelectorAll('.scene').forEach(b=>b.classList.toggle('active',b.dataset.scene===name));render();toast('Vista previa · sin cambios en las luces')}
function cieloEscapeHtml(value){
 return String(value??'').replace(/[&<>\"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','\"':'&quot;',"'":'&#39;'}[c]));
}
function wifiSetup(){
 const d=state.status||{};
 sheet(`<h2 class="sheet-title">Red Wi-Fi</h2>
 <p class="sheet-sub">Conectada: ${cieloEscapeHtml(d.homeSsid||'—')} · ${cieloEscapeHtml(d.homeIp||'sin IP')}</p>
 <p class="note">La red de recuperación WPA2 sigue disponible como <strong>CIELO96-CONFIG</strong>, IP <strong>192.168.4.1</strong>.</p>
 <div class="wifiForm">
 <button class="wifiSecondary" id="wifiScan">Buscar redes cercanas</button>
 <p id="wifiScanInfo" class="note">Puedes buscar o escribir manualmente otra red de 2,4 GHz.</p>
 <label>Red encontrada<select id="wifiNetworks"><option value="">Selecciona una red…</option></select></label>
 <label>Nombre de red<input id="wifiSsid" type="text" autocapitalize="none" autocomplete="off" placeholder="SSID"></label>
 <label>Contraseña<input id="wifiPass" type="password" autocomplete="new-password" placeholder="WPA2 de la nueva red"></label>
 <button class="wifiPrimary" id="wifiTry">Guardar y conectar</button>
 <button class="wifiSecondary" id="wifiForget">Olvidar red configurada</button></div>
 <details class="curveHelper"><summary>Más información de conexión</summary>
 <p class="note">El ATOM admite redes de 2,4 GHz y mantiene su punto de acceso privado de rescate. Al cambiar la red, la IP puede variar. Consulta el router o entra al AP CIELO96-CONFIG por 192.168.4.1.</p></details>`);
 $('wifiSsid').value=d.homeSsid||'';
 $('wifiNetworks').onchange=()=>{if($('wifiNetworks').value)$('wifiSsid').value=$('wifiNetworks').value};
 $('wifiScan').onclick=async()=>{
  const status=$('wifiScanInfo');status.textContent='Buscando redes…';$('wifiScan').disabled=true;
  try{
   await api('/api/wifi/scan',{});
   let networks=[];
   for(let n=0;n<18;n++){
    await new Promise(r=>setTimeout(r,750));
    const out=await api('/api/wifi/networks');
    if(!out.pending){networks=out.networks||[];break}
   }
   const select=$('wifiNetworks');
   select.replaceChildren(new Option('Selecciona una red…',''));
   const unique=new Set();
   for(const ap of networks){
    if(!ap.ssid||unique.has(ap.ssid))continue;
    unique.add(ap.ssid);
    select.add(new Option(ap.ssid+' · '+ap.rssi+' dBm',ap.ssid));
   }
   status.textContent=networks.length?networks.length+' redes detectadas. Elige una o escribe su nombre.':'No se encontraron redes; comprueba la banda 2,4 GHz.';
  }catch(e){status.textContent='No se pudo buscar: '+e.message}
  finally{$('wifiScan').disabled=false}
 };
 $('wifiTry').onclick=async()=>{
  const ssid=$('wifiSsid').value.trim(),password=$('wifiPass').value;
  if(!ssid||password.length<8)return toast('Indica red y contraseña (mínimo 8 caracteres)');
  if(!confirm('Cambiar CIELO a la red «'+ssid+'». Si cambia la IP, entra al AP privado CIELO96-CONFIG en 192.168.4.1. ¿Continuar?'))return;
  try{await api('/api/wifi',{ssid,password});toast('Conectando a la nueva Wi-Fi…');close()}
  catch(e){toast(e.message)}
 };
 $('wifiForget').onclick=async()=>{
  if(!confirm('¿Olvidar la red actual? El AP CIELO96-CONFIG seguirá disponible.'))return;
  try{await api('/api/wifi/forget',{});toast('Red olvidada');close();await sync()}
  catch(e){toast(e.message)}
 };
}
function chooseCurve(which){
 state.curve=which;
 document.querySelectorAll('[data-curve]').forEach(b=>b.classList.toggle('active',b.dataset.curve===which));
 render();
}
function selectedCurve(){return state.curve==='both'?'pez':state.curve}
// Historial corto SOLO en RAM del navegador, no es una coleccion de perfiles NVS.
function snapshotCurves(){return {chi:state.chi.map(p=>p.slice()),pez:state.pez.map(p=>p.slice()),dirty:{...state.dirty}}}
function pushUndo(){state.undoStack.push(snapshotCurves());if(state.undoStack.length>24)state.undoStack.shift();state.redoStack=[]}
function curveUndo(){if(!state.undoStack.length)return toast('Nada que deshacer');state.redoStack.push(snapshotCurves());restoreSnapshot(state.undoStack.pop());toast('Cambio deshecho. Pulsa Guardar si quieres conservarlo.')}
function curveRedo(){if(!state.redoStack.length)return toast('Nada que rehacer');state.undoStack.push(snapshotCurves());restoreSnapshot(state.redoStack.pop());toast('Cambio rehecho. Pulsa Guardar si quieres conservarlo.')}
function restoreSnapshot(s){state.chi=s.chi.map(p=>p.slice());state.pez=s.pez.map(p=>p.slice());state.dirty={...s.dirty};render()}
function pointsWire(a){return a.map(p=>p.join(',')).join(';')}
function wireChecksum(pv,ch){let h=2166136261>>>0;const input=pointsWire(pv)+'|'+pointsWire(ch);for(let i=0;i<input.length;i++){h=Math.imul(h^input.charCodeAt(i),16777619)>>>0;}return h.toString(16).padStart(8,'0')}
function versionedProfile(pv,ch){return {schema:'iwagumi-cielo-profile',schema_version:2,curve_unit:'minute',name:'IWAGUMI CIELO V5',exported:new Date().toISOString(),lights:{pezverde:{points:pv,unit:'rgb255-plus-brightness'},chihiros:{points:ch,unit:'percent'}},integrity:{algorithm:'fnv1a32',checksum:wireChecksum(pv,ch)}}}
async function cadenceTrialUi(){if(!state.connected)return toast('Conecta el ATOM');if(!confirm('ENSAYO SUPERVISADO: 15 segundos durante 20 minutos. Regresará a 30 segundos si detecta problema BLE o memoria. No dejarlo activo sin vigilancia. ¿Continuar?'))return;try{const d=await api('/api/cielo/cadence',{mode:'15',confirm:'ENSAYO_SUPERVISADO_15S'});setCadenceLabel(d);toast('Ensayo 15s activado')}catch(e){toast(e.message)}}
async function cadenceNormalUi(){try{const d=await api('/api/cielo/cadence',{mode:'30'});setCadenceLabel(d);toast('Vuelta a 30s')}catch(e){toast(e.message)}}
function setCadenceLabel(d){if(!$('cadenceState')||!d)return;$('cadenceState').textContent=d.trial_active?'Ensayo 15 s · '+d.trial_remaining_sec+' s restantes · '+(d.skipped_busy||0)+' ciclos con cola ocupada':'30 s · estable · '+(d.automatic_fallbacks||0)+' regresos automáticos · '+(d.fallback_reason||'')}

function parseMinute(s){if(!/^(?:[01]\d|2[0-3]):[0-5]\d$|^24:00$/.test(s))return NaN;const a=s.split(':').map(Number);return a[0]*60+a[1]}
function timeChoices(minute,fixed){
 const h=Math.floor(minute/60),m=minute%60;
 let hs='',ms='';
 for(let n=0;n<=24;n++){if(n===24&&!fixed)continue;hs+='<option value="'+n+'" '+(n===h?'selected':'')+'>'+String(n).padStart(2,'0')+'</option>'}
 for(let n=0;n<60;n++){ms+='<option value="'+n+'" '+(n===m?'selected':'')+'>'+String(n).padStart(2,'0')+'</option>'}
 return '<div class="timeGrid"><label>Hora<select id="hourH" '+(fixed?'disabled':'')+'>'+hs+'</select></label><label>Minutos<select id="hourM" '+(fixed?'disabled':'')+'>'+ms+'</select></label></div>';
}
function editorTime(){return Number($('hourH').value)*60+Number($('hourM').value)}
function pointEditor(which){
 const arr=state[which];state.editWhich=which;state.editIndex=Math.max(0,Math.min(state.editIndex,arr.length-1));
 const pos=state.editIndex,v=arr[pos],max=which==='chi'?100:255;
 let options=arr.map((p,i)=>'<option value="'+i+'" '+(i===pos?'selected':'')+'>'+fmt(p[0])+(p[0]===1440?' (fin)':'')+'</option>').join('');
 sheet('<h2 class="sheet-title">'+(which==='chi'?'Chihiros':'Pez Verde')+'</h2><p class="sheet-sub">Curva con '+arr.length+' puntos</p>'+
 '<div class="editorHint">Elige un punto existente para editarlo o pulsa «+ Añadir punto» para crear otro.</div>'+
 '<div class="pointSelect"><label>Punto existente<select id="pointPick">'+options+'</select></label><button id="pointAdd">+ Añadir punto</button></div>'+
 timeChoices(v[0],pos===arr.length-1)+
 '<div class="fields '+(which==='chi'?'three':'')+'">'+
 input('pointR','Rojo',v[1],max)+input('pointG','Verde',v[2],max)+input('pointB','Azul',v[3],max)+
 (which==='pez'?input('pointBright','Brillo',v[4],100):'')+'</div>'+
 '<div class="pointNudge"><span>Mover hora:</span><button id="pointBack" type="button" aria-label="Mover un minuto antes">−1 min</button><button id="pointForward" type="button" aria-label="Mover un minuto después">+1 min</button></div>'+ 
 '<div class="actions"><button class="btn" id="pointApply">Aplicar cambios</button><button class="btn sub" id="pointRemove">Eliminar</button></div>'+
 '<p class="note" style="margin-top:12px">Pulsa Guardar desde Curvas para conservar los cambios. El último punto de las 24:00 está protegido.</p>');
 $('pointPick').onchange=e=>{state.editIndex=Number(e.target.value);pointEditor(which)};
 $('pointApply').onclick=()=>applyPoint(which);
 $('pointAdd').onclick=()=>addPoint(which);
 $('pointRemove').onclick=()=>removePoint(which);
 $('pointBack').onclick=()=>nudgePoint(-1);$('pointForward').onclick=()=>nudgePoint(1);
 if(pos===arr.length-1){$('pointRemove').disabled=true;$('pointBack').disabled=true;$('pointForward').disabled=true;}
}
function nudgePoint(delta){
 const which=state.editWhich,arr=state[which],i=state.editIndex;
 if(i===arr.length-1)return toast('El cierre de 24:00 está protegido');
 const target=Number($('hourH').value)*60+Number($('hourM').value)+delta;
 if(target<0||target>=1440||(i>0&&target<=arr[i-1][0])||target>=arr[i+1][0])return toast('No cabe ese minuto entre los puntos vecinos');
 $('hourH').value=String(Math.floor(target/60));$('hourM').value=String(target%60);
 toast('Nueva hora '+fmt(target)+' · pulsa Aplicar cambios para confirmar');
}
function applyPoint(which){
 const arr=state[which],idx=state.editIndex,last=idx===arr.length-1,max=which==='chi'?100:255;
 const minute=last?1440:editorTime();
 const fields=['pointR','pointG','pointB',...(which==='pez'?['pointBright']:[])];
 const a=fields.map(id=>{let v=$(id).value.trim();return /^\d+$/.test(v)?Number(v):NaN});
 if(!Number.isFinite(minute)||minute>1440||(idx>0&&minute<=arr[idx-1][0])||(!last&&minute>=arr[idx+1][0])){
  toast('Hora repetida o fuera de orden. Selecciona una hora entre los puntos vecinos.');return;
 }
 if(a.some((v,i)=>!Number.isInteger(v)||v<0||v>(i===3?100:max))){toast('Valores RGB fuera de rango');return}
 if(which==='chi'&&last&&a.some(x=>x!==0)){toast('24:00 Chihiros debe ser RGB 0');return}
 if(which==='pez'&&((idx===0&&a[3]!==0)||(last&&a[3]!==0))){toast('Pez Verde: brillo inicial y final 0');return}
 pushUndo();arr[idx]=[minute,...a];state.dirty[which]=true;render();toast('Punto actualizado. Recuerda Guardar.');pointEditor(which);
}
function addPoint(which){
 const arr=state[which],max=64;
 if(arr.length>=max){toast('Has alcanzado el máximo de '+max+' puntos');return}
 const pos=state.editIndex,anchor=Math.min(pos,arr.length-2);
 const lo=arr[anchor][0],hi=arr[anchor+1][0];
 let guess=(hi-lo>1)?Math.floor((lo+hi)/2):NaN;
 if(!Number.isFinite(guess)){let spot=arr.findIndex((x,i)=>i<arr.length-1&&arr[i+1][0]-x[0]>1);guess=spot>=0?Math.floor((arr[spot][0]+arr[spot+1][0])/2):NaN}
 if(!Number.isFinite(guess)){toast('No quedan minutos libres entre los puntos existentes.');return}
 sheet('<h2 class="sheet-title">Añadir punto</h2><p class="sheet-sub">'+(which==='chi'?'Chihiros':'Pez Verde')+' · Nuevo horario</p>'+
 '<div class="editorHint">Elige hora y minutos. Calcularemos automáticamente su color y brillo a partir de la curva actual. Luego podrás ajustarlos.</div>'+timeChoices(guess,false)+
 '<p class="note">Puedes elegir cualquier minuto entre el primer punto y las 24:00, siempre que no exista ya.</p>'+
 '<div class="actions"><button id="confirmNewPoint" class="btn">Crear punto</button><button id="cancelNewPoint" class="btn sub">Cancelar</button></div>');
 $('confirmNewPoint').onclick=()=>{
   const t=editorTime();
   if(t<=arr[0][0]||t>=1440||arr.some(v=>v[0]===t)){toast('Selecciona una hora nueva posterior al comienzo y anterior a las 24:00');return}
   const value=vAt(arr,t),idx=arr.findIndex(p=>p[0]>t);
   pushUndo();arr.splice(idx,0,[t,...value]);state.editIndex=idx;state.dirty[which]=true;
   render();pointEditor(which);toast('Punto '+fmt(t)+' creado con valores interpolados.');
 };
 $('cancelNewPoint').onclick=()=>pointEditor(which);
}
function removePoint(which){
 const arr=state[which],idx=state.editIndex;
 if(idx===arr.length-1||arr.length<=2){toast('Ese punto es necesario');return}
 if(!confirm('¿Eliminar el punto de '+fmt(arr[idx][0])+'?'))return;
 pushUndo();arr.splice(idx,1);state.editIndex=Math.min(idx,arr.length-1);state.dirty[which]=true;render();pointEditor(which);
}
function validCurve(which,a){
 const max=which==='chi'?100:255,n=which==='chi'?4:5;
 if(!Array.isArray(a)||a.length<2||a.length>64)return false;
 if(!a.every((p,i)=>Array.isArray(p)&&p.length===n&&p.every(Number.isInteger)&&p[0]>=0&&p[0]<=1440&&(!i||p[0]>a[i-1][0])&&p.slice(1,4).every(v=>v>=0&&v<=max)))return false;
 if(which==='chi')return a.at(-1)[0]===1440&&a.at(-1).slice(1).every(x=>x===0);
 return a.every(p=>p[4]>=0&&p[4]<=100)&&a[0][4]===0&&a.at(-1)[0]===1440&&a.at(-1)[4]===0;
}
async function saveProgram(){
 if(!state.connected)return toast('Conecta el ATOM para guardar');
 if(!validCurve('chi',state.chi)||!validCurve('pez',state.pez))return toast('Una curva contiene puntos inválidos; nada guardado');
 if(!confirm('Guardar ambas curvas de una vez (un único perfil CIELO). Si falla una validación, ninguna cambia. ¿Continuar?'))return;
 try{
  const checksum=wireChecksum(state.pez,state.chi);
  const out=await api('/api/curves/commit',{schema_version:'2',generation:String(state.profileGeneration),checksum,pv:pointsWire(state.pez),ch:pointsWire(state.chi)});
  if(!out?.ok||!Number.isInteger(out.generation))throw Error('El ATOM no confirmó el guardado completo');
  state.profileGeneration=out.generation;state.dirty.chi=false;state.dirty.pez=false;
  state.undoStack=[];state.redoStack=[];
  toast('Guardado atómico: '+out.pv+' Pez Verde + '+out.chi+' Chihiros. Generación '+out.generation);
  await sync();
 }catch(e){toast(e.message+' · el perfil anterior continúa activo')}
}
async function restoreProgram(){
 if(!confirm('Restaurar en el EDITOR los valores originales de fábrica (no V5). No modifica el ATOM hasta pulsar Guardar. ¿Continuar?'))return;
 pushUndo();state.chi=CHI.map(p=>p.slice());state.pez=PEZ.map(p=>p.slice());state.dirty.chi=true;state.dirty.pez=true;
 chooseCurve('both');toast('Originales en editor. Puedes deshacer o guardar ambos juntos.');
}
function exportProgram(){
 if(!validCurve('chi',state.chi)||!validCurve('pez',state.pez))return toast('Corrige antes los puntos');
 const obj=versionedProfile(state.pez,state.chi);
 const url=URL.createObjectURL(new Blob([JSON.stringify(obj,null,2)],{type:'application/json'}));
 const a=document.createElement('a');a.href=url;a.download='IWAGUMI-CIELO-perfil-v2.json';a.click();
 setTimeout(()=>URL.revokeObjectURL(url),1000);toast('Perfil completo exportado con checksum');
}
async function importProgram(ev){
 const file=ev.target.files?.[0];if(!file)return;
 try{
  if(file.size>100000)throw Error('Archivo demasiado grande');
  const obj=JSON.parse(await file.text()),chi=obj?.lights?.chihiros?.points,pez=obj?.lights?.pezverde?.points;
  if(!validCurve('chi',chi)||!validCurve('pez',pez))throw Error('Perfil incompatible o incompleto');
  if(obj.schema_version===2){
   if(obj.schema!=='iwagumi-cielo-profile'||obj.curve_unit!=='minute'||obj.integrity?.algorithm!=='fnv1a32'||obj.integrity?.checksum!==wireChecksum(pez,chi))throw Error('Checksum o schema del perfil incorrecto. No se ha importado.');
  }else if(obj.schema_version!==undefined)throw Error('Versión de perfil no compatible');
  else if(!confirm('Perfil LEGADO sin checksum. Comprobaremos los valores pero no podemos verificar su integridad de origen. ¿Continuar?'))return;
  if(!confirm('¿Cargar este perfil en el editor? No se transmitirá a las luces hasta pulsar Guardar.'))return;
  pushUndo();state.chi=chi.map(x=>x.slice());state.pez=pez.map(x=>x.slice());
  state.dirty.chi=true;state.dirty.pez=true;chooseCurve('both');toast('Perfil verificado · pulsa Guardar (atómico)');
 }catch(e){toast(e.message)}finally{ev.target.value=''}
}
function manualChooser(){
 sheet('<h2 class="sheet-title">Prueba manual</h2><p class="sheet-sub">Pruebas breves con retorno automático</p>'+
 '<div class="actions"><button class="btn" id="testChi">Chihiros · 8 s</button><button class="btn sub" id="testPez">Pez Verde · 20 s</button></div>'+
 '<p class="note" style="margin-top:15px">R4.4: usa la pestaña Pruebas. Una orden manual no garantiza volver automáticamente al estado anterior.</p>');
 $('testChi').onclick=()=>lampSheet('chi');$('testPez').onclick=()=>lampSheet('pez');
}

async function monitorSettings(){
 try{
  const d=await api('/api/monitor/status');
  sheet('<h2 class="sheet-title">Alertas e historial</h2><p>El observador independiente guarda 7 y 30 días y envía avisos. Las luces funcionan aunque se desconecte.</p><p>Lectura local sin clave: '+(d.enabled?'habilitada':'desactivada')+'. Esto no confirma que el observador esté funcionando.</p><div class="actions"><button class="btn" id="monitorEnable">Habilitar seguimiento</button><button class="btn" id="monitorDisable">Desactivar seguimiento</button></div>');
  $('monitorEnable').onclick=async()=>{try{await api('/api/monitor/enable',{});toast('Lectura local habilitada')}catch(e){toast(e.message)}};
  $('monitorDisable').onclick=async()=>{try{await api('/api/monitor/disable',{});toast('Lectura local desactivada')}catch(e){toast(e.message)}};
 }catch(e){toast(e.message)}
}
async function maintenanceRead(){
 try{const d=await api('/api/maintenance');
  sheet('<h2 class="sheet-title">Mantenimiento</h2><p>Consulta sin modificar memoria. El respaldo completo requiere USB; no se ha realizado desde esta pantalla.</p><pre id="maintenanceResult" style="white-space:pre-wrap;overflow-wrap:anywhere"></pre>');
  $('maintenanceResult').textContent=JSON.stringify(d,null,2);
 }catch(e){toast(e.message)}
}
function settings(){
 const d=state.status||{};
 sheet(`<h2 class="sheet-title">Ajustes</h2><p class="sheet-sub">${cieloEscapeHtml(d.firmware||'CIELO')}</p>
 <div class="settings-row"><span>Wi-Fi</span><button id="wifiHome">Cambiar red ↗</button></div>
 <div class="settings-row"><span>Hora</span><button id="syncClock">Sincronizar ↗</button></div>
 <details class="curveHelper"><summary>Más opciones</summary>
 <div class="settings-row"><span>Alertas e historial</span><button id="monitorSettings">Configurar ↗</button></div>
 <div class="settings-row"><span>Mantenimiento</span><button id="maintenanceRead">Consultar ↗</button></div>
 <p class="note">Las curvas arrancan automáticamente. Estos controles solo son para detenerlas o ceder Chihiros a su app.</p>
 <div class="liveButtons"><button id="chNightOn">Reanudar curvas</button><button id="chNightOff">Usar My Chihiros</button><button id="cieloPauseAll">Pausar CIELO</button></div></details>`);
 $('monitorSettings').onclick=monitorSettings;$('maintenanceRead').onclick=maintenanceRead;
 $('wifiHome').onclick=wifiSetup;
 $('chNightOn').onclick=chNightStart;$('chNightOff').onclick=chNightStop;$('cieloPauseAll').onclick=cieloPauseAllUi;
 $('syncClock').onclick=async()=>{
  try{await api('/api/clock',{epoch:Math.floor(Date.now()/1000)});toast('Hora sincronizada');close();await sync()}
  catch(e){toast(e.message)}
 };
}
function exportCurves(){let d={name:'IWAGUMI CIELO',version:9,exported:new Date().toISOString(),lights:{chihiros:{points:state.chi,unit:'percent',source:'local-MyChihiros-not-synced'},pezverde:{points:state.pez,unit:'rgb255-plus-brightness',source:state.connected?'ATOM':'demo'}}};let b=new Blob([JSON.stringify(d,null,2)],{type:'application/json'}),u=URL.createObjectURL(b),a=document.createElement('a');a.href=u;a.download='Cielo-curvas.json';a.click();setTimeout(()=>URL.revokeObjectURL(u),1000)}
// Audio opcional: trueno suave sintetizado en el navegador, nunca en el ATOM.
let stormSoundOn=false,stormAudioContext=null,lastStormAudio=0;
function stormThunder(){
 if(!stormSoundOn||!stormAudioContext)return;
 try{
  const ctx=stormAudioContext,seconds=3.0;
  const buffer=ctx.createBuffer(1,Math.floor(ctx.sampleRate*seconds),ctx.sampleRate),arr=buffer.getChannelData(0);
  let x=0;for(let i=0;i<arr.length;i++){x=x*.90+(Math.random()*2-1)*.10;arr[i]=x;}
  const src=ctx.createBufferSource();src.buffer=buffer;
  const filter=ctx.createBiquadFilter();filter.type='lowpass';filter.frequency.value=270;
  const gain=ctx.createGain();gain.gain.setValueAtTime(.001,ctx.currentTime);
  gain.gain.exponentialRampToValueAtTime(.48,ctx.currentTime+.18);
  gain.gain.exponentialRampToValueAtTime(.001,ctx.currentTime+seconds);
  src.connect(filter);filter.connect(gain);gain.connect(ctx.destination);src.start();src.stop(ctx.currentTime+seconds);
 }catch(e){console.log('Audio opcional:',e)}
}
async function toggleStormSound(){
 try{
  if(!stormSoundOn){
    const AC=window.AudioContext||window.webkitAudioContext;
    if(!AC)throw Error('Safari no permite Web Audio en este dispositivo');
    stormAudioContext=stormAudioContext||new AC();
    await stormAudioContext.resume();stormSoundOn=true;
    $('stormSound').textContent='Desactivar audio de tormenta';$('stormSoundStatus').textContent='Audio activado. Pulsa Escuchar trueno para comprobar volumen; mantén Safari visible.';
    toast('Audio activado. Prueba el trueno ahora.');
  }else{
    stormSoundOn=false;
    $('stormSound').textContent='Activar truenos suaves en este iPhone';
    toast('Truenos desactivados');
  }
 }catch(e){toast(e.message)}
}
async function stormTestSoundUi(){
 try{
  const AC=window.AudioContext||window.webkitAudioContext;
  if(!AC)throw Error('Web Audio no está disponible en este iPhone');
  stormAudioContext=stormAudioContext||new AC();
  await stormAudioContext.resume();
  stormSoundOn=true;
  stormThunder();
  $('stormSound').textContent='Desactivar audio de tormenta';
  $('stormSoundStatus').textContent='Si has oído el trueno, mantén Safari abierto, visible y con volumen.';
  toast('Trueno de prueba enviado al altavoz del iPhone');
 }catch(e){$('stormSoundStatus').textContent='ERROR de audio: '+e.message;toast(e.message)}
}
async function stormCopyReport(){
 try{
  const response=await fetch('/api/storm/report',{cache:'no-store'});
  if(!response.ok)throw Error('No se pudo obtener informe');
  const report=await response.text();
  if(navigator.clipboard&&navigator.clipboard.writeText){
    await navigator.clipboard.writeText(report);toast('Informe copiado');
  }else{
    const el=document.createElement('textarea');
    el.value=report;el.style.position='fixed';el.style.top='0';el.style.opacity='0';
    document.body.appendChild(el);el.select();
    const ok=document.execCommand('copy');el.remove();
    if(ok)toast('Informe copiado');
    else{
      const w=window.open('/api/storm/report','_blank');
      if(!w)toast('Abre el informe con el botón Ver informe');
    }
  }
 }catch(e){toast('Abre el informe con Ver informe: '+e.message)}
}
async function stormStartUi(){
 if(!state.connected)return toast('ATOM sin conexión');
 if(!confirm('Iniciar una tormenta SUAVE de diez minutos entre las 15:40 y las 21:00. Oscurecerá Chihiros y Pez Verde y producirá dos relámpagos moderados. Recuperará My Chihiros AUTO y la curva Pez Verde. Supervisa las luces; evita la prueba si los animales se estresan. ¿Continuar?'))return;
 try{
  await api('/api/storm/start',{confirm:'storm600'});lastStormAudio=0;
  toast('Tormenta iniciada · diez minutos');await sync()
 }catch(e){toast(e.message)}
}
async function stormStopUi(){
 if(!state.connected)return toast('ATOM sin conexión');
 try{await api('/api/storm/stop',{});toast('Restaurando AUTO y luz natural');await sync()}
 catch(e){toast(e.message)}
}
async function pezImpactApply(){
 if(!state.connected)return toast('Sin conexión');
 if(state.dirty.pez)return toast('Guarda o exporta ediciones pendientes primero');
 if(!confirm('Pez Verde: colores 9.4 mucho más intensos (magenta, oro, cobre). Brillo hasta el 50% no probado físicamente. Guardaremos la curva anterior. ¿Aplicar?'))return;
 try{await api('/api/pez/cielo94/apply',{confirm:'impact94'});toast('Impacto 9.4 activado, anterior guardado');await sync()}catch(e){toast(e.message)}
}
async function pezImpactRestore(){
 if(!confirm('¿Recuperar la curva Pez Verde anterior?'))return;
 try{await api('/api/pez/cielo94/restore',{});toast('Curva anterior recuperada');await sync()}catch(e){toast(e.message)}
}


function diagFacts(d){
 const pv=d.bluetooth?.per_device?.pez_verde||{},ch=d.bluetooth?.per_device?.chihiros||{};
 const v=d.lighting?.natural_v4||{},n=d.network||{},sy=d.system||{},al=d.alerts||{};
 const num=x=>Number.isFinite(Number(x))?Number(x):0;
 const up=num(d.uptime_sec),p=num(pv.gatt_ok),c=num(ch.gatt_ok);
 const badWiFi=n.home_wifi_connected===false,badWorker=d.bluetooth?.worker_unresponsive===true;
 const badClock=d.clock_diagnostics?.valid===false;
 const critical=al.current_critical===true||d.status==='incidencia_actual';
 const professional=d.professional||{};
 let level='good',heading='CIELO sin incidencias actuales',why='El diagnóstico se ha recibido del ATOM. Las confirmaciones GATT no miden la luz.';
 let advice='No necesitas intervenir. Los avisos anteriores quedan conservados en el historial.';
 if(p+c===0&&up<180){
  level='wait';heading='El sistema está arrancando';why='Esperando las primeras transmisiones.';
  advice='Espera unos tres minutos; los datos se consultan automáticamente.';
 }else if(badWiFi||badWorker||badClock||critical){
  level='error';heading='CIELO requiere atención';
  if(badWiFi){why='La red Wi-Fi doméstica está desconectada.';advice='Comprueba la Wi-Fi desde Ajustes; si cambiaste de router utiliza el acceso de recuperación.';}
  else if(badWorker){why='La tarea Bluetooth dejó de responder.';advice='Comprueba la luz de ambas pantallas y descarga el Informe Maestro antes de reiniciar.';}
  else if(badClock){why='El reloj no está sincronizado.';advice='Sincroniza la hora desde Ajustes.';}
  else {why='El firmware ha señalado una incidencia.';advice='Descarga el Informe Maestro para revisar su causa. No modifiques las curvas mientras la analizamos.';}
 }else if(p===0||c===0){
  level='wait';heading='Esperando una pantalla';
  why='Falta todavía una comunicación Bluetooth confirmada en este arranque.';
  advice='Espera al siguiente envío; el informe se actualizará solo.';
 }
 if(professional.pez_verde==='FALLO'||professional.chihiros==='FALLO'){
  level='error';heading='Alerta de conexión con una pantalla';
  why='El Informe Maestro detecta un fallo actual. Comprueba visualmente ambas luces.';
 }else if(level==='good' && (professional.pez_verde==='ADVERTENCIA'||professional.chihiros==='ADVERTENCIA')){
  level='wait';heading='Bluetooth recuperándose o pendiente';
  why='Hay una operación pendiente o una conexión que todavía debe verificarse.';
 }
 return {level,heading,why,advice,pv,ch,v,n,sy,al,up,p,c,num};
}
function diagRender(d){
 const a=diagFacts(d),v=d.lighting?.natural_v4||{};
 const retrying=v.ch_reconnect_pending||v.pv_off_failures>0||v.pv_consecutive_errors>0||v.ch_consecutive_errors>0;
 const paused=v.always_on===false&&!v.pez_verde_active&&!v.chihiros_active;
 const level=a.level==='error'?'error':retrying||paused||a.level==='wait'?'warning':'good';
 $('diagIndicator').className='health-indicator '+level;
 $('diagState').textContent=level==='error'?'Revisar conexión':paused?'Control pausado':retrying?'Reconectando…':level==='warning'?'Preparando las pantallas':'Todo en orden';
 for(const [id,key,retrying] of [['diagPvStatus','pez_verde',v.pv_consecutive_errors>0||v.pv_off_failures>0],['diagChStatus','chihiros',v.ch_reconnect_pending||v.ch_consecutive_errors>0]]){
  const level=d.professional?.[key];
  $(id).textContent=paused?'En pausa':retrying?'Reconectando…':level==='CORRECTO'?'Disponible':level==='FALLO'?'Sin respuesta':'Esperando';
 }
}
async function diagLoad(){
 try{
  const d=await api('/api/diagnostico/maestro');window.cieloMasterSnapshot=d;diagRender(d);return d;
 }catch(e){
  $('diagIndicator').className='health-indicator warning';
  $('diagState').textContent='Sin comunicación con el ATOM';
  $('diagPvStatus').textContent='Sin datos';$('diagChStatus').textContent='Sin datos';throw e;
 }
}
async function serviceRefresh(){
 try{
  await diagLoad();
  const d=await api('/api/service/status');
  if($('otaInstalledVersion'))$('otaInstalledVersion').textContent='Versión instalada: '+(d.version||'sin identificar');
  if($('otaInstall'))$('otaInstall').disabled=!d.otaEnabled;
  $('otaState').textContent=d.otaBusy?'Instalando... bytes '+d.otaBytes:
    !d.otaEnabled?'Actualizaciones OTA desactivadas':'Elige el BIN compatible. Sin clave ni pulsación física para actualizar.';
 }catch(e){toast('Diagnóstico no disponible: '+e.message);}
}
async function serviceCopy(onlyErrors){
 try{
  const res=await fetch('/api/service/report?share=1'+(onlyErrors?'&errors=1':''),{cache:'no-store'});
  if(!res.ok)throw Error('No hay informe');
  const txt=await res.text();
  if(navigator.clipboard?.writeText){await navigator.clipboard.writeText(txt);}
  else{
   const ta=document.createElement('textarea');ta.value=txt;ta.style.position='fixed';ta.style.opacity='0';
   document.body.appendChild(ta);ta.select();
   if(!document.execCommand('copy'))throw Error('Copia no permitida');
   ta.remove();
  }
  toast(onlyErrors?'Incidencias copiadas':'Informe copiado. Pégalo en ChatGPT.');
 }catch(e){toast(e.message+' · prueba Descargar historial')}
}

async function otaPreflightUi(){
 try{
  const r=await fetch('/api/ota/preflight',{cache:'no-store',headers:{}});
  if(!r.ok)throw Error('No se pudo comprobar el estado del ATOM');
  const d=await r.json();
  if(!d.ready){toast('Actualización detenida: '+(d.reason||'revisión necesaria'));return false;}
  return true;
 }catch(e){toast('No se puede verificar si es seguro actualizar: '+e.message);return false;}
}
function sha256Local(data){
 const rotr=(n,s)=>(n>>>s)|(n<<(32-s));
 const h=[0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19];
 const k=[0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2];
 const raw=data instanceof Uint8Array?data:new Uint8Array(data),n=raw.length;
 const total=Math.ceil((n+9)/64)*64,p=new Uint8Array(total);
 p.set(raw);p[n]=128;
 const hi=Math.floor(n*8/4294967296),lo=(n*8)>>>0;
 const dv=new DataView(p.buffer);dv.setUint32(total-8,hi,false);dv.setUint32(total-4,lo,false);
 const w=new Uint32Array(64);
 for(let offset=0;offset<total;offset+=64){
  for(let i=0;i<16;i++)w[i]=dv.getUint32(offset+i*4,false);
  for(let i=16;i<64;i++){
   const x=w[i-15],y=w[i-2];
   const s0=rotr(x,7)^rotr(x,18)^(x>>>3),s1=rotr(y,17)^rotr(y,19)^(y>>>10);
   w[i]=(w[i-16]+s0+w[i-7]+s1)>>>0;
  }
  let [a,b,c,d,e,f,g,hh]=h;
  for(let i=0;i<64;i++){
   const s1=rotr(e,6)^rotr(e,11)^rotr(e,25),ch=(e&f)^(~e&g);
   const t1=(hh+s1+ch+k[i]+w[i])>>>0;
   const s0=rotr(a,2)^rotr(a,13)^rotr(a,22),maj=(a&b)^(a&c)^(b&c);
   const t2=(s0+maj)>>>0;
   hh=g;g=f;f=e;e=(d+t1)>>>0;d=c;c=b;b=a;a=(t1+t2)>>>0;
  }
  for(const [i,v] of [a,b,c,d,e,f,g,hh].entries())h[i]=(h[i]+v)>>>0;
 }
 return h.map(v=>v.toString(16).padStart(8,'0')).join('');
}
async function sha256Firmware(file){
 const a=await file.arrayBuffer();
 if(globalThis.crypto?.subtle){
  try{const hash=await crypto.subtle.digest('SHA-256',a);return Array.from(new Uint8Array(hash),v=>v.toString(16).padStart(2,'0')).join('')}catch(_){}
 }
 return sha256Local(new Uint8Array(a)); // Safari por HTTP: sin WebCrypto en origen no seguro
}
async function otaPost(path){
 const response=await fetch(path,{method:'POST',cache:'no-store',credentials:'same-origin',headers:{}});
 const body=await response.text();
 if(!response.ok)throw Error(body||'HTTP '+response.status);
 try{return JSON.parse(body)}catch{return body}
}
async function otaPrepareLights(){
 const output=$('otaState');
 output.textContent='Preparando Bluetooth y cediendo temporalmente las dos pantallas…';
 await otaPost('/api/ota/prepare');
 const until=Date.now()+54000;
 while(Date.now()<until){
  const r=await fetch('/api/ota/prepare/status',{cache:'no-store',headers:{}});
  if(!r.ok)throw Error('El ATOM no responde al preparar las luces');
  const d=await r.json();
  if(d.reason)output.textContent='Preparación: '+d.reason;
  if(d.phase==='ready')return;
  if(d.phase==='failed')throw Error(d.reason||'Preparación detenida por seguridad');
  await new Promise(resolve=>setTimeout(resolve,900));
 }
 throw Error('La preparación superó el tiempo seguro. Revisa el Informe Maestro.');
}
async function otaInstallUi(){
 const file=$('otaFile').files?.[0];
 if(!file||!file.name.toLowerCase().endsWith('.bin'))return toast('Selecciona el firmware .bin');
 if(file.size<500000||file.size>0x330000)return toast('El tamaño del BIN no es compatible');
 if(!await otaPreflightUi())return;
 if(!confirm('CIELO detendrá temporalmente las dos curvas, devolverá Chihiros a su programación guardada y liberará Pez Verde antes de instalar. ¿Continuar?'))return;
 $('otaInstall').disabled=true;
 const output=$('otaState'),progress=$('otaProgress');
 progress.value=0;progress.hidden=false;
 let preparationStarted=false;
 try{
  output.textContent='Calculando SHA-256 antes de detener las luces…';
  const sha=await sha256Firmware(file);
  preparationStarted=true;
  await otaPrepareLights();
  const form=new FormData();form.append('firmware',file,file.name);
  output.textContent='Luces cedidas por Bluetooth. Transfiriendo BIN al ATOM…';
  await new Promise((resolve,reject)=>{
   const x=new XMLHttpRequest();
   x.open('POST','/api/ota/upload',true);
   x.setRequestHeader('X-Cielo-Sha256',sha);
   x.upload.onprogress=ev=>{
    if(ev.lengthComputable){progress.value=Math.min(99,Math.round(ev.loaded/ev.total*99));
     output.textContent='Transfiriendo '+Math.round(ev.loaded/ev.total*100)+' % · esperando verificación interna';
    }
   };
   x.onload=()=>x.status>=200&&x.status<300?resolve():reject(new Error(x.responseText||'HTTP '+x.status));
   x.onerror=()=>reject(new Error('Conexión interrumpida. Comprueba la versión instalada antes de repetir'));
   x.ontimeout=()=>reject(new Error('Tiempo agotado. Comprueba el estado tras reinicio'));
   x.timeout=120000;x.send(form);
  });
  progress.value=100;
  output.textContent='Firmware transferido y aceptado; esperando reinicio. Comprueba después la versión y la salud BLE en Centro.';
  toast('Transferencia aceptada; verificaremos el arranque después');
 }catch(e){
  output.textContent='Actualización no confirmada: '+e.message;
  if(preparationStarted)try{await otaPost('/api/ota/prepare/cancel')}catch(_){}
  toast('Comprueba las luces y el Informe Maestro antes de repetir');
 }finally{$('otaInstall').disabled=false}
}
async function chiAuroraApply(){
 if(!state.connected)return toast('ATOM desconectado');
 if(state.dirty.chi)return toast('Guarda o exporta antes tus cambios sin guardar');
 if(!confirm('Aplicar la Aurora Chihiros 9.3.1 con rosa/púrpura al amanecer y dorado/rojo al atardecer. Guardaremos la curva anterior. My Chihiros no cambia: solo CIELO usa estos puntos. ¿Continuar?'))return;
 try{await api('/api/chihiros/aurora931/apply',{confirm:'aurora931'});toast('Nueva curva Chihiros guardada en ATOM');await sync()}
 catch(e){toast(e.message)}
}
async function chiAuroraRestore(){
 if(!state.connected)return toast('ATOM desconectado');
 if(!confirm('¿Restaurar la curva Chihiros que había antes de aplicar Aurora 9.3.1?'))return;
 try{await api('/api/chihiros/aurora931/restore',{});toast('Curva Chihiros anterior recuperada');await sync()}
 catch(e){toast(e.message)}
}
async function chNightStart(){
 if(!state.connected)return toast('Conecta el ATOM');
 if(state.dirty.chi||state.dirty.pez)return toast('Guarda antes las curvas');
 if(!confirm('CIELO controlará ambas luces las 24 horas, enviando incluso valores cero de noche y reanudando tras reinicios estables. ¿Activar control continuo?'))return;
 try{await api('/api/cielo/v4/start',{confirm:'ACTIVAR_NATURAL_V4'});toast('Curvas continuas activadas');await sync()}
 catch(e){toast(e.message)}
}
async function chNightStop(){
 if(!state.connected)return toast('Sin conexión');
 if(!confirm('EMERGENCIA: devolver Chihiros a la curva de My Chihiros. CIELO dejará de controlarla; Pez Verde sigue mientras el ATOM esté encendido. ¿Continuar?'))return;
 try{await api('/api/cielo/v4/return',{confirm:'VOLVER_MY_CHIHIROS'});toast('Recuperando My Chihiros');await sync()}
 catch(e){toast(e.message)}
}
async function cieloPauseAllUi(){
 if(!state.connected)return toast('Sin conexión');
 if(!confirm('Pausar ambas curvas y recuperar My Chihiros; Pez Verde conservará su último estado. ¿Confirmar?'))return;
 try{await api('/api/cielo/v4/pause',{confirm:'PAUSAR_TODO'});toast('Envíos pausados');await sync()}
 catch(e){toast(e.message)}
}
async function pezWarmApply(){
 if(!state.connected)return toast('Conecta ATOM');
 if(state.dirty.pez)return toast('Exporta o guarda antes las ediciones pendientes de Pez Verde');
 if(!confirm('Aplicar el cielo rosado y dorado 9.3.1 a Pez Verde. Se guardarán los puntos actuales para recuperarlos. ¿Continuar?'))return;
 try{await api('/api/pez/cielo931/apply',{confirm:'pink931'});toast('Nuevo Pez Verde 9.3.1 aplicado; copia guardada');await sync()}catch(e){toast(e.message)}
}
async function pezWarmRestore(){
 if(!state.connected)return toast('Sin conexión');
 if(!confirm('Restaurar Pez Verde a la curva que tenía antes de aplicar Cielo cálido 9.3. ¿Continuar?'))return;
 try{await api('/api/pez/cielo931/restore',{});toast('Curva anterior Pez Verde recuperada');await sync()}catch(e){toast(e.message)}
}
async function runChTest(n){
 if(!state.connected)return toast('Conecta el ATOM primero');
 if(!confirm('PRUEBA '+n+' del banco Chihiros. Cambiará temporalmente la luz durante un máximo de dos minutos. La curva guardada no se modifica. El ATOM pedirá AUTO al terminar. Supervisa la pantalla y verifica el retorno a AUTO. ¿Iniciar?'))return;
 try{await api('/api/chihiros/test',{confirm:'suite',test:String(n)});toast('Prueba '+n+' iniciada');await sync()}
 catch(e){toast(e.message)}
}
async function chLiveDisableUi(){
 if(!state.connected)return toast('Sin conexión');
 try{await api('/api/ble/release',{device:'chihiros'});toast('Chihiros liberada: recupera tu curva en My Chihiros');await sync()}
 catch(e){toast(e.message)}
}

let labProbeBusy=false;
function labPretty(d){ /* Pruebas históricas ocultas: resumen en el único Informe Maestro. */ }
async function labRefresh(){
 if(!isLive())return;
 try{labPretty(await api('/api/lab/status'))}
 catch(e){if($('labGuidedState'))$('labGuidedState').textContent='Última comprobación sin datos: '+e.message}
}
async function releaseLight(device){
 try{
  await api('/api/ble/release',{device});toast('Liberando '+device+' sin cambiar iluminacion');
  await new Promise(r=>setTimeout(r,1200));await labRefresh();
 }catch(e){toast(e.message)}
}
async function waitLab(check,maxWait){
 const until=Date.now()+maxWait;
 while(Date.now()<until){
  const d=await api('/api/lab/status');
  if(check(d))return d;
  await new Promise(r=>setTimeout(r,850));
 }
 throw Error('Tiempo de espera agotado; revisa el Informe Maestro');
}
let guidedBusy=false;
async function labGuided(){
 if(guidedBusy)return;
 guidedBusy=true;$('labGuided').disabled=true;
 const label=$('labGuidedState');
 try{
  label.textContent='1/3 · comprobando memoria, red y reloj';
  // /api/lab/start SOLO admite POST; el RC1 enviaba GET: origen del error R4.
  await api('/api/lab/start',{});
  label.textContent='2/3 · midiendo velocidad de respuesta';
  const times=[];
  for(let i=0;i<12;i++){
   const st=performance.now();
   await api('/api/lab/status');
   times.push(Math.round(performance.now()-st));
  }
  const avg=Math.round(times.reduce((x,y)=>x+y,0)/times.length);
  await api('/api/lab/browser-sample',{count:'12',avg:String(avg),max:String(Math.max(...times))});
  label.textContent='3/3 · buscando luces sin escribir GATT';
  const current=await api('/api/lab/status');
  if(current.checks?.ble_worker){
   const prev=current.discovery_scan?.completed||0;
   await api('/api/lab/scan',{});
   await waitLab(x=>(x.discovery_scan?.completed||0)>prev&&!x.discovery_scan.pending,19000);
   label.textContent='Comprobación terminada. Consulta Informe Maestro: red, HTTP y búsqueda Bluetooth.';
  }else label.textContent='Informe actualizado; Bluetooth todavía no disponible para búsqueda.';
  await api('/api/lab/stop',{});
  await Promise.all([labRefresh(),serviceRefresh()]);
 }catch(e){
  label.textContent='Comprobación parcial: '+e.message+'. El Informe Maestro conserva los datos anteriores.';
  try{await api('/api/lab/stop',{})}catch(_){}
  await serviceRefresh();
 }finally{guidedBusy=false;$('labGuided').disabled=false}
}
async function contrast30(){
 if(!confirm('CHIH IROS: enviar 30/30/30 para comprobar una BAJADA VISIBLE desde la programación de My Chihiros. No modifica horarios. Comprueba que My Chihiros esté desconectada. ¿Continuar?'.replace('CHIH IROS','CHIHIROS')))return;
 $('contrast30').disabled=true;
 const note=$('contrastState');
 try{
  note.textContent='Enviando 30 / 30 / 30 por Bluetooth. Espera a ver la bajada…';
  await api('/api/chihiros/contrast/start',{confirm:'ENSAYO_30_30_30'});
  const result=await waitLab(d=>['esperando_confirmacion_visual','error_manual','ensayo_caducado'].includes(d.chihiros_contrast?.stage),33000);
  if(result.chihiros_contrast?.stage!=='esperando_confirmacion_visual')
   throw Error('No se ha confirmado el envío. Recupera la iluminación desde My Chihiros.');
  note.textContent='Orden 30 / 30 / 30 terminada. ¿Ha bajado la luz? Si es así, pulsa PASO 2 para intentar recuperar tu curva.';
  $('restoreCustomTest').disabled=false;
 }catch(e){
  note.textContent='Prueba interrumpida: '+e.message;
  $('restoreCustomTest').disabled=true;
 }finally{$('contrast30').disabled=false}
}
async function testMyChihirosCurve(){
 if(!confirm('¿HAS VISTO que Chihiros bajó a 30/30/30? Ahora enviaremos exclusivamente el comando 0x03 de curva PERSONALIZADA. Debería subir hacia tu intensidad programada (ahora nos indicas 70/65/60). Si no sube, abre My Chihiros. ¿Continuar?'))return;
 $('restoreCustomTest').disabled=true;
 const note=$('restoreCustomState');
 try{
  const before=await api('/api/lab/status');
  if(before.chihiros_contrast?.stage!=='esperando_confirmacion_visual')
   throw Error('Primero completa el paso 1. El cambio a 30 debe haberse enviado.');
  const prev=(before.chihiros_my_curve?.gatt_ok||0)+(before.chihiros_my_curve?.gatt_failed||0);
  note.textContent='Enviando selección de curva personalizada. Mira si la luz SUBE.';
  await api('/api/chihiros/my-curve-test',{confirm:'PROBAR_CURVA_MY_CHIHIROS'});
  const final=await waitLab(d=>!d.chihiros_my_curve?.pending &&
     (d.chihiros_my_curve?.gatt_ok||0)+(d.chihiros_my_curve?.gatt_failed||0)>prev,28000);
  await api('/api/lab/tx/disarm',{});
  if(final.chihiros_contrast?.stage==='retorno_gatt_ok_verificar_luz'){
   note.textContent='El comando de curva ha terminado y liberó Bluetooth. ¿Volvió la iluminación a la programada (aprox. 70/65/60)? Observa y descarga el Informe Maestro.';
  }else note.textContent='El retorno no se ha completado correctamente. Comprueba la luz y usa My Chihiros para recuperarla.';
  await labRefresh();
 }catch(e){
  note.textContent='No se completó la recuperación: '+e.message+'. Usa My Chihiros si la luz no vuelve.';
  try{await api('/api/lab/tx/disarm',{})}catch(_){}
 }finally{$('restoreCustomTest').disabled=false}
}
async function cancelContrast(){
 if(!confirm('Cancelar y liberar Bluetooth. Esto NO garantiza restaurar automáticamente la luz. Puedes recuperarla desde My Chihiros. ¿Cancelar?'))return;
 try{
  await api('/api/chihiros/contrast/cancel',{});
  $('restoreCustomTest').disabled=true;
  $('contrastState').textContent='Ensayo cancelado. Recupera tu curva en My Chihiros si es necesario.';
  await labRefresh();
 }catch(e){toast(e.message)}
}
function init(){document.querySelectorAll('[data-view]').forEach(b=>b.onclick=()=>nav(b.dataset.view));document.querySelectorAll('[data-lamp]').forEach(b=>b.onclick=()=>{chooseCurve(b.dataset.lamp);state.editIndex=0;pointEditor(b.dataset.lamp)});
document.querySelectorAll('[data-curve]').forEach(b=>b.onclick=()=>{chooseCurve(b.dataset.curve)});$('openSettings').onclick=settings;$('sheetClose').onclick=close;$('editCurve').onclick=()=>{state.editIndex=0;pointEditor(selectedCurve())};$('editSelectedPoint').onclick=()=>{const sel=state.selectedPoint;if(sel){state.editIndex=sel.index;chooseCurve(sel.which);pointEditor(sel.which)}};document.querySelectorAll('[data-zoom]').forEach(b=>b.onclick=()=>selectZoom(Number(b.dataset.zoom)));$('compareCurves').onclick=()=>{state.compare=!state.compare;$('compareCurves').setAttribute('aria-pressed',String(state.compare));render()};$('saveCurve').onclick=saveProgram;$('undoCurve').onclick=curveUndo;$('redoCurve').onclick=curveRedo;$('resetCurve').onclick=restoreProgram;$('exportProfile').onclick=exportProgram;$('loadProfile').onclick=()=>$('importFile').click();$('importFile').onchange=importProgram;

$('diagDownload').onclick=async()=>{
 try{
  const res=await fetch('/api/diagnostico/maestro?download=1',{cache:'no-store'});
  if(!res.ok)throw Error('HTTP '+res.status);
  const u=URL.createObjectURL(await res.blob()),a=document.createElement('a');
  a.href=u;a.download='CIELO_INFORME_MAESTRO.json';a.click();
  setTimeout(()=>URL.revokeObjectURL(u),1500);
 }catch(e){toast('No se pudo descargar: '+e.message)}
};
$('otaInstall').onclick=otaInstallUi;


$('chart').onclick=e=>{
 const circle=e.target.closest?.('[data-which][data-index]');
 if(circle){chartPointSelect(circle.dataset.which,Number(circle.dataset.index));return;}
 const rect=$('chart').getBoundingClientRect(),window=chartWindow();
 const rel=clamp(((e.clientX-rect.left)/rect.width*338-29)/302,0,1);
 state.scrub=clamp(Math.round(window.begin+rel*window.span),0,1440);
 state.preview=state.scrub;$('scrub').value=state.scrub;render();
};
$('chart').onkeydown=e=>{
 if(!['ArrowLeft','ArrowRight','Home','End'].includes(e.key))return;
 e.preventDefault();const win=chartWindow();
 if(e.key==='Home')state.scrub=win.begin;
 else if(e.key==='End')state.scrub=win.end;
 else state.scrub=clamp(state.scrub+(e.key==='ArrowLeft'?-1:1),0,1440);
 state.preview=state.scrub;$('scrub').value=state.scrub;render();
};
$('scrub').oninput=e=>{state.scrub=Number(e.target.value);state.preview=state.scrub;render()};$('resetTime').onclick=()=>{state.scrub=currentClock();state.preview=null;$('scrub').value=state.scrub;render()};$('modal').onclick=e=>{if(e.target===$('modal'))close()};document.addEventListener('keydown',e=>{if(e.key==='Escape')close();dialogKeydown(e)});state.scrub=currentClock();$('scrub').value=state.scrub;render();sync();serviceRefresh();setInterval(()=>{if(state.preview===null){state.scrub=currentClock();$('scrub').value=state.scrub}sync()},6000);setInterval(()=>{if(state.view==='lab')diagLoad().catch(()=>{});},15000)}
window.addEventListener('DOMContentLoaded',init);
</script></body></html>)CIELOUI_96";
