# Open issues

Things that could change the code later. Delete an entry when it is settled.

## 2026-11-24: After the server ends a chat, the device does not close the connection

- Context: When the server ends a chat (the bridge's idle timeout, a goodbye, the daily limit), the WebSocket library marks the connection as closed and the device returns to idle. The library does not answer the server's close frame and does not close the connection. The bridge does not wait for the answer: its server (uvicorn) goes on at once and closes the connection itself 10 seconds later. This was read in the server's code on 2026-11-24 and not watched on the device. The firmware keeps its WebSocket until the next chat starts, and `WebsocketProtocol::CreateWebsocket` then frees it without stopping its transport first. This is not new: the firmware behaved this way before the close frame was added. The coder noticed it while adding the close frame for chats that the device ends.
- Open: If the next chat starts while the server still has the old connection open (with the bridge, in the 10 seconds after it ended the chat), data arriving at that moment could reach freed memory. This was not observed. Until the next chat, the memory of the old connection stays allocated; how much was not measured.
- For now: Unchanged. The bridge needs nothing from the firmware here: it takes the session length before it closes the connection and does not wait for the device.
- Needs: Decide whether the device should drop the connection as soon as the server closes it, as the MQTT transport does when the server says goodbye (`CloseAudioChannel(false)`). `WebsocketProtocol::DiscardWebsocket()` stops the transport before it frees the WebSocket. Test with a chat that the bridge ends, for example by saying `再见`.

## 2026-11-24: The close frame is untested while the server is sending, and on the stock variant

- Context: When the device ends a chat, it now sends a WebSocket close frame and then drops the connection at once, so that a server behind the Azure Container Apps ingress learns it without a 5-second delay. The frame carries no status code: the WebSocket library cannot send one. With the kids variant on the device on 2026-11-24, the bridge's session length matched the device's to within 0.1 seconds in four chats; before, the bridge logged the end of a session 6.5 to 11.5 seconds late. In those four chats the bridge sent no audio.
- Open: If data from the server is still unread when the connection is shut down, lwIP resets the connection instead of closing it, and the close frame can be lost. The bridge then learns 5 seconds late, as before. The stock variant shares the code and was built, but not run on the device.
- For now: Accepted: the worst case is the behavior before this change.
- Needs: Nothing unless session ends in the bridge's log are late again; then check this first.

## 2026-11-24: Six emotion names show the default face

- Context: The bridge now sends one of XiaoZhi's 21 emotion names with each reply. The kids display has a face for 14 of them.
- Open: `embarrassed`, `cool`, `relaxed`, `delicious`, `confident` and `silly` show the default face, like `neutral`.
- For now: Left as is; the handoff from the bridge calls the current faces acceptable.
- Needs: Decide whether these six should be mapped to eyes and mouths that already exist.

## 2026-11-24: The error for a server that never starts is untested

- Context: The bridge scales to zero outside its warm windows, and the firmware now waits up to `CONFIG_SERVER_START_TIMEOUT_SEC` (90 seconds) for it to start. The kids variant was tested on the device against the scaled-to-zero bridge on 2026-11-24. At boot the version check got its answer after 53 seconds, and the device went to idle without an error. A chat started from the console connected on the fifth handshake attempt, 59 seconds after it started, and was listening 2 seconds later, without an error. A chat started with the OK key connected after 52 seconds and stayed open although OK was pressed again during the wait. With the bridge awake a chat connected in 1.5 seconds, without retries. The `waiting_for_server` frame of `KUI DEMO` shows `服务器正在开机，请稍候...` on two lines. The free memory lost to the closed attempts came back once their connections had expired.
- Open: The error after about 100 seconds, for a server that accepts connections but never answers, was not produced.
- For now: That path relies on code review.
- Needs: Nothing unless such a server turns up; a test needs a stored WebSocket URL that points to one.

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
