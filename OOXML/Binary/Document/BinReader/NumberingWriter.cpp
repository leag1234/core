/*
 * (c) Copyright Ascensio System SIA 2010-2023
 *
 * This program is a free software product. You can redistribute it and/or
 * modify it under the terms of the GNU Affero General Public License (AGPL)
 * version 3 as published by the Free Software Foundation. In accordance with
 * Section 7(a) of the GNU AGPL its Section 15 shall be amended to the effect
 * that Ascensio System SIA expressly excludes the warranty of non-infringement
 * of any third-party rights.
 *
 * This program is distributed WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR  PURPOSE. For
 * details, see the GNU AGPL at: http://www.gnu.org/licenses/agpl-3.0.html
 *
 * The  interactive user interfaces in modified source and object code versions
 * of the Program must display Appropriate Legal Notices, as required under
 * Section 5 of the GNU AGPL version 3.
 *
 * All the Product's GUI elements, including illustrations and icon sets, as
 * well as technical writing content are licensed under the terms of the
 * Creative Commons Attribution-ShareAlike 4.0 International. See the License
 * terms at http://creativecommons.org/licenses/by-sa/4.0/legalcode
 *
 */

#include "NumberingWriter.h"

namespace Writers
{
	NumberingWriter::NumberingWriter(std::wstring sDir) : m_sDir(sDir)
	{
	}
	static std::wstring TwipsToPtString(int nTwips)
	{
		std::wstring sResult = std::to_wstring(nTwips / 20);

		int nHundredths = (nTwips % 20) * 5;
		if (0 != nHundredths)
		{
			sResult += L".";
			if (0 == nHundredths % 10)
			{
				sResult += std::to_wstring(nHundredths / 10);
			}
			else
			{
				if (nHundredths < 10)
					sResult += L"0";
				sResult += std::to_wstring(nHundredths);
			}
		}
		return sResult;
	}
	void NumberingWriter::AddPicBullet(int nId, const std::wstring& sRId, int nWidthTwips, int nHeightTwips)
	{
		if (nWidthTwips <= 0 || nHeightTwips <= 0)
		{
			nWidthTwips = 180;	//9pt, the size Word uses
			nHeightTwips = 180;
		}
		std::wstring sShapeId = L"_x0000_i" + std::to_wstring(m_nPicBulletShapeId++);

		m_oPicBullets.WriteString(L"<w:numPicBullet w:numPicBulletId=\"" + std::to_wstring(nId) + L"\"><w:pict>");
		m_oPicBullets.WriteString(L"<v:shapetype id=\"_x0000_t75\" coordsize=\"21600,21600\" o:spt=\"75\" o:preferrelative=\"t\" path=\"m@4@5l@4@11@9@11@9@5xe\" filled=\"f\" stroked=\"f\"><v:stroke joinstyle=\"miter\"/><v:formulas><v:f eqn=\"if lineDrawn pixelLineWidth 0\"/><v:f eqn=\"sum @0 1 0\"/><v:f eqn=\"sum 0 0 @1\"/><v:f eqn=\"prod @2 1 2\"/><v:f eqn=\"prod @3 21600 pixelWidth\"/><v:f eqn=\"prod @3 21600 pixelHeight\"/><v:f eqn=\"sum @0 0 1\"/><v:f eqn=\"prod @6 1 2\"/><v:f eqn=\"prod @7 21600 pixelWidth\"/><v:f eqn=\"sum @8 21600 0\"/><v:f eqn=\"prod @7 21600 pixelHeight\"/><v:f eqn=\"sum @10 21600 0\"/></v:formulas><v:path o:extrusionok=\"f\" gradientshapeok=\"t\" o:connecttype=\"rect\"/><o:lock v:ext=\"edit\" aspectratio=\"t\"/></v:shapetype>");
		m_oPicBullets.WriteString(L"<v:shape id=\"" + sShapeId + L"\" type=\"#_x0000_t75\" style=\"width:" + TwipsToPtString(nWidthTwips) + L"pt;height:" + TwipsToPtString(nHeightTwips) + L"pt\" o:bullet=\"t\">");
		m_oPicBullets.WriteString(L"<v:imagedata r:id=\"" + sRId + L"\" o:title=\"\"/></v:shape></w:pict></w:numPicBullet>");
	}
	bool NumberingWriter::IsEmpty()
	{
		return 0 == m_oANum.GetCurSize();
	}
	void NumberingWriter::Write(bool bGlossary)
	{
		if(IsEmpty()) return;

		m_oWriter.WriteString(g_string_n_Start);
		m_oWriter.Write(m_oPicBullets);//CT_Numbering: numPicBullet elements go first
		m_oWriter.Write(m_oANum);
		m_oWriter.Write(m_oNumList);
		m_oWriter.WriteString(g_string_n_End);

		OOX::CPath filePath = m_sDir + FILE_SEPARATOR_STR +_T("word") + (bGlossary ? (FILE_SEPARATOR_STR + std::wstring(L"glossary")) : L"") + FILE_SEPARATOR_STR + _T("numbering.xml");

		NSFile::CFileBinary oFile;
		oFile.CreateFileW(filePath.GetPath());

		oFile.WriteStringUTF8(m_oWriter.GetData());
		oFile.CloseFile();
	}
}
