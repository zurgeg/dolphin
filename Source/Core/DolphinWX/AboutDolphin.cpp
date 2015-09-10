// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <wx/bitmap.h>
#include <wx/dialog.h>
#include <wx/hyperlink.h>
#include <wx/image.h>
#include <wx/mstream.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/generic/statbmpg.h>

#ifdef __APPLE__
#import <AppKit/AppKit.h>
#endif

#include "Common/Common.h"
#include "DolphinWX/AboutDolphin.h"
#include "DolphinWX/resources/dolphin_logo.cpp"
#include "VideoCommon/VR.h"

AboutDolphin::AboutDolphin(wxWindow *parent, wxWindowID id,
		const wxString &title, const wxPoint &position,
		const wxSize& size, long style)
	: wxDialog(parent, id, title, position, size, style)
{
	const unsigned char* dolphin_logo_bin = dolphin_logo_png;
	size_t dolphin_logo_size = sizeof dolphin_logo_png;
#ifdef __APPLE__
	double scaleFactor = 1.0;
	if (GetContentScaleFactor() >= 2)
	{
		dolphin_logo_bin = dolphin_logo_2x_png;
		dolphin_logo_size = sizeof dolphin_logo_2x_png;
		scaleFactor = 2.0;
	}
#endif
	wxMemoryInputStream istream(dolphin_logo_bin, dolphin_logo_size);
	wxImage iDolphinLogo(istream, wxBITMAP_TYPE_PNG);
#ifdef __APPLE__
	wxGenericStaticBitmap* const sbDolphinLogo = new wxGenericStaticBitmap(this, wxID_ANY,
			wxBitmap(iDolphinLogo, -1, scaleFactor));
#else
	wxGenericStaticBitmap* const sbDolphinLogo = new wxGenericStaticBitmap(this, wxID_ANY,
			wxBitmap(iDolphinLogo));
#endif

	const wxString DolphinText = _("Dolphin VR");
	const wxString RevisionText = scm_desc_str + wxString::Format("%s", SCM_OCULUS_STR);
	const wxString CopyrightText = _("(c) 2003-2015+ Dolphin Team. \"GameCube\" and \"Wii\" are trademarks of Nintendo. Dolphin is not affiliated with Nintendo in any way.");
	const wxString BranchText = wxString::Format(_("Branch: %s "), scm_branch_str);
	const wxString BranchRevText = wxString::Format(_("Revision: %s"), scm_rev_git_str);
	const wxString CompiledText = wxString::Format(_("Compiled: %s @ %s"), __DATE__, __TIME__);
	const wxString CheckUpdateText = _("Check for updates: ");
	const wxString Text = _("\n"
		"Dolphin is a free and open-source GameCube and Wii emulator.\n"
				"\n"
		"This software should not be used to play games you do not legally own.\n");
	const wxString LicenseText = _("License");
	const wxString AuthorsText = _("Authors");
	const wxString SupportText = _("Support");

	wxStaticText* const Dolphin = new wxStaticText(this, wxID_ANY, DolphinText);
	wxStaticText* const Revision = new wxStaticText(this, wxID_ANY, RevisionText);

	wxStaticText* const Copyright = new wxStaticText(this, wxID_ANY, CopyrightText);
	wxStaticText* const Branch = new wxStaticText(this, wxID_ANY, BranchText + "\n" + BranchRevText + "\n" + CompiledText+"\n");
	wxStaticText* const Message = new wxStaticText(this, wxID_ANY, Text);
	wxStaticText* const UpdateText = new wxStaticText(this, wxID_ANY, CheckUpdateText);
	wxStaticText* const FirstSpacer = new wxStaticText(this, wxID_ANY, "  |  ");
	wxStaticText* const SecondSpacer = new wxStaticText(this, wxID_ANY, "  |  ");
	wxHyperlinkCtrl* const Download = new wxHyperlinkCtrl(this, wxID_ANY, "dolphinvr.wordpress.com/downloads", "https://dolphinvr.wordpress.com/downloads/");
	wxHyperlinkCtrl* const License = new wxHyperlinkCtrl(this, wxID_ANY, LicenseText, "https://github.com/dolphin-emu/dolphin/blob/master/license.txt");
	wxHyperlinkCtrl* const Authors = new wxHyperlinkCtrl(this, wxID_ANY, AuthorsText, "https://github.com/CarlKenner/dolphin/graphs/contributors");
	wxHyperlinkCtrl* const Support = new wxHyperlinkCtrl(this, wxID_ANY, SupportText, "https://forums.oculus.com/viewtopic.php?f=42&t=11241&start=1180");

	wxFont DolphinFont = Dolphin->GetFont();
	wxFont RevisionFont = Revision->GetFont();
	wxFont CopyrightFont = Copyright->GetFont();
	wxFont BranchFont = Branch->GetFont();

	DolphinFont.SetPointSize(36);
	Dolphin->SetFont(DolphinFont);

	RevisionFont.SetWeight(wxFONTWEIGHT_BOLD);
	Revision->SetFont(RevisionFont);

	BranchFont.SetPointSize(7);
	Branch->SetFont(BranchFont);

	CopyrightFont.SetPointSize(7);
	Copyright->SetFont(CopyrightFont);
	Copyright->SetFocus();

	wxBoxSizer* const sCheckUpdates = new wxBoxSizer(wxHORIZONTAL);
	sCheckUpdates->Add(UpdateText);
	sCheckUpdates->Add(Download);

	wxBoxSizer* const sLinks = new wxBoxSizer(wxHORIZONTAL);
	sLinks->Add(License);
	sLinks->Add(FirstSpacer);
	sLinks->Add(Authors);
	sLinks->Add(SecondSpacer);
	sLinks->Add(Support);

	wxBoxSizer* const sInfo = new wxBoxSizer(wxVERTICAL);
	sInfo->Add(Dolphin);
	sInfo->AddSpacer(5);
	sInfo->Add(Revision);
	sInfo->AddSpacer(10);
	sInfo->Add(Branch);
	sInfo->Add(sCheckUpdates);
	sInfo->Add(Message);
	sInfo->Add(sLinks);

	wxBoxSizer* const sLogo = new wxBoxSizer(wxVERTICAL);
	sLogo->AddSpacer(75);
	sLogo->Add(sbDolphinLogo);
	sLogo->AddSpacer(40);

	wxBoxSizer* const sMainHor = new wxBoxSizer(wxHORIZONTAL);
	sMainHor->AddSpacer(30);
	sMainHor->Add(sLogo);
	sMainHor->AddSpacer(30);
	sMainHor->Add(sInfo);
	sMainHor->AddSpacer(30);

	wxBoxSizer* const sFooter = new wxBoxSizer(wxVERTICAL);
	sFooter->AddSpacer(15);
	sFooter->Add(Copyright, 0, wxALIGN_BOTTOM | wxALIGN_CENTER);
	sFooter->AddSpacer(5);

	wxBoxSizer* const sMain = new wxBoxSizer(wxVERTICAL);
	sMain->Add(sMainHor, 1, wxEXPAND);
	sMain->Add(sFooter, 0, wxEXPAND);

	SetSizerAndFit(sMain);
	Center();
	SetFocus();
}
