/* Cali Camping Unit - UI bound to camperd over WebSocket /ws (SPEC 4 and 9).
   No texts in this file: everything visible comes from i18n/de.json. */
(() => {
'use strict';
const $ = id => document.getElementById(id);
// Version suffix of this script (demo builds load app.js?v=<build>); used for the language
// files so a browser never mixes texts and code from different builds.
const ASSET_V = (document.currentScript && new URL(document.currentScript.src).search) || '';
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
let LANG = 'de';
const LOCALE = {de: 'de-CH', en: 'en-GB'};
const fmt = (v, unit = '', d = 0, signed = false) => {
  if (v == null) return '–';
  let n = Number(v).toFixed(d).replace('-', '−');
  if (LANG === 'de') n = n.replace('.', ',');
  return (signed && v > 0 ? '+' : '') + n + (unit ? `<span class="unit">${unit}</span>` : '');
};
const set = (id, html) => { const n = $(id); if (n) n.innerHTML = html; };
const text = (id, s) => { const n = $(id); if (n) n.textContent = s; };
const chip = (id, label, cls = '') => { const n = $(id); if (!n) return; n.className = 'chip ' + cls; n.textContent = label; };
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
    else if (m.type === 'net') { S.net = m.data; S.netDraft = null; render(); }
    else if (m.type === 'ack' && m.ok && String(m.id).startsWith('net-')) toast(t('x.net.applied'));
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
  if (S.roof.auto) {  // tap-to-run: any touch anywhere stops the roof (incl. the STOP button)
    e.stopPropagation(); e.preventDefault(); stopRoof(t('roof.stopped')); return;
  }
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

const tickClock = () => text('clock', new Date().toLocaleTimeString(LOCALE[LANG], {hour: '2-digit', minute: '2-digit'}));

/* Zurück zur Übersicht nach Inaktivität, nie während das Dach fährt */
setInterval(() => {
  const idleMin = S.st?.settings?.idle_min || 2;
  const roofMoving = S.roof.dir || S.st?.roof?.pos === 'moving' || S.st?.roof?.afterrun;
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

/* ---------- Dach: Laufmeldung alle 100 ms ----------
   Hold-to-run: while the button is held. Tap-to-run (once the end positions are measured):
   one tap, the UI keeps sending until STOP, any touch, the end position or a reject. */
const tapMode = () => !!(S.st?.roof?.tap);
function startRoof(dir, btn, pointer, auto = false) {
  if (!S.online || S.roof.dir) return false;
  const r = S.roof; r.dir = dir; r.btn = btn; r.pointer = pointer; r.auto = auto;
  const pulse = () => { r.seq++; S.lastInput = Date.now(); send({type: 'hold', target: 'roof', dir, seq: r.seq, auto}); };
  pulse();
  r.timer = setInterval(pulse, HOLD_MS);
  render(); return true;
}
function stopRoof(reason, notify = true) {
  const r = S.roof; if (!r.dir) return;
  clearInterval(r.timer);
  if (notify) send({type: 'release', target: 'roof'});
  if (r.btn && r.pointer != null && r.btn.hasPointerCapture?.(r.pointer)) r.btn.releasePointerCapture(r.pointer);
  r.dir = null; r.btn = null; r.pointer = null; r.auto = false;
  if (reason) toast(reason);
  render();
}
for (const dir of ['open', 'close']) {
  const b = $('rf-' + dir);
  b.addEventListener('pointerdown', e => {
    if (e.button > 0 || !e.isPrimary || tapMode()) return;
    e.preventDefault();
    if (startRoof(dir, b, e.pointerId)) b.setPointerCapture(e.pointerId);
  });
  b.addEventListener('click', () => { if (tapMode()) startRoof(dir, null, null, true); });
  b.addEventListener('pointerup', () => { if (!S.roof.auto) stopRoof(null); });
  b.addEventListener('pointercancel', () => { if (!S.roof.auto) stopRoof(t('roof.stopped')); });
  b.addEventListener('lostpointercapture', () => { if (!S.roof.auto) stopRoof(null); });
  b.addEventListener('contextmenu', e => e.preventDefault());
}
window.addEventListener('blur', () => stopRoof(t('roof.stopped')));
document.addEventListener('visibilitychange', () => { if (document.hidden) stopRoof(t('roof.stopped')); });

/* ---------- Einstellungen ---------- */
$('st-bright').addEventListener('input', e => set('st-bright-val', fmt(+e.target.value, '%')));
$('st-bright').addEventListener('change', e => send({type: 'settings', set: {brightness: +e.target.value}}));
$('st-theme').querySelectorAll('button').forEach(b => b.addEventListener('click', () => send({type: 'settings', set: {theme: b.dataset.v}})));
$('st-idle').querySelectorAll('button').forEach(b => b.addEventListener('click', () => send({type: 'settings', set: {idle_min: +b.dataset.v}})));
$('st-lang').querySelectorAll('button').forEach(b => b.addEventListener('click', () => send({type: 'settings', set: {language: b.dataset.v}})));
$('x-tabs').querySelectorAll('button').forEach(b => b.addEventListener('click', () => {
  S.expertTab = b.dataset.v;
  if (S.expertTab === 'net') send({type: 'net_get'});
  render();
}));

/* ---------- Expertenansicht -> Netzwerk ----------
   Draft of the settings while editing; the Wi-Fi password is never sent to the screen and
   only goes back to camperd when a new one was typed. Apply needs two taps. */
const netDraft = () => {
  if (!S.netDraft && S.net?.config) {
    const c = S.net.config;
    S.netDraft = {eth: {...c.eth, dns: (c.eth.dns || []).join(', ')}, wifi: {ssid: c.wifi.ssid, psk: ''}, cerbo: {...c.cerbo}};
  }
  return S.netDraft;
};
const fieldGet = f => { const [a, b] = f.split('.'); return netDraft()?.[a]?.[b] ?? ''; };
const fieldSet = (f, v) => { const [a, b] = f.split('.'); netDraft()[a][b] = v; };
$('x-eth-mode').querySelectorAll('button').forEach(b => b.addEventListener('click', () => {
  if (netDraft()) { netDraft().eth.mode = b.dataset.v; render(); }
}));
document.querySelectorAll('.fld').forEach(b => b.addEventListener('click', () => {
  if (!netDraft()) return;
  openKeyboard(b.dataset.kb, t('x.net.' + b.dataset.f.split('.')[1]), String(fieldGet(b.dataset.f)), v => { fieldSet(b.dataset.f, v); render(); });
}));
let netArm = 0;
$('x-net-apply').addEventListener('click', () => {
  const d = netDraft(); if (!d) return;
  if (Date.now() > netArm) { netArm = Date.now() + 3000; setTimeout(render, 3100); render(); return; }
  netArm = 0;
  const eth = {mode: d.eth.mode};
  if (d.eth.mode === 'static') Object.assign(eth, {address: d.eth.address, gateway: d.eth.gateway, dns: d.eth.dns.split(/[ ,]+/).filter(Boolean)});
  const wifi = {ssid: d.wifi.ssid};
  if (d.wifi.psk) wifi.psk = d.wifi.psk;
  send({type: 'net_set', id: 'net-' + Date.now(), set: {eth, wifi, cerbo: {host: d.cerbo.host, port: +d.cerbo.port}}});
  render();
});

/* ---------- On-screen keyboard ---------- */
const KB_IP = [['1', '2', '3', '⌫'], ['4', '5', '6', '.'], ['7', '8', '9', '/'], [',', '0', '✕', '✓']];
const KB_TEXT = {
  abc: [['1', '2', '3', '4', '5', '6', '7', '8', '9', '0'], ['q', 'w', 'e', 'r', 't', 'z', 'u', 'i', 'o', 'p'],
    ['a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', '-'], ['⇧', 'y', 'x', 'c', 'v', 'b', 'n', 'm', '_', '⌫'], ['#+=', '.', '@', '␣', '✕', '✓']],
  sym: [['1', '2', '3', '4', '5', '6', '7', '8', '9', '0'], ['!', '"', '#', '$', '%', '&', '/', '(', ')', '='],
    ['?', '+', '*', ',', ';', ':', '<', '>', '[', ']'], ['{', '}', '~', '^', "'", '\\', '|', '€', '§', '⌫'], ['abc', '.', '@', '␣', '✕', '✓']],
};
const KB = {mode: 'ip', layer: 'abc', shift: false, value: '', done: null};
function openKeyboard(mode, label, value, done) {
  Object.assign(KB, {mode, layer: 'abc', shift: false, value, done});
  text('kb-label', label);
  $('kb').hidden = false;
  drawKeyboard();
}
function drawKeyboard() {
  const rows = KB.mode === 'ip' ? KB_IP : KB_TEXT[KB.layer];
  $('kb-keys').style.gridTemplateRows = `repeat(${rows.length}, 1fr)`;
  $('kb-keys').innerHTML = rows.map(r => {
    const cols = r.map(k => k === '␣' ? '4fr' : '1fr').join(' ');
    return `<div class="kb-row" style="grid-template-columns:${cols}">` + r.map(k => {
      const label = k.length === 1 && KB.shift ? k.toUpperCase() : k;
      const cls = k === '✓' ? 'ok' : ['⌫', '✕', '⇧', '#+=', 'abc', '␣'].includes(k) ? 'act' + (k === '⇧' && KB.shift ? ' on' : '') : '';
      return `<button type="button" class="${cls}" data-k="${k.replace(/"/g, '&quot;')}">${label === '␣' ? '' : label.replace(/</g, '&lt;')}</button>`;
    }).join('') + '</div>';
  }).join('');
  text('kb-val', KB.value || ' ');
}
$('kb-keys').addEventListener('click', e => {
  const k = e.target.closest('button')?.dataset.k; if (k == null) return;
  if (k === '✕') { $('kb').hidden = true; return; }
  if (k === '✓') { $('kb').hidden = true; KB.done && KB.done(KB.value.trim()); return; }
  if (k === '⌫') KB.value = KB.value.slice(0, -1);
  else if (k === '⇧') KB.shift = !KB.shift;
  else if (k === '#+=' || k === 'abc') KB.layer = k === '#+=' ? 'sym' : 'abc';
  else { KB.value += k === '␣' ? ' ' : KB.shift ? k.toUpperCase() : k; KB.shift = false; }
  drawKeyboard();
});

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

/* Long status texts on the overview tiles shrink to one line instead of wrapping. */
function fitLine(n) {
  n.style.fontSize = '';
  let fs = parseFloat(getComputedStyle(n).fontSize);
  while (n.scrollWidth > n.clientWidth && fs > 28) { fs -= 2; n.style.fontSize = fs + 'px'; }
}

/* Innentemperatur vom Ruuvi-Sensor über den Cerbo (nur Anzeige) */
function renderInterior(it) {
  $('top-interior').hidden = !it; $('ht-in-row').hidden = !it;
  if (!it) return;
  set('interior', it.c != null ? fmt(it.c, '°C', 1) : '–');
  set('ht-interior', it.c != null
    ? fmt(it.c, '°C', 1) + (it.rh != null ? ' · ' + t('heat.humidity', {n: fmt(it.rh, '%')}) : '')
    : t('interior.' + it.q));
}

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
  renderInterior(on ? st.interior : null);
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
  set('ov-fridge', ftemp); text('ov-fridge-sub', t('fridge.target_sub', {t: f.target_c}));
  chip('ov-fridge-chip', fcode ? t('fridge.fault', {n: fcode}) : frState[0], frState[1]);
  set('fr-temp', ftemp); chip('fr-chip', frState[0], frState[1]);
  set('fr-target', fmt(f.target_c, '°C'));
  text('fr-toggle', t(f.on ? 'fridge.toggle_on' : 'fridge.toggle_off'));
  $('fr-toggle').setAttribute('aria-pressed', String(f.on));
  $('fr-minus').disabled = !f.on || f.target_c <= 1;
  $('fr-plus').disabled = !f.on || f.target_c >= 10;
  text('fr-note', fcode ? t('fridge.note_fault') : f.on && f.temp_q !== 'ok' ? t('fridge.note_sensor') : t('fridge.note', {on: fmt(1.5, '', 1), off: fmt(1, '', 0)}));

  // Heizung
  const h = st.heater;
  const PH = {off: ['', t('heat.chip.ready')], start: ['accent', t('heat.chip.wait')], heating: ['ok', t('heat.chip.heating')],
    afterrun: ['warn', t('heat.chip.afterrun', {t: mmss(h.afterrun_s)})], fault: ['err', t('heat.chip.fault')]}[h.phase];
  const phase = t('heat.phase.' + h.phase);
  set('ov-heat', phase);
  text('ov-heat-sub', h.phase === 'afterrun' ? t('heat.chip.afterrun', {t: mmss(h.afterrun_s)}) : t('heat.level_sub', {n: h.level + 1, max: h.levels || 10}));
  chip('ov-heat-chip', h.phase === 'start' ? t('heat.chip.start') : PH[1], PH[0]);
  set('ht-phase', phase); chip('ht-chip', PH[1], PH[0]);
  text('ht-note', h.phase === 'afterrun' ? t('heat.note_afterrun') : h.phase === 'fault' ? t('heat.note_fault') : t('heat.note'));
  // Levels are counted 1..N like the original panel; internally 0..N-1.
  const maxLevel = (h.levels || 10) - 1;
  set('ht-level', h.level_c != null ? fmt(h.level_c, '°C') : h.level + 1);
  text('ht-of', h.level_ok === false ? t('heat.level_uncal') : t('heat.of', {n: maxLevel + 1}));
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
  // tap-to-run reached the end position (camperd stops too)
  if (S.roof.auto && r.pos === (S.roof.dir === 'open' ? 'open' : 'closed')) stopRoof(null);
  const roofChip = !rel.roof ? [t('common.not_released'), 'warn']
    : moving && !['closed', 'unknown'].includes(r.pos) ? [t('roof.chip.moving_open'), 'err']
    : r.block === 'unit_io' ? [t('roof.chip.io'), 'err']
    : noIgn ? [t('roof.chip.no_ignition'), 'warn']
    : moving ? [t(r.direction_verified ? 'roof.chip.blocked_driving' : 'roof.chip.blocked_driving_all'), 'warn']
    : r.block === 'timeout' ? [t('roof.chip.timeout'), 'warn']
    : r.pos === 'moving' ? [t('roof.chip.running'), 'accent']
    : r.afterrun ? [t('roof.chip.afterrun'), 'accent']
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
  const running = !!S.roof.auto;
  $('rf-stop').hidden = !running;
  $('rf-open').hidden = $('rf-close').hidden = running;
  document.querySelectorAll('.rf-how').forEach(n => { n.textContent = t(r.tap ? 'roof.tap' : 'roof.hold'); });
  text('rf-note', t(r.tap ? 'roof.note_tap' : 'roof.note'));

  // Einstellungen
  const cfg = st.settings;
  if (document.activeElement !== $('st-bright')) { $('st-bright').value = cfg.brightness; set('st-bright-val', fmt(cfg.brightness, '%')); }
  $('st-theme').querySelectorAll('button').forEach(b => b.setAttribute('aria-pressed', String(b.dataset.v === cfg.theme)));
  $('st-idle').querySelectorAll('button').forEach(b => b.setAttribute('aria-pressed', String(+b.dataset.v === cfg.idle_min)));
  $('st-lang').querySelectorAll('button').forEach(b => b.setAttribute('aria-pressed', String(b.dataset.v === LANG)));
  if (cfg.language && cfg.language !== LANG) loadLanguage(cfg.language);
  text('st-note', cfg.backlight_control === 'none' ? t('set.theme_note') + ' ' + t('set.brightness_na') : t('set.theme_note'));

  renderExpert();
  if (S.page === 'overview') document.querySelectorAll('.tile .big').forEach(fitLine);
}

function renderOfflineValues() {
  for (const id of ['ov-fridge', 'ov-heat', 'ov-fresh', 'ov-roof', 'fr-temp', 'fr-target', 'ht-phase', 'ht-level', 'wa-fresh', 'wa-waste', 'rf-pos'])
    set(id, '–');
  for (const id of ['ov-fridge-sub', 'ov-heat-sub', 'fr-note', 'ht-note']) text(id, '–');
  for (const id of ['ov-fridge-chip', 'ov-heat-chip', 'ov-waste-chip', 'ov-roof-chip', 'fr-chip', 'ht-chip', 'wa-waste-chip', 'rf-chip'])
    chip(id, t('common.no_data'), 'warn');
}

/* Inverter really on: not on shore, inverting state, and the mode allows it.
   In "Nur Laden" (1) and "Aus" (4) the inverter is off whatever State reports. */
function inverterOn() {
  const state = vic('vebus.state'), mode = vic('vebus.mode'), active = vic('vebus.active_input');
  const shore = active != null && active !== 240;
  return !shore && mode !== 1 && mode !== 4 && [1, 9, 10].includes(state);
}

/* What the battery is actually doing, from the Victron values (null = no data). */
function batteryStatus() {
  if (!(S.vic && S.vic.connected)) return null;
  const bw = vic('battery_w'), pv = vic('pv_w'), alt = vic('alternator_w'), state = vic('vebus.state'), mode = vic('vebus.mode');
  const active = vic('vebus.active_input'), shore = active != null && active !== 240;
  if (bw == null) return null;
  let key;
  if (shore && [3, 4, 6, 7].includes(state)) key = 'charging_shore';
  else if (shore && state === 5) key = 'full_shore';
  else if (shore && [8, 11].includes(state) && Math.abs(bw) < 10) key = 'passthru';
  else if (bw > 5 && alt > 5) key = 'charging_alt';
  else if (bw > 5 && pv > 0) key = 'charging_solar';
  else if (bw > 5) key = 'charging';
  else if (bw < -5 && inverterOn()) key = 'inverter';
  else if (bw < -5) key = 'discharging';
  else key = 'idle';
  const status = t('bat.status.' + key);
  return {key, charging: bw > 5, text: mode === 4 ? t('bat.status.mp_off', {status}) : status};
}

function renderEnergy() {
  const has = S.vic && S.vic.connected;
  const soc = vic('soc'), v = vic('battery_v'), a = vic('battery_a'), bw = vic('battery_w');
  const pv = vic('pv_w'), alt = vic('alternator_w'), dc = vic('dc_system_w');
  const outP = vic('vebus.out_p'), state = vic('vebus.state'), active = vic('vebus.active_input');
  const shore = active != null && active !== 240;
  const inv = state === 0 ? 0 : outP;  // 230 V consumers: from the battery or passed through from shore
  const socCls = soc == null ? '' : soc < 15 ? 'err' : soc < 30 ? 'warn' : 'ok';
  const st = batteryStatus();

  // Overview tile: battery graphic + key info
  const ttg = vic('time_to_go_s');
  const ttgText = ttg == null || !(bw < -5) ? null
    : ttg >= 3600 ? t('ov.ttg_h', {h: fmt(ttg / 3600, '', ttg < 36000 ? 1 : 0)}) : t('ov.ttg_min', {m: Math.round(ttg / 60)});
  // One battery graphic for every page: fill = SoC, colour by level, stripes + bolt while charging
  for (const p of ['ov', 'en']) {
    set(p + '-soc', soc == null ? '–' : fmt(soc, '%'));
    $(p + '-batt-fill').style.width = (soc ?? 0) + '%';
    $(p + '-batt-fill').className = 'batt-fill ' + (soc == null ? 'none' : socCls);
    $(p + '-batt').classList.toggle('charging', !!st?.charging);
  }
  const statusText = st ? t('bat.status.' + (st.key === 'inverter' ? 'discharging' : st.key)) : has ? '–' : t('common.no_victron');
  text('ov-bat-status', statusText);
  set('ov-bat-sub', !has || bw == null ? '' : ttgText ? t('ov.sub_ttg', {w: fmt(bw, 'W', 0, true), t: ttgText}) : `${fmt(bw, 'W', 0, true)} · ${fmt(v, 'V', 1)}`);
  // Chips: plain "240 W" (no small unit style); on shore power one chip says it all
  const plain = (x, unit, d = 0) => x == null ? '–' : `${fmt(x, '', d)} ${unit}`;
  const chips = [];
  if (has) {
    // Charging sources in one chip, unit once at the end: "Fahrt 450 · Solar 120 W"
    const src = [];
    if (alt > 5) src.push([t('ov.src_alt'), alt]);
    if (pv > 0) src.push([t('ov.src_solar'), pv]);
    if (src.length) chips.push(['ok', src.map(([n, w]) => `${n} ${fmt(w)}`).join(' · ') + ' W']);
    const limit = vic('vebus.current_limit');
    if (shore) chips.push(['ok', limit != null ? t('ov.chip_shore_lim', {v: plain(vic('vebus.in_v'), 'V'), a: plain(limit, 'A')}) : t('ov.chip_shore', {v: plain(vic('vebus.in_v'), 'V')})]);
    else chips.push(inverterOn() ? ['accent', t('ov.chip_ac_inv')] : ['', t('ov.chip_ac_off')]);
  }
  $('ov-chips').innerHTML = chips.map(([c, x]) => `<span class="chip ${c}">${x}</span>`).join('');
  text('en-soc-sub', ttgText ? `${statusText} · ${t('en.ttg', {t: ttgText})}` : statusText);
  set('en-v', fmt(v, 'V', 1)); set('en-a', fmt(a, 'A', 1, true)); set('en-dc', fmt(dc, 'W'));

  // Charging sources
  // Who charges: shore power -> MultiPlus, sun -> MPPT, driving -> Orion XS
  const vbDc = vic('vebus.dc_w'), shoreW = shore && vbDc != null ? Math.max(0, vbDc) : 0;
  const total = [shoreW, pv, alt].reduce((s, x) => s + (x > 0 ? x : 0), 0);
  set('en-in', !has || bw == null ? '–' : total > 5 ? fmt(total, 'W') : t('en.charging_off'));
  // Phase name (bulk, absorption, float ...) and power; a device that delivers nothing is "aus"
  const src = (phase, w) => {
    if (phase == null && w == null) return '–';
    const watts = w > 5 ? ' · ' + fmt(w, 'W') : '';
    if (phase === 2) return t('phase.2');
    if (phase >= 3 && phase <= 7) return t('phase.' + phase) + watts;
    return watts ? t('phase.on') + watts : t('phase.0');
  };
  set('en-shore', !has || state == null ? '–' : shore ? src(state, shoreW) : t('en.no_shore'));
  set('en-solar', src(vic('solar_state'), pv)); set('en-alt', src(vic('alt_state'), alt));
  set('en-starter', fmt(vic('starter_v'), 'V', 1));

  // MultiPlus: what it does now, switch, 230 V side
  const overload = vic('vebus.alarm_overload');
  const mode = vic('vebus.mode'), chargerOnly = mode === 1 && !shore;
  chip('en-mp-chip', state == null ? t('common.no_data') : overload === 2 ? t('en.alarm_overload') : chargerOnly ? t('mp.chip.0') : t('mp.chip.' + state),
    state == null ? 'warn' : overload === 2 || state === 2 ? 'err' : state === 0 ? '' : 'ok');
  text('en-mp-state', state == null ? (has ? '–' : t('common.no_victron'))
    : mode === 1 ? (shore ? t('mp.charger_only_shore', {desc: t('vebus.desc.' + state)}) : t('mp.charger_only'))
    : t('vebus.desc.' + state));
  renderModeSwitch();
  set('en-inv', fmt(inv, 'W'));
  const outS = vic('vebus.out_s'), load = S.vic?.load_pct ?? null;
  set('en-load', load != null ? `${fmt(load, '%')} · ${fmt(outS, 'VA')}` : '–');
  $('en-load-bar').className = 'bar' + (overload === 2 ? ' err' : load > 80 ? ' warn' : '');
  $('en-load-bar').firstElementChild.style.width = (overload === 2 ? 100 : load || 0) + '%';
  set('en-alarm', overload == null ? '–' : overload === 2 ? `<span style="color:var(--err)">${t('en.alarm_overload')}</span>` : t('en.alarm_none'));

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

function renderNet() {
  const d = netDraft(), c = S.net?.config;
  const enabled = !!d && S.online;
  $('x-eth-mode').querySelectorAll('button').forEach(b => {
    b.setAttribute('aria-pressed', String(d?.eth.mode === b.dataset.v)); b.disabled = !enabled;
  });
  document.querySelectorAll('.fld').forEach(b => {
    const f = b.dataset.f, v = fieldGet(f);
    const staticOnly = f.startsWith('eth.') && d?.eth.mode !== 'static';
    b.disabled = !enabled || staticOnly;
    b.textContent = f === 'wifi.psk' ? (v ? '••••••••' : c?.wifi.psk_set ? t('x.net.psk_set') : t('x.net.psk_unset'))
      : staticOnly ? '–' : (v === '' ? '–' : String(v));
    const orig = !c ? v : f === 'eth.dns' ? (c.eth.dns || []).join(', ') : f === 'wifi.psk' ? '' : c[f.split('.')[0]][f.split('.')[1]];
    b.classList.toggle('dirty', !!d && String(v) !== String(orig ?? ''));
  });
  const st = S.net?.status || {};
  text('x-eth-now', st.eth0 || t('x.net.no_link'));
  text('x-wlan-now', (st.wlan0 || t('x.net.no_link')) + ' · Victron ' + (S.vic?.connected ? t('x.conn.online') : t('x.conn.offline')));
  const armed = Date.now() < netArm;
  $('x-net-apply').textContent = armed ? t('x.confirm') : t('x.net.apply');
  $('x-net-apply').classList.toggle('armed', armed);
  $('x-net-apply').disabled = !enabled;
  text('x-net-note', !S.net ? t('x.net.loading') : !S.net.config ? t('x.net.unavailable') : t('x.net.hint'));
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

  renderNet();

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
/* Language: texts come from i18n/<lang>.json; the choice is a setting stored by camperd. */
let langLoading = null;
function applyTexts() {
  document.documentElement.lang = LANG === 'de' ? 'de-CH' : 'en';
  document.querySelectorAll('[data-t]').forEach(n => { n.textContent = t(n.dataset.t); });
  document.querySelectorAll('[data-t-aria]').forEach(n => n.setAttribute('aria-label', t(n.dataset.tAria)));
  for (const id of ['x-di', 'x-do', 'x-ai', 'x-pins']) $(id).innerHTML = '';
  buildExpert();
  text('title', t('page.' + S.page));
  text('back-label', t('page.' + (PAGES[S.page].back || 'overview')));
  tickClock();
}
function loadLanguage(lang) {
  if (langLoading === lang) return Promise.resolve();
  langLoading = lang;
  return fetch(`i18n/${lang}.json${ASSET_V}`).then(r => r.json()).then(dict => {
    T = dict; LANG = lang; langLoading = null;
    applyTexts();
    render();
  });
}

loadLanguage('de').then(() => {
  go('overview');
  connect();
});
})();
