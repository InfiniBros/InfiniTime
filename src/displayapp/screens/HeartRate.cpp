#include "displayapp/screens/HeartRate.h"
#include <lvgl/lvgl.h>
#include <components/heartrate/HeartRateController.h>

#include "displayapp/DisplayApp.h"
#include "displayapp/InfiniTimeTheme.h"

using namespace Pinetime::Applications::Screens;

namespace {
  const char* ToString(Pinetime::Controllers::HeartRateController::States s) {
    switch (s) {
      case Pinetime::Controllers::HeartRateController::States::NotEnoughData:
        return "Please wait...";
      case Pinetime::Controllers::HeartRateController::States::Searching:
        return "Searching...";
      case Pinetime::Controllers::HeartRateController::States::NoTouch:
        return "No touch detected";
      case Pinetime::Controllers::HeartRateController::States::Ready:
        return "Measuring...";
      case Pinetime::Controllers::HeartRateController::States::Stopped:
        return "Stopped";
      case Pinetime::Controllers::HeartRateController::States::Disabled:
        return "Disabled";
    }
    return "";
  }

  void btnStartStopEventHandler(lv_obj_t* obj, lv_event_t event) {
    auto* screen = static_cast<HeartRate*>(obj->user_data);
    screen->OnStartStopEvent(event);
  }
}

HeartRate::HeartRate(Controllers::HeartRateController& heartRateController, System::SystemTask& systemTask)
  : heartRateController {heartRateController}, wakeLock(systemTask) {
  bool isHrRunning = heartRateController.State() != Controllers::HeartRateController::States::Disabled;

  // create value label
  label_hr_value = lv_label_create(lv_scr_act(), nullptr);
  lv_obj_set_style_local_text_font(label_hr_value, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, &jetbrains_mono_42);
  lv_label_set_text_static(label_hr_value, "--");

  // create text label (BPM)
  label_hr_text = lv_label_create(lv_scr_act(), nullptr);
  lv_obj_set_style_local_text_font(label_hr_text, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, &jetbrains_mono_bold_20);
  lv_label_set_text_static(label_hr_text, "BPM");

  // style both based on state
  lv_color_t color = isHrRunning ? Colors::highlight : Colors::lightGray;
  lv_obj_set_style_local_text_color(label_hr_value, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, color);

  /*
  // info label above
  label_bpm = lv_label_create(lv_scr_act(), nullptr);
  lv_label_set_text_static(label_bpm, "Heart rate BPM");
  lv_obj_align(label_bpm, nullptr, LV_ALIGN_OUT_TOP_MID, 0, 30);
  */

  label_status = lv_label_create(lv_scr_act(), nullptr);
  lv_obj_set_style_local_text_color(label_status, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_GRAY);
  lv_label_set_text_static(label_status, ToString(Pinetime::Controllers::HeartRateController::States::Disabled));

  // heart icon above value
  heart = lv_label_create(lv_scr_act(), nullptr);
  lv_obj_set_style_local_text_font(heart, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, &hrm_icon);
  lv_label_set_text_static(heart, Symbols::heart);
  lv_obj_set_style_local_text_color(heart, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_RED);
  lv_obj_align(heart, nullptr, LV_ALIGN_CENTER, 0, -60);

  // start/stop button at bottom
  btn_startStop = lv_btn_create(lv_scr_act(), nullptr);
  btn_startStop->user_data = this;
  lv_obj_set_size(btn_startStop, lv_obj_get_width(lv_scr_act()), 50);
  lv_obj_set_style_local_radius(btn_startStop, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, LV_RADIUS_CIRCLE);
  lv_obj_set_style_local_bg_color(btn_startStop, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, Colors::bgAlt);
  lv_obj_set_event_cb(btn_startStop, btnStartStopEventHandler);
  lv_obj_align(btn_startStop, nullptr, LV_ALIGN_IN_BOTTOM_MID, 0, 0);

  label_startStop = lv_label_create(btn_startStop, nullptr);
  UpdateStartStopButton(isHrRunning);
  if (isHrRunning) {
    wakeLock.Lock();
  }

  // periodic refresh
  taskRefresh = lv_task_create(RefreshTaskCallback, 100, LV_TASK_PRIO_MID, this);
}

HeartRate::~HeartRate() {
  lv_task_del(taskRefresh);
  lv_obj_clean(lv_scr_act());
}

void HeartRate::RecenterHrmValue() {
  int w_text = lv_obj_get_width(label_hr_text);
  lv_obj_align(label_hr_value, nullptr, LV_ALIGN_CENTER, -(w_text) / 2, 0);
  lv_obj_align(label_hr_text, label_hr_value, LV_ALIGN_OUT_RIGHT_MID, 0, 9);
}

void HeartRate::Refresh() {

  auto state = heartRateController.State();
  if (state == Controllers::HeartRateController::States::NoTouch || state == Controllers::HeartRateController::States::NotEnoughData) {
    lv_label_set_text_static(label_hr_value, "--");
  } else if (heartRateController.HeartRate() == 0) {
    lv_label_set_text_static(label_hr_value, "--");
  } else {
    int hr = heartRateController.HeartRate();
    lv_label_set_text_fmt(label_hr_value, "%d", hr);
  }

  RecenterHrmValue();
  lv_label_set_text_static(label_status, ToString(state));
  lv_obj_align(label_status, nullptr, LV_ALIGN_OUT_BOTTOM_MID, 0, -80);
}

void HeartRate::OnStartStopEvent(lv_event_t event) {
  if (event == LV_EVENT_CLICKED) {
    if (heartRateController.State() == Controllers::HeartRateController::States::Disabled) {
      heartRateController.Enable();
      UpdateStartStopButton(heartRateController.State() != Controllers::HeartRateController::States::Disabled);
      wakeLock.Lock();
      lv_obj_set_style_local_text_color(label_hr_value, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, Colors::highlight);
    } else {
      heartRateController.Disable();
      UpdateStartStopButton(heartRateController.State() != Controllers::HeartRateController::States::Disabled);
      wakeLock.Release();
      lv_obj_set_style_local_text_color(label_hr_value, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, Colors::lightGray);
    }
  }
}

void HeartRate::UpdateStartStopButton(bool isRunning) {
  if (isRunning) {
    lv_label_set_text_static(label_startStop, "Stop");
  } else {
    lv_label_set_text_static(label_startStop, "Start");
  }
}
