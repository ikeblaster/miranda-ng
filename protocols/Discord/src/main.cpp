/*
Copyright © 2016-19 Miranda NG team

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "stdafx.h"

CMPlugin g_plugin;

HWND g_hwndHeartbeat;

/////////////////////////////////////////////////////////////////////////////////////////

PLUGININFOEX pluginInfoEx = {
	sizeof(PLUGININFOEX),
	__PLUGIN_NAME,
	PLUGIN_MAKE_VERSION(__MAJOR_VERSION, __MINOR_VERSION, __RELEASE_NUM, __BUILD_NUM),
	__DESCRIPTION,
	__AUTHOR,
	__COPYRIGHT,
	__AUTHORWEB,
	UNICODE_AWARE,
	// {88928401-2CE8-4568-AAA7-226141870CBF}
	{ 0x88928401, 0x2ce8, 0x4568, { 0xaa, 0xa7, 0x22, 0x61, 0x41, 0x87, 0x0c, 0xbf } }
};

CMPlugin::CMPlugin() :
	ACCPROTOPLUGIN<CDiscordProto>("Discord", pluginInfoEx)
{
	SetUniqueId(DB_KEY_ID);
}

/////////////////////////////////////////////////////////////////////////////////////////
// Interface information

extern "C" __declspec(dllexport) const MUUID MirandaInterfaces[] = { MIID_PROTOCOL, MIID_LAST };

/////////////////////////////////////////////////////////////////////////////////////////
// Load

IconItem g_iconList[] =
{
	{ LPGEN("Main icon"),   "main",      IDI_MAIN },
	{ LPGEN("Group chats"), "groupchat", IDI_GROUPCHAT }
};

int CMPlugin::Load()
{
	g_hwndHeartbeat = CreateWindowEx(0, L"STATIC", nullptr, 0, 0, 0, 0, 0, nullptr, nullptr, nullptr, nullptr);

	g_plugin.registerIcon("Protocols/Discord", g_iconList);
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////
// Unload

int CMPlugin::Unload()
{
	DestroyWindow(g_hwndHeartbeat);
	return 0;
}
