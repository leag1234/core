#!/usr/bin/env python3
import argparse
import json
import os
import re
from pathlib import Path


SOURCE_EXTENSIONS = {".c", ".cc", ".cpp", ".cxx", ".h", ".hpp"}
VENDORED_PARTS = (
    ("DesktopEditor", "xml", "libxml2"),
    ("DesktopEditor", "cximage", "tiff"),
    ("DesktopEditor", "cximage", "png"),
    ("DesktopEditor", "cximage", "zlib"),
    ("DesktopEditor", "freetype-2.10.4"),
    ("DesktopEditor", "freetype-2.15.0"),
    ("DesktopEditor", "freetype-2.5.2"),
    ("OfficeUtils", "src", "zlib-1.2.11"),
    ("OfficeUtils", "src", "zlib-1.4"),
    ("DesktopEditor", "raster", "Jp2", "openjpeg", "openjpeg-2.4.0"),
    ("DesktopEditor", "raster", "Jp2", "openjpeg", "openjpeg-2.5.4"),
)


def is_vendored(path):
    parts = path.parts
    return any(parts[index:index + len(prefix)] == prefix for prefix in VENDORED_PARTS for index in range(len(parts)))


def strip_cmake_comment(text):
    return re.sub(r"#.*", "", text)


def cmake_tokens(text):
    text = strip_cmake_comment(text)
    return re.findall(r'"(?:[^"\\]|\\.)*"|[^\s()]+|[()]', text)


def parse_cmake_command(tokens, start):
    if start >= len(tokens) or not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", tokens[start]):
        return None
    command = tokens[start].upper()
    end = start + 1
    depth = 1
    while end < len(tokens):
        if tokens[end] == "(":
            depth += 1
        elif tokens[end] == ")":
            depth -= 1
            if depth == 0:
                return command, tokens[start + 1:end], end + 1
        end += 1
    return None


def unquote(value):
    if value.startswith('"') and value.endswith('"'):
        return value[1:-1].replace('\\"', '"').replace("\\\\", "\\")
    return value


def expand(value, variables, unresolved):
    def replace(match):
        name = match.group(1)
        if name in variables:
            return variables[name]
        unresolved.add(name)
        return match.group(0)

    return re.sub(r"\$\{([A-Za-z_][A-Za-z0-9_]*)\}", replace, value)


def normalize_cmake_path(value, current_dir, root):
    value = value.replace("/core/", "/work/", 1)
    path = Path(value)
    if path.is_absolute():
        return path
    return (current_dir / path).resolve()


def scan_cmake(path, root):
    missing = []
    unresolved = set()
    variables = {
        "CMAKE_CURRENT_SOURCE_DIR": str(path.parent),
        "CMAKE_CURRENT_LIST_DIR": str(path.parent),
        "CMAKE_SOURCE_DIR": str(root),
    }
    text = path.read_text(encoding="utf-8", errors="replace")
    tokens = cmake_tokens(text)
    index = 0
    while index < len(tokens):
        command = parse_cmake_command(tokens, index)
        if command is None:
            index += 1
            continue
        name, arguments, index = command
        if name == "SET" and arguments:
            variable = arguments[0]
            if re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", variable) and len(arguments) > 1:
                variables[variable] = expand(unquote(arguments[1]), variables, unresolved)
        elif name in {"ADD_LIBRARY", "ADD_EXECUTABLE", "TARGET_SOURCES"} and arguments:
            for argument in arguments[1 if name != "TARGET_SOURCES" else 2:]:
                expanded = expand(unquote(argument), variables, unresolved)
                if not expanded or expanded.startswith("$"):
                    continue
                candidate = normalize_cmake_path(expanded, path.parent, root)
                if candidate.suffix.lower() in SOURCE_EXTENSIONS and not candidate.exists():
                    missing.append((candidate, path, argument))
    return missing, unresolved


def scan_cmake_tree(path, root, inherited=None):
    missing = []
    unresolved = set()
    variables = dict(inherited or {})
    variables.update({
        "CMAKE_CURRENT_SOURCE_DIR": str(path.parent),
        "CMAKE_CURRENT_LIST_DIR": str(path.parent),
        "CMAKE_SOURCE_DIR": str(root),
    })
    tokens = cmake_tokens(path.read_text(encoding="utf-8", errors="replace"))
    index = 0
    while index < len(tokens):
        command = parse_cmake_command(tokens, index)
        if command is None:
            index += 1
            continue
        name, arguments, index = command
        if name == "SET" and arguments:
            variable = arguments[0]
            if re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", variable) and len(arguments) > 1:
                variables[variable] = expand(unquote(arguments[1]), variables, unresolved)
        elif name == "ADD_SUBDIRECTORY" and arguments:
            expanded = expand(unquote(arguments[0]), variables, unresolved)
            child = Path(expanded)
            if not child.is_absolute():
                child = path.parent / child
            child_file = child / "CMakeLists.txt"
            if child_file.is_file() and not is_vendored(child_file):
                found, unknown = scan_cmake_tree(child_file, root, variables)
                missing.extend(found)
                unresolved.update(unknown)
    return missing, unresolved


def scan_all_cmake_files(path, root):
    missing, unresolved = scan_cmake_tree(path, root)
    for child in path.parent.rglob("CMakeLists.txt"):
        if child == path or is_vendored(child):
            continue
        found, unknown = scan_cmake_tree(child, root)
        missing.extend(found)
        unresolved.update(unknown)
    return missing, unresolved


def qmake_value(value, variables, unresolved):
    value = expand(value, variables, unresolved)
    value = value.replace("$$PWD", "__PWD__")
    value = value.replace("$$CORE_ROOT_DIR", "__CORE__")
    value = value.replace("__PWD__", variables.get("PWD", "$$PWD"))
    value = value.replace("__CORE__", variables.get("CORE_ROOT_DIR", "$$CORE_ROOT_DIR"))
    return value


def scan_qmake(path, root):
    missing = []
    unresolved = set()
    variables = {"PWD": str(path.parent), "CORE_ROOT_DIR": str(root)}
    for line_number, line in enumerate(path.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
        line = line.split("#", 1)[0].strip()
        if "=" not in line:
            continue
        key, value = (part.strip() for part in line.split("=", 1))
        if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", key):
            continue
        variables[key] = qmake_value(value, variables, unresolved)
        for item in variables[key].split():
            item = item.rstrip("\\")
            if Path(item).suffix.lower() not in SOURCE_EXTENSIONS:
                continue
            candidate = Path(item)
            if not candidate.is_absolute():
                candidate = path.parent / candidate
            if not candidate.exists():
                missing.append((candidate.resolve(), path, f"{line_number}:{item}"))
    return missing, unresolved


def scan_qmake_file(path, root):
    missing = []
    unresolved = set()
    variables = {"PWD": str(path.parent), "CORE_ROOT_DIR": str(root)}
    for line_number, line in enumerate(path.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
        line = line.split("#", 1)[0].strip()
        if "=" not in line:
            continue
        key, value = (part.strip() for part in line.split("=", 1))
        if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", key):
            continue
        variables[key] = qmake_value(value, variables, unresolved)
        for item in variables[key].split():
            item = item.rstrip("\\")
            if Path(item).suffix.lower() not in SOURCE_EXTENSIONS:
                continue
            candidate = Path(item)
            if not candidate.is_absolute():
                candidate = path.parent / candidate
            if not candidate.exists():
                missing.append((candidate.resolve(), path, f"{line_number}:{item}"))
    return missing, unresolved


def scan_json(path, root):
    missing = []
    unresolved = set()
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return missing, unresolved
    if not isinstance(data, dict):
        return missing, unresolved
    groups = [data] if "files" in data else data.get("compile_files_array", [])
    for group in groups:
        if not isinstance(group, dict):
            continue
        folder = Path(group.get("folder", "."))
        if not folder.is_absolute():
            folder = path.parent / folder
        for item in group.get("files", []):
            candidate = Path(item)
            if not candidate.is_absolute():
                candidate = folder / candidate
            if candidate.suffix.lower() in SOURCE_EXTENSIONS and not candidate.exists():
                missing.append((candidate.resolve(), path, item))
    return missing, unresolved


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("root", nargs="?", default=".")
    args = parser.parse_args()
    root = Path(args.root).resolve()
    missing = []
    unresolved = set()
    for path in root.rglob("*"):
        if not path.is_file() or is_vendored(path):
            continue
        if path.name == "CMakeLists.txt" or path.suffix == ".cmake":
            if path.name == "CMakeLists.txt":
                found, unknown = scan_all_cmake_files(path, root)
            else:
                found, unknown = scan_cmake(path, root)
        elif path.suffix in {".pri", ".pro"}:
            found, unknown = scan_qmake_file(path, root)
        elif path.suffix == ".json":
            found, unknown = scan_json(path, root)
        else:
            continue
        missing.extend(found)
        unresolved.update(unknown)
    for candidate, source, reference in sorted(missing, key=lambda item: (str(item[1]), str(item[0]))):
        print(f"MISSING {candidate} [{source}:{reference}]")
    for name in sorted(unresolved):
        print(f"UNRESOLVED {name}")
    print(f"SUMMARY missing={len(missing)} unresolved={len(unresolved)}")


if __name__ == "__main__":
    main()
