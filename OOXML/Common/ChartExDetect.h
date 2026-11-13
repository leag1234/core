#pragma once

#include "../../DesktopEditor/common/File.h"
#include "../SystemUtility/SystemUtility.h"

namespace OOX
{
	inline bool IsChartExPart(const CPath& oFileName)
	{
		NSFile::CFileBinary oFile;
		if (!oFile.OpenFile(oFileName.GetPath()))
			return false;

		BYTE pBuffer[4096];
		DWORD dwRead = 0;
		oFile.ReadFile(pBuffer, sizeof(pBuffer), dwRead);
		oFile.CloseFile();

		std::string sHead((char*)pBuffer, dwRead);
		size_t nPos = 0;
		while (true)
		{
			nPos = sHead.find('<', nPos);
			if (nPos == std::string::npos || nPos + 1 >= sHead.size())
				return false;
			if (sHead[nPos + 1] != '?' && sHead[nPos + 1] != '!')
				break;
			++nPos;
		}
		size_t nEnd = sHead.find('>', nPos);
		if (nEnd == std::string::npos)
			return false;

		std::string sRoot = sHead.substr(nPos + 1, nEnd - nPos - 1);
		size_t nNameEnd = sRoot.find_first_of(" \t\r\n/");
		std::string sName = sRoot.substr(0, nNameEnd);
		size_t nColon = sName.find(':');
		std::string sPrefix = (nColon == std::string::npos) ? "" : sName.substr(0, nColon);
		std::string sLocal = (nColon == std::string::npos) ? sName : sName.substr(nColon + 1);
		if (sLocal != "chartSpace")
			return false;

		std::string sDecl = sPrefix.empty() ? "xmlns=" : ("xmlns:" + sPrefix + "=");
		size_t nDecl = sRoot.find(sDecl);
		if (nDecl == std::string::npos)
			return false;
		const std::string sChartExNs = "http://schemas.microsoft.com/office/drawing/2014/chartex";
		return sRoot.compare(nDecl + sDecl.size() + 1, sChartExNs.size(), sChartExNs) == 0;
	}
}
