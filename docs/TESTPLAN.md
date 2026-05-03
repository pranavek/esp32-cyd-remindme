# RemindMe — bring-up test plan

Run through this on real hardware after the first flash. Tick boxes as you
go and copy the result into the PR description for the relevant change.

## Hardware (one-time)

- [ ] **Both USB ports** plugged in turn while running `pio device monitor -b 250000`. Document which port carries serial; record on the back of the case.
- [ ] Display lights up. Boot splash reads `RemindMe / connecting…`.
- [ ] **Color check**: replace the splash temporarily with `tft.fillScreen(TFT_RED)` (or just trust the `WiFi unavailable` red text) — red is red, not blue. If it's wrong, `-DTFT_RGB_ORDER=0` is missing.
- [ ] **Yellow check**: the amber accent on `Setup mode` is amber, not cyan. If cyan, same fix.

## First boot — Wi-Fi provisioning

- [ ] Out-of-box: device shows `Setup mode` and broadcasts an AP named `RemindMe-Setup`.
- [ ] AP is visible from a phone within ~30 s.
- [ ] Joining the AP triggers a captive portal automatically.
- [ ] Entering valid Wi-Fi credentials → device reboots → joins target Wi-Fi → renders Today screen.

## mDNS + web UI

- [ ] `http://remindme.local/` resolves and loads the UI from a phone on the same Wi-Fi.
- [ ] Same URL works from a laptop on the same Wi-Fi.
- [ ] If mDNS fails on your phone, the IP shown on the device's status strip works directly (`http://192.168.x.y/`).
- [ ] All six tabs (`Status`, `Reminders`, `Goals`, `Backend`, `Wi-Fi`, `System`) render.

## Touch

- [ ] First boot triggers the 4-corner calibration UI.
- [ ] After calibration, taps in the corners hit the right tap zones (try the carousel toggle: tap anywhere → screen changes from Today to Stats14).
- [ ] Long-press (≥ 800 ms) toggles the diagnostic overlay; another tap dismisses it.
- [ ] **Re-calibration**: from the web UI, *System → Recalibrate touch* → device reboots → cal UI runs again.

## Reminder lifecycle

- [ ] On the *Status* tab, *Test reminder → Fire water*. Popup appears within ~2 s. RGB LED pulses blue.
- [ ] Tap **Done** on the popup. Popup closes; LED flashes green; today's water counter increments.
- [ ] Tap **Snooze** on a fired reminder. LED flashes cyan; popup closes; same reminder fires again ~10 minutes later (or set the snooze interval lower for testing).
- [ ] Tap **Skip** on a fired reminder. LED flashes red; popup closes; today's skipped counter increments.
- [ ] Auto-snooze: trigger a reminder, leave the popup untouched for ~90 s. It auto-snoozes.

## Quiet hours

- [ ] Set quiet hours to span the current local time (e.g. start = current hour, end = next hour). Wait for the next interval. **No popup fires.**
- [ ] Set quiet hours outside the current local time. Reminders fire normally.
- [ ] *Reminders → Quiet hours → Suppress reminders during quiet hours* off → reminders fire even within the configured window.

## Carousel

- [ ] With no popup open, the screen auto-cycles between Today and Stats14 every ~10 s.
- [ ] Tap the screen → carousel toggles immediately and resets the timer.
- [ ] Trigger a reminder → carousel pauses for the duration of the popup.
- [ ] Dismiss the popup → carousel resumes from where it paused.

## Persistence

- [ ] Log a few habit completions. Power-cycle the device.
- [ ] Today's counters are still there post-reboot (resumed from `/state.json`).
- [ ] Edit `/config.json` over the web UI (e.g. change `goals.water_ml`). Save → device reboots → new value is visible.
- [ ] Schema migration: SSH/serial-edit `/config.json` to set `schema_version=0`. Reboot. The migration chain runs and bumps it back to `1`. Counters intact.

## Google Sheets backend

- [ ] Deploy `backend/Code.gs` per [backend/README.md](../backend/README.md). Paste the `/exec` URL into the device.
- [ ] After saving, trigger any reminder action. Within ~5 s a new row appears in the `Events` tab of the sheet with the right `device_id`, `habit`, `action`, and `ts_iso`.
- [ ] `GET https://script.google.com/.../exec?summary=today&device_id=<id>` returns this device's today aggregates as JSON.
- [ ] `GET ...?summary=14d&device_id=<id>` returns 14 days of aggregated rows.

## ntfy

- [ ] Subscribe to your topic on the [ntfy app](https://ntfy.sh/) on your phone.
- [ ] Trigger any reminder action on the device. Notification arrives within ~5 s of the sheet row appearing.
- [ ] Notification message is short and readable (e.g. `💧 water · completed`).

## Offline resilience

- [ ] **Pull network** (router off / cable out / phone hotspot off).
- [ ] Trigger 3-5 reminders + Done/Snooze/Skip them. Status strip's queue indicator goes amber. Today screen still updates.
- [ ] **Restore network.** Within ~30 s the queue indicator goes green and all enqueued events appear in the sheet, in order.
- [ ] Pull network, power-cycle, restore network. Pending events from before the reboot still drain.

## Diagnostics

- [ ] Long-press on Today/Stats → diagnostic overlay shows: WiFi SSID, RSSI, IP, mDNS, queue depth, last sync, free heap, uptime, fw version.
- [ ] Tap to dismiss. Carousel resumes.
- [ ] `GET /api/state` returns the same data as the overlay.

## Backlight

- [ ] Cover the LDR (the small bare component near the screen) → backlight dims, then the screen sleeps after a few seconds.
- [ ] Uncover → screen wakes, backlight ramps back up.
- [ ] Tap the screen while asleep → screen wakes regardless of LDR.

## Web UI smoke test

- [ ] *Status → Quick log → Stretch +5m* → row appears in sheet with `amount=5`.
- [ ] *System → Reboot* → device restarts (RSSI changes, uptime resets).
- [ ] *Wi-Fi → Forget Wi-Fi* → device reboots into setup AP. Re-provision.
