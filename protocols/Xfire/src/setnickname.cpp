//f�rs nick - dialog

#include "stdafx.h"
#include "setnickname.h"

LRESULT CALLBACK DlgNickProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
		case WM_INITDIALOG:
		{
			TranslateDialogDefault(hwndDlg);
			SendMessage(hwndDlg,WM_SETICON, (WPARAM)false, (LPARAM)LoadIcon(hinstance, MAKEINTRESOURCE(IDI_TM)));

			DBVARIANT dbv;
			if(!DBGetContactSetting(NULL,protocolname,"Nick",&dbv)) {
				SetDlgItemText(hwndDlg,IDC_NICKNAME,dbv.pszVal);
				DBFreeVariant(&dbv);
			}
			return TRUE;
		}
		case WM_COMMAND:
		{
			if(LOWORD(wParam) == IDOK)
			{
				char nick[255];
				GetDlgItemText(hwndDlg,IDC_NICKNAME,nick,sizeof(nick));

				CallService(XFIRE_SET_NICK,0,(LPARAM)nick);

				EndDialog(hwndDlg,TRUE);
				return TRUE;
			}
			else if(LOWORD(wParam) == IDCANCEL)
			{
				EndDialog(hwndDlg,FALSE);
				return FALSE;
			}
		}
	}
	return FALSE;
}

BOOL ShowSetNick() {
	return DialogBox(hinstance,MAKEINTRESOURCE(IDD_SETNICKNAME),NULL,DlgNickProc);
}