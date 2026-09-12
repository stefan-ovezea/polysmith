# China Network Build Guide

How to build PolySmith (and use vcpkg) from networks where GitHub is
unreliable — verified on a China network on 2026-09-12.

## The Problem

Direct GitHub downloads from mainland China fail in two characteristic ways:

| Symptom | curl code | Behavior |
|---|---|---|
| DNS resolution failure | `6 (Could not resolve hostname)` | Intermittent; same host works minutes later |
| Mid-transfer reset | `56 (Failure when receiving data from the peer)` | Transfers die at random offsets (observed 0.5 MB to 52 MB into a 54 MB file) |

Worse: **vcpkg's downloader does not resume** and treats these as
non-transient errors (no retry). `git clone` of GitHub repos is similarly
unreliable. Ports hosted elsewhere (GitLab, boost.org, msys2, python.org,
cmake.org, vcpkg.io) are generally reachable directly.

## Working Mirror Survey (tested 2026-09-12)

Test pattern — prefix the full original URL after the mirror domain:

```bash
curl -sL -o /dev/null -w "code=%{http_code} speed=%{speed_download}B/s\n" \
  -r 0-8388607 --max-time 20 \
  "https://<mirror>/https://github.com/Kitware/CMake/releases/download/v4.4.2/cmake-4.4.2-windows-x86_64.zip"
```

| Mirror | 8 MB sample speed | Verdict |
|---|---|---|
| `https://gh-proxy.com/` | **~4.9 MB/s** | ✅ Use this one |
| `https://ghfast.top/` | ~2.1 MB/s | ✅ Backup |
| `https://ghproxy.net/` | ~0.18 MB/s | ⚠️ Too slow |
| `https://gh.llkk.cc/` | dead | ❌ |
| `https://gh-proxy.net/` | redirects oddly | ❌ |

`gh-proxy.com` proxies `github.com`, `raw.githubusercontent.com`, and
`codeload.github.com` (all verified 206/200 OK). Mirrors of this kind come
and go — re-run the test when one stops working.

**Directly reachable without a mirror** (verified): `cmake.org`,
`vcpkg.io`/`vcpkg.github.io` (nasm assets), `gitlab.com` (eigen3),
`mirror.msys2.org` (msys2 packages), `boost.org`, `python.org`,
`nuget.org`, `sourceware.org` (bzip2), `api.github.com` (60 req/h
unauthenticated — don't abuse it).

## Strategy: Pre-Seed vcpkg's Downloads Cache

Because vcpkg does not resume downloads, and its `X_VCPKG_ASSET_SOURCES`
`x-azurl` provider is a **storage cache that looks up `base/<sha512>`** —
it does NOT rewrite download URLs, so it cannot route through a URL-prefix
mirror — the reliable approach is to download every GitHub-hosted file
through the mirror yourself and place it in `C:\vcpkg\downloads\` with the
**exact filename vcpkg expects**. vcpkg verifies the SHA-512 and uses the
cached file instead of downloading.

The mirror serves byte-identical files, so `curl -C -` (resume) works even
if a connection drops mid-file.

### Exact cache filenames

| Download type | Where the name comes from |
|---|---|
| vcpkg's own tools (cmake, ninja, pwsh, 7zip, git…) | `scripts/vcpkg-tools.json` → `archive` field. If absent: `<sha512[:8]>-<executable>` (e.g. `dfdcf16e-7zr.exe`) |
| Port source tarballs | The `FILENAME` argument in the portfile. `vcpkg_from_github(...)` produces `<org>-<repo>-<ref>.tar.gz` with `/` → `_-` (e.g. `boostorg-core-boost-1.92.0.tar.gz`) |
| Acquire-program downloads (LLVM, meson, nasm…) | `scripts/cmake/vcpkg_find_acquire_program(NAME).cmake` spec file → `download_filename` |
| msys2 packages | Fetched from `mirror.msys2.org` — reachable directly, no action needed |

The SHA-512 to verify against is always pinned next to the URL
(manifest/portfile/spec file). A file that fails verification is rejected
by vcpkg and re-downloaded — so a corrupt cache entry is not fatal, just
wasted.

### Recipe

1. **Build the download list.** Walk the dependency closure of the ports
   you need and extract every GitHub URL + cache name + SHA-512.
   For boost + eigen3 this produced ~172 files. PowerShell sketch
   (worked example for boost):

   ```powershell
   # walk vcpkg.json dependencies (note: feature deps can be plain strings!)
   # for each port in the closure, parse portfile.cmake:
   #   vcpkg_from_github: REPO <org>/<repo>, REF <ref> (strip quotes, substitute
   #     ${VERSION} from vcpkg.json; expat computes REF via
   #     string(REPLACE "." "_" REF "R_${VERSION}") — handle variable REFs!)
   #   vcpkg_download_distfile: URLS + FILENAME + SHA512
   # emit lines: <filename>|<url>|<sha512>  (LF endings — CRLF breaks bash read)
   ```

2. **Fetch in parallel through the mirror**, resuming and verifying:

   ```bash
   fetch_one() {
     IFS='|' read -r out url sha <<< "$1"
     [ -f "$out" ] && [ "$(sha512sum "$out" | awk '{print $1}')" = "$sha" ] \
       && { echo "SKIP-OK $out"; return 0; }
     rm -f "$out"
     for i in $(seq 1 15); do
       curl -sSL --fail --connect-timeout 20 -C - -o "$out" \
         "https://gh-proxy.com/$url" && break
       sleep 2
     done
     [ "$(sha512sum "$out" | awk '{print $1}')" = "$sha" ] \
       && echo "OK $out" || echo "BAD $out"
   }
   export -f fetch_one
   cat files.txt | xargs -P 6 -L 1 bash -c 'fetch_one "$1"' _
   ```

3. **Re-run the install.** vcpkg consumes the cache. Each uncached
   GitHub-hosted file fails loudly with its URL — repeat the recipe for
   just that file.

### Gotchas

- **CRLF list files:** a trailing `\r` lands in the hash field and silently
  poisons every comparison. Strip with `tr -d '\r'` or write LF endings.
- **Mirror flakiness:** gh-proxy.com can serve a cached 404 for a path on
  some nodes while others work. If the fetcher 404s but a manual `curl`
  of the same URL succeeds, just download manually and verify.
- **Portfile regexes:** `REF` may be quoted (`REF "v${VERSION}"`), and
  `download_sha512` in acquire-program files is often unquoted. Don't
  require quotes in your parser.
- **`vcpkg_from_git`** (git clones) cannot be pre-seeded this way. Boost
  1.92.0 ports use `vcpkg_from_github` (tarballs), so this was not needed,
  but other ports may require a git mirror instead (see below).

## Git Operations

For `git clone` of GitHub repos (e.g. PolySmith submodules):

```bash
git config --global url."https://gh-proxy.com/https://github.com/".insteadOf "https://github.com/"
```

Alternatively, copy the submodule working trees over manually (as was done
for this machine) — `git submodule status` then shows them initialized.

## What Was Pre-Seeded for This Machine

- **Tools:** cmake 4.4.2, ninja 1.13.2, powershell 7.6.4, 7zip + 7zr 26.02,
  PortableGit 2.55, vswhere 3.1.7, LLVM 21.1.8 (357 MB), meson 1.9.0,
  nasm 3.01 (vcpkg.io, direct), strawberry-perl, winflexbison 2.5.24
- **Sources:** all 160 boost-1.92.0 library tarballs + license, plus
  zlib, zstd, icu4c 78.3, openssl 3.6.4, expat 2.8.3, xz 5.8.3, cpython
  3.12.13, libffi 3.8.0, libbacktrace, BLAKE2, pkgconf, readline-win32
- Everything SHA-512-verified against vcpkg's pinned hashes.
