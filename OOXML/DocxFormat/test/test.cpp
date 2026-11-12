/*
 * SPDX-FileCopyrightText: 2026 Euro-Office contributors
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "gtest/gtest.h"

#include "../Numbering.h"
#include "../Font.h"
#include "../FontObfuscation.h"
#include "../Logic/RunContent.h"
#include "../Logic/ParagraphProperty.h"
#include "../Settings/Settings.h"
#include <vector>
// Needed for compile-time completeness of OOX::Logic::CParagraphProperty's own
// members (CLvl's implicit copy constructor, triggered by any return-by-value
// of a CLvl, requires this even though this file never names the type
// directly) -- not for link-time symbol resolution, which is why the file can
// still avoid constructing CParagraphProperty/CRunProperty objects itself.
#include "../Logic/SectionProperty.h"

// DOCX bullets/numbers were misclassified by Microsoft 365 Online. Root cause:
// OOX::Numbering::CLvl::toXML() emitted its <w:lvl> children in alphabetical order
// instead of the ECMA-376 CT_Lvl schema sequence (start, numFmt, lvlRestart, pStyle,
// isLgl, suff, lvlText, lvlPicBulletId, legacy, lvlJc, pPr, rPr). This suite asserts
// the schema order directly against the production serializer/reader, for both list
// types.

namespace
{
	OOX::Numbering::CLvl BuildLvl(SimpleTypes::ENumberFormat eFormat, const std::wstring& sLvlText)
	{
		OOX::Numbering::CLvl oLvl;

		oLvl.m_oStart.Init();
		*oLvl.m_oStart = ComplexTypes::Word::CDecimalNumber();
		oLvl.m_oStart->m_oVal = 1;

		oLvl.m_oNumFmt.Init();
		oLvl.m_oNumFmt->m_oVal.Init();
		*oLvl.m_oNumFmt->m_oVal = SimpleTypes::CNumberFormat(eFormat);

		oLvl.m_oIsLgl.Init();
		*oLvl.m_oIsLgl = ComplexTypes::Word::COnOff2();
		oLvl.m_oIsLgl->m_oVal = SimpleTypes::COnOff(SimpleTypes::onoffFalse);

		oLvl.m_oSuffix.Init();
		*oLvl.m_oSuffix = ComplexTypes::Word::CLevelSuffix();
		oLvl.m_oSuffix->m_oVal = SimpleTypes::CLevelSuffix(SimpleTypes::levelsuffixTab);

		oLvl.m_oLvlText.Init();
		*oLvl.m_oLvlText = ComplexTypes::Word::CLevelText();
		oLvl.m_oLvlText->m_sVal = sLvlText;

		oLvl.m_oLvlJc.Init();
		*oLvl.m_oLvlJc = ComplexTypes::Word::CJc();
		oLvl.m_oLvlJc->m_oVal = SimpleTypes::CJc(SimpleTypes::jcLeft);

		return oLvl;
	}

	// Populates all twelve CT_Lvl child elements (BuildLvl above only covers the
	// six most commonly emitted ones), so the schema-order assertion has full
	// sequence coverage instead of skipping whichever fields are absent. Built
	// via fromXML() -- which the pre-fix-order tolerance tests below already
	// exercise successfully -- rather than by default-constructing
	// OOX::Logic::CParagraphProperty/CRunProperty directly: those pull in the
	// PPTXFormat::Logic dependency graph (UniFill, Ln, EffectProperties, ...),
	// whose symbols have hidden visibility inside the shared x2tlib.so and
	// aren't resolvable from a separate test binary linked against it.
	OOX::Numbering::CLvl BuildFullyPopulatedLvl()
	{
		std::wstring sXml =
			L"<w:lvl w:ilvl=\"0\">"
			L"<w:start w:val=\"1\"/>"
			L"<w:numFmt w:val=\"decimal\"/>"
			L"<w:lvlRestart w:val=\"1\"/>"
			L"<w:pStyle w:val=\"ListParagraph\"/>"
			L"<w:isLgl w:val=\"false\"/>"
			L"<w:suff w:val=\"tab\"/>"
			L"<w:lvlText w:val=\"%1.\"/>"
			L"<w:lvlPicBulletId w:val=\"0\"/>"
			L"<w:legacy w:legacy=\"1\" w:legacyIndent=\"0\" w:legacySpace=\"0\"/>"
			L"<w:lvlJc w:val=\"left\"/>"
			L"<w:pPr><w:ind w:left=\"0\"/></w:pPr>"
			L"<w:rPr><w:sz w:val=\"24\"/></w:rPr>"
			L"</w:lvl>";

		XmlUtils::CXmlLiteReader oReader;
		oReader.FromString(sXml);
		oReader.ReadNextNode();

		OOX::Numbering::CLvl oLvl;
		oLvl.fromXML(oReader);
		return oLvl;
	}

	// Returns, for each ECMA-376 CT_Lvl element name (in schema order), its first
	// occurrence offset in the serialized XML, or std::wstring::npos if absent.
	std::vector<size_t> SchemaOrderPositions(const std::wstring& sXml)
	{
		static const wchar_t* schemaOrderTags[] = {
			L"<w:start ", L"<w:numFmt ", L"<w:lvlRestart ", L"<w:pStyle ",
			L"<w:isLgl ", L"<w:suff ", L"<w:lvlText ", L"<w:lvlPicBulletId ",
			L"<w:legacy ", L"<w:lvlJc ", L"<w:pPr", L"<w:rPr"
		};
		std::vector<size_t> positions;
		for (const wchar_t* tag : schemaOrderTags)
			positions.push_back(sXml.find(tag));
		return positions;
	}
}

TEST(DocxNumbering, LvlToXml_BulletLevel_ElementsInSchemaOrder)
{
	OOX::Numbering::CLvl oLvl = BuildLvl(SimpleTypes::numberformatBullet, L"·");
	std::wstring sXml = oLvl.toXML();

	std::vector<size_t> positions = SchemaOrderPositions(sXml);
	std::vector<size_t> present;
	for (size_t pos : positions)
		if (pos != std::wstring::npos)
			present.push_back(pos);

	// Every present element must appear in non-decreasing schema-sequence order.
	ASSERT_FALSE(present.empty()) << "no recognised <w:lvl> child elements were emitted";
	for (size_t i = 1; i < present.size(); ++i)
	{
		EXPECT_LT(present[i - 1], present[i])
			<< "element order violates ECMA-376 CT_Lvl schema sequence; got XML: "
			<< std::string(sXml.begin(), sXml.end());
	}

	EXPECT_NE(sXml.find(L"w:val=\"bullet\""), std::wstring::npos);
}

TEST(DocxNumbering, LvlToXml_DecimalLevel_ElementsInSchemaOrder)
{
	OOX::Numbering::CLvl oLvl = BuildLvl(SimpleTypes::numberformatDecimal, L"%1.");
	std::wstring sXml = oLvl.toXML();

	std::vector<size_t> positions = SchemaOrderPositions(sXml);
	std::vector<size_t> present;
	for (size_t pos : positions)
		if (pos != std::wstring::npos)
			present.push_back(pos);

	ASSERT_FALSE(present.empty()) << "no recognised <w:lvl> child elements were emitted";
	for (size_t i = 1; i < present.size(); ++i)
	{
		EXPECT_LT(present[i - 1], present[i])
			<< "element order violates ECMA-376 CT_Lvl schema sequence; got XML: "
			<< std::string(sXml.begin(), sXml.end());
	}

	EXPECT_NE(sXml.find(L"w:val=\"decimal\""), std::wstring::npos);
}

// The reader must classify numFmt correctly even when fed the pre-fix
// (non-schema-ordered) element order — this is what makes "open and re-save" a
// sufficient repair for already-produced defective files, without a separate
// migration tool, and confirms reading files not written by the current writer
// still works.
TEST(DocxNumbering, LvlFromXml_ToleratesPreFixElementOrder_Bullet)
{
	// Exact pre-fix element order, reproduced byte-for-byte from a real sample
	// file's abstractNumId=0 (bullet) level.
	std::wstring sXml =
		L"<w:lvl w:ilvl=\"0\">"
		L"<w:isLgl w:val=\"false\"/>"
		L"<w:lvlJc w:val=\"left\"/>"
		L"<w:lvlText w:val=\"·\"/>"
		L"<w:numFmt w:val=\"bullet\"/>"
		L"<w:start w:val=\"1\"/>"
		L"<w:suff w:val=\"tab\"/>"
		L"</w:lvl>";

	XmlUtils::CXmlLiteReader oReader;
	ASSERT_TRUE(oReader.FromString(sXml));
	ASSERT_TRUE(oReader.ReadNextNode());

	OOX::Numbering::CLvl oLvl;
	oLvl.fromXML(oReader);

	ASSERT_TRUE(oLvl.m_oNumFmt.IsInit());
	ASSERT_TRUE(oLvl.m_oNumFmt->m_oVal.IsInit());
	EXPECT_EQ(oLvl.m_oNumFmt->m_oVal->GetValue(), SimpleTypes::numberformatBullet);
}

TEST(DocxNumbering, LvlFromXml_ToleratesPreFixElementOrder_Decimal)
{
	// Exact pre-fix element order, reproduced byte-for-byte from a real sample
	// file's abstractNumId=1 (decimal) level.
	std::wstring sXml =
		L"<w:lvl w:ilvl=\"0\">"
		L"<w:isLgl w:val=\"false\"/>"
		L"<w:lvlJc w:val=\"left\"/>"
		L"<w:lvlText w:val=\"%1.\"/>"
		L"<w:numFmt w:val=\"decimal\"/>"
		L"<w:start w:val=\"1\"/>"
		L"<w:suff w:val=\"tab\"/>"
		L"</w:lvl>";

	XmlUtils::CXmlLiteReader oReader;
	ASSERT_TRUE(oReader.FromString(sXml));
	ASSERT_TRUE(oReader.ReadNextNode());

	OOX::Numbering::CLvl oLvl;
	oLvl.fromXML(oReader);

	ASSERT_TRUE(oLvl.m_oNumFmt.IsInit());
	ASSERT_TRUE(oLvl.m_oNumFmt->m_oVal.IsInit());
	EXPECT_EQ(oLvl.m_oNumFmt->m_oVal->GetValue(), SimpleTypes::numberformatDecimal);
}

// User-customised marker: classification must key on numFmt, not on lvlText. A
// customised, unrecognisable lvlText string must not change the result.
TEST(DocxNumbering, LvlToXml_CustomisedMarker_StillKeyedOnNumFmt)
{
	OOX::Numbering::CLvl oBullet = BuildLvl(SimpleTypes::numberformatBullet, L"✦"); // custom glyph
	OOX::Numbering::CLvl oOrdered = BuildLvl(SimpleTypes::numberformatDecimal, L"[[Step %1]]"); // custom format string

	EXPECT_NE(oBullet.toXML().find(L"w:val=\"bullet\""), std::wstring::npos);
	EXPECT_NE(oOrdered.toXML().find(L"w:val=\"decimal\""), std::wstring::npos);
}

TEST(DocxNumbering, LvlToXml_FullyPopulatedLevel_ElementsInSchemaOrder)
{
	OOX::Numbering::CLvl oLvl = BuildFullyPopulatedLvl();
	std::wstring sXml = oLvl.toXML();

	std::vector<size_t> positions = SchemaOrderPositions(sXml);
	for (size_t pos : positions)
		ASSERT_NE(pos, std::wstring::npos) << "fully populated level did not emit every CT_Lvl child element";

	for (size_t i = 1; i < positions.size(); ++i)
	{
		EXPECT_LT(positions[i - 1], positions[i])
			<< "element order violates ECMA-376 CT_Lvl schema sequence; got XML: "
			<< std::string(sXml.begin(), sXml.end());
	}
}

// Picture bullets (w:numPicBullet / w:lvlPicBulletId): the picture of the bullet (relationship id of the numbering
// part and size) must be read so that it can be written to the binary document and back.
namespace
{
std::wstring BuildPicBulletXml()
{
	// as written by Word: shapetype + shape with the size in the style and the image relationship
	return
		L"<w:numPicBullet w:numPicBulletId=\"3\">"
		L"<w:pict>"
		L"<v:shapetype id=\"_x0000_t75\" coordsize=\"21600,21600\" o:spt=\"75\" o:preferrelative=\"t\" path=\"m@4@5l@4@11@9@11@9@5xe\" filled=\"f\" stroked=\"f\">"
		L"<v:stroke joinstyle=\"miter\"/><v:path o:extrusionok=\"f\" gradientshapeok=\"t\" o:connecttype=\"rect\"/>"
		L"<o:lock v:ext=\"edit\" aspectratio=\"t\"/></v:shapetype>"
		L"<v:shape id=\"_x0000_i1030\" type=\"#_x0000_t75\" style=\"width:9pt;height:6.5pt\" o:bullet=\"t\">"
		L"<v:imagedata r:id=\"rId7\" o:title=\"BD10267_\"/>"
		L"</v:shape>"
		L"</w:pict>"
		L"</w:numPicBullet>";
}
}

TEST(DocxNumbering, NumPicBulletFromXml_ReadsIdImageRIdAndSize)
{
	XmlUtils::CXmlLiteReader oReader;
	ASSERT_TRUE(oReader.FromString(BuildPicBulletXml()));
	ASSERT_TRUE(oReader.ReadNextNode());

	OOX::Numbering::CNumPicBullet oPicBullet;
	oPicBullet.fromXML(oReader);

	ASSERT_TRUE(oPicBullet.m_oNumPicBulletId.IsInit());
	EXPECT_EQ(*oPicBullet.m_oNumPicBulletId, 3);
	EXPECT_EQ(oPicBullet.m_sImageRId, L"rId7");
	EXPECT_DOUBLE_EQ(oPicBullet.m_dWidthPt, 9.0);
	EXPECT_DOUBLE_EQ(oPicBullet.m_dHeightPt, 6.5);
}

TEST(DocxNumbering, NumPicBulletToXml_KeepsIdAndPicture)
{
	XmlUtils::CXmlLiteReader oReader;
	ASSERT_TRUE(oReader.FromString(BuildPicBulletXml()));
	ASSERT_TRUE(oReader.ReadNextNode());

	OOX::Numbering::CNumPicBullet oPicBullet;
	oPicBullet.fromXML(oReader);

	std::wstring sXml = oPicBullet.toXML();
	EXPECT_NE(sXml.find(L"<w:numPicBullet w:numPicBulletId=\"3\">"), std::wstring::npos);
	EXPECT_NE(sXml.find(L"<v:imagedata r:id=\"rId7\""), std::wstring::npos);
	EXPECT_NE(sXml.find(L"width:9pt;height:6.5pt"), std::wstring::npos);
	EXPECT_NE(sXml.find(L"</w:numPicBullet>"), std::wstring::npos);
}

TEST(DocxNumbering, NumPicBulletFromXml_WithoutPicture_HasNoImage)
{
	XmlUtils::CXmlLiteReader oReader;
	ASSERT_TRUE(oReader.FromString(L"<w:numPicBullet w:numPicBulletId=\"1\"></w:numPicBullet>"));
	ASSERT_TRUE(oReader.ReadNextNode());

	OOX::Numbering::CNumPicBullet oPicBullet;
	oPicBullet.fromXML(oReader);

	EXPECT_TRUE(oPicBullet.m_sImageRId.empty());
	EXPECT_EQ(oPicBullet.m_dWidthPt, 0.0);
	EXPECT_EQ(oPicBullet.m_dHeightPt, 0.0);
}

TEST(DocxNumbering, LvlFromXml_ReadsLvlPicBulletId)
{
	std::wstring sXml =
		L"<w:lvl w:ilvl=\"2\">"
		L"<w:start w:val=\"1\"/>"
		L"<w:numFmt w:val=\"bullet\"/>"
		L"<w:lvlText w:val=\"\u00B7\"/>"
		L"<w:lvlPicBulletId w:val=\"3\"/>"
		L"<w:lvlJc w:val=\"left\"/>"
		L"</w:lvl>";

	XmlUtils::CXmlLiteReader oReader;
	ASSERT_TRUE(oReader.FromString(sXml));
	ASSERT_TRUE(oReader.ReadNextNode());

	OOX::Numbering::CLvl oLvl;
	oLvl.fromXML(oReader);

	ASSERT_TRUE(oLvl.m_oLvlPicBulletId.IsInit());
	ASSERT_TRUE(oLvl.m_oLvlPicBulletId->m_oVal.IsInit());
	EXPECT_EQ(*oLvl.m_oLvlPicBulletId->m_oVal, 3);
	EXPECT_NE(oLvl.toXML().find(L"lvlPicBulletId"), std::wstring::npos);
}

// Regression: reading the picture of w:numPicBullet must leave the reader at the end of the element,
// otherwise the abstractNum that follows it is lost.
TEST(DocxNumbering, NumberingFromXml_PicBulletDoesNotSwallowNextAbstractNum)
{
	std::wstring sXml =
		L"<w:numbering>" + BuildPicBulletXml() +
		L"<w:abstractNum w:abstractNumId=\"0\"><w:lvl w:ilvl=\"0\"><w:numFmt w:val=\"bullet\"/></w:lvl></w:abstractNum>"
		L"<w:abstractNum w:abstractNumId=\"1\"><w:lvl w:ilvl=\"0\"><w:numFmt w:val=\"bullet\"/></w:lvl></w:abstractNum>"
		L"<w:num w:numId=\"1\"><w:abstractNumId w:val=\"0\"/></w:num>"
		L"</w:numbering>";

	XmlUtils::CXmlLiteReader oReader;
	ASSERT_TRUE(oReader.FromString(sXml));
	ASSERT_TRUE(oReader.ReadNextNode());

	OOX::Document* pDocument = NULL;
	OOX::CNumbering oNumbering(pDocument);
	oNumbering.fromXML(oReader);

	ASSERT_EQ(oNumbering.m_arrNumPicBullet.size(), (size_t)1);
	EXPECT_EQ(oNumbering.m_arrNumPicBullet[0]->m_sImageRId, L"rId7");
	EXPECT_EQ(oNumbering.m_arrAbstractNum.size(), (size_t)2);
	EXPECT_EQ(oNumbering.m_arrNum.size(), (size_t)1);
}

// ---- Embedded fonts (word/fonts/*.odttf): obfuscation of ECMA-376 Part 2, 17.8.1 ----

TEST(DocxEmbeddedFonts, GuidToKey_ReadsHexDigitsFromTheEnd)
{
	unsigned char aKey[16];
	ASSERT_TRUE(OOX::FontObfuscation::GuidToKey(L"{00112233-4455-6677-8899-AABBCCDDEEFF}", aKey));
	const unsigned char aExpected[16] = { 0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA, 0x99, 0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00 };
	for (int i = 0; i < 16; ++i)
		EXPECT_EQ(aKey[i], aExpected[i]) << i;

	// braces and case do not matter
	unsigned char aKey2[16];
	ASSERT_TRUE(OOX::FontObfuscation::GuidToKey(L"00112233-4455-6677-8899-aabbccddeeff", aKey2));
	for (int i = 0; i < 16; ++i)
		EXPECT_EQ(aKey2[i], aExpected[i]) << i;
}

TEST(DocxEmbeddedFonts, GuidToKey_RejectsMalformedGuids)
{
	unsigned char aKey[16];
	EXPECT_FALSE(OOX::FontObfuscation::GuidToKey(L"", aKey));
	EXPECT_FALSE(OOX::FontObfuscation::GuidToKey(L"{00112233-4455-6677-8899-AABBCCDDEE}", aKey));		// too short
	EXPECT_FALSE(OOX::FontObfuscation::GuidToKey(L"{00112233-4455-6677-8899-AABBCCDDEEFF00}", aKey));	// too long
	EXPECT_FALSE(OOX::FontObfuscation::GuidToKey(L"{0011223G-4455-6677-8899-AABBCCDDEEFF}", aKey));	// not hex
}

namespace
{
	// a font-like buffer: sfnt signature, then a recognizable pattern
	std::vector<unsigned char> BuildFont(size_t nSize, const char* sTag4)
	{
		std::vector<unsigned char> aFont(nSize);
		for (size_t i = 0; i < nSize; ++i)
			aFont[i] = (unsigned char)(i * 7 + 3);
		aFont[0] = sTag4[0]; aFont[1] = sTag4[1]; aFont[2] = sTag4[2]; aFont[3] = sTag4[3];
		return aFont;
	}
}

TEST(DocxEmbeddedFonts, Deobfuscate_RestoresTheFontAndChecksTheSignature)
{
	const std::wstring sGuid = L"{3EEE3167-E5B8-4798-AE48-EA6B71E31D4D}";
	unsigned char aKey[16];
	ASSERT_TRUE(OOX::FontObfuscation::GuidToKey(sGuid, aKey));

	const char aTrueType[5] = { 0, 1, 0, 0, 0 };
	const char* aTags[] = { aTrueType, "OTTO", "true", "ttcf" };
	for (int t = 0; t < 4; ++t)
	{
		std::vector<unsigned char> aPlain = BuildFont(100, aTags[t]);
		std::vector<unsigned char> aObfuscated = aPlain;
		ASSERT_TRUE(OOX::FontObfuscation::Xor(&aObfuscated[0], aObfuscated.size(), aKey));
		EXPECT_NE(aObfuscated, aPlain);
		// only the first 32 bytes are changed
		for (size_t i = 32; i < aPlain.size(); ++i)
			ASSERT_EQ(aObfuscated[i], aPlain[i]);

		ASSERT_TRUE(OOX::FontObfuscation::Deobfuscate(&aObfuscated[0], aObfuscated.size(), sGuid));
		EXPECT_EQ(aObfuscated, aPlain);
	}
}

TEST(DocxEmbeddedFonts, Deobfuscate_RejectsWrongKeyShortDataAndNoFont)
{
	std::vector<unsigned char> aPlain = BuildFont(100, "OTTO");
	unsigned char aKey[16];
	ASSERT_TRUE(OOX::FontObfuscation::GuidToKey(L"{3EEE3167-E5B8-4798-AE48-EA6B71E31D4D}", aKey));
	std::vector<unsigned char> aObfuscated = aPlain;
	OOX::FontObfuscation::Xor(&aObfuscated[0], aObfuscated.size(), aKey);

	// another key gives garbage at the start: no sfnt signature
	std::vector<unsigned char> aWrong = aObfuscated;
	EXPECT_FALSE(OOX::FontObfuscation::Deobfuscate(&aWrong[0], aWrong.size(), L"{773052E6-96AB-44E5-B2F7-4EE7F6CC51B1}"));

	// bad GUID
	std::vector<unsigned char> aCopy = aObfuscated;
	EXPECT_FALSE(OOX::FontObfuscation::Deobfuscate(&aCopy[0], aCopy.size(), L"not a guid"));

	// shorter than the 32 bytes that are obfuscated, and no data at all
	std::vector<unsigned char> aShort(20, 0);
	EXPECT_FALSE(OOX::FontObfuscation::Deobfuscate(&aShort[0], aShort.size(), L"{3EEE3167-E5B8-4798-AE48-EA6B71E31D4D}"));
	EXPECT_FALSE(OOX::FontObfuscation::Deobfuscate(NULL, 0, L"{3EEE3167-E5B8-4798-AE48-EA6B71E31D4D}"));

	// plain data that is not a font stays unusable even with the right key
	std::vector<unsigned char> aText(64, 'a');
	OOX::FontObfuscation::Xor(&aText[0], aText.size(), aKey);
	EXPECT_FALSE(OOX::FontObfuscation::Deobfuscate(&aText[0], aText.size(), L"{3EEE3167-E5B8-4798-AE48-EA6B71E31D4D}"));
}

TEST(DocxEmbeddedFonts, FontFromXml_ReadsTheEmbeddedFaces)
{
	std::wstring sXml =
		L"<w:font w:name=\"Ubuntu\"><w:charset w:val=\"00\"/>"
		L"<w:embedRegular r:id=\"rId1\" w:fontKey=\"{3EEE3167-E5B8-4798-AE48-EA6B71E31D4D}\"/>"
		L"<w:embedBold r:id=\"rId2\" w:fontKey=\"{773052E6-96AB-44E5-B2F7-4EE7F6CC51B1}\" w:subsetted=\"1\"/>"
		L"<w:embedBoldItalic r:id=\"rId4\" w:fontKey=\"{4115F819-FF2D-4160-9270-AE84793D5C59}\"/>"
		L"</w:font>";

	XmlUtils::CXmlLiteReader oReader;
	ASSERT_TRUE(oReader.FromString(sXml));
	ASSERT_TRUE(oReader.ReadNextNode());

	OOX::CFont oFont;
	oFont.fromXML(oReader);

	EXPECT_EQ(oFont.m_sName, L"Ubuntu");
	EXPECT_EQ(oFont.m_arEmbedRId[OOX::CFont::embedRegular], L"rId1");
	EXPECT_EQ(oFont.m_arEmbedKey[OOX::CFont::embedRegular], L"{3EEE3167-E5B8-4798-AE48-EA6B71E31D4D}");
	EXPECT_EQ(oFont.m_arEmbedRId[OOX::CFont::embedBold], L"rId2");
	EXPECT_TRUE(oFont.m_arEmbedRId[OOX::CFont::embedItalic].empty());
	EXPECT_EQ(oFont.m_arEmbedRId[OOX::CFont::embedBoldItalic], L"rId4");
	EXPECT_FALSE(oFont.m_arEmbedSubsetted[OOX::CFont::embedRegular]);
	EXPECT_TRUE(oFont.m_arEmbedSubsetted[OOX::CFont::embedBold]);
	EXPECT_FALSE(oFont.m_arEmbedSubsetted[OOX::CFont::embedBoldItalic]);

	// the next read starts from a clean state
	std::wstring sXml2 = L"<w:font w:name=\"Arial\"/>";
	XmlUtils::CXmlLiteReader oReader2;
	ASSERT_TRUE(oReader2.FromString(sXml2));
	ASSERT_TRUE(oReader2.ReadNextNode());
	oFont.fromXML(oReader2);
	EXPECT_EQ(oFont.m_sName, L"Arial");
	EXPECT_TRUE(oFont.m_arEmbedRId[OOX::CFont::embedRegular].empty());
	EXPECT_TRUE(oFont.m_arEmbedKey[OOX::CFont::embedBold].empty());
	EXPECT_FALSE(oFont.m_arEmbedSubsetted[OOX::CFont::embedBold]);
}

// Saving: the plain font taken from the media folder is obfuscated again with the same key and gives back the
// bytes of the original .odttf exactly (the operation is its own inverse), and the original key is still valid.
TEST(DocxEmbeddedFonts, Save_ReobfuscationRestoresTheOriginalFileExactly)
{
	const std::wstring sGuid = L"{3EEE3167-E5B8-4798-AE48-EA6B71E31D4D}";
	unsigned char aKey[16];
	ASSERT_TRUE(OOX::FontObfuscation::GuidToKey(sGuid, aKey));

	std::vector<unsigned char> aPlain(300);
	for (size_t i = 0; i < aPlain.size(); ++i)
		aPlain[i] = (unsigned char)(i * 7 + 3);
	aPlain[0] = 0; aPlain[1] = 1; aPlain[2] = 0; aPlain[3] = 0;	// TrueType signature

	// the file of the document as Word wrote it
	std::vector<unsigned char> aOriginal = aPlain;
	ASSERT_TRUE(OOX::FontObfuscation::Xor(&aOriginal[0], aOriginal.size(), aKey));
	EXPECT_NE(aOriginal, aPlain);

	// open: de-obfuscate (the plain font goes to the media folder); save: obfuscate again with the stored key
	std::vector<unsigned char> aMedia = aOriginal;
	ASSERT_TRUE(OOX::FontObfuscation::Deobfuscate(&aMedia[0], aMedia.size(), sGuid));
	EXPECT_EQ(aMedia, aPlain);
	ASSERT_TRUE(OOX::FontObfuscation::Xor(&aMedia[0], aMedia.size(), aKey));
	EXPECT_EQ(aMedia, aOriginal);
}

// Absolute position tab (w:ptab): the three attributes are read and written back as they were.
static std::wstring ReadAndWritePTab(const std::wstring& sXml, OOX::Logic::CPTab& oPTab)
{
	XmlUtils::CXmlLiteReader oReader;
	EXPECT_TRUE(oReader.FromString(sXml));
	EXPECT_TRUE(oReader.ReadNextNode());
	oPTab.fromXML(oReader);
	return oPTab.toXML();
}

TEST(DocxPTab, FromXmlToXml_KeepsAlignmentRelativeToAndLeader)
{
	OOX::Logic::CPTab oPTab;
	const std::wstring sOut = ReadAndWritePTab(
		L"<w:ptab xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\" "
		L"w:relativeTo=\"margin\" w:alignment=\"right\" w:leader=\"none\"/>", oPTab);

	ASSERT_TRUE(oPTab.m_oAlignment.IsInit());
	ASSERT_TRUE(oPTab.m_oRelativeTo.IsInit());
	ASSERT_TRUE(oPTab.m_oLeader.IsInit());
	EXPECT_EQ(oPTab.m_oAlignment->ToString(), L"right");
	EXPECT_EQ(oPTab.m_oRelativeTo->ToString(), L"margin");
	EXPECT_EQ(oPTab.m_oLeader->ToString(), L"none");
	EXPECT_NE(sOut.find(L"w:alignment=\"right\""), std::wstring::npos);
	EXPECT_NE(sOut.find(L"w:relativeTo=\"margin\""), std::wstring::npos);
	EXPECT_NE(sOut.find(L"w:leader=\"none\""), std::wstring::npos);
	EXPECT_EQ(oPTab.getType(), OOX::et_w_ptab);
}

TEST(DocxPTab, FromXml_CenterIndentDot)
{
	OOX::Logic::CPTab oPTab;
	ReadAndWritePTab(
		L"<w:ptab xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\" "
		L"w:relativeTo=\"indent\" w:alignment=\"center\" w:leader=\"dot\"/>", oPTab);
	EXPECT_EQ(oPTab.m_oAlignment->ToString(), L"center");
	EXPECT_EQ(oPTab.m_oRelativeTo->ToString(), L"indent");
	EXPECT_EQ(oPTab.m_oLeader->ToString(), L"dot");
}

// An empty w:documentProtection (no attribute) is not written: the document had no protection.
TEST(DocxSettings, DocProtectToXml_EmptyElementIsNotWritten)
{
	OOX::Settings::CDocProtect oEmpty;
	EXPECT_TRUE(oEmpty.toXML().empty());
}

// w:autoSpaceDE / w:autoSpaceDN: read from pPr and written back before w:bidi (schema order of CT_PPrBase).
static std::wstring ReadAndWritePPr(const std::wstring& sXml, OOX::Logic::CParagraphProperty& oPPr)
{
	XmlUtils::CXmlLiteReader oReader;
	EXPECT_TRUE(oReader.FromString(sXml));
	EXPECT_TRUE(oReader.ReadNextNode());
	oPPr.fromXML(oReader);
	return oPPr.toXML();
}

TEST(DocxAutoSpace, FromXmlToXml_KeepsAutoSpaceDEAndDNBeforeBidi)
{
	OOX::Logic::CParagraphProperty oPPr;
	const std::wstring sOut = ReadAndWritePPr(
		L"<w:pPr xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
		L"<w:autoSpaceDE w:val=\"0\"/><w:autoSpaceDN w:val=\"0\"/><w:bidi w:val=\"1\"/></w:pPr>", oPPr);

	ASSERT_TRUE(oPPr.m_oAutoSpaceDE.IsInit());
	ASSERT_TRUE(oPPr.m_oAutoSpaceDN.IsInit());
	EXPECT_FALSE(oPPr.m_oAutoSpaceDE->m_oVal.ToBool());
	EXPECT_FALSE(oPPr.m_oAutoSpaceDN->m_oVal.ToBool());
	EXPECT_NE(sOut.find(L"<w:autoSpaceDE w:val=\"0\"/>"), std::wstring::npos);
	EXPECT_NE(sOut.find(L"<w:autoSpaceDN w:val=\"0\"/>"), std::wstring::npos);
	EXPECT_LT(sOut.find(L"<w:autoSpaceDE"), sOut.find(L"<w:bidi"));
	EXPECT_LT(sOut.find(L"<w:autoSpaceDN"), sOut.find(L"<w:bidi"));
}
