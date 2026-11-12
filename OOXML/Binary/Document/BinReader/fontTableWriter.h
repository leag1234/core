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
#pragma once

#include "../../Sheets/Common/Common.h"
#include "../../../../DesktopEditor/graphics/pro/Fonts.h"
#include <boost/unordered_map.hpp>
#include <vector>
#include <set>
#include "../../../DocxFormat/FontObfuscation.h"

namespace Writers
{
	static std::wstring g_string_ft_Start = _T("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><w:fonts xmlns:mc=\"http://schemas.openxmlformats.org/markup-compatibility/2006\" xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\" xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\" xmlns:w14=\"http://schemas.microsoft.com/office/word/2010/wordml\" xmlns:w15=\"http://schemas.microsoft.com/office/word/2012/wordml\" mc:Ignorable=\"w14 w15\">");
    static std::wstring g_string_ft_End = _T("</w:fonts>");

	class FontTableWriter
	{
		NSStringUtils::CStringBuilder	m_oWriter;
        std::wstring					m_sDir;
        NSFonts::IApplicationFonts*		m_pApplicationFonts;
        NSFonts::IFontManager*			m_pFontManager;

	public:
        boost::unordered_map<std::wstring, char> m_mapFonts;

		// Fonts embedded in the source document (word/fonts/*.odttf), kept when the document is saved.
		struct CEmbeddedFont
		{
			std::wstring	m_sName;
			bool			m_aHas[4];			// slots: regular, bold, italic, bold italic
			std::wstring	m_aRId[4];
			std::wstring	m_aKey[4];
			bool			m_aSubsetted[4];
			CEmbeddedFont() { for (int i = 0; i < 4; ++i) m_aHas[i] = false, m_aSubsetted[i] = false; }
		};
		std::vector<CEmbeddedFont>	m_arEmbedded;

		// Re-creates the obfuscated font from the plain font file of the media folder, writes it as
		// word/fonts/fontN.odttf and remembers it for fontTable.xml. False (nothing written) when the face is not usable.
		bool AddEmbeddedFace(const std::wstring& sFontName, int nSlot, const std::wstring& sKey, bool bSubsetted, const std::wstring& sPlainPath);
		bool HasEmbedded() const { return !m_arEmbedded.empty(); }
		bool HasSubsettedEmbedded() const;
	private:
		size_t	m_nEmbeddedTotal = 0;
		int		m_nEmbeddedCount = 0;
		void WriteEmbedElements(const CEmbeddedFont& oFont);
	public:

		FontTableWriter(std::wstring sDir, std::wstring sFontDir, bool bNoFontDir);
		~FontTableWriter();

		void Write(bool bGlossary = false);
		void WriteFont(std::wstring sFontName);
	};
}

