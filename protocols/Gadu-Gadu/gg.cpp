////////////////////////////////////////////////////////////////////////////////
// Gadu-Gadu Plugin for Miranda IM
//
// Copyright (c) 2003-2009 Adam Strzelecki <ono+miranda@java.pl>
// Copyright (c) 2009-2012 Bartosz Bia�ek
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
////////////////////////////////////////////////////////////////////////////////

#include "gg.h"
#include "version.h"
#include <errno.h>

// Plugin info
PLUGININFOEX pluginInfo = {
	sizeof(PLUGININFOEX),
	"Gadu-Gadu Protocol",
	__VERSION_DWORD,
	"Provides support for Gadu-Gadu protocol",
	"Bartosz Bia�ek, Adam Strzelecki",
	"dezred"/*antispam*/"@"/*antispam*/"gmail"/*antispam*/"."/*antispam*/"com",
	"� 2009-2012 Bartosz Bia�ek, 2003-2009 Adam Strzelecki",
	"http://www.miranda-im.pl/",
	UNICODE_AWARE,
	// {F3FF65F3-250E-416A-BEE9-58C93F85AB33}
	{ 0xf3ff65f3, 0x250e, 0x416a, { 0xbe, 0xe9, 0x58, 0xc9, 0x3f, 0x85, 0xab, 0x33 } }
};
static const MUUID interfaces[] = {MIID_PROTOCOL, MIID_LAST};

// Other variables
HINSTANCE hInstance;

XML_API xi;
SSL_API si;
CLIST_INTERFACE *pcli;
int hLangpack;
list_t g_Instances;

// Event hooks
static HANDLE hHookModulesLoaded = NULL;
static HANDLE hHookPreShutdown = NULL;

static unsigned long crc_table[256];

//////////////////////////////////////////////////////////
// Extra winsock function for error description

TCHAR* ws_strerror(int code)
{
	static TCHAR err_desc[160];

	// Not a windows error display WinSock
	if (code == 0)
	{
		TCHAR buff[128];
		int len = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, WSAGetLastError(), 0, buff, SIZEOF(buff), NULL);
		if (len == 0)
			mir_sntprintf(err_desc, SIZEOF(err_desc), _T("WinSock %u: Unknown error."), WSAGetLastError());
		else
			mir_sntprintf(err_desc, SIZEOF(err_desc), _T("WinSock %d: %s"), WSAGetLastError(), buff);
		return err_desc;
	}

	// Return normal error
	return _tcserror(code);
}

//////////////////////////////////////////////////////////
// Build the crc table
void crc_gentable(void)
{
	unsigned long crc, poly;
	int	i, j;

	poly = 0xEDB88320L;
	for (i = 0; i < 256; i++)
	{
		crc = i;
		for (j = 8; j > 0; j--)
		{
			if (crc & 1)
				crc = (crc >> 1) ^ poly;
			else
				crc >>= 1;
		}
		crc_table[i] = crc;
	}
}

//////////////////////////////////////////////////////////
// Calculate the crc value
unsigned long crc_get(char *mem)
{
	register unsigned long crc = 0xFFFFFFFF;
	while(mem && *mem)
		crc = ((crc>>8) & 0x00FFFFFF) ^ crc_table[(crc ^ *(mem++)) & 0xFF];

	return (crc ^ 0xFFFFFFFF);
}

//////////////////////////////////////////////////////////
// http_error_string()
//
// returns http error text
const char *http_error_string(int h)
{
	switch (h)
	{
		case 0:
			return Translate((errno == ENOMEM) ? "HTTP failed memory" : "HTTP failed connecting");
		case GG_ERROR_RESOLVING:
			return Translate("HTTP failed resolving");
		case GG_ERROR_CONNECTING:
			return Translate("HTTP failed connecting");
		case GG_ERROR_READING:
			return Translate("HTTP failed reading");
		case GG_ERROR_WRITING:
			return Translate("HTTP failed writing");
	}

	return Translate("Unknown HTTP error");
}

//////////////////////////////////////////////////////////
// Gets plugin info

extern "C" __declspec(dllexport) PLUGININFOEX *MirandaPluginInfoEx(DWORD mirandaVersion)
{
	return &pluginInfo;
}

extern "C" __declspec(dllexport) const MUUID* MirandaPluginInterfaces(void)
{
	return interfaces;
}

//////////////////////////////////////////////////////////
// Cleanups from last plugin

void GGPROTO::cleanuplastplugin(DWORD version)
{
	HANDLE hContact;
	char *szProto;

	// Remove bad e-mail and phones from
	if (version < PLUGIN_MAKE_VERSION(0, 0, 1, 4))
	{
#ifdef DEBUGMODE
		netlog("gg_cleanuplastplugin(%d): Cleaning junk Phone settings from < 0.0.1.4 ...", version);
#endif
		// Look for contact in DB
		hContact = (HANDLE) CallService(MS_DB_CONTACT_FINDFIRST, 0, 0);
		while (hContact)
		{
			szProto = (char *) CallService(MS_PROTO_GETCONTACTBASEPROTO, (WPARAM) hContact, 0);
			if (szProto != NULL && !strcmp(szProto, m_szModuleName))
			{
				// Do contact cleanup
				db_unset(hContact, m_szModuleName, GG_KEY_EMAIL);
				db_unset(hContact, m_szModuleName, "Phone");
			}
			hContact = (HANDLE) CallService(MS_DB_CONTACT_FINDNEXT, (WPARAM) hContact, 0);
		}
	}

	// Remove GG entries for non GG contacts
	if (version < PLUGIN_MAKE_VERSION(0, 0, 3, 5))
	{
#ifdef DEBUGMODE
		netlog("gg_cleanuplastplugin(%d): Cleaning junk Nick settings from < 0.0.3.5 ...", version);
#endif
		// Look for contact in DB
		hContact = (HANDLE) CallService(MS_DB_CONTACT_FINDFIRST, 0, 0);
		while (hContact)
		{
			szProto = (char *) CallService(MS_PROTO_GETCONTACTBASEPROTO, (WPARAM) hContact, 0);
			if (szProto != NULL && strcmp(szProto, m_szModuleName))
			{
				// Do nick entry cleanup
				db_unset(hContact, m_szModuleName, GG_KEY_NICK);
			}
			hContact = (HANDLE) CallService(MS_DB_CONTACT_FINDNEXT, (WPARAM) hContact, 0);
		}
	}

	// Remove old unneeded entry
	if (version < PLUGIN_MAKE_VERSION(0, 0, 5, 3))
		db_unset(NULL, m_szModuleName, "ShowNotOnMyList");

	// Store this plugin version
	db_set_dw(NULL, m_szModuleName, GG_PLUGINVERSION, pluginInfo.version);
}

//////////////////////////////////////////////////////////
// When Miranda loaded its modules
static int gg_modulesloaded(WPARAM wParam, LPARAM lParam)
{
	// Get SSL API
	mir_getSI(&si);

	// File Association Manager support
	gg_links_init();

	return 0;
}

//////////////////////////////////////////////////////////
// When Miranda starting shutdown sequence
static int gg_preshutdown(WPARAM wParam, LPARAM lParam)
{
	gg_links_destroy();

	return 0;
}

//////////////////////////////////////////////////////////
// Gets protocol instance associated with a contact
static GGPROTO* gg_getprotoinstance(HANDLE hContact)
{
	char* szProto = (char*)CallService(MS_PROTO_GETCONTACTBASEPROTO, (WPARAM)hContact, 0);
	list_t l = g_Instances;

	if (szProto == NULL)
		return NULL;

	for (; l; l = l->next)
	{
		GGPROTO* gg = (GGPROTO*)l->data;
		if (strcmp(szProto, gg->m_szModuleName) == 0)
			return gg;
	}

	return NULL;
}

//////////////////////////////////////////////////////////
// Handles PrebuildContactMenu event
static int gg_prebuildcontactmenu(WPARAM wParam, LPARAM lParam)
{
	const HANDLE hContact = (HANDLE)wParam;
	CLISTMENUITEM mi = {0};
	GGPROTO* gg = gg_getprotoinstance(hContact);

	if (gg == NULL)
		return 0;

	mi.cbSize = sizeof(mi);
	mi.flags = CMIM_NAME | CMIM_FLAGS | CMIF_ICONFROMICOLIB;
	if ( db_get_dw(hContact, gg->m_szModuleName, GG_KEY_UIN, 0) == db_get_b(NULL, gg->m_szModuleName, GG_KEY_UIN, 0) ||
		db_get_b(hContact, gg->m_szModuleName, "ChatRoom", 0) ||
		db_get_b(hContact, "CList", "NotOnList", 0))
		mi.flags |= CMIF_HIDDEN;
	mi.pszName = db_get_b(hContact, gg->m_szModuleName, GG_KEY_BLOCK, 0) ? LPGEN("&Unblock") : LPGEN("&Block");
	CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM)gg->hBlockMenuItem, (LPARAM)&mi);

	return 0;
}

//////////////////////////////////////////////////////////
// Contact block service function
INT_PTR GGPROTO::blockuser(WPARAM wParam, LPARAM lParam)
{
	const HANDLE hContact = (HANDLE)wParam;
	db_set_b(hContact, m_szModuleName, GG_KEY_BLOCK, !db_get_b(hContact, m_szModuleName, GG_KEY_BLOCK, 0));
	notifyuser(hContact, 1);
	return 0;
}


//////////////////////////////////////////////////////////
// Contact blocking initialization

#define GGS_BLOCKUSER "%s/BlockUser"
void GGPROTO::block_init()
{
	CLISTMENUITEM mi = {0};
	char service[64];

	mi.cbSize = sizeof(mi);
	mi.flags = CMIF_ICONFROMICOLIB;

	mir_snprintf(service, sizeof(service), GGS_BLOCKUSER, m_szModuleName);
	createProtoService(service, &GGPROTO::blockuser);
	mi.position = -500050000;
	mi.icolibItem = GetIconHandle(IDI_BLOCK);
	mi.pszName = LPGEN("&Block");
	mi.pszService = service;
	mi.pszContactOwner = m_szModuleName;
	hBlockMenuItem = Menu_AddContactMenuItem(&mi);

	hPrebuildMenuHook = HookEvent(ME_CLIST_PREBUILDCONTACTMENU, gg_prebuildcontactmenu);
}

//////////////////////////////////////////////////////////
// Contact blocking uninitialization

void GGPROTO::block_uninit()
{
	UnhookEvent(hPrebuildMenuHook);
	CallService(MS_CLIST_REMOVECONTACTMENUITEM, (WPARAM)hBlockMenuItem, 0);
}

//////////////////////////////////////////////////////////
// Menus initialization
void GGPROTO::menus_init()
{
	HGENMENU hGCRoot, hCLRoot, hRoot = MO_GetProtoRootMenu(m_szModuleName);
	CLISTMENUITEM mi = {0};

	mi.cbSize = sizeof(mi);
	if (hRoot == NULL)
	{
		mi.ptszName = m_tszUserName;
		mi.position = 500090000;
		mi.hParentMenu = HGENMENU_ROOT;
		mi.flags = CMIF_ICONFROMICOLIB | CMIF_ROOTPOPUP | CMIF_TCHAR | CMIF_KEEPUNTRANSLATED;
		mi.icolibItem = GetIconHandle(IDI_GG);
		hGCRoot = hCLRoot = hRoot = hMenuRoot = Menu_AddProtoMenuItem(&mi);
	}
	else
	{
		mi.hParentMenu = hRoot;
		mi.flags = CMIF_ICONFROMICOLIB | CMIF_ROOTHANDLE | CMIF_TCHAR;

		mi.ptszName = LPGENT("Conference");
		mi.position = 200001;
		mi.icolibItem = GetIconHandle(IDI_CONFERENCE);
		hGCRoot = Menu_AddProtoMenuItem(&mi);

		mi.ptszName = LPGENT("Contact list");
		mi.position = 200002;
		mi.icolibItem = GetIconHandle(IDI_LIST);
		hCLRoot = Menu_AddProtoMenuItem(&mi);

		if (hMenuRoot)
			CallService(MS_CLIST_REMOVEMAINMENUITEM, (WPARAM)hMenuRoot, 0);
		hMenuRoot = NULL;
	}

	gc_menus_init(hGCRoot);
	import_init(hCLRoot);
	sessions_menus_init(hRoot);
}

//////////////////////////////////////////////////////////
// Module instance initialization

static GGPROTO *gg_proto_init(const char* pszProtoName, const TCHAR* tszUserName)
{
	GGPROTO *gg = new GGPROTO(pszProtoName, tszUserName);
	list_add(&g_Instances, gg, 0);
	return gg;
}

//////////////////////////////////////////////////////////
// Module instance uninitialization

static int gg_proto_uninit(PROTO_INTERFACE *proto)
{
	GGPROTO *gg = (GGPROTO *)proto;
	list_remove(&g_Instances, gg, 0);
	delete gg;
	return 0;
}

//////////////////////////////////////////////////////////
// When plugin is loaded

extern "C" int __declspec(dllexport) Load(void)
{
	mir_getXI(&xi);
	mir_getLP(&pluginInfo);

	pcli = (CLIST_INTERFACE*)CallService(MS_CLIST_RETRIEVE_INTERFACE, 0, (LPARAM)hInstance);

	// Hook system events
	hHookModulesLoaded = HookEvent(ME_SYSTEM_MODULESLOADED, gg_modulesloaded);
	hHookPreShutdown = HookEvent(ME_SYSTEM_PRESHUTDOWN, gg_preshutdown);

	// Prepare protocol name
	PROTOCOLDESCRIPTOR pd = { 0 };
	pd.cbSize = sizeof(pd);
	pd.szName = GGDEF_PROTO;
	pd.fnInit = (pfnInitProto)gg_proto_init;
	pd.fnUninit = (pfnUninitProto)gg_proto_uninit;
	pd.type = PROTOTYPE_PROTOCOL;

	// Register module
	CallService(MS_PROTO_REGISTERMODULE, 0, (LPARAM) &pd);
	gg_links_instancemenu_init();

	// Instance list
	g_Instances = NULL;

	return 0;
}

//////////////////////////////////////////////////////////
// When plugin is unloaded

extern "C" int __declspec(dllexport) Unload()
{
	LocalEventUnhook(hHookModulesLoaded);
	LocalEventUnhook(hHookPreShutdown);

	// Cleanup WinSock
	WSACleanup();
	return 0;
}

//////////////////////////////////////////////////////////
// DEBUGING FUNCTIONS
struct
{
	int type;
	char *text;
}
static const ggdebug_eventype2string[] =
{
	{GG_EVENT_NONE,					"GG_EVENT_NONE"},
	{GG_EVENT_MSG,					"GG_EVENT_MSG"},
	{GG_EVENT_NOTIFY,				"GG_EVENT_NOTIFY"},
	{GG_EVENT_NOTIFY_DESCR,			"GG_EVENT_NOTIFY_DESCR"},
	{GG_EVENT_STATUS,				"GG_EVENT_STATUS"},
	{GG_EVENT_ACK,					"GG_EVENT_ACK"},
	{GG_EVENT_PONG,					"GG_EVENT_PONG"},
	{GG_EVENT_CONN_FAILED,			"GG_EVENT_CONN_FAILED"},
	{GG_EVENT_CONN_SUCCESS,			"GG_EVENT_CONN_SUCCESS"},
	{GG_EVENT_DISCONNECT,			"GG_EVENT_DISCONNECT"},
	{GG_EVENT_DCC_NEW,				"GG_EVENT_DCC_NEW"},
	{GG_EVENT_DCC_ERROR,			"GG_EVENT_DCC_ERROR"},
	{GG_EVENT_DCC_DONE,				"GG_EVENT_DCC_DONE"},
	{GG_EVENT_DCC_CLIENT_ACCEPT,	"GG_EVENT_DCC_CLIENT_ACCEPT"},
	{GG_EVENT_DCC_CALLBACK,			"GG_EVENT_DCC_CALLBACK"},
	{GG_EVENT_DCC_NEED_FILE_INFO,	"GG_EVENT_DCC_NEED_FILE_INFO"},
	{GG_EVENT_DCC_NEED_FILE_ACK,	"GG_EVENT_DCC_NEED_FILE_ACK"},
	{GG_EVENT_DCC_NEED_VOICE_ACK,	"GG_EVENT_DCC_NEED_VOICE_ACK"},
	{GG_EVENT_DCC_VOICE_DATA,		"GG_EVENT_DCC_VOICE_DATA"},
	{GG_EVENT_PUBDIR50_SEARCH_REPLY,"GG_EVENT_PUBDIR50_SEARCH_REPLY"},
	{GG_EVENT_PUBDIR50_READ,		"GG_EVENT_PUBDIR50_READ"},
	{GG_EVENT_PUBDIR50_WRITE,		"GG_EVENT_PUBDIR50_WRITE"},
	{GG_EVENT_STATUS60,				"GG_EVENT_STATUS60"},
	{GG_EVENT_NOTIFY60,				"GG_EVENT_NOTIFY60"},
	{GG_EVENT_USERLIST,				"GG_EVENT_USERLIST"},
	{GG_EVENT_IMAGE_REQUEST,		"GG_EVENT_IMAGE_REQUEST"},
	{GG_EVENT_IMAGE_REPLY,			"GG_EVENT_IMAGE_REPLY"},
	{GG_EVENT_DCC_ACK,				"GG_EVENT_DCC_ACK"},
	{GG_EVENT_DCC7_NEW,				"GG_EVENT_DCC7_NEW"},
	{GG_EVENT_DCC7_ACCEPT,			"GG_EVENT_DCC7_ACCEPT"},
	{GG_EVENT_DCC7_REJECT,			"GG_EVENT_DCC7_REJECT"},
	{GG_EVENT_DCC7_CONNECTED,		"GG_EVENT_DCC7_CONNECTED"},
	{GG_EVENT_DCC7_ERROR,			"GG_EVENT_DCC7_ERROR"},
	{GG_EVENT_DCC7_DONE,			"GG_EVENT_DCC7_DONE"},
	{GG_EVENT_DCC7_PENDING,			"GG_EVENT_DCC7_PENDING"},
	{GG_EVENT_XML_EVENT,			"GG_EVENT_XML_EVENT"},
	{GG_EVENT_DISCONNECT_ACK,		"GG_EVENT_DISCONNECT_ACK"},
	{GG_EVENT_XML_ACTION,			"GG_EVENT_XML_ACTION"},
	{GG_EVENT_TYPING_NOTIFICATION,	"GG_EVENT_TYPING_NOTIFICATION"},
	{GG_EVENT_USER_DATA,			"GG_EVENT_USER_DATA"},
	{GG_EVENT_MULTILOGON_MSG,		"GG_EVENT_MULTILOGON_MSG"},
	{GG_EVENT_MULTILOGON_INFO,		"GG_EVENT_MULTILOGON_INFO"},
	{-1,							"<unknown event>"}
};

const char *ggdebug_eventtype(gg_event *e)
{
	int i;
	for(i = 0; ggdebug_eventype2string[i].type != -1; i++)
		if (ggdebug_eventype2string[i].type == e->type)
			return ggdebug_eventype2string[i].text;
	return ggdebug_eventype2string[i].text;
}

#ifdef DEBUGMODE
void gg_debughandler(int level, const char *format, va_list ap)
{
	char szText[1024], *szFormat = _strdup(format);
	// Kill end line
	char *nl = strrchr(szFormat, '\n');
	if (nl) *nl = 0;

	strncpy(szText, "[libgadu] \0", sizeof(szText));

	mir_vsnprintf(szText + strlen(szText), sizeof(szText) - strlen(szText), szFormat, ap);
	CallService(MS_NETLIB_LOG, (WPARAM) NULL, (LPARAM) szText);
	free(szFormat);
}
#endif

//////////////////////////////////////////////////////////
// Log funcion

int GGPROTO::netlog(const char *fmt, ...)
{
	va_list va;
	char szText[1024];

	va_start(va, fmt);
	mir_vsnprintf(szText, sizeof(szText), fmt, va);
	va_end(va);
	return CallService(MS_NETLIB_LOG, (WPARAM)netlib, (LPARAM) szText);
}

//////////////////////////////////////////////////////////
// main DLL function

BOOL APIENTRY DllMain(HINSTANCE hInst, DWORD reason, LPVOID reserved)
{
	crc_gentable();
	hInstance = hInst;
#ifdef DEBUGMODE
	gg_debug_handler = gg_debughandler;
#endif
	return TRUE;
}
