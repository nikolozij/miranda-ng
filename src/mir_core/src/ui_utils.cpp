/*

Object UI extensions
Copyright (c) 2008  Victor Pavlychko, George Hazan
Copyright (�) 2012-16 Miranda NG project

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

#include "stdafx.h"

#include <m_button.h>
#include <m_gui.h>
#include <m_skin.h>

static mir_cs csDialogs, csCtrl;

static int CompareDialogs(const CDlgBase *p1, const CDlgBase *p2)
{	return (INT_PTR)p1->GetHwnd() - (INT_PTR)p2->GetHwnd();
}
static LIST<CDlgBase> arDialogs(10, CompareDialogs);

static int CompareControls(const CCtrlBase *p1, const CCtrlBase *p2)
{	return (INT_PTR)p1->GetHwnd() - (INT_PTR)p2->GetHwnd();
}
static LIST<CCtrlBase> arControls(10, CompareControls);

#pragma comment(lib, "uxtheme")

/////////////////////////////////////////////////////////////////////////////////////////
// CDlgBase

static int CompareControlId(const CCtrlBase *c1, const CCtrlBase *c2)
{
	return c1->GetCtrlId() - c2->GetCtrlId();
}

static int CompareTimerId(const CTimer *t1, const CTimer *t2)
{
	return t1->GetEventId() - t2->GetEventId();
}

CDlgBase::CDlgBase(HINSTANCE hInst, int idDialog)
	: m_controls(1, CompareControlId),
	m_timers(1, CompareTimerId)
{
	m_hInst = hInst;
	m_idDialog = idDialog;
	m_hwnd = m_hwndParent = NULL;
	m_isModal = m_initialized = m_bExiting = false;
	m_autoClose = CLOSE_ON_OK | CLOSE_ON_CANCEL;
	m_forceResizable = false;
}

CDlgBase::~CDlgBase()
{
	if (m_hwnd)
		DestroyWindow(m_hwnd);
}

void CDlgBase::Create()
{
	ShowWindow(CreateDialogParam(m_hInst, MAKEINTRESOURCE(m_idDialog), m_hwndParent, GlobalDlgProc, (LPARAM)this), SW_HIDE);
}

void CDlgBase::Show(int nCmdShow)
{
	ShowWindow(CreateDialogParam(m_hInst, MAKEINTRESOURCE(m_idDialog), m_hwndParent, GlobalDlgProc, (LPARAM)this), nCmdShow);
}

int CDlgBase::DoModal()
{
	m_isModal = true;
	return DialogBoxParam(m_hInst, MAKEINTRESOURCE(m_idDialog), m_hwndParent, GlobalDlgProc, (LPARAM)this);
}

void CDlgBase::EndModal(INT_PTR nResult)
{
	::EndDialog(m_hwnd, nResult);
}

void CDlgBase::NotifyChange(void)
{
	if (m_hwndParent)
		SendMessage(m_hwndParent, PSM_CHANGED, (WPARAM)m_hwnd, 0);
}

void CDlgBase::SetCaption(const wchar_t *ptszCaption)
{
	if (m_hwnd && ptszCaption)
		SetWindowText(m_hwnd, ptszCaption);
}

int CDlgBase::Resizer(UTILRESIZECONTROL*)
{
	return RD_ANCHORX_LEFT | RD_ANCHORY_TOP;
}

INT_PTR CDlgBase::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
	case WM_INITDIALOG:
		m_initialized = false;
		TranslateDialogDefault(m_hwnd);

		NotifyControls(&CCtrlBase::OnInit);
		OnInitDialog();

		m_initialized = true;
		return TRUE;

	case WM_CTLCOLOREDIT:
	case WM_CTLCOLORSTATIC:
		if (CCtrlBase *ctrl = FindControl(HWND(lParam))) {
			if (ctrl->m_bUseSystemColors) {
				SetBkColor((HDC)wParam, GetSysColor(COLOR_WINDOW));
				return (INT_PTR)GetSysColorBrush(COLOR_WINDOW);
			}
		}
		break;

	case WM_MEASUREITEM:
		{
			MEASUREITEMSTRUCT *param = (MEASUREITEMSTRUCT *)lParam;
			if (param && param->CtlID)
				if (CCtrlBase *ctrl = FindControl(param->CtlID))
					return ctrl->OnMeasureItem(param);
		}
		return FALSE;

	case WM_DRAWITEM:
		{
			DRAWITEMSTRUCT *param = (DRAWITEMSTRUCT *)lParam;
			if (param && param->CtlID)
				if (CCtrlBase *ctrl = FindControl(param->CtlID))
					return ctrl->OnDrawItem(param);
		}
		return FALSE;

	case WM_DELETEITEM:
		{
			DELETEITEMSTRUCT *param = (DELETEITEMSTRUCT *)lParam;
			if (param && param->CtlID)
				if (CCtrlBase *ctrl = FindControl(param->CtlID))
					return ctrl->OnDeleteItem(param);
		}
		return FALSE;

	case WM_COMMAND:
		{
			HWND hwndCtrl = (HWND)lParam;
			WORD idCtrl = LOWORD(wParam);
			WORD idCode = HIWORD(wParam);
			if (CCtrlBase *ctrl = FindControl(idCtrl)) {
				BOOL result = ctrl->OnCommand(hwndCtrl, idCtrl, idCode);
				if (result != FALSE)
					return result;
			}

			if (idCode == BN_CLICKED) {
				// close dialog automatically if 'Cancel' button is pressed
				if (idCtrl == IDCANCEL && (m_autoClose & CLOSE_ON_CANCEL)) {
					m_bExiting = true;
					PostMessage(m_hwnd, WM_CLOSE, 0, 0);
				}

				// close dialog automatically if 'OK' button is pressed
				if (idCtrl == IDOK && (m_autoClose & CLOSE_ON_OK)) {
					// validate dialog data first
					m_bExiting = true;
					m_lresult = TRUE;
					NotifyControls(&CCtrlBase::OnApply);
					OnApply();

					// everything ok? good, let's close it
					if (m_lresult == TRUE)
						PostMessage(m_hwnd, WM_CLOSE, 0, 0);
					else
						m_bExiting = false;
				}
			}
		}
		return FALSE;

	case WM_NOTIFY:
		{
			int idCtrl = wParam;
			NMHDR *pnmh = (NMHDR *)lParam;
			if (pnmh->idFrom == 0) {
				if (pnmh->code == PSN_APPLY) {
					if (LPPSHNOTIFY(lParam)->lParam != 3) // IDC_APPLY
						m_bExiting = true;

					m_lresult = true;
					NotifyControls(&CCtrlBase::OnApply);
					if (m_lresult)
						OnApply();
				}
				else if (pnmh->code == PSN_RESET) {
					NotifyControls(&CCtrlBase::OnReset);
					OnReset();
				}
			}

			if (CCtrlBase *ctrl = FindControl(pnmh->idFrom))
				return ctrl->OnNotify(idCtrl, pnmh);
		}
		return FALSE;

	case WM_CONTEXTMENU:
		if (CCtrlBase *ctrl = FindControl(HWND(wParam)))
			ctrl->OnBuildMenu(ctrl);
		break;

	case WM_SIZE:
		if (m_forceResizable || (GetWindowLongPtr(m_hwnd, GWL_STYLE) & WS_THICKFRAME))
			Utils_ResizeDialog(m_hwnd, m_hInst, MAKEINTRESOURCEA(m_idDialog), GlobalDlgResizer);
		return TRUE;

	case WM_TIMER:
		if (CTimer *timer = FindTimer(wParam))
			return timer->OnTimer();
		return FALSE;

	case WM_CLOSE:
		m_bExiting = true;
		m_lresult = FALSE;
		OnClose();
		if (!m_lresult) {
			if (m_isModal)
				EndModal(0);
			else
				DestroyWindow(m_hwnd);
		}
		return TRUE;

	case WM_DESTROY:
		m_bExiting = true;
		OnDestroy();
		NotifyControls(&CCtrlBase::OnDestroy);
		{
			mir_cslock lck(csDialogs);
			int idx = arDialogs.getIndex(this);
			if (idx != -1)
				arDialogs.remove(idx);
		}
		m_hwnd = NULL;
		if (m_isModal)
			m_isModal = false;
		else // modeless dialogs MUST be allocated with 'new'
			delete this;

		return TRUE;
	}

	return FALSE;
}

INT_PTR CALLBACK CDlgBase::GlobalDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	CDlgBase *wnd;
	if (msg == WM_INITDIALOG) {
		wnd = (CDlgBase*)lParam;
		wnd->m_hwnd = hwnd;

		mir_cslock lck(csDialogs);
		arDialogs.insert(wnd);
	}
	else wnd = CDlgBase::Find(hwnd);

	return (wnd == NULL) ? FALSE : wnd->DlgProc(msg, wParam, lParam);
}

int CDlgBase::GlobalDlgResizer(HWND hwnd, LPARAM, UTILRESIZECONTROL *urc)
{
	CDlgBase *wnd = CDlgBase::Find(hwnd);
	return (wnd == NULL) ? 0 : wnd->Resizer(urc);
}

void CDlgBase::ThemeDialogBackground(BOOL tabbed)
{
	EnableThemeDialogTexture(m_hwnd, (tabbed ? ETDT_ENABLE : ETDT_DISABLE) | ETDT_USETABTEXTURE);
}

void CDlgBase::AddControl(CCtrlBase *ctrl)
{
	m_controls.insert(ctrl);
}

void CDlgBase::NotifyControls(void (CCtrlBase::*fn)())
{
	for (int i = 0; i < m_controls.getCount(); i++)
		(m_controls[i]->*fn)();
}

CCtrlBase* CDlgBase::FindControl(int idCtrl)
{
	CCtrlBase search(NULL, idCtrl);
	return m_controls.find(&search);
}

CCtrlBase* CDlgBase::FindControl(HWND hwnd)
{
	for (int i = 0; i < m_controls.getCount(); i++)
		if (m_controls[i]->GetHwnd() == hwnd)
			return m_controls[i];
	
	return NULL;
}

void CDlgBase::AddTimer(CTimer *timer)
{
	m_timers.insert(timer);
}

CTimer* CDlgBase::FindTimer(int idEvent)
{
	CTimer search(NULL, idEvent);
	return m_timers.find(&search);
}

CDlgBase* CDlgBase::Find(HWND hwnd)
{
	PVOID bullshit[2]; // vfptr + hwnd
	bullshit[1] = hwnd;
	return arDialogs.find((CDlgBase*)&bullshit);
}

/////////////////////////////////////////////////////////////////////////////////////////
// CCtrlCombo class

CCtrlCombo::CCtrlCombo(CDlgBase *dlg, int ctrlId)
	: CCtrlData(dlg, ctrlId)
{}

BOOL CCtrlCombo::OnCommand(HWND, WORD, WORD idCode)
{
	switch (idCode) {
	case CBN_CLOSEUP:  OnCloseup(this);  break;
	case CBN_DROPDOWN: OnDropdown(this); break;

	case CBN_EDITCHANGE:
	case CBN_EDITUPDATE:
	case CBN_SELCHANGE:
	case CBN_SELENDOK:
		NotifyChange();
		break;
	}
	return TRUE;
}

void CCtrlCombo::OnInit()
{
	CSuper::OnInit();
	OnReset();
}

void CCtrlCombo::OnApply()
{
	CSuper::OnApply();

	if (GetDataType() == DBVT_WCHAR) {
		int len = GetWindowTextLength(m_hwnd) + 1;
		wchar_t *buf = (wchar_t *)_alloca(sizeof(wchar_t) * len);
		GetWindowText(m_hwnd, buf, len);
		SaveText(buf);
	}
	else if (GetDataType() != DBVT_DELETED) {
		SaveInt(GetInt());
	}
}

void CCtrlCombo::OnReset()
{
	if (GetDataType() == DBVT_WCHAR)
		SetText(LoadText());
	else if (GetDataType() != DBVT_DELETED)
		SetInt(LoadInt());
}

int CCtrlCombo::AddString(const wchar_t *text, LPARAM data)
{
	int iItem = SendMessage(m_hwnd, CB_ADDSTRING, 0, (LPARAM)text);
	if (data)
		SendMessage(m_hwnd, CB_SETITEMDATA, iItem, data);
	return iItem;
}

int CCtrlCombo::AddStringA(const char *text, LPARAM data)
{
	int iItem = SendMessageA(m_hwnd, CB_ADDSTRING, 0, (LPARAM)text);
	if (data)
		SendMessage(m_hwnd, CB_SETITEMDATA, iItem, data);
	return iItem;
}

void CCtrlCombo::DeleteString(int index)
{	SendMessage(m_hwnd, CB_DELETESTRING, index, 0);
}

int CCtrlCombo::FindString(const wchar_t *str, int index, bool exact)
{	return SendMessage(m_hwnd, exact?CB_FINDSTRINGEXACT:CB_FINDSTRING, index, (LPARAM)str);
}

int CCtrlCombo::FindStringA(const char *str, int index, bool exact)
{	return SendMessageA(m_hwnd, exact?CB_FINDSTRINGEXACT:CB_FINDSTRING, index, (LPARAM)str);
}

int CCtrlCombo::GetCount()
{	return SendMessage(m_hwnd, CB_GETCOUNT, 0, 0);
}

int CCtrlCombo::GetCurSel()
{	return SendMessage(m_hwnd, CB_GETCURSEL, 0, 0);
}

bool CCtrlCombo::GetDroppedState()
{	return SendMessage(m_hwnd, CB_GETDROPPEDSTATE, 0, 0) ? true : false;
}

LPARAM CCtrlCombo::GetItemData(int index)
{	return SendMessage(m_hwnd, CB_GETITEMDATA, index, 0);
}

wchar_t* CCtrlCombo::GetItemText(int index)
{
	wchar_t *result = (wchar_t *)mir_alloc(sizeof(wchar_t) * (SendMessage(m_hwnd, CB_GETLBTEXTLEN, index, 0) + 1));
	SendMessage(m_hwnd, CB_GETLBTEXT, index, (LPARAM)result);
	return result;
}

wchar_t* CCtrlCombo::GetItemText(int index, wchar_t *buf, int size)
{
	wchar_t *result = (wchar_t *)_alloca(sizeof(wchar_t) * (SendMessage(m_hwnd, CB_GETLBTEXTLEN, index, 0) + 1));
	SendMessage(m_hwnd, CB_GETLBTEXT, index, (LPARAM)result);
	mir_wstrncpy(buf, result, size);
	return buf;
}

int CCtrlCombo::InsertString(wchar_t *text, int pos, LPARAM data)
{
	int iItem = SendMessage(m_hwnd, CB_INSERTSTRING, pos, (LPARAM)text);
	SendMessage(m_hwnd, CB_SETITEMDATA, iItem, data);
	return iItem;
}

void CCtrlCombo::ResetContent()
{	SendMessage(m_hwnd, CB_RESETCONTENT, 0, 0);
}

int CCtrlCombo::SelectString(wchar_t *str)
{	return SendMessage(m_hwnd, CB_SELECTSTRING, 0, (LPARAM)str);
}

int CCtrlCombo::SetCurSel(int index)
{	return SendMessage(m_hwnd, CB_SETCURSEL, index, 0);
}

void CCtrlCombo::SetItemData(int index, LPARAM data)
{	SendMessage(m_hwnd, CB_SETITEMDATA, index, data);
}

void CCtrlCombo::ShowDropdown(bool show)
{	SendMessage(m_hwnd, CB_SHOWDROPDOWN, show ? TRUE : FALSE, 0);
}

/////////////////////////////////////////////////////////////////////////////////////////
// CCtrlListBox class

CCtrlListBox::CCtrlListBox(CDlgBase *dlg, int ctrlId)
	: CCtrlBase(dlg, ctrlId)
{}

BOOL CCtrlListBox::OnCommand(HWND, WORD, WORD idCode)
{
	switch (idCode) {
		case LBN_DBLCLK:    OnDblClick(this); break;
		case LBN_SELCANCEL: OnSelCancel(this); break;
		case LBN_SELCHANGE: OnSelChange(this); break;
	}
	return TRUE;
}

int CCtrlListBox::AddString(wchar_t *text, LPARAM data)
{
	int iItem = ListBox_AddString(m_hwnd, text);
	ListBox_SetItemData(m_hwnd, iItem, data);
	return iItem;
}

void CCtrlListBox::DeleteString(int index)
{	ListBox_DeleteString(m_hwnd, index);
}

int CCtrlListBox::FindString(wchar_t *str, int index, bool exact)
{	return SendMessage(m_hwnd, exact?LB_FINDSTRINGEXACT:LB_FINDSTRING, index, (LPARAM)str);
}

int CCtrlListBox::GetCount()
{	return ListBox_GetCount(m_hwnd);
}

int CCtrlListBox::GetCurSel()
{	return ListBox_GetCurSel(m_hwnd);
}

LPARAM CCtrlListBox::GetItemData(int index)
{	return ListBox_GetItemData(m_hwnd, index);
}

int CCtrlListBox::GetItemRect(int index, RECT *pResult)
{	return ListBox_GetItemRect(m_hwnd, index, pResult);
}

wchar_t* CCtrlListBox::GetItemText(int index)
{
	wchar_t *result = (wchar_t *)mir_alloc(sizeof(wchar_t) * (SendMessage(m_hwnd, LB_GETTEXTLEN, index, 0) + 1));
	SendMessage(m_hwnd, LB_GETTEXT, index, (LPARAM)result);
	return result;
}

wchar_t* CCtrlListBox::GetItemText(int index, wchar_t *buf, int size)
{
	wchar_t *result = (wchar_t *)_alloca(sizeof(wchar_t) * (SendMessage(m_hwnd, LB_GETTEXTLEN, index, 0) + 1));
	SendMessage(m_hwnd, LB_GETTEXT, index, (LPARAM)result);
	mir_wstrncpy(buf, result, size);
	return buf;
}

bool CCtrlListBox::GetSel(int index)
{	return ListBox_GetSel(m_hwnd, index) ? true : false;
}

int CCtrlListBox::GetSelCount()
{	return ListBox_GetSelCount(m_hwnd);
}

int* CCtrlListBox::GetSelItems(int *items, int count)
{
	ListBox_GetSelItems(m_hwnd, count, items);
	return items;
}

int* CCtrlListBox::GetSelItems()
{
	int count = GetSelCount() + 1;
	int *result = (int *)mir_alloc(sizeof(int) * count);
	ListBox_GetSelItems(m_hwnd, count, result);
	result[count-1] = -1;
	return result;
}

int CCtrlListBox::InsertString(wchar_t *text, int pos, LPARAM data)
{
	int iItem = ListBox_InsertString(m_hwnd, pos, text);
	ListBox_SetItemData(m_hwnd, iItem, data);
	return iItem;
}

void CCtrlListBox::ResetContent()
{	ListBox_ResetContent(m_hwnd);
}

int CCtrlListBox::SelectString(wchar_t *str)
{	return ListBox_SelectString(m_hwnd, 0, str);
}

int CCtrlListBox::SetCurSel(int index)
{	return ListBox_SetCurSel(m_hwnd, index);
}

void CCtrlListBox::SetItemData(int index, LPARAM data)
{	ListBox_SetItemData(m_hwnd, index, data);
}

void CCtrlListBox::SetItemHeight(int index, int iHeight)
{	ListBox_SetItemHeight(m_hwnd, index, iHeight);
}

void CCtrlListBox::SetSel(int index, bool sel)
{	ListBox_SetSel(m_hwnd, sel ? TRUE : FALSE, index);
}

/////////////////////////////////////////////////////////////////////////////////////////
// CCtrlCheck class

CCtrlCheck::CCtrlCheck(CDlgBase *dlg, int ctrlId)
	: CCtrlData(dlg, ctrlId)
{}

BOOL CCtrlCheck::OnCommand(HWND, WORD, WORD)
{
	NotifyChange();
	return TRUE;
}

void CCtrlCheck::OnApply()
{
	CSuper::OnApply();

	SaveInt(GetState());
}

void CCtrlCheck::OnReset()
{
	SetState(LoadInt());
}

int CCtrlCheck::GetState()
{	return SendMessage(m_hwnd, BM_GETCHECK, 0, 0);
}

void CCtrlCheck::SetState(int state)
{	SendMessage(m_hwnd, BM_SETCHECK, state, 0);
}

/////////////////////////////////////////////////////////////////////////////////////////
// CCtrlEdit class

CCtrlEdit::CCtrlEdit(CDlgBase *dlg, int ctrlId)
	: CCtrlData(dlg, ctrlId)
{}

BOOL CCtrlEdit::OnCommand(HWND, WORD, WORD idCode)
{
	if (idCode == EN_CHANGE)
		NotifyChange();
	return TRUE;
}

void CCtrlEdit::OnApply()
{
	CSuper::OnApply();

	if (GetDataType() == DBVT_WCHAR) {
		int len = GetWindowTextLength(m_hwnd) + 1;
		wchar_t *buf = (wchar_t *)_alloca(sizeof(wchar_t) * len);
		GetWindowText(m_hwnd, buf, len);
		SaveText(buf);
	}
	else if (GetDataType() != DBVT_DELETED) {
		SaveInt(GetInt());
	}
}

void CCtrlEdit::OnReset()
{
	if (GetDataType() == DBVT_WCHAR)
		SetText(LoadText());
	else if (GetDataType() != DBVT_DELETED)
		SetInt(LoadInt());
}

void CCtrlEdit::SetMaxLength(unsigned int len)
{
	SendMsg(EM_SETLIMITTEXT, len, 0);
}

/////////////////////////////////////////////////////////////////////////////////////////
// CCtrlSpin class

CCtrlSpin::CCtrlSpin(CDlgBase *dlg, int ctrlId)
	: CCtrlBase(dlg, ctrlId)
{}

BOOL CCtrlSpin::OnNotify(int, NMHDR *pnmh)
{
	if (pnmh->code == UDN_DELTAPOS) {
		NotifyChange();
		return TRUE;
	}

	return FALSE;
}

WORD CCtrlSpin::GetPosition()
{
	return SendMsg(UDM_GETPOS, 0, 0);
}

void CCtrlSpin::SetPosition(WORD wPos)
{
	SendMsg(UDM_SETPOS, 0, wPos);
}

void CCtrlSpin::SetRange(WORD wMax, WORD wMin)
{
	SendMsg(UDM_SETRANGE, 0, MAKELPARAM(wMax, wMin));
}

/////////////////////////////////////////////////////////////////////////////////////////
// CCtrlData class

CCtrlData::CCtrlData(CDlgBase *wnd, int idCtrl)
	: CCtrlBase(wnd, idCtrl),
	m_dbLink(NULL)
{}

CCtrlData::~CCtrlData()
{
	if (m_dbLink)
		delete m_dbLink;
}

void CCtrlData::OnInit()
{
	CCtrlBase::OnInit();
	OnReset();
}

void CCtrlData::CreateDbLink(const char* szModuleName, const char* szSetting, BYTE type, DWORD iValue)
{
	m_dbLink = new CDbLink(szModuleName, szSetting, type, iValue);
}

void CCtrlData::CreateDbLink(const char* szModuleName, const char* szSetting, wchar_t* szValue)
{
	m_dbLink = new CDbLink(szModuleName, szSetting, DBVT_WCHAR, szValue);
}

/////////////////////////////////////////////////////////////////////////////////////////
// CCtrlMButton

CCtrlMButton::CCtrlMButton(CDlgBase *dlg, int ctrlId, HICON hIcon, const char* tooltip) 
	: CCtrlButton(dlg, ctrlId),
	m_hIcon(hIcon),
	m_toolTip(tooltip)
{}

CCtrlMButton::CCtrlMButton(CDlgBase *dlg, int ctrlId, int iCoreIcon, const char* tooltip)
	: CCtrlButton(dlg, ctrlId),
	m_hIcon(::Skin_LoadIcon(iCoreIcon)),
	m_toolTip(tooltip)
{}

CCtrlMButton::~CCtrlMButton()
{
	::IcoLib_ReleaseIcon(m_hIcon);
}

void CCtrlMButton::OnInit()
{
	CCtrlButton::OnInit();

	SendMessage(m_hwnd, BM_SETIMAGE, IMAGE_ICON, (LPARAM)m_hIcon);
	SendMessage(m_hwnd, BUTTONADDTOOLTIP, (WPARAM)m_toolTip, 0);
	SendMessage(m_hwnd, BUTTONSETASFLATBTN, (WPARAM)m_toolTip, 0);
}

void CCtrlMButton::MakeFlat()
{
	SendMessage(m_hwnd, BUTTONSETASFLATBTN, TRUE, 0);
}

void CCtrlMButton::MakePush()
{
	SendMessage(m_hwnd, BUTTONSETASPUSHBTN, TRUE, 0);
}

/////////////////////////////////////////////////////////////////////////////////////////
// CCtrlButton

CCtrlButton::CCtrlButton(CDlgBase* wnd, int idCtrl)
	: CCtrlBase(wnd, idCtrl)
{}

BOOL CCtrlButton::OnCommand(HWND, WORD, WORD idCode)
{
	if (idCode == BN_CLICKED || idCode == STN_CLICKED)
		OnClick(this);
	return FALSE;
}

/////////////////////////////////////////////////////////////////////////////////////////
// CCtrlHyperlink

CCtrlHyperlink::CCtrlHyperlink(CDlgBase* wnd, int idCtrl, const char* url)
	: CCtrlBase(wnd, idCtrl),
	m_url(url)
{}

BOOL CCtrlHyperlink::OnCommand(HWND, WORD, WORD)
{
	ShellExecuteA(m_hwnd, "open", m_url, "", "", SW_SHOW);
	return FALSE;
}

/////////////////////////////////////////////////////////////////////////////////////////
// CTimer

CTimer::CTimer(CDlgBase *wnd, int idEvent)
	: m_wnd(wnd), m_idEvent(idEvent)
{
	if (wnd)
		wnd->AddTimer(this);
}

BOOL CTimer::OnTimer()
{
	OnEvent(this);
	return FALSE;
}

void CTimer::Start(int elapse)
{
	SetTimer(m_wnd->GetHwnd(), m_idEvent, elapse, NULL);
}

void CTimer::Stop()
{
	KillTimer(m_wnd->GetHwnd(), m_idEvent);
}

/////////////////////////////////////////////////////////////////////////////////////////
// CProgress

CProgress::CProgress(CDlgBase *wnd, int idCtrl)
	: CCtrlBase(wnd, idCtrl)
{
}

void CProgress::SetRange(WORD max, WORD min)
{
	SendMsg(PBM_SETRANGE, 0, MAKELPARAM(min, max));
}

void CProgress::SetPosition(WORD value)
{
	SendMsg(PBM_SETPOS, value, 0);
}

void CProgress::SetStep(WORD value)
{
	SendMsg(PBM_SETSTEP, value, 0);
}

WORD CProgress::Move(WORD delta)
{
	return delta == 0
		? SendMsg(PBM_STEPIT, 0, 0)
		: SendMsg(PBM_DELTAPOS, delta, 0);
}

/////////////////////////////////////////////////////////////////////////////////////////
// CCtrlClc
CCtrlClc::CCtrlClc(CDlgBase *dlg, int ctrlId)
	: CCtrlBase(dlg, ctrlId)
{}

BOOL CCtrlClc::OnNotify(int, NMHDR *pnmh)
{
	TEventInfo evt = { this, (NMCLISTCONTROL *)pnmh };
	switch (pnmh->code) {
		case CLN_EXPANDED:       OnExpanded(&evt); break;
		case CLN_LISTREBUILT:    OnListRebuilt(&evt); break;
		case CLN_ITEMCHECKED:    OnItemChecked(&evt); break;
		case CLN_DRAGGING:       OnDragging(&evt); break;
		case CLN_DROPPED:        OnDropped(&evt); break;
		case CLN_LISTSIZECHANGE: OnListSizeChange(&evt); break;
		case CLN_OPTIONSCHANGED: OnOptionsChanged(&evt); break;
		case CLN_DRAGSTOP:       OnDragStop(&evt); break;
		case CLN_NEWCONTACT:     OnNewContact(&evt); break;
		case CLN_CONTACTMOVED:   OnContactMoved(&evt); break;
		case CLN_CHECKCHANGED:   OnCheckChanged(&evt); break;
		case NM_CLICK:           OnClick(&evt); break;
	}
	return FALSE;
}

void CCtrlClc::AddContact(MCONTACT hContact)
{	SendMessage(m_hwnd, CLM_ADDCONTACT, hContact, 0);
}

void CCtrlClc::AddGroup(HANDLE hGroup)
{	SendMessage(m_hwnd, CLM_ADDGROUP, (WPARAM)hGroup, 0);
}

void CCtrlClc::AutoRebuild()
{	SendMessage(m_hwnd, CLM_AUTOREBUILD, 0, 0);
}

void CCtrlClc::DeleteItem(HANDLE hItem)
{	SendMessage(m_hwnd, CLM_DELETEITEM, (WPARAM)hItem, 0);
}

void CCtrlClc::EditLabel(HANDLE hItem)
{	SendMessage(m_hwnd, CLM_EDITLABEL, (WPARAM)hItem, 0);
}

void CCtrlClc::EndEditLabel(bool save)
{	SendMessage(m_hwnd, CLM_ENDEDITLABELNOW, save ? 0 : 1, 0);
}

void CCtrlClc::EnsureVisible(HANDLE hItem, bool partialOk)
{	SendMessage(m_hwnd, CLM_ENSUREVISIBLE, (WPARAM)hItem, partialOk ? TRUE : FALSE);
}

void CCtrlClc::Expand(HANDLE hItem, DWORD flags)
{	SendMessage(m_hwnd, CLM_EXPAND, (WPARAM)hItem, flags);
}

HANDLE CCtrlClc::FindContact(MCONTACT hContact)
{	return (HANDLE)SendMessage(m_hwnd, CLM_FINDCONTACT, hContact, 0);
}

HANDLE CCtrlClc::FindGroup(MGROUP hGroup)
{	return (HANDLE)SendMessage(m_hwnd, CLM_FINDGROUP, hGroup, 0);
}

COLORREF CCtrlClc::GetBkColor()
{	return (COLORREF)SendMessage(m_hwnd, CLM_GETBKCOLOR, 0, 0);
}

bool CCtrlClc::GetCheck(HANDLE hItem)
{	return SendMessage(m_hwnd, CLM_GETCHECKMARK, (WPARAM)hItem, 0) ? true : false;
}

int CCtrlClc::GetCount()
{	return SendMessage(m_hwnd, CLM_GETCOUNT, 0, 0);
}

HWND CCtrlClc::GetEditControl()
{	return (HWND)SendMessage(m_hwnd, CLM_GETEDITCONTROL, 0, 0);
}

DWORD CCtrlClc::GetExpand(HANDLE hItem)
{	return SendMessage(m_hwnd, CLM_GETEXPAND, (WPARAM)hItem, 0);
}

int CCtrlClc::GetExtraColumns()
{	return SendMessage(m_hwnd, CLM_GETEXTRACOLUMNS, 0, 0);
}

BYTE CCtrlClc::GetExtraImage(HANDLE hItem, int iColumn)
{
	return (BYTE)(SendMessage(m_hwnd, CLM_GETEXTRAIMAGE, (WPARAM)hItem, MAKELPARAM(iColumn, 0)) & 0xFFFF);
}

HIMAGELIST CCtrlClc::GetExtraImageList()
{	return (HIMAGELIST)SendMessage(m_hwnd, CLM_GETEXTRAIMAGELIST, 0, 0);
}

HFONT CCtrlClc::GetFont(int iFontId)
{	return (HFONT)SendMessage(m_hwnd, CLM_GETFONT, (WPARAM)iFontId, 0);
}

HANDLE CCtrlClc::GetSelection()
{	return (HANDLE)SendMessage(m_hwnd, CLM_GETSELECTION, 0, 0);
}

HANDLE CCtrlClc::HitTest(int x, int y, DWORD *hitTest)
{	return (HANDLE)SendMessage(m_hwnd, CLM_HITTEST, (WPARAM)hitTest, MAKELPARAM(x,y));
}

void CCtrlClc::SelectItem(HANDLE hItem)
{	SendMessage(m_hwnd, CLM_SELECTITEM, (WPARAM)hItem, 0);
}

void CCtrlClc::SetBkBitmap(DWORD mode, HBITMAP hBitmap)
{	SendMessage(m_hwnd, CLM_SETBKBITMAP, mode, (LPARAM)hBitmap);
}

void CCtrlClc::SetBkColor(COLORREF clBack)
{	SendMessage(m_hwnd, CLM_SETBKCOLOR, (WPARAM)clBack, 0);
}

void CCtrlClc::SetCheck(HANDLE hItem, bool check)
{	SendMessage(m_hwnd, CLM_SETCHECKMARK, (WPARAM)hItem, check ? 1 : 0);
}

void CCtrlClc::SetExtraColumns(int iColumns)
{	SendMessage(m_hwnd, CLM_SETEXTRACOLUMNS, (WPARAM)iColumns, 0);
}

void CCtrlClc::SetExtraImage(HANDLE hItem, int iColumn, int iImage)
{	SendMessage(m_hwnd, CLM_SETEXTRAIMAGE, (WPARAM)hItem, MAKELPARAM(iColumn, iImage));
}

void CCtrlClc::SetExtraImageList(HIMAGELIST hImgList)
{	SendMessage(m_hwnd, CLM_SETEXTRAIMAGELIST, 0, (LPARAM)hImgList);
}

void CCtrlClc::SetFont(int iFontId, HANDLE hFont, bool bRedraw)
{	SendMessage(m_hwnd, CLM_SETFONT, (WPARAM)hFont, MAKELPARAM(bRedraw ? 1 : 0, iFontId));
}

void CCtrlClc::SetIndent(int iIndent)
{	SendMessage(m_hwnd, CLM_SETINDENT, (WPARAM)iIndent, 0);
}

void CCtrlClc::SetItemText(HANDLE hItem, char *szText)
{	SendMessage(m_hwnd, CLM_SETITEMTEXT, (WPARAM)hItem, (LPARAM)szText);
}

void CCtrlClc::SetHideEmptyGroups(bool state)
{	SendMessage(m_hwnd, CLM_SETHIDEEMPTYGROUPS, state ? 1 : 0, 0);
}

void CCtrlClc::SetGreyoutFlags(DWORD flags)
{	SendMessage(m_hwnd, CLM_SETGREYOUTFLAGS, (WPARAM)flags, 0);
}

bool CCtrlClc::GetHideOfflineRoot()
{	return SendMessage(m_hwnd, CLM_GETHIDEOFFLINEROOT, 0, 0) ? true : false;
}

void CCtrlClc::SetHideOfflineRoot(bool state)
{	SendMessage(m_hwnd, CLM_SETHIDEOFFLINEROOT, state ? 1 : 0, 9);
}

void CCtrlClc::SetUseGroups(bool state)
{	SendMessage(m_hwnd, CLM_SETUSEGROUPS, state ? 1 : 0, 0);
}

void CCtrlClc::SetOfflineModes(DWORD modes)
{	SendMessage(m_hwnd, CLM_SETOFFLINEMODES, modes, 0);
}

DWORD CCtrlClc::GetExStyle()
{	return SendMessage(m_hwnd, CLM_GETEXSTYLE, 0, 0);
}

void CCtrlClc::SetExStyle(DWORD exStyle)
{	SendMessage(m_hwnd, CLM_SETEXSTYLE, (WPARAM)exStyle, 0);
}

int CCtrlClc::GetLefrMargin()
{	return SendMessage(m_hwnd, CLM_GETLEFTMARGIN, 0, 0);
}

void CCtrlClc::SetLeftMargin(int iMargin)
{	SendMessage(m_hwnd, CLM_SETLEFTMARGIN, (WPARAM)iMargin, 0);
}

HANDLE CCtrlClc::AddInfoItem(CLCINFOITEM *cii)
{	return (HANDLE)SendMessage(m_hwnd, CLM_ADDINFOITEM, 0, (LPARAM)cii);
}

int CCtrlClc::GetItemType(HANDLE hItem)
{	return SendMessage(m_hwnd, CLM_GETITEMTYPE, (WPARAM)hItem, 0);
}

HANDLE CCtrlClc::GetNextItem(HANDLE hItem, DWORD flags)
{	return (HANDLE)SendMessage(m_hwnd, CLM_GETNEXTITEM, (WPARAM)flags, (LPARAM)hItem);
}

COLORREF CCtrlClc::GetTextColor(int iFontId)
{	return (COLORREF)SendMessage(m_hwnd, CLM_GETTEXTCOLOR, (WPARAM)iFontId, 0);
}

void CCtrlClc::SetTextColor(int iFontId, COLORREF clText)
{	SendMessage(m_hwnd, CLM_SETTEXTCOLOR, (WPARAM)iFontId, (LPARAM)clText);
}

/////////////////////////////////////////////////////////////////////////////////////////
// CCtrlListView

CCtrlListView::CCtrlListView(CDlgBase *dlg, int ctrlId)
	: CCtrlBase(dlg, ctrlId)
{}

BOOL CCtrlListView::OnNotify(int, NMHDR *pnmh)
{
	TEventInfo evt = { this, pnmh };

	switch (pnmh->code) {
		case NM_DBLCLK:             OnDoubleClick(&evt);       return TRUE;
		case LVN_BEGINDRAG:         OnBeginDrag(&evt);         return TRUE;
		case LVN_BEGINLABELEDIT:    OnBeginLabelEdit(&evt);    return TRUE;
		case LVN_BEGINRDRAG:        OnBeginRDrag(&evt);        return TRUE;
		case LVN_BEGINSCROLL:       OnBeginScroll(&evt);       return TRUE;
		case LVN_COLUMNCLICK:       OnColumnClick(&evt);       return TRUE;
		case LVN_DELETEALLITEMS:    OnDeleteAllItems(&evt);    return TRUE;
		case LVN_DELETEITEM:        OnDeleteItem(&evt);        return TRUE;
		case LVN_ENDLABELEDIT:      OnEndLabelEdit(&evt);      return TRUE;
		case LVN_ENDSCROLL:         OnEndScroll(&evt);         return TRUE;
		case LVN_GETDISPINFO:       OnGetDispInfo(&evt);       return TRUE;
		case LVN_GETINFOTIP:        OnGetInfoTip(&evt);        return TRUE;
		case LVN_HOTTRACK:          OnHotTrack(&evt);          return TRUE;
		case LVN_INSERTITEM:        OnInsertItem(&evt);        return TRUE;
		case LVN_ITEMACTIVATE:      OnItemActivate(&evt);      return TRUE;
		case LVN_ITEMCHANGED:       OnItemChanged(&evt);       return TRUE;
		case LVN_ITEMCHANGING:      OnItemChanging(&evt);      return TRUE;
		case LVN_KEYDOWN:           OnKeyDown(&evt);           return TRUE;
		case LVN_MARQUEEBEGIN:      OnMarqueeBegin(&evt);      return TRUE;
		case LVN_SETDISPINFO:       OnSetDispInfo(&evt);       return TRUE;
	}

	return FALSE;
}

// additional api
HIMAGELIST CCtrlListView::CreateImageList(int iImageList)
{
	HIMAGELIST hIml = GetImageList(iImageList);
	if (hIml)
		return hIml;

	hIml = ImageList_Create(16, 16, ILC_COLOR32 | ILC_MASK, 0, 1);
	SetImageList(hIml, iImageList);
	return hIml;
}

void CCtrlListView::AddColumn(int iSubItem, wchar_t *name, int cx)
{
	LVCOLUMN lvc;
	lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
	lvc.iImage = 0;
	lvc.pszText = name;
	lvc.cx = cx;
	lvc.iSubItem = iSubItem;
	InsertColumn(iSubItem, &lvc);
}

void CCtrlListView::AddGroup(int iGroupId, wchar_t *name)
{
	LVGROUP lvg = { 0 };
	lvg.cbSize = sizeof(lvg);
	lvg.mask = LVGF_HEADER | LVGF_GROUPID;
	lvg.pszHeader = name;
	lvg.cchHeader = (int)mir_wstrlen(lvg.pszHeader);
	lvg.iGroupId = iGroupId;
	InsertGroup(-1, &lvg);
}

int CCtrlListView::AddItem(wchar_t *text, int iIcon, LPARAM lParam, int iGroupId)
{
	LVITEM lvi = { 0 };
	lvi.mask = LVIF_PARAM | LVIF_TEXT | LVIF_IMAGE;
	lvi.iSubItem = 0;
	lvi.pszText = text;
	lvi.iImage = iIcon;
	lvi.lParam = lParam;
	if (iGroupId >= 0) {
		lvi.mask |= LVIF_GROUPID;
		lvi.iGroupId = iGroupId;
	}

	return InsertItem(&lvi);
}

void CCtrlListView::SetItem(int iItem, int iSubItem, wchar_t *text, int iIcon)
{
	LVITEM lvi = { 0 };
	lvi.mask = LVIF_TEXT;
	lvi.iItem = iItem;
	lvi.iSubItem = iSubItem;
	lvi.pszText = text;
	if (iIcon >= 0) {
		lvi.mask |= LVIF_IMAGE;
		lvi.iImage = iIcon;
	}

	SetItem(&lvi);
}

LPARAM CCtrlListView::GetItemData(int iItem)
{
	LVITEM lvi = { 0 };
	lvi.mask = LVIF_PARAM;
	lvi.iItem = iItem;
	GetItem(&lvi);
	return lvi.lParam;
}

// classic api
DWORD CCtrlListView::ApproximateViewRect(int cx, int cy, int iCount)
{	return ListView_ApproximateViewRect(m_hwnd, cx, cy, iCount);
}
void CCtrlListView::Arrange(UINT code)
{	ListView_Arrange(m_hwnd, code);
}
void CCtrlListView::CancelEditLabel()
{	ListView_CancelEditLabel(m_hwnd);
}
HIMAGELIST CCtrlListView::CreateDragImage(int iItem, LPPOINT lpptUpLeft)
{	return ListView_CreateDragImage(m_hwnd, iItem, lpptUpLeft);
}
void CCtrlListView::DeleteAllItems()
{	ListView_DeleteAllItems(m_hwnd);
}
void CCtrlListView::DeleteColumn(int iCol)
{	ListView_DeleteColumn(m_hwnd, iCol);
}
void CCtrlListView::DeleteItem(int iItem)
{	ListView_DeleteItem(m_hwnd, iItem);
}
HWND CCtrlListView::EditLabel(int iItem)
{	return ListView_EditLabel(m_hwnd, iItem);
}
int CCtrlListView::EnableGroupView(BOOL fEnable)
{	return ListView_EnableGroupView(m_hwnd, fEnable);
}
BOOL CCtrlListView::EnsureVisible(int i, BOOL fPartialOK)
{	return ListView_EnsureVisible(m_hwnd, i, fPartialOK);
}
int CCtrlListView::FindItem(int iStart, const LVFINDINFO *plvfi)
{	return ListView_FindItem(m_hwnd, iStart, plvfi);
}
COLORREF CCtrlListView::GetBkColor()
{	return ListView_GetBkColor(m_hwnd);
}
void CCtrlListView::GetBkImage(LPLVBKIMAGE plvbki)
{	ListView_GetBkImage(m_hwnd, plvbki);
}
UINT CCtrlListView::GetCallbackMask()
{	return ListView_GetCallbackMask(m_hwnd);
}
BOOL CCtrlListView::GetCheckState(UINT iIndex)
{	return ListView_GetCheckState(m_hwnd, iIndex);
}
void CCtrlListView::GetColumn(int iCol, LPLVCOLUMN pcol)
{	ListView_GetColumn(m_hwnd, iCol, pcol);
}
void CCtrlListView::GetColumnOrderArray(int iCount, int *lpiArray)
{	ListView_GetColumnOrderArray(m_hwnd, iCount, lpiArray);
}
int CCtrlListView::GetColumnWidth(int iCol)
{	return ListView_GetColumnWidth(m_hwnd, iCol);
}
int CCtrlListView::GetCountPerPage()
{	return ListView_GetCountPerPage(m_hwnd);
}
HWND CCtrlListView::GetEditControl()
{	return ListView_GetEditControl(m_hwnd);
}
DWORD CCtrlListView::GetExtendedListViewStyle()
{	return ListView_GetExtendedListViewStyle(m_hwnd);
}
void CCtrlListView::GetGroupMetrics(LVGROUPMETRICS *pGroupMetrics)
{	ListView_GetGroupMetrics(m_hwnd, pGroupMetrics);
}
HWND CCtrlListView::GetHeader()
{	return ListView_GetHeader(m_hwnd);
}
HCURSOR CCtrlListView::GetHotCursor()
{	return ListView_GetHotCursor(m_hwnd);
}
INT CCtrlListView::GetHotItem()
{	return ListView_GetHotItem(m_hwnd);
}
DWORD CCtrlListView::GetHoverTime()
{	return ListView_GetHoverTime(m_hwnd);
}
HIMAGELIST CCtrlListView::GetImageList(int iImageList)
{	return ListView_GetImageList(m_hwnd, iImageList);
}
BOOL CCtrlListView::GetInsertMark(LVINSERTMARK *plvim)
{	return ListView_GetInsertMark(m_hwnd, plvim);
}
COLORREF CCtrlListView::GetInsertMarkColor()
{	return ListView_GetInsertMarkColor(m_hwnd);
}
int CCtrlListView::GetInsertMarkRect(LPRECT prc)
{	return ListView_GetInsertMarkRect(m_hwnd, prc);
}
BOOL CCtrlListView::GetISearchString(LPSTR lpsz)
{	return ListView_GetISearchString(m_hwnd, lpsz);
}
bool CCtrlListView::GetItem(LPLVITEM pitem)
{	return ListView_GetItem(m_hwnd, pitem) == TRUE;
}
int CCtrlListView::GetItemCount()
{	return ListView_GetItemCount(m_hwnd);
}
void CCtrlListView::GetItemPosition(int i, POINT *ppt)
{	ListView_GetItemPosition(m_hwnd, i, ppt);
}
void CCtrlListView::GetItemRect(int i, RECT *prc, int code)
{	ListView_GetItemRect(m_hwnd, i, prc, code);
}
DWORD CCtrlListView::GetItemSpacing(BOOL fSmall)
{	return ListView_GetItemSpacing(m_hwnd, fSmall);
}
UINT CCtrlListView::GetItemState(int i, UINT mask)
{	return ListView_GetItemState(m_hwnd, i, mask);
}
void CCtrlListView::GetItemText(int iItem, int iSubItem, LPTSTR pszText, int cchTextMax)
{	ListView_GetItemText(m_hwnd, iItem, iSubItem, pszText, cchTextMax);
}
int CCtrlListView::GetNextItem(int iStart, UINT flags)
{	return ListView_GetNextItem(m_hwnd, iStart, flags);
}
BOOL CCtrlListView::GetNumberOfWorkAreas(LPUINT lpuWorkAreas)
{	return  ListView_GetNumberOfWorkAreas(m_hwnd, lpuWorkAreas);
}
BOOL CCtrlListView::GetOrigin(LPPOINT lpptOrg)
{	return ListView_GetOrigin(m_hwnd, lpptOrg);
}
COLORREF CCtrlListView::GetOutlineColor()
{	return ListView_GetOutlineColor(m_hwnd);
}
UINT CCtrlListView::GetSelectedColumn()
{	return ListView_GetSelectedColumn(m_hwnd);
}
UINT CCtrlListView::GetSelectedCount()
{	return ListView_GetSelectedCount(m_hwnd);
}
INT CCtrlListView::GetSelectionMark()
{	return ListView_GetSelectionMark(m_hwnd);
}
int CCtrlListView::GetStringWidth(LPCSTR psz)
{	return ListView_GetStringWidth(m_hwnd, psz);
}
BOOL CCtrlListView::GetSubItemRect(int iItem, int iSubItem, int code, LPRECT lpRect)
{	return ListView_GetSubItemRect(m_hwnd, iItem, iSubItem, code, lpRect);
}
COLORREF CCtrlListView::GetTextBkColor()
{	return ListView_GetTextBkColor(m_hwnd);
}
COLORREF CCtrlListView::GetTextColor()
{	return ListView_GetTextColor(m_hwnd);
}
void CCtrlListView::GetTileInfo(PLVTILEINFO plvtinfo)
{	ListView_GetTileInfo(m_hwnd, plvtinfo);
}
void CCtrlListView::GetTileViewInfo(PLVTILEVIEWINFO plvtvinfo)
{	ListView_GetTileViewInfo(m_hwnd, plvtvinfo);
}
HWND CCtrlListView::GetToolTips()
{	return ListView_GetToolTips(m_hwnd);
}
int CCtrlListView::GetTopIndex()
{	return ListView_GetTopIndex(m_hwnd);
}
BOOL CCtrlListView::GetUnicodeFormat()
{	return ListView_GetUnicodeFormat(m_hwnd);
}
DWORD CCtrlListView::GetView()
{	return ListView_GetView(m_hwnd);
}
BOOL CCtrlListView::GetViewRect(RECT *prc)
{	return ListView_GetViewRect(m_hwnd, prc);
}
void CCtrlListView::GetWorkAreas(INT nWorkAreas, LPRECT lprc)
{	ListView_GetWorkAreas(m_hwnd, nWorkAreas, lprc);
}
BOOL CCtrlListView::HasGroup(int dwGroupId)
{	return ListView_HasGroup(m_hwnd, dwGroupId);
}
int CCtrlListView::HitTest(LPLVHITTESTINFO pinfo)
{	return ListView_HitTest(m_hwnd, pinfo);
}
int CCtrlListView::InsertColumn(int iCol, const LPLVCOLUMN pcol)
{	return ListView_InsertColumn(m_hwnd, iCol, pcol);
}
int CCtrlListView::InsertGroup(int index, PLVGROUP pgrp)
{	return ListView_InsertGroup(m_hwnd, index, pgrp);
}
void CCtrlListView::InsertGroupSorted(PLVINSERTGROUPSORTED structInsert)
{	ListView_InsertGroupSorted(m_hwnd, structInsert);
}
int CCtrlListView::InsertItem(const LPLVITEM pitem)
{	return ListView_InsertItem(m_hwnd, pitem);
}
BOOL CCtrlListView::InsertMarkHitTest(LPPOINT point, LVINSERTMARK *plvim)
{	return ListView_InsertMarkHitTest(m_hwnd, point, plvim);
}
BOOL CCtrlListView::IsGroupViewEnabled()
{	return ListView_IsGroupViewEnabled(m_hwnd);
}
UINT CCtrlListView::MapIDToIndex(UINT id)
{	return ListView_MapIDToIndex(m_hwnd, id);
}
UINT CCtrlListView::MapIndexToID(UINT index)
{	return ListView_MapIndexToID(m_hwnd, index);
}
BOOL CCtrlListView::RedrawItems(int iFirst, int iLast)
{	return ListView_RedrawItems(m_hwnd, iFirst, iLast);
}
void CCtrlListView::RemoveAllGroups()
{	ListView_RemoveAllGroups(m_hwnd);
}
int CCtrlListView::RemoveGroup(int iGroupId)
{	return ListView_RemoveGroup(m_hwnd, iGroupId);
}
BOOL CCtrlListView::Scroll(int dx, int dy)
{	return ListView_Scroll(m_hwnd, dx, dy);
}
BOOL CCtrlListView::SetBkColor(COLORREF clrBk)
{	return ListView_SetBkColor(m_hwnd, clrBk);
}
BOOL CCtrlListView::SetBkImage(LPLVBKIMAGE plvbki)
{	return ListView_SetBkImage(m_hwnd, plvbki);
}
BOOL CCtrlListView::SetCallbackMask(UINT mask)
{	return ListView_SetCallbackMask(m_hwnd, mask);
}
void CCtrlListView::SetCheckState(UINT iIndex, BOOL fCheck)
{	ListView_SetCheckState(m_hwnd, iIndex, fCheck);
}
BOOL CCtrlListView::SetColumn(int iCol, LPLVCOLUMN pcol)
{	return ListView_SetColumn(m_hwnd, iCol, pcol);
}
BOOL CCtrlListView::SetColumnOrderArray(int iCount, int *lpiArray)
{	return ListView_SetColumnOrderArray(m_hwnd, iCount, lpiArray);
}
BOOL CCtrlListView::SetColumnWidth(int iCol, int cx)
{	return ListView_SetColumnWidth(m_hwnd, iCol, cx);
}
void CCtrlListView::SetExtendedListViewStyle(DWORD dwExStyle)
{	ListView_SetExtendedListViewStyle(m_hwnd, dwExStyle);
}
void CCtrlListView::SetExtendedListViewStyleEx(DWORD dwExMask, DWORD dwExStyle)
{	ListView_SetExtendedListViewStyleEx(m_hwnd, dwExMask, dwExStyle);
}
int CCtrlListView::SetGroupInfo(int iGroupId, PLVGROUP pgrp)
{	return ListView_SetGroupInfo(m_hwnd, iGroupId, pgrp);
}
void CCtrlListView::SetGroupMetrics(PLVGROUPMETRICS pGroupMetrics)
{	ListView_SetGroupMetrics(m_hwnd, pGroupMetrics);
}
HCURSOR CCtrlListView::SetHotCursor(HCURSOR hCursor)
{	return ListView_SetHotCursor(m_hwnd, hCursor);
}
INT CCtrlListView::SetHotItem(INT iIndex)
{	return ListView_SetHotItem(m_hwnd, iIndex);
}
void CCtrlListView::SetHoverTime(DWORD dwHoverTime)
{	ListView_SetHoverTime(m_hwnd, dwHoverTime);
}
DWORD CCtrlListView::SetIconSpacing(int cx, int cy)
{	return ListView_SetIconSpacing(m_hwnd, cx, cy);
}
HIMAGELIST CCtrlListView::SetImageList(HIMAGELIST himl, int iImageList)
{	return ListView_SetImageList(m_hwnd, himl, iImageList);
}
BOOL CCtrlListView::SetInfoTip(PLVSETINFOTIP plvSetInfoTip)
{	return ListView_SetInfoTip(m_hwnd, plvSetInfoTip);
}
BOOL CCtrlListView::SetInsertMark(LVINSERTMARK *plvim)
{	return ListView_SetInsertMark(m_hwnd, plvim);
}
COLORREF CCtrlListView::SetInsertMarkColor(COLORREF color)
{	return ListView_SetInsertMarkColor(m_hwnd, color);
}
BOOL CCtrlListView::SetItem(const LPLVITEM pitem)
{	return ListView_SetItem(m_hwnd, pitem);
}
void CCtrlListView::SetItemCount(int cItems)
{	ListView_SetItemCount(m_hwnd, cItems);
}
void CCtrlListView::SetItemCountEx(int cItems, DWORD dwFlags)
{	ListView_SetItemCountEx(m_hwnd, cItems, dwFlags);
}
BOOL CCtrlListView::SetItemPosition(int i, int x, int y)
{	return ListView_SetItemPosition(m_hwnd, i, x, y);
}
void CCtrlListView::SetItemPosition32(int iItem, int x, int y)
{	ListView_SetItemPosition32(m_hwnd, iItem, x, y);
}
void CCtrlListView::SetItemState(int i, UINT state, UINT mask)
{	ListView_SetItemState(m_hwnd, i, state, mask);
}
void CCtrlListView::SetItemText(int i, int iSubItem, wchar_t *pszText)
{	ListView_SetItemText(m_hwnd, i, iSubItem, pszText);
}
COLORREF CCtrlListView::SetOutlineColor(COLORREF color)
{	return ListView_SetOutlineColor(m_hwnd, color);
}
void CCtrlListView::SetSelectedColumn(int iCol)
{	ListView_SetSelectedColumn(m_hwnd, iCol);
}
INT CCtrlListView::SetSelectionMark(INT iIndex)
{	return ListView_SetSelectionMark(m_hwnd, iIndex);
}
BOOL CCtrlListView::SetTextBkColor(COLORREF clrText)
{	return ListView_SetTextBkColor(m_hwnd, clrText);
}
BOOL CCtrlListView::SetTextColor(COLORREF clrText)
{	return ListView_SetTextColor(m_hwnd, clrText);
}
BOOL CCtrlListView::SetTileInfo(PLVTILEINFO plvtinfo)
{	return ListView_SetTileInfo(m_hwnd, plvtinfo);
}
BOOL CCtrlListView::SetTileViewInfo(PLVTILEVIEWINFO plvtvinfo)
{	return ListView_SetTileViewInfo(m_hwnd, plvtvinfo);
}
HWND CCtrlListView::SetToolTips(HWND ToolTip)
{	return ListView_SetToolTips(m_hwnd, ToolTip);
}
BOOL CCtrlListView::SetUnicodeFormat(BOOL fUnicode)
{	return ListView_SetUnicodeFormat(m_hwnd, fUnicode);
}
int CCtrlListView::SetView(DWORD iView)
{	return ListView_SetView(m_hwnd, iView);
}
void CCtrlListView::SetWorkAreas(INT nWorkAreas, LPRECT lprc)
{	ListView_SetWorkAreas(m_hwnd, nWorkAreas, lprc);
}
int CCtrlListView::SortGroups(PFNLVGROUPCOMPARE pfnGroupCompare, LPVOID plv)
{	return ListView_SortGroups(m_hwnd, pfnGroupCompare, plv);
}
BOOL CCtrlListView::SortItems(PFNLVCOMPARE pfnCompare, LPARAM lParamSort)
{	return ListView_SortItems(m_hwnd, pfnCompare, lParamSort);
}
BOOL CCtrlListView::SortItemsEx(PFNLVCOMPARE pfnCompare, LPARAM lParamSort)
{	return ListView_SortItemsEx(m_hwnd, pfnCompare, lParamSort);
}
INT CCtrlListView::SubItemHitTest(LPLVHITTESTINFO pInfo)
{	return ListView_SubItemHitTest(m_hwnd, pInfo);
}
BOOL CCtrlListView::Update(int iItem)
{	return ListView_Update(m_hwnd, iItem);
}

/////////////////////////////////////////////////////////////////////////////////////////
// CCtrlTreeView

CCtrlTreeView::CCtrlTreeView(CDlgBase *dlg, int ctrlId)
	: CCtrlBase(dlg, ctrlId),
	m_dwFlags(0)
{}

void CCtrlTreeView::SetFlags(uint32_t dwFlags)
{
	if (dwFlags & MTREE_CHECKBOX)
		m_bCheckBox = true;

	if (dwFlags & MTREE_MULTISELECT)
		m_bMultiSelect = true;

	if (dwFlags & MTREE_DND) {
		m_bDndEnabled = true;
		m_bDragging = false;
		m_hDragItem = NULL;
	}
}

void CCtrlTreeView::OnInit()
{
	CSuper::OnInit();

	if (m_bDndEnabled)
		Subclass();
}

HTREEITEM CCtrlTreeView::MoveItemAbove(HTREEITEM hItem, HTREEITEM hInsertAfter, HTREEITEM hParent)
{
	if (hItem == NULL || hInsertAfter == NULL)
		return NULL;

	if (hItem == hInsertAfter)
		return hItem;

	wchar_t name[128];
	TVINSERTSTRUCT tvis = { 0 };
	tvis.itemex.mask = (UINT)-1;
	tvis.itemex.pszText = name;
	tvis.itemex.cchTextMax = _countof(name);
	tvis.itemex.hItem = hItem;
	if (!GetItem(&tvis.itemex))
		return NULL;

	// the pointed lParam will be freed inside TVN_DELETEITEM
	// so lets substitute it with 0
	LPARAM saveOldData = tvis.itemex.lParam;
	tvis.itemex.lParam = 0;
	SetItem(&tvis.itemex);

	// now current item contain lParam = 0 we can delete it. the memory will be kept.
	DeleteItem(hItem);

	tvis.itemex.stateMask = tvis.itemex.state;
	tvis.itemex.lParam = saveOldData;
	tvis.hParent = hParent;
	tvis.hInsertAfter = hInsertAfter;
	return InsertItem(&tvis);
}

LRESULT CCtrlTreeView::CustomWndProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
	TVHITTESTINFO hti;

	switch (msg) {
	case WM_MOUSEMOVE:
		if (m_bDragging) {
			hti.pt.x = (short)LOWORD(lParam);
			hti.pt.y = (short)HIWORD(lParam);
			HitTest(&hti);
			if (hti.flags & (TVHT_ONITEM | TVHT_ONITEMRIGHT)) {
				HTREEITEM it = hti.hItem;
				hti.pt.y -= GetItemHeight() / 2;
				HitTest(&hti);
				if (!(hti.flags & TVHT_ABOVE))
					SetInsertMark(hti.hItem, 1);
				else
					SetInsertMark(it, 0);
			}
			else {
				if (hti.flags & TVHT_ABOVE) SendMsg(WM_VSCROLL, MAKEWPARAM(SB_LINEUP, 0), 0);
				if (hti.flags & TVHT_BELOW) SendMsg(WM_VSCROLL, MAKEWPARAM(SB_LINEDOWN, 0), 0);
				SetInsertMark(NULL, 0);
			}
		}
		break;

	case WM_LBUTTONUP:
		if (m_bDragging) {
			SetInsertMark(NULL, 0);
			m_bDragging = false;
			ReleaseCapture();

			hti.pt.x = (short)LOWORD(lParam);
			hti.pt.y = (short)HIWORD(lParam) - GetItemHeight() / 2;
			HitTest(&hti);
			if (m_hDragItem == hti.hItem)
				break;

			if (hti.flags & TVHT_ABOVE)
				hti.hItem = TVI_FIRST;
			else if (hti.flags & TVHT_BELOW)
				hti.hItem = TVI_LAST;

			HTREEITEM insertAfter = hti.hItem, hParent;
			if (insertAfter != TVI_FIRST) {
				hParent = GetParent(insertAfter);
				if (GetChild(insertAfter) != NULL) {
					hParent = insertAfter;
					insertAfter = TVI_FIRST;
				}
			}
			else hParent = NULL;

			HTREEITEM FirstItem = NULL;
			if (m_bMultiSelect) {
				LIST<_TREEITEM> arItems(10);
				GetSelected(arItems);

				// Proceed moving
				for (int i = 0; i < arItems.getCount(); i++) {
					if (!insertAfter)
						break;
					if (GetParent(arItems[i]) != hParent) // prevent subitems from being inserted at the same level
						continue;

					insertAfter = MoveItemAbove(arItems[i], insertAfter, hParent);
					if (!i)
						FirstItem = insertAfter;
				}
			}
			else FirstItem = MoveItemAbove(m_hDragItem, insertAfter, hParent);
			if (FirstItem)
				SelectItem(FirstItem);

			NotifyChange();
		}
		break;

	case WM_LBUTTONDOWN:
		if (!m_bMultiSelect)
			break;

		hti.pt.x = (short)LOWORD(lParam);
		hti.pt.y = (short)HIWORD(lParam);
		if (!TreeView_HitTest(m_hwnd, &hti)) {
			UnselectAll();
			break;
		}

		if (!m_bDndEnabled)
			if (!(wParam & (MK_CONTROL | MK_SHIFT)) || !(hti.flags & (TVHT_ONITEMICON | TVHT_ONITEMLABEL | TVHT_ONITEMRIGHT))) {
				UnselectAll();
				TreeView_SelectItem(m_hwnd, hti.hItem);
				break;
			}

		if (wParam & MK_CONTROL) {
			LIST<_TREEITEM> selected(1);
			GetSelected(selected);

			// Check if have to deselect it
			for (int i = 0; i < selected.getCount(); i++) {
				if (selected[i] == hti.hItem) {
					// Deselect it
					UnselectAll();
					selected.remove(i);

					if (i > 0)
						hti.hItem = selected[0];
					else if (i < selected.getCount())
						hti.hItem = selected[i];
					else
						hti.hItem = NULL;
					break;
				}
			}

			TreeView_SelectItem(m_hwnd, hti.hItem);
			Select(selected);
		}
		else if (wParam & MK_SHIFT) {
			HTREEITEM hItem = TreeView_GetSelection(m_hwnd);
			if (hItem == NULL)
				break;

			LIST<_TREEITEM> selected(1);
			GetSelected(selected);

			TreeView_SelectItem(m_hwnd, hti.hItem);
			Select(selected);
			SelectRange(hItem, hti.hItem);
		}
		break;
	}

	return CSuper::CustomWndProc(msg, wParam, lParam);
}

BOOL CCtrlTreeView::OnNotify(int, NMHDR *pnmh)
{
	TEventInfo evt = { this, pnmh };

	switch (pnmh->code) {
	case TVN_BEGINLABELEDIT: OnBeginLabelEdit(&evt);  return TRUE;
	case TVN_BEGINRDRAG:     OnBeginRDrag(&evt);      return TRUE;
	case TVN_DELETEITEM:     OnDeleteItem(&evt);      return TRUE;
	case TVN_ENDLABELEDIT:   OnEndLabelEdit(&evt);    return TRUE;
	case TVN_GETDISPINFO:    OnGetDispInfo(&evt);     return TRUE;
	case TVN_GETINFOTIP:     OnGetInfoTip(&evt);      return TRUE;
	case TVN_ITEMEXPANDED:   OnItemExpanded(&evt);    return TRUE;
	case TVN_ITEMEXPANDING:  OnItemExpanding(&evt);   return TRUE;
	case TVN_SELCHANGED:     OnSelChanged(&evt);      return TRUE;
	case TVN_SELCHANGING:    OnSelChanging(&evt);     return TRUE;
	case TVN_SETDISPINFO:    OnSetDispInfo(&evt);     return TRUE;
	case TVN_SINGLEEXPAND:   OnSingleExpand(&evt);    return TRUE;

	case TVN_BEGINDRAG:
		OnBeginDrag(&evt);

		// user-defined can clear the event code to disable dragging
		if (m_bDndEnabled && pnmh->code) {
			::SetCapture(m_hwnd);
			m_bDragging = true;
			m_hDragItem = evt.nmtv->itemNew.hItem;
			SelectItem(m_hDragItem);
		}
		return TRUE;

	case TVN_KEYDOWN:
		if (evt.nmtvkey->wVKey == VK_SPACE) {
			if (m_bCheckBox)
				InvertCheck(GetSelection());
			OnItemChanged(&evt);
			NotifyChange();
		}

		OnKeyDown(&evt);
		return TRUE;
	}

	if (pnmh->code == NM_CLICK) {
		TVHITTESTINFO hti;
		hti.pt.x = (short)LOWORD(GetMessagePos());
		hti.pt.y = (short)HIWORD(GetMessagePos());
		ScreenToClient(pnmh->hwndFrom, &hti.pt);
		if (HitTest(&hti)) {
			if (m_bCheckBox && (hti.flags & TVHT_ONITEMICON) || !m_bCheckBox && (hti.flags & TVHT_ONITEMSTATEICON)) {
				if (m_bCheckBox)
					InvertCheck(hti.hItem);
				else
					SelectItem(hti.hItem);
				OnItemChanged(&evt);
				NotifyChange();
			}
		}
	}

	return FALSE;
}

void CCtrlTreeView::InvertCheck(HTREEITEM hItem)
{
	TVITEMEX tvi;
	tvi.mask = TVIF_HANDLE | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM;
	tvi.hItem = hItem;
	if (!GetItem(&tvi))
		return;

	tvi.iImage = tvi.iSelectedImage = !tvi.iImage;
	SetItem(&tvi);
	
	SelectItem(hItem);
}

void CCtrlTreeView::TranslateItem(HTREEITEM hItem)
{
	TVITEMEX tvi;
	wchar_t buf[128];
	GetItem(hItem, &tvi, buf, _countof(buf));
	tvi.pszText = TranslateW(tvi.pszText);
	SetItem(&tvi);
}

void CCtrlTreeView::TranslateTree()
{
	HTREEITEM hItem = GetRoot();
	while (hItem) {
		TranslateItem(hItem);

		HTREEITEM hItemTmp = 0;
		if (hItemTmp = GetChild(hItem))
			hItem = hItemTmp;
		else if (hItemTmp = GetNextSibling(hItem))
			hItem = hItemTmp;
		else {
			while (true) {
				if (!(hItem = GetParent(hItem)))
					break;
				if (hItemTmp = GetNextSibling(hItem)) {
					hItem = hItemTmp;
					break;
				}
			}
		}
	}
}

HTREEITEM CCtrlTreeView::FindNamedItem(HTREEITEM hItem, const wchar_t *name)
{
	TVITEMEX tvi = { 0 };
	wchar_t str[MAX_PATH];

	if (hItem)
		tvi.hItem = GetChild(hItem);
	else
		tvi.hItem = GetRoot();

	if (!name)
		return tvi.hItem;

	tvi.mask = TVIF_TEXT;
	tvi.pszText = str;
	tvi.cchTextMax = _countof(str);

	while (tvi.hItem) {
		GetItem(&tvi);

		if (!mir_wstrcmp(tvi.pszText, name))
			return tvi.hItem;

		tvi.hItem = GetNextSibling(tvi.hItem);
	}
	return NULL;
}

void CCtrlTreeView::GetItem(HTREEITEM hItem, TVITEMEX *tvi)
{
	memset(tvi, 0, sizeof(*tvi));
	tvi->mask = TVIF_CHILDREN | TVIF_HANDLE | TVIF_IMAGE | TVIF_INTEGRAL | TVIF_PARAM | TVIF_SELECTEDIMAGE | TVIF_STATE;
	tvi->hItem = hItem;
	GetItem(tvi);
}

void CCtrlTreeView::GetItem(HTREEITEM hItem, TVITEMEX *tvi, wchar_t *szText, int iTextLength)
{
	memset(tvi, 0, sizeof(*tvi));
	tvi->mask = TVIF_CHILDREN | TVIF_HANDLE | TVIF_IMAGE | TVIF_INTEGRAL | TVIF_PARAM | TVIF_SELECTEDIMAGE | TVIF_STATE | TVIF_TEXT;
	tvi->hItem = hItem;
	tvi->pszText = szText;
	tvi->cchTextMax = iTextLength;
	GetItem(tvi);
}

bool CCtrlTreeView::IsSelected(HTREEITEM hItem)
{
	return (TVIS_SELECTED & TreeView_GetItemState(m_hwnd, hItem, TVIS_SELECTED)) == TVIS_SELECTED;
}

void CCtrlTreeView::Select(HTREEITEM hItem)
{
	TreeView_SetItemState(m_hwnd, hItem, TVIS_SELECTED, TVIS_SELECTED);
}

void CCtrlTreeView::Unselect(HTREEITEM hItem)
{
	TreeView_SetItemState(m_hwnd, hItem, 0, TVIS_SELECTED);
}

void CCtrlTreeView::DropHilite(HTREEITEM hItem)
{
	TreeView_SetItemState(m_hwnd, hItem, TVIS_DROPHILITED, TVIS_DROPHILITED);
}

void CCtrlTreeView::DropUnhilite(HTREEITEM hItem)
{
	TreeView_SetItemState(m_hwnd, hItem, 0, TVIS_DROPHILITED);
}

void CCtrlTreeView::SelectAll()
{
	TreeView_SelectItem(m_hwnd, NULL);

	HTREEITEM hItem = TreeView_GetRoot(m_hwnd);
	while (hItem) {
		Select(hItem);
		hItem = TreeView_GetNextSibling(m_hwnd, hItem);
	}
}

void CCtrlTreeView::UnselectAll()
{
	TreeView_SelectItem(m_hwnd, NULL);

	HTREEITEM hItem = TreeView_GetRoot(m_hwnd);
	while (hItem) {
		Unselect(hItem);
		hItem = TreeView_GetNextSibling(m_hwnd, hItem);
	}
}

void CCtrlTreeView::SelectRange(HTREEITEM hStart, HTREEITEM hEnd)
{
	int start = 0, end = 0, i = 0;
	HTREEITEM hItem = TreeView_GetRoot(m_hwnd);
	while (hItem) {
		if (hItem == hStart)
			start = i;
		if (hItem == hEnd)
			end = i;

		i++;
		hItem = TreeView_GetNextSibling(m_hwnd, hItem);
	}

	if (end < start) {
		int tmp = start;
		start = end;
		end = tmp;
	}

	i = 0;
	hItem = TreeView_GetRoot(m_hwnd);
	while (hItem) {
		if (i >= start)
			Select(hItem);
		if (i == end)
			break;

		i++;
		hItem = TreeView_GetNextSibling(m_hwnd, hItem);
	}
}

int CCtrlTreeView::GetNumSelected()
{
	int ret = 0;
	for (HTREEITEM hItem = TreeView_GetRoot(m_hwnd); hItem; hItem = TreeView_GetNextSibling(m_hwnd, hItem))
		if (IsSelected(hItem))
			ret++;

	return ret;
}

void CCtrlTreeView::GetSelected(LIST<_TREEITEM> &selected)
{
	HTREEITEM hItem = TreeView_GetRoot(m_hwnd);
	while (hItem) {
		if (IsSelected(hItem))
			selected.insert(hItem);
		hItem = TreeView_GetNextSibling(m_hwnd, hItem);
	}
}

void CCtrlTreeView::Select(LIST<_TREEITEM> &selected)
{
	for (int i = 0; i < selected.getCount(); i++)
		if (selected[i] != NULL)
			Select(selected[i]);
}

/////////////////////////////////////////////////////////////////////////////////////////

HIMAGELIST CCtrlTreeView::CreateDragImage(HTREEITEM hItem)
{	return TreeView_CreateDragImage(m_hwnd, hItem);
}

void CCtrlTreeView::DeleteAllItems()
{	TreeView_DeleteAllItems(m_hwnd);
}

void CCtrlTreeView::DeleteItem(HTREEITEM hItem)
{	TreeView_DeleteItem(m_hwnd, hItem);
}

HWND CCtrlTreeView::EditLabel(HTREEITEM hItem)
{	return TreeView_EditLabel(m_hwnd, hItem);
}

void CCtrlTreeView::EndEditLabelNow(BOOL cancel)
{	TreeView_EndEditLabelNow(m_hwnd, cancel);
}

void CCtrlTreeView::EnsureVisible(HTREEITEM hItem)
{	TreeView_EnsureVisible(m_hwnd, hItem);
}

void CCtrlTreeView::Expand(HTREEITEM hItem, DWORD flag)
{	TreeView_Expand(m_hwnd, hItem, flag);
}

COLORREF CCtrlTreeView::GetBkColor()
{	return TreeView_GetBkColor(m_hwnd);
}

DWORD CCtrlTreeView::GetCheckState(HTREEITEM hItem)
{	return TreeView_GetCheckState(m_hwnd, hItem);
}

HTREEITEM CCtrlTreeView::GetChild(HTREEITEM hItem)
{	return TreeView_GetChild(m_hwnd, hItem);
}

int CCtrlTreeView::GetCount()
{	return TreeView_GetCount(m_hwnd);
}

HTREEITEM CCtrlTreeView::GetDropHilight()
{	return TreeView_GetDropHilight(m_hwnd);
}

HWND CCtrlTreeView::GetEditControl()
{	return TreeView_GetEditControl(m_hwnd);
}

HTREEITEM CCtrlTreeView::GetFirstVisible()
{	return TreeView_GetFirstVisible(m_hwnd);
}

HIMAGELIST CCtrlTreeView::GetImageList(int iImage)
{	return TreeView_GetImageList(m_hwnd, iImage);
}

int CCtrlTreeView::GetIndent()
{	return TreeView_GetIndent(m_hwnd);
}

COLORREF CCtrlTreeView::GetInsertMarkColor()
{	return TreeView_GetInsertMarkColor(m_hwnd);
}

bool CCtrlTreeView::GetItem(TVITEMEX *tvi)
{	return TreeView_GetItem(m_hwnd, tvi) == TRUE;
}

int CCtrlTreeView::GetItemHeight()
{	return TreeView_GetItemHeight(m_hwnd);
}

void CCtrlTreeView::GetItemRect(HTREEITEM hItem, RECT *rcItem, BOOL fItemRect)
{	TreeView_GetItemRect(m_hwnd, hItem, rcItem, fItemRect);
}

DWORD CCtrlTreeView::GetItemState(HTREEITEM hItem, DWORD stateMask)
{	return TreeView_GetItemState(m_hwnd, hItem, stateMask);
}

HTREEITEM CCtrlTreeView::GetLastVisible()
{	return TreeView_GetLastVisible(m_hwnd);
}

COLORREF CCtrlTreeView::GetLineColor()
{	return TreeView_GetLineColor(m_hwnd);
}

HTREEITEM CCtrlTreeView::GetNextItem(HTREEITEM hItem, DWORD flag)
{	return TreeView_GetNextItem(m_hwnd, hItem, flag);
}

HTREEITEM CCtrlTreeView::GetNextSibling(HTREEITEM hItem)
{	return TreeView_GetNextSibling(m_hwnd, hItem);
}

HTREEITEM CCtrlTreeView::GetNextVisible(HTREEITEM hItem)
{	return TreeView_GetNextVisible(m_hwnd, hItem);
}

HTREEITEM CCtrlTreeView::GetParent(HTREEITEM hItem)
{	return TreeView_GetParent(m_hwnd, hItem);
}

HTREEITEM CCtrlTreeView::GetPrevSibling(HTREEITEM hItem)
{	return TreeView_GetPrevSibling(m_hwnd, hItem);
}

HTREEITEM CCtrlTreeView::GetPrevVisible(HTREEITEM hItem)
{	return TreeView_GetPrevVisible(m_hwnd, hItem);
}

HTREEITEM CCtrlTreeView::GetRoot()
{	return TreeView_GetRoot(m_hwnd);
}

DWORD CCtrlTreeView::GetScrollTime()
{	return TreeView_GetScrollTime(m_hwnd);
}

HTREEITEM CCtrlTreeView::GetSelection()
{	return TreeView_GetSelection(m_hwnd);
}

COLORREF CCtrlTreeView::GetTextColor()
{	return TreeView_GetTextColor(m_hwnd);
}

HWND CCtrlTreeView::GetToolTips()
{	return TreeView_GetToolTips(m_hwnd);
}

BOOL CCtrlTreeView::GetUnicodeFormat()
{	return TreeView_GetUnicodeFormat(m_hwnd);
}

unsigned CCtrlTreeView::GetVisibleCount()
{	return TreeView_GetVisibleCount(m_hwnd);
}

HTREEITEM CCtrlTreeView::HitTest(TVHITTESTINFO *hti)
{	return TreeView_HitTest(m_hwnd, hti);
}

HTREEITEM CCtrlTreeView::InsertItem(TVINSERTSTRUCT *tvis)
{	return TreeView_InsertItem(m_hwnd, tvis);
}

void CCtrlTreeView::Select(HTREEITEM hItem, DWORD flag)
{	TreeView_Select(m_hwnd, hItem, flag);
}

void CCtrlTreeView::SelectDropTarget(HTREEITEM hItem)
{	TreeView_SelectDropTarget(m_hwnd, hItem);
}

void CCtrlTreeView::SelectItem(HTREEITEM hItem)
{	TreeView_SelectItem(m_hwnd, hItem);
}

void CCtrlTreeView::SelectSetFirstVisible(HTREEITEM hItem)
{	TreeView_SelectSetFirstVisible(m_hwnd, hItem);
}

COLORREF CCtrlTreeView::SetBkColor(COLORREF clBack)
{	return TreeView_SetBkColor(m_hwnd, clBack);
}

void CCtrlTreeView::SetCheckState(HTREEITEM hItem, DWORD state)
{	TreeView_SetCheckState(m_hwnd, hItem, state);
}

void CCtrlTreeView::SetImageList(HIMAGELIST hIml, int iImage)
{	TreeView_SetImageList(m_hwnd, hIml, iImage);
}

void CCtrlTreeView::SetIndent(int iIndent)
{	TreeView_SetIndent(m_hwnd, iIndent);
}

void CCtrlTreeView::SetInsertMark(HTREEITEM hItem, BOOL fAfter)
{	TreeView_SetInsertMark(m_hwnd, hItem, fAfter);
}

COLORREF CCtrlTreeView::SetInsertMarkColor(COLORREF clMark)
{	return TreeView_SetInsertMarkColor(m_hwnd, clMark);
}

void CCtrlTreeView::SetItem(TVITEMEX *tvi)
{	TreeView_SetItem(m_hwnd, tvi);
}

void CCtrlTreeView::SetItemHeight(short cyItem)
{	TreeView_SetItemHeight(m_hwnd, cyItem);
}

void CCtrlTreeView::SetItemState(HTREEITEM hItem, DWORD state, DWORD stateMask)
{	TreeView_SetItemState(m_hwnd, hItem, state, stateMask);
}

COLORREF CCtrlTreeView::SetLineColor(COLORREF clLine)
{	return TreeView_SetLineColor(m_hwnd, clLine);
}

void CCtrlTreeView::SetScrollTime(UINT uMaxScrollTime)
{	TreeView_SetScrollTime(m_hwnd, uMaxScrollTime);
}

COLORREF CCtrlTreeView::SetTextColor(COLORREF clText)
{	return TreeView_SetTextColor(m_hwnd, clText);
}

HWND CCtrlTreeView::SetToolTips(HWND hwndToolTips)
{	return TreeView_SetToolTips(m_hwnd, hwndToolTips);
}

BOOL CCtrlTreeView::SetUnicodeFormat(BOOL fUnicode)
{	return TreeView_SetUnicodeFormat(m_hwnd, fUnicode);
}

void CCtrlTreeView::SortChildren(HTREEITEM hItem, BOOL fRecurse)
{	TreeView_SortChildren(m_hwnd, hItem, fRecurse);
}

void CCtrlTreeView::SortChildrenCB(TVSORTCB *cb, BOOL fRecurse)
{	TreeView_SortChildrenCB(m_hwnd, cb, fRecurse);
}

/////////////////////////////////////////////////////////////////////////////////////////
// CCtrlPages

CCtrlPages::CCtrlPages(CDlgBase *dlg, int ctrlId)
	: CCtrlBase(dlg, ctrlId),
	m_hIml(NULL),
	m_pActivePage(NULL),
	m_pages(4)
{}

void CCtrlPages::OnInit()
{
	CSuper::OnInit();
	Subclass();

	for (int i = 0; i < m_pages.getCount(); i++)
		InsertPage(m_pages[i]);
	m_pages.destroy();

	::SetWindowLongPtr(m_hwnd, GWL_EXSTYLE, ::GetWindowLongPtr(m_hwnd, GWL_EXSTYLE) | WS_EX_CONTROLPARENT);

	TPageInfo *info = GetCurrPage();
	if (info) {
		m_pActivePage = info->m_pDlg;
		ShowPage(m_pActivePage);

		PSHNOTIFY pshn;
		pshn.hdr.code = PSN_INFOCHANGED;
		pshn.hdr.hwndFrom = m_pActivePage->GetHwnd();
		pshn.hdr.idFrom = 0;
		pshn.lParam = 0;
		SendMessage(pshn.hdr.hwndFrom, WM_NOTIFY, 0, (LPARAM)&pshn);
	}
}

LRESULT CCtrlPages::CustomWndProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
	int tabCount;

	switch (msg) {
	case WM_SIZE:
		RECT rc;
		GetClientRect(m_hwnd, &rc);
		TabCtrl_AdjustRect(m_hwnd, FALSE, &rc);

		tabCount = GetCount();
		for (int i = 0; i < tabCount; i++) {
			TPageInfo *p = GetItemPage(i);
			if (p && p->m_pDlg->GetHwnd() != NULL)
				SetWindowPos(p->m_pDlg->GetHwnd(), HWND_TOP, rc.left, rc.top, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);
		}
		break;

	case PSM_CHANGED:
		if (TPageInfo *info = GetCurrPage())
			info->m_bChanged = TRUE;
		return TRUE;

	case PSM_FORCECHANGED:
		tabCount = GetCount();

		PSHNOTIFY pshn;
		pshn.hdr.code = PSN_INFOCHANGED;
		pshn.hdr.idFrom = 0;
		pshn.lParam = 0;
		for (int i = 0; i < tabCount; i++) {
			TPageInfo *p = GetItemPage(i);
			if (p) {
				pshn.hdr.hwndFrom = p->m_pDlg->GetHwnd();
				if (pshn.hdr.hwndFrom != NULL)
					SendMessage(pshn.hdr.hwndFrom, WM_NOTIFY, 0, (LPARAM)&pshn);
			}
		}
		break;
	}

	return CSuper::CustomWndProc(msg, wParam, lParam);
}

void CCtrlPages::AddPage(wchar_t *ptszName, HICON hIcon, CDlgBase *pDlg)
{
	TPageInfo *info = new TPageInfo;
	info->m_pDlg = pDlg;
	info->m_hIcon = hIcon;
	info->m_ptszHeader = mir_wstrdup(ptszName);

	if (m_hwnd != NULL) {
		InsertPage(info);

		if (GetCount() == 1) {
			m_pActivePage = info->m_pDlg;
			ShowPage(m_pActivePage);
		}
	}
	else m_pages.insert(info);
}

void CCtrlPages::ActivatePage(int iPage)
{
	TPageInfo *info = GetItemPage(iPage);
	if (info == NULL)
		return;

	if (m_pActivePage != NULL)
		ShowWindow(m_pActivePage->GetHwnd(), SW_HIDE);

	m_pActivePage = info->m_pDlg;
	ShowPage(m_pActivePage);

	TabCtrl_SetCurSel(m_hwnd, info->m_pageId);
}

int CCtrlPages::GetCount()
{
	return TabCtrl_GetItemCount(m_hwnd);
}

CDlgBase* CCtrlPages::GetNthPage(int iPage)
{
	TPageInfo *info = GetItemPage(iPage);
	return (info == NULL) ? NULL : info->m_pDlg;
}

CCtrlPages::TPageInfo* CCtrlPages::GetCurrPage()
{
	TCITEM tci = { 0 };
	tci.mask = TCIF_PARAM;
	if (!TabCtrl_GetItem(m_hwnd, TabCtrl_GetCurSel(m_hwnd), &tci))
		return NULL;

	return (TPageInfo*)tci.lParam;
}

CCtrlPages::TPageInfo* CCtrlPages::GetItemPage(int iPage)
{
	TCITEM tci = { 0 };
	tci.mask = TCIF_PARAM;
	if (!TabCtrl_GetItem(m_hwnd, iPage, &tci))
		return NULL;

	return (TPageInfo*)tci.lParam;
}

int CCtrlPages::GetDlgIndex(CDlgBase *pDlg)
{
	int tabCount = TabCtrl_GetItemCount(m_hwnd);
	for (int i = 0; i < tabCount; i++) {
		TCITEM tci;
		tci.mask = TCIF_PARAM | TCIF_IMAGE;
		TabCtrl_GetItem(m_hwnd, i, &tci);
		TPageInfo *pPage = (TPageInfo *)tci.lParam;
		if (pPage == NULL)
			continue;

		if (pPage->m_pDlg == pDlg)
			return i;
	}

	return -1;
}


void CCtrlPages::InsertPage(TPageInfo *pPage)
{
	TCITEM tci = { 0 };
	tci.mask = TCIF_PARAM | TCIF_TEXT;
	tci.lParam = (LPARAM)pPage;
	tci.pszText = TranslateW(pPage->m_ptszHeader);
	if (pPage->m_hIcon) {
		if (!m_hIml) {
			m_hIml = ImageList_Create(16, 16, ILC_COLOR32 | ILC_MASK, 0, 1);
			TabCtrl_SetImageList(m_hwnd, m_hIml);
		}

		tci.mask |= TCIF_IMAGE;
		tci.iImage = ImageList_AddIcon(m_hIml, pPage->m_hIcon);
	}

	pPage->m_pageId = TabCtrl_InsertItem(m_hwnd, TabCtrl_GetItemCount(m_hwnd), &tci);
}

void CCtrlPages::RemovePage(int iPage)
{
	TPageInfo *p = GetItemPage(iPage);
	if (p == NULL)
		return;

	TabCtrl_DeleteItem(m_hwnd, iPage);
	delete p;
}

void CCtrlPages::ShowPage(CDlgBase *pDlg)
{
	if (pDlg->GetHwnd() == NULL) {
		pDlg->SetParent(m_hwnd);
		pDlg->Create();

		RECT rc;
		GetClientRect(m_hwnd, &rc);
		TabCtrl_AdjustRect(m_hwnd, FALSE, &rc);
		SetWindowPos(pDlg->GetHwnd(), HWND_TOP, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, SWP_NOACTIVATE);

		EnableThemeDialogTexture(pDlg->GetHwnd(), ETDT_ENABLETAB);

		PSHNOTIFY pshn;
		pshn.hdr.code = PSN_INFOCHANGED;
		pshn.hdr.hwndFrom = pDlg->GetHwnd();
		pshn.hdr.idFrom = 0;
		pshn.lParam = 0;
		SendMessage(pshn.hdr.hwndFrom, WM_NOTIFY, 0, (LPARAM)&pshn);
	}
	ShowWindow(pDlg->GetHwnd(), SW_SHOW);
}

BOOL CCtrlPages::OnNotify(int /*idCtrl*/, NMHDR *pnmh)
{
	TPageInfo *info;
	PSHNOTIFY pshn;

	switch (pnmh->code) {
	case TCN_SELCHANGING:
		if (info = GetCurrPage()) {
			pshn.hdr.code = PSN_KILLACTIVE;
			pshn.hdr.hwndFrom = info->m_pDlg->GetHwnd();
			pshn.hdr.idFrom = 0;
			pshn.lParam = 0;
			if (SendMessage(pshn.hdr.hwndFrom, WM_NOTIFY, 0, (LPARAM)&pshn)) {
				SetWindowLongPtr(GetParent()->GetHwnd(), DWLP_MSGRESULT, TRUE);
				return TRUE;
			}
		}
		return TRUE;

	case TCN_SELCHANGE:
		if (m_pActivePage != NULL)
			ShowWindow(m_pActivePage->GetHwnd(), SW_HIDE);

		if (info = GetCurrPage()) {
			m_pActivePage = info->m_pDlg;
			ShowPage(m_pActivePage);
		}
		else m_pActivePage = NULL;
		return TRUE;
	}

	return FALSE;
}

void CCtrlPages::OnReset()
{
	CSuper::OnReset();

	PSHNOTIFY pshn;
	pshn.hdr.code = PSN_INFOCHANGED;
	pshn.hdr.idFrom = 0;
	pshn.lParam = 0;

	int tabCount = GetCount();
	for (int i = 0; i < tabCount; i++) {
		TPageInfo *p = GetItemPage(i);
		if (p->m_pDlg->GetHwnd() == NULL || !p->m_bChanged)
			continue;

		pshn.hdr.hwndFrom = p->m_pDlg->GetHwnd();
		SendMessage(pshn.hdr.hwndFrom, WM_NOTIFY, 0, (LPARAM)&pshn);
	}
}

void CCtrlPages::OnApply()
{
	PSHNOTIFY pshn;
	pshn.hdr.idFrom = 0;
	pshn.lParam = 0;

	if (m_pActivePage != NULL) {
		pshn.hdr.code = PSN_KILLACTIVE;
		pshn.hdr.hwndFrom = m_pActivePage->GetHwnd();
		if (SendMessage(pshn.hdr.hwndFrom, WM_NOTIFY, 0, (LPARAM)&pshn)) {
			m_parentWnd->Fail();
			return;
		}
	}

	pshn.hdr.code = PSN_APPLY;
	int tabCount = GetCount();
	for (int i = 0; i < tabCount; i++) {
		TPageInfo *p = GetItemPage(i);
		if (p->m_pDlg->GetHwnd() == NULL || !p->m_bChanged)
			continue;

		pshn.hdr.hwndFrom = p->m_pDlg->GetHwnd();
		SendMessage(pshn.hdr.hwndFrom, WM_NOTIFY, 0, (LPARAM)&pshn);
		if (GetWindowLongPtr(pshn.hdr.hwndFrom, DWLP_MSGRESULT) == PSNRET_INVALID_NOCHANGEPAGE) {
			TabCtrl_SetCurSel(m_hwnd, p->m_pageId);
			if (m_pActivePage != NULL)
				ShowWindow(m_pActivePage->GetHwnd(), SW_HIDE);
			m_pActivePage = p->m_pDlg;
			ShowWindow(m_pActivePage->GetHwnd(), SW_SHOW);
			m_parentWnd->Fail();
			return;
		}
	}
	
	CSuper::OnApply();
}

void CCtrlPages::OnDestroy()
{
	int tabCount = GetCount();
	for (int i = 0; i < tabCount; i++) {
		TPageInfo *p = GetItemPage(i);
		if (p->m_pDlg->GetHwnd())
			p->m_pDlg->Close();
		delete p;
	}			

	TabCtrl_DeleteAllItems(m_hwnd);

	if (m_hIml) {
		TabCtrl_SetImageList(m_hwnd, NULL);
		ImageList_Destroy(m_hIml);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////
// CCtrlBase

CCtrlBase::CCtrlBase(CDlgBase *wnd, int idCtrl)
	: m_parentWnd(wnd),
	m_idCtrl(idCtrl),
	m_hwnd(NULL),
	m_bChanged(false),
	m_bSilent(false)
{
	if (wnd)
		wnd->AddControl(this);
}

void CCtrlBase::OnInit()
{
	m_hwnd = (m_idCtrl && m_parentWnd && m_parentWnd->GetHwnd()) ? GetDlgItem(m_parentWnd->GetHwnd(), m_idCtrl) : NULL;
}

void CCtrlBase::OnDestroy()
{
	m_hwnd = NULL;
}

void CCtrlBase::OnApply()
{
	m_bChanged = false;
}

void CCtrlBase::OnReset()
{}

void CCtrlBase::Enable(int bIsEnable)
{
	::EnableWindow(m_hwnd, bIsEnable);
}

BOOL CCtrlBase::Enabled() const
{
	return (m_hwnd) ? IsWindowEnabled(m_hwnd) : FALSE;
}

void CCtrlBase::NotifyChange()
{
	if (!m_parentWnd || m_parentWnd->IsInitialized())
		m_bChanged = true;

	if (m_parentWnd && !m_bSilent) {
		m_parentWnd->OnChange(this);
		if (m_parentWnd->IsInitialized())
			::SendMessage(::GetParent(m_parentWnd->GetHwnd()), PSM_CHANGED, 0, 0);
	}

	OnChange(this);
}

LRESULT CCtrlBase::SendMsg(UINT Msg, WPARAM wParam, LPARAM lParam)
{
	return ::SendMessage(m_hwnd, Msg, wParam, lParam);
}

void CCtrlBase::SetText(const wchar_t *text)
{
	::SetWindowText(m_hwnd, text);
}

void CCtrlBase::SetTextA(const char *text)
{
	::SetWindowTextA(m_hwnd, text);
}

void CCtrlBase::SetInt(int value)
{
	wchar_t buf[32] = { 0 };
	mir_snwprintf(buf, L"%d", value);
	SetWindowText(m_hwnd, buf);
}

wchar_t* CCtrlBase::GetText()
{
	int length = GetWindowTextLength(m_hwnd) + 1;
	wchar_t *result = (wchar_t *)mir_alloc(length * sizeof(wchar_t));
	GetWindowText(m_hwnd, result, length);
	return result;
}

char* CCtrlBase::GetTextA()
{
	int length = GetWindowTextLength(m_hwnd) + 1;
	char *result = (char *)mir_alloc(length * sizeof(char));
	GetWindowTextA(m_hwnd, result, length);
	return result;
}

wchar_t* CCtrlBase::GetText(wchar_t *buf, int size)
{
	GetWindowText(m_hwnd, buf, size);
	buf[size - 1] = 0;
	return buf;
}

char* CCtrlBase::GetTextA(char *buf, int size)
{
	GetWindowTextA(m_hwnd, buf, size);
	buf[size - 1] = 0;
	return buf;
}

int CCtrlBase::GetInt()
{
	int length = GetWindowTextLength(m_hwnd) + 1;
	wchar_t *result = (wchar_t *)_alloca(length * sizeof(wchar_t));
	GetWindowText(m_hwnd, result, length);
	return _wtoi(result);
}

LRESULT CCtrlBase::CustomWndProc(UINT, WPARAM, LPARAM)
{
	return FALSE;
}

LRESULT CALLBACK CCtrlBase::GlobalSubclassWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	PVOID bullshit[2];  // vfptr + hwnd
	bullshit[1] = hwnd;
	CCtrlBase *pCtrl = arControls.find((CCtrlBase*)&bullshit);
	if (pCtrl) {
		LRESULT res = pCtrl->CustomWndProc(msg, wParam, lParam);
		if (msg == WM_DESTROY) {
			pCtrl->Unsubclass();
			arControls.remove(pCtrl);
		}

		if (res != 0)
			return res;
	}

	return mir_callNextSubclass(hwnd, GlobalSubclassWndProc, msg, wParam, lParam);
}

void CCtrlBase::Subclass()
{
	mir_subclassWindow(m_hwnd, GlobalSubclassWndProc);

	mir_cslock lck(csCtrl);
	arControls.insert(this);
}

void CCtrlBase::Unsubclass()
{
	mir_unsubclassWindow(m_hwnd, GlobalSubclassWndProc);
}

/////////////////////////////////////////////////////////////////////////////////////////
// CDbLink class

CDbLink::CDbLink(const char *szModule, const char *szSetting, BYTE type, DWORD iValue)
	: CDataLink(type)
{
	m_szModule = mir_strdup(szModule);
	m_szSetting = mir_strdup(szSetting);
	m_iDefault = iValue;
	m_szDefault = 0;
	dbv.type = DBVT_DELETED;
}

CDbLink::CDbLink(const char *szModule, const char *szSetting, BYTE type, wchar_t *szValue)
	: CDataLink(type)
{
	m_szModule = mir_strdup(szModule);
	m_szSetting = mir_strdup(szSetting);
	m_szDefault = mir_wstrdup(szValue);
	dbv.type = DBVT_DELETED;
}

CDbLink::~CDbLink()
{
	mir_free(m_szModule);
	mir_free(m_szSetting);
	mir_free(m_szDefault);
	if (dbv.type != DBVT_DELETED)
		db_free(&dbv);
}

DWORD CDbLink::LoadInt()
{
	switch (m_type) {
		case DBVT_BYTE:  return db_get_b(NULL, m_szModule, m_szSetting, m_iDefault);
		case DBVT_WORD:  return db_get_w(NULL, m_szModule, m_szSetting, m_iDefault);
		case DBVT_DWORD: return db_get_dw(NULL, m_szModule, m_szSetting, m_iDefault);
		default:         return m_iDefault;
	}
}

void CDbLink::SaveInt(DWORD value)
{
	switch (m_type) {
		case DBVT_BYTE:  db_set_b(NULL, m_szModule, m_szSetting, (BYTE)value); break;
		case DBVT_WORD:  db_set_w(NULL, m_szModule, m_szSetting, (WORD)value); break;
		case DBVT_DWORD: db_set_dw(NULL, m_szModule, m_szSetting, value); break;
	}
}

wchar_t* CDbLink::LoadText()
{
	if (dbv.type != DBVT_DELETED) db_free(&dbv);
	if (!db_get_ws(NULL, m_szModule, m_szSetting, &dbv)) {
		if (dbv.type == DBVT_WCHAR)
			return dbv.ptszVal;
		return m_szDefault;
	}

	dbv.type = DBVT_DELETED;
	return m_szDefault;
}

void CDbLink::SaveText(wchar_t *value)
{
	db_set_ws(NULL, m_szModule, m_szSetting, value);
}
