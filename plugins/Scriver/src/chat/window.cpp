/*
Chat module plugin for Miranda IM

Copyright (C) 2003 Jörgen Persson
Copyright 2003-2009 Miranda ICQ/IM project,

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "../stdafx.h"

struct MESSAGESUBDATA
{
	time_t lastEnterTime;
	wchar_t *szSearchQuery;
	wchar_t *szSearchResult;
	SESSION_INFO *lastSession;
};

static LRESULT CALLBACK SplitterSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	RECT rc;

	switch (msg) {
	case WM_NCHITTEST:
		return HTCLIENT;

	case WM_SETCURSOR:
		GetClientRect(hwnd, &rc);
		SetCursor(rc.right > rc.bottom ? hCurSplitNS : hCurSplitWE);
		return TRUE;

	case WM_LBUTTONDOWN:
		SetCapture(hwnd);
		return 0;

	case WM_MOUSEMOVE:
		if (GetCapture() == hwnd) {
			GetClientRect(hwnd, &rc);
			SendMessage(GetParent(hwnd), GC_SPLITTERMOVED, rc.right > rc.bottom ? (short)HIWORD(GetMessagePos()) + rc.bottom / 2 : (short)LOWORD(GetMessagePos()) + rc.right / 2, (LPARAM)hwnd);
		}
		return 0;

	case WM_LBUTTONUP:
		ReleaseCapture();
		PostMessage(GetParent(hwnd), WM_SIZE, 0, 0);
		return 0;
	}
	return mir_callNextSubclass(hwnd, SplitterSubclassProc, msg, wParam, lParam);
}

static void InitButtons(HWND hwndDlg, SESSION_INFO *si)
{
	SendDlgItemMessage(hwndDlg, IDC_CHAT_SHOWNICKLIST, BM_SETIMAGE, IMAGE_ICON, (LPARAM)GetCachedIcon(si->bNicklistEnabled ? "chat_nicklist" : "chat_nicklist2"));
	SendDlgItemMessage(hwndDlg, IDC_CHAT_FILTER, BM_SETIMAGE, IMAGE_ICON, (LPARAM)GetCachedIcon(si->bFilterEnabled ? "chat_filter" : "chat_filter2"));

	MODULEINFO *pInfo = pci->MM_FindModule(si->pszModule);
	if (pInfo) {
		EnableWindow(GetDlgItem(hwndDlg, IDC_CHAT_BOLD), pInfo->bBold);
		EnableWindow(GetDlgItem(hwndDlg, IDC_CHAT_ITALICS), pInfo->bItalics);
		EnableWindow(GetDlgItem(hwndDlg, IDC_CHAT_UNDERLINE), pInfo->bUnderline);
		EnableWindow(GetDlgItem(hwndDlg, IDC_CHAT_COLOR), pInfo->bColor);
		EnableWindow(GetDlgItem(hwndDlg, IDC_CHAT_BKGCOLOR), pInfo->bBkgColor);
		if (si->iType == GCW_CHATROOM)
			EnableWindow(GetDlgItem(hwndDlg, IDC_CHAT_CHANMGR), pInfo->bChanMgr);
	}
}

static void MessageDialogResize(HWND hwndDlg, SESSION_INFO *si, int w, int h)
{
	bool bNick = si->iType != GCW_SERVER && si->bNicklistEnabled;
	bool bToolbar = SendMessage(GetParent(hwndDlg), CM_GETTOOLBARSTATUS, 0, 0) != 0;
	int  hSplitterMinTop = TOOLBAR_HEIGHT + si->minLogBoxHeight, hSplitterMinBottom = si->minEditBoxHeight;
	int  toolbarHeight = bToolbar ? TOOLBAR_HEIGHT : 0;

	si->iSplitterY = si->desiredInputAreaHeight + SPLITTER_HEIGHT + 3;

	if (h - si->iSplitterY < hSplitterMinTop)
		si->iSplitterY = h - hSplitterMinTop;
	if (si->iSplitterY < hSplitterMinBottom)
		si->iSplitterY = hSplitterMinBottom;

	ShowWindow(GetDlgItem(hwndDlg, IDC_SPLITTERX), bNick ? SW_SHOW : SW_HIDE);
	if (si->iType != GCW_SERVER)
		ShowWindow(GetDlgItem(hwndDlg, IDC_CHAT_LIST), si->bNicklistEnabled ? SW_SHOW : SW_HIDE);
	else
		ShowWindow(GetDlgItem(hwndDlg, IDC_CHAT_LIST), SW_HIDE);

	if (si->iType == GCW_SERVER) {
		EnableWindow(GetDlgItem(hwndDlg, IDC_CHAT_SHOWNICKLIST), FALSE);
		EnableWindow(GetDlgItem(hwndDlg, IDC_CHAT_FILTER), FALSE);
		EnableWindow(GetDlgItem(hwndDlg, IDC_CHAT_CHANMGR), FALSE);
	}
	else {
		EnableWindow(GetDlgItem(hwndDlg, IDC_CHAT_SHOWNICKLIST), TRUE);
		EnableWindow(GetDlgItem(hwndDlg, IDC_CHAT_FILTER), TRUE);
		if (si->iType == GCW_CHATROOM)
			EnableWindow(GetDlgItem(hwndDlg, IDC_CHAT_CHANMGR), pci->MM_FindModule(si->pszModule)->bChanMgr);
	}

	HDWP hdwp = BeginDeferWindowPos(5);
	int toolbarTopY = bToolbar ? h - si->iSplitterY - toolbarHeight : h - si->iSplitterY;
	int logBottom = (si->hwndLog != NULL) ? toolbarTopY / 2 : toolbarTopY;

	hdwp = DeferWindowPos(hdwp, GetDlgItem(hwndDlg, IDC_LOG), 0, 1, 0, bNick ? w - si->iSplitterX - 1 : w - 2, logBottom, SWP_NOZORDER);
	hdwp = DeferWindowPos(hdwp, GetDlgItem(hwndDlg, IDC_CHAT_LIST), 0, w - si->iSplitterX + 2, 0, si->iSplitterX - 3, toolbarTopY, SWP_NOZORDER);
	hdwp = DeferWindowPos(hdwp, GetDlgItem(hwndDlg, IDC_SPLITTERX), 0, w - si->iSplitterX, 1, 2, toolbarTopY - 1, SWP_NOZORDER);
	hdwp = DeferWindowPos(hdwp, GetDlgItem(hwndDlg, IDC_SPLITTERY), 0, 0, h - si->iSplitterY, w, SPLITTER_HEIGHT, SWP_NOZORDER);
	hdwp = DeferWindowPos(hdwp, GetDlgItem(hwndDlg, IDC_MESSAGE), 0, 1, h - si->iSplitterY + SPLITTER_HEIGHT, w - 2, si->iSplitterY - SPLITTER_HEIGHT - 1, SWP_NOZORDER);
	EndDeferWindowPos(hdwp);

	SetButtonsPos(hwndDlg, bToolbar);

	if (si->hwndLog != NULL) {
		IEVIEWWINDOW ieWindow;
		ieWindow.cbSize = sizeof(IEVIEWWINDOW);
		ieWindow.iType = IEW_SETPOS;
		ieWindow.parent = hwndDlg;
		ieWindow.hwnd = si->hwndLog;
		ieWindow.x = 0;
		ieWindow.y = logBottom + 1;
		ieWindow.cx = bNick ? w - si->iSplitterX : w;
		ieWindow.cy = logBottom;
		CallService(MS_IEVIEW_WINDOW, 0, (LPARAM)&ieWindow);
	}
	else RedrawWindow(GetDlgItem(hwndDlg, IDC_LOG), NULL, NULL, RDW_INVALIDATE);

	RedrawWindow(GetDlgItem(hwndDlg, IDC_CHAT_LIST), NULL, NULL, RDW_INVALIDATE);
	RedrawWindow(GetDlgItem(hwndDlg, IDC_MESSAGE), NULL, NULL, RDW_INVALIDATE);
}

static void TabAutoComplete(HWND hwnd, MESSAGESUBDATA *dat, SESSION_INFO *si)
{
	LRESULT lResult = (LRESULT)SendMessage(hwnd, EM_GETSEL, 0, 0);
	int start = LOWORD(lResult), end = HIWORD(lResult);
	SendMessage(hwnd, EM_SETSEL, end, end);

	GETTEXTEX gt = { 0 };
	gt.codepage = 1200;
	int iLen = GetRichTextLength(hwnd, gt.codepage, TRUE);
	if (iLen <= 0)
		return;

	bool isTopic = false, isRoom = false;
	wchar_t *pszName = NULL;
	wchar_t* pszText = (wchar_t*)mir_alloc(iLen + 100 * sizeof(wchar_t));
	gt.cb = iLen + 99 * sizeof(wchar_t);
	gt.flags = GT_DEFAULT;

	SendMessage(hwnd, EM_GETTEXTEX, (WPARAM)&gt, (LPARAM)pszText);
	if (start > 1 && pszText[start - 1] == ' ' && pszText[start - 2] == ':')
		start -= 2;

	if (dat->szSearchResult != NULL) {
		int cbResult = (int)mir_wstrlen(dat->szSearchResult);
		if (start >= cbResult && !wcsnicmp(dat->szSearchResult, pszText + start - cbResult, cbResult)) {
			start -= cbResult;
			goto LBL_SkipEnd;
		}
	}

	while (start > 0 && pszText[start - 1] != ' ' && pszText[start - 1] != 13 && pszText[start - 1] != VK_TAB)
		start--;

LBL_SkipEnd:
	while (end < iLen && pszText[end] != ' ' && pszText[end] != 13 && pszText[end - 1] != VK_TAB)
		end++;

	if (pszText[start] == '#')
		isRoom = true;
	else {
		int topicStart = start;
		while (topicStart >0 && (pszText[topicStart - 1] == ' ' || pszText[topicStart - 1] == 13 || pszText[topicStart - 1] == VK_TAB))
			topicStart--;
		if (topicStart > 5 && wcsstr(&pszText[topicStart - 6], L"/topic") == &pszText[topicStart - 6])
			isTopic = true;
	}

	if (dat->szSearchQuery == NULL) {
		dat->szSearchQuery = (wchar_t*)mir_alloc(sizeof(wchar_t)*(end - start + 1));
		mir_wstrncpy(dat->szSearchQuery, pszText + start, end - start + 1);
		dat->szSearchResult = mir_wstrdup(dat->szSearchQuery);
		dat->lastSession = NULL;
	}

	if (isTopic)
		pszName = si->ptszTopic;
	else if (isRoom) {
		dat->lastSession = SM_FindSessionAutoComplete(si->pszModule, si, dat->lastSession, dat->szSearchQuery, dat->szSearchResult);
		if (dat->lastSession != NULL)
			pszName = dat->lastSession->ptszName;
	}
	else pszName = pci->UM_FindUserAutoComplete(si->pUsers, dat->szSearchQuery, dat->szSearchResult);

	mir_free(pszText);
	replaceStrW(dat->szSearchResult, NULL);

	if (pszName == NULL) {
		if (end != start) {
			SendMessage(hwnd, EM_SETSEL, start, end);
			SendMessage(hwnd, EM_REPLACESEL, FALSE, (LPARAM)dat->szSearchQuery);
		}
		replaceStrW(dat->szSearchQuery, NULL);
	}
	else {
		dat->szSearchResult = mir_wstrdup(pszName);
		if (end != start) {
			ptrW szReplace;
			if (!isRoom && !isTopic && g_Settings.bAddColonToAutoComplete && start == 0) {
				szReplace = (wchar_t*)mir_alloc((mir_wstrlen(pszName) + 4) * sizeof(wchar_t));
				mir_wstrcpy(szReplace, pszName);
				mir_wstrcat(szReplace, L": ");
				pszName = szReplace;
			}
			SendMessage(hwnd, EM_SETSEL, start, end);
			SendMessage(hwnd, EM_REPLACESEL, FALSE, (LPARAM)pszName);
		}
	}
}

static LRESULT CALLBACK MessageSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	BOOL isShift = GetKeyState(VK_SHIFT) & 0x8000;
	BOOL isCtrl = GetKeyState(VK_CONTROL) & 0x8000;
	BOOL isAlt = GetKeyState(VK_MENU) & 0x8000;

	SESSION_INFO *si = (SESSION_INFO*)GetWindowLongPtr(GetParent(hwnd), GWLP_USERDATA);
	MESSAGESUBDATA *dat = (MESSAGESUBDATA *)GetWindowLongPtr(hwnd, GWLP_USERDATA);

	int result = InputAreaShortcuts(hwnd, msg, wParam, lParam, si);
	if (result != -1)
		return result;

	switch (msg) {
	case EM_SUBCLASSED:
		dat = (MESSAGESUBDATA*)mir_calloc(sizeof(MESSAGESUBDATA));
		SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)dat);
		return 0;

	case WM_MOUSEWHEEL:
		if ((GetWindowLongPtr(hwnd, GWL_STYLE) & WS_VSCROLL) == 0)
			SendDlgItemMessage(GetParent(hwnd), IDC_LOG, WM_MOUSEWHEEL, wParam, lParam);

		dat->lastEnterTime = 0;
		return TRUE;

	case EM_REPLACESEL:
		PostMessage(hwnd, EM_ACTIVATE, 0, 0);
		break;

	case EM_ACTIVATE:
		SetActiveWindow(GetParent(hwnd));
		break;

	case WM_KEYDOWN:
		if (wParam == VK_RETURN) {
			mir_free(dat->szSearchQuery);
			dat->szSearchQuery = NULL;
			mir_free(dat->szSearchResult);
			dat->szSearchResult = NULL;
			if ((isCtrl != 0) ^ (0 != db_get_b(NULL, SRMMMOD, SRMSGSET_SENDONENTER, SRMSGDEFSET_SENDONENTER))) {
				PostMessage(GetParent(hwnd), WM_COMMAND, IDOK, 0);
				return 0;
			}
			if (db_get_b(NULL, SRMMMOD, SRMSGSET_SENDONDBLENTER, SRMSGDEFSET_SENDONDBLENTER)) {
				if (dat->lastEnterTime + 2 < time(NULL))
					dat->lastEnterTime = time(NULL);
				else {
					SendMessage(hwnd, WM_KEYDOWN, VK_BACK, 0);
					SendMessage(hwnd, WM_KEYUP, VK_BACK, 0);
					PostMessage(GetParent(hwnd), WM_COMMAND, IDOK, 0);
					return 0;
				}
			}
		}
		else dat->lastEnterTime = 0;

		if (wParam == VK_TAB && isShift && !isCtrl) { // SHIFT-TAB (go to nick list)
			SetFocus(GetDlgItem(GetParent(hwnd), IDC_CHAT_LIST));
			return TRUE;
		}

		if (wParam == VK_TAB && !isCtrl && !isShift) {    //tab-autocomplete
			SendMessage(hwnd, WM_SETREDRAW, FALSE, 0);
			TabAutoComplete(hwnd, dat, si);
			SendMessage(hwnd, WM_SETREDRAW, TRUE, 0);
			RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE);
			return 0;
		}
		if (wParam != VK_RIGHT && wParam != VK_LEFT) {
			mir_free(dat->szSearchQuery);
			dat->szSearchQuery = NULL;
			mir_free(dat->szSearchResult);
			dat->szSearchResult = NULL;
		}
		if (wParam == 0x49 && isCtrl && !isAlt) { // ctrl-i (italics)
			CheckDlgButton(GetParent(hwnd), IDC_CHAT_ITALICS, IsDlgButtonChecked(GetParent(hwnd), IDC_CHAT_ITALICS) == BST_UNCHECKED ? BST_CHECKED : BST_UNCHECKED);
			SendMessage(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(IDC_CHAT_ITALICS, 0), 0);
			return TRUE;
		}

		if (wParam == 0x42 && isCtrl && !isAlt) { // ctrl-b (bold)
			CheckDlgButton(GetParent(hwnd), IDC_CHAT_BOLD, IsDlgButtonChecked(GetParent(hwnd), IDC_CHAT_BOLD) == BST_UNCHECKED ? BST_CHECKED : BST_UNCHECKED);
			SendMessage(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(IDC_CHAT_BOLD, 0), 0);
			return TRUE;
		}

		if (wParam == 0x55 && isCtrl && !isAlt) { // ctrl-u (paste clean text)
			CheckDlgButton(GetParent(hwnd), IDC_CHAT_UNDERLINE, IsDlgButtonChecked(GetParent(hwnd), IDC_CHAT_UNDERLINE) == BST_UNCHECKED ? BST_CHECKED : BST_UNCHECKED);
			SendMessage(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(IDC_CHAT_UNDERLINE, 0), 0);
			return TRUE;
		}

		if (wParam == 0x4b && isCtrl && !isAlt) { // ctrl-k (paste clean text)
			CheckDlgButton(GetParent(hwnd), IDC_CHAT_COLOR, IsDlgButtonChecked(GetParent(hwnd), IDC_CHAT_COLOR) == BST_UNCHECKED ? BST_CHECKED : BST_UNCHECKED);
			SendMessage(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(IDC_CHAT_COLOR, 0), 0);
			return TRUE;
		}

		if (wParam == VK_SPACE && isCtrl && !isAlt) { // ctrl-space (paste clean text)
			CheckDlgButton(GetParent(hwnd), IDC_CHAT_BKGCOLOR, BST_UNCHECKED);
			CheckDlgButton(GetParent(hwnd), IDC_CHAT_COLOR, BST_UNCHECKED);
			CheckDlgButton(GetParent(hwnd), IDC_CHAT_BOLD, BST_UNCHECKED);
			CheckDlgButton(GetParent(hwnd), IDC_CHAT_UNDERLINE, BST_UNCHECKED);
			CheckDlgButton(GetParent(hwnd), IDC_CHAT_ITALICS, BST_UNCHECKED);
			SendMessage(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(IDC_CHAT_BKGCOLOR, 0), 0);
			SendMessage(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(IDC_CHAT_COLOR, 0), 0);
			SendMessage(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(IDC_CHAT_BOLD, 0), 0);
			SendMessage(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(IDC_CHAT_UNDERLINE, 0), 0);
			SendMessage(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(IDC_CHAT_ITALICS, 0), 0);
			return TRUE;
		}

		if (wParam == 0x4c && isCtrl && !isAlt) { // ctrl-l (paste clean text)
			CheckDlgButton(GetParent(hwnd), IDC_CHAT_BKGCOLOR, IsDlgButtonChecked(GetParent(hwnd), IDC_CHAT_BKGCOLOR) == BST_UNCHECKED ? BST_CHECKED : BST_UNCHECKED);
			SendMessage(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(IDC_CHAT_BKGCOLOR, 0), 0);
			return TRUE;
		}

		if (wParam == 0x46 && isCtrl && !isAlt) { // ctrl-f (paste clean text)
			if (IsWindowEnabled(GetDlgItem(GetParent(hwnd), IDC_CHAT_FILTER)))
				SendMessage(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(IDC_CHAT_FILTER, 0), 0);
			return TRUE;
		}

		if (wParam == 0x4e && isCtrl && !isAlt) { // ctrl-n (nicklist)
			if (IsWindowEnabled(GetDlgItem(GetParent(hwnd), IDC_CHAT_SHOWNICKLIST)))
				SendMessage(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(IDC_CHAT_SHOWNICKLIST, 0), 0);
			return TRUE;
		}

		if (wParam == 0x48 && isCtrl && !isAlt) { // ctrl-h (history)
			SendMessage(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(IDC_HISTORY, 0), 0);
			return TRUE;
		}

		if (wParam == 0x4f && isCtrl && !isAlt) { // ctrl-o (options)
			if (IsWindowEnabled(GetDlgItem(GetParent(hwnd), IDC_CHAT_CHANMGR)))
				SendMessage(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(IDC_CHAT_CHANMGR, 0), 0);
			return TRUE;
		}

		if (((wParam == VK_INSERT && isShift) || (wParam == 'V' && isCtrl)) && !isAlt) { // ctrl-v (paste clean text)
			SendMessage(hwnd, EM_PASTESPECIAL, CF_UNICODETEXT, 0);
			return TRUE;
		}

		if (wParam == VK_NEXT || wParam == VK_PRIOR) {
			HWND htemp = GetParent(hwnd);
			SendDlgItemMessage(htemp, IDC_LOG, msg, wParam, lParam);
			return TRUE;
		}
		break;

	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_KILLFOCUS:
		dat->lastEnterTime = 0;
		break;

	case WM_CONTEXTMENU:
		InputAreaContextMenu(hwnd, wParam, lParam, si->hContact);
		return TRUE;

	case WM_KEYUP:
	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
	case WM_MBUTTONUP:
		{
			UINT u = 0;
			UINT u2 = 0;
			COLORREF cr;

			LoadMsgDlgFont(MSGFONTID_MESSAGEAREA, NULL, &cr);

			CHARFORMAT2 cf;
			cf.cbSize = sizeof(CHARFORMAT2);
			cf.dwMask = CFM_BOLD | CFM_ITALIC | CFM_UNDERLINE | CFM_BACKCOLOR | CFM_COLOR;
			SendMessage(hwnd, EM_GETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);

			if (pci->MM_FindModule(si->pszModule) && pci->MM_FindModule(si->pszModule)->bColor) {
				int index = pci->GetColorIndex(si->pszModule, cf.crTextColor);
				u = IsDlgButtonChecked(GetParent(hwnd), IDC_CHAT_COLOR);

				if (index >= 0) {
					si->bFGSet = TRUE;
					si->iFG = index;
				}

				if (u == BST_UNCHECKED && cf.crTextColor != cr)
					CheckDlgButton(GetParent(hwnd), IDC_CHAT_COLOR, BST_CHECKED);
				else if (u == BST_CHECKED && cf.crTextColor == cr)
					CheckDlgButton(GetParent(hwnd), IDC_CHAT_COLOR, BST_UNCHECKED);
			}

			if (pci->MM_FindModule(si->pszModule) && pci->MM_FindModule(si->pszModule)->bBkgColor) {
				int index = pci->GetColorIndex(si->pszModule, cf.crBackColor);
				COLORREF crB = db_get_dw(NULL, SRMMMOD, SRMSGSET_INPUTBKGCOLOUR, SRMSGDEFSET_INPUTBKGCOLOUR);
				u = IsDlgButtonChecked(GetParent(hwnd), IDC_CHAT_BKGCOLOR);

				if (index >= 0) {
					si->bBGSet = TRUE;
					si->iBG = index;
				}
				if (u == BST_UNCHECKED && cf.crBackColor != crB)
					CheckDlgButton(GetParent(hwnd), IDC_CHAT_BKGCOLOR, BST_CHECKED);
				else if (u == BST_CHECKED && cf.crBackColor == crB)
					CheckDlgButton(GetParent(hwnd), IDC_CHAT_BKGCOLOR, BST_UNCHECKED);
			}

			if (pci->MM_FindModule(si->pszModule) && pci->MM_FindModule(si->pszModule)->bBold) {
				u = IsDlgButtonChecked(GetParent(hwnd), IDC_CHAT_BOLD);
				u2 = cf.dwEffects;
				u2 &= CFE_BOLD;
				if (u == BST_UNCHECKED && u2)
					CheckDlgButton(GetParent(hwnd), IDC_CHAT_BOLD, BST_CHECKED);
				else if (u == BST_CHECKED && u2 == 0)
					CheckDlgButton(GetParent(hwnd), IDC_CHAT_BOLD, BST_UNCHECKED);
			}

			if (pci->MM_FindModule(si->pszModule) && pci->MM_FindModule(si->pszModule)->bItalics) {
				u = IsDlgButtonChecked(GetParent(hwnd), IDC_CHAT_ITALICS);
				u2 = cf.dwEffects;
				u2 &= CFE_ITALIC;
				if (u == BST_UNCHECKED && u2)
					CheckDlgButton(GetParent(hwnd), IDC_CHAT_ITALICS, BST_CHECKED);
				else if (u == BST_CHECKED && u2 == 0)
					CheckDlgButton(GetParent(hwnd), IDC_CHAT_ITALICS, BST_UNCHECKED);
			}

			if (pci->MM_FindModule(si->pszModule) && pci->MM_FindModule(si->pszModule)->bUnderline) {
				u = IsDlgButtonChecked(GetParent(hwnd), IDC_CHAT_UNDERLINE);
				u2 = cf.dwEffects;
				u2 &= CFE_UNDERLINE;
				if (u == BST_UNCHECKED && u2)
					CheckDlgButton(GetParent(hwnd), IDC_CHAT_UNDERLINE, BST_CHECKED);
				else if (u == BST_CHECKED && u2 == 0)
					CheckDlgButton(GetParent(hwnd), IDC_CHAT_UNDERLINE, BST_UNCHECKED);
			}
		}
		break;

	case WM_DESTROY:
		mir_free(dat->szSearchQuery);
		mir_free(dat->szSearchResult);
		mir_free(dat);
		return 0;
	}

	return mir_callNextSubclass(hwnd, MessageSubclassProc, msg, wParam, lParam);
}

static INT_PTR CALLBACK FilterWndProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static SESSION_INFO *si = NULL;
	switch (uMsg) {
	case WM_INITDIALOG:
		si = (SESSION_INFO *)lParam;
		CheckDlgButton(hwndDlg, IDC_CHAT_1, si->iLogFilterFlags & GC_EVENT_ACTION ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hwndDlg, IDC_CHAT_2, si->iLogFilterFlags & GC_EVENT_MESSAGE ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hwndDlg, IDC_CHAT_3, si->iLogFilterFlags & GC_EVENT_NICK ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hwndDlg, IDC_CHAT_4, si->iLogFilterFlags & GC_EVENT_JOIN ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hwndDlg, IDC_CHAT_5, si->iLogFilterFlags & GC_EVENT_PART ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hwndDlg, IDC_CHAT_6, si->iLogFilterFlags & GC_EVENT_TOPIC ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hwndDlg, IDC_CHAT_7, si->iLogFilterFlags & GC_EVENT_ADDSTATUS ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hwndDlg, IDC_CHAT_8, si->iLogFilterFlags & GC_EVENT_INFORMATION ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hwndDlg, IDC_CHAT_9, si->iLogFilterFlags & GC_EVENT_QUIT ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hwndDlg, IDC_CHAT_10, si->iLogFilterFlags & GC_EVENT_KICK ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hwndDlg, IDC_CHAT_11, si->iLogFilterFlags & GC_EVENT_NOTICE ? BST_CHECKED : BST_UNCHECKED);
		break;

	case WM_CTLCOLOREDIT:
	case WM_CTLCOLORSTATIC:
		SetTextColor((HDC)wParam, RGB(60, 60, 150));
		SetBkColor((HDC)wParam, GetSysColor(COLOR_WINDOW));
		return (INT_PTR)GetSysColorBrush(COLOR_WINDOW);

	case WM_ACTIVATE:
		if (LOWORD(wParam) == WA_INACTIVE) {
			int iFlags = 0;

			if (IsDlgButtonChecked(hwndDlg, IDC_CHAT_1) == BST_CHECKED)
				iFlags |= GC_EVENT_ACTION;
			if (IsDlgButtonChecked(hwndDlg, IDC_CHAT_2) == BST_CHECKED)
				iFlags |= GC_EVENT_MESSAGE;
			if (IsDlgButtonChecked(hwndDlg, IDC_CHAT_3) == BST_CHECKED)
				iFlags |= GC_EVENT_NICK;
			if (IsDlgButtonChecked(hwndDlg, IDC_CHAT_4) == BST_CHECKED)
				iFlags |= GC_EVENT_JOIN;
			if (IsDlgButtonChecked(hwndDlg, IDC_CHAT_5) == BST_CHECKED)
				iFlags |= GC_EVENT_PART;
			if (IsDlgButtonChecked(hwndDlg, IDC_CHAT_6) == BST_CHECKED)
				iFlags |= GC_EVENT_TOPIC;
			if (IsDlgButtonChecked(hwndDlg, IDC_CHAT_7) == BST_CHECKED)
				iFlags |= GC_EVENT_ADDSTATUS;
			if (IsDlgButtonChecked(hwndDlg, IDC_CHAT_8) == BST_CHECKED)
				iFlags |= GC_EVENT_INFORMATION;
			if (IsDlgButtonChecked(hwndDlg, IDC_CHAT_9) == BST_CHECKED)
				iFlags |= GC_EVENT_QUIT;
			if (IsDlgButtonChecked(hwndDlg, IDC_CHAT_10) == BST_CHECKED)
				iFlags |= GC_EVENT_KICK;
			if (IsDlgButtonChecked(hwndDlg, IDC_CHAT_11) == BST_CHECKED)
				iFlags |= GC_EVENT_NOTICE;

			if (iFlags & GC_EVENT_ADDSTATUS)
				iFlags |= GC_EVENT_REMOVESTATUS;

			SendMessage(si->hWnd, GC_CHANGEFILTERFLAG, 0, iFlags);
			if (si->bFilterEnabled)
				SendMessage(si->hWnd, GC_REDRAWLOG, 0, 0);
			PostMessage(hwndDlg, WM_CLOSE, 0, 0);
		}
		break;

	case WM_CLOSE:
		DestroyWindow(hwndDlg);
		break;
	}

	return FALSE;
}

static LRESULT CALLBACK ButtonSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
	case WM_RBUTTONUP:
		if (db_get_b(NULL, CHAT_MODULE, "RightClickFilter", 0) != 0) {
			if (GetDlgItem(GetParent(hwnd), IDC_CHAT_FILTER) == hwnd)
				SendMessage(GetParent(hwnd), GC_SHOWFILTERMENU, 0, 0);
			if (GetDlgItem(GetParent(hwnd), IDC_CHAT_COLOR) == hwnd)
				SendMessage(GetParent(hwnd), GC_SHOWCOLORCHOOSER, 0, IDC_CHAT_COLOR);
			if (GetDlgItem(GetParent(hwnd), IDC_CHAT_BKGCOLOR) == hwnd)
				SendMessage(GetParent(hwnd), GC_SHOWCOLORCHOOSER, 0, IDC_CHAT_BKGCOLOR);
		}
		break;
	}

	return mir_callNextSubclass(hwnd, ButtonSubclassProc, msg, wParam, lParam);
}

static LRESULT CALLBACK LogSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static BOOL inMenu = FALSE;
	SESSION_INFO *si = (SESSION_INFO*)GetWindowLongPtr(GetParent(hwnd), GWLP_USERDATA);
	int result = InputAreaShortcuts(hwnd, msg, wParam, lParam, si);
	if (result != -1)
		return result;

	CHARRANGE sel;

	switch (msg) {
	case WM_MEASUREITEM:
		MeasureMenuItem(wParam, lParam);
		return TRUE;

	case WM_DRAWITEM:
		return DrawMenuItem(wParam, lParam);

	case WM_SETCURSOR:
		if (inMenu) {
			SetCursor(LoadCursor(NULL, IDC_ARROW));
			return TRUE;
		}
		break;

	case WM_LBUTTONUP:
		SendMessage(hwnd, EM_EXGETSEL, 0, (LPARAM)&sel);
		if (sel.cpMin != sel.cpMax) {
			SendMessage(hwnd, WM_COPY, 0, 0);
			sel.cpMin = sel.cpMax;
			SendMessage(hwnd, EM_EXSETSEL, 0, (LPARAM)&sel);
		}
		SetFocus(GetDlgItem(GetParent(hwnd), IDC_MESSAGE));
		break;

	case WM_ACTIVATE:
		if (LOWORD(wParam) == WA_INACTIVE) {
			SendMessage(hwnd, EM_EXGETSEL, 0, (LPARAM)&sel);
			if (sel.cpMin != sel.cpMax) {
				sel.cpMin = sel.cpMax;
				SendMessage(hwnd, EM_EXSETSEL, 0, (LPARAM)&sel);
			}
		}
		break;

	case WM_CONTEXTMENU:
		POINT pt;
		POINTL ptl;
		SendMessage(hwnd, EM_EXGETSEL, 0, (LPARAM)&sel);
		if (lParam == 0xFFFFFFFF) {
			SendMessage(hwnd, EM_POSFROMCHAR, (WPARAM)&pt, (LPARAM)sel.cpMax);
			ClientToScreen(hwnd, &pt);
		}
		else {
			pt.x = GET_X_LPARAM(lParam);
			pt.y = GET_Y_LPARAM(lParam);
		}
		ptl.x = (LONG)pt.x;
		ptl.y = (LONG)pt.y;
		ScreenToClient(hwnd, (LPPOINT)&ptl);
		{
			ptrW pszWord(GetRichTextWord(hwnd, &ptl));
			inMenu = TRUE;

			CHARRANGE all = { 0, -1 };
			HMENU hMenu = NULL;
			UINT uID = CreateGCMenu(hwnd, &hMenu, 1, pt, si, NULL, pszWord);
			inMenu = FALSE;
			switch (uID) {
			case 0:
				PostMessage(GetParent(hwnd), WM_MOUSEACTIVATE, 0, 0);
				break;

			case ID_COPYALL:
				SendMessage(hwnd, EM_EXGETSEL, 0, (LPARAM)&sel);
				SendMessage(hwnd, EM_EXSETSEL, 0, (LPARAM)&all);
				SendMessage(hwnd, WM_COPY, 0, 0);
				SendMessage(hwnd, EM_EXSETSEL, 0, (LPARAM)&sel);
				PostMessage(GetParent(hwnd), WM_MOUSEACTIVATE, 0, 0);
				break;

			case IDM_CLEAR:
				if (si) {
					SetWindowText(hwnd, L"");
					pci->LM_RemoveAll(&si->pLog, &si->pLogEnd);
					si->iEventCount = 0;
					si->LastTime = 0;
					PostMessage(GetParent(hwnd), WM_MOUSEACTIVATE, 0, 0);
				}
				break;

			case IDM_SEARCH_GOOGLE:
			case IDM_SEARCH_BING:
			case IDM_SEARCH_YANDEX:
			case IDM_SEARCH_YAHOO:
			case IDM_SEARCH_WIKIPEDIA:
			case IDM_SEARCH_FOODNETWORK:
			case IDM_SEARCH_GOOGLE_MAPS:
			case IDM_SEARCH_GOOGLE_TRANSLATE:
				SearchWord(pszWord, uID - IDM_SEARCH_GOOGLE + SEARCHENGINE_GOOGLE);
				PostMessage(GetParent(hwnd), WM_MOUSEACTIVATE, 0, 0);
				break;

			default:
				PostMessage(GetParent(hwnd), WM_MOUSEACTIVATE, 0, 0);
				pci->DoEventHookAsync(GetParent(hwnd), si->ptszID, si->pszModule, GC_USER_LOGMENU, NULL, NULL, uID);
				break;
			}
			DestroyGCMenu(&hMenu, 5);
		}
		break;

	case WM_CHAR:
		SetFocus(GetDlgItem(GetParent(hwnd), IDC_MESSAGE));
		SendDlgItemMessage(GetParent(hwnd), IDC_MESSAGE, WM_CHAR, wParam, lParam);
		break;
	}

	return mir_callNextSubclass(hwnd, LogSubclassProc, msg, wParam, lParam);
}

static LRESULT CALLBACK NicklistSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	SESSION_INFO *si = (SESSION_INFO*)GetWindowLongPtr(GetParent(hwnd), GWLP_USERDATA);
	int result = InputAreaShortcuts(hwnd, msg, wParam, lParam, si);
	if (result != -1)
		return result;

	switch (msg) {
	case WM_ERASEBKGND:
		{
			HDC dc = (HDC)wParam;
			if (dc) {
				int index = SendMessage(hwnd, LB_GETTOPINDEX, 0, 0);
				if (index == LB_ERR || si->nUsersInNicklist <= 0)
					return 0;

				int items = si->nUsersInNicklist - index;
				int height = SendMessage(hwnd, LB_GETITEMHEIGHT, 0, 0);

				if (height != LB_ERR) {
					RECT rc = { 0 };
					GetClientRect(hwnd, &rc);

					if (rc.bottom - rc.top > items * height) {
						rc.top = items*height;
						FillRect(dc, &rc, pci->hListBkgBrush);
					}
				}
			}
		}
		return 1;

	case WM_RBUTTONDOWN:
		SendMessage(hwnd, WM_LBUTTONDOWN, wParam, lParam);
		break;

	case WM_RBUTTONUP:
		SendMessage(hwnd, WM_LBUTTONUP, wParam, lParam);
		break;

	case WM_MEASUREITEM:
		{
			MEASUREITEMSTRUCT *mis = (MEASUREITEMSTRUCT *)lParam;
			if (mis->CtlType == ODT_MENU)
				return Menu_MeasureItem(lParam);
		}
		return FALSE;

	case WM_DRAWITEM:
		{
			DRAWITEMSTRUCT *dis = (DRAWITEMSTRUCT *)lParam;
			if (dis->CtlType == ODT_MENU)
				return Menu_DrawItem(lParam);
		}
		return FALSE;

	case WM_CONTEXTMENU:
		{
			int height = 0;

			TVHITTESTINFO hti;
			hti.pt.x = GET_X_LPARAM(lParam);
			hti.pt.y = GET_Y_LPARAM(lParam);
			if (hti.pt.x == -1 && hti.pt.y == -1) {
				int index = SendMessage(hwnd, LB_GETCURSEL, 0, 0);
				int top = SendMessage(hwnd, LB_GETTOPINDEX, 0, 0);
				height = SendMessage(hwnd, LB_GETITEMHEIGHT, 0, 0);
				hti.pt.x = 4;
				hti.pt.y = (index - top)*height + 1;
			}
			else ScreenToClient(hwnd, &hti.pt);

			DWORD item = (DWORD)(SendMessage(hwnd, LB_ITEMFROMPOINT, 0, MAKELPARAM(hti.pt.x, hti.pt.y)));
			if (HIWORD(item) == 1)
				item = (DWORD)(-1);
			else
				item &= 0xFFFF;

			USERINFO *ui = pci->SM_GetUserFromIndex(si->ptszID, si->pszModule, (int)item);
			if (ui) {
				HMENU hMenu = 0;
				UINT uID;
				USERINFO uinew;

				memcpy(&uinew, ui, sizeof(USERINFO));
				if (hti.pt.x == -1 && hti.pt.y == -1)
					hti.pt.y += height - 4;
				ClientToScreen(hwnd, &hti.pt);
				uID = CreateGCMenu(hwnd, &hMenu, 0, hti.pt, si, uinew.pszUID, uinew.pszNick);

				switch (uID) {
				case 0:
					break;

				case ID_MESS:
					pci->DoEventHookAsync(GetParent(hwnd), si->ptszID, si->pszModule, GC_USER_PRIVMESS, ui->pszUID, NULL, 0);
					break;

				default:
					pci->DoEventHookAsync(GetParent(hwnd), si->ptszID, si->pszModule, GC_USER_NICKLISTMENU, ui->pszUID, NULL, uID);
					break;
				}
				DestroyGCMenu(&hMenu, 1);
				return TRUE;
			}
		}
		break;

	case WM_GETDLGCODE:
		{
			BOOL isAlt = GetKeyState(VK_MENU) & 0x8000;
			BOOL isCtrl = (GetKeyState(VK_CONTROL) & 0x8000) && !isAlt;

			LPMSG lpmsg;
			if ((lpmsg = (LPMSG)lParam) != NULL) {
				if (lpmsg->message == WM_KEYDOWN
					&& (lpmsg->wParam == VK_RETURN || lpmsg->wParam == VK_ESCAPE || (lpmsg->wParam == VK_TAB && (isAlt || isCtrl))))
					return DLGC_WANTALLKEYS;
			}
		}
		break;

	case WM_KEYDOWN:
		if (wParam == VK_RETURN) {
			int index = SendMessage(hwnd, LB_GETCURSEL, 0, 0);
			if (index != LB_ERR) {
				USERINFO *ui = pci->SM_GetUserFromIndex(si->ptszID, si->pszModule, index);
				pci->DoEventHookAsync(GetParent(hwnd), si->ptszID, si->pszModule, GC_USER_PRIVMESS, ui->pszUID, NULL, 0);
			}
			break;
		}
		if (wParam == VK_ESCAPE || wParam == VK_UP || wParam == VK_DOWN || wParam == VK_NEXT ||
			wParam == VK_PRIOR || wParam == VK_TAB || wParam == VK_HOME || wParam == VK_END) {
			si->szSearch[0] = 0;
		}
		break;

	case WM_CHAR:
	case WM_UNICHAR:
		/*
		* simple incremental search for the user (nick) - list control
		* typing esc or movement keys will clear the current search string
		*/
		if (wParam == 27 && si->szSearch[0]) {						// escape - reset everything
			si->szSearch[0] = 0;
			break;
		}
		else if (wParam == '\b' && si->szSearch[0])					// backspace
			si->szSearch[mir_wstrlen(si->szSearch) - 1] = '\0';
		else if (wParam < ' ')
			break;
		else {
			wchar_t szNew[2];
			szNew[0] = (wchar_t)wParam;
			szNew[1] = '\0';
			if (mir_wstrlen(si->szSearch) >= _countof(si->szSearch) - 2) {
				MessageBeep(MB_OK);
				break;
			}
			mir_wstrcat(si->szSearch, szNew);
		}
		if (si->szSearch[0]) {
			// iterate over the (sorted) list of nicknames and search for the
			// string we have
			int iItems = SendMessage(hwnd, LB_GETCOUNT, 0, 0);
			for (int i = 0; i < iItems; i++) {
				USERINFO *ui = pci->UM_FindUserFromIndex(si->pUsers, i);
				if (ui) {
					if (!wcsnicmp(ui->pszNick, si->szSearch, mir_wstrlen(si->szSearch))) {
						SendMessage(hwnd, LB_SETCURSEL, i, 0);
						InvalidateRect(hwnd, NULL, FALSE);
						return 0;
					}
				}
			}

			MessageBeep(MB_OK);
			si->szSearch[mir_wstrlen(si->szSearch) - 1] = '\0';
			return 0;
		}
		break;

	case WM_MOUSEMOVE:
		Chat_HoverMouse(si, hwnd, lParam, ServiceExists("mToolTip/HideTip"));
		break;
	}

	return mir_callNextSubclass(hwnd, NicklistSubclassProc, msg, wParam, lParam);
}

int GetTextPixelSize(wchar_t* pszText, HFONT hFont, BOOL bWidth)
{
	if (!pszText || !hFont)
		return 0;

	HDC hdc = GetDC(NULL);
	HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

	RECT rc = { 0 };
	DrawText(hdc, pszText, -1, &rc, DT_CALCRECT);
	SelectObject(hdc, hOldFont);
	ReleaseDC(NULL, hdc);
	return bWidth ? rc.right - rc.left : rc.bottom - rc.top;
}

static void __cdecl phase2(void *lParam)
{
	Thread_SetName("Scriver: phase2");

	SESSION_INFO *si = (SESSION_INFO*)lParam;
	Sleep(30);
	if (si && si->hWnd)
		PostMessage(si->hWnd, GC_REDRAWLOG2, 0, 0);
}

static INT_PTR CALLBACK RoomWndProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static HMENU hToolbarMenu;
	RECT rc;
	POINT pt;
	HICON hIcon;
	TabControlData tcd;
	TitleBarData tbd;
	wchar_t szTemp[512];

	SESSION_INFO *si = (SESSION_INFO *)GetWindowLongPtr(hwndDlg, GWLP_USERDATA);
	if (!si && uMsg != WM_INITDIALOG)
		return FALSE;

	switch (uMsg) {
	case WM_INITDIALOG:
		TranslateDialogDefault(hwndDlg);
		{
			SESSION_INFO *psi = (SESSION_INFO*)lParam;
			NotifyLocalWinEvent(psi->hContact, hwndDlg, MSG_WINDOW_EVT_OPENING);

			TranslateDialogDefault(hwndDlg);
			SetWindowLongPtr(hwndDlg, GWLP_USERDATA, (LONG_PTR)psi);
			si = psi;
			RichUtil_SubClass(GetDlgItem(hwndDlg, IDC_MESSAGE));
			RichUtil_SubClass(GetDlgItem(hwndDlg, IDC_LOG));
			RichUtil_SubClass(GetDlgItem(hwndDlg, IDC_CHAT_LIST));
			mir_subclassWindow(GetDlgItem(hwndDlg, IDC_SPLITTERX), SplitterSubclassProc);
			mir_subclassWindow(GetDlgItem(hwndDlg, IDC_CHAT_LIST), NicklistSubclassProc);
			mir_subclassWindow(GetDlgItem(hwndDlg, IDC_LOG), LogSubclassProc);
			mir_subclassWindow(GetDlgItem(hwndDlg, IDC_CHAT_FILTER), ButtonSubclassProc);
			mir_subclassWindow(GetDlgItem(hwndDlg, IDC_CHAT_COLOR), ButtonSubclassProc);
			mir_subclassWindow(GetDlgItem(hwndDlg, IDC_CHAT_BKGCOLOR), ButtonSubclassProc);
			mir_subclassWindow(GetDlgItem(hwndDlg, IDC_MESSAGE), MessageSubclassProc);

			Srmm_CreateToolbarIcons(hwndDlg, BBBF_ISCHATBUTTON);

			RECT minEditInit;
			GetWindowRect(GetDlgItem(hwndDlg, IDC_MESSAGE), &minEditInit);
			si->minEditBoxHeight = minEditInit.bottom - minEditInit.top;
			si->minLogBoxHeight = si->minEditBoxHeight;

			SendDlgItemMessage(hwndDlg, IDC_MESSAGE, EM_SUBCLASSED, 0, 0);
			SendDlgItemMessage(hwndDlg, IDC_LOG, EM_AUTOURLDETECT, 1, 0);
			int mask = (int)SendDlgItemMessage(hwndDlg, IDC_LOG, EM_GETEVENTMASK, 0, 0);
			SendDlgItemMessage(hwndDlg, IDC_LOG, EM_SETEVENTMASK, 0, mask | ENM_LINK | ENM_MOUSEEVENTS);
			SendDlgItemMessage(hwndDlg, IDC_MESSAGE, EM_SETEVENTMASK, 0, ENM_MOUSEEVENTS | ENM_KEYEVENTS | ENM_CHANGE | ENM_REQUESTRESIZE);
			SendDlgItemMessage(hwndDlg, IDC_LOG, EM_LIMITTEXT, sizeof(wchar_t) * 0x7FFFFFFF, 0);
			SendDlgItemMessage(hwndDlg, IDC_LOG, EM_SETOLECALLBACK, 0, (LPARAM)&reOleCallback);

			if (db_get_b(NULL, CHAT_MODULE, "UseIEView", 0)) {
				IEVIEWWINDOW ieWindow = { sizeof(ieWindow) };
				ieWindow.iType = IEW_CREATE;
				ieWindow.dwMode = IEWM_CHAT;
				ieWindow.parent = hwndDlg;
				ieWindow.cx = 200;
				ieWindow.cy = 300;
				CallService(MS_IEVIEW_WINDOW, 0, (LPARAM)&ieWindow);

				si->hwndLog = ieWindow.hwnd;

				IEVIEWEVENT iee = { sizeof(iee) };
				iee.iType = IEE_CLEAR_LOG;
				iee.hwnd = si->hwndLog;
				iee.hContact = si->hContact;
				iee.pszProto = si->pszModule;
				CallService(MS_IEVIEW_EVENT, 0, (LPARAM)&iee);
			}

			SendDlgItemMessage(hwndDlg, IDC_LOG, EM_HIDESELECTION, TRUE, 0);

			SendMessage(hwndDlg, GC_SETWNDPROPS, 0, 0);
			SendMessage(hwndDlg, DM_UPDATESTATUSBAR, 0, 0);
			SendMessage(hwndDlg, DM_UPDATETITLEBAR, 0, 0);

			SendMessage(GetParent(hwndDlg), CM_ADDCHILD, (WPARAM)hwndDlg, psi->hContact);
			PostMessage(hwndDlg, GC_UPDATENICKLIST, 0, 0);
			NotifyLocalWinEvent(psi->hContact, hwndDlg, MSG_WINDOW_EVT_OPEN);
		}
		break;

	case GC_SETWNDPROPS:
		// LoadGlobalSettings();
		InitButtons(hwndDlg, si);

		SendMessage(hwndDlg, DM_UPDATESTATUSBAR, 0, 0);
		SendMessage(hwndDlg, DM_UPDATETITLEBAR, 0, 0);
		SendMessage(hwndDlg, GC_FIXTABICONS, 0, 0);

		SendDlgItemMessage(hwndDlg, IDC_LOG, EM_SETBKGNDCOLOR, 0, g_Settings.crLogBackground);
		{
			// messagebox
			COLORREF crFore;
			LoadMsgDlgFont(MSGFONTID_MESSAGEAREA, NULL, &crFore);

			CHARFORMAT2 cf;
			cf.cbSize = sizeof(CHARFORMAT2);
			cf.dwMask = CFM_COLOR | CFM_BOLD | CFM_UNDERLINE | CFM_BACKCOLOR;
			cf.dwEffects = 0;
			cf.crTextColor = crFore;
			cf.crBackColor = db_get_dw(NULL, SRMMMOD, SRMSGSET_INPUTBKGCOLOUR, SRMSGDEFSET_INPUTBKGCOLOUR);
			SendDlgItemMessage(hwndDlg, IDC_MESSAGE, EM_SETBKGNDCOLOR, 0, db_get_dw(NULL, SRMMMOD, SRMSGSET_INPUTBKGCOLOUR, SRMSGDEFSET_INPUTBKGCOLOUR));
			SendDlgItemMessage(hwndDlg, IDC_MESSAGE, WM_SETFONT, (WPARAM)g_Settings.MessageBoxFont, MAKELPARAM(TRUE, 0));
			SendDlgItemMessage(hwndDlg, IDC_MESSAGE, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);
		
			// nicklist
			int ih = GetTextPixelSize(L"AQG_glo'", g_Settings.UserListFont, FALSE);
			int ih2 = GetTextPixelSize(L"AQG_glo'", g_Settings.UserListHeadingsFont, FALSE);
			int height = db_get_b(NULL, CHAT_MODULE, "NicklistRowDist", 12);
			int font = ih > ih2 ? ih : ih2;
			// make sure we have space for icon!
			if (db_get_b(NULL, CHAT_MODULE, "ShowContactStatus", 0))
				font = font > 16 ? font : 16;

			SendDlgItemMessage(hwndDlg, IDC_CHAT_LIST, LB_SETITEMHEIGHT, 0, height > font ? height : font);
			InvalidateRect(GetDlgItem(hwndDlg, IDC_CHAT_LIST), NULL, TRUE);
		}
		SendDlgItemMessage(hwndDlg, IDC_MESSAGE, EM_REQUESTRESIZE, 0, 0);
		SendMessage(hwndDlg, WM_SIZE, 0, 0);
		SendMessage(hwndDlg, GC_REDRAWLOG2, 0, 0);
		break;

	case DM_UPDATETITLEBAR:
		if (g_dat.flags & SMF_STATUSICON) {
			MODULEINFO *mi = pci->MM_FindModule(si->pszModule);
			tbd.hIcon = (si->wStatus == ID_STATUS_ONLINE) ? mi->hOnlineIcon : mi->hOfflineIcon;
			tbd.hIconBig = (si->wStatus == ID_STATUS_ONLINE) ? mi->hOnlineIconBig : mi->hOfflineIconBig;
		}
		else {
			tbd.hIcon = GetCachedIcon("chat_window");
			tbd.hIconBig = g_dat.hIconChatBig;
		}
		tbd.hIconNot = (si->wState & (GC_EVENT_HIGHLIGHT | STATE_TALK)) ? GetCachedIcon("chat_overlay") : NULL;

		switch (si->iType) {
		case GCW_CHATROOM:
			mir_snwprintf(szTemp,
				(si->nUsersInNicklist == 1) ? TranslateT("%s: chat room (%u user)") : TranslateT("%s: chat room (%u users)"),
				si->ptszName, si->nUsersInNicklist);
			break;
		case GCW_PRIVMESS:
			mir_snwprintf(szTemp,
				(si->nUsersInNicklist == 1) ? TranslateT("%s: message session") : TranslateT("%s: message session (%u users)"),
				si->ptszName, si->nUsersInNicklist);
			break;
		case GCW_SERVER:
			mir_snwprintf(szTemp, L"%s: Server", si->ptszName);
			break;
		}
		tbd.iFlags = TBDF_TEXT | TBDF_ICON;
		tbd.pszText = szTemp;
		SendMessage(GetParent(hwndDlg), CM_UPDATETITLEBAR, (WPARAM)&tbd, (LPARAM)hwndDlg);
		SendMessage(hwndDlg, DM_UPDATETABCONTROL, 0, 0);
		break;

	case DM_UPDATESTATUSBAR:
		{
			MODULEINFO *mi = pci->MM_FindModule(si->pszModule);
			hIcon = si->wStatus == ID_STATUS_ONLINE ? mi->hOnlineIcon : mi->hOfflineIcon;
			mir_snwprintf(szTemp, L"%s : %s", mi->ptszModDispName, si->ptszStatusbarText ? si->ptszStatusbarText : L"");

			StatusBarData sbd;
			sbd.iItem = 0;
			sbd.iFlags = SBDF_TEXT | SBDF_ICON;
			sbd.hIcon = hIcon;
			sbd.pszText = szTemp;
			SendMessage(GetParent(hwndDlg), CM_UPDATESTATUSBAR, (WPARAM)&sbd, (LPARAM)hwndDlg);

			sbd.iItem = 1;
			sbd.hIcon = NULL;
			sbd.pszText = L"";
			SendMessage(GetParent(hwndDlg), CM_UPDATESTATUSBAR, (WPARAM)&sbd, (LPARAM)hwndDlg);

			StatusIconData sid = { sizeof(sid) };
			sid.szModule = SRMMMOD;
			Srmm_ModifyIcon(si->hContact, &sid);
		}
		break;

	case DM_SWITCHINFOBAR:
	case DM_SWITCHTOOLBAR:
		SendMessage(hwndDlg, WM_SIZE, 0, 0);
		break;

	case WM_SIZE:
		if (wParam == SIZE_MAXIMIZED)
			PostMessage(hwndDlg, GC_SCROLLTOBOTTOM, 0, 0);

		if (IsIconic(hwndDlg)) break;

		if (wParam == SIZE_RESTORED || wParam == SIZE_MAXIMIZED) {
			int dlgWidth, dlgHeight;
			dlgWidth = LOWORD(lParam);
			dlgHeight = HIWORD(lParam);
			GetClientRect(hwndDlg, &rc);

			dlgWidth = rc.right - rc.left;
			dlgHeight = rc.bottom - rc.top;
			MessageDialogResize(hwndDlg, si, dlgWidth, dlgHeight);
		}
		break;

	case GC_REDRAWWINDOW:
		InvalidateRect(hwndDlg, NULL, TRUE);
		break;

	case GC_REDRAWLOG:
		si->LastTime = 0;
		if (si->pLog) {
			LOGINFO *pLog = si->pLog;
			if (si->iEventCount > 60) {
				int index = 0;
				while (index < 59) {
					if (pLog->next == NULL)
						break;

					pLog = pLog->next;
					if ((si->iType != GCW_CHATROOM && si->iType != GCW_PRIVMESS) || !si->bFilterEnabled || (si->iLogFilterFlags&pLog->iType) != 0)
						index++;
				}
				Log_StreamInEvent(hwndDlg, pLog, si, TRUE);
				mir_forkthread(phase2, si);
			}
			else Log_StreamInEvent(hwndDlg, si->pLogEnd, si, TRUE);
		}
		else SendMessage(hwndDlg, GC_CONTROL_MSG, WINDOW_CLEARLOG, 0);
		break;

	case GC_REDRAWLOG2:
		si->LastTime = 0;
		if (si->pLog)
			Log_StreamInEvent(hwndDlg, si->pLogEnd, si, TRUE);
		break;

	case GC_ADDLOG:
		if (si->pLogEnd)
			Log_StreamInEvent(hwndDlg, si->pLog, si, FALSE);
		else
			SendMessage(hwndDlg, GC_CONTROL_MSG, WINDOW_CLEARLOG, 0);
		break;

	case DM_UPDATETABCONTROL:
		tcd.iFlags = TCDF_TEXT;
		tcd.pszText = si->ptszName;
		SendMessage(GetParent(hwndDlg), CM_UPDATETABCONTROL, (WPARAM)&tcd, (LPARAM)hwndDlg);
		// fall through

	case GC_FIXTABICONS:
		if (!(si->wState & GC_EVENT_HIGHLIGHT)) {
			if (si->wState & STATE_TALK)
				hIcon = (si->wStatus == ID_STATUS_ONLINE) ? pci->MM_FindModule(si->pszModule)->hOnlineTalkIcon : pci->MM_FindModule(si->pszModule)->hOfflineTalkIcon;
			else
				hIcon = (si->wStatus == ID_STATUS_ONLINE) ? pci->MM_FindModule(si->pszModule)->hOnlineIcon : pci->MM_FindModule(si->pszModule)->hOfflineIcon;
		}
		else hIcon = g_dat.hMsgIcon;

		tcd.iFlags = TCDF_ICON;
		tcd.hIcon = hIcon;
		SendMessage(GetParent(hwndDlg), CM_UPDATETABCONTROL, (WPARAM)&tcd, (LPARAM)hwndDlg);
		break;

	case GC_SETMESSAGEHIGHLIGHT:
		si->wState |= GC_EVENT_HIGHLIGHT;
		SendMessage(si->hWnd, GC_FIXTABICONS, 0, 0);
		SendMessage(hwndDlg, DM_UPDATETITLEBAR, 0, 0);
		if (g_Settings.bFlashWindowHighlight && GetActiveWindow() != hwndDlg && GetForegroundWindow() != GetParent(hwndDlg))
			SendMessage(GetParent(si->hWnd), CM_STARTFLASHING, 0, 0);
		break;

	case GC_SETTABHIGHLIGHT:
		SendMessage(si->hWnd, GC_FIXTABICONS, 0, 0);
		SendMessage(hwndDlg, DM_UPDATETITLEBAR, 0, 0);
		if (g_Settings.bFlashWindow && GetActiveWindow() != GetParent(hwndDlg) && GetForegroundWindow() != GetParent(hwndDlg))
			SendMessage(GetParent(si->hWnd), CM_STARTFLASHING, 0, 0);
		break;

	case DM_ACTIVATE:
		if (si->wState & STATE_TALK) {
			si->wState &= ~STATE_TALK;
			db_set_w(si->hContact, si->pszModule, "ApparentMode", 0);
		}

		if (si->wState & GC_EVENT_HIGHLIGHT) {
			si->wState &= ~GC_EVENT_HIGHLIGHT;

			if (pcli->pfnGetEvent(si->hContact, 0))
				pcli->pfnRemoveEvent(si->hContact, GC_FAKE_EVENT);
		}

		SendMessage(hwndDlg, GC_FIXTABICONS, 0, 0);
		if (!si->hWnd) {
			ShowRoom(si, WINDOW_VISIBLE, TRUE);
			SendMessage(hwndDlg, WM_MOUSEACTIVATE, 0, 0);
		}
		break;

	case WM_CTLCOLORLISTBOX:
		SetBkColor((HDC)wParam, g_Settings.crUserListBGColor);
		return (INT_PTR)pci->hListBkgBrush;

	case WM_MEASUREITEM:
		if (!MeasureMenuItem(wParam, lParam)) {
			MEASUREITEMSTRUCT *mis = (MEASUREITEMSTRUCT *)lParam;
			if (mis->CtlType == ODT_MENU)
				return Menu_MeasureItem(lParam);

			int ih = GetTextPixelSize(L"AQGgl'", g_Settings.UserListFont, FALSE);
			int ih2 = GetTextPixelSize(L"AQGg'", g_Settings.UserListHeadingsFont, FALSE);
			int font = ih > ih2 ? ih : ih2;
			int height = db_get_b(NULL, CHAT_MODULE, "NicklistRowDist", 12);
			// make sure we have space for icon!
			if (db_get_b(NULL, CHAT_MODULE, "ShowContactStatus", 0))
				font = font > 16 ? font : 16;
			mis->itemHeight = height > font ? height : font;
		}
		return TRUE;

	case WM_DRAWITEM:
		if (!DrawMenuItem(wParam, lParam)) {
			DRAWITEMSTRUCT *dis = (DRAWITEMSTRUCT *)lParam;
			if (dis->CtlType == ODT_MENU)
				return Menu_DrawItem(lParam);

			if (dis->CtlID == IDC_CHAT_LIST) {
				int index = dis->itemID;
				USERINFO *ui = pci->SM_GetUserFromIndex(si->ptszID, si->pszModule, index);
				if (ui) {
					int x_offset = 2;

					int height = dis->rcItem.bottom - dis->rcItem.top;
					if (height & 1)
						height++;

					int offset = (height == 10) ? 0 : height / 2 - 5;
					HFONT hFont = (ui->iStatusEx == 0) ? g_Settings.UserListFont : g_Settings.UserListHeadingsFont;
					HFONT hOldFont = (HFONT)SelectObject(dis->hDC, hFont);
					SetBkMode(dis->hDC, TRANSPARENT);

					if (dis->itemAction == ODA_FOCUS && dis->itemState & ODS_SELECTED)
						FillRect(dis->hDC, &dis->rcItem, pci->hListSelectedBkgBrush);
					else //if (dis->itemState & ODS_INACTIVE)
						FillRect(dis->hDC, &dis->rcItem, pci->hListBkgBrush);

					if (g_Settings.bShowContactStatus && g_Settings.bContactStatusFirst && ui->ContactStatus) {
						hIcon = Skin_LoadProtoIcon(si->pszModule, ui->ContactStatus);
						DrawIconEx(dis->hDC, x_offset, dis->rcItem.top + offset - 3, hIcon, 16, 16, 0, NULL, DI_NORMAL);
						IcoLib_ReleaseIcon(hIcon);
						x_offset += 18;
					}
					DrawIconEx(dis->hDC, x_offset, dis->rcItem.top + offset, pci->SM_GetStatusIcon(si, ui), 10, 10, 0, NULL, DI_NORMAL);
					x_offset += 12;
					if (g_Settings.bShowContactStatus && !g_Settings.bContactStatusFirst && ui->ContactStatus) {
						hIcon = Skin_LoadProtoIcon(si->pszModule, ui->ContactStatus);
						DrawIconEx(dis->hDC, x_offset, dis->rcItem.top + offset - 3, hIcon, 16, 16, 0, NULL, DI_NORMAL);
						IcoLib_ReleaseIcon(hIcon);
						x_offset += 18;
					}

					SetTextColor(dis->hDC, ui->iStatusEx == 0 ? g_Settings.crUserListColor : g_Settings.crUserListHeadingsColor);
					TextOut(dis->hDC, dis->rcItem.left + x_offset, dis->rcItem.top, ui->pszNick, (int)mir_wstrlen(ui->pszNick));
					SelectObject(dis->hDC, hOldFont);
				}
				return TRUE;
			}
		}
		break;

	case GC_UPDATENICKLIST:
		SendDlgItemMessage(hwndDlg, IDC_CHAT_LIST, WM_SETREDRAW, FALSE, 0);
		SendDlgItemMessage(hwndDlg, IDC_CHAT_LIST, LB_RESETCONTENT, 0, 0);
		for (int index = 0; index < si->nUsersInNicklist; index++) {
			USERINFO *ui = pci->SM_GetUserFromIndex(si->ptszID, si->pszModule, index);
			if (ui) {
				char szIndicator = SM_GetStatusIndicator(si, ui);
				if (szIndicator > '\0') {
					static wchar_t ptszBuf[128];
					mir_snwprintf(ptszBuf, L"%c%s", szIndicator, ui->pszNick);
					SendDlgItemMessage(hwndDlg, IDC_CHAT_LIST, LB_ADDSTRING, 0, (LPARAM)ptszBuf);
				}
				else SendDlgItemMessage(hwndDlg, IDC_CHAT_LIST, LB_ADDSTRING, 0, (LPARAM)ui->pszNick);
			}
		}
		SendDlgItemMessage(hwndDlg, IDC_CHAT_LIST, WM_SETREDRAW, TRUE, 0);
		InvalidateRect(GetDlgItem(hwndDlg, IDC_CHAT_LIST), NULL, FALSE);
		UpdateWindow(GetDlgItem(hwndDlg, IDC_CHAT_LIST));
		SendMessage(hwndDlg, DM_UPDATETITLEBAR, 0, 0);
		break;

	case GC_CONTROL_MSG:
		switch (wParam) {
		case SESSION_OFFLINE:
			SendMessage(hwndDlg, DM_UPDATESTATUSBAR, 0, 0);
			SendMessage(si->hWnd, GC_UPDATENICKLIST, 0, 0);
			return TRUE;

		case SESSION_ONLINE:
			SendMessage(hwndDlg, DM_UPDATESTATUSBAR, 0, 0);
			return TRUE;

		case WINDOW_HIDDEN:
			SendMessage(hwndDlg, GC_CLOSEWINDOW, 0, 0);
			return TRUE;

		case WINDOW_CLEARLOG:
			SetDlgItemText(hwndDlg, IDC_LOG, L"");
			return TRUE;

		case SESSION_TERMINATE:
			if (pcli->pfnGetEvent(si->hContact, 0))
				pcli->pfnRemoveEvent(si->hContact, GC_FAKE_EVENT);
			si->wState &= ~STATE_TALK;
			db_set_w(si->hContact, si->pszModule, "ApparentMode", 0);
			SendMessage(hwndDlg, GC_CLOSEWINDOW, 0, 0);
			return TRUE;

		case WINDOW_MINIMIZE:
			ShowWindow(hwndDlg, SW_MINIMIZE);
			goto LABEL_SHOWWINDOW;

		case WINDOW_MAXIMIZE:
			ShowWindow(hwndDlg, SW_MAXIMIZE);
			goto LABEL_SHOWWINDOW;

		case SESSION_INITDONE:
			if (db_get_b(NULL, CHAT_MODULE, "PopupOnJoin", 0) != 0)
				return TRUE;
			// fall through
		case WINDOW_VISIBLE:
			if (IsIconic(hwndDlg))
				ShowWindow(hwndDlg, SW_NORMAL);
LABEL_SHOWWINDOW:
			SendMessage(hwndDlg, WM_SIZE, 0, 0);
			SendMessage(hwndDlg, GC_REDRAWLOG, 0, 0);
			SendMessage(hwndDlg, GC_UPDATENICKLIST, 0, 0);
			SendMessage(hwndDlg, DM_UPDATESTATUSBAR, 0, 0);
			ShowWindow(hwndDlg, SW_SHOW);
			SendMessage(hwndDlg, WM_SIZE, 0, 0);
			SetForegroundWindow(hwndDlg);
			return TRUE;
		}
		break;

	case GC_SPLITTERMOVED:
		if ((HWND)lParam == GetDlgItem(hwndDlg, IDC_SPLITTERX)) {
			GetClientRect(hwndDlg, &rc);
			pt.x = wParam; pt.y = 0;
			ScreenToClient(hwndDlg, &pt);

			si->iSplitterX = rc.right - pt.x + 1;
			if (si->iSplitterX < 35)
				si->iSplitterX = 35;
			if (si->iSplitterX > rc.right - rc.left - 35)
				si->iSplitterX = rc.right - rc.left - 35;
			g_Settings.iSplitterX = si->iSplitterX;
		}
		else if ((HWND)lParam == GetDlgItem(hwndDlg, IDC_SPLITTERY)) {
			GetClientRect(hwndDlg, &rc);
			pt.x = 0; pt.y = wParam;
			ScreenToClient(hwndDlg, &pt);
			si->iSplitterY = rc.bottom - pt.y;
			g_Settings.iSplitterY = si->iSplitterY;
		}
		PostMessage(hwndDlg, WM_SIZE, 0, 0);
		break;

	case GC_FIREHOOK:
		if (lParam) {
			NotifyEventHooks(pci->hSendEvent, 0, lParam);
			GCHOOK *gch = (GCHOOK*)lParam;
			if (gch->pDest) {
				mir_free((void*)gch->pDest->ptszID);
				mir_free((void*)gch->pDest->pszModule);
				mir_free(gch->pDest);
			}
			mir_free(gch->ptszText);
			mir_free(gch->ptszUID);
			mir_free(gch);
		}
		break;

	case GC_CHANGEFILTERFLAG:
		si->iLogFilterFlags = lParam;
		break;

	case GC_SHOWFILTERMENU:
		{
			HWND hwnd = CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_FILTER), hwndDlg, FilterWndProc, (LPARAM)si);
			TranslateDialogDefault(hwnd);
			GetWindowRect(GetDlgItem(hwndDlg, IDC_CHAT_FILTER), &rc);
			SetWindowPos(hwnd, HWND_TOP, rc.left - 85, (IsWindowVisible(GetDlgItem(hwndDlg, IDC_CHAT_FILTER)) || IsWindowVisible(GetDlgItem(hwndDlg, IDC_CHAT_BOLD))) ? rc.top - 206 : rc.top - 186, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);
		}
		break;

	case GC_SHOWCOLORCHOOSER:
		pci->ColorChooser(si, lParam == IDC_CHAT_COLOR, hwndDlg, GetDlgItem(hwndDlg, IDC_MESSAGE), GetDlgItem(hwndDlg, lParam));
		break;

	case GC_SCROLLTOBOTTOM:
		if ((GetWindowLongPtr(GetDlgItem(hwndDlg, IDC_LOG), GWL_STYLE) & WS_VSCROLL) != 0) {
			SCROLLINFO sci = { 0 };
			sci.cbSize = sizeof(sci);
			sci.fMask = SIF_PAGE | SIF_RANGE;
			GetScrollInfo(GetDlgItem(hwndDlg, IDC_LOG), SB_VERT, &sci);

			sci.fMask = SIF_POS;
			sci.nPos = sci.nMax - sci.nPage + 1;
			SetScrollInfo(GetDlgItem(hwndDlg, IDC_LOG), SB_VERT, &sci, TRUE);

			CHARRANGE sel;
			sel.cpMin = sel.cpMax = GetRichTextLength(GetDlgItem(hwndDlg, IDC_LOG), CP_ACP, FALSE);
			SendDlgItemMessage(hwndDlg, IDC_LOG, EM_EXSETSEL, 0, (LPARAM)&sel);
			PostMessage(GetDlgItem(hwndDlg, IDC_LOG), WM_VSCROLL, MAKEWPARAM(SB_BOTTOM, 0), 0);
		}
		break;

	case WM_ACTIVATE:
		if (LOWORD(wParam) != WA_ACTIVE)
			break;

		//fall through
	case WM_MOUSEACTIVATE:
		if (uMsg != WM_ACTIVATE)
			SetFocus(GetDlgItem(hwndDlg, IDC_MESSAGE));

		pci->SetActiveSession(si->ptszID, si->pszModule);

		if (db_get_w(si->hContact, si->pszModule, "ApparentMode", 0) != 0)
			db_set_w(si->hContact, si->pszModule, "ApparentMode", 0);
		if (pcli->pfnGetEvent(si->hContact, 0))
			pcli->pfnRemoveEvent(si->hContact, GC_FAKE_EVENT);
		break;

	case WM_NOTIFY:
		{
			LPNMHDR pNmhdr = (LPNMHDR)lParam;
			switch (pNmhdr->code) {
			case EN_REQUESTRESIZE:
				if (pNmhdr->idFrom == IDC_MESSAGE) {
					REQRESIZE *rr = (REQRESIZE *)lParam;
					int height = rr->rc.bottom - rr->rc.top + 1;
					if (height < g_dat.minInputAreaHeight)
						height = g_dat.minInputAreaHeight;

					if (si->desiredInputAreaHeight != height) {
						si->desiredInputAreaHeight = height;
						SendMessage(hwndDlg, WM_SIZE, 0, 0);
						PostMessage(hwndDlg, GC_SCROLLTOBOTTOM, 0, 0);
					}
				}
				break;

			case EN_MSGFILTER:
				if (pNmhdr->idFrom == IDC_LOG && ((MSGFILTER *)lParam)->msg == WM_RBUTTONUP) {
					SetWindowLongPtr(hwndDlg, DWLP_MSGRESULT, TRUE);
					return TRUE;
				}
				break;

			case EN_LINK:
				if (pNmhdr->idFrom == IDC_LOG) {
					switch (((ENLINK *)lParam)->msg) {
					case WM_RBUTTONDOWN:
					case WM_LBUTTONUP:
					case WM_LBUTTONDBLCLK:
						if (HandleLinkClick(g_hInst, hwndDlg, GetDlgItem(hwndDlg, IDC_MESSAGE), (ENLINK*)lParam)) {
							SetWindowLongPtr(hwndDlg, DWLP_MSGRESULT, TRUE);
							return TRUE;
						}
						break;
					}
				}
				break;

			case TTN_NEEDTEXT:
				if (pNmhdr->idFrom == (UINT_PTR)GetDlgItem(hwndDlg, IDC_CHAT_LIST)) {
					LPNMTTDISPINFO lpttd = (LPNMTTDISPINFO)lParam;
					SESSION_INFO* parentdat = (SESSION_INFO*)GetWindowLongPtr(hwndDlg, GWLP_USERDATA);

					POINT p;
					GetCursorPos(&p);
					ScreenToClient(GetDlgItem(hwndDlg, IDC_CHAT_LIST), &p);
					int item = LOWORD(SendDlgItemMessage(hwndDlg, IDC_CHAT_LIST, LB_ITEMFROMPOINT, 0, MAKELPARAM(p.x, p.y)));
					USERINFO *ui = pci->SM_GetUserFromIndex(parentdat->ptszID, parentdat->pszModule, item);
					if (ui != NULL) {
						static wchar_t ptszBuf[1024];
						mir_snwprintf(ptszBuf, L"%s: %s\r\n%s: %s\r\n%s: %s",
							TranslateT("Nickname"), ui->pszNick,
							TranslateT("Unique ID"), ui->pszUID,
							TranslateT("Status"), pci->TM_WordToString(parentdat->pStatuses, ui->Status));
						lpttd->lpszText = ptszBuf;
					}
				}
				break;
			}
		}
		break;

	case WM_COMMAND:
		if (!lParam && Clist_MenuProcessCommand(LOWORD(wParam), MPCF_CONTACTMENU, si->hContact))
			break;

		if (HIWORD(wParam) == BN_CLICKED)
			if (LOWORD(wParam) >= MIN_CBUTTONID && LOWORD(wParam) <= MAX_CBUTTONID) {
				Srmm_ClickToolbarIcon(si->hContact, LOWORD(wParam), GetDlgItem(hwndDlg, LOWORD(wParam)), 0);
				break;
			}

		switch (LOWORD(wParam)) {
		case IDC_CHAT_LIST:
			if (HIWORD(wParam) == LBN_DBLCLK) {
				TVHITTESTINFO hti;
				hti.pt.x = (short)LOWORD(GetMessagePos());
				hti.pt.y = (short)HIWORD(GetMessagePos());
				ScreenToClient(GetDlgItem(hwndDlg, IDC_CHAT_LIST), &hti.pt);

				int item = LOWORD(SendDlgItemMessage(hwndDlg, IDC_CHAT_LIST, LB_ITEMFROMPOINT, 0, MAKELPARAM(hti.pt.x, hti.pt.y)));
				USERINFO *ui = pci->SM_GetUserFromIndex(si->ptszID, si->pszModule, item);
				if (ui) {
					if (GetKeyState(VK_SHIFT) & 0x8000) {
						LRESULT lResult = (LRESULT)SendDlgItemMessage(hwndDlg, IDC_MESSAGE, EM_GETSEL, 0, 0);
						int start = LOWORD(lResult);
						size_t dwNameLenMax = (mir_wstrlen(ui->pszUID) + 4);
						wchar_t* pszName = (wchar_t*)alloca(sizeof(wchar_t) * dwNameLenMax);
						if (start == 0)
							mir_snwprintf(pszName, dwNameLenMax, L"%s: ", ui->pszUID);
						else
							mir_snwprintf(pszName, dwNameLenMax, L"%s ", ui->pszUID);

						SendDlgItemMessage(hwndDlg, IDC_MESSAGE, EM_REPLACESEL, FALSE, (LPARAM)pszName);
						PostMessage(hwndDlg, WM_MOUSEACTIVATE, 0, 0);
					}
					else pci->DoEventHookAsync(hwndDlg, si->ptszID, si->pszModule, GC_USER_PRIVMESS, ui->pszUID, NULL, 0);
				}

				return TRUE;
			}

			if (HIWORD(wParam) == LBN_KILLFOCUS)
				RedrawWindow(GetDlgItem(hwndDlg, IDC_CHAT_LIST), NULL, NULL, RDW_INVALIDATE);
			break;

		case IDOK:
			if (IsWindowEnabled(GetDlgItem(hwndDlg, IDOK))) {
				char *pszRtf = GetRichTextRTF(GetDlgItem(hwndDlg, IDC_MESSAGE));
				if (pszRtf == NULL)
					break;

				MODULEINFO *mi = pci->MM_FindModule(si->pszModule);
				if (mi == NULL)
					break;

				TCmdList *cmdListNew = tcmdlist_last(si->cmdList);
				while (cmdListNew != NULL && cmdListNew->temporary) {
					si->cmdList = tcmdlist_remove(si->cmdList, cmdListNew);
					cmdListNew = tcmdlist_last(si->cmdList);
				}

				// takes pszRtf to a queue, no leak here
				si->cmdList = tcmdlist_append(si->cmdList, pszRtf, 20, FALSE);

				CMStringW ptszText(ptrW(mir_utf8decodeW(pszRtf)));
				pci->DoRtfToTags(ptszText, mi->nColorCount, mi->crColors);
				ptszText.Trim();
				ptszText.Replace(L"%", L"%%");

				if (mi->bAckMsg) {
					EnableWindow(GetDlgItem(hwndDlg, IDC_MESSAGE), FALSE);
					SendDlgItemMessage(hwndDlg, IDC_MESSAGE, EM_SETREADONLY, TRUE, 0);
				}
				else SetDlgItemText(hwndDlg, IDC_MESSAGE, L"");

				EnableWindow(GetDlgItem(hwndDlg, IDOK), FALSE);

				pci->DoEventHookAsync(hwndDlg, si->ptszID, si->pszModule, GC_USER_MESSAGE, NULL, ptszText, 0);
				SetFocus(GetDlgItem(hwndDlg, IDC_MESSAGE));
			}
			break;

		case IDC_CHAT_SHOWNICKLIST:
			if (!IsWindowEnabled(GetDlgItem(hwndDlg, IDC_CHAT_SHOWNICKLIST)))
				break;
			if (si->iType == GCW_SERVER)
				break;

			si->bNicklistEnabled = !si->bNicklistEnabled;

			SendDlgItemMessage(hwndDlg, IDC_CHAT_SHOWNICKLIST, BM_SETIMAGE, IMAGE_ICON, (LPARAM)GetCachedIcon(si->bNicklistEnabled ? "chat_nicklist" : "chat_nicklist2"));
			SendMessage(hwndDlg, GC_SCROLLTOBOTTOM, 0, 0);
			SendMessage(hwndDlg, WM_SIZE, 0, 0);
			break;

		case IDC_MESSAGE:
			if (HIWORD(wParam) == EN_CHANGE) {
				si->cmdListCurrent = NULL;
				EnableWindow(GetDlgItem(hwndDlg, IDOK), GetRichTextLength(GetDlgItem(hwndDlg, IDC_MESSAGE), 1200, FALSE) != 0);
			}
			break;

		case IDC_HISTORY:
			if (IsWindowEnabled(GetDlgItem(hwndDlg, IDC_HISTORY))) {
				MODULEINFO *pInfo = pci->MM_FindModule(si->pszModule);
				if (pInfo)
					ShellExecute(hwndDlg, NULL, pci->GetChatLogsFilename(si, 0), NULL, NULL, SW_SHOW);
			}
			break;

		case IDC_CHAT_CHANMGR:
			if (!IsWindowEnabled(GetDlgItem(hwndDlg, IDC_CHAT_CHANMGR)))
				break;
			pci->DoEventHookAsync(hwndDlg, si->ptszID, si->pszModule, GC_USER_CHANMGR, NULL, NULL, 0);
			break;

		case IDC_CHAT_FILTER:
			if (!IsWindowEnabled(GetDlgItem(hwndDlg, IDC_CHAT_FILTER)))
				break;

			si->bFilterEnabled = !si->bFilterEnabled;
			SendDlgItemMessage(hwndDlg, IDC_CHAT_FILTER, BM_SETIMAGE, IMAGE_ICON, (LPARAM)GetCachedIcon(si->bFilterEnabled ? "chat_filter" : "chat_filter2"));
			if (si->bFilterEnabled && db_get_b(NULL, CHAT_MODULE, "RightClickFilter", 0) == 0) {
				SendMessage(hwndDlg, GC_SHOWFILTERMENU, 0, 0);
				break;
			}
			SendMessage(hwndDlg, GC_REDRAWLOG, 0, 0);
			break;

		case IDC_CHAT_BKGCOLOR:
			if (IsWindowEnabled(GetDlgItem(hwndDlg, IDC_CHAT_BKGCOLOR))) {
				MODULEINFO *pInfo = pci->MM_FindModule(si->pszModule);
				CHARFORMAT2 cf;
				cf.cbSize = sizeof(CHARFORMAT2);
				cf.dwEffects = 0;

				if (IsDlgButtonChecked(hwndDlg, IDC_CHAT_BKGCOLOR)) {
					if (db_get_b(NULL, CHAT_MODULE, "RightClickFilter", 0) == 0)
						SendMessage(hwndDlg, GC_SHOWCOLORCHOOSER, 0, IDC_CHAT_BKGCOLOR);
					else if (si->bBGSet) {
						cf.dwMask = CFM_BACKCOLOR;
						cf.crBackColor = pInfo->crColors[si->iBG];
						if (pInfo->bSingleFormat)
							SendDlgItemMessage(hwndDlg, IDC_MESSAGE, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);
						else
							SendDlgItemMessage(hwndDlg, IDC_MESSAGE, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
					}
				}
				else {
					cf.dwMask = CFM_BACKCOLOR;
					cf.crBackColor = (COLORREF)db_get_dw(NULL, SRMMMOD, SRMSGSET_INPUTBKGCOLOUR, SRMSGDEFSET_INPUTBKGCOLOUR);
					if (pInfo->bSingleFormat)
						SendDlgItemMessage(hwndDlg, IDC_MESSAGE, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);
					else
						SendDlgItemMessage(hwndDlg, IDC_MESSAGE, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
				}
			}
			break;

		case IDC_CHAT_COLOR:
			{
				MODULEINFO *pInfo = pci->MM_FindModule(si->pszModule);
				CHARFORMAT2 cf;
				cf.cbSize = sizeof(CHARFORMAT2);
				cf.dwEffects = 0;

				if (!IsWindowEnabled(GetDlgItem(hwndDlg, IDC_CHAT_COLOR)))
					break;

				if (IsDlgButtonChecked(hwndDlg, IDC_CHAT_COLOR)) {
					if (db_get_b(NULL, CHAT_MODULE, "RightClickFilter", 0) == 0)
						SendMessage(hwndDlg, GC_SHOWCOLORCHOOSER, 0, IDC_CHAT_COLOR);
					else if (si->bFGSet) {
						cf.dwMask = CFM_COLOR;
						cf.crTextColor = pInfo->crColors[si->iFG];
						if (pInfo->bSingleFormat)
							SendDlgItemMessage(hwndDlg, IDC_MESSAGE, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);
						else
							SendDlgItemMessage(hwndDlg, IDC_MESSAGE, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
					}
				}
				else {
					COLORREF cr;
					LoadMsgDlgFont(MSGFONTID_MESSAGEAREA, NULL, &cr);
					cf.dwMask = CFM_COLOR;
					cf.crTextColor = cr;
					if (pInfo->bSingleFormat)
						SendDlgItemMessage(hwndDlg, IDC_MESSAGE, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);
					else
						SendDlgItemMessage(hwndDlg, IDC_MESSAGE, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
				}
			}
			break;

		case IDC_CHAT_BOLD:
		case IDC_CHAT_ITALICS:
		case IDC_CHAT_UNDERLINE:
			{
				MODULEINFO *pInfo = pci->MM_FindModule(si->pszModule);
				CHARFORMAT2 cf;
				cf.cbSize = sizeof(CHARFORMAT2);
				cf.dwMask = CFM_BOLD | CFM_ITALIC | CFM_UNDERLINE;
				cf.dwEffects = 0;

				if (LOWORD(wParam) == IDC_CHAT_BOLD && !IsWindowEnabled(GetDlgItem(hwndDlg, IDC_CHAT_BOLD)))
					break;
				if (LOWORD(wParam) == IDC_CHAT_ITALICS && !IsWindowEnabled(GetDlgItem(hwndDlg, IDC_CHAT_ITALICS)))
					break;
				if (LOWORD(wParam) == IDC_CHAT_UNDERLINE && !IsWindowEnabled(GetDlgItem(hwndDlg, IDC_CHAT_UNDERLINE)))
					break;
				if (IsDlgButtonChecked(hwndDlg, IDC_CHAT_BOLD))
					cf.dwEffects |= CFE_BOLD;
				if (IsDlgButtonChecked(hwndDlg, IDC_CHAT_ITALICS))
					cf.dwEffects |= CFE_ITALIC;
				if (IsDlgButtonChecked(hwndDlg, IDC_CHAT_UNDERLINE))
					cf.dwEffects |= CFE_UNDERLINE;
				if (pInfo->bSingleFormat)
					SendDlgItemMessage(hwndDlg, IDC_MESSAGE, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);
				else
					SendDlgItemMessage(hwndDlg, IDC_MESSAGE, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
			}
			break;

		case IDCANCEL:
			PostMessage(hwndDlg, WM_CLOSE, 0, 0);
		}
		break;

	case WM_KEYDOWN:
		SetFocus(GetDlgItem(hwndDlg, IDC_MESSAGE));
		break;

	case WM_GETMINMAXINFO:
		{
			MINMAXINFO* mmi = (MINMAXINFO*)lParam;
			mmi->ptMinTrackSize.x = si->iSplitterX + 43;
			if (mmi->ptMinTrackSize.x < 350)
				mmi->ptMinTrackSize.x = 350;

			mmi->ptMinTrackSize.y = si->minLogBoxHeight + TOOLBAR_HEIGHT + si->minEditBoxHeight + 5;
		}
		break;

	case WM_LBUTTONDBLCLK:
		if (LOWORD(lParam) < 30)
			PostMessage(hwndDlg, GC_SCROLLTOBOTTOM, 0, 0);
		else
			SendMessage(GetParent(hwndDlg), WM_SYSCOMMAND, SC_MINIMIZE, 0);
		break;

	case WM_LBUTTONDOWN:
		SendMessage(GetParent(hwndDlg), WM_LBUTTONDOWN, wParam, lParam);
		return TRUE;

	case DM_GETCONTEXTMENU:
		SetWindowLongPtr(hwndDlg, DWLP_MSGRESULT, (LPARAM)Menu_BuildContactMenu(si->hContact));
		return TRUE;

	case WM_CONTEXTMENU:
		if (GetParent(hwndDlg) == (HWND)wParam) {
			HMENU hMenu = Menu_BuildContactMenu(si->hContact);
			GetCursorPos(&pt);
			TrackPopupMenu(hMenu, 0, pt.x, pt.y, 0, hwndDlg, NULL);
			DestroyMenu(hMenu);
		}
		break;

	case WM_CLOSE:
		SendMessage(hwndDlg, GC_CLOSEWINDOW, 0, 0);
		break;

	case GC_CLOSEWINDOW:
		DestroyWindow(hwndDlg);
		break;

	case WM_DESTROY:
		NotifyLocalWinEvent(si->hContact, hwndDlg, MSG_WINDOW_EVT_CLOSING);
		si->hWnd = NULL;
		SetWindowLongPtr(hwndDlg, GWLP_USERDATA, 0);

		SendMessage(GetParent(hwndDlg), CM_REMOVECHILD, 0, (LPARAM)hwndDlg);
		if (si->hwndLog != NULL) {
			IEVIEWWINDOW ieWindow;
			ieWindow.cbSize = sizeof(IEVIEWWINDOW);
			ieWindow.iType = IEW_DESTROY;
			ieWindow.hwnd = si->hwndLog;
			CallService(MS_IEVIEW_WINDOW, 0, (LPARAM)&ieWindow);
		}

		NotifyLocalWinEvent(si->hContact, hwndDlg, MSG_WINDOW_EVT_CLOSE);
		break;
	}
	return FALSE;
}

void ShowRoom(SESSION_INFO *si, WPARAM, BOOL)
{
	if (si == NULL)
		return;

	// Do we need to create a window?
	if (si->hWnd == NULL) {
		HWND hParent = GetParentWindow(si->hContact, TRUE);
		si->parent = (ParentWindowData*)GetWindowLongPtr(hParent, GWLP_USERDATA);
		si->hWnd = CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_CHANNEL), hParent, RoomWndProc, (LPARAM)si);
	}
	SendMessage(si->hWnd, DM_UPDATETABCONTROL, -1, (LPARAM)si);
	SendMessage(GetParent(si->hWnd), CM_ACTIVATECHILD, 0, (LPARAM)si->hWnd);
	SendMessage(GetParent(si->hWnd), CM_POPUPWINDOW, 0, (LPARAM)si->hWnd);
	SendMessage(si->hWnd, WM_MOUSEACTIVATE, 0, 0);
	SetFocus(GetDlgItem(si->hWnd, IDC_MESSAGE));
}
