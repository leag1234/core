import sys
sys.path.append("../../../../../build_tools/scripts")
sys.path.append("../../../../Common/js")
import base
import common

base.configure_common_apps()

if not base.is_dir("xml"):
  base.copy_dir("../../../xml", "./xml")
  base.replaceInFile("./xml/src/xmllight_private.h", "#include \"../../common/", "#include \"../../../../../common/")
  base.replaceInFile("./xml/src/xmllight_private.h", "#include \"../../../UnicodeConverter/", "#include \"../../../../../../UnicodeConverter/")
  base.replaceInFile("./xml/include/xmlutils.h", "#include \"../../common/", "#include \"../../../../../common/")
  base.replaceInFile("./xml/libxml2/globals.c", "int xmlGetWarningsDefaultValue = 1;", "int xmlGetWarningsDefaultValue = 0;")
  base.replaceInFile("./xml/libxml2/globals.c", "static int xmlGetWarningsDefaultValueThrDef = 1;", "static int xmlGetWarningsDefaultValueThrDef = 0;")

if not base.is_dir("freetype-2.10.4"):
  base.copy_dir("../../../freetype-2.10.4", "./freetype-2.10.4")

base.replaceInFile("../../../../Common/3dParty/icu/icu/source/common/udata.cpp", "\n{\n    UDataMemory tData;", "\n{\n#ifdef BUILDING_WASM_MODULE\nreturn NULL;\n#endif\n    UDataMemory tData;")
base.replaceInFile("../../../../DesktopEditor/cximage/png/pnglibconf.h", "#define PNG_CONSOLE_IO_SUPPORTED", "//#define PNG_CONSOLE_IO_SUPPORTED")

base.replaceInFile("../../../../Common/3dParty/openssl/openssl/crypto/sha/sha512.c", "const SHA_LONG64 *W = in;", "const SHA_LONG64 *W = (const SHA_LONG64*)in;")
