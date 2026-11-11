#pragma once
#ifndef OOX_FONTOBFUSCATION_INCLUDE_H_
#define OOX_FONTOBFUSCATION_INCLUDE_H_

#include <string>
#include <cstddef>

namespace OOX
{
	// Embedded fonts of a DOCX (word/fonts/*.odttf, w:embedRegular & co. in word/fontTable.xml).
	// An .odttf file is a TrueType / OpenType font whose first 32 bytes are XORed with the 16 key bytes
	// built from the w:fontKey GUID (ECMA-376 Part 2, 17.8.1 "Font Embedding"): the hex digits of the GUID
	// (braces and dashes removed) are read as bytes from the END of the string, and the 16 key bytes are
	// applied to bytes 0..15 and again to bytes 16..31.
	namespace FontObfuscation
	{
		const size_t c_nMaxFontBytes  = 32u * 1024u * 1024u;	// a font larger than this is skipped
		const size_t c_nMaxTotalBytes = 96u * 1024u * 1024u;	// no more fonts are taken once this total is reached

		inline int HexValue(wchar_t c)
		{
			if (c >= L'0' && c <= L'9') return c - L'0';
			if (c >= L'a' && c <= L'f') return c - L'a' + 10;
			if (c >= L'A' && c <= L'F') return c - L'A' + 10;
			return -1;
		}
		// "{3A7E6E5F-1F4B-4D2D-A1B2-0123456789AB}" -> 16 key bytes; false when the string is not a GUID
		inline bool GuidToKey(const std::wstring& sGuid, unsigned char aKey[16])
		{
			int aDigits[32];
			size_t nDigits = 0;
			for (size_t i = 0; i < sGuid.size(); ++i)
			{
				wchar_t c = sGuid[i];
				if (c == L'{' || c == L'}' || c == L'-')
					continue;
				int v = HexValue(c);
				if (v < 0 || nDigits >= 32)
					return false;
				aDigits[nDigits++] = v;
			}
			if (nDigits != 32)
				return false;
			for (int i = 0; i < 16; ++i)
			{
				int nPos = (15 - i) * 2;
				aKey[i] = (unsigned char)((aDigits[nPos] << 4) | aDigits[nPos + 1]);
			}
			return true;
		}
		// The operation is its own inverse. Files shorter than 32 bytes are rejected.
		inline bool Xor(unsigned char* pData, size_t nSize, const unsigned char aKey[16])
		{
			if (!pData || nSize < 32)
				return false;
			for (int i = 0; i < 32; ++i)
				pData[i] ^= aKey[i % 16];
			return true;
		}
		// sfnt signature: TrueType 00 01 00 00, OpenType 'OTTO', Apple 'true', collection 'ttcf'
		inline bool IsSfnt(const unsigned char* p, size_t nSize)
		{
			if (!p || nSize < 12)
				return false;
			if (p[0] == 0 && p[1] == 1 && p[2] == 0 && p[3] == 0) return true;
			if (p[0] == 'O' && p[1] == 'T' && p[2] == 'T' && p[3] == 'O') return true;
			if (p[0] == 't' && p[1] == 'r' && p[2] == 'u' && p[3] == 'e') return true;
			if (p[0] == 't' && p[1] == 't' && p[2] == 'c' && p[3] == 'f') return true;
			return false;
		}
		// Whole step: de-obfuscates in place and checks the result (false = not a usable font)
		inline bool Deobfuscate(unsigned char* pData, size_t nSize, const std::wstring& sGuid)
		{
			unsigned char aKey[16];
			if (!GuidToKey(sGuid, aKey))
				return false;
			if (!Xor(pData, nSize, aKey))
				return false;
			return IsSfnt(pData, nSize);
		}
	}
}

#endif
