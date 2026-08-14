#ifndef UISFX_CUES_H
#define UISFX_CUES_H
/*
  UiSfx_Cues.h — short UI sound cues + theme packs for the buzzer.

  Auto-generated from doc/sound/uisfx-origin (uisfx, MIT) catalog.ts —
  do not edit by hand; re-run the converter instead.

  CUES: each cue is a tiny SongNote pattern (the melodic skeleton of the
  original synthesized sound: midi = baseMidi + semitone). What the buzzer
  cannot do was dropped: waveform, noise, envelopes, panning, volume.

  PACKS (themes): the original packs mostly differ in timbre, which a
  piezo cannot reproduce. The two parameters that DO translate are kept:
    pitch    -> semitone transpose (12 * log2(pitch), rounded)
    duration -> speed scale in percent
  So 'glass' plays cues higher and slower, 'mechanical' lower and
  quicker, etc.

  Play cues via the Buzzer player:
    Buzzer.playCue(UISFX_PRESS);   // by enum id
    Buzzer.playCue("press");       // by name
    Buzzer.setCuePack("arcade");   // choose the theme
  A running song is paused during a cue and resumes afterwards.
*/

#include "Buzzer/SongsPlayer/SongPlayer.h"

// hover — Fine-pointer discovery without commitment.
const SongNote UiSfx_hover[] = { { 78, 90, 0u } };
// press — A control is physically engaged.
const SongNote UiSfx_press[] = { { 65, 130, 0u } };
// release — A pressed control springs back.
const SongNote UiSfx_release[] = { { 66, 140, 10u } };
// double-click — A rapid secondary activation.
const SongNote UiSfx_double_click[] = { { 72, 70, 0u }, { 74, 80, 105u } };
// focus — A control becomes ready for keyboard or text input.
const SongNote UiSfx_focus[] = { { 76, 170, 0u } };
// long-press — A sustained press reveals a secondary action.
const SongNote UiSfx_long_press[] = { { 58, 340, 0u }, { 65, 130, 280u } };
// select — An item enters the active set.
const SongNote UiSfx_select[] = { { 70, 120, 0u }, { 77, 160, 90u } };
// deselect — An item leaves the active set.
const SongNote UiSfx_deselect[] = { { 74, 90, 0u }, { 68, 150, 75u } };
// toggle-on — A binary setting becomes active.
const SongNote UiSfx_toggle_on[] = { { 64, 65, 0u }, { 76, 200, 65u } };
// toggle-off — A binary setting becomes inactive.
const SongNote UiSfx_toggle_off[] = { { 76, 65, 0u }, { 64, 180, 70u } };
// check — A checkbox or task enters its completed state.
const SongNote UiSfx_check[] = { { 66, 75, 0u }, { 80, 170, 70u } };
// uncheck — A checkbox or task returns to its incomplete state.
const SongNote UiSfx_uncheck[] = { { 80, 70, 0u }, { 66, 150, 65u } };
// delete — A destructive removal is committed.
const SongNote UiSfx_delete[] = { { 64, 200, 0u }, { 56, 240, 160u } };
// cancel — A pending action is abandoned without applying.
const SongNote UiSfx_cancel[] = { { 67, 110, 0u }, { 61, 200, 80u } };
// undo — The most recent change is reversed.
const SongNote UiSfx_undo[] = { { 76, 160, 0u }, { 71, 140, 150u }, { 65, 130, 270u } };
// redo — A reversed change is applied again.
const SongNote UiSfx_redo[] = { { 65, 130, 0u }, { 71, 140, 120u }, { 76, 160, 250u } };
// copy — Selected content is placed on the clipboard.
const SongNote UiSfx_copy[] = { { 70, 140, 0u }, { 82, 150, 100u } };
// paste — Clipboard content is inserted into the current context.
const SongNote UiSfx_paste[] = { { 79, 80, 0u }, { 67, 190, 65u }, { 70, 140, 170u } };
// open — A menu, sheet, panel, or detail view appears.
const SongNote UiSfx_open[] = { { 62, 280, 0u }, { 76, 110, 180u } };
// close — A menu, sheet, panel, or detail view recedes.
const SongNote UiSfx_close[] = { { 76, 100, 0u }, { 69, 240, 55u } };
// back — Navigation returns to the previous place.
const SongNote UiSfx_back[] = { { 69, 230, 0u } };
// forward — Navigation advances to the next place.
const SongNote UiSfx_forward[] = { { 61, 120, 0u }, { 70, 170, 90u } };
// expand — A collapsed region reveals more detail.
const SongNote UiSfx_expand[] = { { 64, 120, 0u }, { 68, 130, 110u }, { 73, 120, 220u } };
// collapse — An expanded region returns to its compact state.
const SongNote UiSfx_collapse[] = { { 73, 110, 0u }, { 68, 120, 100u }, { 64, 120, 200u } };
// drag-start — An object lifts from its resting place.
const SongNote UiSfx_drag_start[] = { { 55, 220, 0u } };
// drop — A dragged object lands in a valid target.
const SongNote UiSfx_drop[] = { { 62, 100, 0u }, { 55, 170, 45u }, { 50, 130, 120u } };
// snap — An object locks into a precise position.
const SongNote UiSfx_snap[] = { { 73, 100, 0u }, { 85, 80, 45u } };
// swipe — A touch gesture moves content spatially.
const SongNote UiSfx_swipe[] = { { 65, 290, 20u } };
// reorder — An item settles into a new position in a sequence.
const SongNote UiSfx_reorder[] = { { 68, 100, 0u }, { 63, 100, 85u }, { 61, 130, 170u } };
// invalid-drop — A dragged object cannot land in the current target.
const SongNote UiSfx_invalid_drop[] = { { 60, 110, 0u }, { 55, 130, 120u }, { 60, 110, 240u } };
// send — A message or object leaves the user.
const SongNote UiSfx_send[] = { { 67, 280, 0u }, { 81, 160, 190u } };
// receive — A response or object arrives.
const SongNote UiSfx_receive[] = { { 84, 120, 0u }, { 76, 200, 110u }, { 72, 170, 250u } };
// notification — New information is available, without urgency.
const SongNote UiSfx_notification[] = { { 72, 260, 0u }, { 77, 300, 190u } };
// mention — The user is directly addressed.
const SongNote UiSfx_mention[] = { { 74, 180, 0u }, { 78, 180, 160u }, { 83, 250, 320u } };
// typing — A brief key contact during text entry.
const SongNote UiSfx_typing[] = { { 73, 24, 0u } };
// reaction — A lightweight social response is added.
const SongNote UiSfx_reaction[] = { { 75, 100, 0u }, { 87, 130, 75u }, { 82, 160, 180u } };
// success — An action finished with the expected result.
const SongNote UiSfx_success[] = { { 67, 300, 0u }, { 71, 320, 160u }, { 74, 330, 330u } };
// error — An action failed and needs attention.
const SongNote UiSfx_error[] = { { 68, 280, 0u }, { 62, 320, 220u } };
// warning — A risky or consequential state needs review.
const SongNote UiSfx_warning[] = { { 65, 220, 0u }, { 65, 300, 280u } };
// info — A neutral system fact is surfaced.
const SongNote UiSfx_info[] = { { 70, 180, 0u }, { 75, 180, 200u } };
// blocked — An action cannot continue in the current state.
const SongNote UiSfx_blocked[] = { { 52, 130, 0u }, { 52, 200, 140u } };
// retry — A failed action is attempted again.
const SongNote UiSfx_retry[] = { { 62, 170, 0u }, { 69, 210, 160u } };
// start — A process, recording, or session begins.
const SongNote UiSfx_start[] = { { 55, 120, 0u }, { 60, 180, 100u }, { 67, 180, 240u } };
// stop — A process, recording, or session ends.
const SongNote UiSfx_stop[] = { { 67, 110, 0u }, { 62, 120, 100u }, { 55, 150, 200u } };
// progress-step — A discrete step advances inside a longer process.
const SongNote UiSfx_progress_step[] = { { 72, 70, 0u }, { 75, 100, 85u } };
// complete — A multi-step process reaches its final state.
const SongNote UiSfx_complete[] = { { 65, 220, 0u }, { 72, 250, 220u }, { 77, 270, 460u } };
// queued — Work is accepted and waiting to begin.
const SongNote UiSfx_queued[] = { { 64, 140, 0u }, { 66, 180, 130u } };
// checkpoint — A meaningful stage in a longer process is saved.
const SongNote UiSfx_checkpoint[] = { { 69, 180, 0u }, { 74, 180, 140u }, { 78, 170, 280u } };
// loading — A quiet repeating pulse while an interface fetches or waits.
const SongNote UiSfx_loading[] = { { 72, 160, 0u }, { 77, 160, 300u }, { 74, 160, 600u }, { 79, 160, 900u } };
// processing — A restrained repeating bed while sustained work is running.
const SongNote UiSfx_processing[] = { { 62, 240, 0u }, { 67, 200, 400u }, { 64, 240, 800u }, { 69, 200, 1200u } };
// recording — A calm periodic pulse while audio or video capture is live.
const SongNote UiSfx_recording[] = { { 67, 200, 0u }, { 67, 140, 500u } };
// connecting — A repeating search pattern while a device or live session connects.
const SongNote UiSfx_connecting[] = { { 69, 180, 0u }, { 73, 180, 375u }, { 76, 200, 750u }, { 73, 160, 1125u } };
// scanning — A spatial sweep repeats while content or devices are discovered.
const SongNote UiSfx_scanning[] = { { 67, 280, 0u }, { 75, 280, 700u } };
// streaming — A quiet repeating flow while live data or media continues.
const SongNote UiSfx_streaming[] = { { 65, 200, 0u }, { 72, 160, 300u }, { 67, 200, 600u }, { 74, 160, 900u } };
// play — Media playback begins or resumes.
const SongNote UiSfx_play[] = { { 62, 90, 0u }, { 67, 110, 75u }, { 74, 130, 160u } };
// pause — Media playback pauses at the current position.
const SongNote UiSfx_pause[] = { { 71, 110, 0u }, { 71, 130, 130u } };
// seek — The playback position moves to a new point.
const SongNote UiSfx_seek[] = { { 67, 105, 0u }, { 77, 120, 115u } };
// volume-change — Playback loudness moves to a new level.
const SongNote UiSfx_volume_change[] = { { 72, 55, 0u }, { 76, 65, 55u }, { 80, 80, 115u } };
// skip-next — Playback advances to the next item.
const SongNote UiSfx_skip_next[] = { { 70, 75, 0u }, { 74, 85, 70u }, { 82, 105, 145u } };
// skip-previous — Playback returns to the previous item.
const SongNote UiSfx_skip_previous[] = { { 82, 75, 0u }, { 74, 85, 70u }, { 70, 105, 145u } };
// connect — A device, service, or live session becomes available.
const SongNote UiSfx_connect[] = { { 64, 220, 0u }, { 69, 230, 180u }, { 76, 180, 370u } };
// disconnect — A device, service, or live session goes offline.
const SongNote UiSfx_disconnect[] = { { 76, 210, 0u }, { 69, 220, 180u }, { 64, 180, 340u } };
// lock — Access closes or a protected state engages.
const SongNote UiSfx_lock[] = { { 59, 140, 0u }, { 55, 170, 110u } };
// unlock — Access opens or a protected state disengages.
const SongNote UiSfx_unlock[] = { { 60, 100, 0u }, { 67, 140, 95u }, { 74, 180, 210u } };
// wake — A device or dormant interface becomes active.
const SongNote UiSfx_wake[] = { { 56, 310, 0u }, { 68, 160, 240u } };
// sleep — A device or interface enters a dormant state.
const SongNote UiSfx_sleep[] = { { 68, 260, 0u }, { 59, 200, 210u } };
// reward — The user receives a small unit of value.
const SongNote UiSfx_reward[] = { { 76, 220, 0u }, { 88, 240, 100u }, { 83, 290, 260u } };
// level-up — Capability, rank, or progression increases.
const SongNote UiSfx_level_up[] = { { 64, 240, 0u }, { 68, 250, 170u }, { 71, 260, 340u }, { 76, 320, 510u } };
// achievement — A rare milestone deserves a fuller celebration.
const SongNote UiSfx_achievement[] = { { 62, 340, 0u }, { 69, 340, 160u }, { 74, 380, 340u }, { 78, 420, 560u }, { 81, 380, 730u } };
// streak — Repeated participation extends an active streak.
const SongNote UiSfx_streak[] = { { 69, 170, 0u }, { 71, 180, 140u }, { 73, 190, 280u }, { 76, 210, 430u } };
// badge — A collectible distinction is awarded.
const SongNote UiSfx_badge[] = { { 71, 190, 0u }, { 80, 230, 140u }, { 87, 350, 330u } };
// bonus — An unexpected extra reward is revealed.
const SongNote UiSfx_bonus[] = { { 66, 180, 0u }, { 70, 190, 120u }, { 75, 230, 250u }, { 82, 340, 430u } };
// add-to-cart — An item enters a cart or pending order.
const SongNote UiSfx_add_to_cart[] = { { 68, 100, 0u }, { 77, 180, 105u }, { 72, 170, 260u } };
// remove-from-cart — An item leaves a cart or pending order.
const SongNote UiSfx_remove_from_cart[] = { { 80, 100, 0u }, { 73, 140, 100u }, { 68, 160, 220u } };
// checkout — A cart advances into the payment flow.
const SongNote UiSfx_checkout[] = { { 65, 220, 0u }, { 70, 230, 180u }, { 74, 240, 350u } };
// purchase — A paid transaction or value exchange completes.
const SongNote UiSfx_purchase[] = { { 64, 120, 0u }, { 69, 240, 110u }, { 76, 320, 280u } };
// coupon — A discount or promotional code is accepted.
const SongNote UiSfx_coupon[] = { { 72, 160, 0u }, { 76, 180, 130u }, { 81, 210, 260u } };
// refund — Value returns after a completed transaction.
const SongNote UiSfx_refund[] = { { 74, 240, 0u }, { 69, 250, 190u }, { 67, 230, 390u } };


/* ----------------------------------------------------------------
   Cue registry — lookup by index or name
---------------------------------------------------------------- */
struct UiSfxCue {
  const char*     name;
  const SongNote* notes;
  uint8_t         count;
};

const UiSfxCue UISFX_CUES[] = {
  { "hover", UiSfx_hover, sizeof(UiSfx_hover) / sizeof(SongNote) },
  { "press", UiSfx_press, sizeof(UiSfx_press) / sizeof(SongNote) },
  { "release", UiSfx_release, sizeof(UiSfx_release) / sizeof(SongNote) },
  { "double-click", UiSfx_double_click, sizeof(UiSfx_double_click) / sizeof(SongNote) },
  { "focus", UiSfx_focus, sizeof(UiSfx_focus) / sizeof(SongNote) },
  { "long-press", UiSfx_long_press, sizeof(UiSfx_long_press) / sizeof(SongNote) },
  { "select", UiSfx_select, sizeof(UiSfx_select) / sizeof(SongNote) },
  { "deselect", UiSfx_deselect, sizeof(UiSfx_deselect) / sizeof(SongNote) },
  { "toggle-on", UiSfx_toggle_on, sizeof(UiSfx_toggle_on) / sizeof(SongNote) },
  { "toggle-off", UiSfx_toggle_off, sizeof(UiSfx_toggle_off) / sizeof(SongNote) },
  { "check", UiSfx_check, sizeof(UiSfx_check) / sizeof(SongNote) },
  { "uncheck", UiSfx_uncheck, sizeof(UiSfx_uncheck) / sizeof(SongNote) },
  { "delete", UiSfx_delete, sizeof(UiSfx_delete) / sizeof(SongNote) },
  { "cancel", UiSfx_cancel, sizeof(UiSfx_cancel) / sizeof(SongNote) },
  { "undo", UiSfx_undo, sizeof(UiSfx_undo) / sizeof(SongNote) },
  { "redo", UiSfx_redo, sizeof(UiSfx_redo) / sizeof(SongNote) },
  { "copy", UiSfx_copy, sizeof(UiSfx_copy) / sizeof(SongNote) },
  { "paste", UiSfx_paste, sizeof(UiSfx_paste) / sizeof(SongNote) },
  { "open", UiSfx_open, sizeof(UiSfx_open) / sizeof(SongNote) },
  { "close", UiSfx_close, sizeof(UiSfx_close) / sizeof(SongNote) },
  { "back", UiSfx_back, sizeof(UiSfx_back) / sizeof(SongNote) },
  { "forward", UiSfx_forward, sizeof(UiSfx_forward) / sizeof(SongNote) },
  { "expand", UiSfx_expand, sizeof(UiSfx_expand) / sizeof(SongNote) },
  { "collapse", UiSfx_collapse, sizeof(UiSfx_collapse) / sizeof(SongNote) },
  { "drag-start", UiSfx_drag_start, sizeof(UiSfx_drag_start) / sizeof(SongNote) },
  { "drop", UiSfx_drop, sizeof(UiSfx_drop) / sizeof(SongNote) },
  { "snap", UiSfx_snap, sizeof(UiSfx_snap) / sizeof(SongNote) },
  { "swipe", UiSfx_swipe, sizeof(UiSfx_swipe) / sizeof(SongNote) },
  { "reorder", UiSfx_reorder, sizeof(UiSfx_reorder) / sizeof(SongNote) },
  { "invalid-drop", UiSfx_invalid_drop, sizeof(UiSfx_invalid_drop) / sizeof(SongNote) },
  { "send", UiSfx_send, sizeof(UiSfx_send) / sizeof(SongNote) },
  { "receive", UiSfx_receive, sizeof(UiSfx_receive) / sizeof(SongNote) },
  { "notification", UiSfx_notification, sizeof(UiSfx_notification) / sizeof(SongNote) },
  { "mention", UiSfx_mention, sizeof(UiSfx_mention) / sizeof(SongNote) },
  { "typing", UiSfx_typing, sizeof(UiSfx_typing) / sizeof(SongNote) },
  { "reaction", UiSfx_reaction, sizeof(UiSfx_reaction) / sizeof(SongNote) },
  { "success", UiSfx_success, sizeof(UiSfx_success) / sizeof(SongNote) },
  { "error", UiSfx_error, sizeof(UiSfx_error) / sizeof(SongNote) },
  { "warning", UiSfx_warning, sizeof(UiSfx_warning) / sizeof(SongNote) },
  { "info", UiSfx_info, sizeof(UiSfx_info) / sizeof(SongNote) },
  { "blocked", UiSfx_blocked, sizeof(UiSfx_blocked) / sizeof(SongNote) },
  { "retry", UiSfx_retry, sizeof(UiSfx_retry) / sizeof(SongNote) },
  { "start", UiSfx_start, sizeof(UiSfx_start) / sizeof(SongNote) },
  { "stop", UiSfx_stop, sizeof(UiSfx_stop) / sizeof(SongNote) },
  { "progress-step", UiSfx_progress_step, sizeof(UiSfx_progress_step) / sizeof(SongNote) },
  { "complete", UiSfx_complete, sizeof(UiSfx_complete) / sizeof(SongNote) },
  { "queued", UiSfx_queued, sizeof(UiSfx_queued) / sizeof(SongNote) },
  { "checkpoint", UiSfx_checkpoint, sizeof(UiSfx_checkpoint) / sizeof(SongNote) },
  { "loading", UiSfx_loading, sizeof(UiSfx_loading) / sizeof(SongNote) },
  { "processing", UiSfx_processing, sizeof(UiSfx_processing) / sizeof(SongNote) },
  { "recording", UiSfx_recording, sizeof(UiSfx_recording) / sizeof(SongNote) },
  { "connecting", UiSfx_connecting, sizeof(UiSfx_connecting) / sizeof(SongNote) },
  { "scanning", UiSfx_scanning, sizeof(UiSfx_scanning) / sizeof(SongNote) },
  { "streaming", UiSfx_streaming, sizeof(UiSfx_streaming) / sizeof(SongNote) },
  { "play", UiSfx_play, sizeof(UiSfx_play) / sizeof(SongNote) },
  { "pause", UiSfx_pause, sizeof(UiSfx_pause) / sizeof(SongNote) },
  { "seek", UiSfx_seek, sizeof(UiSfx_seek) / sizeof(SongNote) },
  { "volume-change", UiSfx_volume_change, sizeof(UiSfx_volume_change) / sizeof(SongNote) },
  { "skip-next", UiSfx_skip_next, sizeof(UiSfx_skip_next) / sizeof(SongNote) },
  { "skip-previous", UiSfx_skip_previous, sizeof(UiSfx_skip_previous) / sizeof(SongNote) },
  { "connect", UiSfx_connect, sizeof(UiSfx_connect) / sizeof(SongNote) },
  { "disconnect", UiSfx_disconnect, sizeof(UiSfx_disconnect) / sizeof(SongNote) },
  { "lock", UiSfx_lock, sizeof(UiSfx_lock) / sizeof(SongNote) },
  { "unlock", UiSfx_unlock, sizeof(UiSfx_unlock) / sizeof(SongNote) },
  { "wake", UiSfx_wake, sizeof(UiSfx_wake) / sizeof(SongNote) },
  { "sleep", UiSfx_sleep, sizeof(UiSfx_sleep) / sizeof(SongNote) },
  { "reward", UiSfx_reward, sizeof(UiSfx_reward) / sizeof(SongNote) },
  { "level-up", UiSfx_level_up, sizeof(UiSfx_level_up) / sizeof(SongNote) },
  { "achievement", UiSfx_achievement, sizeof(UiSfx_achievement) / sizeof(SongNote) },
  { "streak", UiSfx_streak, sizeof(UiSfx_streak) / sizeof(SongNote) },
  { "badge", UiSfx_badge, sizeof(UiSfx_badge) / sizeof(SongNote) },
  { "bonus", UiSfx_bonus, sizeof(UiSfx_bonus) / sizeof(SongNote) },
  { "add-to-cart", UiSfx_add_to_cart, sizeof(UiSfx_add_to_cart) / sizeof(SongNote) },
  { "remove-from-cart", UiSfx_remove_from_cart, sizeof(UiSfx_remove_from_cart) / sizeof(SongNote) },
  { "checkout", UiSfx_checkout, sizeof(UiSfx_checkout) / sizeof(SongNote) },
  { "purchase", UiSfx_purchase, sizeof(UiSfx_purchase) / sizeof(SongNote) },
  { "coupon", UiSfx_coupon, sizeof(UiSfx_coupon) / sizeof(SongNote) },
  { "refund", UiSfx_refund, sizeof(UiSfx_refund) / sizeof(SongNote) },
};
const uint8_t UISFX_CUE_COUNT = sizeof(UISFX_CUES) / sizeof(UISFX_CUES[0]);
const uint8_t UISFX_CUE_MAX_NOTES = 5;   // longest cue pattern

// Cue ids (same order as UISFX_CUES)
enum UiSfxCueId : uint8_t {
  UISFX_HOVER = 0,
  UISFX_PRESS = 1,
  UISFX_RELEASE = 2,
  UISFX_DOUBLE_CLICK = 3,
  UISFX_FOCUS = 4,
  UISFX_LONG_PRESS = 5,
  UISFX_SELECT = 6,
  UISFX_DESELECT = 7,
  UISFX_TOGGLE_ON = 8,
  UISFX_TOGGLE_OFF = 9,
  UISFX_CHECK = 10,
  UISFX_UNCHECK = 11,
  UISFX_DELETE = 12,
  UISFX_CANCEL = 13,
  UISFX_UNDO = 14,
  UISFX_REDO = 15,
  UISFX_COPY = 16,
  UISFX_PASTE = 17,
  UISFX_OPEN = 18,
  UISFX_CLOSE = 19,
  UISFX_BACK = 20,
  UISFX_FORWARD = 21,
  UISFX_EXPAND = 22,
  UISFX_COLLAPSE = 23,
  UISFX_DRAG_START = 24,
  UISFX_DROP = 25,
  UISFX_SNAP = 26,
  UISFX_SWIPE = 27,
  UISFX_REORDER = 28,
  UISFX_INVALID_DROP = 29,
  UISFX_SEND = 30,
  UISFX_RECEIVE = 31,
  UISFX_NOTIFICATION = 32,
  UISFX_MENTION = 33,
  UISFX_TYPING = 34,
  UISFX_REACTION = 35,
  UISFX_SUCCESS = 36,
  UISFX_ERROR = 37,
  UISFX_WARNING = 38,
  UISFX_INFO = 39,
  UISFX_BLOCKED = 40,
  UISFX_RETRY = 41,
  UISFX_START = 42,
  UISFX_STOP = 43,
  UISFX_PROGRESS_STEP = 44,
  UISFX_COMPLETE = 45,
  UISFX_QUEUED = 46,
  UISFX_CHECKPOINT = 47,
  UISFX_LOADING = 48,
  UISFX_PROCESSING = 49,
  UISFX_RECORDING = 50,
  UISFX_CONNECTING = 51,
  UISFX_SCANNING = 52,
  UISFX_STREAMING = 53,
  UISFX_PLAY = 54,
  UISFX_PAUSE = 55,
  UISFX_SEEK = 56,
  UISFX_VOLUME_CHANGE = 57,
  UISFX_SKIP_NEXT = 58,
  UISFX_SKIP_PREVIOUS = 59,
  UISFX_CONNECT = 60,
  UISFX_DISCONNECT = 61,
  UISFX_LOCK = 62,
  UISFX_UNLOCK = 63,
  UISFX_WAKE = 64,
  UISFX_SLEEP = 65,
  UISFX_REWARD = 66,
  UISFX_LEVEL_UP = 67,
  UISFX_ACHIEVEMENT = 68,
  UISFX_STREAK = 69,
  UISFX_BADGE = 70,
  UISFX_BONUS = 71,
  UISFX_ADD_TO_CART = 72,
  UISFX_REMOVE_FROM_CART = 73,
  UISFX_CHECKOUT = 74,
  UISFX_PURCHASE = 75,
  UISFX_COUPON = 76,
  UISFX_REFUND = 77,
};


/* ----------------------------------------------------------------
   Theme packs — pitch transpose + speed, from the original packs
---------------------------------------------------------------- */
struct UiSfxPack {
  const char* name;
  int8_t      semitones;     // transpose applied to every cue note
  uint8_t     speed_pct;     // 100 = original timing, 78 = quicker, 122 = slower
};

const UiSfxPack UISFX_PACKS[] = {
  { "minimal", 0, 78 },   // Dry, precise, almost invisible.
  { "soft", -2, 108 },   // Rounded felt, warm and reassuring.
  { "glass", 3, 122 },   // Bright, crystalline, and premium.
  { "arcade", 1, 86 },   // Chunky pixels and cheerful voltage.
  { "mechanical", -5, 72 },   // Switches, relays, and firm detents.
  { "organic", -1, 112 },   // Wood, water, breath, and small stones.
  { "dreamy", 1, 118 },   // Airy blooms, soft light, and slow sparkle.
  { "scifi", 2, 84 },   // Clean holographic pings with a restrained digital shimmer.
  { "rubber", -3, 88 },   // Tactile elastic taps with a quick, friendly rebound.
  { "cinematic", -6, 128 },   // Deep impacts, polished tails, and quiet scale.
  { "studio", -3, 82 },   // Tactile editing precision with warm cinematic restraint.
  { "zen", -1, 82 },   // Pure tones, dry wood, and brief washi detail.
};
const uint8_t UISFX_PACK_COUNT = sizeof(UISFX_PACKS) / sizeof(UISFX_PACKS[0]);

#endif // UISFX_CUES_H