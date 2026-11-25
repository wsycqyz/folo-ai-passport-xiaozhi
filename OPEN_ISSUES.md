# Open issues

Things that could change the code later. Delete an entry when it is settled.

## 2026-11-25: Short questions are sometimes misheard or get no answer, although the device records them whole

- Context: In the joint test with the bridge's session on 2026-11-25 a recorded child's voice was played on the PC's speakers next to the device. Two things went wrong. First, the start of a sentence was lost in 12 of 28 chats (你叫什么名字 became 叫什么名字 or 什么名字). That was a setting of the bridge: the service finds speech 150 to 350 ms after it begins and then keeps only `prefix_padding_ms` of audio from before that moment, and the bridge asked for 300 ms. With 800 ms for this device, 16 of 16 chats were complete. Second, and still open: with 800 ms in place the question 现在几点了 was played in 6 chats, about 3 seconds after the device began to listen. One caption was complete, four were clipped or wrong (几点了, 你点了) with the right answer, and one chat got no reaction at all: the device listened until the chat was ended. Two more chats that day got no reaction (奶奶的生日是哪一天, 现在几点了), and once the question was heard as 好啦, to which the assistant said goodbye and closed the chat. A diagnostic build (not committed) wrote out the microphone audio of each chat: in all six the sentence is there, whole, 1.2 seconds long, at a frame level of up to 2650 of 32768 over a room at 35. The bridge received every frame (997 to 999 in 60 seconds, no decode error), and in the silent chats the service took nothing as speech. The same recorded audio, encoded on the PC by libopus with the firmware's settings and sent to the service by the bridge's session, was found every time.
- Open: What differs between the device's stream and that simulation. The device's encoder (Espressif's, `AS_OPUS_ENC_CONFIG`: application AUDIO, complexity 0, DTX on) sends CELT-only packets (first byte 0xBB: wideband, three 20 ms frames; 143 bytes for silence and 146 to 190 for speech, so DTX has no effect), while libopus with the same settings sends SILK packets and uses DTX. Whether the service hears CELT-coded speech worse, or the timing of the frames in a real chat matters, is not known. Whether a real voice is affected is not known either: the test voice is synthetic, and the PC's sound output levels it and loses the band below 300 Hz (the first syllable arrives 14 dB weaker than in the recording).
- For now: Unchanged. The bridge keeps 800 ms for this device; whether that becomes its default is the owner's decision there.
- Needs: The owner's voice first: ask short questions and note each one that is misheard or gets no answer. If it happens with a real voice, two next steps: write out the Opus packets of such a chat (the diagnostic build can), which the bridge's session can decode and replay; and try `ESP_OPUS_ENC_APPLICATION_VOIP` in `AS_OPUS_ENC_CONFIG` with the same block of chats.

## 2026-11-25: Speech that begins before the device listens gets no answer

- Context: From the key press to listening the device needs 2.5 to 3 seconds (about 1.5 for the connection and 1 for the bridge's hello), and the microphone opens 0.13 seconds after that. In six chats on 2026-11-25 the recorded question 你叫什么名字 was started around the WebSocket handshake. When its speech began 0.3 seconds or more after the device listened, it was answered (3 of 3). When it began before that (3 of 3), there was no caption and no reply, and the device went on listening in silence. The bridge's session reproduced it: the service needs 0.3 to 0.45 seconds of clean speech to find a sentence, and what is left of a short question without its start is too little. No setting of the server changes that.
- Open: How often a child speaks that early. Nobody has tried it with a voice.
- For now: Unchanged: the pill shows `我在听` when the device listens, and nothing is recorded before.
- Needs: The owner's test: press OK and speak at once. If it matters, the choices are: record from the key press and send that audio once the channel is open (frames that arrive in a burst do no harm, the bridge's session found); the bridge answering its hello about one second sooner (its own open issue); a sound when listening starts.

## 2026-11-25: Each time the codec opens, the microphone stream begins with a full-scale click

- Context: Measured with the diagnostic build on 2026-11-25: after `EnableInput(true)` the first 50 ms clip at +-32768, and a DC step decays within 0.25 seconds; from 0.15 seconds on the level is right. `AudioService` powers the codec's input down 15 seconds after the last audio (`AUDIO_POWER_TIMEOUT_MS`), also during a reply that is longer than that, so most chats and the turn after a long reply start with the click. The 120 ms that the input task waits before it reads (`audio_input_need_warmup_`) pass before the codec is opened, so they skip nothing. The click is encoded and sent like any other audio.
- Open: Whether it ever hurts. In eight chats, four with the first 250 ms dropped and four without, the captions were the same, and in the bridge's simulation speech that begins 0.1 seconds or more after the click is recognised.
- For now: Unchanged.
- Needs: Nothing unless the assistant mishears the first word after a pause; then drop the first 250 ms after the codec opens (tried in the diagnostic build).

## 2026-11-25: The assistant says that the device is not charging while it is on USB power

- Context: Asked for the battery in the joint test, with the device on USB power, the assistant answered 99 % and not charging. `GetBatteryLevel()` of this board always reports `charging = false`: the CW2017 gauge has no charge state and the board has no charge-detect pin (the comment in the code says so). The device's status tool passes that on as a fact.
- Open: The model tells the child something the device does not know.
- For now: Unchanged.
- Needs: Decide whether the status should leave the charging field out when a board cannot know it (the common code builds the status), or leave it as it is.

## 2026-11-25: After a wake from deep sleep nothing sets the clock

- Context: Both kids builds now skip XiaoZhi's version check after a wake from deep sleep (`CONFIG_SKIP_VERSION_CHECK_AFTER_DEEP_SLEEP`), as the owner chose, so that a wake does not start the bridge's container. A power-on still checks and still waits for a server that has to start. On the device on 2026-11-25 the idle state was reached 2.6 seconds after a wake, and a chat started after such a wake was listening 3.9 seconds later. The answer to that check was also the only thing that set the device's clock, which the idle screen shows. In deep sleep the clock runs on the chip's internal RC oscillator; the board cannot have a 32 kHz crystal, because the keys use its pin. Over a sleep of 600 seconds the clock gained 3.5 seconds (0.59 %), which is about 8.5 minutes for each day asleep. The chip's other internal source (`CONFIG_RTC_CLK_SRC_INT_8MD256`) lost 4.0 seconds (0.67 %) in the same test and was not kept.
- Open: With release firmware, a device that is left to sleep instead of being switched off shows a clock that runs ahead, by about an hour after a week. Only a power-on or a reset corrects it. This is one measurement at room temperature, and an RC oscillator also changes with temperature. With test firmware the device sleeps only on `KUI SLEEP`, so its clock stays right.
- For now: Accepted for the test firmware.
- Needs: A decision before the release firmware is used every day. Choices: SNTP after each wake, with the time zone remembered from the last check; the time in the bridge's WebSocket hello, which corrects the clock at the first chat after a wake (the bridge's session has noted the idea); or the version check in the background after a wake, which starts the server again.

## 2026-11-25: The skipped version check was run only over WebSocket, with timer wakes

- Context: On the device on 2026-11-25, with the test firmware and `KUI SLEEP`: after timer wakes from sleeps of 10 to 600 seconds the log showed `version check skipped, using WebSocket`, no request to the OTA address and the idle state after 2.6 seconds; a chat after such a wake worked; a reset over USB still ran the check. In the joint test of that day the bridge's log confirmed it: no provisioning request between a wake and the WebSocket of the chat that followed. With the bridge scaled to zero, a chat started 1.5 seconds after a wake was listening 53.5 seconds later, after four handshake retries, and a reset waited 60 seconds for the answer of the version check, both without an error. The stock variant was built with the option off.
- Open: Not run: a wake by a key since the change (the owner was away; key wakes were tested before it, and the skip does not look at the cause of the wake); the MQTT path, because the bridge only offers WebSocket; a failed check, after which the next wake must check again; a wake that ends in the Wi-Fi setup, after which the check must run. A changed token or address on the server reaches the device only at the next power-on or reset: a chat after a wake would fail with the old settings until then.
- For now: Relies on the code review and the builds.
- Needs: One key wake when the owner is back. The MQTT path if MQTT is ever used. If the server's settings can change while the device sleeps, a chat that is refused after a skipped check should run the check.

## 2026-11-25: The version notification after a start is replaced at once

- Context: When the device is ready, `Application::HandleActivationDoneEvent` shows the firmware version as a notification. The change of state to idle, queued just before it, then sets the status text, which hides the notification. This was noticed while the source of that version was changed for the skipped check; the behaviour is older and is in the common code.
- Open: The version is on the screen for one pass of the main loop at most, so nobody sees it.
- For now: Unchanged. The boot log has the version.
- Needs: Nothing unless the version should be visible at a start; then show it after the state change.

## 2026-11-25: After a wake the access point can keep the device out, and Wi-Fi setup never sleeps

- Context: Before deep sleep the firmware disconnects from the access point; a wake restarts the firmware, which connects again. On 2026-11-25, with the test firmware and `KUI SLEEP`: after five sleeps of 10 seconds to 2.5 minutes the device was connected again within a few seconds (an address 2.0 to 2.5 seconds after the wake, measured four times). After one sleep of 2.5 minutes the access point answered the first association request late and then ignored every authentication, as in the entry of 2026-11-24 on restarts. After 60 seconds the firmware opened Wi-Fi setup mode. A restart with the power key ended the same way, and a second restart connected after 11 seconds. Resets, after flashing or after such a failure, ended in Wi-Fi setup nine times that day, three of them in a row, and so did three of the six timer wakes of that evening, each after a sleep of only 10 seconds; the wake after a sleep of 120 seconds connected in 3.5 seconds.
- Open: With release firmware every wake is a new connection. When the access point keeps the device out for more than 60 seconds, the child sees the Wi-Fi setup screen. In that mode nothing dims or sleeps, because the idle timers only run in the idle state (a code-review finding), so the battery drains until someone restarts the device. Away from home, an accidental key press ends the same way.
- For now: Unchanged common behaviour (`CONNECT_TIMEOUT_SEC = 60` in `WifiBoard`). The owner knows the access point's behaviour and asked not to debug it.
- Needs: Decide before the release firmware is used every day: keep retrying the saved network after a wake instead of opening setup mode, go back to deep sleep after some minutes in Wi-Fi setup, or change the access point.

## 2026-11-25: The release firmware's sleep after 180 seconds was compiled, not run

- Context: `ai-passport-kids-release` differs from the test firmware only by `CONFIG_AI_PASSPORT_IDLE_DEEP_SLEEP`, and the owner asked that only test firmware is flashed unless he confirms. On the device, with the test firmware on 2026-11-25: deep sleep entered with `KUI SLEEP`; wakes by timer after 10 to 150 seconds, by a DOWN key held for 2 seconds (its click was ignored and the volume stayed) and by a short OK press; the dimming after 30 seconds and its end on a key press; the mascot asleep at boot, awake during and after a chat, asleep again 30 seconds later.
- Open: The one line that starts deep sleep after 180 seconds of idle has only been compiled. The stock variant was built, not run on the device.
- For now: Relies on the build and on review: the check that dims the screen at 30 seconds calls the same `EnterDeepSleep()` as `KUI SLEEP`.
- Needs: When the release firmware is wanted, flash it and leave the device idle for 3 minutes.

## 2026-11-25: The current in deep sleep is not measured

- Context: The firmware cannot switch the device off. Before deep sleep it stops Wi-Fi, closes the codec (the driver's ES8311 suspend), puts the panel to sleep, switches the backlight off and holds the LCD pins.
- Open: What the device draws then is unknown. The amplifier has no enable pin, and the fuel gauge stays active on purpose, so that the battery level is right after a wake (a slept gauge reports 0 % for a while, which would raise the low-battery popup). FoloToy's BSP also writes an ES8311 register sequence of its own and releases the I2S and I2C pins; that was not copied. During sleep the key pin has the chip's pull-up next to the board's 10 kOhm one, so OK reads about 0.70 V against a limit of 0.825 V for a low level (an estimate); a short OK press woke the device in the one test.
- For now: As described.
- Needs: A current measurement if the battery does not last as expected in deep sleep, or a report if OK ever fails to wake the device.

## 2026-11-25: One power-on reset in Wi-Fi setup mode

- Context: During the device tests on 2026-11-25, on USB power, the device restarted once about 6 seconds after Wi-Fi setup mode had started (its access point on, its alert sound playing). The reset reason was power-on.
- Open: The cause. Firmware cannot produce a power-on reset, so a dip of the supply is possible. It was seen once.
- For now: Nothing.
- Needs: Nothing unless it happens again.

## 2026-11-24: After a sentence with an English name, the assistant answered in English and missed the name

- Context: The owner tried the bridge's new features by voice on the device on 2026-11-24. The time, the battery level, the volume and the brightness, an interruption with OK, a goodbye and a new chat right after it worked. Then, in a chat held in Mandarin, the owner said a Mandarin sentence with an English first name in it and a liking for dinosaurs. From that reply on the assistant spoke English for the rest of the chat (four replies), although what the owner said next was Mandarin; in the next chat it spoke Mandarin again. It also missed the name: asked for it 9 seconds later in the same chat, and again in the next chat, it said that it did not know it. The memory itself worked: the bridge wrote the notes of that chat with one fact, the dinosaurs, and loaded them for the next chat. The words on the screen had the name, in Chinese characters; they come from a separate speech recognizer.
- Open: The cause. The device only carries the audio: the speech model behind the bridge (Azure Voice Live) chooses the language and decides what it has heard, which is the owner's guess too. Whether it happens again is not known.
- For now: Nothing is changed, at the owner's request: watch it first.
- Needs: Nothing in the firmware. If it keeps happening, the remedy is in the bridge, whose persona can require Mandarin; its `OPEN_ISSUES.md` has an entry on other languages. The bridge's session has been told.

## 2026-11-24: After the server ends a chat, the device does not close the connection

- Context: When the server ends a chat (the bridge's idle timeout, a goodbye, the daily limit), the WebSocket library marks the connection as closed and the device returns to idle. The library does not answer the server's close frame and does not close the connection. The bridge does not wait for the answer: its server (uvicorn) goes on at once and closes the connection itself 10 seconds later. This was read in the server's code on 2026-11-24. The firmware keeps its WebSocket until the next chat starts, and `WebsocketProtocol::CreateWebsocket` then frees it without stopping its transport first. This is not new: the firmware behaved this way before the close frame was added. The coder noticed it while adding the close frame for chats that the device ends. On the device on 2026-11-24, the owner started a chat 5 seconds after the bridge had ended one with a goodbye: the old connection was still open and was shut down at that moment (the log shows `SSL receive failed: -76`), the new chat was listening 2.4 seconds later, and nothing crashed.
- Open: If the next chat starts while the server still has the old connection open (with the bridge, in the 10 seconds after it ended the chat), data arriving at that moment could reach freed memory. It did not happen in the one chat that was started in that time. Until the next chat, the memory of the old connection stays allocated; how much was not measured.
- For now: Unchanged. The bridge needs nothing from the firmware here: it takes the session length before it closes the connection and does not wait for the device.
- Needs: Decide whether the device should drop the connection as soon as the server closes it, as the MQTT transport does when the server says goodbye (`CloseAudioChannel(false)`). `WebsocketProtocol::DiscardWebsocket()` stops the transport before it frees the WebSocket. A change needs the same test: say `再见`, then press OK within 10 seconds.

## 2026-11-24: The close frame is untested while the server is sending, and on the stock variant

- Context: When the device ends a chat, it now sends a WebSocket close frame and then drops the connection at once, so that a server behind the Azure Container Apps ingress learns it without a 5-second delay. The frame carries no status code: the WebSocket library cannot send one. With the kids variant on the device on 2026-11-24, the bridge's session length matched the device's to within 0.1 seconds in four chats; before, the bridge logged the end of a session 6.5 to 11.5 seconds late. In those four chats the bridge sent no audio. A fifth chat, with a spoken reply, which the owner ended 0.9 seconds after the assistant's last word, matched as well.
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
