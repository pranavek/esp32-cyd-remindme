// RemindMe local web UI. Vanilla JS — no build step.
(() => {
  const $  = (q, root=document) => root.querySelector(q);
  const $$ = (q, root=document) => Array.from(root.querySelectorAll(q));
  let cfg = null;

  const toast = (msg, ms=2200) => {
    const t = $('#toast'); t.textContent = msg; t.hidden = false;
    clearTimeout(toast._t);
    toast._t = setTimeout(() => { t.hidden = true; }, ms);
  };

  const fmtUptime = s => {
    s = +s|0;
    if (s < 60) return s + 's';
    if (s < 3600) return (s/60|0) + 'm ' + (s%60) + 's';
    return (s/3600|0) + 'h ' + ((s%3600)/60|0) + 'm';
  };

  const get = (obj, path) => path.split('.').reduce((o,k) => (o ? o[k] : undefined), obj);
  const set = (obj, path, v) => {
    const ks = path.split('.'); const last = ks.pop();
    const tgt = ks.reduce((o,k) => (o[k] = o[k] || {}), obj);
    tgt[last] = v;
  };

  // ─── Config load/save ─────────────────────────────────────────────────
  async function loadConfig() {
    const r = await fetch('/api/config'); cfg = await r.json();
    $('#device-line').textContent = `${cfg.device_id} · ${cfg.mdns_hostname}.local`;
    $('#mdns-host').textContent   = cfg.mdns_hostname;
    $$('input[data-key]').forEach(el => {
      const v = get(cfg, el.dataset.key);
      if (el.type === 'checkbox') el.checked = !!v;
      else if (v !== undefined) el.value = v;
    });
  }

  async function saveConfig() {
    const patch = {};
    $$('input[data-key]').forEach(el => {
      let v;
      if (el.type === 'checkbox') v = el.checked;
      else if (el.type === 'number') v = +el.value;
      else v = el.value;
      set(patch, el.dataset.key, v);
    });
    const r = await fetch('/api/config', {
      method:'POST', headers:{'Content-Type':'application/json'},
      body: JSON.stringify(patch)
    });
    const j = await r.json().catch(() => ({}));
    if (!r.ok) { toast('Save failed: ' + (j.error || r.status), 4000); return; }
    toast('Saved · rebooting…', 6000);
    setTimeout(() => location.reload(), 4000);
  }

  // ─── Status pane ─────────────────────────────────────────────────────
  async function refreshState() {
    try {
      const r = await fetch('/api/state'); const j = await r.json();
      $('#wifi-ssid').textContent = j.wifi_ssid || '—';
      $('#wifi-rssi').textContent = (j.wifi_rssi || 0) + ' dBm';
      $('#wifi-ip').textContent   = j.ip || '—';
      $('#s-fw').textContent      = j.fw_version || '?';
      $('#s-up').textContent      = fmtUptime(j.uptime_s);
      $('#s-heap').textContent    = (j.heap || 0).toLocaleString() + ' B';
      $('#s-q').textContent       = j.queue_depth ?? '—';
      $('#s-sync').textContent    = j.last_sync_ms_ago
        ? Math.round(j.last_sync_ms_ago/1000) + 's ago' : 'never';
      $('#s-err').textContent     = j.last_error || '—';

      if (j.today) {
        const g = cfg ? cfg.goals : {stretch_min:0,water_ml:0,walk_min:0};
        $('#t-stretch').textContent = `${j.today.stretch_min} / ${g.stretch_min} min`;
        $('#t-water').textContent   = `${j.today.water_ml} / ${g.water_ml} ml`;
        $('#t-walk').textContent    = `${j.today.walk_min} / ${g.walk_min} min`;
      }
    } catch (e) { /* offline — keep last values */ }
  }

  // ─── Tabs ─────────────────────────────────────────────────────────────
  $$('.tabs button').forEach(b => b.addEventListener('click', () => {
    $$('.tabs button').forEach(x => x.classList.toggle('active', x === b));
    $$('section[data-pane]').forEach(s =>
      s.classList.toggle('active', s.dataset.pane === b.dataset.tab));
  }));

  // ─── Buttons ──────────────────────────────────────────────────────────
  $$('[data-save]').forEach(b => b.addEventListener('click', saveConfig));
  $$('[data-act]').forEach(b => b.addEventListener('click', async () => {
    await fetch('/api/event', {
      method:'POST', headers:{'Content-Type':'application/json'},
      body: JSON.stringify({habit:b.dataset.act, action:'completed'})
    });
    toast(`Logged ${b.dataset.act}`);
    setTimeout(refreshState, 400);
  }));
  $$('[data-fire]').forEach(b => b.addEventListener('click', async () => {
    await fetch('/api/event', {
      method:'POST', headers:{'Content-Type':'application/json'},
      body: JSON.stringify({habit:b.dataset.fire, action:'fire'})
    });
    toast(`Triggered ${b.dataset.fire} reminder`);
  }));
  $('[data-action="wifi-reset"]').addEventListener('click', async () => {
    if (!confirm('Forget Wi-Fi credentials and reboot into setup AP?')) return;
    await fetch('/api/wifi/reset', {method:'POST'});
    toast('Rebooting into setup AP…', 4000);
  });
  $('[data-action="reboot"]').addEventListener('click', async () => {
    if (!confirm('Reboot the device?')) return;
    await fetch('/api/reboot', {method:'POST'});
    toast('Rebooting…', 4000);
  });
  $('[data-action="touch-recal"]').addEventListener('click', async () => {
    if (!confirm('Recalibrate the touchscreen on next boot?')) return;
    await fetch('/api/touch/recalibrate', {method:'POST'});
    toast('Will recalibrate on reboot', 4000);
  });
  $('[data-action="stats-refresh"]').addEventListener('click', async () => {
    await fetch('/api/stats/refresh', {method:'POST'});
    toast('Stats refresh queued');
  });

  // ─── Boot ─────────────────────────────────────────────────────────────
  loadConfig().then(refreshState);
  setInterval(refreshState, 5000);
})();
