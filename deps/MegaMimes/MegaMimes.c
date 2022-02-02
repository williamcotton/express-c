#include "MegaMimes.h"
#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

static void splitFileParts(const char *path, char **filename,
                           char **fileextension, char **fileparents) {
  const char *slashPos = strrchr(path, FILE_PATH_SEP);
  if (!slashPos) {
    *fileparents = malloc(1);
    strcpy(*fileparents, "");

    const char *dotPos = strrchr(path, '.');
    if (!dotPos) { // no  extension
      *fileextension = malloc(1);
      strcpy(*fileextension, "");
      *filename = malloc(strlen(path));
      strcpy(*filename, path);
    } else {
      *fileextension = malloc(strlen(dotPos) + 1);
      strcpy(*fileextension, dotPos);
      size_t bytes = dotPos - path; // do not add the dot
      *filename = malloc(bytes + 1);
      strncpy(*filename, path, bytes);
      (*filename)[bytes] = '\0';
    }
  } else {
    size_t bytes = slashPos - path + 1;
    *fileparents = malloc(bytes + 1);
    strncpy(*fileparents, path, bytes);
    (*fileparents)[bytes] = '\0';

    const char *dotPos = strrchr(path, '.');
    if (dotPos < slashPos || dotPos == NULL) { // not an extension
      *fileextension = malloc(1);
      strcpy(*fileextension, "");
      *filename = malloc(strlen(slashPos));
      strcpy(*filename, slashPos + 1);
    } else {
      *fileextension = malloc(strlen(dotPos));
      strcpy(*fileextension, dotPos);
      size_t sbytes = dotPos - slashPos - 1;
      *filename = malloc(sbytes + 1);
      strncpy(*filename, slashPos + 1, sbytes);
      (*filename)[sbytes] = '\0';
    }
  }
}

static bool splitMimeTypeParts(const char *target, char **firstPart,
                               char **lastPart, char **trailingPart) {
  const size_t MAX_PART_SIZE = 64;
  char *mimeFirstPart = malloc(MAX_PART_SIZE),
       *mimeLastPart = malloc(MAX_PART_SIZE),
       *trailingDetails = malloc(MAX_PART_SIZE * 2);
  const char *slashPosition = strchr(target, '/');
  if (!slashPosition)
    return false;
  const char *colonPosition = strchr(target, ';');
  if (!colonPosition)
    colonPosition = target + strlen(target);

  size_t firstPartLen = slashPosition - target;
  assert(firstPartLen < MAX_PART_SIZE);
  strncpy(mimeFirstPart, target, firstPartLen);
  mimeFirstPart[firstPartLen] = '\0';

  size_t lastPartLen = colonPosition - slashPosition;
  assert(lastPartLen < MAX_PART_SIZE);
  strncpy(mimeLastPart, slashPosition + 1, lastPartLen - 1);
  mimeLastPart[lastPartLen - 1] = '\0';

  if (*colonPosition) {
    size_t trailingLen = target + strlen(target) - colonPosition;
    assert(trailingLen < MAX_PART_SIZE * 2);
    strncpy(trailingDetails, colonPosition + 1, trailingLen - 1);
    trailingDetails[trailingLen - 1] = '\0';
  } else {
    trailingDetails[0] = '\0';
  }
  *firstPart = mimeFirstPart;
  *lastPart = mimeLastPart;
  *trailingPart = trailingDetails;

  return true;
}

static bool matchesMimeType(const char *target, const char *mimetype) {
  char *targetFirstPart, *targetLastPart, *targetDetailsPart;
  char *mimeFirstPart, *mimeLastPart, *mimeDetailsPart;

  if (!splitMimeTypeParts(target, &targetFirstPart, &targetLastPart,
                          &targetDetailsPart))
    return false;
  if (!splitMimeTypeParts(mimetype, &mimeFirstPart, &mimeLastPart,
                          &mimeDetailsPart)) {
    free(targetFirstPart);
    free(targetLastPart);
    free(targetDetailsPart);
    return false;
  }
  bool ret = ((strcmp(targetFirstPart, mimeFirstPart) == 0 ||
               strcmp(mimeFirstPart, "*") == 0) &&
              (strcmp(targetLastPart, mimeLastPart) == 0 ||
               strcmp(mimeLastPart, "*") == 0));

  free(targetFirstPart);
  free(targetLastPart);
  free(targetDetailsPart);
  free(mimeFirstPart);
  free(mimeLastPart);
  free(mimeDetailsPart);

  return ret;
}

static bool isNotReadableFile(const char *url) {
  FILE *file = fopen(url, "rb");
  if (!file)
    return false;

  char c = fgetc(file);
  fseek(file, 0, SEEK_END);
  long long pos = ftell(file);

  fclose(file);

  return pos == LLONG_MAX && c == (char)WEOF;
}

static bool searchThroughMimes(const char *target, const char **extension,
                               const char **name, const char **type,
                               bool mimetype, bool reset) {

  static THREAD_LOCAL size_t position = 0;
  if (reset)
    position = 0;

  if (!target)
    return false;

  for (; MegaMimeTypes[position][0]; ++position) {
    // printf("%s\t",  MegaMimeTypes[position][0]);

    if (mimetype) {
      if (matchesMimeType(MegaMimeTypes[position][MIMETYPE_POS], target)) {
        if (extension)
          *extension = MegaMimeTypes[position][EXTENSION_POS];
        if (name)
          *name = MegaMimeTypes[position][MIMENAME_POS];
        if (type)
          *type = MegaMimeTypes[position][MIMETYPE_POS];
        ++position;
        return true;
      }
    } else {
      if (target[0] == '*')
        target++;

      if (strcmp(target, (MegaMimeTypes[position][EXTENSION_POS]) + 1) ==
          0) { // start comparing from . not *
        if (extension)
          *extension = MegaMimeTypes[position][EXTENSION_POS];
        if (name)
          *name = MegaMimeTypes[position][MIMENAME_POS];
        if (type)
          *type = MegaMimeTypes[position][MIMETYPE_POS];
        ++position;
        return true;
      }
    }
  }
  return false;
}

static const char *guessFileEncoding(const char *url) {
  FILE *pFile = fopen(url, "rb");
  if (!pFile)
    return NULL;

  // A is just a place holder
  unsigned char firstByte = fgetc(pFile);
  unsigned char secondByte = !feof(pFile) ? fgetc(pFile) : 'A';
  unsigned char thirdByte = !feof(pFile) ? fgetc(pFile) : 'A';
  unsigned char fourthByte = !feof(pFile) ? fgetc(pFile) : 'A';

  if (firstByte == 0xEF && secondByte == 0xBB && thirdByte == 0xBF) {
    return "UTF-8";
  } else if (firstByte == 0xFF && secondByte == 0xFE) {
    return "UTF-16LE";
  } else if (firstByte == 0xFE && secondByte == 0xFF) {
    return "UTF-16BE";
  } else if (firstByte == 0xFF && secondByte == 0xFE && thirdByte == 0x0 &&
             fourthByte == 0x0) {
    return "UTF-32LE";
  } else if (firstByte == 0x00 && secondByte == 0x00 && thirdByte == 0xFE &&
             fourthByte == 0xFF) {
    return "UTF-32BE";
  } else
    return NULL;
}

const char *getMegaMimeType(const char *pFilePath) // TODO change in header
{
  if (pFilePath == NULL)
    return NULL;

  char *pDotPos = strrchr(pFilePath, '.');
  char *pSeparator = strrchr(pFilePath, FILE_PATH_SEP);

  // no extension. Cannot determine mime type
  if (!pDotPos)
    return NULL;

  if (pSeparator)
    if (pDotPos < pSeparator)
      return NULL;

  const char *name, *type, *ext;
  if (searchThroughMimes(pDotPos, &ext, &name, &type, false, true)) {
    return type;
  }
  return NULL;
}

const char **getMegaMimeExtensions(const char *pMimeType) {
  const char *ext;
  size_t number = 5, pos = 0;
  const char **extensions = calloc(sizeof(char *), number);

  searchThroughMimes(NULL, NULL, NULL, NULL, true, true); // reset
  while (searchThroughMimes(pMimeType, &ext, NULL, NULL, true, false)) {
    if (pos == number - 1) {
      number += 5;
      extensions =
          (const char **)realloc(extensions, (sizeof(char *)) * (number));
    }
    extensions[pos++] = ext;
  }
  extensions[pos] = NULL;
  if (!pos) {
    free(extensions);
    return NULL;
  } else
    return extensions;
}

bool isTextFile(const char *url) {
  if (isNotReadableFile(url)) {
    return false;
  }

  const char *zEnc = guessFileEncoding(url);
  if (!zEnc)
    zEnc = "UTF-8";

  if (strcmp(zEnc, "UTF-8") == 0) {
    FILE *pFile = fopen(url, "r");
    if (!pFile)
      return false;

    char ch = fgetc(pFile);
    while (!feof(pFile) && ch != (char)EOF) {
      if (iscntrl(ch) && !isspace(ch)) {
        fclose(pFile);
        return 0;
      }
      ch = fgetc(pFile);
    }
    fclose(pFile);
    return true;
  } else {
    FILE *pFile = fopen(url, "r,ccs=UNICODE");
    if (!pFile)
      return false;

    for (wint_t ch = fgetwc(pFile); ch != WEOF && !feof(pFile);
         ch = fgetwc(pFile)) {
      if (iswcntrl(ch) && !iswspace(ch)) {
        fclose(pFile);
        return 0;
      }
    }
    fclose(pFile);
    return true;
  }
  return false;
}

bool isBinaryFile(const char *url) { return !isTextFile(url); }

MegaFileInfo *getMegaFileInformation(const char *pFilePath) {
  FILE *fp = fopen(pFilePath, "rb");
  if (!fp)
    return NULL;
  if (isNotReadableFile(pFilePath)) {
    return NULL;
  }

  MegaFileInfo *info = malloc(sizeof(MegaFileInfo));
  char *filename, *fileextension, *fileparents;
  splitFileParts(pFilePath, &filename, &fileextension, &fileparents);

  info->mBaseDir = fileparents;
  info->mBaseName = filename;
  info->mExtension = fileextension;

  fseek(fp, 0, SEEK_END);
  info->mFileSize = ftell(fp);

  const char *mimetype = NULL, *mimename = NULL;
  searchThroughMimes(fileextension, NULL, &mimename, &mimetype, false, true);
  info->mMimeName = mimename;
  info->mMimeType = mimetype;

  info->mTextFile = isTextFile(pFilePath);
  if (info->mTextFile) {
    const char *enc = getMegaTextFileEncoding(pFilePath);
    info->mTextEncoding = enc;
  } else {
    info->mTextEncoding = NULL;
  }

  return info;
}

const char *getMegaTextFileEncoding(const char *path) {
  return guessFileEncoding(path);
}

void freeMegaFileInfo(MegaFileInfo *pData) {
  if (!pData)
    return;

  free(pData->mBaseDir);
  free(pData->mBaseName);
  free(pData->mExtension);

  free(pData);
}

void freeMegaString(char *pData) {
  if (pData)
    free(pData);
}

void freeMegaStringArray(char **pData) {
  if (!pData)
    return;
  free(pData);
}

const char *MegaMimeTypes[][COMPONENTS_NUMBER] = {
    {"*.1", "Roff Manpage", "text/troff", "0"},
    {"*.3ds", "3DS", "image/x-3ds", "0"},
    {"*.3fr", "Hasselblad raw image", "image/x-raw-hasselblad", "0"},
    {"*.3g2", "3G2", "video/3gpp2", "0"},
    {"*.3gp", "3GP", "video/3gpp", "0"},
    {"*.3mf", "3MF 3D Manufacturing Format",
     "application/vnd.ms-package.3dmanufacturing-3dmodel+xml", "0"},
    {"*.4th", "Forth source code", "text/x-forth", "0"},
    {"*.669", "Module Music Formats (Mods)", "audio/x-mod", "0"},
    {"*.6pl", "Perl 6", "text/x-perl", "0"},
    {"*.7z", "7-zip archive", "application/x-7z-compressed", "0"},
    {"*.8svx", "8-Bit Sampled Voice", "audio/8svx", "0"},
    {"*.ML", "Standard ML", "text/x-ocaml", "0"},
    {"*.MYD", "MySQL MISAM Data", "application/x-mysql-misam-data", "0"},
    {"*.MYI", "MySQL MISAM Compressed Index",
     "application/x-mysql-misam-compressed-index", "0"},
    {"*.R", "R", "text/x-rsrc", "0"},
    {"*.TIF", "TIFF Uncompressed File with Exif Metadata", "image/tiff", "0"},
    {"*.XXX", "Template:File Format/Preload", "application/octet-stream", "0"},
    {"*.Z", "Compress", "application/x-compress", "0"},
    {"*.aa", "Audible.Com File Format", "audio/x-pn-audibleaudio", "0"},
    {"*.ac3", "Dolby Digital AC-3", "audio/ac3", "0"},
    {"*.acsm", "Adobe Content Server Message File",
     "application/vnd.adobe.adept+xml", "0"},
    {"*.ada", "Ada source code", "text/x-ada", "0"},
    {"*.adf", "AppleDouble", "multipart/appledouble", "0"},
    {"*.afa", "Astrotite", "application/x-astrotite-afa", "0"},
    {"*.afm", "Adobe Font Metric", "application/x-font-adobe-metric", "0"},
    {"*.ai", "Adobe Illustrator Artwork", "application/illustrator", "0"},
    {"*.aif", "Audio Interchange File Format", "audio/x-aiff", "0"},
    {"*.aifc", "Audio Interchange File Format (compressed)", "audio/x-aiff",
     "0"},
    {"*.aiff", "AIFF", "audio/x-aiff", "0"},
    {"*.air", "Adobe Air 1.0",
     "application/"
     "vnd.adobe.air-application-installer-package+zip;version=\"1.0\"",
     "0"},
    {"*.aj", "AspectJ source code", "text/x-aspectj", "0"},
    {"*.amr", "AMR, Adaptive Multi-Rate Speech Codec", "audio/amr", "0"},
    {"*.anim", "Unity3D Asset", "text/x-yaml", "0"},
    {"*.anpa", "American Newspaper Publishers Association Wire Feeds",
     "text/vnd.iptc.anpa", "0"},
    {"*.ans", "7-bit ANSI Text", "text/plain", "0"},
    {"*.apl", "APL", "text/apl", "0"},
    {"*.apng", "Animated Portable Network Graphics", "image/vnd.mozilla.apng",
     "0"},
    {"*.applescript", "AppleScript source code", "text/x-applescript", "0"},
    {"*.apr", "Lotus Approach View File 97",
     "application/vnd.lotus-approach;version=\"97\"", "0"},
    {"*.apt", "Lotus Approach View File", "application/vnd.lotus-approach",
     "0"},
    {"*.arc", "Internet Archive 1.0",
     "application/x-internet-archive;version=\"1.0\"", "0"},
    {"*.arw", "Sony raw image", "image/x-raw-sony", "0"},
    {"*.as", "ActionScript source code", "text/x-actionscript", "0"},
    {"*.asc", "7-bit ASCII Text", "text/plain", "0"},
    {"*.asciidoc", "Asciidoc source code", "text/x-asciidoc", "0"},
    {"*.asf", "Advanced Systems Format", "application/vnd.ms-asf", "0"},
    {"*.asice", "Extended Associated Signature Container",
     "application/vnd.etsi.asic-e+zip", "0"},
    {"*.asics", "Simple Associated Signature Container",
     "application/vnd.etsi.asic-s+zip", "0"},
    {"*.asn", "ASN.1", "text/x-ttcn-asn", "0"},
    {"*.asp", "ASP", "application/x-aspx", "0"},
    {"*.aspx", "ASP .NET", "text/aspdotnet", "0"},
    {"*.asy", "LTspice Symbol", "text/x-spreadsheet", "0"},
    {"*.atom", "Atom", "application/atom+xml", "0"},
    {"*.au", "uLaw/AU Audio File", "audio/basic", "0"},
    {"*.avi", "AVI (Audio Video Interleaved) File Format", "video/msvideo",
     "0"},
    {"*.awb", "Adaptive Multi-Rate Wideband Audio", "audio/amr-wb", "0"},
    {"*.awk", "AWK script", "text/x-awk", "0"},
    {"*.axc", "Axc", "text/plain", "0"},
    {"*.axx", "AxCrypt", "application/x-axcrypt", "0"},
    {"*.b", "Brainfuck", "text/x-brainfuck", "0"},
    {"*.b6z", "B6Z", "application/x-b6z-compressed", "0"},
    {"*.bas", "Basic source code", "text/x-basic", "0"},
    {"*.bay", "Casio raw image", "image/x-raw-casio", "0"},
    {"*.bik", "Bink Video", "video/vnd.radgamettools.bink", "0"},
    {"*.bik2", "Bink Video Format 2",
     "video/vnd.radgamettools.bink;version=\"2\"", "0"},
    {"*.bin", "MacBinary", "application/x-macbinary", "0"},
    {"*.bmp", "Windows bitmap", "image/bmp", "0"},
    {"*.bpg", "Better Portable Graphics", "image/x-bpg", "0"},
    {"*.bpm", "BizAgi Process Modeler", "application/bizagi-modeler", "0"},
    {"*.bsp", "BSP", "model/vnd.valve.source.compiled-map", "0"},
    {"*.btf", "BigTIFF", "image/tiff", "0"},
    {"*.bw", "Silicon Graphics Image", "image/x-sgi-bw", "0"},
    {"*.bz2", "Bzip2", "application/x-bzip2", "0"},
    {"*.c", "C source code", "text/x-csrc", "0"},
    {"*.cab", "Cabinet", "application/vnd.ms-cab-compressed", "0"},
    {"*.cabal", "Cabal Config", "text/x-haskell", "0"},
    {"*.cap", "pcap Packet Capture", "application/vnd.tcpdump.pcap", "0"},
    {"*.catmaterial", "CATIA 5", "application/octet-stream;version=\"5\"", "0"},
    {"*.catpart", "CATIA Model (Part Description) 5",
     "application/octet-stream;version=\"5\"", "0"},
    {"*.catproduct", "CATIA Product Description 5",
     "application/octet-stream;version=\"5\"", "0"},
    {"*.cbl", "COBOL source code", "text/x-cobol", "0"},
    {"*.cbor", "Concise Binary Object Representation container",
     "application/cbor", "0"},
    {"*.cbz", "Comic Book Archive", "application/x-cbr", "0"},
    {"*.cca", "cc:Mail Archive Email Format", "application/octet-stream", "0"},
    {"*.cda", "CD Audio", "application/x-cdf", "0"},
    {"*.cdf", "netCDF-3 Classic", "application/x-netcdf", "0"},
    {"*.cdx", "Chemical Draw Exchange Format", "chemical/x-cdx", "0"},
    {"*.cfm", "ColdFusion source code", "text/x-coldfusion", "0"},
    {"*.cfs", "Compact File Set", "application/x-cfs-compressed", "0"},
    {"*.cgi", "CGI script", "text/x-cgi", "0"},
    {"*.cgm", "Computer Graphics Metafile", "image/cgm", "0"},
    {"*.chm", "Microsoft Compiled HTML Help", "application/vnd.ms-htmlhelp",
     "0"},
    {"*.chrt", "KChart File", "application/x-kchart", "0"},
    {"*.chs", "C2hs Haskell", "text/x-haskell", "0"},
    {"*.cif", "CIF", "chemical/x-cif", "0"},
    {"*.ck", "ChucK", "text/x-java", "0"},
    {"*.cl", "Common Lisp source code", "text/x-common-lisp", "0"},
    {"*.class", "Java Class File", "application/x-java", "0"},
    {"*.clj", "Clojure source code", "text/x-clojure", "0"},
    {"*.cls", "Visual basic source code", "text/x-vbasic", "0"},
    {"*.cmake", "CMake", "text/x-cmake", "0"},
    {"*.cml", "CML", "chemical/x-cml", "0"},
    {"*.cob", "COBOL", "text/x-cobol", "0"},
    {"*.cod", "Lightning Strike", "image/cis-cod", "0"},
    {"*.coffee", "CoffeeScript", "application/vnd.coffeescript", "0"},
    {"*.cp", "Component Pascal", "text/x-pascal", "0"},
    {"*.cpio", "UNIX CPIO Archive", "application/x-cpio", "0"},
    {"*.cpp", "C++ source code", "text/x-c++src", "0"},
    {"*.cr", "Crystal", "text/x-crystal", "0"},
    {"*.crw", "Camera Image File Format", "image/x-canon-crw", "0"},
    {"*.crx", "Chrome Extension Package", "application/x-chrome-package", "0"},
    {"*.cs", "C# source code", "text/x-csharp", "0"},
    {"*.cshtml", "HTML+Razor", "text/html", "0"},
    {"*.cson", "CSON", "text/x-coffeescript", "0"},
    {"*.csr", "PKCS10", "application/pkcs10", "0"},
    {"*.css", "Cascading Style Sheet", "text/css", "0"},
    {"*.csv", "Comma Separated Values", "text/csv", "0"},
    {"*.csvs", "CSV Schema", "text/csv-schema", "0"},
    {"*.cu", "Cuda", "text/x-c++src", "0"},
    {"*.cur", "Microsoft Windows Cursor", "image/x-win-bitmap", "0"},
    {"*.cwl", "Common Workflow Language", "text/x-yaml", "0"},
    {"*.cy", "Cycript", "text/javascript", "0"},
    {"*.d", "D source code", "text/x-d", "0"},
    {"*.dae", "COLLADA", "text/xml", "0"},
    {"*.dart", "Dart", "application/dart", "0"},
    {"*.dat", "MapInfo Data File (DAT)", "application/dbase", "0"},
    {"*.db", "SQLite Database File Format", "application/x-sqlite3", "0"},
    {"*.dbf", "dBASE Table File Format (DBF)", "application/x-dbf", "0"},
    {"*.dcm", "Digital Imaging and Communications in Medicine File Format",
     "application/dicom", "0"},
    {"*.dcr", "Shockwave (Director)", "application/x-director", "0"},
    {"*.dcx", "Multipage Zsoft Paintbrush Bitmap Graphics", "image/x-dcx", "0"},
    {"*.deb", "Deb", "application/vnd.debian.binary-package", "0"},
    {"*.der", "DER encoded certificate", "application/x-x509-ca-cert", "0"},
    {"*.dex", "Dalvik Executable Format", "application/x-dex", "0"},
    {"*.dgc", "DGCA", "application/x-dgc-compressed", "0"},
    {"*.diff", "Diff", "text/x-diff", "0"},
    {"*.dir", "Shockwave Movie", "application/x-director", "0"},
    {"*.dita", "DITA Topic", "application/dita+xml;format=topic", "0"},
    {"*.ditamap", "DITA Map", "application/dita+xml;format=map", "0"},
    {"*.ditaval", "DITA Conditional Processing Profile",
     "application/dita+xml;format=val", "0"},
    {"*.diz", "FILE ID.DIZ", "text/plain", "0"},
    {"*.djv", "Secure DjVU", "image/vnd.djvu", "0"},
    {"*.djvu", "DjVu", "image/vnd.djvu", "0"},
    {"*.dll", "Windows Portable Executable", "application/octet-stream", "0"},
    {"*.dls", "Downloadable Sounds Audio", "audio/dls", "0"},
    {"*.dmg", "Apple Disk Image", "application/x-apple-diskimage", "0"},
    {"*.dng", "Adobe Digital Negative", "image/x-raw-adobe", "0"},
    {"*.doc", "Microsoft Word", "application/msword", "0"},
    {"*.dockerfile", "Dockerfile", "text/x-dockerfile", "0"},
    {"*.docm", "Office Open XML Document (macro-enabled)",
     "application/vnd.ms-word.document.macroenabled.12", "0"},
    {"*.docx", "OOXML Format Family -- ISO/IEC 29500 and ECMA 376",
     "application/vnd.openxmlformats-officedocument.wordprocessingml.document",
     "0"},
    {"*.dot", "Microsoft Word Document Template (Password Protected) 97-2003",
     "application/msword;version=\"97-2003\"", "0"},
    {"*.dotm", "Office Open XML Document Template (macro-enabled)",
     "application/vnd.ms-word.template.macroenabled.12", "0"},
    {"*.dotx", "Office Open XML Document Template",
     "application/vnd.openxmlformats-officedocument.wordprocessingml.template",
     "0"},
    {"*.dpx", "Digital Moving-Picture Exchange (DPX), Version 2.0",
     "image/x-dpx", "0"},
    {"*.drc", "Ogg Packaged Dirac Video", "video/x-dirac", "0"},
    {"*.druby", "Mirah", "text/x-ruby", "0"},
    {"*.dtd", "XML Document Type Definition (DTD)", "application/xml-dtd", "0"},
    {"*.dts", "DTS Coherent Acoustics (DCA) Audio", "audio/vnd.dts", "0"},
    {"*.dv", "Digital Video", "video/dv", "0"},
    {"*.dvi", "DVI (Device Independent File Format)", "application/x-dvi", "0"},
    {"*.dwf", "AutoCAD Design Web Format 6.0",
     "application/dwf;version=\"6.0\"", "0"},
    {"*.dwfx", "AutoCAD Design Web Format", "model/vnd.dwfx+xps", "0"},
    {"*.dwg", "AutoCad Drawing", "image/vnd.dwg", "0"},
    {"*.dwl", "AutoCAD Database File Locking Information",
     "application/octet-stream", "0"},
    {"*.dx", "DEC Data Exchange File", "application/dec-dx.", "0"},
    {"*.dxb", "AutoCAD DXF simplified Binary", "image/vnd.dxb", "0"},
    {"*.dxf", "AutoCAD DXF", "image/vnd.dxf", "0"},
    {"*.dxr", "Play SID Audio 1", "audio/prs.sid;version=\"1\"", "0"},
    {"*.dylan", "Dylan", "text/x-dylan", "0"},
    {"*.e", "Eiffel source code", "text/x-eiffel", "0"},
    {"*.eb", "Easybuild", "text/x-python", "0"},
    {"*.ebnf", "EBNF", "text/x-ebnf", "0"},
    {"*.ebuild", "Gentoo Ebuild", "text/x-sh", "0"},
    {"*.ecl", "ECL", "text/x-ecl", "0"},
    {"*.eclass", "Gentoo Eclass", "text/x-sh", "0"},
    {"*.ecr", "HTML+ECR", "text/html", "0"},
    {"*.edc", "Edje Data Collection", "application/json", "0"},
    {"*.edn", "edn", "text/x-clojure", "0"},
    {"*.eex", "HTML+EEX", "text/html", "0"},
    {"*.el", "Emacs Lisp source code", "text/x-emacs-lisp", "0"},
    {"*.elc", "Emacs Lisp bytecode", "application/x-elc", "0"},
    {"*.elm", "Elm", "text/x-elm", "0"},
    {"*.em", "EmberScript", "text/x-coffeescript", "0"},
    {"*.emf", "Enhanced Metafile", "image/emf", "0"},
    {"*.eml", "Internet e-mail message format", "message/rfc822", "0"},
    {"*.enr", "EndNote Import File", "application/x-endnote-refer", "0"},
    {"*.ens", "EndNote Style File", "application/x-endnote-style", "0"},
    {"*.enz", "EndNote Connection File", "application/x-endnote-connect", "0"},
    {"*.eot", "Embedded OpenType", "application/vnd.ms-fontobject", "0"},
    {"*.epj", "Ecere Projects", "application/json", "0"},
    {"*.eps", "Encapsulated PostScript (EPS) File Format, Version 3.x",
     "application/postscript", "0"},
    {"*.epub", "Electronic Publication", "application/epub+zip", "0"},
    {"*.eq", "EQ", "text/x-csharp", "0"},
    {"*.erb", "HTML+ERB", "application/x-erb", "0"},
    {"*.erf", "Epson raw image", "image/x-raw-epson", "0"},
    {"*.erl", "Erlang source code", "text/x-erlang", "0"},
    {"*.es", "ECMAScript", "application/ecmascript", "0"},
    {"*.exe", "DOS/Windows executable (EXE)", "application/x-dosexec", "0"},
    {"*.exp", "Expect Script", "text/x-expect", "0"},
    {"*.exr", "OpenEXR 2", "image/x-exr;version=\"2\"", "0"},
    {"*.f", "Fortran source code", "text/x-fortran", "0"},
    {"*.f4a", "MPEG-4 Media File", "video/mp4", "0"},
    {"*.f4m", "F4M", "application/f4m", "0"},
    {"*.f90", "Fortran", "text/x-fortran", "0"},
    {"*.factor", "Factor", "text/x-factor", "0"},
    {"*.fb2", "FictionBook document", "application/x-fictionbook+xml", "0"},
    {"*.fdf", "Forms Data Format", "application/vnd.fdf", "0"},
    {"*.fff", "Imacon raw image", "image/x-raw-imacon", "0"},
    {"*.fh", "FreeHand image", "image/x-freehand", "0"},
    {"*.fig", "Matlab figure", "application/x-matlab-data", "0"},
    {"*.fits", "Flexible Image Transport System", "image/fits", "0"},
    {"*.flac", "FLAC (Free Lossless Audio Codec), Version 1.1.2", "audio/flac",
     "0"},
    {"*.flif", "Free Lossless Image Format (FLIF)", "image/flif", "0"},
    {"*.flv", "FLV", "video/x-flv", "0"},
    {"*.fm", "FrameMaker", "application/vnd.framemaker", "0"},
    {"*.fmz", "form*Z Project File", "application/octet-stream", "0"},
    {"*.folio", "Folio", "application/vnd.adobe.folio+zip", "0"},
    {"*.fp7", "FileMaker Pro 7", "application/x-filemaker", "0"},
    {"*.fpx", "FlashPix", "image/vnd.fpx", "0"},
    {"*.fref", "Freenet node reference", "text/plain", "0"},
    {"*.fs", "F#", "text/x-fsharp", "0"},
    {"*.fth", "Forth", "text/x-forth", "0"},
    {"*.g3", "CCITT Group 3", "image/g3fax", "0"},
    {"*.gbr", "Gerber Format", "application/vnd.gerber", "0"},
    {"*.gca", "GCA", "application/x-gca-compressed", "0"},
    {"*.geo", "GeoGebra 1.0", "application/vnd.geogebra.file;version=\"1.0\"",
     "0"},
    {"*.gf", "Grammatical Framework", "text/x-haskell", "0"},
    {"*.ggb", "Ggb", "application/vnd.geogebra.file", "0"},
    {"*.gif", "Graphics Interchange Format", "image/gif", "0"},
    {"*.gitconfig", "Git Config", "text/x-properties", "0"},
    {"*.gitignore", "Ignore List", "text/x-sh", "0"},
    {"*.glf", "Glyph", "text/x-tcl", "0"},
    {"*.gml", "Geography Markup Language", "application/gml+xml", "0"},
    {"*.gn", "GN", "text/x-python", "0"},
    {"*.gnumeric", "Gnumeric", "application/x-gnumeric", "0"},
    {"*.go", "Go source code", "text/x-go", "0"},
    {"*.grb", "General Regularly-distributed Information in Binary form",
     "application/x-grib", "0"},
    {"*.groovy", "Groovy source code", "text/x-groovy", "0"},
    {"*.gsp", "Groovy Server Pages", "application/x-jsp", "0"},
    {"*.gtar", "GNU tar Compressed File Archive (GNU Tape Archive)",
     "application/x-gtar", "0"},
    {"*.gz", "Gzip Compressed Archive", "application/gzip", "0"},
    {"*.h", "C source code header", "text/x-chdr", "0"},
    {"*.haml", "HAML source code", "text/x-haml", "0"},
    {"*.hcl", "HCL", "text/x-ruby", "0"},
    {"*.hdf", "Hierarchical Data Format File", "application/x-hdf", "0"},
    {"*.hdp", "HD Photo, Version 1.0 (Windows Media Photo)",
     "image/vnd.ms-photo", "0"},
    {"*.hdr", "Radiance HDR", "image/vnd.radiance", "0"},
    {"*.hh", "Hack", "application/x-httpd-php", "0"},
    {"*.hpgl", "Hewlett Packard Graphics Language", "application/vnd.hp-hpgl",
     "0"},
    {"*.hpp", "C++ source code header", "text/x-c++hdr", "0"},
    {"*.hqx", "BinHex", "application/mac-binhex", "0"},
    {"*.hs", "Haskell source code", "text/x-haskell", "0"},
    {"*.htm", "Hypertext Markup Language", "text/html", "0"},
    {"*.html", "HyperText Markup Language", "text/html", "0"},
    {"*.http", "HTTP", "message/http", "0"},
    {"*.hx", "Haxe source code", "text/x-haxe", "0"},
    {"*.ibooks", "Apple iBook format", "application/x-ibooks+zip", "0"},
    {"*.ico", "ICO", "image/vnd.microsoft.icon", "0"},
    {"*.ics", "Internet Calendar and Scheduling format", "text/calendar", "0"},
    {"*.idl", "Inteface Definition Language", "text/x-idl", "0"},
    {"*.idml", "IDML", "application/vnd.adobe.indesign-idml-package", "0"},
    {"*.iges", "Initial Graphics Exchange Specification (IGES) 5.x",
     "model/iges;version=\"5.x\"", "0"},
    {"*.igs", "Initial Graphics Exchange Specification Format", "model/iges",
     "0"},
    {"*.iiq", "Phase One raw image", "image/x-raw-phaseone", "0"},
    {"*.ind", "Adobe InDesign Document Generic",
     "application/octet-stream;version=\"generic\"", "0"},
    {"*.indd", "Adobe InDesign document", "application/x-adobe-indesign", "0"},
    {"*.inf", "Windows Setup File", "application/inf", "0"},
    {"*.ini", "Configuration file", "text/x-ini", "0"},
    {"*.inx", "Adobe InDesign Interchange format",
     "application/x-adobe-indesign-interchange", "0"},
    {"*.ipa", "Apple iOS IPA AppStore file", "application/x-itunes-ipa", "0"},
    {"*.ipynb", "Jupyter Notebook", "application/json", "0"},
    {"*.irclog", "IRC log", "text/mirc", "0"},
    {"*.iso", "ISO Disk Image File Format", "application/x-iso9660-image", "0"},
    {"*.itk", "Tcl script", "application/x-tcl", "0"},
    {"*.j2c", "JPEG 2000 Codestream", "image/x-jp2-codestream", "0"},
    {"*.jade", "Pug", "text/x-pug", "0"},
    {"*.jar", "Java Archive", "application/java-archive", "0"},
    {"*.java", "Java source code", "text/x-java", "0"},
    {"*.jinja", "HTML+Django", "text/x-django", "0"},
    {"*.jl", "Julia", "text/x-julia", "0"},
    {"*.jls", "JPEG-LS", "image/jls", "0"},
    {"*.jng", "JPEG Network Graphics", "image/x-jng", "0"},
    {"*.jnilib", "Java Native Library for OSX", "application/x-java-jnilib",
     "0"},
    {"*.jp2", "JPEG 2000", "image/jpx", "0"},
    {"*.jpe", "Raw JPEG Stream", "image/jpeg", "0"},
    {"*.jpf", "JPX (JPEG 2000 part 2)", "image/jpx", "0"},
    {"*.jpg", "Joint Photographic Experts Group", "image/jpeg", "0"},
    {"*.jpm", "JPEG 2000 Part 6 (JPM)", "image/jpm", "0"},
    {"*.jq", "JSONiq", "application/json", "0"},
    {"*.js", "JavaScript Source Code", "application/javascript", "0"},
    {"*.json", "JSON (JavaScript Object Notation)", "application/json", "0"},
    {"*.json-patch", "JSON Patch", "application/json-patch+json", "0"},
    {"*.json5", "JSON5", "application/json", "0"},
    {"*.jsonld", "JSON-LD", "application/ld+json", "0"},
    {"*.jsp", "Java Server Pages", "application/x-jsp", "0"},
    {"*.jsx", "JSX", "text/jsx", "0"},
    {"*.jxr", "JPEG Extended Range", "image/jxr", "0"},
    {"*.k25", "Kodak raw image", "image/x-raw-kodak", "0"},
    {"*.kicad_pcb", "KiCad Layout", "text/x-common-lisp", "0"},
    {"*.kid", "Genshi", "text/xml", "0"},
    {"*.kil", "KIllustrator File", "application/x-killustrator", "0"},
    {"*.kit", "Kit", "text/html", "0"},
    {"*.kml", "Keyhole Markup Language", "application/vnd.google-earth.kml+xml",
     "0"},
    {"*.kmz", "Keyhole Markup Language (Container)",
     "application/vnd.google-earth.kmz", "0"},
    {"*.kpr", "KPresenter File", "application/x-kpresenter", "0"},
    {"*.kra", "Krita Document Format", "application/x-krita", "0"},
    {"*.ksp", "KSpread File", "application/x-kspread", "0"},
    {"*.kt", "Kotlin", "text/x-kotlin", "0"},
    {"*.ktx", "Khronos Texture File", "image/ktx", "0"},
    {"*.kwd", "KWord File", "application/x-kword", "0"},
    {"*.l", "Lex/Flex source code", "text/x-lex", "0"},
    {"*.latex", "LaTeX Source Document", "application/x-latex", "0"},
    {"*.latte", "Latte", "text/x-smarty", "0"},
    {"*.less", "LESS source code", "text/x-less", "0"},
    {"*.lfe", "LFE", "text/x-common-lisp", "0"},
    {"*.lha", "LHA", "application/x-lzh-compressed", "0"},
    {"*.lhs", "Literate Haskell", "text/x-literate-haskell", "0"},
    {"*.lisp", "Common Lisp", "text/x-common-lisp", "0"},
    {"*.log", "application log", "text/x-log", "0"},
    {"*.lookml", "LookML", "text/x-yaml", "0"},
    {"*.ls", "LiveScript", "text/x-livescript", "0"},
    {"*.lua", "Lua source code", "text/x-lua", "0"},
    {"*.lvproj", "LabVIEW", "text/xml", "0"},
    {"*.lwp", "Lotus Word Pro", "application/vnd.lotus-wordpro", "0"},
    {"*.lz", "Lzip", "application/x-lzip", "0"},
    {"*.lzma", "LZMA Alone", "application/x-lzma", "0"},
    {"*.m", "Objective-C source code", "text/x-objcsrc", "0"},
    {"*.m3", "Modula source code", "text/x-modula", "0"},
    {"*.m3u", "MP3 Playlist File", "audio/x-mpegurl", "0"},
    {"*.maff", "MAFF", "application/x-maff", "0"},
    {"*.mak", "Makefile", "text/x-cmake", "0"},
    {"*.marko", "Marko", "text/html", "0"},
    {"*.mas", "Lotus Freelance Smartmaster Graphics",
     "application/vnd.lotus-freelance", "0"},
    {"*.mat", "MAT-File Level 5 File Format", "application/matlab-mat", "0"},
    {"*.mathematica", "Mathematica", "text/x-mathematica", "0"},
    {"*.matlab", "MATLAB", "text/x-octave", "0"},
    {"*.maxpat", "Max", "application/json", "0"},
    {"*.mbox", "MBOX Email Format", "application/mbox", "0"},
    {"*.mbx", "Mbox", "application/mbox", "0"},
    {"*.mcw", "Microsoft Word for Macintosh Document 5.0",
     "application/msword;version=\"5.0\"", "0"},
    {"*.md", "Markdown", "text/markdown", "0"},
    {"*.mdi", "Microsoft Document Imaging", "image/vnd.ms-modi", "0"},
    {"*.mef", "Mamiya raw image", "image/x-raw-mamiya", "0"},
    {"*.metal", "Metal", "text/x-c++src", "0"},
    {"*.mht", "Microsoft Web Archive", "multipart/related", "0"},
    {"*.mid", "Musical Instrument Digital Interface", "audio/midi", "0"},
    {"*.mif", "FrameMaker Interchange Format", "application/x-mif", "0"},
    {"*.mix", "MIX (PhotoDraw)", "image/vnd.mix", "0"},
    {"*.mj2", "MJ2 (Motion JPEG 2000)", "video/mj2", "0"},
    {"*.mkv", "Matroska Multimedia Container", "video/x-matroska", "0"},
    {"*.ml", "OCaml", "text/x-ocaml", "0"},
    {"*.mlp", "Dolby MLP Lossless Audio", "audio/vnd.dolby.mlp", "0"},
    {"*.mm", "Mm", "application/x-freemind", "0"},
    {"*.mmp", "MindManager", "application/vnd.mindjet.mindmanager", "0"},
    {"*.mmr", "Xerox EDMICS-MMR", "image/vnd.fujixerox.edmics-mmr", "0"},
    {"*.mng", "Multiple-image Network Graphics", "video/x-mng", "0"},
    {"*.mo", "Modelica", "text/x-modelica", "0"},
    {"*.mobi", "Mobipocket File Format", "application/x-mobipocket-ebook", "0"},
    {"*.mod", "CATIA Model 4", "application/octet-stream;version=\"4\"", "0"},
    {"*.mol", "MOL", "chemical/x-mdl-molfile", "0"},
    {"*.mos", "Leaf raw image", "image/x-raw-leaf", "0"},
    {"*.mov", "QuickTime File Format", "video/quicktime", "0"},
    {"*.mp1", "MPEG Audio Layer I", "audio/mpeg", "0"},
    {"*.mp2", "MPEG Audio Stream, Layer II", "audio/mpeg", "0"},
    {"*.mp3", "MP3 File Format", "audio/mpeg", "0"},
    {"*.mp4", "MPEG-4 File Format, Version 2", "video/mp4", "0"},
    {"*.mpeg", "MPEG Movie Clip", "video/mpeg", "0"},
    {"*.mpg", "MPEG-1", "video/mpeg", "0"},
    {"*.mpga", "MPEG-1 Audio Layer 3", "audio/mpeg", "0"},
    {"*.mpp", "Microsoft Project 2010",
     "application/vnd.ms-project;version=\"2010\"", "0"},
    {"*.mpx", "Microsoft Project Export File 4.0",
     "application/x-project;version=\"4.0\"", "0"},
    {"*.mrc", "MARC", "application/marc", "0"},
    {"*.mrw", "Minolta raw image", "image/x-raw-minolta", "0"},
    {"*.msg", "Microsoft Outlook Message", "application/vnd.ms-outlook", "0"},
    {"*.msi", "Microsoft Windows Installer", "application/x-msi", "0"},
    {"*.mso", "ActiveMime", "application/x-mso", "0"},
    {"*.mtml", "MTML", "text/html", "0"},
    {"*.muf", "MUF", "text/x-forth", "0"},
    {"*.mumps", "M", "text/x-mumps", "0"},
    {"*.mxf", "Material Exchange Format (MXF)", "application/mxf", "0"},
    {"*.mxl", "Compressed Music XML", "application/vnd.recordare.musicxml",
     "0"},
    {"*.mxmf", "Mobile eXtensible Music Format", "audio/mobile-xmf", "0"},
    {"*.n3", "Notation3", "text/n3", "0"},
    {"*.nap", "NAPLPS", "image/naplps", "0"},
    {"*.nb", "Mathematica Notebook", "application/mathematica", "0"},
    {"*.nc", "NetCDF-3 (Network Common Data Form, version 3)",
     "application/x-netcdf", "0"},
    {"*.nef", "Nikon raw image", "image/x-raw-nikon", "0"},
    {"*.nfo", "NFO", "text/x-nfo", "0"},
    {"*.nginxconf", "Nginx", "text/x-nginx-conf", "0"},
    {"*.nif", "Notation Interchange File Format", "application/vnd.music-niff",
     "0"},
    {"*.nl", "NewLisp", "text/x-common-lisp", "0"},
    {"*.nlogo", "NetLogo", "text/x-common-lisp", "0"},
    {"*.ns2", "Lotus Notes Database 2",
     "application/vnd.lotus-notes;version=\"2\"", "0"},
    {"*.ns3", "Lotus Notes Database 3",
     "application/vnd.lotus-notes;version=\"3\"", "0"},
    {"*.ns4", "Lotus Notes Database 4",
     "application/vnd.lotus-notes;version=\"4\"", "0"},
    {"*.nsf", "Notes Storage Facility", "application/vnd.lotus-notes", "0"},
    {"*.nsi", "NSIS", "text/x-nsis", "0"},
    {"*.ntf", "National Imagery Transmission Format", "application/vnd.nitf",
     "0"},
    {"*.nu", "Nu", "text/x-scheme", "0"},
    {"*.numpy", "NumPy", "text/x-python", "0"},
    {"*.nut", "Squirrel", "text/x-c++src", "0"},
    {"*.ocaml", "Ocaml source code", "text/x-ocaml", "0"},
    {"*.odb",
     "OpenDocument Database Front End Document Format (ODB), Version 1.2,  ISO "
     "26300-1:2015",
     "application/vnd.oasis.opendocument.database", "0"},
    {"*.odc", "OpenDocument v1.0: Chart document",
     "application/vnd.oasis.opendocument.chart", "0"},
    {"*.odf", "OpenDocument v1.0: Formula document",
     "application/vnd.oasis.opendocument.formula", "0"},
    {"*.odft", "OpenDocument v1.0: Formula document used as template",
     "application/vnd.oasis.opendocument.formula-template", "0"},
    {"*.odg", "OpenDocument Drawing",
     "application/vnd.oasis.opendocument.graphics", "0"},
    {"*.odi", "OpenDocument v1.0: Image document",
     "application/vnd.oasis.opendocument.image", "0"},
    {"*.odm", "OpenDocument", "application/vnd.oasis.opendocument.text-master",
     "0"},
    {"*.odp", "OpenDocument Presentation",
     "application/vnd.oasis.opendocument.presentation", "0"},
    {"*.ods", "OpenDocument Spreadsheet",
     "application/vnd.oasis.opendocument.spreadsheet", "0"},
    {"*.odt", "OpenDocument Text", "application/vnd.oasis.opendocument.text",
     "0"},
    {"*.ofx", "Open Financial Exchange 2.1.1",
     "application/x-ofx;version=\"2.1.1\"", "0"},
    {"*.oga", "Ogg Vorbis Audio", "audio/ogg", "0"},
    {"*.ogg", "Ogg Vorbis Codec Compressed Multimedia File", "audio/ogg", "0"},
    {"*.ogm", "Ogg Packaged OGM Video", "video/x-ogm", "0"},
    {"*.ogv", "Ogg Vorbis Video", "video/ogg", "0"},
    {"*.ogx", "Ogg Skeleton", "application/ogg", "0"},
    {"*.one", "Microsoft OneNote", "application/msonenote", "0"},
    {"*.opf", "DTB (Digital Talking Book), 2005", "application/x-dtbook+xml",
     "0"},
    {"*.opus", "Ogg Opus Codec Compressed WAV File", "audio/opus", "0"},
    {"*.ora", "OpenRaster", "image/openraster", "0"},
    {"*.orf", "Olympus raw image", "image/x-raw-olympus", "0"},
    {"*.otc", "OpenDocument v1.0: Chart document used as template",
     "application/vnd.oasis.opendocument.chart-template", "0"},
    {"*.otf", "OpenType Font", "application/x-font-otf", "0"},
    {"*.otg", "OpenDocument v1.0: Graphics document used as template",
     "application/vnd.oasis.opendocument.graphics-template", "0"},
    {"*.oth",
     "OpenDocument v1.0: Text document used as template for HTML documents",
     "application/vnd.oasis.opendocument.text-web", "0"},
    {"*.oti", "OpenDocument v1.0: Image document used as template",
     "application/vnd.oasis.opendocument.image-template", "0"},
    {"*.otm", "OpenDocument v1.0: Global Text document",
     "application/vnd.oasis.opendocument.text-master", "0"},
    {"*.otp", "OpenDocument v1.0: Presentation document used as template",
     "application/vnd.oasis.opendocument.presentation-template", "0"},
    {"*.ots", "OpenDocument v1.0: Spreadsheet document used as template",
     "application/vnd.oasis.opendocument.spreadsheet-template", "0"},
    {"*.ott", "OpenDocument v1.0: Text document used as template",
     "application/vnd.oasis.opendocument.text-template", "0"},
    {"*.oxps", "OpenXPS", "application/oxps", "0"},
    {"*.oz", "Oz", "text/x-oz", "0"},
    {"*.p", "Pascal source code", "text/x-pascal", "0"},
    {"*.p65", "Pagemaker Document (Generic)", "application/vnd.pagemaker", "0"},
    {"*.pab", "Personal Folder File", "application/vnd.ms-outlook", "0"},
    {"*.pack", "Package (Web)", "application/package", "0"},
    {"*.pam", "Portable Arbitrary Map", "image/x-portable-arbitrarymap", "0"},
    {"*.pas", "Pascal", "text/x-pascal", "0"},
    {"*.pbm", "Netpbm formats", "image/x-portable-bitmap", "0"},
    {"*.pcap", "TCPDump pcap packet capture", "application/vnd.tcpdump.pcap",
     "0"},
    {"*.pcapng", "pcap Next Generation Packet Capture",
     "application/vnd.tcpdump.pcap", "0"},
    {"*.pcl", "PCL", "application/vnd.hp-pcl", "0"},
    {"*.pct", "Macintosh PICT Image 2.0", "image/x-pict;version=\"2.0\"", "0"},
    {"*.pcx", "PCX", "image/vnd.zbrush.pcx", "0"},
    {"*.pdb", "Palm OS Database", "application/vnd.palm", "0"},
    {"*.pdf", "Portable Document Format", "application/pdf", "0"},
    {"*.pfm", "Printer Font Metric", "application/x-font-printer-metric", "0"},
    {"*.pfr", "PFR", "application/font-tdpfr", "0"},
    {"*.pgm", "Portable Graymap Graphic", "image/x-portable-graymap", "0"},
    {"*.pgn", "PGN", "application/x-chess-pgn", "0"},
    {"*.pgsql", "PLpgSQL", "text/x-sql", "0"},
    {"*.php", "PHP script", "text/x-php", "0"},
    {"*.phtml", "HTML+PHP", "application/x-httpd-php", "0"},
    {"*.pic", "Apple Macintosh QuickDraw/PICT Format", "image/x-pict", "0"},
    {"*.pict", "PICT", "image/x-pict", "0"},
    {"*.pkpass", "PKPass", "application/vnd.apple.pkpass", "0"},
    {"*.pl", "Perl script", "text/x-perl", "0"},
    {"*.pls", "PLSQL", "text/x-plsql", "0"},
    {"*.png", "Portable Network Graphics", "image/png", "0"},
    {"*.pnm", "Portable Any Map", "image/x-portable-anymap", "0"},
    {"*.pod", "Pod", "text/x-perl", "1"},
    {"*.por", "SPSS Portable File, ASCII encoding", "application/x-spss-por",
     "0"},
    {"*.potm", "Microsoft PowerPoint Macro-Enabled Template 2007",
     "application/vnd.ms-powerpoint.template.macroenabled.12;version=\"2007\"",
     "0"},
    {"*.potx", "Office Open XML Presentation Template",
     "application/vnd.openxmlformats-officedocument.presentationml.template",
     "0"},
    {"*.pp", "Puppet", "text/x-puppet", "0"},
    {"*.ppam", "Office Open XML Presentation Add-in (macro-enabled)",
     "application/vnd.ms-powerpoint.addin.macroenabled.12", "0"},
    {"*.ppm", "Portable Pixel Map - ASCII", "image/x-portable-pixmap", "0"},
    {"*.pps", "Microsoft Powerpoint Presentation Show 97-2003",
     "application/vnd.ms-powerpoint;version=\"97-2003\"", "0"},
    {"*.ppsm", "Office Open XML Presentation Slideshow (macro-enabled)",
     "application/vnd.ms-powerpoint.slideshow.macroenabled.12", "0"},
    {"*.ppsx", "Office Open XML Presentation Slideshow",
     "application/vnd.openxmlformats-officedocument.presentationml.slideshow",
     "0"},
    {"*.ppt", "Microsoft Powerpoint Presentation",
     "application/vnd.ms-powerpoint", "0"},
    {"*.pptm", "Office Open XML Presentation (macro-enabled)",
     "application/vnd.ms-powerpoint.presentation.macroenabled.12", "0"},
    {"*.pptx", "Office Open XML Presentation",
     "application/"
     "vnd.openxmlformats-officedocument.presentationml.presentation",
     "0"},
    {"*.prc", "PRC (Palm OS)", "application/vnd.palm", "0"},
    {"*.pro", "Prolog source code", "text/x-prolog", "0"},
    {"*.project", "CATIA Project 4", "application/octet-stream;version=\"4\"",
     "0"},
    {"*.properties", "Java Properties", "text/properties", "0"},
    {"*.proto", "Protocol Buffer", "text/x-protobuf", "0"},
    {"*.ps", "PostScript", "application/postscript", "0"},
    {"*.ps1", "PowerShell", "application/x-powershell", "0"},
    {"*.psb", "Adobe Photoshop Large Document Format",
     "image/vnd.adobe.photoshop", "0"},
    {"*.psd", "Adobe Photoshop", "image/vnd.adobe.photoshop", "0"},
    {"*.psid", "Play SID Audio 2", "audio/prs.sid;version=\"2\"", "0"},
    {"*.pst", "Outlook Personal Folders File Format",
     "application/vnd.ms-outlook-pst", "0"},
    {"*.ptx", "Pentax raw image", "image/x-raw-pentax", "0"},
    {"*.purs", "PureScript", "text/x-haskell", "0"},
    {"*.pxn", "Logitech raw image", "image/x-raw-logitech", "0"},
    {"*.py", "Python script", "text/x-python", "0"},
    {"*.pyx", "Cython", "text/x-cython", "0"},
    {"*.qcd", "Quark Xpress Data File", "application/vnd.quark.quarkxpress",
     "0"},
    {"*.qcp", "QCP Audio File Format", "audio/qcelp", "0"},
    {"*.qif", "Quicken Interchange Format", "application/qif", "0"},
    {"*.qxp", "QuarkXPress", "application/vnd.quark.quarkxpress", "0"},
    {"*.qxp report", "Quark Xpress Report File",
     "application/vnd.quark.quarkxpress", "0"},
    {"*.r", "R source code", "text/x-rsrc", "0"},
    {"*.r3d", "Red raw image", "image/x-raw-red", "0"},
    {"*.ra", "RealAudio", "audio/vnd.rn-realaudio", "0"},
    {"*.raf", "Fuji raw image", "image/x-raw-fuji", "0"},
    {"*.ram", "RealAudio Metafile", "audio/vnd.rn-realaudio", "0"},
    {"*.raml", "RAML", "text/x-yaml", "0"},
    {"*.rar", "RAR", "application/vnd.rar", "0"},
    {"*.rar ", "RAR Archive File Format Family", "application/vnd.rar", "0"},
    {"*.ras", "Sun Raster Image", "image/x-sun-raster", "0"},
    {"*.raw", "Panasonic raw image", "image/x-raw-panasonic", "0"},
    {"*.rb", "Ruby source code", "text/x-ruby", "0"},
    {"*.rdf", "RDF", "application/rdf+xml", "0"},
    {"*.re", "Reason", "text/x-rustsrc", "0"},
    {"*.reg", "Windows Registry Entries", "text/x-properties", "0"},
    {"*.rest", "reStructuredText source code", "text/x-rst", "0"},
    {"*.rexx", "Rexx source code", "text/x-rexx", "0"},
    {"*.rf64", "Broadcast WAVE 0 WAVEFORMATEXTENSIBLE Encoding",
     "audio/x-wav;version=\"0waveformatextensibleencoding\"", "0"},
    {"*.rfa", "Revit Family File", "application/octet-stream", "0"},
    {"*.rft", "Revit Family Template", "application/octet-stream", "0"},
    {"*.rg", "Rouge", "text/x-clojure", "0"},
    {"*.rgb", "Silicon Graphics RGB Bitmap", "image/x-rgb", "0"},
    {"*.rhtml", "RHTML", "application/x-erb", "0"},
    {"*.rlc", "Xerox EDMICS-RLC", "image/vnd.fujixerox.edmics-rlc", "0"},
    {"*.rm", "RealAudio, Version 10", "application/vnd.rn-realmedia", "0"},
    {"*.rmd", "RMarkdown", "text/x-gfm", "1"},
    {"*.rmi", "RIFF-based MIDI File Format", "audio/mid", "0"},
    {"*.rmp", "RealMedia Player Plug-in", "audio/x-pn-realaudio-plugin", "0"},
    {"*.roff", "Roff", "text/troff", "0"},
    {"*.rpm", "RedHat Package Manager", "application/x-rpm", "0"},
    {"*.rs", "Rust", "text/x-rustsrc", "0"},
    {"*.rss", "RSS", "application/rss+xml", "0"},
    {"*.rst", "reStructuredText", "text/x-rst", "1"},
    {"*.rte", "Revit Template", "application/octet-stream", "0"},
    {"*.rtf", "Rich Text Format File", "application/rtf", "0"},
    {"*.rv", "Real Video", "video/vnd.rn-realvideo", "0"},
    {"*.rvg", "Revit External Group", "application/octet-stream", "0"},
    {"*.rvt", "Revit Project", "application/octet-stream", "0"},
    {"*.rws", "Revit Workspace", "application/octet-stream", "0"},
    {"*.rwz", "Rawzor raw image", "image/x-raw-rawzor", "0"},
    {"*.s", "Assembler source code", "text/x-asm", "0"},
    {"*.s7m", "SAS DMDB Data Mining Database File", "application/x-sas-dmdb",
     "0"},
    {"*.sa7", "SAS Access Descriptor", "application/x-sas-access", "0"},
    {"*.sage", "Sage", "text/x-python", "0"},
    {"*.sam", "AMI Professional Document", "application/vnd.lotus-wordpro",
     "0"},
    {"*.sas", "SAS Program", "application/x-sas", "0"},
    {"*.sas7bbak", "SAS Backup", "application/x-sas-backup", "0"},
    {"*.sass", "Sass", "text/x-sass", "0"},
    {"*.sav", "SPSS System Data File Format Family (.sav)",
     "application/x-spss-sav", "0"},
    {"*.sc7", "SAS Catalog", "application/x-sas-catalog", "0"},
    {"*.scala", "Scala source code", "text/x-scala", "0"},
    {"*.sch", "Eagle", "text/xml", "0"},
    {"*.scm", "Scheme source code", "text/x-scheme", "0"},
    {"*.scores", "Xbill.scores", "text/plain", "0"},
    {"*.scss", "SCSS", "text/x-scss", "0"},
    {"*.sd7", "SAS Data Set", "application/x-sas-data", "0"},
    {"*.sda", "SDA (StarOffice)", "application/vnd.stardivision.draw", "0"},
    {"*.sdc", "SDC", "application/vnd.stardivision.calc", "0"},
    {"*.sdn", "Steel Detailing Neutral Format", "text/plain", "0"},
    {"*.sdw", "StarOffice binary formats",
     "application/vnd.stardivision.writer", "0"},
    {"*.sed", "Sed code", "text/x-sed", "0"},
    {"*.sf7", "SAS FDB Consolidation Database File", "application/x-sas-fdb",
     "0"},
    {"*.sfd", "Spline Font Database", "application/vnd.font-fontforge-sfd",
     "0"},
    {"*.sgm", "Standard Generalized Markup Language", "text/sgml", "0"},
    {"*.sgml", "SGML", "text/sgml", "0"},
    {"*.sh", "UNIX/LINUX Shell Script", "application/x-sh", "0"},
    {"*.sh-session", "ShellSession", "text/x-sh", "0"},
    {"*.shar", "Shell Archive Format", "application/x-shar", "0"},
    {"*.shtml", "Server Side Includes", "text/x-server-parsed-html", "0"},
    {"*.si7", "SAS Data Set Index", "application/x-sas-data-index", "0"},
    {"*.sid", "SID", "audio/prs.sid", "0"},
    {"*.sit", "StuffIt", "application/x-stuffit", "0"},
    {"*.sitx", "StuffIt X", "application/x-stuffitx", "0"},
    {"*.skb", "SketchUp Document", "application/octet-stream", "0"},
    {"*.skp", "SSEYO Koan File", "application/vnd.koan", "0"},
    {"*.sla", "Scribus", "application/vnd.scribus", "0"},
    {"*.sld", "AutoCAD Slide", "application/sld", "0"},
    {"*.sldm", "Microsoft PowerPoint Macro-Enabled Slide 2007",
     "application/vnd.ms-powerpoint.slide.macroenabled.12;version=\"2007\"",
     "0"},
    {"*.sldprt", "SolidWorks CAD program", "application/sldworks", "0"},
    {"*.slim", "Slim", "text/x-slim", "0"},
    {"*.sls", "SaltStack", "text/x-yaml", "0"},
    {"*.sm7", "SAS MDDB Multi-Dimensional Database File",
     "application/x-sas-mddb", "0"},
    {"*.smi", "SMIL Multimedia", "application/smil+xml", "0"},
    {"*.smk", "Smacker", "video/vnd.radgamettools.smacker", "0"},
    {"*.soy", "Closure Templates", "text/x-soy", "0"},
    {"*.sp7", "SAS Permanent Utility", "application/x-sas-putility", "0"},
    {"*.sparql", "SPARQL", "application/sparql-query", "0"},
    {"*.spec", "RPM Spec", "text/x-rpm-spec", "0"},
    {"*.spl", "Macromedia FutureSplash File", "application/x-futuresplash",
     "0"},
    {"*.spx", "Ogg Speex Audio Format", "audio/speex", "0"},
    {"*.sql", "SQL code", "text/x-sql", "0"},
    {"*.sr7", "SAS Item Store (ItemStor) File", "application/x-sas-itemstor",
     "0"},
    {"*.srl", "Sereal binary serialization format", "application/sereal", "0"},
    {"*.srt", "SRecode Template", "text/x-common-lisp", "0"},
    {"*.ss7", "SAS Stored Program (DATA Step)",
     "application/x-sas-program-data", "0"},
    {"*.ssml", "Speech Synthesis Markup Language", "application/ssml+xml", "0"},
    {"*.st", "Smalltalk source code", "text/x-stsrc", "0"},
    {"*.st7", "SAS Audit", "application/x-sas-audit", "0"},
    {"*.stw", "STW", "application/vnd.sun.xml.writer.template", "0"},
    {"*.stx", "SAS Transport File", "application/x-sas-transport", "0"},
    {"*.su7", "SAS Utility", "application/x-sas-utility", "0"},
    {"*.sublime-build", "JSON with Comments", "text/javascript", "0"},
    {"*.sv", "SystemVerilog", "text/x-systemverilog", "0"},
    {"*.sv7", "SAS Data Set View", "application/x-sas-view", "0"},
    {"*.svf", "Simple Vector Format", "image/vnd.svf", "0"},
    {"*.svg", "Scalable Vector Graphics", "image/svg+xml", "0"},
    {"*.svgz", "Scalable Vector Graphics Compressed", "image/svg+xml", "0"},
    {"*.swf", "SWF", "application/vnd.adobe.flash-movie", "0"},
    {"*.swift", "Swift", "text/x-swift", "0"},
    {"*.sxc", "OpenOffice Calc 1.0",
     "application/vnd.sun.xml.calc;version=\"1.0\"", "0"},
    {"*.sxd", "SXD", "application/vnd.sun.xml.draw", "0"},
    {"*.sxi", "SXI", "application/vnd.sun.xml.impress", "0"},
    {"*.sxw", "OpenOffice.org XML", "application/vnd.sun.xml.writer", "0"},
    {"*.sz", "Snappy Framed", "application/x-snappy-framed", "0"},
    {"*.t", "TADS", "application/x-tads", "0"},
    {"*.tap", "TAP (Tencent)", "image/vnd.tencent.tap", "0"},
    {"*.tar", "Tape Archive", "application/x-tar", "0"},
    {"*.tcl", "Tcl", "text/x-tcl", "0"},
    {"*.tcsh", "Tcsh", "text/x-sh", "0"},
    {"*.tex", "TeX Source", "text/x-tex", "0"},
    {"*.textile", "Textile", "text/x-textile", "1"},
    {"*.tfw", "ESRI World File", "text/plain", "0"},
    {"*.tfx", "Tagged Image File Format for Internet Fax (TIFF-FX)",
     "image/tiff", "0"},
    {"*.thmx", "Microsoft Office Theme", "application/vnd.ms-officetheme", "0"},
    {"*.tif", "Tagged Image File Format for Image Technology (TIFF/IT)",
     "image/tiff", "0"},
    {"*.tif ", "Digital Raster Graphic as TIFF", "image/tiff", "0"},
    {"*.tiff", "Tagged Image File Format", "image/tiff", "0"},
    {"*.toml", "TOML", "text/x-toml", "0"},
    {"*.torrent", "Torrent file", "application/x-bittorrent", "0"},
    {"*.tpl", "Smarty", "text/x-smarty", "0"},
    {"*.ts", "TypeScript", "application/typescript", "0"},
    {"*.tsv", "Tab-separated values", "text/tab-separated-values", "0"},
    {"*.tta", "True Audio 1", "audio/tta;version=\"1\"", "0"},
    {"*.ttf", "TrueType Font", "application/x-font-ttf", "0"},
    {"*.ttl", "Turtle", "text/turtle", "0"},
    {"*.twig", "Twig", "text/x-twig", "0"},
    {"*.txt", "Plain text", "text/plain", "0"},
    {"*.u3d", "Universal 3D (U3D) format family. ECMA-363, Editions 1-4",
     "model/u3d", "0"},
    {"*.uc", "UnrealScript", "text/x-java", "0"},
    {"*.ulx", "Glulx", "application/x-glulx", "0"},
    {"*.uno", "Uno", "text/x-csharp", "0"},
    {"*.upc", "Unified Parallel C", "text/x-csrc", "0"},
    {"*.url", "Internet Shortcut", "application/x-url", "0"},
    {"*.v", "Verilog source code", "text/x-verilog", "0"},
    {"*.vb", "Visual Basic", "text/x-vb", "0"},
    {"*.vbs", "VBScript source code", "text/x-vbscript", "0"},
    {"*.vcd", "Virtual CD-ROM CD Image File", "application/x-cdlink", "0"},
    {"*.vcf", "VCard", "text/vcard", "0"},
    {"*.vcs", "VCalendar format", "text/x-vcalendar", "0"},
    {"*.vdx", "Microsoft Visio XML Drawing 2003-2010",
     "application/vnd.visio;version=\"2003-2010\"", "0"},
    {"*.vhd", "VHDL source code", "text/x-vhdl", "0"},
    {"*.vhdl", "VHDL", "text/x-vhdl", "0"},
    {"*.viv", "VivoActive", "video/vnd-vivo", "0"},
    {"*.vmdk", "Virtual Disk Format", "application/x-vmdk", "0"},
    {"*.vmt", "Valve Material Type", "application/vnd.valve.source.material",
     "0"},
    {"*.volt", "Volt", "text/x-d", "0"},
    {"*.vot", "VOTable", "application/x-votable+xml", "0"},
    {"*.vpb", "Quantel VPB image", "image/vpb", "0"},
    {"*.vsd", "Microsoft Visio Diagram", "application/vnd.visio", "0"},
    {"*.vsdm", "Office Open XML Visio Drawing (macro-enabled)",
     "application/vnd.ms-visio.drawing.macroenabled.12", "0"},
    {"*.vsdx", "Visio VSDX Drawing File Format", "application/vnd.visio", "0"},
    {"*.vssm", "Office Open XML Visio Stencil (macro-enabled)",
     "application/vnd.ms-visio.stencil.macroenabled.12", "0"},
    {"*.vssx", "Office Open XML Visio Stencil (macro-free)",
     "application/vnd.ms-visio.stencil", "0"},
    {"*.vstm", "Office Open XML Visio Template (macro-enabled)",
     "application/vnd.ms-visio.template.macroenabled.12", "0"},
    {"*.vstx", "Office Open XML Visio Template (macro-free)",
     "application/vnd.ms-visio.template", "0"},
    {"*.vtf", "Valve Texture Format", "image/vnd.valve.source.texture", "0"},
    {"*.vtt", "Web Video Text Tracks Format", "text/vtt", "0"},
    {"*.vwx", "Vectorworks 2015",
     "application/vnd.vectorworks;version=\"2015\"", "0"},
    {"*.w50", "WordPerfect for MS-DOS Document 5.0",
     "application/vnd.wordperfect;version=\"5.0\"", "0"},
    {"*.w51", "WordPerfect for MS-DOS/Windows Document 5.1",
     "application/vnd.wordperfect;version=\"5.1\"", "0"},
    {"*.w52", "WordPerfect for Windows Document 5.2",
     "application/vnd.wordperfect;version=\"5.2\"", "0"},
    {"*.warc", "WARC, Web ARChive file format", "application/warc", "0"},
    {"*.wast", "WebAssembly", "text/x-common-lisp", "0"},
    {"*.wav", "Waveform Audio", "audio/x-wav", "0"},
    {"*.wbmp", "Wireless Bitmap File Format", "image/vnd.wap.wbmp", "0"},
    {"*.webapp", "Open Web App Manifest", "application/x-web-app-manifest+json",
     "0"},
    {"*.webidl", "WebIDL", "text/x-webidl", "0"},
    {"*.webm", "WebM", "video/webm", "0"},
    {"*.webp", "WebP", "image/webp", "0"},
    {"*.wisp", "wisp", "text/x-clojure", "0"},
    {"*.wk1", "Lotus 1-2-3 Worksheet 2.0",
     "application/vnd.lotus-1-2-3;version=\"2.0\"", "0"},
    {"*.wk3", "Lotus 1-2-3 Worksheet 3.0",
     "application/vnd.lotus-1-2-3;version=\"3.0\"", "0"},
    {"*.wk4", "Lotus 1-2-3 Worksheet 4-5",
     "application/vnd.lotus-1-2-3;version=\"4-5\"", "0"},
    {"*.wks", "Lotus 1-2-3", "application/vnd.lotus-1-2-3", "0"},
    {"*.wma", "WMA (Windows Media Audio) File Format", "audio/x-ms-wma", "0"},
    {"*.wmf", "Windows Metafile", "image/wmf", "0"},
    {"*.wmlc", "Compiled WML Document", "application/vnd.wap.wmlc", "0"},
    {"*.wmls", "WML Script", "text/vnd.wap.wmlscript", "0"},
    {"*.wmlsc", "Compiled WML Script", "application/vnd.wap.wmlscriptc", "0"},
    {"*.wmv", "WMV (Windows Media Video) File Format", "video/x-ms-wmv", "0"},
    {"*.woff", "Web Open Font Format", "application/font-woff", "0"},
    {"*.wp4", "WordPerfect 4.0/4.1/4.2",
     "application/vnd.wordperfect;version=\"4.0/4.1/4.2\"", "0"},
    {"*.wpd", "WordPerfect", "application/vnd.wordperfect", "0"},
    {"*.wpl", "Windows Media Playlist", "application/vnd.ms-wpl", "0"},
    {"*.wrl", "VRML", "model/vrml", "0"},
    {"*.wsz", "Winamp Skin", "interface/x-winamp-skin", "0"},
    {"*.x3d", "X3D", "model/x3d+xml", "0"},
    {"*.x3f", "Sigma raw image", "image/x-raw-sigma", "0"},
    {"*.xap", "Silverlight", "application/x-silverlight-app", "0"},
    {"*.xar", "Xar (vector graphics)", "application/vnd.xara", "0"},
    {"*.xbm", "XBM", "image/x-xbitmap", "0"},
    {"*.xc", "XC", "text/x-csrc", "0"},
    {"*.xcf", "GIMP Image File", "image/xcf", "0"},
    {"*.xdm", "X-Windows Screen Dump File X10",
     "image/x-xwindowdump;version=\"x10\"", "0"},
    {"*.xfdf", "XFDF", "application/vnd.adobe.xfdf", "0"},
    {"*.xhtml", "Extensible HyperText Markup Language (XHTML), 1.0",
     "application/xhtml+xml", "0"},
    {"*.xif", "XIFF", "image/vnd.xiff", "0"},
    {"*.xla", "Microsoft Excel Macro 4.0",
     "application/vnd.ms-excel;version=\"4.0\"", "0"},
    {"*.xlam", "Office Open XML Workbook Add-in (macro-enabled)",
     "application/vnd.ms-excel.addin.macroenabled.12", "0"},
    {"*.xlc", "Microsoft Excel Chart 3.0",
     "application/vnd.ms-excel;version=\"3.0\"", "0"},
    {"*.xlm", "Microsoft Excel Macro 3.0",
     "application/vnd.ms-excel;version=\"3.0\"", "0"},
    {"*.xls", "Microsoft Excel Spreadsheet", "application/vnd.ms-excel", "0"},
    {"*.xlsb", "Microsoft Excel 2007 Binary Spreadsheet",
     "application/vnd.ms-excel.sheet.binary.macroenabled.12", "0"},
    {"*.xlsm", "Office Open XML Workbook (macro-enabled)",
     "application/vnd.ms-excel.sheet.macroenabled.12", "0"},
    {"*.xlsx", "Office Open XML Workbook",
     "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet", "0"},
    {"*.xltm", "Office Open XML Workbook Template (macro-enabled)",
     "application/vnd.ms-excel.template.macroenabled.12", "0"},
    {"*.xltx", "Office Open XML Workbook Template",
     "application/vnd.openxmlformats-officedocument.spreadsheetml.template",
     "0"},
    {"*.xlw", "Microsoft Excel 4.0 Workbook (xls) 4W",
     "application/vnd.ms-excel;version=\"4w\"", "0"},
    {"*.xm", "Extended Module Audio File", "audio/xm", "0"},
    {"*.xmf", "XMF, eXtensible Music File Format, Version 1.0",
     "audio/mobile-xmf", "0"},
    {"*.xmind", "XMind Pro", "application/x-xmind", "0"},
    {"*.xml", "Extensible Markup Language", "application/xml", "0"},
    {"*.xmt", "MPEG-4, eXtensible MPEG-4 Textual Format (XMT)",
     "application/mpeg4-iod-xmt", "0"},
    {"*.xpi", "Cross-Platform Installer Module", "application/x-xpinstall",
     "0"},
    {"*.xpl", "XProc", "text/xml", "0"},
    {"*.xpm", "X-Windows Pixmap Image X10", "image/x-xpixmap;version=\"x10\"",
     "0"},
    {"*.xps", "Open XML Paper Specification", "application/oxps", "0"},
    {"*.xpt", "SAS Transport File Format (XPORT) Family ",
     "application/x-sas-xport", "0"},
    {"*.xq", "XQuery source code", "application/xquery", "0"},
    {"*.xquery", "XQuery", "application/xquery", "0"},
    {"*.xs", "XS", "text/x-csrc", "0"},
    {"*.xsd", "XML Schema Definition", "application/xml", "0"},
    {"*.xsl", "Extensible Stylesheet Language", "application/xml", "0"},
    {"*.xslfo", "XSL Format", "application/xslfo+xml", "0"},
    {"*.xslt", "XSL Transformations", "application/xslt+xml", "0"},
    {"*.xsp-config", "XPages", "text/xml", "0"},
    {"*.xspf", "XML Shareable Playlist Format", "application/xspf+xml", "0"},
    {"*.xwd", "X Windows Dump", "image/x-xwindowdump", "0"},
    {"*.xz", "XZ", "application/x-xz", "0"},
    {"*.y", "Yacc/Bison source code", "text/x-yacc", "0"},
    {"*.yaml", "YAML source code", "text/x-yaml", "0"},
    {"*.yml", "YAML", "text/x-yaml", "0"},
    {"*.zip", "Compressed Archive File", "application/zip", "0"},
    {NULL, NULL, NULL, NULL}};
