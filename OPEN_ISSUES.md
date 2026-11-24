# Open issues

Things that could change the code later. Delete an entry when it is settled.

## 2026-11-24: Waiting for a server that starts on demand is untested on the device

- Context: The bridge now scales to zero outside its warm windows, and a cold start took 48 to 54 seconds on 2026-11-24, while the firmware gave up on the WebSocket handshake after 10 seconds and on the boot-time version check after 30. The firmware now waits up to `CONFIG_SERVER_START_TIMEOUT_SEC` (90 seconds): it connects again after each 10-second handshake wait and shows `正在等待服务器开机，请稍候...`, the version check waits up to 90 seconds, and OK is ignored while connecting. A script that connects the way the firmware does reached the scaled-to-zero bridge on 2026-11-24 after 53.8 seconds, on its sixth attempt. Both variants build.
- Open: None of it has run on the device: a chat started outside a warm window, a boot outside a window, the `waiting_for_server` frame of `KUI DEMO`, and the error after about 100 seconds when the server never answers.
- For now: The host test checks only that the new message can be drawn with the built-in font.
- Needs: A device test outside a warm window, at least 6 minutes after the bridge was last used.

## 2026-11-24: The volume keys wait while a chat connects

- Context: Every key press is scheduled onto the main task, and opening a chat blocks that task, for about a minute when the server has to start first. OK is now ignored while connecting.
- Open: UP and DOWN pressed during that time take effect only when the connection is made or fails, all at once.
- For now: Left as is.
- Needs: Decide whether the volume keys should act at once (which means touching the codec from the button task) or be ignored while connecting.

## 2026-11-24: Glyphs pushed by a server are untested on the kids variant

- Context: A live test on 2026-11-24 settled the rest of the first device check. In four chat sessions through a self-hosted server, the speaker's words were transcribed exactly, wrapped over four lines and scrolled; the pill (`我在听`, `请稍等`, `我在说`) and the mouth followed the speech; the assistant's words never appeared; the sound at 16 kHz was clear, with no clipped start or stutter; the volume keys moved the bar; the first frame was dark; OK stopped a long answer at once; and the server's two-minute idle close brought back the idle screen. That server pushes no glyphs.
- Open: How her words look when a server pushes glyphs (`glyph_push`) for characters that the built-in font lacks.
- For now: The kids display keeps a pushed batch until the next message and applies it only to her words.
- Needs: A test with a server that pushes glyphs, if one is ever used.

## 2026-11-24: `请稍等` can stay on screen when no answer follows

- Context: Review finding. Once her words arrive, the pill shows `请稍等` and the mascot looks up until the assistant starts to speak.
- Open: If a server sends her transcript but no reply (a failed or filtered response), the screen keeps asking her to wait although the device is listening.
- For now: Nothing. The next thing she says replaces the text. Not seen in the 2026-11-24 live test, where every utterance got an answer.
- Needs: Decide whether to return to the listening prompt after a timeout of about 10 seconds.

## 2026-11-24: A server alert hides her words until the chat ends

- Context: Review finding. System text has priority in the bubble and is cleared only when the device returns to idle.
- Open: An `alert` message from a server in the middle of a conversation covers her words for the rest of that conversation. The server this device uses does not send alerts.
- For now: Left as is.
- Needs: Decide whether system text should also be cleared when listening starts.

## 2026-11-24: Only the device screens were restyled, not the phone page

- Context: The request named the "Wi-Fi setup" screen. The device shows one in setup mode; the phone also shows a setup page, which belongs to the third-party component `78/esp-wifi-connect`.
- Open: Whether the phone page should be dark and themed too.
- For now: The device screen in setup mode is restyled. The phone page is unchanged.
- Needs: Say whether the phone page is wanted. It means keeping a patched copy of that component.

## 2026-11-24: The artwork is drawn by code

- Context: `scripts/kids_ui/gen_art.py` draws the sky and the mascot. No image generator was used (that needs an explicit request) and no third-party art, so the artwork is original.
- Open: Whether richer, illustrated art is wanted.
- For now: Code-drawn art, 237 KB in flash.
- Needs: Ask for generated or supplied artwork if wanted. The patch layout (stage, eyes, mouth, effects) takes any art of the same size.

## 2026-11-24: After a restart the device can end up in Wi-Fi setup

- Context: When the device restarts without a clean disconnect (switched off and on, or reset), the home access point does not let it back in at once. Measured on 2026-11-24: after a reset the access point first refused the association temporarily (`Association refused temporarily ... comeback time`), as access points with PMF (802.11w) do while a station still holds a protected association, and then ignored every authentication until 34 seconds after the reset (36 seconds after a power-on). The firmware then paused 10 seconds and scanned again, its next attempt connected at once, and the connection timer stopped at 45 and 48 seconds. The firmware gives up after 60 seconds and opens Wi-Fi setup mode, where it stays until it is restarted.
- Open: A child who switches the device off and on can land on the setup screen; the margin was 12 to 15 seconds.
- For now: Unchanged common behaviour (`WifiBoard::OnWifiConnectTimeout`, `CONNECT_TIMEOUT_SEC = 60`). A restart a few minutes later connects in under a second.
- Needs: Decide whether the kids variant should keep retrying the saved network instead of opening setup mode, or whether the access point's settings should change.

## 2026-11-24: The Wi-Fi setup page stores 2 dBm by accident

- Context: In `78/esp-wifi-connect` 3.3.3, saving the Advanced tab on a device without Wi-Fi settings stores the first entry of the transmit power list (8 = 2 dBm).
- Open: The cause is in the component. Only the kids variant works around it, and it cannot tell an accidental 2 dBm from a deliberate one.
- For now: The kids variant erases `wifi/max_tx_power` when it is 8, and creates the `wifi` namespace so that the page shows the real default.
- Needs: Decide whether to report it to the component's author, and whether the stock variant should get the same guard.

## 2026-11-24: The host tests already fail on `main`

- Context: `python -m unittest discover -s scripts/tests` has 41 failing or erroring entries on `main`, because the tests refer to boards that this repository removed.
- Open: The suite cannot serve as a gate.
- For now: The new `scripts/tests/test_kids_ui.py` and `scripts/tests/test_locales.py` pass on their own, and the 41 old entries are unchanged.
- Needs: Decide whether to prune or fix the old tests.
