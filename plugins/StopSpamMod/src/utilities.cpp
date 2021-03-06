/* Copyright (C) Miklashevsky Roman, sss, elzor
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "stdafx.h"

wstring DBGetContactSettingStringPAN(MCONTACT hContact, char const * szModule, char const * szSetting, wstring errorValue)
{
	DBVARIANT dbv;
	//if(db_get(hContact, szModule, szSetting, &dbv))
	if (db_get_ws(hContact, szModule, szSetting, &dbv))
		return errorValue;
	//	if(DBVT_WCHAR == dbv.type )
	errorValue = dbv.ptszVal;
	db_free(&dbv);
	return errorValue;
}

std::string DBGetContactSettingStringPAN_A(MCONTACT hContact, char const * szModule, char const * szSetting, std::string errorValue)
{
	DBVARIANT dbv;
	//if(db_get(hContact, szModule, szSetting, &dbv))
	if (db_get_s(hContact, szModule, szSetting, &dbv))
		return errorValue;
	//	if(DBVT_ASCIIZ == dbv.type )
	errorValue = dbv.pszVal;
	db_free(&dbv);
	return errorValue;
}

wstring &GetDlgItemString(HWND hwnd, int id)
{
	HWND h = GetDlgItem(hwnd, id);
	int len = GetWindowTextLength(h);
	wchar_t * buf = new wchar_t[len + 1];
	GetWindowText(h, buf, len + 1);
	static wstring s;
	s = buf;
	delete[]buf;
	return s;
}

std::string &GetProtoList()
{
	static std::string s;
	return s = DBGetContactSettingStringPAN_A(NULL, pluginName, "protoList", "ICQ\r\n");
}

bool ProtoInList(std::string proto)
{
	return std::string::npos != GetProtoList().find(proto + "\r\n");
}

void DeleteCListGroupsByName(wchar_t* szGroupName)
{
	BYTE ConfirmDelete = db_get_b(NULL, "CList", "ConfirmDelete", SETTING_CONFIRMDELETE_DEFAULT);
	if (ConfirmDelete)
		db_set_b(NULL, "CList", "ConfirmDelete", 0);

	wchar_t *szGroup;
	for (int i = 1; (szGroup = Clist_GroupGetName(i, NULL)) != NULL; i++)
		if (!mir_wstrcmp(szGroupName, szGroup))
			Clist_GroupDelete(i);

	if (ConfirmDelete)
		db_set_b(NULL, "CList", "ConfirmDelete", ConfirmDelete);
}

int RemoveTmp(WPARAM, LPARAM)
{
	void CleanThread();
	CleanThread();
	return 0;
}

wstring variables_parse(wstring const &tstrFormat, MCONTACT hContact)
{
	if (gbVarsServiceExist) {
		FORMATINFO fi;
		wchar_t *tszParsed;
		wstring tstrResult;

		memset(&fi, 0, sizeof(fi));
		fi.cbSize = sizeof(fi);
		fi.tszFormat = wcsdup(tstrFormat.c_str());
		fi.hContact = hContact;
		fi.flags |= FIF_TCHAR;
		tszParsed = (wchar_t *)CallService(MS_VARS_FORMATSTRING, (WPARAM)&fi, 0);
		free(fi.tszFormat);
		if (tszParsed) {
			tstrResult = tszParsed;
			mir_free(tszParsed);
			return tstrResult;
		}
	}
	return tstrFormat;
}

// case-insensitive mir_wstrcmp
//by nullbie as i remember...
#define NEWTSTR_MALLOC(A) (A==NULL) ? NULL : mir_wstrcpy((wchar_t*)mir_alloc(sizeof(wchar_t)*(mir_wstrlen(A)+1)),A)
const int Stricmp(const wchar_t *str, const wchar_t *substr)
{
	int i = 0;

	wchar_t *str_up = NEWTSTR_MALLOC(str);
	wchar_t *substr_up = NEWTSTR_MALLOC(substr);

	CharUpperBuff(str_up, (int)mir_wstrlen(str_up));
	CharUpperBuff(substr_up, (int)mir_wstrlen(substr_up));

	i = mir_wstrcmp(str_up, substr_up);

	mir_free(str_up);
	mir_free(substr_up);

	return i;
}

wchar_t* ReqGetText(DBEVENTINFO* dbei)
{
	if (!dbei->pBlob)
		return 0;

	char * ptr = (char *)&dbei->pBlob[sizeof(DWORD) * 2];
	int len = dbei->cbBlob - sizeof(DWORD) * 2;
	int i = 0;

	while (len && (i < 4)) {
		if (!ptr[0]) i++;
		ptr++;
		len--;
	};

	if (len) {
		char * tstr = (char *)mir_alloc(len + 1);
		memcpy(tstr, ptr, len);
		tstr[len] = 0;
		WCHAR* msg = NULL;
		msg = (dbei->flags&DBEF_UTF) ? mir_utf8decodeW(tstr) : mir_a2u(tstr);
		mir_free(tstr);
		return (wchar_t *)msg;
	};
	return 0;
}

BOOL IsUrlContains(wchar_t * Str)
{
	const int CountUrl = 11;
	const wchar_t  URL[CountUrl][5] =
	{
		L"http",
		L"www",
		L".ru",
		L".com",
		L".de",
		L".cz",
		L".org",
		L".net",
		L".su",
		L".ua",
		L".tv"
	};

	if (Str && mir_wstrlen(Str) > 0) {
		wchar_t *StrLower = NEWTSTR_MALLOC(Str);
		CharLowerBuff(StrLower, (int)mir_wstrlen(StrLower));
		for (int i = 0; i < CountUrl; i++)
			if (wcsstr(StrLower, URL[i])) {
				mir_free(StrLower);
				return 1;
			}
		mir_free(StrLower);
	}
	return 0;
}

wstring GetContactUid(MCONTACT hContact, wstring Protocol)
{
	char *szProto = mir_utf8encodeW(Protocol.c_str());
	ptrW uid(Contact_GetInfo(CNF_DISPLAYUID, hContact, szProto));
	return (uid) ? uid : L"";
}

void LogSpamToFile(MCONTACT hContact, wstring message)
{
	if (!gbLogToFile) return;

	wstring LogStrW, LogTime, LogProtocol, LogContactId, LogContactName;
	std::fstream file;
	wchar_t pszName[MAX_PATH];

	if (hStopSpamLogDirH)
		FoldersGetCustomPathT(hStopSpamLogDirH, pszName, MAX_PATH, L"");
	else
		mir_wstrncpy(pszName, VARSW(L"%miranda_logpath%"), _countof(pszName));

	wstring filename = pszName;
	filename += L"\\stopspam_mod.log";
	file.open(filename.c_str(), std::ios::out | std::ios::app);

	// Time Log line
	time_t time_now;
	time(&time_now);

	struct tm *TimeNow = localtime(&time_now);
	LogTime = _wasctime(TimeNow);
	// Time Log line

	// Name, UID and Protocol Log line
	LogProtocol = DBGetContactSettingStringPAN(hContact, "Protocol", "p", L"");
	LogContactName = (wchar_t*)pcli->pfnGetContactDisplayName(hContact, 0);
	LogContactId = (LogProtocol == L"") ? L"" : GetContactUid(hContact, LogProtocol);
	// Name, UID  and Protocol Log line

	LogStrW = L"[" + LogTime.substr(0, LogTime.length() - 1) + L"] " +
		LogContactId + L" - " +
		LogContactName + L" (" +
		LogProtocol + L"): " +
		message + L"\n";

	char *buf = mir_u2a(LogStrW.c_str());
	file.write(buf, LogStrW.length());
	mir_free(buf);

	file.close();
}

boost::mutex clean_mutex;

void CleanProtocolTmpThread(std::string proto)
{
	while (true) {
		UINT status = CallProtoService(proto.c_str(), PS_GETSTATUS, 0, 0);
		if (status > ID_STATUS_OFFLINE)
			break;
		boost::this_thread::sleep(boost::posix_time::seconds(2));
	}

	std::list<MCONTACT> contacts;
	for (MCONTACT hContact = db_find_first(proto.c_str()); hContact; hContact = db_find_next(hContact, proto.c_str()))
		if (db_get_b(hContact, "CList", "NotOnList", 0) || (L"Not In List" == DBGetContactSettingStringPAN(hContact, "CList", "Group", L"")))
			contacts.push_back(hContact);

	boost::this_thread::sleep(boost::posix_time::seconds(5));
	clean_mutex.lock();
	std::list<MCONTACT>::iterator end = contacts.end();
	for (std::list<MCONTACT>::iterator i = contacts.begin(); i != end; ++i) {
		LogSpamToFile(*i, L"Deleted");
		HistoryLogFunc(*i, "Deleted");
		db_delete_contact(*i);
	}
	clean_mutex.unlock();
}

void CleanProtocolExclThread(std::string proto)
{
	while (true) {
		UINT status = CallProtoService(proto.c_str(), PS_GETSTATUS, 0, 0);
		if (status > ID_STATUS_OFFLINE)
			break;
		boost::this_thread::sleep(boost::posix_time::seconds(2));
	}

	std::list<MCONTACT> contacts;
	for (MCONTACT hContact = db_find_first(proto.c_str()); hContact; hContact = db_find_next(hContact, proto.c_str()))
		if (db_get_b(hContact, "CList", "NotOnList", 0) && db_get_b(hContact, pluginName, "Excluded", 0))
			contacts.push_back(hContact);

	boost::this_thread::sleep(boost::posix_time::seconds(5));
	clean_mutex.lock();
	std::list<MCONTACT>::iterator end = contacts.end();
	for (std::list<MCONTACT>::iterator i = contacts.begin(); i != end; ++i) {
		LogSpamToFile(*i, L"Deleted");
		HistoryLogFunc(*i, "Deleted");
		db_delete_contact(*i);
	}
	clean_mutex.unlock();
}

void CleanThread()
{
	std::list<std::string> protocols;
	int count = 0;
	PROTOACCOUNT **accounts;
	Proto_EnumAccounts(&count, &accounts);
	for (int i = 0; i < count; i++)
		if (!strstr(accounts[i]->szModuleName, "MetaContacts") && !strstr(accounts[i]->szModuleName, "Weather")) //not real protocols
			protocols.push_back(accounts[i]->szModuleName);

	std::list<std::string>::iterator end = protocols.end();
	for (std::list<std::string>::iterator i = protocols.begin(); i != end; ++i) {
		if (gbDelAllTempory)
			new boost::thread(boost::bind(&CleanProtocolTmpThread, *i));
		if (gbDelExcluded)
			new boost::thread(boost::bind(&CleanProtocolExclThread, *i));
	}
}

void HistoryLog(MCONTACT hContact, char *data, int event_type, int flags)
{
	DBEVENTINFO Event = { sizeof(Event) };
	Event.szModule = pluginName;
	Event.eventType = event_type;
	Event.flags = flags | DBEF_UTF;
	Event.timestamp = (DWORD)time(NULL);
	Event.cbBlob = (DWORD)mir_strlen(data) + 1;
	Event.pBlob = (PBYTE)_strdup(data);
	db_event_add(hContact, &Event);
}

void HistoryLogFunc(MCONTACT hContact, std::string message)
{
	if (gbHistoryLog) {
		if (hContact == INVALID_CONTACT_ID)
			return;

		std::string msg = message;
		msg.append("\n");
		msg.append("Protocol: ").append(GetContactProto(hContact)).append(" Contact: ");
		msg.append(toUTF8((wchar_t*)pcli->pfnGetContactDisplayName(hContact, 0))).append(" ID: ");
		msg.append(toUTF8(GetContactUid(hContact, toUTF16(GetContactProto(hContact)))));
		HistoryLog(NULL, (char*)msg.c_str(), EVENTTYPE_MESSAGE, DBEF_READ);
	}
}

std::string toUTF8(std::wstring str)
{
	std::string ustr;
	try {
		utf8::utf16to8(str.begin(), str.end(), back_inserter(ustr));
	}
	catch (const std::exception&) {
		//TODO: handle utf8cpp exceptions
	}
	return ustr;
}

std::string toUTF8(std::string str)
{
	return toUTF8(toUTF16(str));
}

std::wstring toUTF16(std::string str) //convert as much as possible
{
	std::wstring ustr;
	std::string tmpstr;
	try {
		utf8::replace_invalid(str.begin(), str.end(), back_inserter(tmpstr));
		utf8::utf8to16(tmpstr.begin(), tmpstr.end(), back_inserter(ustr));
	}
	catch (const std::exception &) {
		//TODO: handle utf8cpp exceptions
	}
	return ustr;
}

std::string get_random_num(int length)
{
	std::string chars("123456789");
	std::string data;
	boost::random_device rng;
	boost::variate_generator<boost::random_device&, boost::uniform_int<>> gen(rng, boost::uniform_int<>(0, (int)chars.length() - 1));
	for (int i = 0; i < length; ++i)
		data += chars[gen()];
	return data;
}
