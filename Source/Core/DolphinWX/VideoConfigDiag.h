// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <vector>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/dialog.h>
#include <wx/msgdlg.h>
#include <wx/radiobut.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>

#include "Common/CommonTypes.h"
#include "Core/Config/GraphicsSettings.h"

class DolphinSlider;
struct VideoConfig;

class wxBoxSizer;
class wxControl;
class wxPanel;

template <typename W>
class BoolSetting : public W
{
public:
  BoolSetting(wxWindow* parent, const wxString& label, const wxString& tooltip,
              const Config::ConfigInfo<bool>& setting, bool reverse = false, long style = 0);

  void UpdateValue(wxCommandEvent& ev)
  {
    Config::SetBaseOrCurrent(m_setting, (ev.GetInt() != 0) != m_reverse);
    ev.Skip();
  }

public:
  Config::ConfigInfo<bool> m_setting;
  const bool m_reverse;
};

template <typename W>
class RefBoolSetting : public W
{
public:
  RefBoolSetting(wxWindow* parent, const wxString& label, const wxString& tooltip, bool& setting,
                 bool reverse = false, long style = 0);

  void UpdateValue(wxCommandEvent& ev)
  {
    m_setting = (ev.GetInt() != 0) != m_reverse;
    ev.Skip();
  }

private:
  bool& m_setting;
  const bool m_reverse;
};

typedef BoolSetting<wxCheckBox> SettingCheckBox;
typedef BoolSetting<wxRadioButton> SettingRadioButton;

template <typename T>
class IntegerSetting : public wxSpinCtrl
{
public:
  IntegerSetting(wxWindow* parent, const wxString& label, const Config::ConfigInfo<T>& setting,
                 T minVal, T maxVal, long style = 0)
      : wxSpinCtrl(parent, wxID_ANY, label, wxDefaultPosition, wxDefaultSize, style),
        m_setting(setting)
  {
    SetRange(minVal, maxVal);
    SetValue(Config::Get(m_setting));
    Bind(wxEVT_SPINCTRL, &IntegerSetting<T>::UpdateValue, this);
  }

  void UpdateValue(wxCommandEvent& ev)
  {
    Config::SetBaseOrCurrent(m_setting, (T)ev.GetInt());
    ev.Skip();
  }

public:
  Config::ConfigInfo<T> m_setting;
};

template <typename T>
class RefIntegerSetting : public wxSpinCtrl
{
public:
  RefIntegerSetting(wxWindow* parent, const wxString& label, T& setting, T minVal, T maxVal,
                    long style = 0)
      : wxSpinCtrl(parent, wxID_ANY, label, wxDefaultPosition, wxDefaultSize, style),
        m_setting(setting)
  {
    SetRange(minVal, maxVal);
    SetValue(m_setting);
    Bind(wxEVT_SPINCTRL, &RefIntegerSetting<T>::UpdateValue, this);
  }

  void UpdateValue(wxCommandEvent& ev)
  {
    m_setting = ev.GetInt();
    ev.Skip();
  }

private:
  T& m_setting;
};

typedef IntegerSetting<s32> I32Setting;
typedef IntegerSetting<u32> U32Setting;
typedef RefIntegerSetting<s32> RefI32Setting;
typedef RefIntegerSetting<u32> RefU32Setting;

template <typename T>
class FloatSetting : public wxSpinCtrlDouble
{
public:
  FloatSetting(wxWindow* parent, const wxString& label, const Config::ConfigInfo<T>& setting, T minVal, T maxVal,
               T increment = 0, long style = 0);

  void UpdateValue(wxSpinDoubleEvent& ev)
  {
    Config::SetBaseOrCurrent(m_setting, (T)(ev.GetValue()));
    ev.Skip();
  }

private:
   Config::ConfigInfo<T> m_setting;
};

typedef FloatSetting<double> SettingDouble;
typedef FloatSetting<float> SettingNumber;

class SettingChoice : public wxChoice
{
public:
  SettingChoice(wxWindow* parent, const Config::ConfigInfo<int>& setting, const wxString& tooltip,
                int num = 0, const wxString choices[] = nullptr, long style = 0);
  void UpdateValue(wxCommandEvent& ev);

private:
  Config::ConfigInfo<int> m_setting;
};

class VideoConfigDiag : public wxDialog
{
public:
  VideoConfigDiag(wxWindow* parent, const std::string& title);

protected:
  void Event_Backend(wxCommandEvent& ev);
  void Event_DisplayResolution(wxCommandEvent& ev);
  void Event_ProgressiveScan(wxCommandEvent& ev);
  void Event_SafeTextureCache(wxCommandEvent& ev);

  void Event_PPShader(wxCommandEvent& ev);
  void Event_ConfigurePPShader(wxCommandEvent& ev);

  void Event_StereoDepth(wxCommandEvent& ev);
  void Event_StereoConvergence(wxCommandEvent& ev);
  void Event_StereoMode(wxCommandEvent& ev);

  void Event_ClickSave(wxCommandEvent&);
  void Event_Close(wxCommandEvent&);

  // Enables/disables UI elements depending on current config
  void OnUpdateUI(wxUpdateUIEvent& ev);

  // Creates controls and connects their enter/leave window events to Evt_Enter/LeaveControl
  SettingCheckBox* CreateCheckBox(wxWindow* parent, const wxString& label,
                                  const wxString& description,
                                  const Config::ConfigInfo<bool>& setting, bool reverse = false,
                                  long style = 0);
  RefBoolSetting<wxCheckBox>* CreateCheckBoxRefBool(wxWindow* parent, const wxString& label,
                                                    const wxString& description, bool& setting);
  SettingChoice* CreateChoice(wxWindow* parent, const Config::ConfigInfo<int>& setting,
                              const wxString& description, int num = 0,
                              const wxString choices[] = nullptr, long style = 0);
  SettingRadioButton* CreateRadioButton(wxWindow* parent, const wxString& label,
                                        const wxString& description,
                                        const Config::ConfigInfo<bool>& setting,
                                        bool reverse = false, long style = 0);
  SettingNumber* CreateNumber(wxWindow* parent, const Config::ConfigInfo<float>& setting,
                              const wxString& description, float min, float max, float inc,
                              long style = 0);

  // Same as above but only connects enter/leave window events
  wxControl* RegisterControl(wxControl* const control, const wxString& description);

  void Evt_EnterControl(wxMouseEvent& ev);
  void Evt_LeaveControl(wxMouseEvent& ev);
  void CreateDescriptionArea(wxPanel* const page, wxBoxSizer* const sizer);
  void PopulatePostProcessingShaders();
  void PopulateAAList();
  void OnAAChanged(wxCommandEvent& ev);
  void OnUberShaderModeChanged(wxCommandEvent& ev);

  wxChoice* choice_backend;
  wxChoice* choice_adapter;
  wxChoice* choice_display_resolution;

  wxStaticText* label_backend;
  wxStaticText* label_adapter;

  wxStaticText* text_aamode;
  wxChoice* choice_aamode;
  DolphinSlider* conv_slider;

  wxStaticText* label_display_resolution;

  wxButton* button_config_pp;

  SettingCheckBox* borderless_fullscreen;
  RefBoolSetting<wxCheckBox>* render_to_main_checkbox;
  SettingCheckBox* async_timewarp_checkbox;
  SettingCheckBox* efbcopy_clear_disable;

  SettingRadioButton* efbcopy_texture;
  SettingRadioButton* efbcopy_ram;

  SettingRadioButton* virtual_xfb;
  SettingRadioButton* real_xfb;

  SettingCheckBox* cache_hires_textures;

  wxCheckBox* progressive_scan_checkbox;
  wxCheckBox* vertex_rounding_checkbox;

  wxChoice* choice_ppshader;

  std::map<wxWindow*, wxString> ctrl_descs;       // maps setting controls to their descriptions
  std::map<wxWindow*, wxStaticText*> desc_texts;  // maps dialog tabs (which are the parents of the
                                                  // setting controls) to their description text
                                                  // objects

  VideoConfig& vconfig;

  size_t m_msaa_modes;
};
