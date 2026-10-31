/* Cali Camping Unit - UI bound to camperd over WebSocket /ws (SPEC 4 and 9).
   No texts in this file: everything visible comes from i18n/de.json. */
(() => {
'use strict';
const $ = id => document.getElementById(id);
const screen = $('screen');
const PROTO = 1;
const HOLD_MS = 100;

let T = {};
const t = (key, p = {}) => {
  let s = T[key];
  if (s == null) return key;
  for (const [k, v] of Object.entries(p)) s = s.replace('{' + k + '}', v);
  return s;
};

/* ---------- Zustand ---------- */
const S = {
  page: 'overview', expertTab: 'io', pin: 1,
  online: false, st: null, vic: null, io: null, hello: null,
  lastInput: Date.now(), discardTouch: false, lastTouchSent: 0, lastCerbo: null,
  roof: {dir: null, seq: 0, timer: null, btn: null, pointer: null},
  cmdSeq: 0,
};

/* ---------- Hilfen ---------- */
const fmt = (v, unit = '', d = 0, signed = false) => v == null ? '–'
  : (signed && v > 0 ? '+' : '') + Number(v).toFixed(d).replace('-', '−').replace('.', ',') + (unit ? `<span class="unit">${unit}</span>` : '');
const set = (id, html) => { const n = $(id); if (n) n.innerHTML = html; };
const text = (id, s) => { const n = $(id); if (n) n.textContent = s; };
const chip = (id, label, cls = '') => { const n = $(id); n.className = 'chip ' + cls; n.textContent = label; };
let toastT;
const toast = (msg, cls = '') => {
  const el = $('toast'); el.textContent = msg; el.className = 'toast show ' + cls;
  clearTimeout(toastT); toastT = setTimeout(() => { el.className = 'toast'; }, 2600);
};
const mmss = s => `${Math.floor(s / 60)}:${String(Math.floor(s % 60)).padStart(2, '0')}`;
const qText = q => ({uncalibrated: t('common.uncalibrated'), open: t('common.sensor_open'), short: t('common.sensor_short'),
  stale: t('common.sensor_stale'), implausible: t('common.sensor_implausible')})[q] || t('common.none');

/* Victron: Wert nur, wenn vorhanden und frisch; sonst null (nie Ersatzwerte). */
const vic = key => {
  const V = S.vic; if (!V || !V.connected) return null;
  const parts = key.split('.');
  const e = parts.length === 2 ? V[parts[0]]?.[parts[1]] : V[key];
  return e && e.age_s <= V.stale_s ? e.val : null;
};

/* ---------- WebSocket ---------- */
let ws = null;
function connect() {
  ws = new WebSocket(`${location.protocol === 'https:' ? 'wss' : 'ws'}://${location.host}/ws`);
  ws.onmessage = ev => {
    let m; try { m = JSON.parse(ev.data); } catch { return; }
    if (m.v !== PROTO) return;
    if (m.type === 'hello') S.hello = m.data;
    else if (m.type === 'state') {
      S.st = m.data; S.online = true;
      if (m.data.ui?.discard_first_touch) S.discardTouch = true;
      render();
    } else if (m.type === 'victron') {
      const was = S.lastCerbo; S.vic = m.data; S.lastCerbo = !!m.data.connected;
      if (was != null && was !== S.lastCerbo) toast(S.lastCerbo ? t('victron.back') : t('victron.lost'));
      render();
    } else if (m.type === 'io') { S.io = m.data; if (S.page === 'expert') render(); }
    else if (m.type === 'ack' && !m.ok) onReject(m);
  };
  ws.onopen = () => { subscribe(); };
  ws.onclose = () => {
    const wasOnline = S.online;
    S.online = false; S.st = null; S.io = null;
    if (S.roof.dir) stopRoof(t('roof.offline_stop'), false);
    else if (wasOnline) toast(t('top.offline'), 'err');
    render();
    setTimeout(connect, 1000);
  };
}
const send = msg => { if (ws && ws.readyState === 1) { ws.send(JSON.stringify({v: PROTO, ...msg})); return true; } return false; };
const cmd = (target, values) => send({type: 'cmd', id: 'c-' + (++S.cmdSeq), target, set: values});
const subscribe = () => send({type: 'subscribe', topics: S.page === 'expert' ? ['io'] : []});

function onReject(m) {
  const reason = m.reason || '';
  if (String(m.id).startsWith('hold-')) {
    stopRoof(T['roof.reject.' + reason] ? t('roof.reject.' + reason) : t('roof.stopped'));
  } else {
    toast(T['ack.' + reason] ? t('ack.' + reason) : t('ack.failed', {reason}), 'err');
  }
}

/* ---------- Navigation: Übersicht → Detail → Übersicht ---------- */
const PAGES = {
  overview: {}, energy: {back: 'overview'}, fridge: {back: 'overview'}, heat: {back: 'overview'},
  water: {back: 'overview'}, roof: {back: 'overview'}, settings: {back: 'overview'}, expert: {back: 'settings'},
};
function go(page) {
  stopRoof(null);
  S.page = page;
  document.querySelectorAll('main>section').forEach(s => { s.hidden = s.dataset.page !== page; });
  const p = PAGES[page];
  text('title', t('page.' + page));
  $('back').hidden = !p.back;
  $('brand').hidden = !!p.back;
  text('back-label', t('page.' + (p.back || 'overview')));
  $('open-settings').hidden = page === 'settings' || page === 'expert';
  subscribe();
  render();
}

/* ---------- Eingaben ---------- */
screen.addEventListener('pointerdown', e => {
  if (S.discardTouch) {  // erster Touch nach dem Aufwecken wird verworfen
    S.discardTouch = false; e.stopPropagation(); e.preventDefault(); return;
  }
  S.lastInput = Date.now();
  if (Date.now() - S.lastTouchSent > 1000) { S.lastTouchSent = Date.now(); send({type: 'touch'}); }
}, true);
screen.addEventListener('click', e => { if (S.discardTouch) { e.stopPropagation(); e.preventDefault(); } }, true);

$('back').addEventListener('click', () => go(PAGES[S.page].back || 'overview'));
$('open-settings').addEventListener('click', () => go('settings'));
$('open-expert').addEventListener('click', () => go('expert'));
document.querySelectorAll('[data-go]').forEach(b => b.addEventListener('click', () => go(b.dataset.go)));

const tickClock = () => text('clock', new Date().toLocaleTimeString('de-CH', {hour: '2-digit', minute: '2-digit'}));

/* Zurück zur Übersicht nach Inaktivität, nie während das Dach fährt */
setInterval(() => {
  const idleMin = S.st?.settings?.idle_min || 2;
  const roofMoving = S.roof.dir || S.st?.roof?.pos === 'moving';
  if (S.page !== 'overview' && !roofMoving && Date.now() - S.lastInput > idleMin * 60000) go('overview');
  tickClock();
}, 1000);

const act = fn => () => { if (S.online && S.st) fn(S.st); };
$('fr-minus').addEventListener('click', act(st => cmd('fridge', {target_c: st.fridge.target_c - 1})));
$('fr-plus').addEventListener('click', act(st => cmd('fridge', {target_c: st.fridge.target_c + 1})));
$('fr-toggle').addEventListener('click', act(st => {
  cmd('fridge', {on: !st.fridge.on}); toast(t(st.fridge.on ? 'fridge.toast_off' : 'fridge.toast_on'));
}));
$('ht-minus').addEventListener('click', act(st => cmd('heater', {level: st.heater.level - 1})));
$('ht-plus').addEventListener('click', act(st => cmd('heater', {level: st.heater.level + 1})));
$('ht-toggle').addEventListener('click', act(st => {
  cmd('heater', {on: !st.heater.on}); toast(t(st.heater.on ? 'heat.toast_off' : 'heat.toast_on'));
}));

/* ---------- Dach: Halten, Haltepuls alle 100 ms ---------- */
function startRoof(dir, btn, pointer) {
  if (!S.online || S.roof.dir) return false;
  const r = S.roof; r.dir = dir; r.btn = btn; r.pointer = pointer;
  const pulse = () => { r.seq++; S.lastInput = Date.now(); send({type: 'hold', target: 'roof', dir, seq: r.seq}); };
  pulse();
  r.timer = setInterval(pulse, HOLD_MS);
  render(); return true;
}
function stopRoof(reason, notify = true) {
  const r = S.roof; if (!r.dir) return;
  clearInterval(r.timer);
  if (notify) send({type: 'release', target: 'roof'});
  if (r.btn && r.pointer != null && r.btn.hasPointerCapture?.(r.pointer)) r.btn.releasePointerCapture(r.pointer);
  r.dir = null; r.btn = null; r.pointer = null;
  if (reason) toast(reason);
  render();
}
for (const dir of ['open', 'close']) {
  const b = $('rf-' + dir);
  b.addEventListener('pointerdown', e => {
    if (e.button > 0 || !e.isPrimary) return;
    e.preventDefault();
    if (startRoof(dir, b, e.pointerId)) b.setPointerCapture(e.pointerId);
  });
  b.addEventListener('pointerup', () => stopRoof(null));
  b.addEventListener('pointercancel', () => stopRoof(t('roof.stopped')));
  b.addEventListener('lostpointercapture', () => stopRoof(null));
  b.addEventListener('contextmenu', e => e.preventDefault());
}
window.addEventListener('blur', () => stopRoof(t('roof.stopped')));
document.addEventListener('visibilitychange', () => { if (document.hidden) stopRoof(t('roof.stopped')); });

/* ---------- Einstellungen ---------- */
$('st-bright').addEventListener('input', e => set('st-bright-val', fmt(+e.target.value, '%')));
$('st-bright').addEventListener('change', e => send({type: 'settings', set: {brightness: +e.target.value}}));
$('st-theme').querySelectorAll('button').forEach(b => b.addEventListener('click', () => send({type: 'settings', set: {theme: b.dataset.v}})));
$('st-idle').querySelectorAll('button').forEach(b => b.addEventListener('click', () => send({type: 'settings', set: {idle_min: +b.dataset.v}})));
$('x-tabs').querySelectorAll('button').forEach(b => b.addEventListener('click', () => { S.expertTab = b.dataset.v; render(); }));

/* ---------- MultiPlus-Steuerung: jede Änderung zweimal tippen ---------- */
let arm = null;  // {key, value, until}
function armOrSend(key, value) {
  if (arm && arm.key === key && arm.value === value && Date.now() < arm.until) {
    cmd('victron', key === 'mode' ? {mode: value} : {current_limit_a: value});
    arm = null;
  } else {
    arm = {key, value, until: Date.now() + 3000};
    setTimeout(render, 3100);
  }
  render();
}
$('en-mode').querySelectorAll('button').forEach(b => b.addEventListener('click', () => armOrSend('mode', +b.dataset.v)));
const limStep = d => {
  const c = S.st?.victron_control; if (!c || c.limit_min_a == null) return;
  const cur = S.limDraft ?? vic('vebus.current_limit') ?? c.limit_min_a;
  S.limDraft = Math.min(c.limit_max_a, Math.max(c.limit_min_a, Math.round((cur + d) * 2) / 2));
  render();
};
$('x-lim-minus').addEventListener('click', () => limStep(-0.5));
$('x-lim-plus').addEventListener('click', () => limStep(0.5));
$('x-lim-set').addEventListener('click', () => { if (S.limDraft != null) armOrSend('limit', S.limDraft); });

/* ---------- Expertenansicht ---------- */
function buildExpert() {
  const row = (el, ch, name, key) => {
    const r = document.createElement('div'); r.className = 'io-row' + (name ? '' : ' free');
    r.innerHTML = `<span class="ch">${ch}</span><span></span><span class="v num" data-io="${key}">–</span>`;
    r.children[1].textContent = name || t('x.free'); $(el).appendChild(r);
  };
  T['io.di'].forEach((n, i) => row('x-di', 'DI' + (i + 1), n, 'di-' + i));
  // SM-H-001: K1-K4 used, K5-K8 free (same COM group, +12 V)
  for (let i = 0; i < 8; i++) row('x-do', 'K' + (i + 1), i < 4 ? T['io.k'][i] : t('x.unused_k'), 'k-' + i);
  // Purecrea module on GPIO: heater setpoint ladder H1-H4
  const g = document.createElement('div'); g.className = 'io-group'; g.textContent = t('x.ladder'); $('x-do').appendChild(g);
  for (let i = 0; i < 4; i++) row('x-do', 'H' + (i + 1), t('x.ladder_seg', {n: i + 1}), 'h-' + i);
  row('x-do', 'T2p', t('x.ladder_sum'), 'ohm');
  T['io.ai'].forEach((n, i) => row('x-ai', 'AI' + (i + 1), n, 'ai-' + i));
  for (let i = 1; i <= 32; i++) {
    const b = document.createElement('button'); b.type = 'button'; b.textContent = String(i).padStart(2, '0');
    if (!T.pins[i]) b.className = 'reserve';
    b.addEventListener('click', () => { S.pin = i; render(); }); $('x-pins').appendChild(b);
  }
}
const rowsHtml = list => list.map(([k, v]) => `<div class="row"><span>${k}</span><strong class="num">${v}</strong></div>`).join('');

/* ---------- Darstellung ---------- */
function render() {
  const st = S.st, on = S.online && !!st;
  const theme = !st ? 'dark' : st.settings.theme === 'auto' ? (st.vehicle.dim58 ? 'dark' : 'light') : st.settings.theme;
  screen.dataset.theme = theme;

  // Kopf
  $('dot-unit').classList.toggle('off', !on);
  $('banner').hidden = on; $('link-unit').hidden = !on;
  $('dot-cerbo').classList.toggle('off', !(S.vic && S.vic.connected));
  set('outside', on ? fmt(st.outside_c, '°C', 1) : '–');
  document.querySelectorAll('[data-act]').forEach(b => { b.disabled = !on; });
  const alert = on ? (st.alerts.find(a => a.level === 'err') || st.alerts[0]) : null;
  $('alert').hidden = !alert;
  if (alert) {
    const id = alert.id.startsWith('fridge_fault_') ? 'fridge_fault' : alert.id;
    $('alert').textContent = t('alert.' + id); $('alert').className = 'alert ' + (alert.level === 'err' ? 'err' : '');
  }

  renderEnergy();
  if (!on) { renderOfflineValues(); renderExpert(); return; }

  // Kühlbox
  const f = st.fridge, fcode = f.fault;
  const frState = fcode ? [t('fridge.fault_long', {n: fcode, text: t('fridge.code.' + fcode)}), 'err']
    : !f.on ? [t('fridge.off'), ''] : f.temp_q !== 'ok' ? [qText(f.temp_q), 'warn']
    : f.request ? [t('fridge.requested'), 'accent'] : [t('fridge.reached'), 'ok'];
  const ftemp = f.temp_q === 'ok' ? fmt(f.temp_c, '°C', 1) : '–';
  set('ov-fridge', ftemp); text('ov-fridge-sub', f.on ? t('fridge.target_sub', {t: f.target_c}) : t('fridge.switched_off'));
  chip('ov-fridge-chip', fcode ? t('fridge.fault', {n: fcode}) : frState[0], frState[1]);
  set('fr-temp', ftemp); chip('fr-chip', frState[0], frState[1]);
  set('fr-target', fmt(f.target_c, '°C'));
  text('fr-toggle', t(f.on ? 'fridge.toggle_on' : 'fridge.toggle_off'));
  $('fr-toggle').setAttribute('aria-pressed', String(f.on));
  $('fr-minus').disabled = !f.on || f.target_c <= 1;
  $('fr-plus').disabled = !f.on || f.target_c >= 10;
  text('fr-note', fcode ? t('fridge.note_fault') : f.on && f.temp_q !== 'ok' ? t('fridge.note_sensor') : t('fridge.note', {on: '1,5', off: '1'}));

  // Heizung
  const h = st.heater;
  const PH = {off: ['', t('heat.chip.ready')], start: ['accent', t('heat.chip.wait')], heating: ['ok', t('heat.chip.heating')],
    afterrun: ['warn', t('heat.chip.afterrun', {t: mmss(h.afterrun_s)})], fault: ['err', t('heat.chip.fault')]}[h.phase];
  const phase = t('heat.phase.' + h.phase);
  set('ov-heat', phase);
  text('ov-heat-sub', h.phase === 'afterrun' ? t('heat.chip.afterrun', {t: mmss(h.afterrun_s)}) : t('heat.level_sub', {n: h.level, max: (h.levels || 16) - 1}));
  chip('ov-heat-chip', h.phase === 'start' ? t('heat.chip.start') : PH[1], PH[0]);
  set('ht-phase', phase); chip('ht-chip', PH[1], PH[0]);
  text('ht-note', h.phase === 'afterrun' ? t('heat.note_afterrun') : h.phase === 'fault' ? t('heat.note_fault') : t('heat.note'));
  const maxLevel = (h.levels || 16) - 1;
  set('ht-level', h.level_c != null ? fmt(h.level_c, '°C') : h.level);
  text('ht-of', h.level_ok === false ? t('heat.level_uncal') : t('heat.of', {n: maxLevel}));
  text('ht-toggle', t(h.on ? 'heat.toggle_on' : 'heat.toggle_off'));
  $('ht-toggle').setAttribute('aria-pressed', String(h.on));
  $('ht-minus').disabled = h.level_ok === false || h.level <= 0;
  $('ht-plus').disabled = h.level_ok === false || h.level >= maxLevel;

  // Wasser
  const tk = st.tanks;
  const fresh = tk.fresh_q === 'ok' ? t('water.level.' + tk.fresh_step) : qText(tk.fresh_q);
  text('ov-fresh', tk.fresh_q === 'ok' ? t('water.fresh_short', {level: fresh}) : fresh); text('wa-fresh', fresh);
  for (const id of ['ov-fresh-steps', 'wa-fresh-steps']) [...$(id).children].forEach((s, i) => s.classList.toggle('on', tk.fresh_q === 'ok' && i < tk.fresh_step));
  if (tk.waste_q !== 'ok') {
    chip('ov-waste-chip', t('water.waste') + ': ' + qText(tk.waste_q), 'warn');
    text('wa-waste', qText(tk.waste_q)); chip('wa-waste-chip', t('common.no_data'), 'warn');
  } else {
    chip('ov-waste-chip', t(tk.waste_full ? 'water.waste_full_chip' : 'water.waste_ok_chip'), tk.waste_full ? 'warn' : 'ok');
    text('wa-waste', t(tk.waste_full ? 'water.waste_full' : 'water.waste_ok'));
    chip('wa-waste-chip', t(tk.waste_full ? 'water.empty_it' : 'water.in_order'), tk.waste_full ? 'warn' : 'ok');
  }

  // Dach
  const r = st.roof, moving = st.vehicle.moving, localDir = S.roof.dir;
  const posText = r.pos === 'moving'
    ? t(r.cmd === 'open' ? 'roof.pos.opening' : r.cmd === 'close' ? 'roof.pos.closing' : 'roof.pos.moving')
    : t('roof.pos.' + r.pos);
  text('ov-roof', posText); text('rf-pos', posText);
  const noIgn = r.require_ignition && !st.power.ignition;
  const rel = st.release || {roof: true, fridge: true, heater: true};
  const roofChip = !rel.roof ? [t('common.not_released'), 'warn']
    : moving && !['closed', 'unknown'].includes(r.pos) ? [t('roof.chip.moving_open'), 'err']
    : r.block === 'unit_io' ? [t('roof.chip.io'), 'err']
    : noIgn ? [t('roof.chip.no_ignition'), 'warn']
    : moving ? [t(r.direction_verified ? 'roof.chip.blocked_driving' : 'roof.chip.blocked_driving_all'), 'warn']
    : r.block === 'timeout' ? [t('roof.chip.timeout'), 'warn']
    : r.pos === 'moving' ? [t('roof.chip.running'), 'accent']
    : r.pos === 'open' || r.pos === 'closed' ? [t('roof.chip.endpos'), 'ok']
    : r.pos === 'unknown' ? [t('roof.chip.unknown'), 'warn'] : [t('roof.chip.stopped'), 'warn'];
  chip('ov-roof-chip', roofChip[0], roofChip[1]); chip('rf-chip', roofChip[0], roofChip[1]);
  $('rf-open').disabled = !rel.roof || ((moving || noIgn) && localDir !== 'open');
  $('rf-close').disabled = !rel.roof || (((moving && !r.direction_verified) || noIgn) && localDir !== 'close');
  for (const id of ['fr-toggle', 'fr-minus', 'fr-plus']) if (!rel.fridge) $(id).disabled = true;
  for (const id of ['ht-toggle', 'ht-minus', 'ht-plus']) if (!rel.heater) $(id).disabled = true;
  if (!rel.fridge) { chip('fr-chip', t('common.not_released'), 'warn'); chip('ov-fridge-chip', t('common.not_released'), 'warn'); }
  if (!rel.heater) { chip('ht-chip', t('common.not_released'), 'warn'); chip('ov-heat-chip', t('common.not_released'), 'warn'); }
  for (const dir of ['open', 'close']) $('rf-' + dir).setAttribute('aria-pressed', String(localDir === dir));

  // Einstellungen
  const cfg = st.settings;
  if (document.activeElement !== $('st-bright')) { $('st-bright').value = cfg.brightness; set('st-bright-val', fmt(cfg.brightness, '%')); }
  $('st-theme').querySelectorAll('button').forEach(b => b.setAttribute('aria-pressed', String(b.dataset.v === cfg.theme)));
  $('st-idle').querySelectorAll('button').forEach(b => b.setAttribute('aria-pressed', String(+b.dataset.v === cfg.idle_min)));
  text('st-note', cfg.backlight_control === 'none' ? t('set.theme_note') + ' ' + t('set.brightness_na') : t('set.theme_note'));

  renderExpert();
}

function renderOfflineValues() {
  for (const id of ['ov-fridge', 'ov-heat', 'ov-fresh', 'ov-roof', 'fr-temp', 'fr-target', 'ht-phase', 'ht-level', 'wa-fresh', 'wa-waste', 'rf-pos'])
    set(id, '–');
  for (const id of ['ov-fridge-sub', 'ov-heat-sub', 'fr-note', 'ht-note']) text(id, '–');
  for (const id of ['ov-fridge-chip', 'ov-heat-chip', 'ov-waste-chip', 'ov-roof-chip', 'fr-chip', 'ht-chip', 'wa-waste-chip', 'rf-chip'])
    chip(id, t('common.no_data'), 'warn');
}

/* What the battery is actually doing, from the Victron values (null = no data). */
function batteryStatus() {
  if (!(S.vic && S.vic.connected)) return null;
  const bw = vic('battery_w'), pv = vic('pv_w'), state = vic('vebus.state'), mode = vic('vebus.mode');
  const active = vic('vebus.active_input'), shore = active != null && active !== 240;
  if (bw == null) return null;
  let key;
  if (shore && [3, 4, 6, 7].includes(state)) key = 'charging_shore';
  else if (shore && state === 5) key = 'full_shore';
  else if (shore && [8, 11].includes(state) && Math.abs(bw) < 10) key = 'passthru';
  else if (bw > 5 && pv > 0) key = 'charging_solar';
  else if (bw > 5) key = 'charging';
  else if (bw < -5 && !shore && [9, 1].includes(state)) key = 'inverter';
  else if (bw < -5) key = 'discharging';
  else key = 'idle';
  const status = t('bat.status.' + key);
  return {key, charging: bw > 5, text: mode === 4 ? t('bat.status.mp_off', {status}) : status};
}

function renderEnergy() {
  const has = S.vic && S.vic.connected;
  const soc = vic('soc'), v = vic('battery_v'), a = vic('battery_a'), bw = vic('battery_w');
  const pv = vic('pv_w'), charger = vic('charger_w'), dc = vic('dc_system_w');
  const outP = vic('vebus.out_p'), state = vic('vebus.state'), active = vic('vebus.active_input');
  const shore = active != null && active !== 240;
  const inv = active === 240 ? outP : 0;
  const socCls = soc == null ? '' : soc < 15 ? 'err' : soc < 30 ? 'warn' : 'ok';
  const st = batteryStatus();
  const stCls = !st ? 'warn' : st.charging ? 'ok' : socCls || 'accent';

  // Overview tile and battery panel
  set('ov-soc', fmt(soc, '%'));
  set('ov-bat-sub', has ? `${fmt(v, 'V', 1)} · ${fmt(bw, 'W', 0, true)}` : t('common.no_victron'));
  chip('ov-bat-chip', st ? st.text : t('common.no_data'), stCls);
  set('en-soc', fmt(soc, '%'));
  text('en-soc-sub', !st ? t('common.no_data') : st.charging ? t('bat.charging_lc') : bw < -5 ? t('bat.discharging_lc') : t('bat.status.idle'));
  $('ring-val').setAttribute('stroke-dasharray', `${soc != null ? soc / 100 * 270 : 0} 360`);
  $('ring-val').setAttribute('stroke', socCls === 'err' ? 'var(--err)' : socCls === 'warn' ? 'var(--warn)' : 'var(--accent)');
  set('en-v', fmt(v, 'V', 1)); set('en-a', fmt(a, 'A', 1, true)); set('en-dc', fmt(dc, 'W'));

  // Charging sources
  set('en-in', pv != null && charger != null ? fmt(pv + charger, 'W') : '–');
  set('en-solar', fmt(pv, 'W')); set('en-charger', fmt(charger, 'W')); set('en-alt', '–');
  text('en-phase', state == null ? '–' : shore ? t('vebus.desc.' + state) : t('en.no_shore'));

  // MultiPlus: what it does now, switch, 230 V side
  const overload = vic('vebus.alarm_overload');
  chip('en-mp-chip', state == null ? t('common.no_data') : overload === 2 ? t('en.alarm_overload') : t('mp.chip.' + state),
    state == null ? 'warn' : overload === 2 || state === 2 ? 'err' : state === 0 ? '' : 'ok');
  text('en-mp-state', state == null ? (has ? '–' : t('common.no_victron')) : t('vebus.desc.' + state));
  renderModeSwitch();
  set('en-inv', fmt(inv, 'W'));
  const outS = vic('vebus.out_s'), load = S.vic?.load_pct ?? null;
  set('en-load', load != null ? `${fmt(load, '%')} · ${fmt(outS, 'VA')}` : '–');
  $('en-load-bar').className = 'bar' + (overload === 2 ? ' err' : load > 80 ? ' warn' : '');
  $('en-load-bar').firstElementChild.style.width = (overload === 2 ? 100 : load || 0) + '%';
  set('en-alarm', overload == null ? '–' : overload === 2 ? `<span style="color:var(--err)">${t('en.alarm_overload')}</span>` : t('en.alarm_none'));

  // Shore tile
  if (!has || active == null) { set('ov-shore', '–'); text('ov-shore-sub', t('common.no_victron')); chip('ov-shore-chip', t('common.no_data'), 'warn'); }
  else if (!shore) {
    text('ov-shore', t('shore.disconnected')); set('ov-shore-sub', t('shore.inverter_sub', {w: fmt(outP, 'W')}));
    chip('ov-shore-chip', overload === 2 ? t('shore.overload') : state != null ? t('mp.chip.' + state) : '–', overload === 2 ? 'err' : 'accent');
  } else {
    set('ov-shore', fmt(vic('vebus.in_v'), 'V')); set('ov-shore-sub', t('shore.charging_sub', {w: fmt(charger, 'W')}));
    chip('ov-shore-chip', state != null ? t('mp.chip.' + state) : '–', overload === 2 ? 'err' : 'ok');
  }
}

/* MultiPlus mode switch on the battery page: Nur Laden / Ein / Aus, two taps to confirm */
function renderModeSwitch() {
  const c = S.st?.victron_control || {enabled: false, modes: []};
  const online = !!(S.vic && S.vic.connected) && S.online;
  const curMode = vic('vebus.mode');
  $('en-mode').querySelectorAll('button').forEach(b => {
    const v = +b.dataset.v, isArmed = !!arm && arm.key === 'mode' && arm.value === v && Date.now() < arm.until;
    b.hidden = !c.modes.includes(v);
    b.disabled = !c.enabled || !online;
    b.textContent = isArmed ? t('x.confirm') : t('x.mode.' + v);
    b.classList.toggle('armed', isArmed);
    b.setAttribute('aria-pressed', String(curMode === v && !isArmed));
  });
  text('en-mode-note', !c.enabled ? t('mp.mode_disabled') : !online ? t('x.ctl.offline')
    : c.unconfirmed === 'mode' ? t('x.ctl.unconfirmed') : c.pending?.includes('mode') ? t('x.ctl.pending') : t('mp.mode_hint'));
}

function renderExpert() {
  if (S.page !== 'expert') return;
  $('x-tabs').querySelectorAll('button').forEach(b => b.setAttribute('aria-pressed', String(b.dataset.v === S.expertTab)));
  document.querySelectorAll('[data-x]').forEach(x => { x.hidden = x.dataset.x !== S.expertTab; });
  const io = S.online ? S.io : null;
  const bit = (key, v) => {
    const n = document.querySelector(`[data-io="${key}"]`); if (!n) return;
    n.textContent = v == null ? '–' : String(v); n.className = 'v num ' + (v ? 'hi' : 'lo');
  };
  for (let i = 0; i < 16; i++) bit('di-' + i, io ? io.di[i] : null);
  for (let i = 0; i < 8; i++) bit('k-' + i, io ? io.k[i] : null);
  for (let i = 0; i < 4; i++) bit('h-' + i, io ? io.h[i] : null);
  for (let i = 0; i < 8; i++) {
    const n = document.querySelector(`[data-io="ai-${i}"]`);
    n.innerHTML = io && io.ai_v[i] != null ? fmt(io.ai_v[i], 'V', 2) : '–'; n.className = 'v num';
  }
  const ohm = document.querySelector('[data-io="ohm"]');
  ohm.innerHTML = io && io.ladder_ohm != null ? fmt(io.ladder_ohm, 'Ω', 1) : t('common.uncalibrated'); ohm.className = 'v num';

  $('x-pins').querySelectorAll('button').forEach((b, i) => b.setAttribute('aria-pressed', String(i + 1 === S.pin)));
  text('x-pin-name', 'T32b/' + String(S.pin).padStart(2, '0'));
  text('x-pin-desc', T.pins[S.pin] || t('x.reserve'));

  const alarm = val => val == null ? '–' : val === 2 ? `<span style="color:var(--err)">${t('x.ac.alarm.2')}</span>` : t('x.ac.alarm.' + val);
  const active = vic('vebus.active_input');
  $('x-ac-in').innerHTML = rowsHtml([[t('x.ac.voltage'), fmt(vic('vebus.in_v'), 'V', 1)], [t('x.ac.current'), fmt(vic('vebus.in_i'), 'A', 2)],
    [t('x.ac.freq'), fmt(vic('vebus.in_f'), 'Hz', 1)], [t('x.ac.power'), fmt(vic('vebus.in_p'), 'W')],
    [t('x.ac.active_in'), active == null ? '–' : active === 240 ? t('x.ac.disconnected') : t('x.ac.input1')]]);
  $('x-ac-out').innerHTML = rowsHtml([[t('x.ac.voltage'), fmt(vic('vebus.out_v'), 'V', 1)], [t('x.ac.current'), fmt(vic('vebus.out_i'), 'A', 2)],
    [t('x.ac.freq'), fmt(vic('vebus.out_f'), 'Hz', 1)], [t('x.ac.power'), fmt(vic('vebus.out_p'), 'W')], [t('x.ac.apparent'), fmt(vic('vebus.out_s'), 'VA')]]);
  const state = vic('vebus.state'), mode = vic('vebus.mode');
  $('x-ac-dev').innerHTML = rowsHtml([[t('x.ac.model'), t('x.ac.model_val')], [t('x.ac.state'), state == null ? '–' : t('vebus.' + state)],
    [t('x.ac.mode'), mode == null ? '–' : t('x.ac.mode.' + mode)], [t('x.ac.overload'), alarm(vic('vebus.alarm_overload'))],
    [t('x.ac.low_bat'), alarm(vic('vebus.alarm_low_battery'))], [t('x.ac.temp'), alarm(vic('vebus.alarm_temperature'))],
    [t('x.ac.ripple'), alarm(vic('vebus.alarm_ripple'))]]);

  const c = S.st?.victron_control || {enabled: false, modes: []};
  const online = !!(S.vic && S.vic.connected) && S.online;
  const armed = k => arm && arm.key === k.key && arm.value === k.value && Date.now() < arm.until;
  if (S.limDraft != null && !arm && vic('vebus.current_limit') === S.limDraft) S.limDraft = null;
  const limOk = c.enabled && online && c.limit_min_a != null;
  const limShown = S.limDraft ?? vic('vebus.current_limit');
  set('x-lim', fmt(limShown, 'A', 1));
  for (const id of ['x-lim-minus', 'x-lim-plus', 'x-lim-set']) $(id).disabled = !limOk;
  const limArmed = S.limDraft != null && armed({key: 'limit', value: S.limDraft});
  $('x-lim-set').textContent = limArmed ? t('x.confirm') : t('x.lim_set');
  $('x-lim-set').classList.toggle('armed', limArmed);
  text('x-ctl-note', !c.enabled ? t('x.ctl.disabled') : !online ? t('x.ctl.offline')
    : c.unconfirmed ? t('x.ctl.unconfirmed') : c.pending?.length ? t('x.ctl.pending')
    : c.limit_min_a == null ? t('x.ctl.hint') + ' ' + t('x.ctl.limit_disabled') : t('x.ctl.hint'));

  $('x-conn-cerbo').innerHTML = rowsHtml([[t('x.conn.state'), S.vic?.connected ? t('x.conn.online') : t('x.conn.offline')],
    [t('x.conn.portal'), S.vic?.portal_id || '–']]);
  const h = S.hello || {};
  $('x-conn-unit').innerHTML = rowsHtml([[t('x.conn.version'), h.version || '–'], [t('x.conn.hal'), h.hal || '–'],
    [t('x.conn.profile'), h.profile || '–'], [t('x.conn.ws'), t('x.conn.ws_val')],
    [t('x.conn.mode'), S.st ? t('power.' + S.st.power.mode) : '–'],
    [t('x.conn.rail'), io ? fmt(io.rail_v, 'V', 2) : '–'],
    [t('x.conn.hold'), S.roof.dir ? `${S.roof.dir} #${S.roof.seq}` : t('x.conn.hold_none')]]);
}

/* ---------- Skalierung der festen Leinwand (Entwicklung am Mac) ---------- */
const frame = $('frame');
const fit = () => screen.style.setProperty('--k', String(frame.getBoundingClientRect().width / 1280));
new ResizeObserver(fit).observe(frame); fit();

/* ---------- Start ---------- */
fetch('i18n/de.json').then(r => r.json()).then(dict => {
  T = dict;
  document.querySelectorAll('[data-t]').forEach(n => { n.textContent = t(n.dataset.t); });
  document.querySelectorAll('[data-t-aria]').forEach(n => n.setAttribute('aria-label', t(n.dataset.tAria)));
  buildExpert();
  tickClock();
  go('overview');
  connect();
});
})();
