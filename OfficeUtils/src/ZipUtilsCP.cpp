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
#include "ZipUtilsCP.h"
#include <memory>
#include <algorithm>
#include "../../DesktopEditor/common/Directory.h"
#include "../../DesktopEditor/common/Path.h"
#include "../../DesktopEditor/common/PackagePath.h"
#include <new>

#if !defined(_WIN32) && !defined (_WIN64)
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#endif

#define WRITEBUFFERSIZE 8192
#define READBUFFERSIZE 8192

unzFile unzOpenHelp(const wchar_t* filename);

namespace ZLibZipUtils
{
	// Fixed conversion budgets apply before any extraction or header-sized allocation.
	static const unsigned long long MaxArchiveEntries = 50000;
	static const unsigned long long MaxEntryBytes = 256ULL * 1024 * 1024;
	static const unsigned long long MaxArchiveBytes = 1024ULL * 1024 * 1024;
	static const unsigned long long MaxCompressionRatio = 1000;
	static const unsigned long long CompressionRatioMinimum = 1024 * 1024;

	static bool ValidateArchive(unzFile uf)
	{
		unz_global_info64 global;
		if (!uf || unzGetGlobalInfo64(uf, &global) != UNZ_OK ||
			global.number_entry == 0 || global.number_entry > MaxArchiveEntries ||
			unzGoToFirstFile(uf) != UNZ_OK)
			return false;
		unsigned long long total = 0;
		for (unsigned long long i = 0; i < global.number_entry; ++i)
		{
			unz_file_info64 info;
			if (unzGetCurrentFileInfo64(uf, &info, NULL, 0, NULL, 0, NULL, 0) != UNZ_OK ||
				info.uncompressed_size > MaxEntryBytes ||
				info.uncompressed_size > MaxArchiveBytes - total)
				return false;
			if (info.uncompressed_size > CompressionRatioMinimum &&
				info.compressed_size < (info.uncompressed_size +
					MaxCompressionRatio - 1) / MaxCompressionRatio)
				return false;
			total += info.uncompressed_size;
			if (i + 1 < global.number_entry && unzGoToNextFile(uf) != UNZ_OK)
				return false;
		}
		return unzGoToFirstFile(uf) == UNZ_OK;
	}

	// internal class for C unzFile struct
	class CunzFileWrapped
	{
	public:
		CunzFileWrapped() = default;
		~CunzFileWrapped();

		// NOTE: calls Close() first
		void Open(const wchar_t* filename);

		// NOTE: calls Close() first
		void Open(BYTE* data, size_t len);

		int Close();

		// NOTE: unzfile will be released after Close() or ~CunzFileWrapped()!
		// return value will be NULL, if Open(...) fails
		unzFile Get();

	private:
		unzFile m_unzFile {NULL};
		BUFFER_IO* m_buffer {NULL};
	};

	CunzFileWrapped::~CunzFileWrapped()
	{
		Close();
	}

	void CunzFileWrapped::Open(const wchar_t* filename)
	{
		Close();
		if(filename != NULL)
		{
			int old = zlip_get_addition_flag();
			zlip_set_addition_flag(old | ZLIB_ADDON_FLAG_READ_ONLY);
			m_unzFile = unzOpenHelp(filename);
			zlip_set_addition_flag(old);
		}
	}

	void CunzFileWrapped::Open(BYTE* data, size_t len)
	{
		Close();
		m_buffer = new BUFFER_IO;
		if ((data != NULL) && (len != 0))
		{
			int old = zlip_get_addition_flag();
			zlip_set_addition_flag(old | ZLIB_ADDON_FLAG_READ_ONLY);

			m_buffer->buffer = data;
			m_buffer->nSize  = len;

			zlib_filefunc_def ffunc;
			fill_buffer_filefunc(&ffunc, m_buffer);
			m_unzFile = unzOpen2(NULL, &ffunc);

			zlip_set_addition_flag(old);
		}
	}
	int CunzFileWrapped::Close()
	{
		int err = 0;
		if(m_unzFile)
			err = unzClose(m_unzFile);
		m_unzFile = NULL;

		RELEASEOBJECT(m_buffer)
		return err;
	}

	unzFile CunzFileWrapped::Get()
	{
		return m_unzFile;
	}

	zipFile zipOpenHelp(const wchar_t* filename)
	{
#if defined(_WIN32) || defined (_WIN64)
		zlib_filefunc64_def ffunc;
		fill_win32_filefunc64W(&ffunc);
		zipFile zf = zipOpen2_64(filename, APPEND_STATUS_CREATE, NULL, &ffunc);
#else
#ifdef _IOS
		std::string filePath = NSFile::NSIOS::GetFileSystemRepresentation(filename);
		zipFile zf = filePath.empty() ? NULL : zipOpen(filePath.c_str(), APPEND_STATUS_CREATE);
#else
		BYTE* pUtf8 = NULL;
		LONG lLen = 0;
		NSFile::CUtf8Converter::GetUtf8StringFromUnicode(filename, wcslen(filename), pUtf8, lLen, false);
		zipFile zf = zipOpen( (char*)pUtf8, APPEND_STATUS_CREATE );
		delete [] pUtf8;
#endif
#endif
		return zf;
	}
	unzFile unzOpenHelp(const wchar_t* filename)
	{
#if defined(_WIN32) || defined (_WIN64)
		zlib_filefunc64_def ffunc;
		fill_win32_filefunc64W(&ffunc);
		unzFile uf = unzOpen2_64(filename, &ffunc);
#else
#ifdef _IOS
		std::string filePath = NSFile::NSIOS::GetFileSystemRepresentation(filename);
		unzFile uf = filePath.empty() ? NULL : unzOpen(filePath.c_str());
#else
		BYTE* pUtf8 = NULL;
		LONG lLen = 0;
		NSFile::CUtf8Converter::GetUtf8StringFromUnicode(filename, wcslen(filename), pUtf8, lLen, false);
		unzFile uf = unzOpen( (char*)pUtf8 );
		delete [] pUtf8;
#endif
#endif
		return uf;
	}

	static std::wstring ascii_to_unicode(const char *src)
	{
		// TODO: check codepage of system (for "bad" archive)
		std::string sAnsi(src);
		return std::wstring(sAnsi.begin(), sAnsi.end());
	}
	static std::string unicode_to_ascii(const wchar_t *src)
	{
		std::wstring sUnicode(src);
		return std::string(sUnicode.begin(), sUnicode.end());
	}
	/*This static functions are copies from ZLib miniunz.c with some changes.*/
	static std::wstring codepage_issue_fixFromOEM( const char* sVal)
	{
#if defined(_WIN32) || defined (_WIN64)
		int nBufferSize = MultiByteToWideChar( CP_OEMCP, 0, sVal, -1, NULL, 0 );
		wchar_t* pBuffer = new wchar_t[nBufferSize];
		MultiByteToWideChar( CP_OEMCP, 0, sVal, -1, pBuffer, nBufferSize );
		//If this parameter is -1, the function processes the entire input string, including the terminating null character.
		//Therefore, the resulting Unicode string has a terminating null character, and the length returned by the function includes this character.
		std::wstring sRes(pBuffer, nBufferSize - 1);
		RELEASEARRAYOBJECTS(pBuffer);
		return sRes;
#else
		return ascii_to_unicode(sVal);
#endif
	}
	static std::string codepage_issue_fixToOEM(const std::wstring& sVal)
	{
#if defined(_WIN32) || defined (_WIN64)
		const wchar_t* buffer = sVal.c_str();
		int nBufferSize = WideCharToMultiByte( CP_OEMCP, 0, buffer, -1, NULL, 0, NULL, NULL );
		char* pBuffer = new char[nBufferSize];
		WideCharToMultiByte( CP_OEMCP, 0, buffer, -1, pBuffer, nBufferSize, NULL, NULL );
		std::string sRes(pBuffer, nBufferSize);
		RELEASEARRAYOBJECTS(pBuffer);
		return sRes;
#else
		return unicode_to_ascii(sVal.c_str());
#endif
	}
	static void change_file_date( const wchar_t *filename, uLong dosdate, tm_unz tmu_date );
	static int do_extract_currentfile( unzFile uf, const wchar_t* unzip_dir, const int* popt_extract_without_path, int* popt_overwrite, const char* password, bool is_replace_slash = false );
	static int do_extract( unzFile uf, const wchar_t* unzip_dir, int opt_extract_without_path, int opt_overwrite, const char* password, const OnProgressCallback* progress );

	static bool get_file(unzFile uf, BYTE* arr, uInt array_size, const char* password);
	static bool is_file_in_archive(unzFile uf, const wchar_t *filename);
	static bool current_file_is_find(unzFile uf, const wchar_t *filename);
	static bool get_file_in_archive(unzFile uf, const wchar_t *filePathInZip, BYTE** fileInBytes, ULONG& nFileSize);

	static unsigned int get_files_count( const WCHAR* dirname );

	/*========================================================================================================*/

	static void replace_all(std::string& subject, const std::string& search, const std::string& replace)
	{
		size_t pos = 0;
		while ((pos = subject.find(search, pos)) != std::string::npos)
		{
			subject.replace(pos, search.length(), replace);
			pos += replace.length();
		}
	}

	static void replace_all_w(std::wstring& subject, const std::wstring& search, const std::wstring& replace)
	{
		size_t pos = 0;
		while ((pos = subject.find(search, pos)) != std::wstring::npos)
		{
			subject.replace(pos, search.length(), replace);
			pos += replace.length();
		}
	}

	// Unsupported names can be skipped, but a path escape always aborts.
	static int EntryPathDecision(std::wstring name)
	{
		std::replace(name.begin(), name.end(), L'\\', L'/');
		if (name.empty() || name[0] == L'/' ||
			(name.size() > 1 && name[1] == L':' && iswalpha(name[0])))
			return -1;
		int depth = 0;
		bool unusual = name == L"." || name == L"./";
		for (size_t at = 0; at <= name.size();)
		{
			size_t end = name.find(L'/', at);
			if (end == std::wstring::npos) end = name.size();
			const std::wstring part = name.substr(at, end - at);
			if (part == L"..") { if (depth == 0) return -1; --depth; }
			else if (!part.empty() && part != L".") ++depth;
			for (wchar_t ch : part) if (ch < 32 || ch == 127) return -1;
			if (part.find(L':') != std::wstring::npos) unusual = true;
#if defined(_WIN32) || defined(_WIN64)
			if (!NSPackagePath::WindowsComponent(part)) unusual = true;
#endif
			if (end == name.size()) break;
			at = end + 1;
		}
		return unusual ? 1 : 0;
	}

	static int do_extract_currentfile( unzFile uf, const wchar_t* unzip_dir, const int* popt_extract_without_path, int* popt_overwrite, const char* password, bool is_replcace_slash )
	{
		char filename_inzipA[4096];
		int err = UNZ_OK;
		unz_file_info file_info;

		err = unzGetCurrentFileInfo(uf,&file_info,filename_inzipA,sizeof(filename_inzipA),NULL,0,NULL,0);
		if (err!=UNZ_OK)
			return err;
		if (!unzip_dir || file_info.size_filename == 0 || file_info.size_filename >= sizeof(filename_inzipA))
			return UNZ_BADZIPFILE;
		filename_inzipA[file_info.size_filename] = 0;
		if (memchr(filename_inzipA, 0, file_info.size_filename))
			return UNZ_BADZIPFILE;

		std::wstring filenameW;
		if (file_info.flag & 2048 /*11 bit*/)
			filenameW = NSFile::CUtf8Converter::GetUnicodeStringFromUTF8((BYTE*)filename_inzipA, strlen(filename_inzipA));
		else
			filenameW = codepage_issue_fixFromOEM(filename_inzipA);

#ifdef _WIN32
		for(std::wstring::size_type i = 0, len = filenameW.length(); i < len; ++i)
			if(filenameW[i] == L'/')
				filenameW[i] = L'\\';
#else
		if (is_replcace_slash)
			for (std::wstring::size_type i = 0, len = filenameW.length(); i < len; ++i)
				if(filenameW[i] == L'\\')
					filenameW[i] = L'/';

#endif

		const int pathDecision = EntryPathDecision(filenameW);
		if (pathDecision < 0) return UNZ_BADZIPFILE;
		if (pathDecision > 0)
			return get_file(uf, NULL, 0, password) ? UNZ_OK : UNZ_BADZIPFILE;

		std::wstring filenameW_withoutpath = L"";
		std::wstring::size_type posSeparator = filenameW.find_last_of(FILE_SEPARATOR_STR);
		if (posSeparator == std::wstring::npos)
			filenameW_withoutpath = filenameW;
		else if (posSeparator != (filenameW.length() - 1))
			filenameW_withoutpath = filenameW.substr(posSeparator + 1);

		std::wstring output;
		if (!NSPackagePath::Resolve(unzip_dir, unzip_dir, filenameW, output))
			return UNZ_BADZIPFILE;
		std::wstring output_withoutpath;
		if (!filenameW_withoutpath.empty() &&
			!NSPackagePath::Resolve(unzip_dir, unzip_dir, filenameW_withoutpath, output_withoutpath))
			return UNZ_BADZIPFILE;

		if (filenameW_withoutpath.empty())
		{
			if (!get_file(uf, NULL, 0, password)) return UNZ_BADZIPFILE;
			if ((*popt_extract_without_path)==0)
			{
				if (!NSDirectory::Exists(output) && !NSDirectory::CreateDirectories(output))
					return UNZ_ERRNO;
			}
		}
		else
		{
			const wchar_t* write_filename;
			int skip=0;

			if ((*popt_extract_without_path)==0)
				write_filename = output.c_str();
			else
				write_filename = output_withoutpath.c_str();

			err = unzOpenCurrentFilePassword(uf, password);

			NSFile::CFileBinary oFile;
			FILE *fout = NULL;

			if ((skip==0) && (err==UNZ_OK))
			{
				if(oFile.CreateFileW(write_filename))
					fout = oFile.GetFileNative();

				// some zipfile don't contain directory alone before file
				if ((fout == NULL) && ((*popt_extract_without_path)==0) &&
						(filenameW_withoutpath!=filenameW))
				{
					std::wstring folder = NSDirectory::GetFolderPath(write_filename);
					NSDirectory::CreateDirectories(folder);

					if(oFile.CreateFileW(write_filename))
					{
						fout = oFile.GetFileNative();
					}
				}
			}

			uInt size_buf = WRITEBUFFERSIZE;
			void* buf = (void*)malloc(size_buf);

			if (buf == NULL)
			{
				unzCloseCurrentFile(uf);
				if (fout != NULL) { oFile.CloseFile(); NSFile::CFileBinary::Remove(write_filename); }
				return UNZ_INTERNALERROR;
			}
			if (fout == NULL && err == UNZ_OK) err = UNZ_ERRNO;

			if (fout != NULL)
			{
				do
				{
					err = unzReadCurrentFile(uf, buf, size_buf);
					if (err<0)
					{
						break;
					}
					if (err>0)
						if (fwrite(buf, err, 1, fout) != 1)
						{
							err=UNZ_ERRNO;
							break;
						}
				}
				// ?
				while (err>0);
				if (err==0)
				{
					oFile.CloseFile();

					struct tm time;
					memset(&time, 0, sizeof(struct tm));
					time.tm_sec = file_info.tmu_date.tm_sec;
					time.tm_min = file_info.tmu_date.tm_min;
					time.tm_hour = file_info.tmu_date.tm_hour;
					time.tm_mday = file_info.tmu_date.tm_mday;
					time.tm_mon = file_info.tmu_date.tm_mon + 1;
					time.tm_year = file_info.tmu_date.tm_year;

					NSFile::CFileBinary::SetTime(write_filename, &time);
				}
			}

			if (err == UNZ_OK)
			{
				err = unzCloseCurrentFile (uf);
			}
			else
				unzCloseCurrentFile(uf); // don't lose the error

			free(buf);
			if (err != UNZ_OK && fout != NULL)
			{
				oFile.CloseFile();
				NSFile::CFileBinary::Remove(write_filename);
			}
		}

		return err;
	}

	/*========================================================================================================*/

	static int do_extract( unzFile uf, const wchar_t* unzip_dir, int opt_extract_without_path, int opt_overwrite, const char* password, const OnProgressCallback* progress )
	{
		uLong i, number_extract = 0;
		unz_global_info gi;
		int err;
		FILE* fout=NULL;

		// [Content-Types.xml] check
		// if found - it is a office file, so we can replace '\' to '/' for non-windows
		// fixes bad zips
		const char* content_types = "[Content_Types].xml";
		bool is_office = UNZ_OK == unzLocateFile(uf, content_types, true);
		unzGoToFirstFile(uf);

		err = unzGetGlobalInfo (uf,&gi);
		if (err != UNZ_OK) return err;

		for (i = 0; i < gi.number_entry; i++)
		{
			int entryError = do_extract_currentfile(uf, unzip_dir, &opt_extract_without_path,
									   &opt_overwrite, password, is_office);
			if (entryError != UNZ_OK) return entryError;
			if (entryError == UNZ_OK)
			{
				number_extract++;
			}

			if ( progress != NULL )
			{
				short cancel = 0;
				long progressValue = ( 1000000 / gi.number_entry * i );
				if(NULL != progress)
					(*progress)( UTILS_ONPROGRESSEVENT_ID, progressValue, &cancel );

				if ( cancel != 0 )
				{
					return UNZ_ERRNO;
				}
			}

			if ((i + 1) < gi.number_entry)
			{
				err = unzGoToNextFile(uf);
				if (err!=UNZ_OK)
				{
					break;
				}
			}
		}

		if (number_extract < 1)
		{
			err = -1;
		}

		if ( progress != NULL )
		{
			short cancel = 0;
			long progressValue = 1000000;
			if(NULL != progress)
				(*progress)( UTILS_ONPROGRESSEVENT_ID, progressValue, &cancel );
		}

		return err;
	}

	/*========================================================================================================*/

	static bool is_file_in_archive(unzFile uf, const wchar_t *filename)
	{
		uLong i;
		unz_global_info gi;
		int err;

		err = unzGetGlobalInfo (uf,&gi);

		for (i = 0; i < gi.number_entry; i++)
		{
			if (current_file_is_find(uf, filename) == true)
				return true;

			if ((i + 1) < gi.number_entry)
			{
				err = unzGoToNextFile(uf);
				if (err != UNZ_OK)
					break;
			}
		}
		return false;
	}

	/*========================================================================================================*/

	static bool CurrentName(unzFile uf, std::wstring& name)
	{
		char bytes[4096];
		unz_file_info info;
		if (unzGetCurrentFileInfo(uf, &info, bytes, sizeof(bytes), NULL, 0, NULL, 0) != UNZ_OK ||
			info.size_filename == 0 || info.size_filename >= sizeof(bytes) ||
			memchr(bytes, 0, info.size_filename))
			return false;
		bytes[info.size_filename] = 0;
		name = (info.flag & 2048) ?
			NSFile::CUtf8Converter::GetUnicodeStringFromUTF8(reinterpret_cast<BYTE*>(bytes), info.size_filename) :
			codepage_issue_fixFromOEM(bytes);
		return !name.empty();
	}
	static bool current_file_is_find(unzFile uf, const wchar_t *filename)
	{
		std::wstring name;
		return filename && CurrentName(uf, name) && name == filename;
	}

	/*========================================================================================================*/

	static const std::wstring get_filename_from_unzfile(unzFile uf)
	{
		std::wstring name;
		CurrentName(uf, name);
		return name;
	}

	/*========================================================================================================*/

	static bool get_file(unzFile uf, BYTE* arr, uInt array_size, const char* password = NULL)
	{
		unz_file_info info;
		if (unzGetCurrentFileInfo(uf, &info, NULL, 0, NULL, 0, NULL, 0) != UNZ_OK ||
			info.uncompressed_size > MaxEntryBytes ||
			unzOpenCurrentFilePassword(uf, password) != UNZ_OK)
			return false;
		BYTE buffer[READBUFFERSIZE];
		unsigned long long total = 0;
		int count;
		while ((count = unzReadCurrentFile(uf, buffer, sizeof(buffer))) > 0)
		{
			if (total + count > info.uncompressed_size)
			{
				unzCloseCurrentFile(uf);
				return false;
			}
			if (arr && total < array_size)
				memcpy(arr + static_cast<size_t>(total), buffer,
					(std::min)(static_cast<unsigned long long>(count), array_size - total));
			total += count;
		}
		const int closeResult = unzCloseCurrentFile(uf);
		return count == 0 && total == info.uncompressed_size && closeResult == UNZ_OK;
	}

	/*========================================================================================================*/

	static bool get_file_in_archive(unzFile uf, const wchar_t *filePathInZip, BYTE** fileInBytes, ULONG& nFileSize)
	{
		if (!fileInBytes) return false;
		*fileInBytes = NULL;
		if (!ValidateArchive(uf)) { nFileSize = 0; return false; }
		do
		{
			if (!current_file_is_find(uf, filePathInZip)) continue;
			unz_file_info info;
			if (unzGetCurrentFileInfo(uf, &info, NULL, 0, NULL, 0, NULL, 0) != UNZ_OK)
				break;
			nFileSize = nFileSize ? (std::min)(nFileSize, info.uncompressed_size) : info.uncompressed_size;
			BYTE* data = new(std::nothrow) BYTE[nFileSize ? nFileSize : 1];
			if (!data) break;
			if (!get_file(uf, data, nFileSize, NULL)) { delete[] data; break; }
			*fileInBytes = data;
			return true;
		} while (unzGoToNextFile(uf) == UNZ_OK);
		nFileSize = 0;
		return false;
	}

	/*========================================================================================================*/

	int oneZipFile(zipFile & zf, std::wstring & file_name, std::wstring & zip_file_name, int method, int compressionLevel, bool bDateTime)
	{
		int err = -1;
		NSFile::CFileBinary oFile;

		zip_fileinfo zinfo;
		zinfo.dosDate = 0;
		zinfo.external_fa = 0;
		zinfo.internal_fa = 0;

		if (bDateTime)
		{
			struct tm edited;
			bool ok = NSFile::CFileBinary::GetTime(file_name, &edited);
			if (ok)
			{
				zinfo.tmz_date.tm_sec = edited.tm_sec;
				zinfo.tmz_date.tm_min = edited.tm_min;
				zinfo.tmz_date.tm_hour = edited.tm_hour;
				zinfo.tmz_date.tm_mday = edited.tm_mday;
				zinfo.tmz_date.tm_mon = edited.tm_mon - 1;
				zinfo.tmz_date.tm_year = edited.tm_year;
			}
		}
		zip_fileinfo* zi_new = bDateTime ? &zinfo : NULL;

		if (oFile.OpenFile(file_name))
		{
			DWORD dwSizeRead;
			BYTE* pData = new BYTE[oFile.GetFileSize()];
			if (oFile.ReadFile(pData, oFile.GetFileSize(), dwSizeRead))
			{
				std::string zipFileNameA = codepage_issue_fixToOEM(zip_file_name);

				err = zipOpenNewFileInZip( zf, zipFileNameA.c_str(), zi_new, NULL, 0, NULL, 0, NULL, method, compressionLevel );
				err = zipWriteInFileInZip( zf, pData, dwSizeRead );
				err = zipCloseFileInZip( zf );
			}
			RELEASEARRAYOBJECTS(pData);
		}
		return err;
	}
	int ZipDir( const WCHAR* dir, const WCHAR* outputFile, const OnProgressCallback* progress, bool sorted, int method, int compressionLevel, bool bDateTime )
	{
		if ( ( dir != NULL ) && ( outputFile != NULL ) )
		{
			deque<wstring> StringDeque;
			deque<wstring> zipDeque;
			StringDeque.push_back( wstring( dir ) );

			wstring zipDir;
			wstring file;
			wstring zipFileName;
			wstring szText;

			zipFile zf = zipOpenHelp(outputFile);
			if (!zf) return -1;

			unsigned int filesCount = get_files_count( dir );
			unsigned int currentFileIndex = 0;

			while ( !StringDeque.empty() )
			{
				szText = StringDeque.front();
				StringDeque.pop_front();
				if(zipDeque.size() > 0)
				{
					zipDir = zipDeque.front() + wstring( L"/" );
					zipDeque.pop_front();
				}

				std::vector<std::wstring> aCurFiles			= NSDirectory::GetFiles(szText);
				std::vector<std::wstring> aCurDirectories	= NSDirectory::GetDirectories(szText);

				for(size_t i = 0; i < aCurDirectories.size(); ++i)
				{
					std::wstring sDirName = NSSystemPath::GetFileName(aCurDirectories[i]);

					if (sorted)
					{
						if (sDirName == L"ppt")
						{
							StringDeque.push_front(aCurDirectories[i] );
							zipDeque.push_front( zipDir + sDirName );
						}
						else if(sDirName == L"xl")
						{
							StringDeque.push_front( aCurDirectories[i] );
							zipDeque.push_front( zipDir + sDirName );
						}
						else if (sDirName == L"word")
						{
							StringDeque.push_front( aCurDirectories[i] );
							zipDeque.push_front( zipDir + sDirName );
						}
						else if (sDirName == L"_rels")
						{
							StringDeque.push_front(aCurDirectories[i]);
							zipDeque.push_front(zipDir + sDirName);
						}
						else
						{
							StringDeque.push_back( aCurDirectories[i] );
							zipDeque.push_back( zipDir + sDirName );
						}
					}
					else
					{
						StringDeque.push_back( aCurDirectories[i] );
						zipDeque.push_back( zipDir + sDirName );
					}
				}

				for (size_t i = 0; i < aCurFiles.size(); ++i)
				{
					std::wstring cFileName = NSSystemPath::GetFileName(aCurFiles[i]);

                    if (    std::wstring::npos != cFileName.find(L"mimetype") ||
                            std::wstring::npos != cFileName.find(L"[Content_Types]") ||
                            cFileName == L".rels")
                    {
						file = NSSystemPath::Combine(szText, cFileName);
						zipFileName = zipDir + cFileName;

						oneZipFile(zf, file, zipFileName, 0, compressionLevel, bDateTime);

						aCurFiles.erase(aCurFiles.begin() + i, aCurFiles.begin() + i + 1);
						break;
					}
				}

				for (size_t i = 0; i < aCurFiles.size(); ++i)
				{
					std::wstring cFileName = NSSystemPath::GetFileName(aCurFiles[i]);
					file = NSSystemPath::Combine(szText, cFileName);
					zipFileName = zipDir + cFileName;

					oneZipFile(zf, file, zipFileName, method, compressionLevel, bDateTime);

					if ( progress != NULL )
					{
						short cancel = 0;
						long progressValue = ( 1000000 / filesCount * currentFileIndex );

						if(NULL != progress)
							(*progress)( UTILS_ONPROGRESSEVENT_ID, progressValue, &cancel );

						if ( cancel != 0 )
						{
							zipClose( zf, NULL );
						}
					}
					currentFileIndex++;
				}
			}
			zipClose( zf, NULL );

			if ( progress != NULL )
			{
				short cancel = 0;
				long progressValue = 1000000;
				if(NULL != progress)
					(*progress)( UTILS_ONPROGRESSEVENT_ID, progressValue, &cancel );
			}
		}
		return 0;
	}

	/*========================================================================================================*/

	int ZipFile( const WCHAR* inputFile, const WCHAR* outputFile, int method, int compressionLevel, bool bDateTime )
	{
		int err = -1;

		if (( inputFile != NULL) && (outputFile != NULL))
		{
			NSFile::CFileBinary oFile;

			zip_fileinfo zinfo;
			zinfo.dosDate = 0;
			zinfo.external_fa = 0;
			zinfo.internal_fa = 0;

			if (bDateTime)
			{
				struct tm edited;
				bool ok = NSFile::CFileBinary::GetTime(inputFile, &edited);
				if (ok)
				{
					zinfo.tmz_date.tm_sec = edited.tm_sec;
					zinfo.tmz_date.tm_min = edited.tm_min;
					zinfo.tmz_date.tm_hour = edited.tm_hour;
					zinfo.tmz_date.tm_mday = edited.tm_mday;
					zinfo.tmz_date.tm_mon = edited.tm_mon - 1;
					zinfo.tmz_date.tm_year = edited.tm_year;
				}
			}
			zip_fileinfo* zi_new = bDateTime ? &zinfo : NULL;

			if (oFile.OpenFile(inputFile))
			{
				DWORD dwSizeRead;
				BYTE* pData = new BYTE[oFile.GetFileSize()];
				if(oFile.ReadFile(pData, oFile.GetFileSize(), dwSizeRead))
				{
					zipFile zf = zipOpenHelp(outputFile);
					if (zf)
					{
						wstring zipFileName = NSFile::GetFileName(inputFile);
						std::string zipFileNameA = codepage_issue_fixToOEM(zipFileName);

						err = zipOpenNewFileInZip( zf, zipFileNameA.c_str(), zi_new, NULL, 0, NULL, 0, NULL, method, compressionLevel );
						err = zipWriteInFileInZip( zf, pData, dwSizeRead );
						err = zipCloseFileInZip( zf );
						err = zipClose( zf, NULL );
					}
				}
				RELEASEARRAYOBJECTS(pData);
			}
		}

		return err;
	}

	/*========================================================================================================*/


    // Validate existing descendants before the legacy recursive cleanup sees them.
    // The extraction root remains private/stable; this does not address concurrent edits.
    static bool ClearTreeHasNoLinks(const std::wstring& root)
    {
#if defined(_WIN32) || defined(_WIN64)
        std::vector<std::wstring> directories(1, root);
        while (!directories.empty())
        {
            std::wstring directory = directories.back();
            directories.pop_back();
            WIN32_FIND_DATAW item;
            HANDLE search = FindFirstFileW((directory + L"/*").c_str(), &item);
            if (search == INVALID_HANDLE_VALUE)
            {
                if (GetLastError() == ERROR_FILE_NOT_FOUND) continue;
                return false;
            }
            bool valid = true;
            do
            {
                std::wstring name(item.cFileName);
                if (name == L"." || name == L"..") continue;
                std::wstring path = directory + L"/" + name;
                if (!NSPackagePath::NoLinks(root, path)) { valid = false; break; }
                if (item.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                    directories.push_back(path);
            } while (FindNextFileW(search, &item));
            if (valid && GetLastError() != ERROR_NO_MORE_FILES) valid = false;
            FindClose(search);
            if (!valid) return false;
        }
#else
        std::vector<std::string> directories(1, NSFile::CUtf8Converter::GetUtf8StringFromUnicode(root));
        while (!directories.empty())
        {
            std::string directory = directories.back();
            directories.pop_back();
            DIR* stream = opendir(directory.c_str());
            if (!stream) return false;
            bool valid = true;
            for (;;)
            {
                errno = 0;
                struct dirent* item = readdir(stream);
                if (!item) { if (errno) valid = false; break; }
                std::string name(item->d_name);
                if (name == "." || name == "..") continue;
                std::string path = directory + "/" + name;
                struct stat info;
                if (lstat(path.c_str(), &info) != 0 || S_ISLNK(info.st_mode) ||
                    (!S_ISDIR(info.st_mode) && (!S_ISREG(info.st_mode) || info.st_nlink > 1)))
                { valid = false; break; }
                if (S_ISDIR(info.st_mode)) directories.push_back(path);
            }
            if (closedir(stream) != 0) valid = false;
            if (!valid) return false;
        }
#endif
        return true;
    }

	bool ClearDirectory( const WCHAR* dir, bool delDir )
	{
		bool result = false;

		if ( dir != NULL )
		{
			unsigned int size = (unsigned int)wcslen( dir );
			WCHAR* _dir = new WCHAR[size + 2];
			wcsncpy( _dir, dir, size );
			_dir[size++] = L'\0';
			_dir[size] = L'\0';

			NSDirectory::DeleteDirectory(_dir);

			if ( !delDir )
			{
				NSDirectory::GetDirectories(dir);
			}

			if ( _dir != NULL )
			{
				delete []_dir;
				_dir = NULL;
			}

			result = true;
		}
		else
		{
			result = false;
		}

		return result;
	}

	/*========================================================================================================*/

	int UnzipToDir( const WCHAR* zipFile, const WCHAR* unzipDir, const OnProgressCallback* progress, const WCHAR* password, bool opt_extract_without_path, bool clearOutputDirectory )
	{
		CunzFileWrapped ufw;
		ufw.Open(zipFile);
		int err = UnzipToDir(ufw.Get(), unzipDir, progress, password, opt_extract_without_path, clearOutputDirectory);

		// call Close() instead ~...() because of error code
		if(err == UNZ_OK)
			err = ufw.Close();
		return err;
	}

	/*========================================================================================================*/

	int UnzipToDir(BYTE* data, size_t len, const WCHAR* unzipDir, const OnProgressCallback* progress, const WCHAR* password, bool opt_extract_without_path, bool clearOutputDirectory )
	{
		CunzFileWrapped ufw;
		ufw.Open(data, len);
		int err = UnzipToDir(ufw.Get(), unzipDir, progress, password, opt_extract_without_path, clearOutputDirectory);

		// call Close() instead ~...() because of error code
		if(err == UNZ_OK)
			err = ufw.Close();
		return err;
	}

	/*========================================================================================================*/

	int UnzipToDir(unzFile uf, const WCHAR* unzipDir, const OnProgressCallback* progress, const WCHAR* password, bool opt_extract_without_path, bool clearOutputDirectory )
	{
		int err = -1;

		if ( uf != NULL && unzipDir != NULL )
		{
			if (!ValidateArchive(uf)) return UNZ_BADZIPFILE;
			std::wstring root;
			if (!NSPackagePath::Absolute(unzipDir, root) || !NSPackagePath::NoLinks(root, root))
				return UNZ_BADZIPFILE;
			if (NSDirectory::Exists(unzipDir))
				err = 0;

			if ( clearOutputDirectory )
			{
				if (!ClearTreeHasNoLinks(root)) return UNZ_BADZIPFILE;
				ClearDirectory( unzipDir );
				// ClearDirectory removes the root too; recreate it before validating entries.
				if (!NSDirectory::CreateDirectory(root) || !NSPackagePath::NoLinks(root, root))
					return UNZ_ERRNO;
			}

			if ( err == 0 )
			{
				if(NULL != password)
				{
					std::string passwordA = codepage_issue_fixToOEM(password);
					err = do_extract( uf, unzipDir, opt_extract_without_path, 1, passwordA.c_str(), progress );
				}
				else
					err = do_extract( uf, unzipDir, opt_extract_without_path, 1, NULL, progress );
			}
		}
		return err;
	}

	/*========================================================================================================*/

	int UncompressBytes( BYTE* destBuf, ULONG* destSize, const BYTE* sourceBuf, ULONG sourceSize )
	{
		int err = -1;

		err = uncompress( destBuf, destSize, sourceBuf, sourceSize );

		return err;
	}

	/*========================================================================================================*/

	int CompressBytes( BYTE* destBuf, ULONG* destSize, const BYTE* sourceBuf, ULONG sourceSize, SHORT level )
	{
		int err = -1;

		err = compress2( destBuf, destSize, sourceBuf, sourceSize, level );

		return err;
	}

	/*========================================================================================================*/

	bool IsArchive(const WCHAR* filename)
	{
		CunzFileWrapped ufw;
		ufw.Open(filename);
		return ufw.Get() != NULL;
	}

	/*========================================================================================================*/

	bool IsArchive(BYTE* data, size_t len)
	{
		CunzFileWrapped ufw;
		ufw.Open(data, len);
		return ufw.Get() != NULL;
	}

	/*========================================================================================================*/

	bool IsFileExistInArchive(const WCHAR* zipFile, const WCHAR* filePathInZip)
	{
		CunzFileWrapped ufw;
		ufw.Open(zipFile);
		unzFile uf = ufw.Get();

		return filePathInZip != NULL && uf != NULL && is_file_in_archive(uf, filePathInZip);
	}

	/*========================================================================================================*/

	bool IsFileExistInArchive(BYTE* data, size_t len, const WCHAR* filePathInZip)
	{
		CunzFileWrapped ufw;
		ufw.Open(data, len);
		unzFile uf = ufw.Get();

		return filePathInZip != NULL && uf != NULL && is_file_in_archive(uf, filePathInZip);
	}

	/*========================================================================================================*/

	bool LoadFileFromArchive(const WCHAR* zipFile, const WCHAR* filePathInZip, BYTE** fileInBytes, ULONG& nFileSize)
	{
		CunzFileWrapped ufw;
		ufw.Open(zipFile);
		unzFile uf = ufw.Get();

		return filePathInZip != NULL && uf != NULL && get_file_in_archive( uf, filePathInZip, fileInBytes, nFileSize);
	}

	/*========================================================================================================*/

	bool LoadFileFromArchive(BYTE* data, size_t len, const WCHAR* filePathInZip, BYTE** fileInBytes, ULONG& nFileSize)
	{
		CunzFileWrapped ufw;
		ufw.Open(data, len);
		unzFile uf = ufw.Get();

		return filePathInZip != NULL && uf != NULL && get_file_in_archive( uf, filePathInZip, fileInBytes, nFileSize);
	}

	/*========================================================================================================*/

	bool ExtractFiles(const wchar_t* zip_file_path, const ExtractedFileCallback& callback, void* pParam)
	{
		CunzFileWrapped wrapped;
		wrapped.Open(zip_file_path);
		unzFile uf = wrapped.Get();
		if (!callback || !ValidateArchive(uf)) return false;
		do
		{
			unz_file_info info;
			if (unzGetCurrentFileInfo(uf, &info, NULL, 0, NULL, 0, NULL, 0) != UNZ_OK)
				return false;
			std::wstring name;
			if (!CurrentName(uf, name)) return false;
			const int decision = EntryPathDecision(name);
			if (decision < 0) return false;
			if (decision > 0)
			{
				if (!get_file(uf, NULL, 0, NULL)) return false;
				const int next = unzGoToNextFile(uf);
				if (next == UNZ_END_OF_LIST_OF_FILE) return true;
				if (next != UNZ_OK) return false;
				continue;
			}
			BYTE* data = new(std::nothrow) BYTE[info.uncompressed_size ? info.uncompressed_size : 1];
			if (!data) return false;
			if (!get_file(uf, data, info.uncompressed_size, NULL)) { delete[] data; return false; }
			callback(name, data, info.uncompressed_size, pParam);
			delete[] data;
			const int next = unzGoToNextFile(uf);
			if (next == UNZ_END_OF_LIST_OF_FILE) return true;
			if (next != UNZ_OK) return false;
		} while (true);
	}

	/*========================================================================================================*/

	bool CompressFiles(const wchar_t* zip_file_path, const RequestFileCallback& callback, void* pParam, int compression_level)
	{
		zipFile zip_file_handle = zipOpenHelp(zip_file_path);

		if(NULL != zip_file_handle)
		{
			BYTE* pData = NULL;
			long nSize;
			std::wstring in_zip_filename;
			while(callback(in_zip_filename, pData, nSize, pParam))
			{
				std::string in_zip_filenameA = codepage_issue_fixToOEM(in_zip_filename);
				if (ZIP_OK != zipOpenNewFileInZip( zip_file_handle, in_zip_filenameA.c_str(), NULL, NULL, 0, NULL, 0, NULL, Z_DEFLATED, compression_level ) ||
						ZIP_OK != zipWriteInFileInZip(zip_file_handle, pData, nSize) ||
						ZIP_OK != zipCloseFileInZip(zip_file_handle))
				{
					zipClose(zip_file_handle, NULL);
					return false;
				}
			}
			zipClose(zip_file_handle, NULL);

			return true;
		}
		return false;
	}

	bool GetFilesSize(const WCHAR*  zip_file_path, const std::wstring& searchPattern, ULONG64& nCommpressed, ULONG64& nUncommpressed)
	{
		nCommpressed = 0;
		nUncommpressed = 0;
		unzFile unzip_file_handle = unzOpenHelp(zip_file_path);
		if (unzip_file_handle != NULL)
		{
			//todo implement true pattern
			bool isAny = false;
			std::wstring searchExt;
			if (0 == searchPattern.length() || searchPattern == L"*")
			{
				isAny = true;
			}
			else if (searchPattern.length() > 1)
			{
				searchExt = searchPattern.substr(2);
			}
			do
			{
				char filename_inzip[256];
				unz_file_info file_info;
				unzGetCurrentFileInfo(unzip_file_handle, &file_info, filename_inzip, sizeof(filename_inzip), NULL, 0, NULL, 0);
				std::wstring filenameW = codepage_issue_fixFromOEM(filename_inzip);
				std::transform(filenameW.begin(), filenameW.end(), filenameW.begin(), ::tolower);
				if (isAny || NSFile::GetFileExtention(filenameW) == searchExt)
				{
					nCommpressed += file_info.compressed_size;
					nUncommpressed += file_info.uncompressed_size;
				}
				// else just skip the erroneous file
			} while (UNZ_OK == unzGoToNextFile(unzip_file_handle));

			unzClose (unzip_file_handle);
			return true;
		}
		return false;
	}

	/*========================================================================================================*/

	static unsigned int get_files_count( const WCHAR* dirname )
	{
		unsigned int filescount = 0;

		if ( dirname != NULL )
		{
			deque<wstring> StringDeque;
			StringDeque.push_back( dirname );

			wstring file;
			wstring szText;

			while ( !StringDeque.empty() )
			{
				szText = StringDeque.front();

				StringDeque.pop_front();

				std::vector<std::wstring> aCurFiles = NSDirectory::GetFiles(szText);

				filescount += aCurFiles.size();
				std::vector<std::wstring> aCurDirectories = NSDirectory::GetDirectories(szText);

				for(size_t i = 0; i < aCurDirectories.size(); ++i)
				{
					std::wstring sCurDirectory = aCurDirectories[i];
					StringDeque.push_back( sCurDirectory );
				}
			}
		}

		return filescount;
	}
}
