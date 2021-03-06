/*
Weather Protocol plugin for Miranda IM
Copyright (c) 2012 Miranda NG Team
Copyright (c) 2005-2011 Boris Krasnovskiy All Rights Reserved
Copyright (c) 2002-2005 Calvin Che

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2
of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
Main file for the Weather Protocol, includes loading, unloading,
upgrading, support for plugin uninsaller, and anything that doesn't
belong to any other file.
*/

#include "stdafx.h"

//============  GLOBAL VARIABLES  ============

WIDATALIST *WIHead;
WIDATALIST *WITail;

HINSTANCE hInst;
HWND hPopupWindow;

HANDLE hHookWeatherUpdated;
HANDLE hHookWeatherError;

MWindowList hDataWindowList, hWindowList;

HANDLE hUpdateMutex;
CLIST_INTERFACE *pcli;

unsigned status;
unsigned old_status;

UINT_PTR timerId;
int hLangpack;

MYOPTIONS opt;

// check if weather is currently updating
BOOL ThreadRunning;

// variable to determine if module loaded
BOOL ModuleLoaded;

HANDLE hTBButton = NULL;

// plugin info
static const PLUGININFOEX pluginInfoEx =
{
	sizeof(PLUGININFOEX),
	__PLUGIN_NAME,
	PLUGIN_MAKE_VERSION(__MAJOR_VERSION, __MINOR_VERSION, __RELEASE_NUM, __BUILD_NUM),
	__DESCRIPTION,
	__AUTHOR,
	__AUTHOREMAIL,
	__COPYRIGHT,
	__AUTHORWEB,
	UNICODE_AWARE,
	// {6B612A34-DCF2-4E32-85CF-B6FD006B745E}
	{0x6b612a34, 0xdcf2, 0x4e32, {0x85, 0xcf, 0xb6, 0xfd, 0x0, 0x6b, 0x74, 0x5e}}
};

extern "C" __declspec(dllexport) const PLUGININFOEX* MirandaPluginInfoEx(DWORD)
{
	return &pluginInfoEx;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD, LPVOID)
{
	hInst = hinstDLL;
	return TRUE;
}

int WeatherShutdown(WPARAM, LPARAM)
{
	KillTimer(NULL, timerId);		// kill update timer

	SaveOptions();					// save options once more
	status = ID_STATUS_OFFLINE;		// set status to offline

	CallService(MS_NETLIB_SHUTDOWN, (WPARAM)hNetlibHttp, 0);

	WindowList_Broadcast(hWindowList, WM_CLOSE, 0, 0);
	WindowList_Broadcast(hDataWindowList, WM_CLOSE, 0, 0);
	SendMessage(hWndSetup, WM_CLOSE, 0, 0);

	return 0;
}

int OnToolbarLoaded(WPARAM, LPARAM)
{
	TTBButton ttb = { 0 };
	ttb.name = LPGEN("Enable/disable auto update");
	ttb.pszService = MS_WEATHER_ENABLED;
	ttb.pszTooltipUp = LPGEN("Auto Update Enabled");
	ttb.pszTooltipDn = LPGEN("Auto Update Disabled");
	ttb.hIconHandleUp = GetIconHandle("main");
	ttb.hIconHandleDn = GetIconHandle("disabled");
	ttb.dwFlags = (db_get_b(NULL, WEATHERPROTONAME, "AutoUpdate", 1) ? 0 : TTBBF_PUSHED) | TTBBF_ASPUSHBUTTON | TTBBF_VISIBLE;
	hTBButton = TopToolbar_AddButton(&ttb);
	return 0;
}

// weather protocol initialization function
// run after the event ME_SYSTEM_MODULESLOADED occurs
int WeatherInit(WPARAM, LPARAM)
{
	// initialize netlib
	NetlibInit();

	InitMwin();

	// load weather menu items
	AddMenuItems();

	// timer for the first update
	timerId = SetTimer(NULL, 0, 5000, timerProc2);  // first update is 5 sec after load

	// weather user detail
	HookEvent(ME_USERINFO_INITIALISE, UserInfoInit);
	HookEvent(ME_TTB_MODULELOADED, OnToolbarLoaded);
	return 0;
}

//============  MISC FUNCTIONS  ============

// initialize the global variables at startup
void InitVar()
{
	// setup the linklist for weather update list
	UpdateListTail = NULL;
	UpdateListHead = NULL;

	// other settings
	timerId = 0;
	opt.DefStn = NULL;
	ModuleLoaded = FALSE;
}

// unload function
extern "C" int __declspec(dllexport) Unload(void)
{
	DestroyMwin();
	DestroyWindow(hPopupWindow);

	DestroyHookableEvent(hHookWeatherUpdated);
	DestroyHookableEvent(hHookWeatherError);

	NetlibHttpDisconnect();
	Netlib_CloseHandle(hNetlibUser);

	DestroyUpdateList();
	DestroyOptions();
	DestroyWIList();				// unload all ini data from memory

	WindowList_Destroy(hDataWindowList);
	WindowList_Destroy(hWindowList);

	CloseHandle(hUpdateMutex);
	return 0;
}

extern "C" int __declspec(dllexport) Load(void)
{
	mir_getLP(&pluginInfoEx);
	pcli = Clist_GetInterface();

	// initialize global variables
	InitVar();

	// load options and set defaults
	LoadOptions();

	InitIcons();

	// reset the weather data at startup for individual contacts
	EraseAllInfo();

	// load weather update data
	LoadWIData(true);

	// set status to online if "Do not display weather condition as protocol status" is enabled
	old_status = status = ID_STATUS_OFFLINE;

	// add an event on weather update and error
	hHookWeatherUpdated = CreateHookableEvent(ME_WEATHER_UPDATED);
	hHookWeatherError = CreateHookableEvent(ME_WEATHER_ERROR);

	// initialize options and network
	HookEvent(ME_OPT_INITIALISE, OptInit);
	HookEvent(ME_SYSTEM_MODULESLOADED, WeatherInit);
	HookEvent(ME_DB_CONTACT_DELETED, ContactDeleted);
	HookEvent(ME_CLIST_DOUBLECLICKED, BriefInfo);
	HookEvent(ME_WEATHER_UPDATED, WeatherPopup);
	HookEvent(ME_WEATHER_ERROR, WeatherError);
	HookEvent(ME_SYSTEM_PRESHUTDOWN, WeatherShutdown);
	HookEvent(ME_CLIST_PREBUILDCONTACTMENU, BuildContactMenu);

	hDataWindowList = WindowList_Create();
	hWindowList = WindowList_Create();

	hUpdateMutex = CreateMutex(NULL, FALSE, NULL);

	// register weather protocol
	PROTOCOLDESCRIPTOR pd = { 0 };
	pd.cbSize = sizeof(pd);
	pd.szName = WEATHERPROTONAME;
	pd.type = (opt.NoProtoCondition) ? PROTOTYPE_VIRTUAL : PROTOTYPE_PROTOCOL;
	Proto_RegisterModule(&pd);

	// initialize weather protocol services
	InitServices();

	// add sound event
	SkinAddNewSoundExW("weatherupdated", _A2W(WEATHERPROTONAME), LPGENW("Weather Condition Changed"));
	SkinAddNewSoundExW("weatheralert", _A2W(WEATHERPROTONAME), LPGENW("Weather Alert Issued"));

	// window needed for popup commands
	wchar_t SvcFunc[100];
	mir_snwprintf(SvcFunc, L"%s__PopupWindow", _A2W(WEATHERPROTONAME));
	hPopupWindow = CreateWindowEx(WS_EX_TOOLWINDOW, L"static", SvcFunc, 0, CW_USEDEFAULT, CW_USEDEFAULT,
		CW_USEDEFAULT, CW_USEDEFAULT, HWND_DESKTOP, NULL, hInst, NULL);
	SetWindowLongPtr(hPopupWindow, GWLP_WNDPROC, (LONG_PTR)PopupWndProc);
	return 0;
}
