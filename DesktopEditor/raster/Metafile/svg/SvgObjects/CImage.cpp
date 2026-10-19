#include "CImage.h"

#include "../CSvgFile.h"
#include "../../../../graphics/Image.h"
#include "../../../BgraFrame.h"
#include "../../../../common/Base64.h"

namespace SVG
{
	CImage::CImage(CSvgReader& oReader, CRenderedObject* pParent)
		: CRenderedObject(oReader, pParent)
	{}

	void CImage::SetAttribute(const std::string& sName, CSvgReader& oReader)
	{
		if ("x" == sName)
			m_oRect.m_oX.SetValue(oReader.GetText());
		else if ("y" == sName)
			m_oRect.m_oY.SetValue(oReader.GetText());
		else if ("width" == sName)
			m_oRect.m_oWidth.SetValue(oReader.GetText());
		else if ("height" == sName)
			m_oRect.m_oHeight.SetValue(oReader.GetText());
		else if ("href" == sName || "xlink:href" == sName)
			m_wsHref = oReader.GetText(); // TODO:: In the future, it might be worth implementing a separate CHref class for all types of links
		else
			CRenderedObject::SetAttribute(sName, oReader);
	}

	bool CImage::Draw(IRenderer *pRenderer, const CSvgFile *pFile, CommandeMode oMode, const TSvgStyles *pOtherStyles, const CRenderedObject* pContexObject) const
	{
		// SVG resources are embedded; never resolve href against a host directory.
		const std::wstring pngPrefix = L"data:image/png;base64,";
		const std::wstring jpegPrefix = L"data:image/jpeg;base64,";
		size_t offset;
		if (m_wsHref.compare(0, pngPrefix.size(), pngPrefix) == 0)
			offset = pngPrefix.size();
		else if (m_wsHref.compare(0, jpegPrefix.size(), jpegPrefix) == 0)
			offset = jpegPrefix.size();
		else
			return false;
		const std::wstring wsImageData = m_wsHref.substr(offset);
		if (wsImageData.empty() || wsImageData.size() > 256 * 1024 * 1024)
			return false;
		int decodedSize = NSBase64::Base64DecodeGetRequiredLength(wsImageData.size());
		if (decodedSize <= 0)
			return false;
		BYTE* pBuffer = new(std::nothrow) BYTE[decodedSize];
		if (NULL == pBuffer)
			return false;
		if (!NSBase64::Base64Decode(wsImageData.c_str(), wsImageData.size(), pBuffer, &decodedSize) || decodedSize <= 0)
		{
			delete[] pBuffer;
			return false;
		}
		DWORD ulSize = static_cast<DWORD>(decodedSize);

		Aggplus::CMatrix oOldTransform;

		if (!StartPath(pRenderer, pFile, oOldTransform, oMode))
		{
			delete[] pBuffer;
			return false;
		}

		TBounds oBounds = (NULL != m_pParent) ? m_pParent->GetBounds() : TBounds{0., 0., 0., 0.};

		double dParentWidth  = oBounds.m_dRight  - oBounds.m_dLeft;
		double dParentHeight = oBounds.m_dBottom - oBounds.m_dTop;

		double dX      = m_oRect.m_oX     .ToDouble(NSCSS::Pixel, dParentWidth);
		double dY      = m_oRect.m_oY     .ToDouble(NSCSS::Pixel, dParentHeight);
		double dWidth  = m_oRect.m_oWidth .ToDouble(NSCSS::Pixel, dParentWidth);
		double dHeight = m_oRect.m_oHeight.ToDouble(NSCSS::Pixel, dParentHeight);


		CBgraFrame oBgraFrame;
		oBgraFrame.Decode(pBuffer, ulSize);

		double dImageW = oBgraFrame.get_Width();
		double dImageH = oBgraFrame.get_Height();

		Aggplus::CImage oImage;
		oImage.Create(oBgraFrame.get_Data(), dImageW, dImageH, -4 * dImageW, true);

		if (dImageW / dWidth > dImageH / dHeight)
		{
			double dValue = dImageW / dWidth;
			dY += (dHeight - (dImageH / dValue)) / 2.;
			dHeight = dImageH / dValue;
		}
		else if (dImageW / dWidth < dImageH / dHeight)
		{
			double dValue = dImageH / dHeight;
			dX += (dWidth - (dImageW / dValue)) / 2.;
			dWidth = dImageW / dValue;
		}

		if (CommandeModeClip != oMode)
			pRenderer->DrawImage(&oImage, dX, dY, dWidth, dHeight);
		else
		{
			pRenderer->PathCommandMoveTo(dX, dY);
			pRenderer->PathCommandLineTo(dX + dWidth, dY);
			pRenderer->PathCommandLineTo(dX + dWidth, dY + dHeight);
			pRenderer->PathCommandLineTo(dX, dY + dHeight);
			pRenderer->PathCommandClose();
		}

		EndPath(pRenderer, pFile, oOldTransform, oMode, pOtherStyles, pContexObject);

		delete[] pBuffer;

		return true;
	}

	TBounds CImage::GetBounds(SvgMatrix* pTransform) const
	{
		TBounds oBounds;

		oBounds.m_dLeft   = m_oRect.m_oX.ToDouble(NSCSS::Pixel);
		oBounds.m_dTop    = m_oRect.m_oY.ToDouble(NSCSS::Pixel);
		oBounds.m_dRight  = oBounds.m_dLeft + m_oRect.m_oWidth.ToDouble(NSCSS::Pixel);
		oBounds.m_dBottom = oBounds.m_dTop  + m_oRect.m_oHeight.ToDouble(NSCSS::Pixel);

		if (nullptr != pTransform)
		{
			*pTransform += m_oTransformation.m_oTransform.GetMatrix();

			pTransform->GetFinalValue().TransformPoint(oBounds.m_dLeft,  oBounds.m_dTop   );
			pTransform->GetFinalValue().TransformPoint(oBounds.m_dRight, oBounds.m_dBottom);

			*pTransform -= m_oTransformation.m_oTransform.GetMatrix();
		}

		return oBounds;
	}
}
