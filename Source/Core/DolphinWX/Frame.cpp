// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifdef __APPLE__
#include <Cocoa/Cocoa.h>
#endif

#include <cstddef>
#include <fstream>
#include <string>
#include <utility>
#include <vector>
#include <wx/chartype.h>
#include <wx/defs.h>
#include <wx/event.h>
#include <wx/filename.h>
#include <wx/frame.h>
#include <wx/gdicmn.h>
#include <wx/icon.h>
#include <wx/listbase.h>
#include <wx/menu.h>
#include <wx/menuitem.h>
#include <wx/mousestate.h>
#include <wx/msgdlg.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/statusbr.h>
#include <wx/string.h>
#include <wx/textctrl.h>
#include <wx/thread.h>
#include <wx/toplevel.h>
#include <wx/translation.h>
#include <wx/window.h>
#include <wx/windowid.h>
#include <wx/aui/auibook.h>
#include <wx/aui/framemanager.h>

#include "AudioCommon/AudioCommon.h"

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/Thread.h"
#include "Common/Logging/ConsoleListener.h"

#include "Core/ARBruteForcer.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreParameter.h"
#include "Core/HotkeyManager.h"
#include "Core/Movie.h"
#include "Core/State.h"
#include "Core/HW/DVDInterface.h"
#include "Core/HW/GCKeyboard.h"
#include "Core/HW/GCPad.h"
#include "Core/HW/Wiimote.h"

#include "DolphinWX/Frame.h"
#include "DolphinWX/GameListCtrl.h"
#include "DolphinWX/Globals.h"
#include "DolphinWX/LogWindow.h"
#include "DolphinWX/Main.h"
#include "DolphinWX/TASInputDlg.h"
#include "DolphinWX/WxUtils.h"
#include "DolphinWX/Debugger/CodeWindow.h"

#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/VR.h"

// Resources

extern "C" {
#include "DolphinWX/resources/Dolphin.c" // NOLINT: Dolphin icon
};

int g_saveSlot = 1;

#if defined(HAVE_X11) && HAVE_X11
// X11Utils nastiness that's only used here
namespace X11Utils {

Window XWindowFromHandle(void *Handle)
{
	return GDK_WINDOW_XID(gtk_widget_get_window(GTK_WIDGET(Handle)));
}

Display *XDisplayFromHandle(void *Handle)
{
	return GDK_WINDOW_XDISPLAY(gtk_widget_get_window(GTK_WIDGET(Handle)));
}

}
#endif

CRenderFrame::CRenderFrame(wxFrame* parent, wxWindowID id, const wxString& title,
		const wxPoint& pos, const wxSize& size, long style)
	: wxFrame(parent, id, title, pos, size, style)
{
	// Give it an icon
	wxIcon IconTemp;
	IconTemp.CopyFromBitmap(wxGetBitmapFromMemory(Dolphin_png));
	SetIcon(IconTemp);

	DragAcceptFiles(true);
	Bind(wxEVT_DROP_FILES, &CRenderFrame::OnDropFiles, this);
}

void CRenderFrame::OnDropFiles(wxDropFilesEvent& event)
{
	if (event.GetNumberOfFiles() != 1)
		return;
	if (File::IsDirectory(WxStrToStr(event.GetFiles()[0])))
		return;

	wxFileName file = event.GetFiles()[0];
	const std::string filepath = WxStrToStr(file.GetFullPath());

	if (file.GetExt() == "dtm")
	{
		if (Core::IsRunning())
			return;

		if (!Movie::IsReadOnly())
		{
			// let's make the read-only flag consistent at the start of a movie.
			Movie::SetReadOnly(true);
			main_frame->GetMenuBar()->FindItem(IDM_RECORD_READ_ONLY)->Check(true);
		}

		if (Movie::PlayInput(filepath))
			main_frame->BootGame("");
	}
	else if (!Core::IsRunning())
	{
		main_frame->BootGame(filepath);
	}
	else if (IsValidSavestateDropped(filepath) && Core::IsRunning())
	{
		State::LoadAs(filepath);
	}
	else
	{
		DVDInterface::ChangeDisc(filepath);
	}
}

bool CRenderFrame::IsValidSavestateDropped(const std::string& filepath)
{
	const int game_id_length = 6;
	std::ifstream file(filepath, std::ios::in | std::ios::binary);

	if (!file)
		return false;

	std::string internal_game_id(game_id_length, ' ');
	file.read(&internal_game_id[0], game_id_length);

	return internal_game_id == SConfig::GetInstance().m_LocalCoreStartupParameter.GetUniqueID();
}

#ifdef _WIN32
WXLRESULT CRenderFrame::MSWWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam)
{
	if (ARBruteForcer::ch_bruteforce)
		ARBruteForcer::ARBruteForceDriver();

	switch (nMsg)
	{
		case WM_SYSCOMMAND:
			switch (wParam)
			{
				case SC_SCREENSAVE:
				case SC_MONITORPOWER:
					if (Core::GetState() == Core::CORE_RUN && SConfig::GetInstance().m_LocalCoreStartupParameter.bDisableScreenSaver)
						break;
				default:
					return wxFrame::MSWWindowProc(nMsg, wParam, lParam);
			}
			break;

		case WM_USER:
			switch (wParam)
			{
			case WM_USER_STOP:
				main_frame->DoStop();
				break;

			case WM_USER_SETCURSOR:
				if (SConfig::GetInstance().m_LocalCoreStartupParameter.bHideCursor &&
					main_frame->RendererHasFocus() && Core::GetState() == Core::CORE_RUN)
					SetCursor(wxCURSOR_BLANK);
				else
					SetCursor(wxNullCursor);
				break;
			}
			break;

		case WM_CLOSE:
			// Let Core finish initializing before accepting any WM_CLOSE messages
			if (!Core::IsRunning()) break;
			// Use default action otherwise

		default:
			// By default let wxWidgets do what it normally does with this event
			return wxFrame::MSWWindowProc(nMsg, wParam, lParam);
	}
	return 0;
}
#endif

bool CRenderFrame::ShowFullScreen(bool show, long style)
{
#if defined WIN32
	if (show && !g_Config.bBorderlessFullscreen)
	{
		// OpenGL requires the pop-up style to activate exclusive mode.
		SetWindowStyle((GetWindowStyle() & ~wxDEFAULT_FRAME_STYLE) | wxPOPUP_WINDOW);

		// Some backends don't support exclusive fullscreen, so we
		// can't tell exactly when exclusive mode is activated.
		if (!g_Config.backend_info.bSupportsExclusiveFullscreen)
			OSD::AddMessage("Enabled exclusive fullscreen.");
	}
#endif

	bool result = wxTopLevelWindow::ShowFullScreen(show, style);

#if defined WIN32
	if (!show)
	{
		// Restore the default style.
		SetWindowStyle((GetWindowStyle() & ~wxPOPUP_WINDOW) | wxDEFAULT_FRAME_STYLE);
	}
#endif

	return result;
}

// event tables
// Notice that wxID_HELP will be processed for the 'About' menu and the toolbar
// help button.

wxDEFINE_EVENT(wxEVT_HOST_COMMAND, wxCommandEvent);

BEGIN_EVENT_TABLE(CFrame, CRenderFrame)

// Menu bar
EVT_MENU(wxID_OPEN, CFrame::OnOpen)
EVT_MENU(wxID_EXIT, CFrame::OnQuit)
EVT_MENU(IDM_HELP_WEBSITE, CFrame::OnHelp)
EVT_MENU(IDM_HELP_ONLINE_DOCS, CFrame::OnHelp)
EVT_MENU(IDM_HELP_GITHUB, CFrame::OnHelp)
EVT_MENU(wxID_ABOUT, CFrame::OnHelp)
EVT_MENU(wxID_REFRESH, CFrame::OnRefresh)
EVT_MENU(IDM_PLAY, CFrame::OnPlay)
EVT_MENU(IDM_STOP, CFrame::OnStop)
EVT_MENU(IDM_RESET, CFrame::OnReset)
EVT_MENU(IDM_RECORD, CFrame::OnRecord)
EVT_MENU(IDM_PLAY_RECORD, CFrame::OnPlayRecording)
EVT_MENU(IDM_RECORD_EXPORT, CFrame::OnRecordExport)
EVT_MENU(IDM_RECORD_READ_ONLY, CFrame::OnRecordReadOnly)
EVT_MENU(IDM_TAS_INPUT, CFrame::OnTASInput)
EVT_MENU(IDM_TOGGLE_PAUSE_MOVIE, CFrame::OnTogglePauseMovie)
EVT_MENU(IDM_SHOW_LAG, CFrame::OnShowLag)
EVT_MENU(IDM_SHOW_FRAME_COUNT, CFrame::OnShowFrameCount)
EVT_MENU(IDM_SHOW_INPUT_DISPLAY, CFrame::OnShowInputDisplay)
EVT_MENU(IDM_FRAMESTEP, CFrame::OnFrameStep)
EVT_MENU(IDM_SCREENSHOT, CFrame::OnScreenshot)
EVT_MENU(IDM_TOGGLE_DUMP_FRAMES, CFrame::OnToggleDumpFrames)
EVT_MENU(IDM_TOGGLE_DUMP_AUDIO, CFrame::OnToggleDumpAudio)
EVT_MENU(wxID_PREFERENCES, CFrame::OnConfigMain)
EVT_MENU(IDM_CONFIG_GFX_BACKEND, CFrame::OnConfigGFX)
EVT_MENU(IDM_CONFIG_AUDIO, CFrame::OnConfigAudio)
EVT_MENU(IDM_CONFIG_CONTROLLERS, CFrame::OnConfigControllers)
EVT_MENU(IDM_CONFIG_VR, CFrame::OnConfigVR)
EVT_MENU(IDM_CONFIG_HOTKEYS, CFrame::OnConfigHotkey)
#ifdef NEW_HOTKEYS
EVT_MENU(IDM_CONFIG_MENU_COMMANDS, CFrame::OnConfigMenuCommands)
#endif
EVT_MENU(IDM_SAVE_PERSPECTIVE, CFrame::OnPerspectiveMenu)
EVT_MENU(IDM_EDIT_PERSPECTIVES, CFrame::OnPerspectiveMenu)
// Drop down
EVT_MENU(IDM_PERSPECTIVES_ADD_PANE_TOP, CFrame::OnPerspectiveMenu)
EVT_MENU(IDM_PERSPECTIVES_ADD_PANE_BOTTOM, CFrame::OnPerspectiveMenu)
EVT_MENU(IDM_PERSPECTIVES_ADD_PANE_LEFT, CFrame::OnPerspectiveMenu)
EVT_MENU(IDM_PERSPECTIVES_ADD_PANE_RIGHT, CFrame::OnPerspectiveMenu)
EVT_MENU(IDM_PERSPECTIVES_ADD_PANE_CENTER, CFrame::OnPerspectiveMenu)
EVT_MENU_RANGE(IDM_PERSPECTIVES_0, IDM_PERSPECTIVES_100, CFrame::OnSelectPerspective)
EVT_MENU(IDM_ADD_PERSPECTIVE, CFrame::OnPerspectiveMenu)
EVT_MENU(IDM_TAB_SPLIT, CFrame::OnPerspectiveMenu)
EVT_MENU(IDM_NO_DOCKING, CFrame::OnPerspectiveMenu)
// Drop down float
EVT_MENU_RANGE(IDM_FLOAT_LOG_WINDOW, IDM_FLOAT_CODE_WINDOW, CFrame::OnFloatWindow)

EVT_MENU(IDM_NETPLAY, CFrame::OnNetPlay)
EVT_MENU(IDM_BROWSE, CFrame::OnBrowse)
EVT_MENU(IDM_MEMCARD, CFrame::OnMemcard)
EVT_MENU(IDM_IMPORT_SAVE, CFrame::OnImportSave)
EVT_MENU(IDM_EXPORT_ALL_SAVE, CFrame::OnExportAllSaves)
EVT_MENU(IDM_CHEATS, CFrame::OnShowCheatsWindow)
EVT_MENU(IDM_CHANGE_DISC, CFrame::OnChangeDisc)
EVT_MENU(IDM_MENU_INSTALL_WAD, CFrame::OnInstallWAD)
EVT_MENU(IDM_LIST_INSTALL_WAD, CFrame::OnInstallWAD)
EVT_MENU(IDM_LOAD_WII_MENU, CFrame::OnLoadWiiMenu)
EVT_MENU(IDM_FIFOPLAYER, CFrame::OnFifoPlayer)
EVT_MENU(IDM_DEBUGGER, CFrame::OnDebugger)

EVT_MENU(IDM_TOGGLE_FULLSCREEN, CFrame::OnToggleFullscreen)
EVT_MENU(IDM_TOGGLE_DUAL_CORE, CFrame::OnToggleDualCore)
EVT_MENU(IDM_TOGGLE_SKIP_IDLE, CFrame::OnToggleSkipIdle)
EVT_MENU(IDM_TOGGLE_TOOLBAR, CFrame::OnToggleToolbar)
EVT_MENU(IDM_TOGGLE_STATUSBAR, CFrame::OnToggleStatusbar)
EVT_MENU_RANGE(IDM_LOG_WINDOW, IDM_VIDEO_WINDOW, CFrame::OnToggleWindow)
EVT_MENU_RANGE(IDM_SHOW_SYSTEM, IDM_SHOW_STATE, CFrame::OnChangeColumnsVisible)

EVT_MENU(IDM_PURGE_CACHE, CFrame::GameListChanged)

EVT_MENU(IDM_SAVE_FIRST_STATE, CFrame::OnSaveFirstState)
EVT_MENU(IDM_UNDO_LOAD_STATE, CFrame::OnUndoLoadState)
EVT_MENU(IDM_UNDO_SAVE_STATE, CFrame::OnUndoSaveState)
EVT_MENU(IDM_LOAD_STATE_FILE, CFrame::OnLoadStateFromFile)
EVT_MENU(IDM_SAVE_STATE_FILE, CFrame::OnSaveStateToFile)
EVT_MENU(IDM_SAVE_SELECTED_SLOT, CFrame::OnSaveCurrentSlot)
EVT_MENU(IDM_LOAD_SELECTED_SLOT, CFrame::OnLoadCurrentSlot)

EVT_MENU_RANGE(IDM_LOAD_SLOT_1, IDM_LOAD_SLOT_10, CFrame::OnLoadState)
EVT_MENU_RANGE(IDM_LOAD_LAST_1, IDM_LOAD_LAST_8, CFrame::OnLoadLastState)
EVT_MENU_RANGE(IDM_SAVE_SLOT_1, IDM_SAVE_SLOT_10, CFrame::OnSaveState)
EVT_MENU_RANGE(IDM_SELECT_SLOT_1, IDM_SELECT_SLOT_10, CFrame::OnSelectSlot)
EVT_MENU_RANGE(IDM_FRAME_SKIP_0, IDM_FRAME_SKIP_9, CFrame::OnFrameSkip)
EVT_MENU_RANGE(IDM_DRIVE1, IDM_DRIVE24, CFrame::OnBootDrive)
EVT_MENU_RANGE(IDM_CONNECT_WIIMOTE1, IDM_CONNECT_BALANCEBOARD, CFrame::OnConnectWiimote)
EVT_MENU_RANGE(IDM_LIST_WAD, IDM_LIST_DRIVES, CFrame::GameListChanged)

// Other
EVT_ACTIVATE(CFrame::OnActive)
EVT_CLOSE(CFrame::OnClose)
EVT_SIZE(CFrame::OnResize)
EVT_MOVE(CFrame::OnMove)
EVT_HOST_COMMAND(wxID_ANY, CFrame::OnHostMessage)

EVT_AUI_PANE_CLOSE(CFrame::OnPaneClose)
EVT_AUINOTEBOOK_PAGE_CLOSE(wxID_ANY, CFrame::OnNotebookPageClose)
EVT_AUINOTEBOOK_ALLOW_DND(wxID_ANY, CFrame::OnAllowNotebookDnD)
EVT_AUINOTEBOOK_PAGE_CHANGED(wxID_ANY, CFrame::OnNotebookPageChanged)
EVT_AUINOTEBOOK_TAB_RIGHT_UP(wxID_ANY, CFrame::OnTab)

// Post events to child panels
EVT_MENU_RANGE(IDM_INTERPRETER, IDM_ADDRBOX, CFrame::PostEvent)
EVT_TEXT(IDM_ADDRBOX, CFrame::PostEvent)

END_EVENT_TABLE()

// ---------------
// Creation and close, quit functions


bool CFrame::InitControllers()
{
	if (!g_controller_interface.IsInit())
	{
#if defined(HAVE_X11) && HAVE_X11
		Window win = X11Utils::XWindowFromHandle(GetHandle());
		HotkeyManagerEmu::Initialize(reinterpret_cast<void*>(win));
		Pad::Initialize(reinterpret_cast<void*>(win));
		Keyboard::Initialize(reinterpret_cast<void*>(win));
		Wiimote::Initialize(reinterpret_cast<void*>(win));
#else
		HotkeyManagerEmu::Initialize(reinterpret_cast<void*>(GetHandle()));
		Pad::Initialize(reinterpret_cast<void*>(GetHandle()));
		Keyboard::Initialize(reinterpret_cast<void*>(GetHandle()));
		Wiimote::Initialize(reinterpret_cast<void*>(GetHandle()));
#endif
		return true;
	}
	return false;
}

CFrame::CFrame(wxFrame* parent,
		wxWindowID id,
		const wxString& title,
		const wxPoint& pos,
		const wxSize& size,
		bool _UseDebugger,
		bool _BatchMode,
		bool ShowLogWindow,
		long style)
	: CRenderFrame(parent, id, title, pos, size, style)
	, g_pCodeWindow(nullptr), g_NetPlaySetupDiag(nullptr), g_CheatsWindow(nullptr)
	, m_SavedPerspectives(nullptr), m_ToolBar(nullptr)
	, m_GameListCtrl(nullptr), m_Panel(nullptr)
	, m_RenderFrame(nullptr), m_RenderParent(nullptr)
	, m_LogWindow(nullptr), m_LogConfigWindow(nullptr)
	, m_FifoPlayerDlg(nullptr), UseDebugger(_UseDebugger)
	, m_bBatchMode(_BatchMode), m_bEdit(false), m_bTabSplit(false), m_bNoDocking(false)
	, m_bGameLoading(false), m_bClosing(false), m_confirmStop(false), m_menubar_shadow(nullptr)
{
	for (int i = 0; i <= IDM_CODE_WINDOW - IDM_LOG_WINDOW; i++)
		bFloatWindow[i] = false;

	if (ShowLogWindow)
		SConfig::GetInstance().m_InterfaceLogWindow = true;

	// Start debugging maximized
	if (UseDebugger)
		this->Maximize(true);

	// Debugger class
	if (UseDebugger)
	{
		g_pCodeWindow = new CCodeWindow(SConfig::GetInstance().m_LocalCoreStartupParameter, this, IDM_CODE_WINDOW);
		LoadIniPerspectives();
		g_pCodeWindow->Load();
	}

	// Create toolbar bitmaps
	InitBitmaps();

	// Give it a status bar
	SetStatusBar(CreateStatusBar(2, wxST_SIZEGRIP, ID_STATUSBAR));
	if (!SConfig::GetInstance().m_InterfaceStatusbar)
		GetStatusBar()->Hide();

	// Give it a menu bar
	wxMenuBar* menubar_active = CreateMenu();
	SetMenuBar(menubar_active);
	// Create a menubar to service requests while the real menubar is hidden from the screen
	m_menubar_shadow = CreateMenu();

	// ---------------
	// Main panel
	// This panel is the parent for rendering and it holds the gamelistctrl
	m_Panel = new wxPanel(this, IDM_MPANEL, wxDefaultPosition, wxDefaultSize, 0);

	m_GameListCtrl = new CGameListCtrl(m_Panel, wxID_ANY,
	        wxDefaultPosition, wxDefaultSize,
	        wxLC_REPORT | wxSUNKEN_BORDER | wxLC_ALIGN_LEFT);
	m_GameListCtrl->Bind(wxEVT_LIST_ITEM_ACTIVATED, &CFrame::OnGameListCtrlItemActivated, this);

	wxBoxSizer *sizerPanel = new wxBoxSizer(wxHORIZONTAL);
	sizerPanel->Add(m_GameListCtrl, 1, wxEXPAND | wxALL);
	m_Panel->SetSizer(sizerPanel);
	// ---------------

	// Manager
	m_Mgr = new wxAuiManager(this, wxAUI_MGR_DEFAULT | wxAUI_MGR_LIVE_RESIZE);

	m_Mgr->AddPane(m_Panel, wxAuiPaneInfo()
			.Name("Pane 0").Caption("Pane 0").PaneBorder(false)
			.CaptionVisible(false).Layer(0).Center().Show());
	if (!g_pCodeWindow)
		m_Mgr->AddPane(CreateEmptyNotebook(), wxAuiPaneInfo()
				.Name("Pane 1").Caption(_("Logging")).CaptionVisible(true)
				.Layer(0).FloatingSize(wxSize(600, 350)).CloseButton(true).Hide());
	AuiFullscreen = m_Mgr->SavePerspective();

	// Create toolbar
	RecreateToolbar();
	if (!SConfig::GetInstance().m_InterfaceToolbar) DoToggleToolbar(false);

	m_LogWindow = new CLogWindow(this, IDM_LOG_WINDOW);
	m_LogWindow->Hide();
	m_LogWindow->Disable();

	for (int i = 0; i < 8; ++i)
		g_TASInputDlg[i] = new TASInputDlg(this);

	Movie::SetGCInputManip(GCTASManipFunction);
	Movie::SetWiiInputManip(WiiTASManipFunction);

	State::SetOnAfterLoadCallback(OnAfterLoadCallback);
	Core::SetOnStoppedCallback(OnStoppedCallback);

	// Setup perspectives
	if (g_pCodeWindow)
	{
		// Load perspective
		DoLoadPerspective();
	}
	else
	{
		if (SConfig::GetInstance().m_InterfaceLogWindow)
			ToggleLogWindow(true);
		if (SConfig::GetInstance().m_InterfaceLogConfigWindow)
			ToggleLogConfigWindow(true);
	}

	// Show window
	Show();

	// Commit
	m_Mgr->Update();

	#ifdef _WIN32
		SetToolTip("");
		GetToolTip()->SetAutoPop(25000);
	#endif

	#if defined(HAVE_XRANDR) && HAVE_XRANDR
		m_XRRConfig = new X11Utils::XRRConfiguration(X11Utils::XDisplayFromHandle(GetHandle()),
				X11Utils::XWindowFromHandle(GetHandle()));
	#endif

	// -------------------------
	// Connect event handlers

	m_Mgr->Bind(wxEVT_AUI_RENDER, &CFrame::OnManagerResize, this);
	// ----------

	// Update controls
	UpdateGUI();
	if (g_pCodeWindow)
		g_pCodeWindow->UpdateButtonStates();

#ifdef NEW_HOTKEYS
	// check if game is running
	InitControllers();

	m_poll_hotkey_timer.SetOwner(this);
	Bind(wxEVT_TIMER, &CFrame::PollHotkeys, this);
	m_poll_hotkey_timer.Start(1000 / 60, wxTIMER_CONTINUOUS);
#endif
}
// Destructor
CFrame::~CFrame()
{
#ifdef NEW_HOTKEYS
	Wiimote::Shutdown();
	Keyboard::Shutdown();
	Pad::Shutdown();
	HotkeyManagerEmu::Shutdown();

#endif
	drives.clear();

	#if defined(HAVE_XRANDR) && HAVE_XRANDR
		delete m_XRRConfig;
	#endif

	ClosePages();

	delete m_Mgr;

	// This object is owned by us, not wxw
	m_menubar_shadow->Destroy();
	m_menubar_shadow = nullptr;
}

bool CFrame::RendererIsFullscreen()
{
	bool fullscreen = false;

	if (Core::GetState() == Core::CORE_RUN || Core::GetState() == Core::CORE_PAUSE)
	{
		fullscreen = m_RenderFrame->IsFullScreen();
	}

#if defined(__APPLE__)
	if (m_RenderFrame != nullptr)
	{
		NSView *view = (NSView *) m_RenderFrame->GetHandle();
		NSWindow *window = [view window];

		fullscreen = (([window styleMask] & NSFullScreenWindowMask) == NSFullScreenWindowMask);
	}
#endif

	return fullscreen;
}

void CFrame::OnQuit(wxCommandEvent& WXUNUSED (event))
{
	Close(true);
}

// --------
// Events
void CFrame::OnActive(wxActivateEvent& event)
{
	if (Core::GetState() == Core::CORE_RUN || Core::GetState() == Core::CORE_PAUSE)
	{
		if (event.GetActive() && event.GetEventObject() == m_RenderFrame)
		{
			if (SConfig::GetInstance().m_LocalCoreStartupParameter.bRenderToMain)
				m_RenderParent->SetFocus();

			if (SConfig::GetInstance().m_LocalCoreStartupParameter.bHideCursor &&
					Core::GetState() == Core::CORE_RUN)
				m_RenderParent->SetCursor(wxCURSOR_BLANK);
		}
		else
		{
			if (SConfig::GetInstance().m_LocalCoreStartupParameter.bHideCursor)
				m_RenderParent->SetCursor(wxNullCursor);
		}
	}
	event.Skip();
}

void CFrame::OnClose(wxCloseEvent& event)
{
	m_bClosing = true;

	// Before closing the window we need to shut down the emulation core.
	// We'll try to close this window again once that is done.
	if (Core::GetState() != Core::CORE_UNINITIALIZED)
	{
		DoStop();
		if (event.CanVeto())
		{
			event.Veto();
		}
		return;
	}

	// Stop Dolphin from saving the minimized Xpos and Ypos
	if (main_frame->IsIconized())
		main_frame->Iconize(false);

	// Don't forget the skip or the window won't be destroyed
	event.Skip();

	// Save GUI settings
	if (g_pCodeWindow)
	{
		SaveIniPerspectives();
	}
	else
	{
		// Close the log window now so that its settings are saved
		if (m_LogWindow)
			m_LogWindow->Close();
		m_LogWindow = nullptr;
	}


	// Uninit
	m_Mgr->UnInit();
}

// Post events

// Warning: This may cause an endless loop if the event is propagated back to its parent
void CFrame::PostEvent(wxCommandEvent& event)
{
	if (g_pCodeWindow &&
		event.GetId() >= IDM_INTERPRETER &&
		event.GetId() <= IDM_ADDRBOX)
	{
		event.StopPropagation();
		g_pCodeWindow->GetEventHandler()->AddPendingEvent(event);
	}
	else
	{
		event.Skip();
	}
}

void CFrame::OnMove(wxMoveEvent& event)
{
	event.Skip();

	if (!IsMaximized() &&
		!(SConfig::GetInstance().m_LocalCoreStartupParameter.bRenderToMain && RendererIsFullscreen()))
	{
		SConfig::GetInstance().m_LocalCoreStartupParameter.iPosX = GetPosition().x;
		SConfig::GetInstance().m_LocalCoreStartupParameter.iPosY = GetPosition().y;
	}
}

void CFrame::OnResize(wxSizeEvent& event)
{
	event.Skip();

	if (!IsMaximized() &&
		!(SConfig::GetInstance().m_LocalCoreStartupParameter.bRenderToMain && RendererIsFullscreen()) &&
		!(Core::GetState() != Core::CORE_UNINITIALIZED &&
			SConfig::GetInstance().m_LocalCoreStartupParameter.bRenderToMain &&
			SConfig::GetInstance().m_LocalCoreStartupParameter.bRenderWindowAutoSize))
	{
		SConfig::GetInstance().m_LocalCoreStartupParameter.iWidth = GetSize().GetWidth();
		SConfig::GetInstance().m_LocalCoreStartupParameter.iHeight = GetSize().GetHeight();
	}

	// Make sure the logger pane is a sane size
	if (!g_pCodeWindow && m_LogWindow && m_Mgr->GetPane("Pane 1").IsShown() &&
			!m_Mgr->GetPane("Pane 1").IsFloating() &&
			(m_LogWindow->x > GetClientRect().GetWidth() ||
			 m_LogWindow->y > GetClientRect().GetHeight()))
		ShowResizePane();
}

// Host messages

#ifdef _WIN32
WXLRESULT CFrame::MSWWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam)
{
	if (WM_SYSCOMMAND == nMsg && (SC_SCREENSAVE == wParam || SC_MONITORPOWER == wParam))
	{
		return 0;
	}
	else if (nMsg == WM_QUERYENDSESSION)
	{
		// Indicate that the application will be able to close
		return 1;
	}
	else if (nMsg == WM_ENDSESSION)
	{
		// Actually trigger the close now
		Close(true);
		return 0;
	}
	else
	{
		return wxFrame::MSWWindowProc(nMsg, wParam, lParam);
	}
}
#endif

void CFrame::UpdateTitle(const std::string &str)
{
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bRenderToMain &&
	    SConfig::GetInstance().m_InterfaceStatusbar)
	{
		GetStatusBar()->SetStatusText(str, 0);
		m_RenderFrame->SetTitle(scm_rev_str);
	}
	else
	{
		std::string titleStr = StringFromFormat("%s | %s", scm_rev_str, str.c_str());
		m_RenderFrame->SetTitle(titleStr);
	}
}

void CFrame::OnHostMessage(wxCommandEvent& event)
{
	switch (event.GetId())
	{
	case IDM_UPDATE_GUI:
		UpdateGUI();
		break;

	case IDM_UPDATE_STATUS_BAR:
		if (GetStatusBar() != nullptr)
			GetStatusBar()->SetStatusText(event.GetString(), event.GetInt());
		break;

	case IDM_UPDATE_TITLE:
		UpdateTitle(WxStrToStr(event.GetString()));
		break;

	case IDM_WINDOW_SIZE_REQUEST:
		{
			std::pair<int, int> *win_size = (std::pair<int, int> *)(event.GetClientData());
			OnRenderWindowSizeRequest(win_size->first, win_size->second);
			delete win_size;
		}
		break;

	case IDM_FULLSCREEN_REQUEST:
		{
			bool enable_fullscreen = event.GetInt() == 0 ? false : true;
			ToggleDisplayMode(enable_fullscreen);
			if (m_RenderFrame != nullptr)
				m_RenderFrame->ShowFullScreen(enable_fullscreen);

			// If the stop dialog initiated this fullscreen switch then we need
			// to pause the emulator after we've completed the switch.
			// TODO: Allow the renderer to switch fullscreen modes while paused.
			if (m_confirmStop)
				Core::SetState(Core::CORE_PAUSE);
		}
		break;

	case WM_USER_CREATE:
		if (SConfig::GetInstance().m_LocalCoreStartupParameter.bHideCursor)
			m_RenderParent->SetCursor(wxCURSOR_BLANK);
		break;

#ifdef __WXGTK__
	case IDM_PANIC:
		{
			wxString caption = event.GetString().BeforeFirst(':');
			wxString text = event.GetString().AfterFirst(':');
			bPanicResult = (wxYES == wxMessageBox(text,
						caption, event.GetInt() ? wxYES_NO : wxOK, wxWindow::FindFocus()));
			panic_event.Set();
		}
		break;
#endif

	case WM_USER_STOP:
		DoStop();
		break;

	case IDM_STOPPED:
		OnStopped();
		break;
	}
}

void CFrame::OnRenderWindowSizeRequest(int width, int height)
{
	if (!Core::IsRunning() ||
			!SConfig::GetInstance().m_LocalCoreStartupParameter.bRenderWindowAutoSize ||
			RendererIsFullscreen() || m_RenderFrame->IsMaximized() || g_has_hmd)
		return;

	int old_width, old_height, log_width = 0, log_height = 0;
	m_RenderFrame->GetClientSize(&old_width, &old_height);

	// Add space for the log/console/debugger window
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bRenderToMain &&
			(SConfig::GetInstance().m_InterfaceLogWindow ||
			 SConfig::GetInstance().m_InterfaceLogConfigWindow) &&
			!m_Mgr->GetPane("Pane 1").IsFloating())
	{
		switch (m_Mgr->GetPane("Pane 1").dock_direction)
		{
			case wxAUI_DOCK_LEFT:
			case wxAUI_DOCK_RIGHT:
				log_width = m_Mgr->GetPane("Pane 1").rect.GetWidth();
				break;
			case wxAUI_DOCK_TOP:
			case wxAUI_DOCK_BOTTOM:
				log_height = m_Mgr->GetPane("Pane 1").rect.GetHeight();
				break;
		}
	}

	if (old_width != width + log_width || old_height != height + log_height)
		m_RenderFrame->SetClientSize(width + log_width, height + log_height);
}

bool CFrame::RendererHasFocus()
{
	if (m_RenderParent == nullptr)
		return false;
#ifdef _WIN32
	HWND window = GetForegroundWindow();
	if (window == nullptr)
		return false;

	if (m_RenderFrame->GetHWND() == window)
		return true;
#else
	wxWindow *window = wxWindow::FindFocus();
	if (window == nullptr)
		return false;
	// Why these different cases?
	if (m_RenderParent == window ||
	    m_RenderParent == window->GetParent() ||
	    m_RenderParent->GetParent() == window->GetParent())
	{
		return true;
	}
#endif
	return false;
}

bool CFrame::UIHasFocus()
{
	// UIHasFocus should return true any time any one of our UI
	// windows has the focus, including any dialogs or other windows.
	//
	// wxWindow::FindFocus() returns the current wxWindow which has
	// focus. If it's not one of our windows, then it will return
	// null.

	wxWindow *focusWindow = wxWindow::FindFocus();
	return (focusWindow != nullptr);
}

void CFrame::OnGameListCtrlItemActivated(wxListEvent& WXUNUSED(event))
{
	// Show all platforms and regions if...
	// 1. All platforms are set to hide
	// 2. All Regions are set to hide
	// Otherwise call BootGame to either...
	// 1. Boot the selected iso
	// 2. Boot the default or last loaded iso.
	// 3. Call BrowseForDirectory if the gamelist is empty
	if (!m_GameListCtrl->GetISO(0) &&
		!((SConfig::GetInstance().m_ListGC &&
		SConfig::GetInstance().m_ListWii &&
		SConfig::GetInstance().m_ListWad) &&
		(SConfig::GetInstance().m_ListJap &&
		SConfig::GetInstance().m_ListUsa  &&
		SConfig::GetInstance().m_ListPal  &&
		SConfig::GetInstance().m_ListAustralia &&
		SConfig::GetInstance().m_ListFrance &&
		SConfig::GetInstance().m_ListGermany &&
		SConfig::GetInstance().m_ListWorld &&
		SConfig::GetInstance().m_ListItaly &&
		SConfig::GetInstance().m_ListKorea &&
		SConfig::GetInstance().m_ListNetherlands &&
		SConfig::GetInstance().m_ListRussia &&
		SConfig::GetInstance().m_ListSpain &&
		SConfig::GetInstance().m_ListTaiwan &&
		SConfig::GetInstance().m_ListUnknown)))
	{
		SConfig::GetInstance().m_ListGC =
		SConfig::GetInstance().m_ListWii =
		SConfig::GetInstance().m_ListWad =
		SConfig::GetInstance().m_ListJap =
		SConfig::GetInstance().m_ListUsa =
		SConfig::GetInstance().m_ListPal =
		SConfig::GetInstance().m_ListAustralia =
		SConfig::GetInstance().m_ListFrance =
		SConfig::GetInstance().m_ListGermany =
		SConfig::GetInstance().m_ListWorld =
		SConfig::GetInstance().m_ListItaly =
		SConfig::GetInstance().m_ListKorea =
		SConfig::GetInstance().m_ListNetherlands =
		SConfig::GetInstance().m_ListRussia =
		SConfig::GetInstance().m_ListSpain =
		SConfig::GetInstance().m_ListTaiwan =
		SConfig::GetInstance().m_ListUnknown = true;

		GetMenuBar()->FindItem(IDM_LIST_GC)->Check(true);
		GetMenuBar()->FindItem(IDM_LIST_WII)->Check(true);
		GetMenuBar()->FindItem(IDM_LIST_WAD)->Check(true);
		GetMenuBar()->FindItem(IDM_LIST_JAP)->Check(true);
		GetMenuBar()->FindItem(IDM_LIST_USA)->Check(true);
		GetMenuBar()->FindItem(IDM_LIST_PAL)->Check(true);
		GetMenuBar()->FindItem(IDM_LIST_AUSTRALIA)->Check(true);
		GetMenuBar()->FindItem(IDM_LIST_FRANCE)->Check(true);
		GetMenuBar()->FindItem(IDM_LIST_GERMANY)->Check(true);
		GetMenuBar()->FindItem(IDM_LIST_WORLD)->Check(true);
		GetMenuBar()->FindItem(IDM_LIST_ITALY)->Check(true);
		GetMenuBar()->FindItem(IDM_LIST_KOREA)->Check(true);
		GetMenuBar()->FindItem(IDM_LIST_NETHERLANDS)->Check(true);
		GetMenuBar()->FindItem(IDM_LIST_RUSSIA)->Check(true);
		GetMenuBar()->FindItem(IDM_LIST_SPAIN)->Check(true);
		GetMenuBar()->FindItem(IDM_LIST_TAIWAN)->Check(true);
		GetMenuBar()->FindItem(IDM_LIST_UNKNOWN)->Check(true);

		m_GameListCtrl->Update();
	}
	else if (!m_GameListCtrl->GetISO(0))
	{
		m_GameListCtrl->BrowseForDirectory();
	}
	else
	{
		// Game started by double click
		BootGame("");
	}
}

static bool IsHotkey(wxKeyEvent &event, int id, bool held = false)
{
#ifdef NEW_HOTKEYS
	if (Core::GetState() == Core::CORE_UNINITIALIZED)
		return false;

	// Input event hotkey
	if (event.GetKeyCode() == WXK_NONE)
	{
		return HotkeyManagerEmu::IsPressed(id, held);
	}

	return (event.GetKeyCode() != WXK_NONE &&
		event.GetKeyCode() == SConfig::GetInstance().m_LocalCoreStartupParameter.iHotkey[id] &&
		event.GetModifiers() == SConfig::GetInstance().m_LocalCoreStartupParameter.iHotkeyModifier[id]);
#else
	return (event.GetKeyCode() != WXK_NONE &&
		event.GetKeyCode() == SConfig::GetInstance().m_LocalCoreStartupParameter.iHotkey[id] &&
		event.GetModifiers() == SConfig::GetInstance().m_LocalCoreStartupParameter.iHotkeyModifier[id] &&
		true == SConfig::GetInstance().m_LocalCoreStartupParameter.bHotkeyKBM[id]);
#endif

}

static bool IsVRSettingsKey(wxKeyEvent &event, int Id)
{
	return (event.GetKeyCode() != WXK_NONE &&
		event.GetKeyCode() == SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettings[Id] &&
		event.GetModifiers() == SConfig::GetInstance().m_LocalCoreStartupParameter.iVRSettingsModifier[Id] &&
		true == SConfig::GetInstance().m_LocalCoreStartupParameter.bVRSettingsKBM[Id]);
}

int GetCmdForHotkey(unsigned int key)
{
	switch (key)
	{
	case HK_OPEN: return wxID_OPEN;
	case HK_CHANGE_DISC: return IDM_CHANGE_DISC;
	case HK_REFRESH_LIST: return wxID_REFRESH;
	case HK_PLAY_PAUSE: return IDM_PLAY;
	case HK_STOP: return IDM_STOP;
	case HK_RESET: return IDM_RESET;
	case HK_FRAME_ADVANCE: return IDM_FRAMESTEP;
	case HK_START_RECORDING: return IDM_RECORD;
	case HK_PLAY_RECORDING: return IDM_PLAY_RECORD;
	case HK_EXPORT_RECORDING: return IDM_RECORD_EXPORT;
	case HK_READ_ONLY_MODE: return IDM_RECORD_READ_ONLY;
	case HK_FULLSCREEN: return IDM_TOGGLE_FULLSCREEN;
	case HK_SCREENSHOT: return IDM_SCREENSHOT;
	case HK_EXIT: return wxID_EXIT;

	case HK_WIIMOTE1_CONNECT: return IDM_CONNECT_WIIMOTE1;
	case HK_WIIMOTE2_CONNECT: return IDM_CONNECT_WIIMOTE2;
	case HK_WIIMOTE3_CONNECT: return IDM_CONNECT_WIIMOTE3;
	case HK_WIIMOTE4_CONNECT: return IDM_CONNECT_WIIMOTE4;
	case HK_BALANCEBOARD_CONNECT: return IDM_CONNECT_BALANCEBOARD;

	case HK_LOAD_STATE_SLOT_1: return IDM_LOAD_SLOT_1;
	case HK_LOAD_STATE_SLOT_2: return IDM_LOAD_SLOT_2;
	case HK_LOAD_STATE_SLOT_3: return IDM_LOAD_SLOT_3;
	case HK_LOAD_STATE_SLOT_4: return IDM_LOAD_SLOT_4;
	case HK_LOAD_STATE_SLOT_5: return IDM_LOAD_SLOT_5;
	case HK_LOAD_STATE_SLOT_6: return IDM_LOAD_SLOT_6;
	case HK_LOAD_STATE_SLOT_7: return IDM_LOAD_SLOT_7;
	case HK_LOAD_STATE_SLOT_8: return IDM_LOAD_SLOT_8;
	case HK_LOAD_STATE_SLOT_9: return IDM_LOAD_SLOT_9;
	case HK_LOAD_STATE_SLOT_10: return IDM_LOAD_SLOT_10;

	case HK_SAVE_STATE_SLOT_1: return IDM_SAVE_SLOT_1;
	case HK_SAVE_STATE_SLOT_2: return IDM_SAVE_SLOT_2;
	case HK_SAVE_STATE_SLOT_3: return IDM_SAVE_SLOT_3;
	case HK_SAVE_STATE_SLOT_4: return IDM_SAVE_SLOT_4;
	case HK_SAVE_STATE_SLOT_5: return IDM_SAVE_SLOT_5;
	case HK_SAVE_STATE_SLOT_6: return IDM_SAVE_SLOT_6;
	case HK_SAVE_STATE_SLOT_7: return IDM_SAVE_SLOT_7;
	case HK_SAVE_STATE_SLOT_8: return IDM_SAVE_SLOT_8;
	case HK_SAVE_STATE_SLOT_9: return IDM_SAVE_SLOT_9;
	case HK_SAVE_STATE_SLOT_10: return IDM_SAVE_SLOT_10;

	case HK_LOAD_LAST_STATE_1: return IDM_LOAD_LAST_1;
	case HK_LOAD_LAST_STATE_2: return IDM_LOAD_LAST_2;
	case HK_LOAD_LAST_STATE_3: return IDM_LOAD_LAST_3;
	case HK_LOAD_LAST_STATE_4: return IDM_LOAD_LAST_4;
	case HK_LOAD_LAST_STATE_5: return IDM_LOAD_LAST_5;
	case HK_LOAD_LAST_STATE_6: return IDM_LOAD_LAST_6;
	case HK_LOAD_LAST_STATE_7: return IDM_LOAD_LAST_7;
	case HK_LOAD_LAST_STATE_8: return IDM_LOAD_LAST_8;

	case HK_SAVE_FIRST_STATE: return IDM_SAVE_FIRST_STATE;
	case HK_UNDO_LOAD_STATE: return IDM_UNDO_LOAD_STATE;
	case HK_UNDO_SAVE_STATE: return IDM_UNDO_SAVE_STATE;
	case HK_LOAD_STATE_FILE: return IDM_LOAD_STATE_FILE;
	case HK_SAVE_STATE_FILE: return IDM_SAVE_STATE_FILE;

	case HK_SELECT_STATE_SLOT_1: return IDM_SELECT_SLOT_1;
	case HK_SELECT_STATE_SLOT_2: return IDM_SELECT_SLOT_2;
	case HK_SELECT_STATE_SLOT_3: return IDM_SELECT_SLOT_3;
	case HK_SELECT_STATE_SLOT_4: return IDM_SELECT_SLOT_4;
	case HK_SELECT_STATE_SLOT_5: return IDM_SELECT_SLOT_5;
	case HK_SELECT_STATE_SLOT_6: return IDM_SELECT_SLOT_6;
	case HK_SELECT_STATE_SLOT_7: return IDM_SELECT_SLOT_7;
	case HK_SELECT_STATE_SLOT_8: return IDM_SELECT_SLOT_8;
	case HK_SELECT_STATE_SLOT_9: return IDM_SELECT_SLOT_9;
	case HK_SELECT_STATE_SLOT_10: return IDM_SELECT_SLOT_10;
	case HK_SAVE_STATE_SLOT_SELECTED: return IDM_SAVE_SELECTED_SLOT;
	case HK_LOAD_STATE_SLOT_SELECTED: return IDM_LOAD_SELECTED_SLOT;

	case HK_FREELOOK_DECREASE_SPEED: return IDM_FREELOOK_DECREASE_SPEED;
	case HK_FREELOOK_INCREASE_SPEED: return IDM_FREELOOK_INCREASE_SPEED;
	case HK_FREELOOK_RESET_SPEED: return IDM_FREELOOK_RESET_SPEED;
	case HK_FREELOOK_LEFT: return IDM_FREELOOK_LEFT;
	case HK_FREELOOK_RIGHT: return IDM_FREELOOK_RIGHT;
	case HK_FREELOOK_UP: return IDM_FREELOOK_UP;
	case HK_FREELOOK_DOWN: return IDM_FREELOOK_DOWN;
	case HK_FREELOOK_ZOOM_IN: return IDM_FREELOOK_ZOOM_IN;
	case HK_FREELOOK_ZOOM_OUT: return IDM_FREELOOK_ZOOM_OUT;
	case HK_FREELOOK_RESET: return IDM_FREELOOK_RESET;
	}

	return -1;
}

void OnAfterLoadCallback()
{
	// warning: this gets called from the CPU thread, so we should only queue things to do on the proper thread
	if (main_frame)
	{
		wxCommandEvent event(wxEVT_HOST_COMMAND, IDM_UPDATE_GUI);
		main_frame->GetEventHandler()->AddPendingEvent(event);
	}
}

void OnStoppedCallback()
{
	// warning: this gets called from the EmuThread, so we should only queue things to do on the proper thread
	if (main_frame)
	{
		wxCommandEvent event(wxEVT_HOST_COMMAND, IDM_STOPPED);
		main_frame->GetEventHandler()->AddPendingEvent(event);
	}
}

void GCTASManipFunction(GCPadStatus* PadStatus, int controllerID)
{
	if (main_frame)
		main_frame->g_TASInputDlg[controllerID]->GetValues(PadStatus);
}

void WiiTASManipFunction(u8* data, WiimoteEmu::ReportFeatures rptf, int controllerID, int ext, const wiimote_key key)
{
	if (main_frame)
	{
		main_frame->g_TASInputDlg[controllerID + 4]->GetValues(data, rptf, ext, key);
	}
}

bool TASInputHasFocus()
{
	for (int i = 0; i < 8; ++i)
	{
		if (main_frame->g_TASInputDlg[i]->TASHasFocus())
			return true;
	}
	return false;
}

#ifdef NEW_HOTKEYS
void CFrame::OnKeyDown(wxKeyEvent& event)
{
	if (Core::GetState() != Core::CORE_UNINITIALIZED &&
	    (RendererHasFocus() || TASInputHasFocus()))
	{
		ParseHotkeys(event);

		if (g_has_hmd && event.GetModifiers() == wxMOD_SHIFT)
		{
			switch (event.GetKeyCode())
			{
			// Previous layer
			case 'B':
				g_Config.iSelectedLayer--;
				if (g_Config.iSelectedLayer < -1)
					g_Config.iSelectedLayer = -2;
				NOTICE_LOG(VR, "Selected layer %d", g_Config.iSelectedLayer);
				debug_nextScene = true;
				break;
			// Next layer
			case 'N':
				g_Config.iSelectedLayer++;
				NOTICE_LOG(VR, "Selected layer %d", g_Config.iSelectedLayer);
				debug_nextScene = true;
				break;
			case '\'':
				NOTICE_LOG(VR, "--- pressed ' ---");
				debug_nextScene = true;
				break;
			}
		}
	}
	else
	{
		event.Skip();
	}
}
#else
void CFrame::OnKeyDown(wxKeyEvent& event)
{
	if (Core::GetState() != Core::CORE_UNINITIALIZED &&
		(RendererHasFocus() || TASInputHasFocus()))
	{
		int WiimoteId = -1;
		// Toggle fullscreen
		if (IsHotkey(event, HK_FULLSCREEN))
			DoFullscreen(!RendererIsFullscreen());
		// Send Debugger keys to CodeWindow
		else if (g_pCodeWindow && (event.GetKeyCode() >= WXK_F9 && event.GetKeyCode() <= WXK_F11))
			event.Skip();
		// Pause and Unpause
		else if (IsHotkey(event, HK_PLAY_PAUSE))
			DoPause();
		// Stop
		else if (IsHotkey(event, HK_STOP))
			DoStop();
		// Screenshot hotkey
		else if (IsHotkey(event, HK_SCREENSHOT))
			Core::SaveScreenShot();
		else if (IsHotkey(event, HK_EXIT))
			wxPostEvent(this, wxCommandEvent(wxID_EXIT));
		else if (IsHotkey(event, HK_VOLUME_DOWN))
			AudioCommon::DecreaseVolume(3);
		else if (IsHotkey(event, HK_VOLUME_UP))
			AudioCommon::IncreaseVolume(3);
		else if (IsHotkey(event, HK_VOLUME_TOGGLE_MUTE))
			AudioCommon::ToggleMuteVolume();
		// Wiimote connect and disconnect hotkeys
		else if (IsHotkey(event, HK_WIIMOTE1_CONNECT))
			WiimoteId = 0;
		else if (IsHotkey(event, HK_WIIMOTE2_CONNECT))
			WiimoteId = 1;
		else if (IsHotkey(event, HK_WIIMOTE3_CONNECT))
			WiimoteId = 2;
		else if (IsHotkey(event, HK_WIIMOTE4_CONNECT))
			WiimoteId = 3;
		else if (IsHotkey(event, HK_BALANCEBOARD_CONNECT))
			WiimoteId = 4;
		else if (IsHotkey(event, HK_TOGGLE_IR))
		{
			OSDChoice = 1;
			// Toggle native resolution
			if (++g_Config.iEFBScale > SCALE_4X)
				g_Config.iEFBScale = SCALE_AUTO;
		}
		else if (IsHotkey(event, HK_TOGGLE_AR))
		{
			OSDChoice = 2;
			// Toggle aspect ratio
			g_Config.iAspectRatio = (g_Config.iAspectRatio + 1) & 3;
		}
		else if (IsHotkey(event, HK_TOGGLE_EFBCOPIES))
		{
			OSDChoice = 3;
			// Toggle EFB copies between EFB2RAM and EFB2Texture
			if (!g_Config.bEFBCopyEnable)
			{
				OSD::AddMessage("EFB Copies are disabled, enable them in Graphics settings for toggling", 6000);
			}
			else
			{
				g_Config.bSkipEFBCopyToRam = !g_Config.bSkipEFBCopyToRam;
			}
		}
		else if (IsHotkey(event, HK_TOGGLE_FOG))
		{
			OSDChoice = 4;
			g_Config.bDisableFog = !g_Config.bDisableFog;
		}
		else if (IsHotkey(event, HK_TOGGLE_THROTTLE))
		{
			Core::SetIsFramelimiterTempDisabled(true);
		}
		else if (IsHotkey(event, HK_DECREASE_FRAME_LIMIT))
		{
			if (--SConfig::GetInstance().m_Framelimit > 0x19)
				SConfig::GetInstance().m_Framelimit = 0x19;
		}
		else if (IsHotkey(event, HK_INCREASE_FRAME_LIMIT))
		{
			if (++SConfig::GetInstance().m_Framelimit > 0x19)
				SConfig::GetInstance().m_Framelimit = 0;
		}
		else if (IsHotkey(event, HK_SAVE_STATE_SLOT_SELECTED))
		{
			State::Save(g_saveSlot);
		}
		else if (IsHotkey(event, HK_LOAD_STATE_SLOT_SELECTED))
		{
			State::Load(g_saveSlot);
		}
		else if (IsHotkey(event, HK_DECREASE_DEPTH))
		{
			if (--g_Config.iStereoDepth < 0)
				g_Config.iStereoDepth = 0;
		}
		else if (IsHotkey(event, HK_INCREASE_DEPTH))
		{
			if (++g_Config.iStereoDepth > 100)
				g_Config.iStereoDepth = 100;
		}
		else if (IsHotkey(event, HK_DECREASE_CONVERGENCE))
		{
			if (--g_Config.iStereoConvergence < 0)
				g_Config.iStereoConvergence = 0;
		}
		else if (IsHotkey(event, HK_INCREASE_CONVERGENCE))
		{
			if (++g_Config.iStereoConvergence > 500)
				g_Config.iStereoConvergence = 500;
		}

		else
		{
			for (int i = HK_SELECT_STATE_SLOT_1; i < HK_SELECT_STATE_SLOT_10; ++i)
			{
				if (IsHotkey(event, i))
				{
					wxCommandEvent slot_event;
					slot_event.SetId(i + IDM_SELECT_SLOT_1 - HK_SELECT_STATE_SLOT_1);
					CFrame::OnSelectSlot(slot_event);
				}
			}

			unsigned int i = NUM_HOTKEYS;
			if (!SConfig::GetInstance().m_LocalCoreStartupParameter.bRenderToMain || TASInputHasFocus())
			{
				for (i = 0; i < NUM_HOTKEYS; i++)
				{
					if (IsHotkey(event, i))
					{
						int cmd = GetCmdForHotkey(i);
						if (cmd >= 0)
						{
							wxCommandEvent evt(wxEVT_MENU, cmd);
							wxMenuItem *item = GetMenuBar()->FindItem(cmd);
							if (item && item->IsCheckable())
							{
								item->wxMenuItemBase::Toggle();
								evt.SetInt(item->IsChecked());
							}
							GetEventHandler()->AddPendingEvent(evt);
							break;
						}
					}
				}
			}
			// On OS X, we claim all keyboard events while
			// emulation is running to avoid wxWidgets sounding
			// the system beep for unhandled key events when
			// receiving pad/Wiimote keypresses which take an
			// entirely different path through the HID subsystem.
#ifndef __APPLE__
			// On other platforms, we leave the key event alone
			// so it can be passed on to the windowing system.
			if (i == NUM_HOTKEYS)
				event.Skip();
#endif
		}

		// Actually perform the Wiimote connection or disconnection
		if (WiimoteId >= 0)
		{
			wxCommandEvent evt;
			evt.SetId(IDM_CONNECT_WIIMOTE1 + WiimoteId);
			OnConnectWiimote(evt);
		}

		if (g_has_hmd)
		{
			// Maths is probably cheaper than if statements, so always recalculate
			float freeLookSpeed = 0.1f * g_ActiveConfig.fFreeLookSensitivity;

			if (IsHotkey(event, HK_FREELOOK_DECREASE_SPEED))
				g_ActiveConfig.fFreeLookSensitivity /= 2.0f;
			else if (IsHotkey(event, HK_FREELOOK_INCREASE_SPEED))
				g_ActiveConfig.fFreeLookSensitivity *= 2.0f;
			else if (IsHotkey(event, HK_FREELOOK_RESET_SPEED))
				g_ActiveConfig.fFreeLookSensitivity = 1.0f;
			else if (IsHotkey(event, HK_FREELOOK_UP))
				VertexShaderManager::TranslateView(0.0f, 0.0f, -freeLookSpeed / 2);
			else if (IsHotkey(event, HK_FREELOOK_DOWN))
				VertexShaderManager::TranslateView(0.0f, 0.0f, freeLookSpeed / 2);
			else if (IsHotkey(event, HK_FREELOOK_LEFT))
				VertexShaderManager::TranslateView(freeLookSpeed, 0.0f);
			else if (IsHotkey(event, HK_FREELOOK_RIGHT))
				VertexShaderManager::TranslateView(-freeLookSpeed, 0.0f);
			else if (IsHotkey(event, HK_FREELOOK_ZOOM_IN))
				VertexShaderManager::TranslateView(0.0f, freeLookSpeed);
			else if (IsHotkey(event, HK_FREELOOK_ZOOM_OUT))
				VertexShaderManager::TranslateView(0.0f, -freeLookSpeed);
			else if (IsHotkey(event, HK_FREELOOK_RESET))
			{
				VertexShaderManager::ResetView();
				VR_RecenterHMD();
			}
			else if (g_has_hmd)
			{
				if (IsVRSettingsKey(event, VR_LARGER_SCALE))
				{
					// Make everything 10% bigger (and further)
					g_Config.fUnitsPerMetre /= 1.10f;
					VertexShaderManager::ScaleView(1.10f);
					NOTICE_LOG(VR, "%f units per metre (each unit is %f cm)", g_Config.fUnitsPerMetre, 100.0f / g_Config.fUnitsPerMetre);
				}
				else if (IsVRSettingsKey(event, VR_SMALLER_SCALE))
				{
					// Make everything 10% smaller (and closer)
					g_Config.fUnitsPerMetre *= 1.10f;
					VertexShaderManager::ScaleView(1.0f / 1.10f);
					NOTICE_LOG(VR, "%f units per metre (each unit is %f cm)", g_Config.fUnitsPerMetre, 100.0f / g_Config.fUnitsPerMetre);
				}
				if (IsVRSettingsKey(event, VR_GLOBAL_LARGER_SCALE))
				{
					// Make everything 10% bigger (and further)
					g_Config.fScale *= 1.10f;
					SConfig::GetInstance().SaveSingleSetting("VR", "Scale", g_Config.fScale);
					VertexShaderManager::ScaleView(1.10f);
				}
				else if (IsVRSettingsKey(event, VR_GLOBAL_SMALLER_SCALE))
				{
					// Make everything 10% smaller (and closer)
					g_Config.fScale /= 1.10f;
					SConfig::GetInstance().SaveSingleSetting("VR", "Scale", g_Config.fScale);
					VertexShaderManager::ScaleView(1.0f / 1.10f);
				}
				else if (IsVRSettingsKey(event, VR_PERMANENT_CAMERA_FORWARD)) {
					// Move camera forward 10cm
					g_Config.fCameraForward += freeLookSpeed;
					NOTICE_LOG(VR, "Camera is %5.1fm (%5.0fcm) forward", g_Config.fCameraForward, g_Config.fCameraForward * 100);
				}
				else if (IsVRSettingsKey(event, VR_PERMANENT_CAMERA_BACKWARD)) {
					// Move camera back 10cm
					g_Config.fCameraForward -= freeLookSpeed;
					NOTICE_LOG(VR, "Camera is %5.1fm (%5.0fcm) forward", g_Config.fCameraForward, g_Config.fCameraForward * 100);
				}
				else if (IsVRSettingsKey(event, VR_CAMERA_TILT_UP)) {
					// Pitch camera up 5 degrees
					g_Config.fCameraPitch += 5.0f;
					NOTICE_LOG(VR, "Camera is pitched %5.1f degrees up", g_Config.fCameraPitch);
				}
				else if (IsVRSettingsKey(event, VR_CAMERA_TILT_DOWN)) {
					// Pitch camera down 5 degrees
					g_Config.fCameraPitch -= 5.0f;
					NOTICE_LOG(VR, "Camera is pitched %5.1f degrees up", g_Config.fCameraPitch);
				}
				else if (IsVRSettingsKey(event, VR_HUD_FORWARD)) {
					// Move HUD out 10cm
					g_Config.fHudDistance += 0.1f;
					NOTICE_LOG(VR, "HUD is %5.1fm (%5.0fcm) away", g_Config.fHudDistance, g_Config.fHudDistance * 100);
				}
				else if (IsVRSettingsKey(event, VR_HUD_BACKWARD)) {
					// Move HUD in 10cm
					g_Config.fHudDistance -= 0.1f;
					if (g_Config.fHudDistance <= 0)
						g_Config.fHudDistance = 0;
					NOTICE_LOG(VR, "HUD is %5.1fm (%5.0fcm) away", g_Config.fHudDistance, g_Config.fHudDistance * 100);
				}
				else if (IsVRSettingsKey(event, VR_HUD_THICKER)) {
					// Make HUD 10cm thicker
					if (g_Config.fHudThickness < 0.01f)
						g_Config.fHudThickness = 0.01f;
					else if (g_Config.fHudThickness < 0.1f)
						g_Config.fHudThickness += 0.01f;
					else
						g_Config.fHudThickness += 0.1f;
					NOTICE_LOG(VR, "HUD is %5.2fm (%5.0fcm) thick", g_Config.fHudThickness, g_Config.fHudThickness * 100);
				}
				else if (IsVRSettingsKey(event, VR_HUD_THINNER)) {
					// Make HUD 10cm thinner
					if (g_Config.fHudThickness <= 0.01f)
						g_Config.fHudThickness = 0;
					else if (g_Config.fHudThickness <= 0.1f)
						g_Config.fHudThickness -= 0.01f;
					else
						g_Config.fHudThickness -= 0.1f;
					NOTICE_LOG(VR, "HUD is %5.2fm (%5.0fcm) thick", g_Config.fHudThickness, g_Config.fHudThickness * 100);
				}
				else if (IsVRSettingsKey(event, VR_HUD_3D_CLOSER)) {
					// Make HUD 3D elements 5% closer (and smaller)
					if (g_Config.fHud3DCloser >= 0.95f)
						g_Config.fHud3DCloser = 1;
					else
						g_Config.fHud3DCloser += 0.05f;
					NOTICE_LOG(VR, "HUD 3D Items are %5.1f%% closer", g_Config.fHud3DCloser * 100);
				}
				else if (IsVRSettingsKey(event, VR_HUD_3D_FURTHER)) {
					// Make HUD 3D elements 5% further (and smaller)
					if (g_Config.fHud3DCloser <= 0.05f)
						g_Config.fHud3DCloser = 0;
					else
						g_Config.fHud3DCloser -= 0.05f;
					NOTICE_LOG(VR, "HUD 3D Items are %5.1f%% closer", g_Config.fHud3DCloser * 100);
				}
				else if (IsVRSettingsKey(event, VR_2D_SCREEN_LARGER)) {
					// Make everything 20% smaller (and closer)
					g_Config.fScreenHeight *= 1.05f;
					NOTICE_LOG(VR, "Screen is %fm high", g_Config.fScreenHeight);
				}
				else if (IsVRSettingsKey(event, VR_2D_SCREEN_SMALLER)) {
					// Make everything 20% bigger (and further)
					g_Config.fScreenHeight /= 1.05f;
					NOTICE_LOG(VR, "Screen is %fm High", g_Config.fScreenHeight);
				}
				else if (IsVRSettingsKey(event, VR_2D_SCREEN_THICKER)) {
					// Make Screen 10cm thicker
					if (g_Config.fScreenThickness < 0.01f)
						g_Config.fScreenThickness = 0.01f;
					else if (g_Config.fScreenThickness < 0.1f)
						g_Config.fScreenThickness += 0.01f;
					else
						g_Config.fScreenThickness += 0.1f;
					NOTICE_LOG(VR, "Screen is %5.2fm (%5.0fcm) thick", g_Config.fScreenThickness, g_Config.fScreenThickness * 100);
				}
				else if (IsVRSettingsKey(event, VR_2D_SCREEN_THINNER)) {
					// Make Screen 10cm thinner
					if (g_Config.fScreenThickness <= 0.01f)
						g_Config.fScreenThickness = 0;
					else if (g_Config.fScreenThickness <= 0.1f)
						g_Config.fScreenThickness -= 0.01f;
					else
						g_Config.fScreenThickness -= 0.1f;
					NOTICE_LOG(VR, "Screen is %5.2fm (%5.0fcm) thick", g_Config.fScreenThickness, g_Config.fScreenThickness * 100);
				}
				else if (IsVRSettingsKey(event, VR_2D_CAMERA_FORWARD)) {
					// Move Screen in 10cm
					g_Config.fScreenDistance -= 0.1f;
					if (g_Config.fScreenDistance <= 0)
						g_Config.fScreenDistance = 0;
					NOTICE_LOG(VR, "Screen is %5.1fm (%5.0fcm) away", g_Config.fScreenDistance, g_Config.fScreenDistance * 100);
				}
				else if (IsVRSettingsKey(event, VR_2D_CAMERA_BACKWARD)) {
					// Move Screen out 10cm
					g_Config.fScreenDistance += 0.1f;
					NOTICE_LOG(VR, "Screen is %5.1fm (%5.0fcm) away", g_Config.fScreenDistance, g_Config.fScreenDistance * 100);
				}
				else if (IsVRSettingsKey(event, VR_2D_CAMERA_UP)) {
					// Move Screen Down (Camera Up) 10cm
					g_Config.fScreenUp -= 0.1f;
					NOTICE_LOG(VR, "Screen is %5.1fm up", g_Config.fScreenUp);
				}
				else if (IsVRSettingsKey(event, VR_2D_CAMERA_DOWN)) {
					// Move Screen Up (Camera Down) 10cm
					g_Config.fScreenUp += 0.1f;
					NOTICE_LOG(VR, "Screen is %5.1fm up", g_Config.fScreenUp);
				}
				else if (IsVRSettingsKey(event, VR_2D_CAMERA_TILT_UP)) {
					// Pitch camera up 5 degrees
					g_Config.fScreenPitch += 5.0f;
					NOTICE_LOG(VR, "2D Camera is pitched %5.1f degrees up", g_Config.fScreenPitch);
				}
				else if (IsVRSettingsKey(event, VR_2D_CAMERA_TILT_DOWN)) {
					// Pitch camera down 5 degrees
					g_Config.fScreenPitch -= 5.0f;
					NOTICE_LOG(VR, "2D Camera is pitched %5.1f degrees up", g_Config.fScreenPitch);;
				}
			}
		}

		if (g_has_hmd && event.GetModifiers() == wxMOD_SHIFT)
		{
			switch (event.GetKeyCode())
			{
				// Previous layer
			case 'B':
				g_Config.iSelectedLayer--;
				if (g_Config.iSelectedLayer < -1)
					g_Config.iSelectedLayer = -2;
				NOTICE_LOG(VR, "Selected layer %d", g_Config.iSelectedLayer);
				debug_nextScene = true;
				break;
				// Next layer
			case 'N':
				g_Config.iSelectedLayer++;
				NOTICE_LOG(VR, "Selected layer %d", g_Config.iSelectedLayer);
				debug_nextScene = true;
				break;
			case '\'':
				NOTICE_LOG(VR, "--- pressed ' ---");
				debug_nextScene = true;
				break;
			}
		}
	}
	else
	{
		event.Skip();
	}
}
#endif

void CFrame::OnKeyUp(wxKeyEvent& event)
{
	if (Core::IsRunning() && (RendererHasFocus() || TASInputHasFocus()))
	{
		if (IsHotkey(event, HK_TOGGLE_THROTTLE))
		{
			Core::SetIsFramelimiterTempDisabled(false);
		}
	}
	else
	{
		event.Skip();
	}
}

void CFrame::OnMouse(wxMouseEvent& event)
{
	// next handlers are all for FreeLook, so we don't need to check them if disabled
	if (!g_Config.bFreeLook)
	{
		event.Skip();
		return;
	}

	// Free look variables
	static bool mouseLookEnabled = false;
	static bool mouseMoveEnabled = false;
	static float lastMouse[2];

	if (event.MiddleDown())
	{
		lastMouse[0] = event.GetX();
		lastMouse[1] = event.GetY();
		mouseMoveEnabled = true;
	}
	else if (event.RightDown())
	{
		lastMouse[0] = event.GetX();
		lastMouse[1] = event.GetY();
		mouseLookEnabled = true;
	}
	else if (event.MiddleUp())
	{
		mouseMoveEnabled = false;
	}
	else if (event.RightUp())
	{
		mouseLookEnabled = false;
	}
	// no button, so it's a move event
	else if (event.GetButton() == wxMOUSE_BTN_NONE)
	{
		if (mouseLookEnabled)
		{
			VertexShaderManager::RotateView((event.GetX() - lastMouse[0]) / 200.0f,
					(event.GetY() - lastMouse[1]) / 200.0f);
			lastMouse[0] = event.GetX();
			lastMouse[1] = event.GetY();
		}

		if (mouseMoveEnabled)
		{
			if (g_has_hmd)
			{
				VertexShaderManager::TranslateView(
					(event.GetX() - lastMouse[0]) * g_ActiveConfig.fScale * g_ActiveConfig.fFreeLookSensitivity / 7.0f,
					(event.GetY() - lastMouse[1]) * g_ActiveConfig.fScale * g_ActiveConfig.fFreeLookSensitivity / 7.0f);
			}
			else
			{
				VertexShaderManager::TranslateView(
					((event.GetX() - lastMouse[0]) * g_ActiveConfig.fFreeLookSensitivity) / 7.0f,
					((event.GetY() - lastMouse[1]) * g_ActiveConfig.fFreeLookSensitivity) / 7.0f);
			}
			lastMouse[0] = event.GetX();
			lastMouse[1] = event.GetY();
		}
	}

	event.Skip();
}

void CFrame::DoFullscreen(bool enable_fullscreen)
{
	if (g_Config.bExclusiveMode && Core::GetState() == Core::CORE_PAUSE)
	{
		// A responsive renderer is required for exclusive fullscreen, but the
		// renderer can only respond in the running state. Therefore we ignore
		// fullscreen switches if we are in exclusive fullscreen, but the
		// renderer is not running.
		// TODO: Allow the renderer to switch fullscreen modes while paused.
		return;
	}

	ToggleDisplayMode(enable_fullscreen);

#if defined(__APPLE__)
	NSView *view = (NSView *)m_RenderFrame->GetHandle();
	NSWindow *window = [view window];

	if (enable_fullscreen != RendererIsFullscreen())
	{
		[window toggleFullScreen : nil];
	}
#else
	if (enable_fullscreen)
	{
		m_RenderFrame->ShowFullScreen(true, wxFULLSCREEN_ALL);
	}
	else if (!g_Config.bExclusiveMode)
	{
		// Exiting exclusive fullscreen should be done from a Renderer callback.
		// Therefore we don't exit fullscreen from here if we are in exclusive mode.
		m_RenderFrame->ShowFullScreen(false, wxFULLSCREEN_ALL);
	}
#endif

	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bRenderToMain)
	{
		if (enable_fullscreen)
		{
			// Save the current mode before going to fullscreen
			AuiCurrent = m_Mgr->SavePerspective();
			m_Mgr->LoadPerspective(AuiFullscreen, true);

			// Hide toolbar
			DoToggleToolbar(false);

			// Hide menubar (by having wxwidgets delete it)
			SetMenuBar(nullptr);

			// Hide the statusbar if enabled
			if (GetStatusBar()->IsShown())
			{
				GetStatusBar()->Hide();
				this->SendSizeEvent();
			}
		}
		else
		{
			// Restore saved perspective
			m_Mgr->LoadPerspective(AuiCurrent, true);

			// Restore toolbar to the status it was at before going fullscreen.
			DoToggleToolbar(SConfig::GetInstance().m_InterfaceToolbar);

			// Recreate the menubar if needed.
			if (wxFrame::GetMenuBar() == nullptr)
			{
				SetMenuBar(CreateMenu());
			}

			// Show statusbar if enabled
			if (SConfig::GetInstance().m_InterfaceStatusbar)
			{
				GetStatusBar()->Show();
				this->SendSizeEvent();
			}
		}
	}
	else
	{
		m_RenderFrame->Raise();
	}

	g_Config.bFullscreen = (SConfig::GetInstance().m_LocalCoreStartupParameter.bRenderToMain) ? false : enable_fullscreen;
}

const CGameListCtrl *CFrame::GetGameListCtrl() const
{
	return m_GameListCtrl;
}

void CFrame::PollHotkeys(wxTimerEvent& event)
{
	if (!HotkeyManagerEmu::IsEnabled())
		return;

	if (Core::GetState() == Core::CORE_UNINITIALIZED || Core::GetState() == Core::CORE_PAUSE)
		g_controller_interface.UpdateInput();

	if (Core::GetState() != Core::CORE_STOPPING)
	{
		HotkeyManagerEmu::GetStatus();
		wxKeyEvent keyevent = 0;

		if (IsHotkey(keyevent, HK_TOGGLE_THROTTLE))
		{
			Core::SetIsFramelimiterTempDisabled(false);
		}
		else
		{
			ParseHotkeys(keyevent);
		}
	}
}

void CFrame::ParseHotkeys(wxKeyEvent &event)
{
	int WiimoteId = -1;
	// Toggle fullscreen
	if (IsHotkey(event, HK_FULLSCREEN))
		DoFullscreen(!RendererIsFullscreen());
	// Send Debugger keys to CodeWindow
	else if (g_pCodeWindow && (event.GetKeyCode() >= WXK_F9 && event.GetKeyCode() <= WXK_F11))
		event.Skip();
	// Pause and Unpause
	else if (IsHotkey(event, HK_PLAY_PAUSE))
		DoPause();
	// Stop
	else if (IsHotkey(event, HK_STOP))
		DoStop();
	// Screenshot hotkey
	else if (IsHotkey(event, HK_SCREENSHOT))
		Core::SaveScreenShot();
	else if (IsHotkey(event, HK_EXIT))
		wxPostEvent(this, wxCommandEvent(wxID_EXIT));
	else if (IsHotkey(event, HK_VOLUME_DOWN))
		AudioCommon::DecreaseVolume(3);
	else if (IsHotkey(event, HK_VOLUME_UP))
		AudioCommon::IncreaseVolume(3);
	else if (IsHotkey(event, HK_VOLUME_TOGGLE_MUTE))
		AudioCommon::ToggleMuteVolume();
	// Wiimote connect and disconnect hotkeys
	else if (IsHotkey(event, HK_WIIMOTE1_CONNECT))
		WiimoteId = 0;
	else if (IsHotkey(event, HK_WIIMOTE2_CONNECT))
		WiimoteId = 1;
	else if (IsHotkey(event, HK_WIIMOTE3_CONNECT))
		WiimoteId = 2;
	else if (IsHotkey(event, HK_WIIMOTE4_CONNECT))
		WiimoteId = 3;
	else if (IsHotkey(event, HK_BALANCEBOARD_CONNECT))
		WiimoteId = 4;
	else if (IsHotkey(event, HK_TOGGLE_IR))
	{
		OSDChoice = 1;
		// Toggle native resolution
		if (++g_Config.iEFBScale > SCALE_4X)
			g_Config.iEFBScale = SCALE_AUTO;
	}
	else if (IsHotkey(event, HK_TOGGLE_AR))
	{
		OSDChoice = 2;
		// Toggle aspect ratio
		g_Config.iAspectRatio = (g_Config.iAspectRatio + 1) & 3;
	}
	else if (IsHotkey(event, HK_TOGGLE_EFBCOPIES))
	{
		OSDChoice = 3;
		// Toggle EFB copies between EFB2RAM and EFB2Texture
		if (!g_Config.bEFBCopyEnable)
		{
			OSD::AddMessage("EFB Copies are disabled, enable them in Graphics settings for toggling", 6000);
		}
		else
		{
			g_Config.bSkipEFBCopyToRam = !g_Config.bSkipEFBCopyToRam;
		}
	}
	else if (IsHotkey(event, HK_TOGGLE_FOG))
	{
		OSDChoice = 4;
		g_Config.bDisableFog = !g_Config.bDisableFog;
	}
	else if (IsHotkey(event, HK_TOGGLE_THROTTLE, true))
	{
		Core::SetIsFramelimiterTempDisabled(true);
	}
	else if (IsHotkey(event, HK_DECREASE_FRAME_LIMIT))
	{
		if (--SConfig::GetInstance().m_Framelimit > 0x19)
			SConfig::GetInstance().m_Framelimit = 0x19;
	}
	else if (IsHotkey(event, HK_INCREASE_FRAME_LIMIT))
	{
		if (++SConfig::GetInstance().m_Framelimit > 0x19)
			SConfig::GetInstance().m_Framelimit = 0;
	}
	else if (IsHotkey(event, HK_SAVE_STATE_SLOT_SELECTED))
	{
		State::Save(g_saveSlot);
	}
	else if (IsHotkey(event, HK_LOAD_STATE_SLOT_SELECTED))
	{
		State::Load(g_saveSlot);
	}
	else if (IsHotkey(event, HK_DECREASE_DEPTH, true))
	{
		if (--g_Config.iStereoDepth < 0)
			g_Config.iStereoDepth = 0;
	}
	else if (IsHotkey(event, HK_INCREASE_DEPTH, true))
	{
		if (++g_Config.iStereoDepth > 100)
			g_Config.iStereoDepth = 100;
	}
	else if (IsHotkey(event, HK_DECREASE_CONVERGENCE, true))
	{
		g_Config.iStereoConvergence -= 5;
		if (g_Config.iStereoConvergence < 0)
			g_Config.iStereoConvergence = 0;
	}
	else if (IsHotkey(event, HK_INCREASE_CONVERGENCE, true))
	{
		g_Config.iStereoConvergence += 5;
		if (g_Config.iStereoConvergence > 500)
			g_Config.iStereoConvergence = 500;
	}

	else
	{
		for (int i = HK_SELECT_STATE_SLOT_1; i < HK_SELECT_STATE_SLOT_10; ++i)
		{
			if (IsHotkey(event, i))
			{
				wxCommandEvent slot_event;
				slot_event.SetId(i + IDM_SELECT_SLOT_1 - HK_SELECT_STATE_SLOT_1);
				CFrame::OnSelectSlot(slot_event);
			}
		}

		unsigned int i = NUM_HOTKEYS;
		for (i = 0; i < NUM_HOTKEYS; i++)
		{
			bool held = false;
			if (i == HK_FRAME_ADVANCE)
				held = true;

			if (IsHotkey(event, i, held))
			{
				int cmd = GetCmdForHotkey(i);
				if (cmd >= 0)
				{
					wxCommandEvent evt(wxEVT_MENU, cmd);
					wxMenuItem* item = GetMenuBar()->FindItem(cmd);
					if (item && item->IsCheckable())
					{
						item->wxMenuItemBase::Toggle();
						evt.SetInt(item->IsChecked());
					}
					GetEventHandler()->AddPendingEvent(evt);
					break;
				}
			}
		}
		// On OS X, we claim all keyboard events while
		// emulation is running to avoid wxWidgets sounding
		// the system beep for unhandled key events when
		// receiving pad/Wiimote keypresses which take an
		// entirely different path through the HID subsystem.
#ifndef __APPLE__
		// On other platforms, we leave the key event alone
		// so it can be passed on to the windowing system.
		if (i == NUM_HOTKEYS)
			event.Skip();
#endif
	}

	// Actually perform the Wiimote connection or disconnection
	if (Core::GetState() != Core::CORE_UNINITIALIZED)
	{
		if (WiimoteId >= 0 && SConfig::GetInstance().m_LocalCoreStartupParameter.bWii)
		{
			wxCommandEvent evt;
			evt.SetId(IDM_CONNECT_WIIMOTE1 + WiimoteId);
			OnConnectWiimote(evt);
		}

		// Maths is probably cheaper than if statements, so always recalculate
		float freeLookSpeed = 0.1f * g_ActiveConfig.fFreeLookSensitivity;

		if (IsHotkey(event, HK_FREELOOK_DECREASE_SPEED))
			g_ActiveConfig.fFreeLookSensitivity /= 2.0f;
		else if (IsHotkey(event, HK_FREELOOK_INCREASE_SPEED))
			g_ActiveConfig.fFreeLookSensitivity *= 2.0f;
		else if (IsHotkey(event, HK_FREELOOK_RESET_SPEED))
			g_ActiveConfig.fFreeLookSensitivity = 1.0f;
		else if (IsHotkey(event, HK_FREELOOK_UP))
			VertexShaderManager::TranslateView(0.0f, 0.0f, -freeLookSpeed / 2);
		else if (IsHotkey(event, HK_FREELOOK_DOWN))
			VertexShaderManager::TranslateView(0.0f, 0.0f, freeLookSpeed / 2);
		else if (IsHotkey(event, HK_FREELOOK_LEFT))
			VertexShaderManager::TranslateView(freeLookSpeed, 0.0f);
		else if (IsHotkey(event, HK_FREELOOK_RIGHT))
			VertexShaderManager::TranslateView(-freeLookSpeed, 0.0f);
		else if (IsHotkey(event, HK_FREELOOK_ZOOM_IN))
			VertexShaderManager::TranslateView(0.0f, freeLookSpeed);
		else if (IsHotkey(event, HK_FREELOOK_ZOOM_OUT))
			VertexShaderManager::TranslateView(0.0f, -freeLookSpeed);
		else if (IsHotkey(event, HK_FREELOOK_RESET))
		{
			VertexShaderManager::ResetView();
			VR_RecenterHMD();
		}
		else if (g_has_hmd)
		{
			if (IsVRSettingsKey(event, VR_LARGER_SCALE))
			{
				// Make everything 10% bigger (and further)
				g_Config.fUnitsPerMetre /= 1.10f;
				VertexShaderManager::ScaleView(1.10f);
				NOTICE_LOG(VR, "%f units per metre (each unit is %f cm)", g_Config.fUnitsPerMetre, 100.0f / g_Config.fUnitsPerMetre);
			}
			else if (IsVRSettingsKey(event, VR_SMALLER_SCALE))
			{
				// Make everything 10% smaller (and closer)
				g_Config.fUnitsPerMetre *= 1.10f;
				VertexShaderManager::ScaleView(1.0f / 1.10f);
				NOTICE_LOG(VR, "%f units per metre (each unit is %f cm)", g_Config.fUnitsPerMetre, 100.0f / g_Config.fUnitsPerMetre);
			}
			if (IsVRSettingsKey(event, VR_GLOBAL_LARGER_SCALE))
			{
				// Make everything 10% bigger (and further)
				g_Config.fScale *= 1.10f;
				SConfig::GetInstance().SaveSingleSetting("VR", "Scale", g_Config.fScale);
				VertexShaderManager::ScaleView(1.10f);
			}
			else if (IsVRSettingsKey(event, VR_GLOBAL_SMALLER_SCALE))
			{
				// Make everything 10% smaller (and closer)
				g_Config.fScale /= 1.10f;
				SConfig::GetInstance().SaveSingleSetting("VR", "Scale", g_Config.fScale);
				VertexShaderManager::ScaleView(1.0f / 1.10f);
			}
			else if (IsVRSettingsKey(event, VR_PERMANENT_CAMERA_FORWARD)) {
				// Move camera forward 10cm
				g_Config.fCameraForward += freeLookSpeed;
				NOTICE_LOG(VR, "Camera is %5.1fm (%5.0fcm) forward", g_Config.fCameraForward, g_Config.fCameraForward * 100);
			}
			else if (IsVRSettingsKey(event, VR_PERMANENT_CAMERA_BACKWARD)) {
				// Move camera back 10cm
				g_Config.fCameraForward -= freeLookSpeed;
				NOTICE_LOG(VR, "Camera is %5.1fm (%5.0fcm) forward", g_Config.fCameraForward, g_Config.fCameraForward * 100);
			}
			else if (IsVRSettingsKey(event, VR_CAMERA_TILT_UP)) {
				// Pitch camera up 5 degrees
				g_Config.fCameraPitch += 5.0f;
				NOTICE_LOG(VR, "Camera is pitched %5.1f degrees up", g_Config.fCameraPitch);
			}
			else if (IsVRSettingsKey(event, VR_CAMERA_TILT_DOWN)) {
				// Pitch camera down 5 degrees
				g_Config.fCameraPitch -= 5.0f;
				NOTICE_LOG(VR, "Camera is pitched %5.1f degrees up", g_Config.fCameraPitch);
			}
			else if (IsVRSettingsKey(event, VR_HUD_FORWARD)) {
				// Move HUD out 10cm
				g_Config.fHudDistance += 0.1f;
				NOTICE_LOG(VR, "HUD is %5.1fm (%5.0fcm) away", g_Config.fHudDistance, g_Config.fHudDistance * 100);
			}
			else if (IsVRSettingsKey(event, VR_HUD_BACKWARD)) {
				// Move HUD in 10cm
				g_Config.fHudDistance -= 0.1f;
				if (g_Config.fHudDistance <= 0)
					g_Config.fHudDistance = 0;
				NOTICE_LOG(VR, "HUD is %5.1fm (%5.0fcm) away", g_Config.fHudDistance, g_Config.fHudDistance * 100);
			}
			else if (IsVRSettingsKey(event, VR_HUD_THICKER)) {
				// Make HUD 10cm thicker
				if (g_Config.fHudThickness < 0.01f)
					g_Config.fHudThickness = 0.01f;
				else if (g_Config.fHudThickness < 0.1f)
					g_Config.fHudThickness += 0.01f;
				else
					g_Config.fHudThickness += 0.1f;
				NOTICE_LOG(VR, "HUD is %5.2fm (%5.0fcm) thick", g_Config.fHudThickness, g_Config.fHudThickness * 100);
			}
			else if (IsVRSettingsKey(event, VR_HUD_THINNER)) {
				// Make HUD 10cm thinner
				if (g_Config.fHudThickness <= 0.01f)
					g_Config.fHudThickness = 0;
				else if (g_Config.fHudThickness <= 0.1f)
					g_Config.fHudThickness -= 0.01f;
				else
					g_Config.fHudThickness -= 0.1f;
				NOTICE_LOG(VR, "HUD is %5.2fm (%5.0fcm) thick", g_Config.fHudThickness, g_Config.fHudThickness * 100);
			}
			else if (IsVRSettingsKey(event, VR_HUD_3D_CLOSER)) {
				// Make HUD 3D elements 5% closer (and smaller)
				if (g_Config.fHud3DCloser >= 0.95f)
					g_Config.fHud3DCloser = 1;
				else
					g_Config.fHud3DCloser += 0.05f;
				NOTICE_LOG(VR, "HUD 3D Items are %5.1f%% closer", g_Config.fHud3DCloser * 100);
			}
			else if (IsVRSettingsKey(event, VR_HUD_3D_FURTHER)) {
				// Make HUD 3D elements 5% further (and smaller)
				if (g_Config.fHud3DCloser <= 0.05f)
					g_Config.fHud3DCloser = 0;
				else
					g_Config.fHud3DCloser -= 0.05f;
				NOTICE_LOG(VR, "HUD 3D Items are %5.1f%% closer", g_Config.fHud3DCloser * 100);
			}
			else if (IsVRSettingsKey(event, VR_2D_SCREEN_LARGER)) {
				// Make everything 20% smaller (and closer)
				g_Config.fScreenHeight *= 1.05f;
				NOTICE_LOG(VR, "Screen is %fm high", g_Config.fScreenHeight);
			}
			else if (IsVRSettingsKey(event, VR_2D_SCREEN_SMALLER)) {
				// Make everything 20% bigger (and further)
				g_Config.fScreenHeight /= 1.05f;
				NOTICE_LOG(VR, "Screen is %fm High", g_Config.fScreenHeight);
			}
			else if (IsVRSettingsKey(event, VR_2D_SCREEN_THICKER)) {
				// Make Screen 10cm thicker
				if (g_Config.fScreenThickness < 0.01f)
					g_Config.fScreenThickness = 0.01f;
				else if (g_Config.fScreenThickness < 0.1f)
					g_Config.fScreenThickness += 0.01f;
				else
					g_Config.fScreenThickness += 0.1f;
				NOTICE_LOG(VR, "Screen is %5.2fm (%5.0fcm) thick", g_Config.fScreenThickness, g_Config.fScreenThickness * 100);
			}
			else if (IsVRSettingsKey(event, VR_2D_SCREEN_THINNER)) {
				// Make Screen 10cm thinner
				if (g_Config.fScreenThickness <= 0.01f)
					g_Config.fScreenThickness = 0;
				else if (g_Config.fScreenThickness <= 0.1f)
					g_Config.fScreenThickness -= 0.01f;
				else
					g_Config.fScreenThickness -= 0.1f;
				NOTICE_LOG(VR, "Screen is %5.2fm (%5.0fcm) thick", g_Config.fScreenThickness, g_Config.fScreenThickness * 100);
			}
			else if (IsVRSettingsKey(event, VR_2D_CAMERA_FORWARD)) {
				// Move Screen in 10cm
				g_Config.fScreenDistance -= 0.1f;
				if (g_Config.fScreenDistance <= 0)
					g_Config.fScreenDistance = 0;
				NOTICE_LOG(VR, "Screen is %5.1fm (%5.0fcm) away", g_Config.fScreenDistance, g_Config.fScreenDistance * 100);
			}
			else if (IsVRSettingsKey(event, VR_2D_CAMERA_BACKWARD)) {
				// Move Screen out 10cm
				g_Config.fScreenDistance += 0.1f;
				NOTICE_LOG(VR, "Screen is %5.1fm (%5.0fcm) away", g_Config.fScreenDistance, g_Config.fScreenDistance * 100);
			}
			else if (IsVRSettingsKey(event, VR_2D_CAMERA_UP)) {
				// Move Screen Down (Camera Up) 10cm
				g_Config.fScreenUp -= 0.1f;
				NOTICE_LOG(VR, "Screen is %5.1fm up", g_Config.fScreenUp);
			}
			else if (IsVRSettingsKey(event, VR_2D_CAMERA_DOWN)) {
				// Move Screen Up (Camera Down) 10cm
				g_Config.fScreenUp += 0.1f;
				NOTICE_LOG(VR, "Screen is %5.1fm up", g_Config.fScreenUp);
			}
			else if (IsVRSettingsKey(event, VR_2D_CAMERA_TILT_UP)) {
				// Pitch camera up 5 degrees
				g_Config.fScreenPitch += 5.0f;
				NOTICE_LOG(VR, "2D Camera is pitched %5.1f degrees up", g_Config.fScreenPitch);
			}
			else if (IsVRSettingsKey(event, VR_2D_CAMERA_TILT_DOWN)) {
				// Pitch camera down 5 degrees
				g_Config.fScreenPitch -= 5.0f;
				NOTICE_LOG(VR, "2D Camera is pitched %5.1f degrees up", g_Config.fScreenPitch);;
			}
		}
	}
	if (g_has_hmd && event.GetModifiers() == wxMOD_SHIFT)
	{
		switch (event.GetKeyCode())
		{
			// Previous layer
		case 'B':
			g_Config.iSelectedLayer--;
			if (g_Config.iSelectedLayer < -1)
				g_Config.iSelectedLayer = -2;
			NOTICE_LOG(VR, "Selected layer %d", g_Config.iSelectedLayer);
			debug_nextScene = true;
			break;
			// Next layer
		case 'N':
			g_Config.iSelectedLayer++;
			NOTICE_LOG(VR, "Selected layer %d", g_Config.iSelectedLayer);
			debug_nextScene = true;
			break;
		case '\'':
			NOTICE_LOG(VR, "--- pressed ' ---");
			debug_nextScene = true;
			break;
		}
	}
}

