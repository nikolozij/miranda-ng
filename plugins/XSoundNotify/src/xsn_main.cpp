/*
Miranda plugin template, originally by Richard Hughes
http://miranda-icq.sourceforge.net/

This file is placed in the public domain. Anybody is free to use or
modify it as they wish with no restriction.
There is no warranty.
*/

#include "stdafx.h"

HINSTANCE hInst;
int hLangpack;
LIST<XSN_Data> XSN_Users(10, NumericKeySortT);
HGENMENU hChangeSound = NULL;
MWindowList hChangeSoundDlgList = NULL;
BYTE isIgnoreSound = 0, isOwnSound = 0;

CLIST_INTERFACE *pcli;
CHAT_MANAGER *pci;

PLUGININFOEX pluginInfo = {
	sizeof(PLUGININFOEX),
	__PLUGIN_NAME,
	PLUGIN_MAKE_VERSION(__MAJOR_VERSION, __MINOR_VERSION, __RELEASE_NUM, __BUILD_NUM),
	__DESCRIPTION,
	__AUTHOR,
	__AUTHOREMAIL,
	__COPYRIGHT,
	__AUTHORWEB,
	UNICODE_AWARE,
	// {08B86253-EC6E-4D09-B7A9-64ACDF0627B8}
	{ 0x8b86253, 0xec6e, 0x4d09, { 0xb7, 0xa9, 0x64, 0xac, 0xdf, 0x6, 0x27, 0xb8 } }
};

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD, LPVOID)
{
	hInst = hinstDLL;
	return TRUE;
}

extern "C" __declspec(dllexport) PLUGININFOEX* MirandaPluginInfoEx(DWORD)
{
	return &pluginInfo;
}

/////////////////////////////////////////////////////////////////////////////////////////

struct {
	int iStatus;
	const char *szName;
}
static selfSounds[] =
{
	{ ID_STATUS_OFFLINE, "SelfOffline" },
	{ ID_STATUS_ONLINE, "SelfOnline" },
	{ ID_STATUS_AWAY, "SelfAway" },
	{ ID_STATUS_DND, "SelfDND" },
	{ ID_STATUS_NA, "SelfNA" },
	{ ID_STATUS_OCCUPIED, "SelfOccupied" },
	{ ID_STATUS_FREECHAT, "SelfFreeForChat" },
	{ ID_STATUS_INVISIBLE, "SelfInvisible" },
	{ ID_STATUS_ONTHEPHONE, "SelfOnThePhone" },
	{ ID_STATUS_OUTTOLUNCH, "SelfOutToLunch" }
};

void InitSelfSounds()
{
	// initializing self sounds for protocols
	int protoCount = 0;
	PROTOACCOUNT** protos = 0;

	Proto_EnumAccounts(&protoCount, &protos);
	for (int i = 0; i < protoCount; i++) {
		for (int j = 0; j < _countof(selfSounds); j++) {
			char namebuf[128];
			mir_snprintf(namebuf, "%s%s", protos[i]->szModuleName, selfSounds[j].szName);

			wchar_t infobuf[256];
			mir_snwprintf(infobuf, L"%s [%s]", TranslateT("Self status"), protos[i]->tszAccountName);
			SkinAddNewSoundExW(namebuf, infobuf, pcli->pfnGetStatusModeDescription(selfSounds[j].iStatus, 0));
		}
	}
}

static int ProtoAck(WPARAM, LPARAM lParam)
{
	ACKDATA *ack = (ACKDATA*)lParam;
	if (ack != 0 && ack->szModule && ack->type == ACKTYPE_STATUS && ack->result == ACKRESULT_SUCCESS) {
		for (int i = 0; i < _countof(selfSounds); i++) {
			if (selfSounds[i].iStatus == ack->lParam) {
				char buf[128];
				mir_snprintf(buf, "%s%s", ack->szModule, selfSounds[i].szName);
				SkinPlaySound(buf);
				break;
			}
		}
	}
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////

static bool isReceiveMessage(MEVENT hDbEvent)
{
	DBEVENTINFO info = { sizeof(info) };
	db_event_get(hDbEvent, &info);
	// i don't understand why it works and how it works, but it works correctly - practice way (������� ����)
	// so, i think correct condition would be : eventType == EVENTTYPE_MESSAGE && info.flags & DBEF_READ, but it really isn't
	return !(((info.eventType != EVENTTYPE_MESSAGE) && !(info.flags & DBEF_READ)) || (info.flags & DBEF_SENT));
}

static int ProcessEvent(WPARAM hContact, LPARAM lParam)
{
	if (!isReceiveMessage(lParam))
		return 0;

	isIgnoreSound = db_get_b(hContact, SETTINGSNAME, SETTINGSIGNOREKEY, 0);
	DBVARIANT dbv;
	if (!isIgnoreSound && !db_get_ws(hContact, SETTINGSNAME, SETTINGSKEY, &dbv)) {
		wchar_t PlaySoundPath[MAX_PATH] = { 0 };
		PathToAbsoluteW(dbv.ptszVal, PlaySoundPath);
		isOwnSound = 0;
		SkinPlaySoundFile(PlaySoundPath);
		db_free(&dbv);
		isOwnSound = 1;
	}

	return 0;
}

static int ProcessChatEvent(WPARAM, LPARAM lParam)
{
	GCEVENT *gce = (GCEVENT*)lParam;
	if (gce == NULL || gce->pDest == NULL)
		return 0;

	GCDEST *gcd = (GCDEST*)gce->pDest;
	if (gcd->iType != GC_EVENT_MESSAGE)
		return 0;

	MCONTACT hContact = pci->FindRoom(gcd->pszModule, gcd->ptszID);
	if (hContact != 0) {
		ptrW nick(db_get_wsa(hContact, gcd->pszModule, "MyNick"));
		if (nick == NULL || gce->ptszText == NULL)
			return 0;
		if (wcsstr(gce->ptszText, nick)) {
			isIgnoreSound = db_get_b(hContact, SETTINGSNAME, SETTINGSIGNOREKEY, 0);
			DBVARIANT dbv;
			if (!isIgnoreSound && !db_get_ws(hContact, SETTINGSNAME, SETTINGSKEY, &dbv)) {
				wchar_t PlaySoundPath[MAX_PATH] = { 0 };
				PathToAbsoluteW(dbv.ptszVal, PlaySoundPath);
				isOwnSound = 0;
				SkinPlaySoundFile(PlaySoundPath);
				db_free(&dbv);
				isOwnSound = 1;
			}
		}
	}

	return 0;
}

static int OnPlaySound(WPARAM, LPARAM)
{
	if (isIgnoreSound)
		return 1;
	if (isOwnSound) {
		isOwnSound = 0;
		return 1;
	}

	return 0;
}

static int OnLoadInit(WPARAM, LPARAM)
{
	CMenuItem mi;
	SET_UID(mi, 0x5d72ca1f, 0xc52, 0x436d, 0x81, 0x47, 0x29, 0xf6, 0xc3, 0x28, 0xb5, 0xd1);
	mi.position = -0x7FFFFFFF;
	mi.flags = CMIF_UNICODE;
	mi.hIcolibItem = Skin_LoadIcon(SKINICON_OTHER_MIRANDA);
	mi.name.w = LPGENW("Custom contact sound");
	mi.pszService = "XSoundNotify/ContactMenuCommand";
	hChangeSound = Menu_AddContactMenuItem(&mi);

	InitSelfSounds();
	return 0;
}

static int PrebuildContactMenu(WPARAM wParam, LPARAM)
{
	MCONTACT hContact = wParam;
	if (hContact) {
		char *szProto = GetContactProto(hContact);
		PROTOACCOUNT *pa = Proto_GetAccount(szProto);
		Menu_ShowItem(hChangeSound, IsSuitableProto(pa));
	}
	return 0;
}

static int OnPreShutdown(WPARAM, LPARAM)
{
	WindowList_Broadcast(hChangeSoundDlgList, WM_CLOSE, 0, 0);
	return 0;
}

extern "C" int __declspec(dllexport) Load()
{
	mir_getLP(&pluginInfo);
	pci = Chat_GetInterface();
	pcli = Clist_GetInterface();

	CreateServiceFunction("XSoundNotify/ContactMenuCommand", ShowDialog);

	hChangeSoundDlgList = WindowList_Create();

	HookEvent(ME_PROTO_ACK, ProtoAck);
	HookEvent(ME_OPT_INITIALISE, OptInit);
	HookEvent(ME_DB_EVENT_ADDED, ProcessEvent);
	HookEvent(ME_GC_HOOK_EVENT, ProcessChatEvent);
	HookEvent(ME_SYSTEM_MODULESLOADED, OnLoadInit);
	HookEvent(ME_CLIST_PREBUILDCONTACTMENU, PrebuildContactMenu);
	HookEvent(ME_SYSTEM_PRESHUTDOWN, OnPreShutdown);
	HookEvent(ME_SKIN_PLAYINGSOUND, OnPlaySound);
	return 0;
}

extern "C" int __declspec(dllexport) Unload(void)
{
	WindowList_Destroy(hChangeSoundDlgList);
	return 0;
}
