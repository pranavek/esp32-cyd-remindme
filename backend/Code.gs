/**
 * RemindMe — Google Apps Script backend
 *
 * Endpoints
 *   POST /exec               body = { device_id, events: [{ts, habit, action, meta}] }
 *                            → appends one row per event to the Events sheet
 *                              and publishes a short ntfy notification.
 *   GET  /exec?summary=today&device_id=...
 *                            → { stretch_min, water_ml, walk_min, ... }
 *   GET  /exec?summary=14d&device_id=...
 *                            → { days: [ { date, stretch_min, water_ml, walk_min, ... }, ... ] }
 *
 * Setup
 *   1. Make a Google Sheet with a tab named "Events" and headers matching
 *      ROW_HEADERS below. (Run setupHeaders_() once from the editor or
 *      the menu; it writes the headers into row 1.)
 *   2. Extensions → Apps Script → paste this file.
 *   3. Project Settings → Script properties → add NTFY_TOPIC=<your-topic>.
 *      Optional: NTFY_BASE=https://ntfy.sh   (for self-hosted, set the URL).
 *   4. Deploy → New deployment → Web app → Execute as Me, access Anyone with link.
 *   5. Copy the /exec URL into the device's web UI under "Apps Script URL".
 *
 * Notes
 *   - Aggregation uses simple sums, not a separate rollup table. With ~50
 *     events/day per device this is fine for years; if it ever balloons,
 *     bucket events by month into multiple sheets.
 *   - Quoting for ntfy: messages are <= 80 chars; topic names are URL-safe.
 */

const SHEET_NAME   = 'Events';
const ROW_HEADERS  = [
  'ts_iso', 'received_iso', 'device_id', 'habit', 'action',
  'amount', 'duration_min', 'source', 'raw_json'
];

// ─── Public endpoints ────────────────────────────────────────────────────
function doPost(e) {
  try {
    const body = JSON.parse(e.postData.contents || '{}');
    const events = Array.isArray(body.events) ? body.events : [body];
    const deviceId = body.device_id || (events[0] && events[0].device_id) || 'unknown';

    const rows = events.map(ev => buildRow_(ev, deviceId));
    if (rows.length) appendRows_(rows);

    publishNtfy_(events, deviceId);

    return jsonOut_({ ok: true, written: rows.length });
  } catch (err) {
    return jsonOut_({ error: String(err && err.message || err) }, 500);
  }
}

function doGet(e) {
  try {
    const summary = (e.parameter.summary || '').toLowerCase();
    const deviceId = e.parameter.device_id || '';
    if (summary === 'today') return jsonOut_(summaryToday_(deviceId));
    if (summary === '14d')   return jsonOut_(summary14d_(deviceId));
    return jsonOut_({ error: 'summary must be today|14d' }, 400);
  } catch (err) {
    return jsonOut_({ error: String(err && err.message || err) }, 500);
  }
}

// ─── Sheet plumbing ──────────────────────────────────────────────────────
function eventsSheet_() {
  const ss = SpreadsheetApp.getActiveSpreadsheet();
  let sh = ss.getSheetByName(SHEET_NAME);
  if (!sh) {
    sh = ss.insertSheet(SHEET_NAME);
    sh.appendRow(ROW_HEADERS);
    sh.setFrozenRows(1);
  }
  return sh;
}

function setupHeaders_() {
  const sh = eventsSheet_();
  sh.getRange(1, 1, 1, ROW_HEADERS.length).setValues([ROW_HEADERS]);
  sh.setFrozenRows(1);
}

function appendRows_(rows) {
  const sh = eventsSheet_();
  sh.getRange(sh.getLastRow() + 1, 1, rows.length, rows[0].length).setValues(rows);
}

function buildRow_(ev, deviceId) {
  const ts = ev.ts || '';
  const meta = ev.meta || {};
  const amount = numberOr_(meta.amount, '');
  const dur    = numberOr_(meta.duration_min, '');
  return [
    ts,
    new Date().toISOString(),
    ev.device_id || deviceId || '',
    ev.habit  || '',
    ev.action || '',
    amount,
    dur,
    'esp32',
    JSON.stringify(ev),
  ];
}

// ─── Aggregations ────────────────────────────────────────────────────────
// Today's accumulated counters. Done events drive the running totals; the
// device persists its own counters too, so this serves as a cross-check
// and as the source of truth for the 14-day view.
function summaryToday_(deviceId) {
  const tz = Session.getScriptTimeZone();
  const today = Utilities.formatDate(new Date(), tz, 'yyyy-MM-dd');
  const rows = readRows_(deviceId);
  const acc = { stretch_min: 0, water_ml: 0, walk_min: 0,
                completed: 0, snoozed: 0, skipped: 0, fired: 0 };
  rows.forEach(r => {
    const day = (r.ts_iso || '').slice(0, 10);
    if (day !== today) return;
    if (r.action === 'completed') {
      acc.completed++;
      if (r.habit === 'stretch') acc.stretch_min += +r.amount || 5;
      if (r.habit === 'water')   acc.water_ml    += +r.amount || 250;
      if (r.habit === 'walk')    acc.walk_min    += +r.amount || 10;
    } else if (r.action === 'snoozed') acc.snoozed++;
    else if   (r.action === 'skipped') acc.skipped++;
    else if   (r.action === 'fired')   acc.fired++;
  });
  acc.date = today;
  acc.device_id = deviceId;
  return acc;
}

function summary14d_(deviceId) {
  const tz = Session.getScriptTimeZone();
  const days = [];
  const today = new Date();
  for (let i = 13; i >= 0; --i) {
    const d = new Date(today);
    d.setDate(d.getDate() - i);
    days.push(Utilities.formatDate(d, tz, 'yyyy-MM-dd'));
  }
  const byDay = {};
  days.forEach(d => byDay[d] = { date: d, stretch_min: 0, water_ml: 0, walk_min: 0,
                                 completed: 0, snoozed: 0, skipped: 0, fired: 0 });

  const rows = readRows_(deviceId);
  rows.forEach(r => {
    const day = (r.ts_iso || '').slice(0, 10);
    if (!byDay[day]) return;
    const slot = byDay[day];
    if (r.action === 'completed') {
      slot.completed++;
      if (r.habit === 'stretch') slot.stretch_min += +r.amount || 5;
      if (r.habit === 'water')   slot.water_ml    += +r.amount || 250;
      if (r.habit === 'walk')    slot.walk_min    += +r.amount || 10;
    } else if (r.action === 'snoozed') slot.snoozed++;
    else if   (r.action === 'skipped') slot.skipped++;
    else if   (r.action === 'fired')   slot.fired++;
  });
  return { device_id: deviceId, days: days.map(d => byDay[d]) };
}

function readRows_(deviceId) {
  const sh = eventsSheet_();
  const last = sh.getLastRow();
  if (last < 2) return [];
  const data = sh.getRange(2, 1, last - 1, ROW_HEADERS.length).getValues();
  const out = [];
  for (let i = 0; i < data.length; ++i) {
    const row = data[i];
    const o = {};
    for (let j = 0; j < ROW_HEADERS.length; ++j) o[ROW_HEADERS[j]] = row[j];
    if (deviceId && o.device_id !== deviceId) continue;
    out.push(o);
  }
  return out;
}

// ─── ntfy publish ────────────────────────────────────────────────────────
function publishNtfy_(events, deviceId) {
  const props = PropertiesService.getScriptProperties();
  const topic = props.getProperty('NTFY_TOPIC');
  if (!topic) return;
  const base = props.getProperty('NTFY_BASE') || 'https://ntfy.sh';

  const msg = events.map(ev => emojiFor_(ev.habit) + ' ' + (ev.habit || '') + ' · ' + (ev.action || ''))
                    .join('  |  ');
  const subject = deviceId ? ('RemindMe ' + deviceId) : 'RemindMe';

  try {
    UrlFetchApp.fetch(base + '/' + encodeURIComponent(topic), {
      method: 'post',
      payload: msg.slice(0, 200),
      headers: { 'Title': subject, 'Tags': 'recycle' },
      muteHttpExceptions: true,
    });
  } catch (err) {
    Logger.log('ntfy publish failed: ' + err);
  }
}

function emojiFor_(habit) {
  if (habit === 'water')   return '💧';
  if (habit === 'walk')    return '🚶';
  if (habit === 'stretch') return '🤸';
  if (habit === 'system')  return '⚙️';
  return '•';
}

// ─── Helpers ─────────────────────────────────────────────────────────────
function jsonOut_(obj, code) {
  // Apps Script web apps don't support arbitrary status codes — they always
  // return 200. We embed `error` in the body for the device to inspect.
  return ContentService.createTextOutput(JSON.stringify(obj))
    .setMimeType(ContentService.MimeType.JSON);
}

function numberOr_(v, fallback) {
  if (v === undefined || v === null || v === '') return fallback;
  const n = Number(v);
  return isNaN(n) ? fallback : n;
}
