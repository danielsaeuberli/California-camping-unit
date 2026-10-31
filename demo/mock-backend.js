/* Browser-only stand-in for camperd, used for the GitHub Pages demo.
   Replaces window.WebSocket for "/ws" and speaks protocol v1 (SPEC 4), so the
   unchanged ui/ files run without a server. Simplified port of camperd/hal/sim.py
   and the module logic; not used on the unit. */
(() => {
'use strict';

const now = () => performance.now() / 1000;
const SCENARIOS = {
  solar: {soc: 76, v: 13.4, a: 3.2, pv: 240, charger: 0, dc: 85, state: 9, active: 240, inp: null, out: [230, 0.43, 50, 90, 100]},
  night: {ttg: 14 * 3600, soc: 54, v: 12.9, a: -21, pv: 0, charger: 0, dc: 120, state: 9, active: 240, inp: null, out: [230, 1.3, 50, 280, 300]},
  shore: {soc: 88, v: 13.8, a: 22, pv: 0, charger: 420, dc: 110, state: 5, active: 0, inp: [230, 2.96, 50, 650], out: [230, 0.87, 50, 180, 200]},
};
const CFG = {holdTimeout: 0.3, reverseGap: 0.3, maxRun: 60, fridgeOn: 1.5, fridgeOff: 1.0, minRun: 120, minPause: 180,
  heaterStartTimeout: 120, ignite: 6, afterrun: 240, roofTravel: 20};

/* ---------- simulated world + modules ---------- */
const W = {
  online: true, cerbo: true, scenario: 'solar', moving: false, ignition: true, dim58: false,
  wasteFull: false, fresh: 2, fridgeFault: null, overload: false, outside: 12.5,
};
const M = {
  roof: {pos: 0, cmd: null, energized: null, lastDir: null, offSince: -1e9, lastHold: -1e9, pressStart: 0, block: 'none', latched: false},
  fridge: {on: false, target: 5, temp: 4.8, request: false, changed: -1e9},
  heater: {on: false, level: 5, phase: 'off', since: 0, running: false, runT: 0},
  settings: {brightness: 80, theme: 'auto', idle_min: 2, language: 'de', backlight_control: 'none'},
  power: 'afterrun',
};

function roofHold(dir, seq) {
  const r = M.roof, t = now();
  if (dir !== 'open' && dir !== 'close') return 'bad_dir';
  if (!W.ignition && !W.moving) { r.block = 'ignition'; return 'blocked_ignition'; }
  if (W.moving && dir === 'open') { r.block = 'moving'; return 'blocked_moving'; }
  const newPress = t - r.lastHold > CFG.holdTimeout || dir !== r.cmd;
  if (newPress) {
    if (r.cmd && dir !== r.cmd) roofOff(t);
    r.cmd = dir; r.pressStart = t; r.latched = false; r.block = 'none';
  }
  r.lastHold = t;
  return r.latched ? 'timeout' : null;
}
function roofOff(t) { const r = M.roof; if (r.energized) r.offSince = t; r.energized = null; }
function roofRelease() { const r = M.roof; roofOff(now()); r.cmd = null; r.lastHold = -1e9; }

function tick(dt) {
  const t = now(), r = M.roof, f = M.fridge, h = M.heater;
  // roof (S3, S4, reversal gap S2)
  if (r.cmd) {
    if (t - r.lastHold > CFG.holdTimeout) roofRelease();
    else if (!W.ignition && !W.moving) { roofOff(t); r.cmd = null; r.block = 'ignition'; }
    else if (W.moving && r.cmd === 'open') { roofOff(t); r.cmd = null; r.block = 'moving'; }
    else if (t - r.pressStart >= CFG.maxRun) { roofOff(t); r.latched = true; r.block = 'timeout'; }
  }
  if (r.cmd && !r.latched && !r.energized) {
    const reversing = r.lastDir && r.lastDir !== r.cmd;
    if (!reversing || t - r.offSince >= CFG.reverseGap) { r.energized = r.cmd; r.lastDir = r.cmd; }
  }
  if (!r.cmd && !W.moving && r.block === 'moving') r.block = 'none';
  if (!r.cmd && W.ignition && r.block === 'ignition') r.block = 'none';
  if (r.energized) r.pos = Math.min(1, Math.max(0, r.pos + (r.energized === 'open' ? 1 : -1) * dt / CFG.roofTravel));
  // fridge thermostat with min run / pause (S6)
  let want = f.request;
  if (!f.on) want = false;
  else if (f.temp >= f.target + CFG.fridgeOn) want = true;
  else if (f.temp <= f.target - CFG.fridgeOff) want = false;
  if (want !== f.request && t - f.changed >= (f.request ? CFG.minRun : CFG.minPause)) { f.request = want; f.changed = t; }
  f.temp += (f.request ? -0.02 : 0.008) * dt;
  // heater phases; sim heater ignites 6 s after S+, runs on for 240 s
  const k4 = h.on && (h.phase === 'start' || h.phase === 'heating');
  if (k4 && !h.running) { h.runT += dt; if (h.runT >= CFG.ignite) { h.running = true; h.runT = 0; } }
  else if (!k4 && h.running) { h.runT += dt; if (h.runT >= CFG.afterrun) { h.running = false; h.runT = 0; } }
  else h.runT = 0;
  if (h.phase === 'start') {
    if (h.running) enter('heating');
    else if (t - h.since >= CFG.heaterStartTimeout) enter('fault');
  } else if (h.phase === 'afterrun' && !h.running) enter('off');
  // power
  M.power = W.ignition || W.moving ? 'drive' : 'afterrun';
}
function enter(phase) { M.heater.phase = phase; M.heater.since = now(); }

function heaterSet(on, level) {
  const h = M.heater;
  if (level != null) { if (level < 0 || level > 9) return 'out_of_range'; h.level = level; }
  if (on != null && on !== h.on) {
    h.on = on;
    if (on) enter('start');
    else enter(h.running ? 'afterrun' : 'off');
  }
  return null;
}
const VB = {mode: 3, limit: 10};
function victronSet(s) {
  if (s.mode != null) { if (![1, 3, 4].includes(s.mode)) return 'bad_mode'; VB.mode = s.mode; }
  if (s.current_limit_a != null) { if (s.current_limit_a < 3 || s.current_limit_a > 16) return 'out_of_range'; VB.limit = s.current_limit_a; }
  return null;
}
function fridgeSet(on, target) {
  const f = M.fridge;
  if (target != null) { const v = Math.round(target); if (v < 1 || v > 10) return 'out_of_range'; f.target = v; }
  if (on != null) f.on = on;
  return null;
}

/* ---------- messages ---------- */
function roofPos() {
  const r = M.roof;
  if (r.energized) return 'moving';
  return r.pos >= 1 ? 'open' : r.pos <= 0 ? 'closed' : 'between';
}
function alerts() {
  const a = [];
  const pos = roofPos();
  if ((W.ignition || W.moving) && pos !== 'closed') a.push({id: 'roof_open_ignition', level: W.moving ? 'err' : 'warn'});
  if (W.fridgeFault) a.push({id: 'fridge_fault_' + W.fridgeFault, level: 'err'});
  if (M.heater.phase === 'fault') a.push({id: 'heater_fault', level: 'err'});
  if (W.wasteFull) a.push({id: 'waste_full', level: 'warn'});
  return a;
}
function stateMsg() {
  const h = M.heater, f = M.fridge, r = M.roof;
  const afterrun = h.phase === 'afterrun' ? Math.max(0, Math.round(CFG.afterrun - (now() - h.since))) : 0;
  return {v: 1, type: 'state', ts: Date.now(), data: {
    power: {mode: M.power, ignition: W.ignition || W.moving},
    roof: {pos: roofPos(), cmd: r.cmd || 'none', block: r.block, direction_verified: true, require_ignition: true},
    fridge: {on: f.on, target_c: f.target, temp_c: Math.round(f.temp * 10) / 10, temp_q: 'ok', request: f.request, fault: W.fridgeFault},
    heater: {on: h.on, level: h.level, phase: h.phase, afterrun_s: afterrun, level_c: null, level_ok: true, levels: 10},
    tanks: {fresh_step: W.fresh, fresh_q: 'ok', waste_full: W.wasteFull, waste_q: 'ok'},
    outside_c: W.outside, outside_q: 'ok',
    vehicle: {moving: W.moving, d_plus: W.moving, t86s: true, dim58: W.dim58},
    alerts: alerts(),
    release: {roof: true, fridge: true, heater: true},
    victron_control: {enabled: true, modes: [1, 3, 4], limit_min_a: 3, limit_max_a: 16, pending: [], unconfirmed: null},
    settings: M.settings,
    ui: {discard_first_touch: false},
  }};
}
function victronMsg() {
  const s = SCENARIOS[W.scenario], e = val => val == null ? null : {val, age_s: 0.4};
  // like the MultiPlus: "Aus" -> off; "Nur Laden" without shore power -> inverter off, nothing to charge
  const invOff = VB.mode === 4 || (VB.mode === 1 && s.active === 240);
  const inp = s.inp || [null, null, null, null];
  return {v: 1, type: 'victron', ts: Date.now(), data: W.cerbo ? {
    soc: e(s.soc), time_to_go_s: e(s.ttg), battery_v: e(s.v), battery_a: e(s.a), battery_w: e(Math.round(s.v * s.a)), pv_w: e(s.pv),
    charger_w: e(s.charger), dc_system_w: e(s.dc),
    vebus: {state: e(invOff ? 0 : s.state), mode: e(VB.mode), current_limit: e(VB.limit), current_limit_adjustable: e(1), active_input: e(s.active), in_v: e(inp[0]), in_i: e(inp[1]), in_f: e(inp[2]), in_p: e(inp[3]),
      out_v: e(invOff ? 0 : s.out[0]), out_i: e(invOff ? 0 : s.out[1]), out_f: e(invOff ? 0 : s.out[2]), out_p: e(invOff ? 0 : s.out[3]), out_s: e(invOff ? 0 : s.out[4]), measurement_type: null, dc_w: null,
      alarm_overload: e(W.overload ? 2 : 0), alarm_low_battery: e(0), alarm_temperature: e(0), alarm_ripple: e(0)},
    load_pct: Math.round(s.out[4] / 1600 * 100), connected: true, portal_id: 'demo00000000', stale_s: 10,
  } : {connected: false, portal_id: null, stale_s: 10, vebus: {}, load_pct: null}};
}
function ioMsg() {
  const t = now(), h = M.heater, pos = M.roof.pos;
  const fault = W.fridgeFault, ph = t % 4, idx = Math.floor(ph / 0.5);
  const fdiag = !(fault && idx < fault && ph % 0.5 < 0.25);
  const di = [pos >= 1, pos <= 0, fdiag, h.running, W.scenario === 'shore', W.moving && t % 0.2 < 0.1, W.dim58, true,
    W.moving, W.ignition || W.moving, false, false, false, false, false, false].map(Number);
  const bits = [0, 1, 2, 3].map(i => h.on ? (h.level >> i) & 1 : 0);
  const ohm = 1750 + [27.4, 54.9, 110, 237].reduce((s, o, i) => s + (bits[i] ? o : 0), 0);  // sim values, not measured
  return {v: 1, type: 'io', ts: Date.now(), data: {
    di, k: [M.roof.energized === 'open', M.roof.energized === 'close', M.fridge.request, h.on && ['start', 'heating'].includes(h.phase),
      0, 0, 0, 0].map(Number),
    h: bits, ai_v: [2.42 - (M.fridge.temp - 5) * 0.03, [0.50, 0.95, 1.37, 1.69][W.fresh], W.wasteFull ? 0.50 : 1.69, 1.31, 0.0,
      M.roof.energized === 'open' ? 2.18 : 0, M.roof.energized === 'close' ? 2.18 : 0, 2.25],
    rail_v: 3.30, ladder_ohm: ohm,
  }};
}

/* ---------- fake WebSocket ---------- */
const sockets = new Set();
const RealWS = window.WebSocket;
class DemoSocket {
  constructor(url) {
    if (!String(url).endsWith('/ws')) return new RealWS(url);
    this.readyState = 0; this.topics = new Set(); this.holding = false;
    setTimeout(() => {
      if (!W.online) { this.readyState = 3; this.onclose?.({}); return; }
      this.readyState = 1; sockets.add(this);
      this.onopen?.({});
      this.push({v: 1, type: 'hello', data: {version: '0.1.0-demo', hal: 'demo', profile: 't6-california-a8l-2018'}});
      this.push(stateMsg()); this.push(victronMsg());
    }, 120);
  }
  push(msg) { if (this.readyState === 1) this.onmessage?.({data: JSON.stringify(msg)}); }
  send(raw) {
    if (this.readyState !== 1) return;
    const m = JSON.parse(raw);
    const ack = (ok, reason, id = m.id) => this.push({v: 1, type: 'ack', id, ok, reason: reason || null});
    if (m.type === 'hold') {
      const reason = roofHold(m.dir, m.seq);
      if (reason === null) this.holding = true;
      else ack(false, reason, 'hold-' + m.seq);
    } else if (m.type === 'release') { roofRelease(); this.holding = false; }
    else if (m.type === 'cmd') {
      const s = m.set || {};
      const reason = m.target === 'fridge' ? fridgeSet(s.on, s.target_c) : m.target === 'heater' ? heaterSet(s.on, s.level)
        : m.target === 'victron' ? victronSet(s) : 'unknown_target';
      if (m.target === 'victron') broadcast(victronMsg());
      ack(reason === null, reason); broadcast(stateMsg());
    } else if (m.type === 'subscribe') this.topics = new Set((m.topics || []).filter(x => x === 'io'));
    else if (m.type === 'settings') { Object.assign(M.settings, m.set || {}); ack(true); broadcast(stateMsg()); }
  }
  close() {
    if (this.readyState > 1) return;
    this.readyState = 3; sockets.delete(this);
    if (this.holding) roofRelease();  // socket loss = no more hold messages (S3)
    this.onclose?.({});
  }
}
DemoSocket.OPEN = 1;
window.WebSocket = DemoSocket;
const broadcast = msg => sockets.forEach(s => s.push(msg));

let last = now(), n = 0, lastState = '';
setInterval(() => {
  const t = now(); tick(t - last); last = t; n++;
  const st = stateMsg(), key = JSON.stringify(st.data);
  if (key !== lastState || n % 20 === 0) { lastState = key; broadcast(st); }
  if (n % 2 === 0) sockets.forEach(s => s.topics.has('io') && s.push(ioMsg()));
  if (n % 20 === 0) broadcast(victronMsg());
}, 50);

/* ---------- demo controls below the screen (not part of the unit) ---------- */
function controls() {
  const bar = document.createElement('div');
  bar.className = 'demo';
  bar.innerHTML = `
    <b>Demo</b>
    <label>Szenario <select data-k="scenario"><option value="solar">Solar lädt</option><option value="night">Nacht, Entladen</option><option value="shore">Landstrom</option></select></label>
    <label><input type="checkbox" data-k="online" checked> Steuerung online</label>
    <label><input type="checkbox" data-k="cerbo" checked> Victron-Daten</label>
    <label><input type="checkbox" data-k="ignition" checked> Zündung</label>
    <label><input type="checkbox" data-k="moving"> Fahrzeug fährt</label>
    <label><input type="checkbox" data-k="dim58"> Armaturenlicht (58d)</label>
    <label><input type="checkbox" data-k="wasteFull"> Abwasser voll</label>
    <label><input type="checkbox" data-k="fridgeFault"> Kühlbox-Fehler 3</label>
    <label><input type="checkbox" data-k="overload"> Überlast MultiPlus</label>
    <label>Frischwasser <select data-k="fresh"><option value="0">leer</option><option value="1">1/3</option><option value="2" selected>2/3</option><option value="3">voll</option></select></label>`;
  bar.querySelectorAll('[data-k]').forEach(el => el.addEventListener('change', () => {
    const k = el.dataset.k;
    if (k === 'scenario') W.scenario = el.value;
    else if (k === 'fresh') W.fresh = +el.value;
    else if (k === 'fridgeFault') W.fridgeFault = el.checked ? 3 : null;
    else W[k] = el.checked;
    if (k === 'online' && !el.checked) [...sockets].forEach(s => s.close());
    broadcast(stateMsg()); broadcast(victronMsg());
  }));
  const intro = document.createElement('div');
  intro.className = 'demo-intro';
  intro.innerHTML = `<b>Cali Camping Unit</b> · Demo der Touch-Oberfläche (1280 × 480). Statt <code>camperd</code> läuft ein Simulator im Browser.
    Dach: Taste gedrückt halten. <a href="https://github.com/danielsaeuberli/California-camping-unit">Code auf GitHub</a>`;
  document.body.prepend(intro);
  document.body.appendChild(bar);
  const css = document.createElement('style');
  css.textContent = `
    .demo-intro{max-width:1280px;margin:0 auto 12px;font:14px system-ui,sans-serif;color:#8a99a6;line-height:1.5}
    .demo-intro b{color:#b8c5cf}.demo-intro a{color:#4fb3d9}.demo-intro code{font-size:13px}
    .demo{max-width:1280px;margin:16px auto 0;display:flex;flex-wrap:wrap;gap:8px 20px;align-items:center;font:13px system-ui,sans-serif;color:#8a99a6}
    .demo label{display:inline-flex;align-items:center;gap:8px;min-height:40px}
    .demo select{min-height:36px;border-radius:8px;padding:0 8px}
    .demo input{width:18px;height:18px}
    .demo b{font-weight:600;color:#b8c5cf}
    body{padding:16px}
    @media (min-width:1320px){body{padding:24px}}`;
  document.head.appendChild(css);
}
if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', controls);
else controls();
})();
