// SPDX-License-Identifier: GPL-3.0-only
// Fukiai glyphs (MIT, Hügelton Instruments) by their names in hugelton-plugin-rules Glyphs.h, as the original editor
const G: Record<string, number> = {
  port_usb_c: 0xEA3F, symbol_audio: 0xEA40, symbol_download: 0xEAD4, symbol_upload: 0xEAD5,
  symbol_folder: 0xEAB8, symbol_folder_open: 0xEAB9, port_disk: 0xEAB3, symbol_trash: 0xEAC1,
  control_arrow_loop: 0xEAD6, note_eight: 0xEAEF, symbol_waves: 0xEAE6, function_env_adsr_exp: 0xEA12,
  ui_knob: 0xEA70, symbol_keyboard: 0xEABC, symbol_effector: 0xEAB7, symbol_echo: 0xEAB5, symbol_spring: 0xEAD1,
  symbol_tuning: 0xEAA7, control_arrow_up_down: 0xEACF, symbol_grid: 0xEABA, symbol_cog: 0xEA46,
  function_filter_hpf_peak: 0xEA9E, ui_slider_vertical: 0xEA73, symbol_books: 0xEADC, control_rec_f: 0xEA09,
  control_stop_f: 0xEA0F, control_play_f: 0xEA07, symbol_pan: 0xEA53, control_speaker_mute: 0xEAEB, symbol_eye: 0xEA2F,
  waveform_sine: 0xEA85, waveform_triangle_ac: 0xEA89, waveform_sawtooth_ac: 0xEA81, waveform_square: 0xEA87,
  waveform_pulse_quarter: 0xEA7F, waveform_pulse_eighth: 0xEA7B, waveform_noise_step: 0xEA79,
  waveform_noise_white: 0xEA7A, waveform_harmonic: 0xEAC5,
};
export const ic = (name: string) => String.fromCharCode(G[name] ?? 0x20);
export const VIEW_ICON: Record<string, string> = {
  sound: "symbol_audio", seq: "symbol_grid", song: "control_arrow_loop", mixer: "ui_slider_vertical",
  library: "symbol_books", projects: "symbol_folder", samples: "symbol_waves", fm6: "ui_knob", screen: "symbol_eye",
  settings: "symbol_cog",
};
export const FAM_ICON: Record<string, string> = {
  EDIT: "ui_knob", ENV: "function_env_adsr_exp", LFO: "waveform_sine", FX: "symbol_effector", SCL: "symbol_tuning",
  ARP: "control_arrow_up_down", GLO: "ui_slider_vertical", TRK: "ui_slider_vertical",
};
