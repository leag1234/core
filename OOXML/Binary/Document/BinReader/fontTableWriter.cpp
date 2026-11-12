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

#include "fontTableWriter.h"
#include "../../../../DesktopEditor/common/Directory.h"

namespace Writers
{
	FontTableWriter::FontTableWriter(std::wstring sDir, std::wstring sFontDir, bool bNoFontDir) : m_sDir(sDir)
	{
		m_pApplicationFonts = NSFonts::NSApplication::Create();
		m_pFontManager = NULL;
		if(!bNoFontDir)
		{
			if(sFontDir.empty())
				m_pApplicationFonts->Initialize();
			else
				m_pApplicationFonts->InitializeFromFolder(sFontDir);
			m_pFontManager = m_pApplicationFonts->GenerateFontManager();
		}
	}
	FontTableWriter::~FontTableWriter()
	{
		RELEASEOBJECT(m_pFontManager);
		RELEASEOBJECT(m_pApplicationFonts);
	}

	void FontTableWriter::Write(bool bGlossary)
	{
		m_oWriter.WriteString(g_string_ft_Start);

		//Those fonts that we always write in FontTable
		bool bCalibri = false;
		bool bTimes = false;
		bool bCambria = false;
		std::set<std::wstring> setWritten;
		for (boost::unordered_map<std::wstring, char>::const_iterator it = m_mapFonts.begin(); it != m_mapFonts.end(); ++it)
		{
			const std::wstring& sFontName = it->first;
			if(_T("Calibri") == sFontName)
				bCalibri = true;
			else if(_T("Times New Roman") == sFontName)
				bTimes = true;
			else if(_T("Cambria") == sFontName)
				bCambria = true;
			WriteFont(sFontName);
			setWritten.insert(sFontName);
		}
		if(false == bCalibri)
		{
			WriteFont(_T("Calibri"));
			setWritten.insert(_T("Calibri"));
		}
		if(false == bTimes)
		{
			WriteFont(_T("Times New Roman"));
			setWritten.insert(_T("Times New Roman"));
		}
		if(false == bCambria)
		{
			WriteFont(_T("Cambria"));
			setWritten.insert(_T("Cambria"));
		}
		// embedded fonts of the source document that the text no longer uses are kept as well
		for (size_t i = 0; i < m_arEmbedded.size(); ++i)
		{
			if (setWritten.insert(m_arEmbedded[i].m_sName).second)
				WriteFont(m_arEmbedded[i].m_sName);
		}

		m_oWriter.WriteString(g_string_ft_End);

		if (!m_arEmbedded.empty())
		{
			// word/_rels/fontTable.xml.rels: one relationship per embedded face (ids are those of the font table)
			std::wstring sRels = L"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">";
			for (size_t i = 0; i < m_arEmbedded.size(); ++i)
			{
				for (int nSlot = 0; nSlot < 4; ++nSlot)
				{
					if (!m_arEmbedded[i].m_aHas[nSlot])
						continue;
					const std::wstring& sRId = m_arEmbedded[i].m_aRId[nSlot];
					sRels += L"<Relationship Id=\"" + sRId + L"\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/font\" Target=\"fonts/font" + sRId.substr(3) + L".odttf\"/>";
				}
			}
			sRels += L"</Relationships>";
			NSFile::CFileBinary oRels;
			if (oRels.CreateFileW(m_sDir + FILE_SEPARATOR_STR + _T("word") + FILE_SEPARATOR_STR + _T("_rels") + FILE_SEPARATOR_STR + _T("fontTable.xml.rels")))
			{
				oRels.WriteStringUTF8(sRels);
				oRels.CloseFile();
			}
		}

		OOX::CPath filePath = m_sDir + FILE_SEPARATOR_STR +_T("word") + (bGlossary ? (FILE_SEPARATOR_STR + std::wstring(L"glossary")) : L"") + FILE_SEPARATOR_STR + _T("fontTable.xml");

		NSFile::CFileBinary oFile;
		oFile.CreateFileW(filePath.GetPath());

		oFile.WriteStringUTF8(m_oWriter.GetData());
		oFile.CloseFile();
	}
	void FontTableWriter::WriteFont(std::wstring sFontName)
		{
			std::wstring sPanose;
			bool bUsePanose = false;
			if(NULL != m_pFontManager)
			{
				NSFonts::CFontSelectFormat oFontSelectFormat;
				oFontSelectFormat.wsName = new std::wstring;
				*oFontSelectFormat.wsName = sFontName;

				NSFonts::CFontInfo* pFontInfo = m_pFontManager->GetFontInfoByParams(oFontSelectFormat);
				if(NULL != pFontInfo)
				{
					for (size_t i = 0; i < 10; ++i)
					{
						BYTE cElem = pFontInfo->m_aPanose[i];
						if(0 != cElem)
							bUsePanose = true;
						sPanose += XmlUtils::ToString(cElem, L"%02X");
					}

				}
			}

			const std::wstring sFontNameRaw = sFontName;
			sFontName = XmlUtils::EncodeXmlString(sFontName);
			m_oWriter.WriteString(_T("<w:font w:name=\"") + sFontName + _T("\">"));
			if (bUsePanose && !sPanose.empty())
				m_oWriter.WriteString(_T("<w:panose1 w:val=\"")+sPanose+_T("\"/>"));
			for (size_t i = 0; i < m_arEmbedded.size(); ++i)
			{
				if (m_arEmbedded[i].m_sName == sFontNameRaw)
				{
					WriteEmbedElements(m_arEmbedded[i]);
					break;
				}
			}
			m_oWriter.WriteString(std::wstring(_T("</w:font>")));
		}
	void FontTableWriter::WriteEmbedElements(const CEmbeddedFont& oFont)
	{
		static const wchar_t* aNames[4] = { L"w:embedRegular", L"w:embedBold", L"w:embedItalic", L"w:embedBoldItalic" };
		for (int nSlot = 0; nSlot < 4; ++nSlot)
		{
			if (!oFont.m_aHas[nSlot])
				continue;
			m_oWriter.WriteString(std::wstring(L"<") + aNames[nSlot] + L" r:id=\"" + oFont.m_aRId[nSlot] + L"\" w:fontKey=\"" +
				XmlUtils::EncodeXmlString(oFont.m_aKey[nSlot]) + L"\"" + (oFont.m_aSubsetted[nSlot] ? L" w:subsetted=\"1\"" : L"") + L"/>");
		}
	}
	bool FontTableWriter::HasSubsettedEmbedded() const
	{
		for (size_t i = 0; i < m_arEmbedded.size(); ++i)
			for (int nSlot = 0; nSlot < 4; ++nSlot)
				if (m_arEmbedded[i].m_aHas[nSlot] && m_arEmbedded[i].m_aSubsetted[nSlot])
					return true;
		return false;
	}
	bool FontTableWriter::AddEmbeddedFace(const std::wstring& sFontName, int nSlot, const std::wstring& sKey, bool bSubsetted, const std::wstring& sPlainPath)
	{
		if (nSlot < 0 || nSlot > 3 || sFontName.empty() || sPlainPath.empty() || m_nEmbeddedCount >= 64)
			return false;
		unsigned char aKey[16];
		if (!OOX::FontObfuscation::GuidToKey(sKey, aKey))
			return false;

		NSFile::CFileBinary oFile;
		if (!oFile.OpenFile(sPlainPath))
			return false;
		long nSize = oFile.GetFileSize();
		if (nSize < 32 || (size_t)nSize > OOX::FontObfuscation::c_nMaxFontBytes || m_nEmbeddedTotal + (size_t)nSize > OOX::FontObfuscation::c_nMaxTotalBytes)
		{
			oFile.CloseFile();
			return false;
		}
		std::vector<unsigned char> aData((size_t)nSize);
		DWORD nRead = 0;
		bool bRead = oFile.ReadFile(&aData[0], (DWORD)nSize, nRead);
		oFile.CloseFile();
		// only a real font is written (a media file that is not an sfnt is never embedded)
		if (!bRead || nRead != (DWORD)nSize || !OOX::FontObfuscation::IsSfnt(&aData[0], aData.size()))
			return false;
		// the XOR is its own inverse: the same key gives back the bytes of the original .odttf
		if (!OOX::FontObfuscation::Xor(&aData[0], aData.size(), aKey))
			return false;

		std::wstring sFontDir = m_sDir + FILE_SEPARATOR_STR + _T("word") + FILE_SEPARATOR_STR + _T("fonts");
		NSDirectory::CreateDirectory(m_sDir + FILE_SEPARATOR_STR + _T("word"));
		NSDirectory::CreateDirectory(m_sDir + FILE_SEPARATOR_STR + _T("word") + FILE_SEPARATOR_STR + _T("_rels"));
		NSDirectory::CreateDirectory(sFontDir);
		int nNumber = m_nEmbeddedCount + 1;
		NSFile::CFileBinary oOut;
		if (!oOut.CreateFileW(sFontDir + FILE_SEPARATOR_STR + _T("font") + std::to_wstring(nNumber) + _T(".odttf")))
			return false;
		bool bWritten = oOut.WriteFile(&aData[0], (DWORD)aData.size());
		oOut.CloseFile();
		if (!bWritten)
			return false;

		++m_nEmbeddedCount;
		m_nEmbeddedTotal += aData.size();
		size_t nIndex = m_arEmbedded.size();
		for (size_t i = 0; i < m_arEmbedded.size(); ++i)
			if (m_arEmbedded[i].m_sName == sFontName) { nIndex = i; break; }
		if (nIndex == m_arEmbedded.size())
		{
			m_arEmbedded.push_back(CEmbeddedFont());
			m_arEmbedded.back().m_sName = sFontName;
		}
		CEmbeddedFont& oFont = m_arEmbedded[nIndex];
		oFont.m_aHas[nSlot] = true;
		oFont.m_aRId[nSlot] = L"rId" + std::to_wstring(nNumber);
		oFont.m_aKey[nSlot] = sKey;
		oFont.m_aSubsetted[nSlot] = bSubsetted;
		return true;
	}
}

