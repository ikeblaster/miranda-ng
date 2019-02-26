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
// Agent registration progress dialog

class CAgentRegProgressDlg : public CJabberDlgBase
{
	CCtrlButton m_ok;

public:
	CAgentRegProgressDlg(CJabberProto *_ppro, HWND _owner) :
		CJabberDlgBase(_ppro, IDD_OPT_REGISTER),
		m_ok(this, IDOK)
	{
		SetParent(_owner);
		m_ok.OnClick = Callback(this, &CAgentRegProgressDlg::OnOk);
	}

	bool OnInitDialog() override
	{
		m_proto->m_hwndRegProgress = m_hwnd;
		SetWindowText(m_hwnd, TranslateT("Jabber Agent Registration"));
		TranslateDialogDefault(m_hwnd);
		return true;
	}

	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam) override
	{
		if (msg == WM_JABBER_REGDLG_UPDATE) {
			if ((wchar_t*)lParam == nullptr)
				SetDlgItemText(m_hwnd, IDC_REG_STATUS, TranslateT("No message"));
			else
				SetDlgItemText(m_hwnd, IDC_REG_STATUS, (wchar_t*)lParam);

			SendDlgItemMessage(m_hwnd, IDC_PROGRESS_REG, PBM_SETPOS, wParam, 0);

			if (wParam >= 100)
				m_ok.SetText(TranslateT("OK"));
		}

		return CJabberDlgBase::DlgProc(msg, wParam, lParam);
	}

	void OnOk(CCtrlButton*)
	{
		m_proto->m_hwndRegProgress = nullptr;
		EndDialog(m_hwnd, 0);
	}
};

/////////////////////////////////////////////////////////////////////////////////////////
// Transport registration form

class CAgentRegDlg : public CJabberDlgBase
{
	int m_curPos;
	int m_formHeight, m_frameHeight;
	RECT m_frameRect;
	TiXmlDocument m_doc;
	TiXmlElement *m_agentRegIqNode;
	char *m_jid;

	CCtrlButton m_submit;
	HWND m_statusBar;

public:
	CAgentRegDlg(CJabberProto *_ppro, char *_jid) :
		CJabberDlgBase(_ppro, IDD_FORM),
		m_submit(this, IDC_SUBMIT),
		m_jid(_jid),
		m_agentRegIqNode(nullptr)
	{
		m_submit.OnClick = Callback(this, &CAgentRegDlg::OnSubmit);
	}

	bool OnInitDialog() override
	{
		EnableWindow(GetParent(m_hwnd), FALSE);
		m_proto->m_hwndAgentRegInput = m_hwnd;
		SetWindowText(m_hwnd, TranslateT("Jabber Agent Registration"));
		SetDlgItemText(m_hwnd, IDC_SUBMIT, TranslateT("Register"));

		m_statusBar = CreateWindowExW(0, STATUSCLASSNAME, nullptr, WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP, 0, 0, 0, 0, m_hwnd, nullptr, g_plugin.getInst(), nullptr);
		SendMessage(m_statusBar, WM_SIZE, 0, 0);
		SetWindowTextW(m_statusBar, TranslateT("Please wait..."));

		m_proto->m_ThreadInfo->send(
			XmlNodeIq(m_proto->AddIQ(&CJabberProto::OnIqResultGetRegister, JABBER_IQ_TYPE_GET, m_jid))
			<< XQUERY(JABBER_FEAT_REGISTER));

		// Enable WS_EX_CONTROLPARENT on IDC_FRAME (so tab stop goes through all its children)
		LONG_PTR frameExStyle = GetWindowLongPtr(GetDlgItem(m_hwnd, IDC_FRAME), GWL_EXSTYLE);
		frameExStyle |= WS_EX_CONTROLPARENT;
		SetWindowLongPtr(GetDlgItem(m_hwnd, IDC_FRAME), GWL_EXSTYLE, frameExStyle);
		return true;
	}

	void OnDestroy() override
	{
		JabberFormDestroyUI(GetDlgItem(m_hwnd, IDC_FRAME));
		m_proto->m_hwndAgentRegInput = nullptr;
		EnableWindow(GetParent(m_hwnd), TRUE);
		SetActiveWindow(GetParent(m_hwnd));
	}

	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam) override
	{
		switch (msg) {
		case WM_CTLCOLORSTATIC:
			switch (GetDlgCtrlID((HWND)lParam)) {
			case IDC_WHITERECT: case IDC_INSTRUCTION: case IDC_TITLE:
				return (INT_PTR)GetStockObject(WHITE_BRUSH);
			default:
				return 0;
			}

		case WM_JABBER_REGINPUT_ACTIVATE:
			if (wParam == 1) { // success
				// lParam = <iq/> node from agent JID as a result of "get jabber:iq:register"
				HWND hwndFrame = GetDlgItem(m_hwnd, IDC_FRAME);
				SetWindowTextW(m_statusBar, L"");

				if ((m_agentRegIqNode = (TiXmlElement*)lParam) == nullptr)
					return TRUE;
				
				m_agentRegIqNode = m_agentRegIqNode->DeepClone(&m_doc)->ToElement();
				auto *queryNode = XmlFirstChild(m_agentRegIqNode, "query");
				if (queryNode == nullptr)
					return TRUE;

				m_curPos = 0;
				GetClientRect(GetDlgItem(m_hwnd, IDC_FRAME), &m_frameRect);

				RECT rect;
				GetClientRect(GetDlgItem(m_hwnd, IDC_VSCROLL), &rect);
				m_frameRect.right -= (rect.right - rect.left);
				GetClientRect(GetDlgItem(m_hwnd, IDC_FRAME), &rect);
				m_frameHeight = rect.bottom - rect.top;

				if (auto *xNode = XmlFirstChild(queryNode, "x")) {
					// use new jabber:x:data form
					if (const char *pszText = XmlGetChildText(xNode, "instructions"))
						JabberFormSetInstruction(m_hwnd, pszText);

					JabberFormCreateUI(hwndFrame, xNode, &m_formHeight);
				}
				else {
					// use old registration information form
					TJabberFormLayoutInfo layout_info(hwndFrame, false);
					for (auto *n : TiXmlEnum(queryNode)) {
						const char *pszName = n->Name();
						if (pszName) {
							if (!mir_strcmp(pszName, "instructions")) {
								JabberFormSetInstruction(m_hwnd, n->GetText());
							}
							else if (!mir_strcmp(pszName, "key") || !mir_strcmp(pszName, "registered")) {
								// do nothing
							}
							else if (!mir_strcmp(pszName, "password"))
								layout_info.AppendControl(JFORM_CTYPE_TEXT_PRIVATE, pszName, n->GetText());
							else 	// everything else is a normal text field
								layout_info.AppendControl(JFORM_CTYPE_TEXT_SINGLE, pszName, n->GetText());
						}
					}
					layout_info.OrderControls(&m_formHeight);
				}

				if (m_formHeight > m_frameHeight) {
					HWND hwndScroll;

					hwndScroll = GetDlgItem(m_hwnd, IDC_VSCROLL);
					EnableWindow(hwndScroll, TRUE);
					SetScrollRange(hwndScroll, SB_CTL, 0, m_formHeight - m_frameHeight, FALSE);
					m_curPos = 0;
				}

				EnableWindow(GetDlgItem(m_hwnd, IDC_SUBMIT), TRUE);
			}
			else if (wParam == 0) {
				// lParam = error message
				SetWindowText(m_statusBar, (const wchar_t *)lParam);
			}
			return TRUE;

		case WM_VSCROLL:
			int pos = m_curPos;
			switch (LOWORD(wParam)) {
				case SB_LINEDOWN:   pos += 10;                    break;
				case SB_LINEUP:     pos -= 10;                    break;
				case SB_PAGEDOWN:   pos += (m_frameHeight - 10);  break;
				case SB_PAGEUP:     pos -= (m_frameHeight - 10);  break;
				case SB_THUMBTRACK: pos = HIWORD(wParam);         break;
			}
			if (pos > (m_formHeight - m_frameHeight))
				pos = m_formHeight - m_frameHeight;
			if (pos < 0)
				pos = 0;
			if (m_curPos != pos) {
				ScrollWindow(GetDlgItem(m_hwnd, IDC_FRAME), 0, m_curPos - pos, nullptr, &(m_frameRect));
				SetScrollPos(GetDlgItem(m_hwnd, IDC_VSCROLL), SB_CTL, pos, TRUE);
				m_curPos = pos;
			}
		}

		return CJabberDlgBase::DlgProc(msg, wParam, lParam);
	}

	void OnSubmit(CCtrlButton*)
	{
		if (m_agentRegIqNode == nullptr)
			return;

		auto *queryNode = XmlFirstChild(m_agentRegIqNode, "query");
		const char *from = XmlGetAttr(m_agentRegIqNode, "from");
		if (from == nullptr || queryNode == nullptr)
			return;
		
		HWND hwndFrame = GetDlgItem(m_hwnd, IDC_FRAME);
		wchar_t *str2 = (wchar_t*)alloca(sizeof(wchar_t) * 128);
		int id = 0;

		XmlNodeIq iq(m_proto->AddIQ(&CJabberProto::OnIqResultSetRegister, JABBER_IQ_TYPE_SET, from));
		TiXmlElement *query = iq << XQUERY(JABBER_FEAT_REGISTER);

		if (auto *xNode = XmlFirstChild(queryNode, "x")) {
			// use new jabber:x:data form
			JabberFormGetData(hwndFrame, query, xNode);
		}
		else {
			// use old registration information form
			for (auto *n : TiXmlEnum(queryNode)) {
				const char *pszName = n->Name();
				if (pszName) {
					if (!mir_strcmp(pszName, "key")) {
						// field that must be passed along with the registration
						if (n->GetText())
							XmlAddChildA(query, pszName, n->GetText());
						else
							XmlAddChild(query, pszName);
					}
					else if (!mir_strcmp(pszName, "registered") || !mir_strcmp(pszName, "instructions")) {
						// do nothing, we will skip these
					}
					else {
						GetDlgItemText(hwndFrame, id, str2, 128);
						XmlAddChildA(query, pszName, T2Utf(str2).get());
						id++;
					}
				}
			}
		}

		m_proto->m_ThreadInfo->send(iq);

		CAgentRegProgressDlg(m_proto, m_hwnd).DoModal();

		Close();
	}
};

void CJabberProto::RegisterAgent(HWND, char *jid)
{
	(new CAgentRegDlg(this, jid))->Show();
}
