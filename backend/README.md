# RemindMe — Google Apps Script + Sheet setup

The device POSTs each habit event (fired / completed / snoozed / skipped /
boot / config_changed) to a Google Apps Script web app, which appends a row
to a Google Sheet and publishes a short ntfy notification. The same web app
exposes `?summary=today` and `?summary=14d` endpoints that the device GETs
to populate its 14-day stats screen.

## One-time setup

1. **Create a Google Sheet.** Any name. The script will create a tab named
   `Events` with the right headers on first write — you don't need to set
   them up by hand.

2. **Open the Apps Script editor.** In the Sheet, choose
   *Extensions → Apps Script*. Paste the contents of [Code.gs](Code.gs) into
   the editor (replacing the boilerplate). Save (Ctrl/Cmd + S) and rename
   the project to "RemindMe".

3. **Add the ntfy topic as a Script Property.**
   *Project Settings (gear icon) → Script properties → Add property:*

   | Name        | Value                                  |
   |-------------|----------------------------------------|
   | `NTFY_TOPIC`| `remindme-yourname-7f3a` (your choice) |
   | `NTFY_BASE` | `https://ntfy.sh` (only for self-hosted)|

   Any string of `[A-Za-z0-9_-]` works as a topic. Pick something hard to
   guess — anyone with the topic name can subscribe.

4. **Deploy as a web app.**
   *Deploy → New deployment → ⚙ Web app.*

   - Description: `RemindMe v1`
   - Execute as: **Me**
   - Who has access: **Anyone with the link**

   Click *Deploy*. On the first deploy you'll be asked to authorise the
   script — accept (it needs `Sheets` + `UrlFetch`).

   Copy the `/exec` URL it shows you.

5. **Paste the URL into the device.** Open `http://remindme.local/` →
   *Backend* tab → paste the URL into "Apps Script URL". Save & reboot.

   The first POST from the device will take ~3 s while Google's TLS
   handshake happens; subsequent ones are sub-second.

## Sheet schema

The script writes one row per event with these columns (auto-created):

| Column         | Example                              | Notes                          |
|----------------|--------------------------------------|--------------------------------|
| `ts_iso`       | `2026-05-01T14:23:11Z`               | UTC, from device clock         |
| `received_iso` | `2026-05-01T14:23:13Z`               | When Apps Script saw it        |
| `device_id`    | `remindme-7f3a`                      | From device config             |
| `habit`        | `water` \| `walk` \| `stretch` \| `system` |                          |
| `action`       | `fired` \| `completed` \| `snoozed` \| `skipped` \| `config_changed` \| `boot` | |
| `amount`       | `250` (water) / `5` (stretch min) / `10` (walk min) | Empty if no `meta.amount` |
| `duration_min` | (optional)                           |                                |
| `source`       | `esp32`                              |                                |
| `raw_json`     | full event JSON                      | For audit / future migrations  |

The aggregation endpoints derive the daily/14-day summaries by simple
GROUP BY on `(device_id, date(ts_iso), habit, action)`. There is no
separate rollup table — this scales fine for personal use (a few hundred
rows per month per device).

## ntfy notifications

Each POST triggers `UrlFetchApp.fetch(base + '/' + topic, ...)` with a
short message like `💧 water · completed`. Subscribe from your phone:

- iOS / Android: install [ntfy](https://ntfy.sh/), tap **Add subscription**,
  enter `https://ntfy.sh/<your-topic>`.
- Browser: open `https://ntfy.sh/<your-topic>` and click *Subscribe to
  notifications*.

If you don't want notifications, leave `NTFY_TOPIC` unset and the script
silently skips the publish call.

## Updating the script

Edit `Code.gs`, then *Deploy → Manage deployments → ✏ Edit → New version
→ Deploy*. **The `/exec` URL stays the same** so the device doesn't need
to be reflashed.

## Troubleshooting

- **Device reports `POST 401` or `403`** → re-authorise the script
  (deploy, then visit the `/exec` URL once in your browser to trigger the
  consent screen).
- **No rows appearing in the sheet** → check the Apps Script "Executions"
  log; runtime errors show up there.
- **ntfy notifications stop arriving** → topic typos are the usual cause;
  double-check the `NTFY_TOPIC` Script Property matches what your phone
  is subscribed to.
- **Want to inspect a specific payload** → the `raw_json` column has the
  full event as the device sent it.
