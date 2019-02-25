/*

Jabber Protocol Plugin for Miranda NG

Copyright (c) 2002-04  Santithorn Bunchua
Copyright (c) 2005-12  George Hazan
Copyright (C) 2012-19 Miranda NG team

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
#include "jabber_iq.h"
#include "jabber_caps.h"

/////////////////////////////////////////////////////////////////////////////////////////
// Global definitions

enum {
	IDM_CANCEL,

	IDM_ROLE, IDM_AFFLTN,

	IDM_CONFIG, IDM_NICK, IDM_DESTROY, IDM_INVITE, IDM_BOOKMARKS, IDM_LEAVE, IDM_TOPIC,
	IDM_LST_PARTICIPANT, IDM_LST_MODERATOR,
	IDM_LST_MEMBER, IDM_LST_ADMIN, IDM_LST_OWNER, IDM_LST_BAN,

	IDM_MESSAGE, IDM_SLAP, IDM_VCARD, IDM_INFO, IDM_KICK,
	IDM_RJID, IDM_RJID_ADD, IDM_RJID_VCARD, IDM_RJID_COPY,
	IDM_SET_VISITOR, IDM_SET_PARTICIPANT, IDM_SET_MODERATOR,
	IDM_SET_NONE, IDM_SET_MEMBER, IDM_SET_ADMIN, IDM_SET_OWNER, IDM_SET_BAN,
	IDM_CPY_NICK, IDM_CPY_TOPIC, IDM_CPY_RJID, IDM_CPY_INROOMJID,

	IDM_LINK0, IDM_LINK1, IDM_LINK2, IDM_LINK3, IDM_LINK4, IDM_LINK5, IDM_LINK6, IDM_LINK7, IDM_LINK8, IDM_LINK9,

	IDM_PRESENCE_ONLINE = ID_STATUS_ONLINE,
	IDM_PRESENCE_AWAY = ID_STATUS_AWAY,
	IDM_PRESENCE_NA = ID_STATUS_NA,
	IDM_PRESENCE_DND = ID_STATUS_DND,
	IDM_PRESENCE_FREE4CHAT = ID_STATUS_FREECHAT,
};

struct TRoleOrAffiliationInfo
{
	int value;
	int id;
	wchar_t *title_en;
	int min_role;
	int min_affiliation;

	wchar_t *title;

	BOOL check(JABBER_RESOURCE_STATUS *me, JABBER_RESOURCE_STATUS *him)
	{
		if (me->m_affiliation == AFFILIATION_OWNER) return TRUE;
		if (me == him) return FALSE;
		if (me->m_affiliation <= him->m_affiliation) return FALSE;
		if (me->m_role < min_role) return FALSE;
		if (me->m_affiliation < min_affiliation) return FALSE;
		return TRUE;
	}
	
	void translate()
	{
		title = TranslateW(title_en);
	}
};

static TRoleOrAffiliationInfo sttAffiliationItems[] =
{
	{ AFFILIATION_NONE,   IDM_SET_NONE,   LPGENW("None"),   ROLE_NONE, AFFILIATION_ADMIN },
	{ AFFILIATION_MEMBER, IDM_SET_MEMBER, LPGENW("Member"), ROLE_NONE, AFFILIATION_ADMIN },
	{ AFFILIATION_ADMIN,  IDM_SET_ADMIN,  LPGENW("Admin"),  ROLE_NONE, AFFILIATION_OWNER },
	{ AFFILIATION_OWNER,  IDM_SET_OWNER,  LPGENW("Owner"),  ROLE_NONE, AFFILIATION_OWNER },
};

static TRoleOrAffiliationInfo sttRoleItems[] =
{
	{ ROLE_VISITOR,     IDM_SET_VISITOR,     LPGENW("Visitor"),     ROLE_MODERATOR, AFFILIATION_NONE  },
	{ ROLE_PARTICIPANT, IDM_SET_PARTICIPANT, LPGENW("Participant"), ROLE_MODERATOR, AFFILIATION_NONE  },
	{ ROLE_MODERATOR,   IDM_SET_MODERATOR,   LPGENW("Moderator"),   ROLE_MODERATOR, AFFILIATION_ADMIN },
};

/////////////////////////////////////////////////////////////////////////////////////////
// JabberGcInit - initializes the new chat

static const wchar_t *sttStatuses[] = { LPGENW("Visitors"), LPGENW("Participants"), LPGENW("Moderators"), LPGENW("Owners") };

int JabberGcGetStatus(JABBER_GC_AFFILIATION a, JABBER_GC_ROLE r)
{
	if (a == AFFILIATION_OWNER)
		return 3;

	switch (r) {
		case ROLE_MODERATOR:   return 2;
		case ROLE_PARTICIPANT: return 1;
	}
	return 0;
}

int JabberGcGetStatus(JABBER_RESOURCE_STATUS *r)
{
	return JabberGcGetStatus(r->m_affiliation, r->m_role);
}

int CJabberProto::GcInit(JABBER_LIST_ITEM *item)
{
	if (item->bChatActive)
		return 1;

	// translate string for menus (this can't be done in initializer)
	for (auto &it : sttAffiliationItems)
		it.translate();
	for (auto &it : sttRoleItems)
		it.translate();

	Utf2T wszJid(item->jid);
	ptrA szNick(JabberNickFromJID(item->jid));
	SESSION_INFO *si = Chat_NewSession(GCW_CHATROOM, m_szModuleName, wszJid, Utf2T(szNick));
	if (si != nullptr) {
		item->hContact = si->hContact;

		if (JABBER_LIST_ITEM *bookmark = ListGetItemPtr(LIST_BOOKMARK, item->jid)) {
			if (bookmark->name) {
				ptrW myHandle(db_get_wsa(si->hContact, "CList", "MyHandle"));
				if (myHandle == nullptr)
					db_set_ws(si->hContact, "CList", "MyHandle", bookmark->name);
			}
		}

		ptrA tszNick(getUStringA(si->hContact, "MyNick"));
		if (tszNick != nullptr) {
			if (!mir_strcmp(tszNick, szNick))
				delSetting(si->hContact, "MyNick");
			else
				setUString(si->hContact, "MyNick", item->nick);
		}
		else setUString(si->hContact, "MyNick", item->nick);

		ptrA passw(getUStringA(si->hContact, "Password"));
		if (mir_strcmp(passw, item->password)) {
			if (!item->password || !item->password[0])
				delSetting(si->hContact, "Password");
			else
				setUString(si->hContact, "Password", item->password);
		}
	}

	item->bChatActive = true;

	for (int i = _countof(sttStatuses) - 1; i >= 0; i--)
		Chat_AddGroup(si, TranslateW(sttStatuses[i]));

	Chat_Control(m_szModuleName, wszJid, (item->bAutoJoin && m_bAutoJoinHidden) ? WINDOW_HIDDEN : SESSION_INITDONE);
	Chat_Control(m_szModuleName, wszJid, SESSION_ONLINE);
	return 0;
}

void CJabberProto::GcLogShowInformation(JABBER_LIST_ITEM *item, pResourceStatus &user, TJabberGcLogInfoType type)
{
	if (!item || !user || (item->bChatActive != 2)) return;

	CMStringW buf;
	Utf2T wszRoomJid(item->jid), wszUserId(user->m_szResourceName);

	switch (type) {
	case INFO_BAN:
		if (m_bGcLogBans)
			buf.Format(TranslateT("User %s is now banned."), wszUserId.get());
		break;

	case INFO_STATUS:
		if (m_bGcLogStatuses) {
			wchar_t *ptszDescr = Clist_GetStatusModeDescription(user->m_iStatus, 0);
			if (user->m_szStatusMessage)
				buf.Format(TranslateT("User %s changed status to %s with message: %s"), wszUserId.get(), ptszDescr, user->m_szStatusMessage);
			else
				buf.Format(TranslateT("User %s changed status to %s"), wszUserId.get(), ptszDescr);
		}
		break;

	case INFO_CONFIG:
		if (m_bGcLogConfig)
			buf.Format(TranslateT("Room configuration was changed."));
		break;

	case INFO_AFFILIATION:
		if (m_bGcLogAffiliations) {
			wchar_t *name = nullptr;
			switch (user->m_affiliation) {
			case AFFILIATION_NONE:		name = TranslateT("None"); break;
			case AFFILIATION_MEMBER:	name = TranslateT("Member"); break;
			case AFFILIATION_ADMIN:		name = TranslateT("Admin"); break;
			case AFFILIATION_OWNER:		name = TranslateT("Owner"); break;
			case AFFILIATION_OUTCAST:	name = TranslateT("Outcast"); break;
			}
			if (name)
				buf.Format(TranslateT("Affiliation of %s was changed to '%s'."), wszUserId.get(), name);
		}
		break;

	case INFO_ROLE:
		if (m_bGcLogRoles) {
			wchar_t *name = nullptr;
			switch (user->m_role) {
			case ROLE_NONE:			name = TranslateT("None"); break;
			case ROLE_VISITOR:		name = TranslateT("Visitor"); break;
			case ROLE_PARTICIPANT:	name = TranslateT("Participant"); break;
			case ROLE_MODERATOR:    name = TranslateT("Moderator"); break;
			}

			if (name)
				buf.Format(TranslateT("Role of %s was changed to '%s'."), wszUserId.get(), name);
		}
		break;
	}

	if (!buf.IsEmpty()) {
		buf.Replace(L"%", L"%%");

		GCEVENT gce = { m_szModuleName, wszRoomJid, GC_EVENT_INFORMATION };
		gce.ptszNick = wszUserId;
		gce.ptszUID = wszUserId;
		gce.ptszText = buf;
		gce.dwFlags = GCEF_ADDTOLOG;
		gce.time = time(0);
		Chat_Event(&gce);
	}
}

void CJabberProto::GcLogUpdateMemberStatus(JABBER_LIST_ITEM *item, const char *resource, const char *nick, const char *jid, int action, const TiXmlElement *reason, int nStatusCode)
{
	int statusToSet = 0;

	const char *szReason = (reason) ? reason->GetText() : nullptr;
	if (szReason == nullptr) {
		if (nStatusCode == 322)
			szReason = TranslateU("because room is now members-only");
		else if (nStatusCode == 301)
			szReason = TranslateU("user banned");
	}

	ptrA myNick(mir_strdup(item->nick));
	if (myNick == nullptr)
		myNick = JabberNickFromJID(m_szJabberJID);

	Utf2T wszRoomJid(item->jid), wszNick(nick), wszUserId(resource), wszText(szReason), wszUserInfo(jid);
	GCEVENT gce = { m_szModuleName, wszRoomJid };
	gce.ptszNick = wszNick;
	gce.ptszUID = wszUserId;
	gce.ptszUserInfo = wszUserInfo;
	gce.ptszText = wszText;
	if (item->bChatActive == 2) {
		gce.dwFlags |= GCEF_ADDTOLOG;
		gce.time = time(0);
	}

	switch (gce.iType = action) {
	case GC_EVENT_PART:  break;
	case GC_EVENT_KICK:
		gce.ptszStatus = TranslateT("Moderator");
		break;
	
	default:
		mir_cslock lck(m_csLists);
		for (auto &JS : item->arResources) {
			if (!mir_strcmp(resource, JS->m_szResourceName)) {
				if (action != GC_EVENT_JOIN) {
					switch (action) {
					case 0:
						gce.iType = GC_EVENT_ADDSTATUS;
					case GC_EVENT_REMOVESTATUS:
						gce.dwFlags &= ~GCEF_ADDTOLOG;
					}
					gce.ptszText = TranslateT("Moderator");
				}
				gce.ptszStatus = TranslateW(sttStatuses[JabberGcGetStatus(JS)]);
				gce.bIsMe = (mir_strcmp(nick, myNick) == 0);
				statusToSet = JS->m_iStatus;
				break;
			}
		}
	}

	Chat_Event(&gce);

	if (statusToSet != 0) {
		int flags = GC_SSE_ONLYLISTED;
		if (statusToSet == ID_STATUS_AWAY || statusToSet == ID_STATUS_NA || statusToSet == ID_STATUS_DND)
			flags += GC_SSE_ONLINE;
		Chat_SetStatusEx(m_szModuleName, wszRoomJid, flags, wszNick);

		gce.iType = GC_EVENT_SETCONTACTSTATUS;
		gce.ptszText = wszNick;
		gce.ptszUID = wszUserId;
		gce.dwItemData = statusToSet;
		Chat_Event(&gce);
	}
}

void CJabberProto::GcQuit(JABBER_LIST_ITEM *item, int code, const TiXmlElement *reason)
{
	CMStringA szMessage;

	if (code != 307 && code != 301) {
		ptrA quitMessage(getUStringA("GcMsgQuit"));
		if (quitMessage != nullptr)
			szMessage = quitMessage;
		else
			szMessage = TranslateU(JABBER_GC_MSG_QUIT);
	}
	else {
		ptrA myNick(JabberNickFromJID(m_szJabberJID));
		GcLogUpdateMemberStatus(item, myNick, myNick, nullptr, GC_EVENT_KICK, reason);
	}

	Utf2T wszRoomJid(item->jid);
	if (code == 200)
		Chat_Terminate(m_szModuleName, wszRoomJid);
	else
		Chat_Control(m_szModuleName, wszRoomJid, SESSION_OFFLINE);

	db_unset(item->hContact, "CList", "Hidden");
	item->bChatActive = false;

	if (m_bJabberOnline) {
		char szPresenceTo[JABBER_MAX_JID_LEN];
		mir_snprintf(szPresenceTo, "%s/%s", item->jid, item->nick);

		m_ThreadInfo->send(
			XmlNode("presence") << XATTR("to", szPresenceTo) << XATTR("type", "unavailable")
			<< XCHILD("status", szMessage));

		ListRemove(LIST_CHATROOM, item->jid);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////
// Context menu hooks

static gc_item *sttFindGcMenuItem(int nItems, gc_item *items, DWORD id)
{
	for (int i = 0; i < nItems; i++)
		if (items[i].dwID == id)
			return items + i;
	
	return nullptr;
}

static void sttSetupGcMenuItem(int nItems, gc_item *items, DWORD id, bool disabled)
{
	for (int i = 0; i < nItems; i++)
		if (!id || (items[i].dwID == id))
			items[i].bDisabled = disabled;
}

static void sttShowGcMenuItem(int nItems, gc_item *items, DWORD id, int type)
{
	for (int i = 0; i < nItems; i++)
		if (!id || (items[i].dwID == id))
			items[i].uType = type;
}

static void sttSetupGcMenuItems(int nItems, gc_item *items, DWORD *ids, bool disabled)
{
	for (; *ids; ++ids)
		sttSetupGcMenuItem(nItems, items, *ids, disabled);
}

static void sttShowGcMenuItems(int nItems, gc_item *items, DWORD *ids, int type)
{
	for (; *ids; ++ids)
		sttShowGcMenuItem(nItems, items, *ids, type);
}

static gc_item sttLogListItems[] =
{
	{ LPGENW("Change &nickname"), IDM_NICK, MENU_ITEM },
	{ LPGENW("&Invite a user"), IDM_INVITE, MENU_ITEM },
	{ nullptr, 0, MENU_SEPARATOR },

	{ LPGENW("&Roles"), IDM_ROLE, MENU_NEWPOPUP },
	{ LPGENW("&Participant list"), IDM_LST_PARTICIPANT, MENU_POPUPITEM },
	{ LPGENW("&Moderator list"), IDM_LST_MODERATOR, MENU_POPUPITEM },

	{ LPGENW("&Affiliations"), IDM_AFFLTN, MENU_NEWPOPUP },
	{ LPGENW("&Member list"), IDM_LST_MEMBER, MENU_POPUPITEM },
	{ LPGENW("&Admin list"), IDM_LST_ADMIN, MENU_POPUPITEM },
	{ LPGENW("&Owner list"), IDM_LST_OWNER, MENU_POPUPITEM },
	{ nullptr, 0, MENU_POPUPSEPARATOR },
	{ LPGENW("Outcast list (&ban)"), IDM_LST_BAN, MENU_POPUPITEM },

	{ LPGENW("&Room options"), 0, MENU_NEWPOPUP },
	{ LPGENW("View/change &topic"), IDM_TOPIC, MENU_POPUPITEM },
	{ LPGENW("Add to &bookmarks"), IDM_BOOKMARKS, MENU_POPUPITEM },
	{ LPGENW("&Configure..."), IDM_CONFIG, MENU_POPUPITEM },
	{ LPGENW("&Destroy room"), IDM_DESTROY, MENU_POPUPITEM },

	{ nullptr, 0, MENU_SEPARATOR },

	{ LPGENW("Lin&ks"), 0, MENU_NEWPOPUP },
	{ nullptr, IDM_LINK0, 0 },
	{ nullptr, IDM_LINK1, 0 },
	{ nullptr, IDM_LINK2, 0 },
	{ nullptr, IDM_LINK3, 0 },
	{ nullptr, IDM_LINK4, 0 },
	{ nullptr, IDM_LINK5, 0 },
	{ nullptr, IDM_LINK6, 0 },
	{ nullptr, IDM_LINK7, 0 },
	{ nullptr, IDM_LINK8, 0 },
	{ nullptr, IDM_LINK9, 0 },

	{ LPGENW("Copy room &JID"), IDM_CPY_RJID, MENU_ITEM },
	{ LPGENW("Copy room topic"), IDM_CPY_TOPIC, MENU_ITEM },
	{ nullptr, 0, MENU_SEPARATOR },

	{ LPGENW("&Send presence"), 0, MENU_NEWPOPUP },
	{ LPGENW("Online"), IDM_PRESENCE_ONLINE, MENU_POPUPITEM },
	{ LPGENW("Away"), IDM_PRESENCE_AWAY, MENU_POPUPITEM },
	{ LPGENW("Not available"), IDM_PRESENCE_NA, MENU_POPUPITEM },
	{ LPGENW("Do not disturb"), IDM_PRESENCE_DND, MENU_POPUPITEM },
	{ LPGENW("Free for chat"), IDM_PRESENCE_FREE4CHAT, MENU_POPUPITEM },

	{ LPGENW("&Leave chat session"), IDM_LEAVE, MENU_ITEM }
};

static wchar_t sttRJidBuf[JABBER_MAX_JID_LEN] = { 0 };
static gc_item sttListItems[] =
{
	{ LPGENW("&Slap"), IDM_SLAP, MENU_ITEM },   // 0
	{ LPGENW("&User details"), IDM_VCARD, MENU_ITEM },   // 1
	{ LPGENW("Member &info"), IDM_INFO, MENU_ITEM },   // 2

	{ sttRJidBuf, 0, MENU_NEWPOPUP },   // 3 -> accessed explicitly by index!!!
	{ LPGENW("User &details"), IDM_RJID_VCARD, MENU_POPUPITEM },
	{ LPGENW("&Add to roster"), IDM_RJID_ADD, MENU_POPUPITEM },
	{ LPGENW("&Copy to clipboard"), IDM_RJID_COPY, MENU_POPUPITEM },

	{ LPGENW("Invite to room"), 0, MENU_NEWPOPUP },
	{ nullptr, IDM_LINK0, 0 },
	{ nullptr, IDM_LINK1, 0 },
	{ nullptr, IDM_LINK2, 0 },
	{ nullptr, IDM_LINK3, 0 },
	{ nullptr, IDM_LINK4, 0 },
	{ nullptr, IDM_LINK5, 0 },
	{ nullptr, IDM_LINK6, 0 },
	{ nullptr, IDM_LINK7, 0 },
	{ nullptr, IDM_LINK8, 0 },
	{ nullptr, IDM_LINK9, 0 },

	{ nullptr, 0, MENU_SEPARATOR },

	{ LPGENW("Set &role"), IDM_ROLE, MENU_NEWPOPUP },
	{ LPGENW("&Visitor"), IDM_SET_VISITOR, MENU_POPUPITEM },
	{ LPGENW("&Participant"), IDM_SET_PARTICIPANT, MENU_POPUPITEM },
	{ LPGENW("&Moderator"), IDM_SET_MODERATOR, MENU_POPUPITEM },

	{ LPGENW("Set &affiliation"), IDM_AFFLTN, MENU_NEWPOPUP },
	{ LPGENW("&None"), IDM_SET_NONE, MENU_POPUPITEM },
	{ LPGENW("&Member"), IDM_SET_MEMBER, MENU_POPUPITEM },
	{ LPGENW("&Admin"), IDM_SET_ADMIN, MENU_POPUPITEM },
	{ LPGENW("&Owner"), IDM_SET_OWNER, MENU_POPUPITEM },
	{ nullptr, 0, MENU_POPUPSEPARATOR },
	{ LPGENW("Outcast (&ban)"), IDM_SET_BAN, MENU_POPUPITEM },

	{ LPGENW("&Kick"), IDM_KICK, MENU_ITEM },
	{ nullptr, 0, MENU_SEPARATOR },
	{ LPGENW("Copy &nickname"), IDM_CPY_NICK, MENU_ITEM },
	{ LPGENW("Copy real &JID"), IDM_CPY_RJID, MENU_ITEM },
	{ LPGENW("Copy in-room JID"), IDM_CPY_INROOMJID, MENU_ITEM }
};

int CJabberProto::JabberGcMenuHook(WPARAM, LPARAM lParam)
{
	GCMENUITEMS* gcmi = (GCMENUITEMS*)lParam;
	if (gcmi == nullptr)
		return 0;

	if (mir_strcmpi(gcmi->pszModule, m_szModuleName))
		return 0;

	JABBER_LIST_ITEM *item = ListGetItemPtr(LIST_CHATROOM, T2Utf(gcmi->pszID));
	if (item == nullptr)
		return 0;

	pResourceStatus me(nullptr), him(nullptr);
	for (auto &p : item->arResources) {
		if (!mir_strcmp(p->m_szResourceName, item->nick))
			me = p;
		if (!mir_strcmp(p->m_szResourceName, T2Utf(gcmi->pszUID)))
			him = p;
	}

	if (gcmi->Type == MENU_ON_LOG) {
		static wchar_t url_buf[1024] = { 0 };

		static DWORD sttModeratorItems[] = { IDM_LST_PARTICIPANT, 0 };
		static DWORD sttAdminItems[] = { IDM_LST_MODERATOR, IDM_LST_MEMBER, IDM_LST_ADMIN, IDM_LST_OWNER, IDM_LST_BAN, 0 };
		static DWORD sttOwnerItems[] = { IDM_CONFIG, IDM_DESTROY, 0 };

		sttSetupGcMenuItem(_countof(sttLogListItems), sttLogListItems, 0, FALSE);

		int idx = IDM_LINK0;
		char *ptszStatusMsg = item->getTemp()->m_szStatusMessage;
		if (ptszStatusMsg && *ptszStatusMsg) {
			wchar_t *bufPtr = url_buf;
			for (char *p = strstr(ptszStatusMsg, "http"); p && *p; p = strstr(p + 1, "http")) {
				if (!strncmp(p, "http://", 7) || !strncmp(p, "https://", 8)) {
					mir_wstrncpy(bufPtr, Utf2T(p), _countof(url_buf) - (bufPtr - url_buf));
					gc_item *pItem = sttFindGcMenuItem(_countof(sttLogListItems), sttLogListItems, idx);
					pItem->pszDesc = bufPtr;
					pItem->uType = MENU_POPUPITEM;
					for (; *bufPtr && !iswspace(*bufPtr); ++bufPtr);
					*bufPtr++ = 0;

					if (++idx > IDM_LINK9)
						break;
				}
			}
		}
		for (; idx <= IDM_LINK9; ++idx)
			sttFindGcMenuItem(_countof(sttLogListItems), sttLogListItems, idx)->uType = 0;

		if (!GetAsyncKeyState(VK_CONTROL)) {
			if (me) {
				sttSetupGcMenuItems(_countof(sttLogListItems), sttLogListItems, sttModeratorItems, (me->m_role < ROLE_MODERATOR));
				sttSetupGcMenuItems(_countof(sttLogListItems), sttLogListItems, sttAdminItems, (me->m_affiliation < AFFILIATION_ADMIN));
				sttSetupGcMenuItems(_countof(sttLogListItems), sttLogListItems, sttOwnerItems, (me->m_affiliation < AFFILIATION_OWNER));
			}
			if (m_ThreadInfo->jabberServerCaps & JABBER_CAPS_PRIVATE_STORAGE)
				sttSetupGcMenuItem(_countof(sttLogListItems), sttLogListItems, IDM_BOOKMARKS, FALSE);
		}
		Chat_AddMenuItems(gcmi->hMenu, _countof(sttLogListItems), sttLogListItems, &g_plugin);
	}
	else if (gcmi->Type == MENU_ON_NICKLIST) {
		static DWORD sttRJidItems[] = { IDM_RJID_VCARD, IDM_RJID_ADD, IDM_RJID_COPY, 0 };

		if (me && him) {
			int i, idx;
			BOOL force = GetAsyncKeyState(VK_CONTROL);
			sttSetupGcMenuItem(_countof(sttListItems), sttListItems, 0, FALSE);

			idx = IDM_LINK0;
			LISTFOREACH_NODEF(i, this, LIST_CHATROOM)
				if (item = ListGetItemPtrFromIndex(i)) {
					if (!item->bChatActive)
						continue;

					gc_item *pItem = sttFindGcMenuItem(_countof(sttListItems), sttListItems, idx);
					pItem->uType = MENU_POPUPITEM;
					if (++idx > IDM_LINK9)
						break;
				}

			for (; idx <= IDM_LINK9; ++idx)
				sttFindGcMenuItem(_countof(sttListItems), sttListItems, idx)->uType = 0;

			for (auto &it : sttAffiliationItems) {
				gc_item *pItem = sttFindGcMenuItem(_countof(sttListItems), sttListItems, it.id);
				pItem->uType = (him->m_affiliation == it.value) ? MENU_POPUPCHECK : MENU_POPUPITEM;
				pItem->bDisabled = !(force || it.check(me, him));
			}

			for (auto &it : sttRoleItems) {
				gc_item *pItem = sttFindGcMenuItem(_countof(sttListItems), sttListItems, it.id);
				pItem->uType = (him->m_role == it.value) ? MENU_POPUPCHECK : MENU_POPUPITEM;
				pItem->bDisabled = !(force || it.check(me, him));
			}

			if (him->m_szRealJid && *him->m_szRealJid) {
				mir_snwprintf(sttRJidBuf, TranslateT("Real &JID: %s"), Utf2T(him->m_szRealJid).get());
				if (wchar_t *tmp = wcschr(sttRJidBuf, '/')) *tmp = 0;

				if (MCONTACT hContact = HContactFromJID(him->m_szRealJid)) {
					sttListItems[3].uType = MENU_HMENU;
					sttListItems[3].dwID = (INT_PTR)Menu_BuildContactMenu(hContact);
					sttShowGcMenuItems(_countof(sttListItems), sttListItems, sttRJidItems, 0);
				}
				else {
					sttListItems[3].uType = MENU_NEWPOPUP;
					sttShowGcMenuItems(_countof(sttListItems), sttListItems, sttRJidItems, MENU_POPUPITEM);
				}

				sttSetupGcMenuItem(_countof(sttListItems), sttListItems, IDM_CPY_RJID, FALSE);
			}
			else {
				sttListItems[3].uType = 0;
				sttShowGcMenuItems(_countof(sttListItems), sttListItems, sttRJidItems, 0);

				sttSetupGcMenuItem(_countof(sttListItems), sttListItems, IDM_CPY_RJID, TRUE);
			}

			if (!force) {
				if (me->m_role < ROLE_MODERATOR || (me->m_affiliation <= him->m_affiliation))
					sttSetupGcMenuItem(_countof(sttListItems), sttListItems, IDM_KICK, TRUE);

				if ((me->m_affiliation < AFFILIATION_ADMIN) ||
					(me->m_affiliation == AFFILIATION_ADMIN) && (me->m_affiliation <= him->m_affiliation))
					sttSetupGcMenuItem(_countof(sttListItems), sttListItems, IDM_SET_BAN, TRUE);
			}
		}
		else {
			sttSetupGcMenuItem(_countof(sttListItems), sttListItems, 0, TRUE);
			sttListItems[2].uType = 0;
			sttShowGcMenuItems(_countof(sttListItems), sttListItems, sttRJidItems, 0);
		}
		Chat_AddMenuItems(gcmi->hMenu, _countof(sttListItems), sttListItems, &g_plugin);
	}

	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////
// Conference invitation dialog

class CGroupchatInviteDlg : public CJabberDlgBase
{
	typedef CJabberDlgBase CSuper;

	struct JabberGcLogInviteDlgJidData
	{
		HANDLE hItem;
		char jid[JABBER_MAX_JID_LEN];
	};

	LIST<JabberGcLogInviteDlgJidData> m_newJids;
	char *m_room;

	CCtrlButton  m_btnInvite;
	CCtrlEdit    m_txtNewJid;
	CCtrlMButton m_btnAddJid;
	CCtrlEdit    m_txtReason;
	CCtrlClc     m_clc;

	bool FindJid(const char *buf)
	{
		for (auto &it : m_newJids)
			if (!mir_strcmp(it->jid, buf))
				return true;
		
		return false;
	}

	void FilterList(CCtrlClc *)
	{
		for (auto &hContact : Contacts()) {
			char *proto = GetContactProto(hContact);
			if (mir_strcmp(proto, m_proto->m_szModuleName) || m_proto->isChatRoom(hContact))
				if (HANDLE hItem = m_clc.FindContact(hContact))
					m_clc.DeleteItem(hItem);
		}
	}

	void ResetListOptions(CCtrlClc *)
	{
		m_clc.SetBkBitmap(0, nullptr);
		m_clc.SetBkColor(GetSysColor(COLOR_WINDOW));
		m_clc.SetGreyoutFlags(0);
		m_clc.SetLeftMargin(4);
		m_clc.SetIndent(10);
		m_clc.SetHideEmptyGroups(1);
		m_clc.SetHideOfflineRoot(1);
		for (int i = 0; i <= FONTID_MAX; i++)
			m_clc.SetTextColor(i, GetSysColor(COLOR_WINDOWTEXT));
	}

	void InviteUser(char *pUser, char *text)
	{
		XmlNode msg("message");
		TiXmlElement *invite = msg << XATTR("to", m_room) << XATTRID(m_proto->SerialNext())
			<< XCHILDNS("x", JABBER_FEAT_MUC_USER)
			<< XCHILD("invite") << XATTR("to", pUser);
		if (text)
			invite << XCHILD("reason", text);

		m_proto->m_ThreadInfo->send(msg);
	}

public:
	CGroupchatInviteDlg(CJabberProto *ppro, const char *room) :
		CSuper(ppro, IDD_GROUPCHAT_INVITE),
		m_newJids(1),
		m_btnInvite(this, IDC_INVITE),
		m_txtNewJid(this, IDC_NEWJID),
		m_btnAddJid(this, IDC_ADDJID, ppro->LoadIconEx("addroster"), "Add"),
		m_txtReason(this, IDC_REASON),
		m_clc(this, IDC_CLIST)
	{
		m_room = mir_strdup(room);
		m_btnAddJid.OnClick = Callback(this, &CGroupchatInviteDlg::OnCommand_AddJid);
		m_btnInvite.OnClick = Callback(this, &CGroupchatInviteDlg::OnCommand_Invite);
		m_clc.OnNewContact =
			m_clc.OnListRebuilt = Callback(this, &CGroupchatInviteDlg::FilterList);
		m_clc.OnOptionsChanged = Callback(this, &CGroupchatInviteDlg::ResetListOptions);
	}

	~CGroupchatInviteDlg()
	{
		for (auto &it : m_newJids)
			mir_free(it);
		mir_free(m_room);
	}

	bool OnInitDialog() override
	{
		CSuper::OnInitDialog();

		SetDlgItemText(m_hwnd, IDC_HEADERBAR, CMStringW(FORMAT, TranslateT("Invite Users to\n%s"), m_room));
		Window_SetIcon_IcoLib(m_hwnd, g_GetIconHandle(IDI_GROUP));

		SetWindowLongPtr(m_clc.GetHwnd(), GWL_STYLE,
			GetWindowLongPtr(m_clc.GetHwnd(), GWL_STYLE) | CLS_SHOWHIDDEN | CLS_HIDEOFFLINE | CLS_CHECKBOXES | CLS_HIDEEMPTYGROUPS | CLS_USEGROUPS | CLS_GREYALTERNATE | CLS_GROUPCHECKBOXES);
		m_clc.SendMsg(CLM_SETEXSTYLE, CLS_EX_DISABLEDRAGDROP | CLS_EX_TRACKSELECT, 0);
		ResetListOptions(&m_clc);
		FilterList(&m_clc);
		return true;
	}

	void OnCommand_AddJid(CCtrlButton*)
	{
		ptrA szJid(m_txtNewJid.GetTextU());
		m_txtNewJid.SetTextA("");

		MCONTACT hContact = m_proto->HContactFromJID(szJid);
		if (hContact) {
			int hItem = SendDlgItemMessage(m_hwnd, IDC_CLIST, CLM_FINDCONTACT, hContact, 0);
			if (hItem)
				SendDlgItemMessage(m_hwnd, IDC_CLIST, CLM_SETCHECKMARK, hItem, 1);
			return;
		}

		if (FindJid(szJid))
			return;

		JabberGcLogInviteDlgJidData *jidData = (JabberGcLogInviteDlgJidData *)mir_alloc(sizeof(JabberGcLogInviteDlgJidData));
		mir_strcpy(jidData->jid, szJid);
		
		CMStringW msg(FORMAT, TranslateT("%s (not on roster)"), Utf2T(szJid).get());
		CLCINFOITEM cii = { 0 };
		cii.cbSize = sizeof(cii);
		cii.flags = CLCIIF_CHECKBOX;
		cii.pszText = msg;
		jidData->hItem = m_clc.AddInfoItem(&cii);
		m_clc.SetCheck(jidData->hItem, 1);
		m_newJids.insert(jidData);
	}

	void OnCommand_Invite(CCtrlButton*)
	{
		if (!m_room) return;

		T2Utf text(ptrW(m_txtReason.GetText()));

		// invite users from roster
		for (auto &hContact : m_proto->AccContacts()) {
			if (m_proto->isChatRoom(hContact))
				continue;

			if (HANDLE hItem = m_clc.FindContact(hContact)) {
				if (m_clc.GetCheck(hItem)) {
					ptrA jid(m_proto->getUStringA(hContact, "jid"));
					if (jid != nullptr)
						InviteUser(jid, text);
				}
			}
		}

		// invite others
		for (auto &it : m_newJids)
			if (m_clc.GetCheck(it->hItem))
				InviteUser(it->jid, text);

		Close();
	}
};

/////////////////////////////////////////////////////////////////////////////////////////
// Context menu processing

void CJabberProto::AdminSet(const char *to, const char *ns, const char *szItem, const char *itemVal, const char *var, const char *varVal)
{
	m_ThreadInfo->send(XmlNodeIq("set", SerialNext(), to) << XQUERY(ns) << XCHILD("item") << XATTR(szItem, itemVal) << XATTR(var, varVal));
}

void CJabberProto::AdminSetReason(const char *to, const char *ns, const char *szItem, const char *itemVal, const char *var, const char *varVal, const char *rsn)
{
	m_ThreadInfo->send(XmlNodeIq("set", SerialNext(), to) << XQUERY(ns) << XCHILD("item") << XATTR(szItem, itemVal) << XATTR(var, varVal) << XCHILD("reason", rsn));
}

void CJabberProto::AdminGet(const char *to, const char *ns, const char *var, const char *varVal, JABBER_IQ_HANDLER foo)
{
	m_ThreadInfo->send(XmlNodeIq(AddIQ(foo, JABBER_IQ_TYPE_GET, to))
		<< XQUERY(ns) << XCHILD("item") << XATTR(var, varVal));
}

// Member info dialog
struct TUserInfoData
{
	CJabberProto *ppro;
	JABBER_LIST_ITEM *item;
	JABBER_RESOURCE_STATUS *me, *him;
};

static INT_PTR CALLBACK sttUserInfoDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	TUserInfoData *dat = (TUserInfoData *)GetWindowLongPtr(hwndDlg, GWLP_USERDATA);
	int value, idx;

	switch (msg) {
	case WM_INITDIALOG:
		TranslateDialogDefault(hwndDlg);
		SetWindowLongPtr(hwndDlg, GWLP_USERDATA, lParam);
		dat = (TUserInfoData *)lParam;

		Window_SetIcon_IcoLib(hwndDlg, g_GetIconHandle(IDI_GROUP));
		{
			LOGFONT lf;
			GetObject((HFONT)SendDlgItemMessage(hwndDlg, IDC_TXT_NICK, WM_GETFONT, 0, 0), sizeof(lf), &lf);
			lf.lfWeight = FW_BOLD;
			HFONT hfnt = CreateFontIndirect(&lf);
			SendDlgItemMessage(hwndDlg, IDC_TXT_NICK, WM_SETFONT, (WPARAM)hfnt, TRUE);
		}

		SendDlgItemMessage(hwndDlg, IDC_BTN_AFFILIATION, BM_SETIMAGE, IMAGE_ICON, (LPARAM)Skin_LoadIcon(SKINICON_EVENT_FILE));
		SendDlgItemMessage(hwndDlg, IDC_BTN_AFFILIATION, BUTTONSETASFLATBTN, TRUE, 0);
		SendDlgItemMessage(hwndDlg, IDC_BTN_AFFILIATION, BUTTONADDTOOLTIP, (WPARAM)"Apply", 0);

		SendDlgItemMessage(hwndDlg, IDC_BTN_ROLE, BM_SETIMAGE, IMAGE_ICON, (LPARAM)Skin_LoadIcon(SKINICON_EVENT_FILE));
		SendDlgItemMessage(hwndDlg, IDC_BTN_ROLE, BUTTONSETASFLATBTN, TRUE, 0);
		SendDlgItemMessage(hwndDlg, IDC_BTN_ROLE, BUTTONADDTOOLTIP, (WPARAM)"Apply", 0);

		SendDlgItemMessage(hwndDlg, IDC_ICO_STATUS, STM_SETICON, (WPARAM)Skin_LoadProtoIcon(dat->ppro->m_szModuleName, dat->him->m_iStatus), 0);

		char buf[256];
		mir_snprintf(buf, TranslateU("%s from\n%s"), dat->him->m_szResourceName, dat->item->jid);
		SetDlgItemTextUtf(hwndDlg, IDC_HEADERBAR, buf);

		SetDlgItemTextUtf(hwndDlg, IDC_TXT_NICK, dat->him->m_szResourceName);
		SetDlgItemTextUtf(hwndDlg, IDC_TXT_JID, dat->him->m_szRealJid ? dat->him->m_szRealJid : TranslateU("Real JID not available"));
		SetDlgItemTextUtf(hwndDlg, IDC_TXT_STATUS, dat->him->m_szStatusMessage);

		for (auto &it : sttRoleItems) {
			if ((it.value == dat->him->m_role) || it.check(dat->me, dat->him)) {
				SendDlgItemMessage(hwndDlg, IDC_TXT_ROLE, CB_SETITEMDATA,
					idx = SendDlgItemMessage(hwndDlg, IDC_TXT_ROLE, CB_ADDSTRING, 0, (LPARAM)it.title),
					it.value);
				if (it.value == dat->him->m_role)
					SendDlgItemMessage(hwndDlg, IDC_TXT_ROLE, CB_SETCURSEL, idx, 0);
			}
		}
		
		for (auto &it : sttAffiliationItems) {
			if ((it.value == dat->him->m_affiliation) || it.check(dat->me, dat->him)) {
				SendDlgItemMessage(hwndDlg, IDC_TXT_AFFILIATION, CB_SETITEMDATA,
					idx = SendDlgItemMessage(hwndDlg, IDC_TXT_AFFILIATION, CB_ADDSTRING, 0, (LPARAM)it.title),
					it.value);
				if (it.value == dat->him->m_affiliation)
					SendDlgItemMessage(hwndDlg, IDC_TXT_AFFILIATION, CB_SETCURSEL, idx, 0);
			}
		}

		EnableWindow(GetDlgItem(hwndDlg, IDC_BTN_ROLE), FALSE);
		EnableWindow(GetDlgItem(hwndDlg, IDC_BTN_AFFILIATION), FALSE);
		break;

	case WM_COMMAND:
		if (!dat)
			break;

		switch (LOWORD(wParam)) {
		case IDCANCEL:
			PostMessage(hwndDlg, WM_CLOSE, 0, 0);
			break;

		case IDC_TXT_AFFILIATION:
			if (HIWORD(wParam) == CBN_SELCHANGE) {
				value = SendDlgItemMessage(hwndDlg, IDC_TXT_AFFILIATION, CB_GETITEMDATA,
					SendDlgItemMessage(hwndDlg, IDC_TXT_AFFILIATION, CB_GETCURSEL, 0, 0), 0);
				EnableWindow(GetDlgItem(hwndDlg, IDC_BTN_AFFILIATION), dat->him->m_affiliation != value);
			}
			break;

		case IDC_BTN_AFFILIATION:
			value = SendDlgItemMessage(hwndDlg, IDC_TXT_AFFILIATION, CB_GETITEMDATA,
				SendDlgItemMessage(hwndDlg, IDC_TXT_AFFILIATION, CB_GETCURSEL, 0, 0), 0);
			if (dat->him->m_affiliation == value)
				break;

			char szBareJid[JABBER_MAX_JID_LEN];
			JabberStripJid(dat->him->m_szRealJid, szBareJid, _countof(szBareJid));

			switch (value) {
			case AFFILIATION_NONE:
				if (dat->him->m_szRealJid)
					dat->ppro->AdminSet(dat->item->jid, JABBER_FEAT_MUC_ADMIN, "jid", szBareJid, "affiliation", "none");
				else
					dat->ppro->AdminSet(dat->item->jid, JABBER_FEAT_MUC_ADMIN, "nick", dat->him->m_szResourceName, "affiliation", "none");
				break;
			case AFFILIATION_MEMBER:
				if (dat->him->m_szRealJid)
					dat->ppro->AdminSet(dat->item->jid, JABBER_FEAT_MUC_ADMIN, "jid", szBareJid, "affiliation", "member");
				else
					dat->ppro->AdminSet(dat->item->jid, JABBER_FEAT_MUC_ADMIN, "nick", dat->him->m_szResourceName, "affiliation", "member");
				break;
			case AFFILIATION_ADMIN:
				if (dat->him->m_szRealJid)
					dat->ppro->AdminSet(dat->item->jid, JABBER_FEAT_MUC_ADMIN, "jid", szBareJid, "affiliation", "admin");
				else
					dat->ppro->AdminSet(dat->item->jid, JABBER_FEAT_MUC_ADMIN, "nick", dat->him->m_szResourceName, "affiliation", "admin");
				break;
			case AFFILIATION_OWNER:
				if (dat->him->m_szRealJid)
					dat->ppro->AdminSet(dat->item->jid, JABBER_FEAT_MUC_ADMIN, "jid", szBareJid, "affiliation", "owner");
				else
					dat->ppro->AdminSet(dat->item->jid, JABBER_FEAT_MUC_ADMIN, "nick", dat->him->m_szResourceName, "affiliation", "owner");
			}
			break;

		case IDC_TXT_ROLE:
			if (HIWORD(wParam) == CBN_SELCHANGE) {
				value = SendDlgItemMessage(hwndDlg, IDC_TXT_ROLE, CB_GETITEMDATA,
					SendDlgItemMessage(hwndDlg, IDC_TXT_ROLE, CB_GETCURSEL, 0, 0), 0);
				EnableWindow(GetDlgItem(hwndDlg, IDC_BTN_ROLE), dat->him->m_role != value);
			}
			break;

		case IDC_BTN_ROLE:
			value = SendDlgItemMessage(hwndDlg, IDC_TXT_ROLE, CB_GETITEMDATA,
				SendDlgItemMessage(hwndDlg, IDC_TXT_ROLE, CB_GETCURSEL, 0, 0), 0);
			if (dat->him->m_role == value)
				break;

			switch (value) {
			case ROLE_VISITOR:
				dat->ppro->AdminSet(dat->item->jid, JABBER_FEAT_MUC_ADMIN, "nick", dat->him->m_szResourceName, "role", "visitor");
				break;
			case ROLE_PARTICIPANT:
				dat->ppro->AdminSet(dat->item->jid, JABBER_FEAT_MUC_ADMIN, "nick", dat->him->m_szResourceName, "role", "participant");
				break;
			case ROLE_MODERATOR:
				dat->ppro->AdminSet(dat->item->jid, JABBER_FEAT_MUC_ADMIN, "nick", dat->him->m_szResourceName, "role", "moderator");
				break;
			}
		}
		break;

	case WM_CLOSE:
		DestroyWindow(hwndDlg);
		break;

	case WM_DESTROY:
		Window_FreeIcon_IcoLib(hwndDlg);
		IcoLib_ReleaseIcon((HICON)SendDlgItemMessage(hwndDlg, IDC_BTN_AFFILIATION, BM_SETIMAGE, IMAGE_ICON, 0));
		IcoLib_ReleaseIcon((HICON)SendDlgItemMessage(hwndDlg, IDC_BTN_ROLE, BM_SETIMAGE, IMAGE_ICON, 0));
		if (dat) {
			SetWindowLongPtr(hwndDlg, GWLP_USERDATA, 0);
			mir_free(dat);
		}
		break;
	}
	return FALSE;
}

static void sttNickListHook(CJabberProto *ppro, JABBER_LIST_ITEM *item, GCHOOK* gch)
{
	pResourceStatus me(item->findResource(item->nick)), him(item->findResource(T2Utf(gch->ptszUID)));
	if (him == nullptr || me == nullptr)
		return;

	// 1 kick per second, prevents crashes...
	enum { BAN_KICK_INTERVAL = 1000 };
	static DWORD dwLastBanKickTime = 0;

	CMStringW szBuffer, szTitle;

	if ((gch->dwData >= CLISTMENUIDMIN) && (gch->dwData <= CLISTMENUIDMAX)) {
		if (him->m_szRealJid && *him->m_szRealJid)
			if (MCONTACT hContact = ppro->HContactFromJID(him->m_szRealJid))
				Clist_MenuProcessCommand(gch->dwData, MPCF_CONTACTMENU, hContact);
		return;
	}

	switch (gch->dwData) {
	case IDM_SLAP:
		if (ppro->m_bJabberOnline) {
			ptrA szMessage(ppro->getUStringA("GcMsgSlap"));
			if (szMessage == nullptr)
				szMessage = mir_strdup(TranslateU(JABBER_GC_MSG_SLAP));

			CMStringA buf;
			// do not use snprintf to avoid possible problems with % symbol
			if (char *p = strstr(szMessage, "%s")) {
				*p = 0;
				buf.Format("%s%s%s", szMessage, him->m_szResourceName, p + 2);
			}
			else buf = szMessage;
			buf.Replace("%%", "%");

			ppro->m_ThreadInfo->send(
				XmlNode("message") << XATTR("to", item->jid) << XATTR("type", "groupchat")
				<< XCHILD("body", buf));
		}
		break;

	case IDM_VCARD:
		{
			CMStringA jid(FORMAT, "%s/%s", item->jid, him->m_szResourceName);

			MCONTACT hContact = ppro->AddToListByJID(jid, PALF_TEMPORARY);
			ppro->setUString(hContact, "Nick", him->m_szResourceName);
			
			JABBER_LIST_ITEM *pTmp = ppro->ListAdd(LIST_VCARD_TEMP, jid, hContact);
			ppro->ListAddResource(LIST_VCARD_TEMP, jid, him->m_iStatus, him->m_szStatusMessage, him->m_iPriority);

			pTmp->findResource(him->m_szResourceName)->m_pCaps = ppro->ListGetItemPtr(LIST_CHATROOM, item->jid)->findResource(him->m_szResourceName)->m_pCaps;

			CallService(MS_USERINFO_SHOWDIALOG, hContact, 0);
		}
		break;

	case IDM_INFO:
		{
			TUserInfoData *dat = (TUserInfoData *)mir_alloc(sizeof(TUserInfoData));
			dat->me = me;
			dat->him = him;
			dat->item = item;
			dat->ppro = ppro;
			HWND hwndInfo = CreateDialogParam(g_plugin.getInst(), MAKEINTRESOURCE(IDD_GROUPCHAT_INFO), nullptr, sttUserInfoDlgProc, (LPARAM)dat);
			ShowWindow(hwndInfo, SW_SHOW);
		}
		break;

	case IDM_KICK:
		if ((GetTickCount() - dwLastBanKickTime) > BAN_KICK_INTERVAL) {
			dwLastBanKickTime = GetTickCount();
			szBuffer.Format(L"%s: ", me->m_szResourceName);
			szTitle.Format(TranslateT("Reason to kick %s"), Utf2T(him->m_szResourceName).get());
			char *resourceName_copy = mir_strdup(him->m_szResourceName); // copy resource name to prevent possible crash if user list rebuilds
			if (ppro->EnterString(szBuffer, szTitle, ESF_MULTILINE, "gcReason_"))
				ppro->m_ThreadInfo->send(
				XmlNodeIq("set", ppro->SerialNext(), item->jid) << XQUERY(JABBER_FEAT_MUC_ADMIN)
				<< XCHILD("item") << XATTR("nick", resourceName_copy) << XATTR("role", "none")
				<< XCHILD("reason", T2Utf(szBuffer)));

			mir_free(resourceName_copy);
		}
		dwLastBanKickTime = GetTickCount();
		break;

	case IDM_SET_VISITOR:
		if (him->m_role != ROLE_VISITOR)
			ppro->AdminSet(item->jid, JABBER_FEAT_MUC_ADMIN, "nick", him->m_szResourceName, "role", "visitor");
		break;

	case IDM_SET_PARTICIPANT:
		if (him->m_role != ROLE_PARTICIPANT)
			ppro->AdminSet(item->jid, JABBER_FEAT_MUC_ADMIN, "nick", him->m_szResourceName, "role", "participant");
		break;

	case IDM_SET_MODERATOR:
		if (him->m_role != ROLE_MODERATOR)
			ppro->AdminSet(item->jid, JABBER_FEAT_MUC_ADMIN, "nick", him->m_szResourceName, "role", "moderator");
		break;

	case IDM_SET_NONE:
		if (him->m_affiliation != AFFILIATION_NONE) {
			if (him->m_szRealJid) {
				char szBareJid[JABBER_MAX_JID_LEN];
				JabberStripJid(him->m_szRealJid, szBareJid, _countof(szBareJid));
				ppro->AdminSet(item->jid, JABBER_FEAT_MUC_ADMIN, "jid", szBareJid, "affiliation", "none");
			}
			else ppro->AdminSet(item->jid, JABBER_FEAT_MUC_ADMIN, "nick", him->m_szResourceName, "affiliation", "none");
		}
		break;

	case IDM_SET_MEMBER:
		if (him->m_affiliation != AFFILIATION_MEMBER) {
			if (him->m_szRealJid) {
				char szBareJid[JABBER_MAX_JID_LEN];
				JabberStripJid(him->m_szRealJid, szBareJid, _countof(szBareJid));
				ppro->AdminSet(item->jid, JABBER_FEAT_MUC_ADMIN, "jid", szBareJid, "affiliation", "member");
			}
			else ppro->AdminSet(item->jid, JABBER_FEAT_MUC_ADMIN, "nick", him->m_szResourceName, "affiliation", "member");
		}
		break;

	case IDM_SET_ADMIN:
		if (him->m_affiliation != AFFILIATION_ADMIN) {
			if (him->m_szRealJid) {
				char szBareJid[JABBER_MAX_JID_LEN];
				JabberStripJid(him->m_szRealJid, szBareJid, _countof(szBareJid));
				ppro->AdminSet(item->jid, JABBER_FEAT_MUC_ADMIN, "jid", szBareJid, "affiliation", "admin");
			}
			else ppro->AdminSet(item->jid, JABBER_FEAT_MUC_ADMIN, "nick", him->m_szResourceName, "affiliation", "admin");
		}
		break;

	case IDM_SET_OWNER:
		if (him->m_affiliation != AFFILIATION_OWNER) {
			if (him->m_szRealJid) {
				char szBareJid[JABBER_MAX_JID_LEN];
				JabberStripJid(him->m_szRealJid, szBareJid, _countof(szBareJid));
				ppro->AdminSet(item->jid, JABBER_FEAT_MUC_ADMIN, "jid", szBareJid, "affiliation", "owner");
			}
			else ppro->AdminSet(item->jid, JABBER_FEAT_MUC_ADMIN, "nick", him->m_szResourceName, "affiliation", "owner");
		}
		break;

	case IDM_SET_BAN:
		if ((GetTickCount() - dwLastBanKickTime) > BAN_KICK_INTERVAL) {
			if (him->m_szRealJid && *him->m_szRealJid) {
				char szVictimBareJid[JABBER_MAX_JID_LEN];
				JabberStripJid(him->m_szRealJid, szVictimBareJid, _countof(szVictimBareJid));

				szBuffer.Format(L"%s: ", me->m_szResourceName);
				szTitle.Format(TranslateT("Reason to ban %s"), him->m_szResourceName);

				if (ppro->EnterString(szBuffer, szTitle, ESF_MULTILINE, "gcReason_"))
					ppro->m_ThreadInfo->send(
					XmlNodeIq("set", ppro->SerialNext(), item->jid) << XQUERY(JABBER_FEAT_MUC_ADMIN)
					<< XCHILD("item") << XATTR("jid", szVictimBareJid) << XATTR("affiliation", "outcast")
					<< XCHILD("reason", T2Utf(szBuffer)));
			}
		}
		dwLastBanKickTime = GetTickCount();
		break;

	case IDM_LINK0: case IDM_LINK1: case IDM_LINK2: case IDM_LINK3: case IDM_LINK4:
	case IDM_LINK5: case IDM_LINK6: case IDM_LINK7: case IDM_LINK8: case IDM_LINK9:
		if ((GetTickCount() - dwLastBanKickTime) > BAN_KICK_INTERVAL) {
			Utf2T resourceName_copy(him->m_szResourceName); // copy resource name to prevent possible crash if user list rebuilds

			char *szInviteTo = nullptr;
			int idx = gch->dwData - IDM_LINK0;
			LISTFOREACH(i, ppro, LIST_CHATROOM)
			{
				if (JABBER_LIST_ITEM *pItem = ppro->ListGetItemPtrFromIndex(i)) {
					if (!pItem->bChatActive) continue;
					if (!idx--) {
						szInviteTo = pItem->jid;
						break;
					}
				}
			}

			if (!szInviteTo) break;

			szTitle.Format(TranslateT("Invite %s to %s"), Utf2T(him->m_szResourceName).get(), Utf2T(szInviteTo).get());
			if (!ppro->EnterString(szBuffer, szTitle, ESF_MULTILINE))
				break;

			szTitle.Format(L"%s/%s", item->jid, resourceName_copy);

			XmlNode msg("message");
			msg << XATTR("to", T2Utf(szTitle)) << XATTRID(ppro->SerialNext())
				<< XCHILD("x", T2Utf(szBuffer)) << XATTR("xmlns", JABBER_FEAT_DIRECT_MUC_INVITE) << XATTR("jid", szInviteTo)
				<< XCHILD("invite") << XATTR("from", item->nick);
			ppro->m_ThreadInfo->send(msg);
		}
		dwLastBanKickTime = GetTickCount();
		break;

	case IDM_CPY_NICK:
		JabberCopyText(g_clistApi.hwndContactList, him->m_szResourceName);
		break;

	case IDM_RJID_COPY:
	case IDM_CPY_RJID:
		JabberCopyText(g_clistApi.hwndContactList, him->m_szRealJid);
		break;

	case IDM_CPY_INROOMJID:
		JabberCopyText(g_clistApi.hwndContactList, CMStringA(FORMAT, "%s/%s", item->jid, him->m_szResourceName));
		break;

	case IDM_RJID_VCARD:
		if (him->m_szRealJid && *him->m_szRealJid) {
			char *jid = NEWSTR_ALLOCA(him->m_szRealJid);
			if (char *tmp = strchr(jid, '/'))
				*tmp = 0;

			MCONTACT hContact = ppro->AddToListByJID(jid, PALF_TEMPORARY);
			ppro->ListAdd(LIST_VCARD_TEMP, jid, hContact);
			ppro->ListAddResource(LIST_VCARD_TEMP, jid, him->m_iStatus, him->m_szStatusMessage, him->m_iPriority);

			CallService(MS_USERINFO_SHOWDIALOG, hContact, 0);
		}
		break;

	case IDM_RJID_ADD:
		if (him->m_szRealJid && *him->m_szRealJid) {
			PROTOSEARCHRESULT psr = { 0 };
			psr.cbSize = sizeof(psr);
			psr.id.a = NEWSTR_ALLOCA(him->m_szRealJid);
			if (char *tmp = strchr(psr.id.a, '/'))
				*tmp = 0;
			psr.nick.a = psr.id.a;
			Contact_AddBySearch(ppro->m_szModuleName, &psr, g_clistApi.hwndContactList);
		}
		break;
	}
}

static void sttLogListHook(CJabberProto *ppro, JABBER_LIST_ITEM *item, GCHOOK* gch)
{
	CMStringW szBuffer, szTitle;
	T2Utf roomJid(gch->ptszID);

	switch (gch->dwData) {
	case IDM_LST_PARTICIPANT:
		ppro->AdminGet(roomJid, JABBER_FEAT_MUC_ADMIN, "role", "participant", &CJabberProto::OnIqResultMucGetVoiceList);
		break;

	case IDM_LST_MEMBER:
		ppro->AdminGet(roomJid, JABBER_FEAT_MUC_ADMIN, "affiliation", "member", &CJabberProto::OnIqResultMucGetMemberList);
		break;

	case IDM_LST_MODERATOR:
		ppro->AdminGet(roomJid, JABBER_FEAT_MUC_ADMIN, "role", "moderator", &CJabberProto::OnIqResultMucGetModeratorList);
		break;

	case IDM_LST_BAN:
		ppro->AdminGet(roomJid, JABBER_FEAT_MUC_ADMIN, "affiliation", "outcast", &CJabberProto::OnIqResultMucGetBanList);
		break;

	case IDM_LST_ADMIN:
		ppro->AdminGet(roomJid, JABBER_FEAT_MUC_ADMIN, "affiliation", "admin", &CJabberProto::OnIqResultMucGetAdminList);
		break;

	case IDM_LST_OWNER:
		ppro->AdminGet(roomJid, JABBER_FEAT_MUC_ADMIN, "affiliation", "owner", &CJabberProto::OnIqResultMucGetOwnerList);
		break;

	case IDM_TOPIC:
		szTitle.Format(TranslateT("Set topic for %s"), gch->ptszID);
		szBuffer = item->getTemp()->m_szStatusMessage;
		szBuffer.Replace(L"\n", L"\r\n");
		if (ppro->EnterString(szBuffer, szTitle, ESF_RICHEDIT, "gcTopic_"))
			ppro->m_ThreadInfo->send(
				XmlNode("message") << XATTR("to", roomJid) << XATTR("type", "groupchat") << XCHILD("subject", T2Utf(szBuffer)));
		break;

	case IDM_NICK:
		szTitle.Format(TranslateT("Change nickname in %s"), gch->ptszID);
		if (item->nick)
			szBuffer = item->nick;
		if (ppro->EnterString(szBuffer, szTitle, ESF_COMBO, "gcNick_")) {
			if (ppro->ListGetItemPtr(LIST_CHATROOM, roomJid) != nullptr) {
				char text[1024];
				mir_snprintf(text, "%s/%s", roomJid.get(), szBuffer.c_str());
				ppro->SendPresenceTo(ppro->m_iStatus == ID_STATUS_INVISIBLE ? ID_STATUS_ONLINE : ppro->m_iStatus, text, nullptr);
			}
		}
		break;

	case IDM_INVITE:
		(new CGroupchatInviteDlg(ppro, roomJid))->Show();
		break;

	case IDM_CONFIG:
		ppro->m_ThreadInfo->send(
			XmlNodeIq(ppro->AddIQ(&CJabberProto::OnIqResultGetMuc, JABBER_IQ_TYPE_GET, roomJid))
			<< XQUERY(JABBER_FEAT_MUC_OWNER));
		break;

	case IDM_BOOKMARKS:
		item = ppro->ListGetItemPtr(LIST_BOOKMARK, roomJid);
		if (item == nullptr) {
			item = ppro->ListGetItemPtr(LIST_CHATROOM, roomJid);
			if (item != nullptr) {
				item->type = "conference";
				MCONTACT hContact = ppro->HContactFromJID(item->jid);
				item->name = Clist_GetContactDisplayName(hContact);
				ppro->AddEditBookmark(item);
			}
		}
		break;

	case IDM_DESTROY:
		szTitle.Format(TranslateT("Reason to destroy %s"), gch->ptszID);
		if (ppro->EnterString(szBuffer, szTitle, ESF_MULTILINE, "gcReason_"))
			ppro->m_ThreadInfo->send(
				XmlNodeIq("set", ppro->SerialNext(), roomJid) << XQUERY(JABBER_FEAT_MUC_OWNER)
				<< XCHILD("destroy") << XCHILD("reason", T2Utf(szBuffer)));
		__fallthrough;

	case IDM_LEAVE:
		ppro->GcQuit(item, 200, nullptr);
		break;

	case IDM_PRESENCE_ONLINE:
	case IDM_PRESENCE_AWAY:
	case IDM_PRESENCE_NA:
	case IDM_PRESENCE_DND:
	case IDM_PRESENCE_FREE4CHAT:
		if (MCONTACT h = ppro->HContactFromJID(item->jid))
			ppro->OnMenuHandleDirectPresence((WPARAM)h, 0, gch->dwData);
		break;

	case IDM_LINK0: case IDM_LINK1: case IDM_LINK2: case IDM_LINK3: case IDM_LINK4:
	case IDM_LINK5: case IDM_LINK6: case IDM_LINK7: case IDM_LINK8: case IDM_LINK9:
		{
			int idx = IDM_LINK0;
			for (char *p = strstr(item->getTemp()->m_szStatusMessage, "http://"); p && *p; p = strstr(p + 1, "http://")) {
				if (idx == gch->dwData) {
					char *bufPtr, *url = mir_strdup(p);
					for (bufPtr = url; *bufPtr && !isspace(*bufPtr); ++bufPtr);
					*bufPtr++ = 0;
					Utils_OpenUrl(url);
					mir_free(url);
					break;
				}

				if (++idx > IDM_LINK9) break;
			}
		}
		break;

	case IDM_CPY_RJID:
		JabberCopyText(g_clistApi.hwndContactList, item->jid);
		break;

	case IDM_CPY_TOPIC:
		JabberCopyText(g_clistApi.hwndContactList, item->getTemp()->m_szStatusMessage);
		break;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////
// Sends a private message to a chat user

static void sttSendPrivateMessage(CJabberProto *ppro, JABBER_LIST_ITEM *item, const char *nick)
{
	char szFullJid[JABBER_MAX_JID_LEN];
	mir_snprintf(szFullJid, "%s/%s", item->jid, nick);
	MCONTACT hContact = ppro->DBCreateContact(szFullJid, nullptr, true, false);
	if (hContact != 0) {
		pResourceStatus r(item->findResource(nick));
		if (r)
			ppro->setWord(hContact, "Status", r->m_iStatus);

		db_set_b(hContact, "CList", "Hidden", 1);
		ppro->setUString(hContact, "Nick", nick);
		db_set_dw(hContact, "Ignore", "Mask1", 0);
		CallService(MS_MSG_SENDMESSAGE, hContact, 0);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////
// General chat event processing hook

int CJabberProto::JabberGcEventHook(WPARAM, LPARAM lParam)
{
	GCHOOK *gch = (GCHOOK*)lParam;
	if (gch == nullptr)
		return 0;

	if (mir_strcmpi(gch->pszModule, m_szModuleName))
		return 0;

	T2Utf roomJid(gch->ptszID);
	JABBER_LIST_ITEM *item = ListGetItemPtr(LIST_CHATROOM, roomJid);
	if (item == nullptr)
		return 0;

	switch (gch->iType) {
	case GC_USER_MESSAGE:
		if (gch->ptszText && mir_wstrlen(gch->ptszText) > 0) {
			rtrimw(gch->ptszText);

			if (m_bJabberOnline) {
				char szId[100];
				int64_t id = (_time64(nullptr) << 16) + (GetTickCount() & 0xFFFF);
				_i64toa(id, szId, 36);

				wchar_t *buf = NEWWSTR_ALLOCA(gch->ptszText);
				Chat_UnescapeTags(buf);
				m_ThreadInfo->send(
					XmlNode("message") << XATTR("id", szId) << XATTR("to", item->jid) << XATTR("type", "groupchat")
					<< XCHILD("body", T2Utf(buf)));
			}
		}
		break;

	case GC_USER_PRIVMESS:
		sttSendPrivateMessage(this, item, T2Utf(gch->ptszUID));
		break;

	case GC_USER_LOGMENU:
		sttLogListHook(this, item, gch);
		break;

	case GC_USER_NICKLISTMENU:
		sttNickListHook(this, item, gch);
		break;

	case GC_USER_CHANMGR:
		m_ThreadInfo->send(
			XmlNodeIq(AddIQ(&CJabberProto::OnIqResultGetMuc, JABBER_IQ_TYPE_GET, item->jid))
			<< XQUERY(JABBER_FEAT_MUC_OWNER));
		break;
	}

	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void CJabberProto::AddMucListItem(JABBER_MUC_JIDLIST_INFO* jidListInfo, const char *str, const char *rsn)
{
	const char *field = (jidListInfo->type == MUC_BANLIST || strchr(str, '@')) ? "jid" : "nick";
	char *roomJid = jidListInfo->roomJid;
	if (jidListInfo->type == MUC_BANLIST) {
		AdminSetReason(roomJid, JABBER_FEAT_MUC_ADMIN, field, str, "affiliation", "outcast", rsn);
		AdminGet(roomJid, JABBER_FEAT_MUC_ADMIN, "affiliation", "outcast", &CJabberProto::OnIqResultMucGetBanList);
	}
}

void CJabberProto::AddMucListItem(JABBER_MUC_JIDLIST_INFO* jidListInfo, const char *str)
{
	const char *field = (jidListInfo->type == MUC_BANLIST || strchr(str, '@')) ? "jid" : "nick";
	char *roomJid = jidListInfo->roomJid;

	switch (jidListInfo->type) {
	case MUC_VOICELIST:
		AdminSet(roomJid, JABBER_FEAT_MUC_ADMIN, field, str, "role", "participant");
		AdminGet(roomJid, JABBER_FEAT_MUC_ADMIN, "role", "participant", &CJabberProto::OnIqResultMucGetVoiceList);
		break;
	case MUC_MEMBERLIST:
		AdminSet(roomJid, JABBER_FEAT_MUC_ADMIN, field, str, "affiliation", "member");
		AdminGet(roomJid, JABBER_FEAT_MUC_ADMIN, "affiliation", "member", &CJabberProto::OnIqResultMucGetMemberList);
		break;
	case MUC_MODERATORLIST:
		AdminSet(roomJid, JABBER_FEAT_MUC_ADMIN, field, str, "role", "moderator");
		AdminGet(roomJid, JABBER_FEAT_MUC_ADMIN, "role", "moderator", &CJabberProto::OnIqResultMucGetModeratorList);
		break;
	case MUC_BANLIST:
		AdminSet(roomJid, JABBER_FEAT_MUC_ADMIN, field, str, "affiliation", "outcast");
		AdminGet(roomJid, JABBER_FEAT_MUC_ADMIN, "affiliation", "outcast", &CJabberProto::OnIqResultMucGetBanList);
		break;
	case MUC_ADMINLIST:
		AdminSet(roomJid, JABBER_FEAT_MUC_ADMIN, field, str, "affiliation", "admin");
		AdminGet(roomJid, JABBER_FEAT_MUC_ADMIN, "affiliation", "admin", &CJabberProto::OnIqResultMucGetAdminList);
		break;
	case MUC_OWNERLIST:
		AdminSet(roomJid, JABBER_FEAT_MUC_ADMIN, field, str, "affiliation", "owner");
		AdminGet(roomJid, JABBER_FEAT_MUC_ADMIN, "affiliation", "owner", &CJabberProto::OnIqResultMucGetOwnerList);
		break;
	}
}

void CJabberProto::DeleteMucListItem(JABBER_MUC_JIDLIST_INFO *jidListInfo, const char *jid)
{
	switch (jidListInfo->type) {
	case MUC_VOICELIST:		// change role to visitor (from participant)
		AdminSet(jidListInfo->roomJid, JABBER_FEAT_MUC_ADMIN, "jid", jid, "role", "visitor");
		break;
	case MUC_BANLIST:		// change affiliation to none (from outcast)
	case MUC_MEMBERLIST:	// change affiliation to none (from member)
		AdminSet(jidListInfo->roomJid, JABBER_FEAT_MUC_ADMIN, "jid", jid, "affiliation", "none");
		break;
	case MUC_MODERATORLIST:	// change role to participant (from moderator)
		AdminSet(jidListInfo->roomJid, JABBER_FEAT_MUC_ADMIN, "jid", jid, "role", "participant");
		break;
	case MUC_ADMINLIST:		// change affiliation to member (from admin)
		AdminSet(jidListInfo->roomJid, JABBER_FEAT_MUC_ADMIN, "jid", jid, "affiliation", "member");
		break;
	case MUC_OWNERLIST:		// change affiliation to admin (from owner)
		AdminSet(jidListInfo->roomJid, JABBER_FEAT_MUC_ADMIN, "jid", jid, "affiliation", "admin");
		break;
	}
}
