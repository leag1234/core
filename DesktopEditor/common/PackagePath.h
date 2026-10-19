// SPDX-License-Identifier: AGPL-3.0-only
// Package paths must stay inside their extraction root before filesystem access.
#ifndef ELIXIR_PACKAGE_PATH_H
#define ELIXIR_PACKAGE_PATH_H
#include "File.h"
#include <vector>
#include <algorithm>
#include <cwctype>
#include <cstdlib>
#include <cerrno>
#if !defined(_WIN32) && !defined(_WIN64)
#include <sys/stat.h>
#include <unistd.h>
#endif
namespace NSPackagePath {
// Win32 aliases must never change the destination after lexical validation.
inline bool WindowsComponent(const std::wstring& part) {
    if (part.empty() || part == L"." || part == L"..") return true;
    if (part.back() == L'.' || part.back() == L' ') return false;
    std::wstring name = part.substr(0, part.find(L'.'));
    std::transform(name.begin(), name.end(), name.begin(), towupper);
    if (name == L"CON" || name == L"PRN" || name == L"AUX" || name == L"NUL") return false;
    if (name.size() == 4 && (name.substr(0, 3) == L"COM" || name.substr(0, 3) == L"LPT") &&
        ((name[3] >= L'1' && name[3] <= L'9') || name[3] == 0xB9 || name[3] == 0xB2 || name[3] == 0xB3)) return false;
    return part.find_first_of(L"<>\"|?*") == std::wstring::npos;
}
inline bool Normalize(std::wstring value, std::wstring& out, bool absolute, bool rootPath = false) {
    std::replace(value.begin(), value.end(), L'\\', L'/');
    // Only caller-supplied local drive roots may use an extended-length prefix.
    // Canonicalize the drive spelling; UNC and other device namespaces stay rejected.
    if (value.compare(0, 4, L"//?/") == 0) {
        if (!absolute || !rootPath || value.size() < 7 ||
            !((value[4] >= L'A' && value[4] <= L'Z') || (value[4] >= L'a' && value[4] <= L'z')) ||
            value[5] != L':' || value[6] != L'/') return false;
        value.erase(0, 4);
    }
    if (value.size() > 1 && value[0] == L'/' && value[1] == L'/') return false;
    std::wstring prefix;
    if (!value.empty() && value[0] == L'/') {
        if (!absolute) return false;
        prefix = L"/"; value.erase(0, 1);
    } else if (value.size() > 1 && value[1] == L':') {
        if (!absolute || !iswalpha(value[0]) || value.size() < 3 || value[2] != L'/') return false;
        prefix = value.substr(0, 3); value.erase(0, 3);
    }
    std::vector<std::wstring> parts;
    size_t start = 0;
    while (start <= value.size()) {
        size_t end = value.find(L'/', start);
        if (end == std::wstring::npos) end = value.size();
        std::wstring part = value.substr(start, end - start);
#if defined(_WIN32) || defined(_WIN64)
        if (!WindowsComponent(part)) return false;
#endif
        for (wchar_t c : part) {
            if (c < 32 || c == 127) return false;
#if defined(_WIN32) || defined(_WIN64)
            if (c == L':') return false;
#else
            if (c == L':' && !rootPath) return false;
#endif
        }
        if (part == L"..") {
            if (parts.empty()) return false;
            parts.pop_back();
        } else if (!part.empty() && part != L".") parts.push_back(part);
        if (end == value.size()) break;
        start = end + 1;
    }
    out = prefix;
    for (const auto& part : parts) {
        if (!out.empty() && out.back() != L'/') out += L'/';
        out += part;
    }
    return !out.empty();
}
inline bool Absolute(const std::wstring& value, std::wstring& out) {
    if ((!value.empty() && (value[0] == L'/' || value[0] == L'\\')) ||
        (value.size() > 1 && value[1] == L':')) return Normalize(value, out, true, true);
#if defined(_WIN32) || defined(_WIN64)
    std::vector<wchar_t> buffer(32768);
    DWORD size = GetCurrentDirectoryW(static_cast<DWORD>(buffer.size()), buffer.data());
    if (!size || size >= buffer.size()) return false;
    std::wstring cwd(buffer.data(), size);
#else
    char* raw = getcwd(nullptr, 0);
    if (!raw) return false;
    const std::string bytes(raw);
    free(raw);
    std::wstring cwd = NSFile::CUtf8Converter::GetUnicodeStringFromUTF8(reinterpret_cast<BYTE*>(const_cast<char*>(bytes.data())), static_cast<LONG>(bytes.size()));
#endif
    return Normalize(cwd + L"/" + value, out, true, true);
}
inline bool EqualPrefix(const std::wstring& path, const std::wstring& root) {
    if (root.empty() || path.size() < root.size()) return false;
    for (size_t i = 0; i < root.size(); ++i) {
#if defined(_WIN32) || defined(_WIN64)
        if (towlower(path[i]) != towlower(root[i])) return false;
#else
        if (path[i] != root[i]) return false;
#endif
    }
    return path.size() == root.size() || root.back() == L'/' || path[root.size()] == L'/';
}
// The caller supplies a private, stable extraction root with trusted ancestors.
// Reject existing linked
// destinations; this is not protection against concurrent same-user filesystem edits.
inline bool NoLinks(const std::wstring& root, const std::wstring& path) {
    if (!EqualPrefix(path, root)) return false;
    auto safeItem = [](const std::wstring& item, bool directory) {
#if defined(_WIN32) || defined(_WIN64)
        DWORD attrs = GetFileAttributesW(item.c_str());
        if (attrs == INVALID_FILE_ATTRIBUTES) {
            DWORD error = GetLastError();
            return !directory && (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND);
        }
        if ((attrs & FILE_ATTRIBUTE_REPARSE_POINT) || (directory && !(attrs & FILE_ATTRIBUTE_DIRECTORY))) return false;
        if (!(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
            HANDLE file = ::CreateFileW(item.c_str(), FILE_READ_ATTRIBUTES,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
                FILE_FLAG_OPEN_REPARSE_POINT, NULL);
            if (file == INVALID_HANDLE_VALUE) return false;
            BY_HANDLE_FILE_INFORMATION info;
            bool valid = GetFileInformationByHandle(file, &info) && info.nNumberOfLinks <= 1;
            CloseHandle(file);
            if (!valid) return false;
        }
#else
        struct stat info;
        const std::string utf8 = NSFile::CUtf8Converter::GetUtf8StringFromUnicode(item);
        if (lstat(utf8.c_str(), &info) != 0) return !directory && errno == ENOENT;
        if (S_ISLNK(info.st_mode) || (directory && !S_ISDIR(info.st_mode))) return false;
        if (!S_ISDIR(info.st_mode) && (!S_ISREG(info.st_mode) || info.st_nlink > 1)) return false;
#endif
        return true;
    };
    if (!safeItem(root, true)) return false;
    size_t at = root.size();
    while (at < path.size()) {
        if (path[at] == L'/') ++at;
        size_t end = path.find(L'/', at);
        if (end == std::wstring::npos) end = path.size();
        if (!safeItem(path.substr(0, end), false)) return false;
        if (end == path.size()) break;
        at = end;
    }
    return true;
}
inline int Hex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
inline bool Resolve(const std::wstring& rootValue, const std::wstring& baseValue,
                    std::wstring ref, std::wstring& output, bool uri = false,
                    bool containedAbsolute = false) {
    std::wstring root, base;
    if (!Absolute(rootValue, root) || !Absolute(baseValue, base) || !EqualPrefix(base, root)) return false;
    if (uri) {
        ref = ref.substr(0, ref.find_first_of(L"?#"));
        if (ref.empty()) return false;
        std::string encoded = NSFile::CUtf8Converter::GetUtf8StringFromUnicode(ref), decoded;
        for (size_t i = 0; i < encoded.size(); ++i) {
            if (encoded[i] == '%') {
                if (i + 2 >= encoded.size() || Hex(encoded[i+1]) < 0 || Hex(encoded[i+2]) < 0) return false;
                decoded += char((Hex(encoded[i+1]) << 4) | Hex(encoded[i+2])); i += 2;
            } else decoded += encoded[i];
        }
        ref = NSFile::CUtf8Converter::GetUnicodeStringFromUTF8(reinterpret_cast<BYTE*>(const_cast<char*>(decoded.data())), static_cast<LONG>(decoded.size()));
    }
    std::replace(ref.begin(), ref.end(), L'\\', L'/');
    if ((!ref.empty() && ref[0] == L'/') || (ref.size() > 1 && ref[1] == L':')) {
        // A single leading slash in a package URI names the package root.
        // Internal callers may also supply an already-contained absolute path.
        if (ref.size() > 1 && ref[0] == L'/' && ref[1] == L'/') return false;
        if (containedAbsolute && Normalize(ref, output, true, true) && EqualPrefix(output, root)) {
            // Preserve an already resolved internal filesystem path.
        } else if (uri && ref[0] == L'/') {
            std::wstring relative;
            if (!Normalize(ref.substr(1), relative, false)) return false;
            output = root + (root.back() == L'/' ? L"" : L"/") + relative;
        } else return false;
    } else {
        std::wstring relativeBase = base.substr(root.size());
        if (!relativeBase.empty() && relativeBase[0] == L'/') relativeBase.erase(0, 1);
        std::wstring relative;
        if (!Normalize(relativeBase.empty() ? ref : relativeBase + L'/' + ref, relative, false)) return false;
        output = root + (root.back() == L'/' ? L"" : L"/") + relative;
    }
    return EqualPrefix(output, root) && NoLinks(root, output);
}
}
#endif
