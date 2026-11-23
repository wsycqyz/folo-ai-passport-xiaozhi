# Open issues

Things that could change the code later. Delete an entry when it is settled.

## 2026-11-24: No spoken conversation was tested on the kids variant

- Context: `ai-passport-kids` was checked on the device with a boot log, a scripted gallery of 23 captured screens, and one real session that reached the listening state. Nobody spoke and no key was pressed.
- Open: How real transcribed words look in the bubble, whether the talking mouth and the pill follow real speech, the sound at 16 kHz, the volume bar on real key presses, whether the first frame is dark to the eye, and glyphs pushed by a server.
- For now: The gallery drives the display through the same code path as real messages. The image is flashed on the device, with the volume left at 1.
- Needs: One conversation on the device, and a note of anything that looks or sounds wrong.

## 2026-11-24: `请稍等` can stay on screen when no answer follows

- Context: Review finding. Once her words arrive, the pill shows `请稍等` and the mascot looks up until the assistant starts to speak.
- Open: If a server sends her transcript but no reply (a failed or filtered response), the screen keeps asking her to wait although the device is listening.
- For now: Nothing. The next thing she says replaces the text.
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

## 2026-11-24: After a quick restart the device can end up in Wi-Fi setup

- Context: After a reset without a clean disconnect, the home access point refused the device for 35 to 60 seconds or more. The firmware gives up after 60 seconds and opens Wi-Fi setup mode, where it stays until it is restarted.
- Open: A child who switches the device off and on quickly may land on the setup screen.
- For now: Unchanged common behaviour (`WifiBoard::OnWifiConnectTimeout`). A restart a few minutes later connects in under a second.
- Needs: Decide whether the kids variant should keep retrying the saved network instead of staying in setup mode.

## 2026-11-24: The Wi-Fi setup page stores 2 dBm by accident

- Context: In `78/esp-wifi-connect` 3.3.3, saving the Advanced tab on a device without Wi-Fi settings stores the first entry of the transmit power list (8 = 2 dBm).
- Open: The cause is in the component. Only the kids variant works around it, and it cannot tell an accidental 2 dBm from a deliberate one.
- For now: The kids variant erases `wifi/max_tx_power` when it is 8, and creates the `wifi` namespace so that the page shows the real default.
- Needs: Decide whether to report it to the component's author, and whether the stock variant should get the same guard.

## 2026-11-24: The host tests already fail on `main`

- Context: `python -m unittest discover -s scripts/tests` has 41 failing or erroring entries on `main`, because the tests refer to boards that this repository removed.
- Open: The suite cannot serve as a gate.
- For now: The new `scripts/tests/test_kids_ui.py` passes on its own, and the 41 old entries are unchanged.
- Needs: Decide whether to prune or fix the old tests.
