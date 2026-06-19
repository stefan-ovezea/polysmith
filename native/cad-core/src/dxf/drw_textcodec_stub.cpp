// Stub: replaces libdxfrw's drw_textcodec.cpp for DXF-only builds
// where iconv is not available.  All conversion is a no-op — DXF text
// is assumed to be UTF-8 or the system code page.
#include "intern/drw_textcodec.h"

DRW_TextCodec::DRW_TextCodec()  = default;
DRW_TextCodec::~DRW_TextCodec() = default;

void DRW_TextCodec::setVersion(int, bool) {}
void DRW_TextCodec::setVersion(std::string*, bool) {}
void DRW_TextCodec::setCodePage(std::string*, bool) {}

std::string DRW_TextCodec::toUtf8(std::string s)    { return s; }
std::string DRW_TextCodec::fromUtf8(std::string s)  { return s; }
std::string DRW_TextCodec::correctCodePage(const std::string& s) { return s; }
