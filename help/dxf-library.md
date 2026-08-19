# DXF / DWG Library (libdxfrw)

Polysmith integrates **libdxfrw** v0.6.3, a C++ library for reading and
writing DXF (Drawing Exchange Format) files — the industry-standard
interchange format for 2D CAD drawings.

Consumed by the DXF import/export feature: `native/cad-core/src/dxf/dxf_import.cpp`
(read side, `DxfReadInterface`) and `dxf_export.cpp` (write side,
`DxfWriteInterface`), driven by the `import_dxf` / `export_document_dxf`
IPC commands.

## Source

| Detail | Value |
|---|---|
| Repository | <https://github.com/codelibs/libdxfrw> |
| License | GPL-2.0-or-later |
| Location | `third_party/libdxfrw/` (git submodule) |
| Build | Static library `dxfrw.lib`, part of `pnpm core:rebuild` |

## Current Scope — DXF Only

The build includes the DXF reader and writer. DWG (AutoCAD's binary format)
support is **not compiled** — the following source files are excluded:

| Excluded file | Reason |
|---|---|
| `src/libdwgr.cpp` | DWG entry point |
| `src/intern/dwgutil.cpp` | DWG utilities |
| `src/intern/dwgbuffer.cpp` | DWG binary buffer parser |
| `src/intern/dwgreader.cpp` | DWG reader dispatcher |
| `src/intern/dwgreader15.cpp` | DWG R15 (2000) reader |
| `src/intern/dwgreader18.cpp` | DWG R18 (2004) reader |
| `src/intern/dwgreader21.cpp` | DWG R21 (2007) reader |
| `src/intern/dwgreader24.cpp` | DWG R24 (2010) reader |
| `src/intern/dwgreader27.cpp` | DWG R27 (2013) reader |
| `src/intern/dwgreader32.cpp` | DWG R32 (2018) reader |

Additionally, `drw_textcodec.cpp` is replaced by a **stub**
(`native/cad-core/src/dxf/drw_textcodec_stub.cpp`) that passes text through
without character-set conversion. The original depends on `iconv` for
DWG code-page handling.

## Enabling DWG Support

To add DWG read/write support, iconv must be available:

### Windows (MSVC)
```powershell
vcpkg install libiconv
```
Then add `iconv` to the `target_link_libraries` for `dxfrw` in
`native/cad-core/CMakeLists.txt`, and include the original
`src/intern/drw_textcodec.cpp` (replacing the stub) plus all excluded
DWG source files.

### Linux / macOS
```bash
sudo apt install libiconv-hook-dev   # Debian/Ubuntu
brew install libiconv                # macOS
```
CMake's `find_package(Iconv)` will locate it automatically.

### CMake Changes Required

In `native/cad-core/CMakeLists.txt`, the `dxfrw` library target needs:

1. Replace the stub with the real textcodec:
   ```cmake
   # Remove: src/dxf/drw_textcodec_stub.cpp
   # Add:    ${LIBDXFRW_DIR}/src/intern/drw_textcodec.cpp
   ```

2. Add DWG source files:
   ```cmake
   ${LIBDXFRW_DIR}/src/libdwgr.cpp
   ${LIBDXFRW_DIR}/src/intern/dwgbuffer.cpp
   ${LIBDXFRW_DIR}/src/intern/dwgutil.cpp
   ${LIBDXFRW_DIR}/src/intern/dwgreader.cpp
   ${LIBDXFRW_DIR}/src/intern/dwgreader15.cpp
   ${LIBDXFRW_DIR}/src/intern/dwgreader18.cpp
   ${LIBDXFRW_DIR}/src/intern/dwgreader21.cpp
   ${LIBDXFRW_DIR}/src/intern/dwgreader24.cpp
   ${LIBDXFRW_DIR}/src/intern/dwgreader27.cpp
   ${LIBDXFRW_DIR}/src/intern/dwgreader32.cpp
   ```

3. Link iconv:
   ```cmake
   target_link_libraries(dxfrw PUBLIC ${ICONV_LIBRARIES})
   ```

## Usage

```cpp
#include "drw_entities.h"

// Reading a DXF file
dxfRW dxf("input.dxf");
dxf.read(nullptr, true);  // true = read all entities

// Writing a DXF file
dxfRW dxf("output.dxf");
// ... add entities ...
dxf.write(nullptr, DRW::Version::AC1027, false);  // R2013 DXF
```
