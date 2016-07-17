/**
 * This file is part of Tales of Symphonia "Fix".
 *
 * Tales of Symphonia "Fix" is free software : you can redistribute it
 * and/or modify it under the terms of the GNU General Public License
 * as published by The Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * Tales of Symphonia "Fix" is distributed in the hope that it will be
 * useful,
 *
 * But WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Tales of Symphonia "Fix".
 *
 *   If not, see <http://www.gnu.org/licenses/>.
 *
**/

#include <d3d9.h>

#include "textures.h"
#include "../config.h"
#include "../timing.h"
#include "../hook.h"
#include "../log.h"

#include <atlbase.h>
#include <cstdint>

#define TSFIX_TEXTURE_DIR L"TSFix_Textures"
#define TSFIX_TEXTURE_EXT L".dds"

D3DXSaveTextureToFile_pfn               D3DXSaveTextureToFile                        = nullptr;
D3DXCreateTextureFromFileInMemoryEx_pfn D3DXCreateTextureFromFileInMemoryEx_Original = nullptr;

StretchRect_pfn                         D3D9StretchRect_Original                     = nullptr;
CreateTexture_pfn                       D3D9CreateTexture_Original                   = nullptr;
CreateRenderTarget_pfn                  D3D9CreateRenderTarget_Original              = nullptr;
CreateDepthStencilSurface_pfn           D3D9CreateDepthStencilSurface_Original       = nullptr;

SetTexture_pfn                          D3D9SetTexture_Original                      = nullptr;
SetRenderTarget_pfn                     D3D9SetRenderTarget_Original                 = nullptr;
SetDepthStencilSurface_pfn              D3D9SetDepthStencilSurface_Original          = nullptr;

tsf::RenderFix::TextureManager
  tsf::RenderFix::tex_mgr;

tsf_logger_s tex_log;

// D3DXSaveSurfaceToFile issues a StretchRect, but we don't want to log that...
bool dumping          = false;
bool __remap_textures = true;

// All of the enumerated textures in TSFix_Textures/custom/...
std::set           <uint32_t>             custom_textures;
std::unordered_map <uint32_t, uint32_t>   custom_sizes;
std::set           <uint32_t>             dumped_textures;


std::wstring
SK_D3D9_UsageToStr (DWORD dwUsage)
{
  std::wstring usage;

  if (dwUsage & D3DUSAGE_RENDERTARGET)
    usage += L"RenderTarget ";

  if (dwUsage & D3DUSAGE_DEPTHSTENCIL)
    usage += L"Depth/Stencil ";

  if (dwUsage & D3DUSAGE_DYNAMIC)
    usage += L"Dynamic";

  if (usage.empty ())
    usage = L"Don't Care";

  return usage;
}

std::wstring
SK_D3D9_FormatToStr (D3DFORMAT Format, bool include_ordinal = true)
{
  switch (Format)
  {
    case D3DFMT_UNKNOWN:
      return std::wstring (L"Unknown") + (include_ordinal ? L" (0)" :
                                                            L"");

    case D3DFMT_R8G8B8:
      return std::wstring (L"R8G8B8")   +
                (include_ordinal ? L" (20)" : L"");
    case D3DFMT_A8R8G8B8:
      return std::wstring (L"A8R8G8B8") +
                (include_ordinal ? L" (21)" : L"");
    case D3DFMT_X8R8G8B8:
      return std::wstring (L"X8R8G8B8") +
                (include_ordinal ? L" (22)" : L"");
    case D3DFMT_R5G6B5               :
      return std::wstring (L"R5G6B5")   +
                (include_ordinal ? L" (23)" : L"");
    case D3DFMT_X1R5G5B5             :
      return std::wstring (L"X1R5G5B5") +
                (include_ordinal ? L" (24)" : L"");
    case D3DFMT_A1R5G5B5             :
      return std::wstring (L"A1R5G5B5") +
                (include_ordinal ? L" (25)" : L"");
    case D3DFMT_A4R4G4B4             :
      return std::wstring (L"A4R4G4B4") +
                (include_ordinal ? L" (26)" : L"");
    case D3DFMT_R3G3B2               :
      return std::wstring (L"R3G3B2")   +
                (include_ordinal ? L" (27)" : L"");
    case D3DFMT_A8                   :
      return std::wstring (L"A8")       +
                (include_ordinal ? L" (28)" : L"");
    case D3DFMT_A8R3G3B2             :
      return std::wstring (L"A8R3G3B2") +
                (include_ordinal ? L" (29)" : L"");
    case D3DFMT_X4R4G4B4             :
      return std::wstring (L"X4R4G4B4") +
                (include_ordinal ? L" (30)" : L"");
    case D3DFMT_A2B10G10R10          :
      return std::wstring (L"A2B10G10R10") +
                (include_ordinal ? L" (31)" : L"");
    case D3DFMT_A8B8G8R8             :
      return std::wstring (L"A8B8G8R8") +
                (include_ordinal ? L" (32)" : L"");
    case D3DFMT_X8B8G8R8             :
      return std::wstring (L"X8B8G8R8") +
                (include_ordinal ? L" (33)" : L"");
    case D3DFMT_G16R16               :
      return std::wstring (L"G16R16") +
                (include_ordinal ? L" (34)" : L"");
    case D3DFMT_A2R10G10B10          :
      return std::wstring (L"A2R10G10B10") +
                (include_ordinal ? L" (35)" : L"");
    case D3DFMT_A16B16G16R16         :
      return std::wstring (L"A16B16G16R16") +
                (include_ordinal ? L" (36)" : L"");

    case D3DFMT_A8P8                 :
      return std::wstring (L"A8P8") +
                (include_ordinal ? L" (40)" : L"");
    case D3DFMT_P8                   :
      return std::wstring (L"P8") +
                (include_ordinal ? L" (41)" : L"");

    case D3DFMT_L8                   :
      return std::wstring (L"L8") +
                (include_ordinal ? L" (50)" : L"");
    case D3DFMT_A8L8                 :
      return std::wstring (L"A8L8") +
                (include_ordinal ? L" (51)" : L"");
    case D3DFMT_A4L4                 :
      return std::wstring (L"A4L4") +
                (include_ordinal ? L" (52)" : L"");

    case D3DFMT_V8U8                 :
      return std::wstring (L"V8U8") +
                (include_ordinal ? L" (60)" : L"");
    case D3DFMT_L6V5U5               :
      return std::wstring (L"L6V5U5") +
                (include_ordinal ? L" (61)" : L"");
    case D3DFMT_X8L8V8U8             :
      return std::wstring (L"X8L8V8U8") +
                (include_ordinal ? L" (62)" : L"");
    case D3DFMT_Q8W8V8U8             :
      return std::wstring (L"Q8W8V8U8") +
                (include_ordinal ? L" (63)" : L"");
    case D3DFMT_V16U16               :
      return std::wstring (L"V16U16") +
                (include_ordinal ? L" (64)" : L"");
    case D3DFMT_A2W10V10U10          :
      return std::wstring (L"A2W10V10U10") +
                (include_ordinal ? L" (67)" : L"");

    case D3DFMT_UYVY                 :
      return std::wstring (L"FourCC 'UYVY'");
    case D3DFMT_R8G8_B8G8            :
      return std::wstring (L"FourCC 'RGBG'");
    case D3DFMT_YUY2                 :
      return std::wstring (L"FourCC 'YUY2'");
    case D3DFMT_G8R8_G8B8            :
      return std::wstring (L"FourCC 'GRGB'");
    case D3DFMT_DXT1                 :
      return std::wstring (L"DXT1");
    case D3DFMT_DXT2                 :
      return std::wstring (L"DXT2");
    case D3DFMT_DXT3                 :
      return std::wstring (L"DXT3");
    case D3DFMT_DXT4                 :
      return std::wstring (L"DXT4");
    case D3DFMT_DXT5                 :
      return std::wstring (L"DXT5");

    case D3DFMT_D16_LOCKABLE         :
      return std::wstring (L"D16_LOCKABLE") +
                (include_ordinal ? L" (70)" : L"");
    case D3DFMT_D32                  :
      return std::wstring (L"D32") +
                (include_ordinal ? L" (71)" : L"");
    case D3DFMT_D15S1                :
      return std::wstring (L"D15S1") +
                (include_ordinal ? L" (73)" : L"");
    case D3DFMT_D24S8                :
      return std::wstring (L"D24S8") +
                (include_ordinal ? L" (75)" : L"");
    case D3DFMT_D24X8                :
      return std::wstring (L"D24X8") +
                (include_ordinal ? L" (77)" : L"");
    case D3DFMT_D24X4S4              :
      return std::wstring (L"D24X4S4") +
                (include_ordinal ? L" (79)" : L"");
    case D3DFMT_D16                  :
      return std::wstring (L"D16") +
                (include_ordinal ? L" (80)" : L"");

    case D3DFMT_D32F_LOCKABLE        :
      return std::wstring (L"D32F_LOCKABLE") +
                (include_ordinal ? L" (82)" : L"");
    case D3DFMT_D24FS8               :
      return std::wstring (L"D24FS8") +
                (include_ordinal ? L" (83)" : L"");

/* D3D9Ex only -- */
#if !defined(D3D_DISABLE_9EX)

    /* Z-Stencil formats valid for CPU access */
    case D3DFMT_D32_LOCKABLE         :
      return std::wstring (L"D32_LOCKABLE") +
                (include_ordinal ? L" (84)" : L"");
    case D3DFMT_S8_LOCKABLE          :
      return std::wstring (L"S8_LOCKABLE") +
                (include_ordinal ? L" (85)" : L"");

#endif // !D3D_DISABLE_9EX



    case D3DFMT_L16                  :
      return std::wstring (L"L16") +
                (include_ordinal ? L" (81)" : L"");

    case D3DFMT_VERTEXDATA           :
      return std::wstring (L"VERTEXDATA") +
                (include_ordinal ? L" (100)" : L"");
    case D3DFMT_INDEX16              :
      return std::wstring (L"INDEX16") +
                (include_ordinal ? L" (101)" : L"");
    case D3DFMT_INDEX32              :
      return std::wstring (L"INDEX32") +
                (include_ordinal ? L" (102)" : L"");

    case D3DFMT_Q16W16V16U16         :
      return std::wstring (L"Q16W16V16U16") +
                (include_ordinal ? L" (110)" : L"");

    case D3DFMT_MULTI2_ARGB8         :
      return std::wstring (L"FourCC 'MET1'");

    // Floating point surface formats

    // s10e5 formats (16-bits per channel)
    case D3DFMT_R16F                 :
      return std::wstring (L"R16F") +
                (include_ordinal ? L" (111)" : L"");
    case D3DFMT_G16R16F              :
      return std::wstring (L"G16R16F") +
                (include_ordinal ? L" (112)" : L"");
    case D3DFMT_A16B16G16R16F        :
      return std::wstring (L"A16B16G16R16F") +
               (include_ordinal ? L" (113)" : L"");

    // IEEE s23e8 formats (32-bits per channel)
    case D3DFMT_R32F                 :
      return std::wstring (L"R32F") + 
                (include_ordinal ? L" (114)" : L"");
    case D3DFMT_G32R32F              :
      return std::wstring (L"G32R32F") +
                (include_ordinal ? L" (115)" : L"");
    case D3DFMT_A32B32G32R32F        :
      return std::wstring (L"A32B32G32R32F") +
                (include_ordinal ? L" (116)" : L"");

    case D3DFMT_CxV8U8               :
      return std::wstring (L"CxV8U8") +
                (include_ordinal ? L" (117)" : L"");

/* D3D9Ex only -- */
#if !defined(D3D_DISABLE_9EX)

    // Monochrome 1 bit per pixel format
    case D3DFMT_A1                   :
      return std::wstring (L"A1") +
                (include_ordinal ? L" (118)" : L"");

    // 2.8 biased fixed point
    case D3DFMT_A2B10G10R10_XR_BIAS  :
      return std::wstring (L"A2B10G10R10_XR_BIAS") +
                (include_ordinal ? L" (119)" : L"");


    // Binary format indicating that the data has no inherent type
    case D3DFMT_BINARYBUFFER         :
      return std::wstring (L"BINARYBUFFER") +
                (include_ordinal ? L" (199)" : L"");

#endif // !D3D_DISABLE_9EX
/* -- D3D9Ex only */
  }

  return std::wstring (L"UNKNOWN?!");
}

const wchar_t*
SK_D3D9_PoolToStr (D3DPOOL pool)
{
  switch (pool)
  {
    case D3DPOOL_DEFAULT:
      return L"    Default   (0)";
    case D3DPOOL_MANAGED:
      return L"    Managed   (1)";
    case D3DPOOL_SYSTEMMEM:
      return L"System Memory (2)";
    case D3DPOOL_SCRATCH:
      return L"   Scratch    (3)";
    default:
      return L"   UNKNOWN?!     ";
  }
}

COM_DECLSPEC_NOTHROW
__declspec (noinline)
HRESULT
STDMETHODCALLTYPE
D3D9StretchRect_Detour (      IDirect3DDevice9    *This,
                              IDirect3DSurface9   *pSourceSurface,
                        const RECT                *pSourceRect,
                              IDirect3DSurface9   *pDestSurface,
                        const RECT                *pDestRect,
                              D3DTEXTUREFILTERTYPE Filter )
{
  if (tsf::RenderFix::tracer.log && (! dumping))
  {
    RECT source, dest;

    if (pSourceRect == nullptr) {
      D3DSURFACE_DESC desc;
      pSourceSurface->GetDesc (&desc);
      source.left   = 0;
      source.top    = 0;
      source.bottom = desc.Height;
      source.right  = desc.Width;
    } else
      source = *pSourceRect;

    if (pDestRect == nullptr) {
      D3DSURFACE_DESC desc;
      pDestSurface->GetDesc (&desc);
      dest.left   = 0;
      dest.top    = 0;
      dest.bottom = desc.Height;
      dest.right  = desc.Width;
    } else
      dest = *pDestRect;

    dll_log.Log ( L"[FrameTrace] StretchRect      - "
                  L"%s[%lu,%lu/%lu,%lu] ==> %s[%lu,%lu/%lu,%lu]",
                  pSourceRect != nullptr ?
                    L" " : L" *",
                  source.left, source.top, source.right, source.bottom,
                  pDestRect != nullptr ?
                    L" " : L" *",
                  dest.left,   dest.top,   dest.right,   dest.bottom );
  }

  dumping = false;

  return D3D9StretchRect_Original (This, pSourceSurface, pSourceRect,
                                         pDestSurface,   pDestRect,
                                         Filter);
}


COM_DECLSPEC_NOTHROW
HRESULT
STDMETHODCALLTYPE
D3D9CreateRenderTarget_Detour (IDirect3DDevice9     *This,
                               UINT                  Width,
                               UINT                  Height,
                               D3DFORMAT             Format,
                               D3DMULTISAMPLE_TYPE   MultiSample,
                               DWORD                 MultisampleQuality,
                               BOOL                  Lockable,
                               IDirect3DSurface9   **ppSurface,
                               HANDLE               *pSharedHandle)
{
  tex_log.Log (L"[Unexpected][!] IDirect3DDevice9::CreateRenderTarget (%lu, %lu, "
                      L"%lu, %lu, %lu, %lu, %08Xh, %08Xh)",
                 Width, Height, Format, MultiSample, MultisampleQuality,
                 Lockable, ppSurface, pSharedHandle);

  return D3D9CreateRenderTarget_Original (This, Width, Height, Format,
                                          MultiSample, MultisampleQuality,
                                          Lockable, ppSurface, pSharedHandle);
}

COM_DECLSPEC_NOTHROW
HRESULT
STDMETHODCALLTYPE
D3D9CreateDepthStencilSurface_Detour (IDirect3DDevice9     *This,
                                      UINT                  Width,
                                      UINT                  Height,
                                      D3DFORMAT             Format,
                                      D3DMULTISAMPLE_TYPE   MultiSample,
                                      DWORD                 MultisampleQuality,
                                      BOOL                  Discard,
                                      IDirect3DSurface9   **ppSurface,
                                      HANDLE               *pSharedHandle)
{
  tex_log.Log (L"[Unexpected][!] IDirect3DDevice9::CreateDepthStencilSurface (%lu, %lu, "
                      L"%lu, %lu, %lu, %lu, %08Xh, %08Xh)",
                 Width, Height, Format, MultiSample, MultisampleQuality,
                 Discard, ppSurface, pSharedHandle);

  return D3D9CreateDepthStencilSurface_Original (This, Width, Height, Format,
                                                 MultiSample, MultisampleQuality,
                                                 Discard, ppSurface, pSharedHandle);
}

//
// We will StretchRect (...) these into our textures whenever they are dirty and
//   one of the textures they are associated with are bound.
//

#define MAX_D3D9_RTS 16

struct render_target_ref_s {
  IDirect3DSurface9* surf;
  int                draws; // If this number doesn't change, we're clean
} render_targets [MAX_D3D9_RTS];

std::set           <IDirect3DSurface9*>                     dirty_surfs;
std::set           <IDirect3DSurface9*>                     msaa_surfs;       // Smurfs? :)
std::unordered_map <IDirect3DTexture9*, IDirect3DSurface9*> msaa_backing_map;
std::unordered_map <IDirect3DSurface9*, IDirect3DTexture9*> msaa_backing_map_rev;
std::unordered_map <IDirect3DSurface9*, IDirect3DSurface9*> rt_msaa;

IDirect3DTexture9*
GetMSAABackingTexture (IDirect3DSurface9* pSurf)
{
  std::unordered_map <IDirect3DSurface9*, IDirect3DTexture9*>::iterator it =
    msaa_backing_map_rev.find (pSurf);

  if (it != msaa_backing_map_rev.end ())
    return it->second;

  return nullptr;
}

int
tsf::RenderFix::TextureManager::numMSAASurfs (void)
{
  return msaa_surfs.size ();
}

int
tsf::RenderFix::TextureManager::numCustomTextures (void)
{
  return custom_count;
#if 0
  int count = 0;

  std::unordered_map <uint32_t, tsf::RenderFix::Texture *>::iterator it =
    textures.begin ();

  while (it != textures.end ()) {
    void* dontcare;
    if ( (*it).second->d3d9_tex->QueryInterface (
          IID_SKTextureD3D9, &dontcare ) == S_OK ) {
      ISKTextureD3D9* pSKTex = (ISKTextureD3D9 *)(*it).second->d3d9_tex;

      if (pSKTex->pTexOverride != nullptr) {
        count++;
      }
    }

    ++it;
  }

  return count;
#endif
}

size_t
tsf::RenderFix::TextureManager::cacheSizeCustom (void)
{
#if 0
  size_t size = 0;

  std::unordered_map <uint32_t, tsf::RenderFix::Texture *>::iterator it =
    textures.begin ();

  while (it != textures.end ()) {
    void* dontcare;
    if ( (*it).second->d3d9_tex->QueryInterface (
          IID_SKTextureD3D9, &dontcare ) == S_OK ) {
      ISKTextureD3D9* pSKTex = (ISKTextureD3D9 *)(*it).second->d3d9_tex;

      if (pSKTex->pTexOverride != nullptr) {
        size += pSKTex->override_size;
      }
    }

    ++it;
  }

  return size;
#else
  return custom_size;
#endif
}

size_t
tsf::RenderFix::TextureManager::cacheSizeBasic (void)
{
#if 0
  size_t size = 0;

  std::unordered_map <uint32_t, tsf::RenderFix::Texture *>::iterator it =
    textures.begin ();

  while (it != textures.end ()) {
    void* dontcare;
    if ( (*it).second->d3d9_tex->QueryInterface (
          IID_SKTextureD3D9, &dontcare ) == S_OK ) {
      ISKTextureD3D9* pSKTex = (ISKTextureD3D9 *)(*it).second->d3d9_tex;

      if (pSKTex->pTexOverride != nullptr) {
        size += pSKTex->tex_size;
      }
    }

    ++it;
  }

  return size;
#else
  return basic_size;
#endif
}

size_t
tsf::RenderFix::TextureManager::cacheSizeTotal (void)
{
  return cacheSizeBasic () + cacheSizeCustom ();
}



#include "../render.h"

std::set <UINT> tsf::RenderFix::active_samplers;
extern IDirect3DTexture9* pFontTex;


void
ResolveAllDirty (void)
{
  if (config.render.msaa_samples > 0 && tsf::RenderFix::draw_state.use_msaa) {
    std::set <IDirect3DSurface9*>::iterator dirty = dirty_surfs.begin ();

    while (dirty != dirty_surfs.end ()) {
      if (msaa_backing_map_rev.find (*dirty) != msaa_backing_map_rev.end ()) {

        IDirect3DTexture9* pTexture = msaa_backing_map_rev.find (*dirty)->second;
        IDirect3DSurface9* pSurf    = nullptr;

        if (SUCCEEDED (pTexture->GetSurfaceLevel (0, &pSurf)) && pSurf != nullptr) {
//          tex_log.Log (L"End-Frame MSAA Resolve (StretchRect)");
          D3D9StretchRect_Original ( tsf::RenderFix::pDevice,
                                       *dirty, nullptr,
                                       pSurf,  nullptr,
                                       D3DTEXF_NONE );

          // Render target is now clean, we've resolved it to its texture
          dirty = dirty_surfs.erase (dirty);
          pSurf->Release ();
          continue;
        }
      }
      ++dirty;
    }
  }
}

#if 0
void
FlushOrphanedRTs (void)
{
  if (config.render.msaa_samples > 0) {
    std::unordered_map <IDirect3DSurface9*, IDirect3DSurface9*>::iterator it =
      rt_msaa.begin ();

    while (it != rt_msaa.end ()) {
      if (it->first != nullptr) {
        if (it->first->Release () > 0) {
          it->first->AddRef ();
        } else {
          tex_log.Log (L"[ MSAA Mgr ] Periodic orphan flush activated for %p", it->second);
          it->second->Release ();
          it = rt_msaa.erase (it);
          continue;
        }
      }
      ++it;
    }
  }
}
#endif

COM_DECLSPEC_NOTHROW
HRESULT
STDMETHODCALLTYPE
D3D9SetTexture_Detour (
                  _In_ IDirect3DDevice9      *This,
                  _In_ DWORD                  Sampler,
                  _In_ IDirect3DBaseTexture9 *pTexture
)
{
  // Ignore anything that's not the primary render device.
  if (This != tsf::RenderFix::pDevice) {
    return D3D9SetTexture_Original (This, Sampler, pTexture);
  }

  if (tsf::RenderFix::tracer.log) {
    dll_log.Log ( L"[FrameTrace] SetTexture      - Sampler: %lu, pTexture: %ph",
                     Sampler, pTexture );
  }

  void* dontcare;
  if ( pTexture != nullptr &&
       pTexture->QueryInterface (IID_SKTextureD3D9, &dontcare) == S_OK ) {
    if (__remap_textures && ((ISKTextureD3D9 *)pTexture)->pTexOverride != nullptr)
      pTexture = ((ISKTextureD3D9 *)pTexture)->pTexOverride;
    else
      pTexture = ((ISKTextureD3D9 *)pTexture)->pTex;
  }

  DWORD address_mode = D3DTADDRESS_WRAP;

  if (pTexture == pFontTex) {
    // Clamp these textures to correct their texture coordinates
    address_mode = D3DTADDRESS_CLAMP;

    This->SetSamplerState (Sampler, D3DSAMP_ADDRESSU, address_mode);
    This->SetSamplerState (Sampler, D3DSAMP_ADDRESSV, address_mode);
    This->SetSamplerState (Sampler, D3DSAMP_ADDRESSW, address_mode);
  }

#if 0
  if (pTexture != nullptr) {
    D3DSURFACE_DESC desc;
    ((IDirect3DTexture9 *)pTexture)->GetLevelDesc (0, &desc);

    // Fix issues with some UI textures
    if (desc.Width < 64 || desc.Height < 64)
      address_mode = D3DTADDRESS_CLAMP;

    This->SetSamplerState (Sampler, D3DSAMP_ADDRESSU, address_mode);
    This->SetSamplerState (Sampler, D3DSAMP_ADDRESSV, address_mode);
    This->SetSamplerState (Sampler, D3DSAMP_ADDRESSW, address_mode);
  }
#endif

#if 0
  if (pTexture != nullptr) tsf::RenderFix::active_samplers.insert (Sampler);
  else                     tsf::RenderFix::active_samplers.erase  (Sampler);
#endif

  if ( tsf::RenderFix::draw_state.blur_proxy.first != nullptr &&
       tsf::RenderFix::draw_state.blur_proxy.first == pTexture ) {
    if (tsf::RenderFix::tracer.log)
      dll_log.Log ( L"[FrameTrace] --> Proxying %ph through %ph <--",
                              tsf::RenderFix::draw_state.blur_proxy.first,
                                tsf::RenderFix::draw_state.blur_proxy.second );

    // Intentionally run this through the HOOKED function
    return This->SetTexture (Sampler, tsf::RenderFix::draw_state.blur_proxy.second);
  }

  //
  // MSAA Blit (Before binding a texture, do MSAA resolve from its backing store)
  //
  if (config.render.msaa_samples > 0 && tsf::RenderFix::draw_state.use_msaa) {
    std::unordered_map <IDirect3DTexture9*, IDirect3DSurface9*>::iterator multisample_surf =
      msaa_backing_map.find ((IDirect3DTexture9 *)pTexture);

    if (multisample_surf != msaa_backing_map.end ()) {
      IDirect3DSurface9* pSingleSurf = nullptr;

      //
      // Only do this if the rendertarget is dirty...
      //
      if (dirty_surfs.find (multisample_surf->second) != dirty_surfs.end ()) {
        if (SUCCEEDED (((IDirect3DTexture9 *)pTexture)->GetSurfaceLevel (0, &pSingleSurf))) {
//          tex_log.Log (L"MSAA Resolve (StretchRect)");
          D3D9StretchRect_Original (This, multisample_surf->second, nullptr,
                                          pSingleSurf,              nullptr,
                                          D3DTEXF_NONE);
          pSingleSurf->Release ();
        }

        // Render target is now clean, we've resolved it to its texture
        dirty_surfs.erase (multisample_surf->second);
      }
    }
  }

  return D3D9SetTexture_Original (This, Sampler, pTexture);
}

D3DXSaveSurfaceToFile_pfn D3DXSaveSurfaceToFileW = nullptr;

IDirect3DSurface9* pOld = nullptr;

#define D3DXSaveSurfaceToFile D3DXSaveSurfaceToFileW

#define DUMP_RT

COM_DECLSPEC_NOTHROW
HRESULT
STDMETHODCALLTYPE
D3D9SetRenderTarget_Detour (
                  _In_ IDirect3DDevice9  *This,
                  _In_ DWORD              RenderTargetIndex,
                  _In_ IDirect3DSurface9 *pRenderTarget
)
{
  static int draw_counter = 0;

  // Ignore anything that's not the primary render device.
  if (This != tsf::RenderFix::pDevice) {
    return D3D9SetRenderTarget_Original (This, RenderTargetIndex, pRenderTarget);
  }

  if (tsf::RenderFix::tracer.log) {
#ifdef DUMP_RT
    if (D3DXSaveSurfaceToFileW == nullptr) {
      D3DXSaveSurfaceToFileW =
        (D3DXSaveSurfaceToFile_pfn)
          GetProcAddress ( tsf::RenderFix::d3dx9_43_dll,
                             "D3DXSaveSurfaceToFileW" );
    }

    wchar_t wszDumpName [MAX_PATH];

    if (pRenderTarget != pOld) {
      if (pOld != nullptr) {
        wsprintf (wszDumpName, L"dump\\%03d_out_%p.png", draw_counter, pOld);

        dll_log.Log ( L"[FrameTrace] >>> Dumped: Output RT to %s >>>", wszDumpName );

        dumping = true;
        //D3DXSaveSurfaceToFile (wszDumpName, D3DXIFF_PNG, pOld, nullptr, nullptr);
      }
    }
#endif

    dll_log.Log ( L"[FrameTrace] SetRenderTarget - RenderTargetIndex: %lu, pRenderTarget: %ph",
                    RenderTargetIndex, pRenderTarget );

#ifdef DUMP_RT
    if (pRenderTarget != pOld) {
      pOld = pRenderTarget;

      wsprintf (wszDumpName, L"dump\\%03d_in_%p.png", ++draw_counter, pRenderTarget);

      dll_log.Log ( L"[FrameTrace] <<< Dumped: Input RT to  %s  <<<", wszDumpName );

      dumping = true;
      //D3DXSaveSurfaceToFile (wszDumpName, D3DXIFF_PNG, pRenderTarget, nullptr, nullptr);
    }
#endif
  }

  //
  // MSAA Override
  //
  if (config.render.msaa_samples > 0 && tsf::RenderFix::draw_state.use_msaa) {
    render_target_ref_s rt_ref =
      render_targets [RenderTargetIndex];

    if (rt_ref.surf != nullptr && rt_ref.draws < tsf::RenderFix::draw_state.draws)
      dirty_surfs.insert (rt_ref.surf);

    render_targets [RenderTargetIndex].surf  = nullptr;
    render_targets [RenderTargetIndex].draws = tsf::RenderFix::draw_state.draws;

    if (rt_msaa.find (pRenderTarget) != rt_msaa.end ()) {
//      tex_log.Log (L"MSAA RenderTarget Override");
      IDirect3DSurface9* pSurf = rt_msaa [pRenderTarget];

      render_targets [RenderTargetIndex] =
        render_target_ref_s {
          pSurf,
          tsf::RenderFix::draw_state.draws
      };

      // On the off chance that the optimization work above to detect draw calls
      //   misses something...
      if (! config.render.conservative_msaa)
        dirty_surfs.insert (pSurf);

      return D3D9SetRenderTarget_Original (This, RenderTargetIndex, pSurf);
    }
  }

  return D3D9SetRenderTarget_Original (This, RenderTargetIndex, pRenderTarget);
}

COM_DECLSPEC_NOTHROW
HRESULT
STDMETHODCALLTYPE
D3D9SetDepthStencilSurface_Detour (
                  _In_ IDirect3DDevice9  *This,
                  _In_ IDirect3DSurface9 *pNewZStencil
)
{
  // Ignore anything that's not the primary render device.
  if (This != tsf::RenderFix::pDevice) {
    return D3D9SetDepthStencilSurface_Original (This, pNewZStencil);
  }

  if (tsf::RenderFix::tracer.log) {
    dll_log.Log ( L"[FrameTrace] SetDepthStencilSurface   (%ph)",
                    pNewZStencil );
  }


  //
  // MSAA Depth/Stencil Override
  //
  if (config.render.msaa_samples > 0 && tsf::RenderFix::draw_state.use_msaa) {
    if (rt_msaa.find (pNewZStencil) != rt_msaa.end ()) {
      return D3D9SetDepthStencilSurface_Original ( This,
                                                     rt_msaa [pNewZStencil] );
    }
  }

  return D3D9SetDepthStencilSurface_Original (This, pNewZStencil);
}

IDirect3DTexture9* last_tex = nullptr;

COM_DECLSPEC_NOTHROW
HRESULT
STDMETHODCALLTYPE
D3D9CreateTexture_Detour (IDirect3DDevice9   *This,
                          UINT                Width,
                          UINT                Height,
                          UINT                Levels,
                          DWORD               Usage,
                          D3DFORMAT           Format,
                          D3DPOOL             Pool,
                          IDirect3DTexture9 **ppTexture,
                          HANDLE             *pSharedHandle)
{
  // Ignore anything that's not the primary render device.
  if (This != tsf::RenderFix::pDevice) {
    return D3D9CreateTexture_Original ( This, Width, Height,
                                          Levels, Usage, Format,
                                            Pool, ppTexture, pSharedHandle );
  }

  //
  // Game is seriously screwed up: PLEASE stop creating this target every frame!
  //
  if (Width == 16 && Height == 1 && (Usage & D3DUSAGE_RENDERTARGET))
    return E_FAIL;

  if (config.textures.log) {
    tex_log.Log ( L"[Load Trace] >> Creating Texture: "
                  L"(%d x %d), Format: %s, Usage: [%s], Pool: %s",
                    Width, Height,
                      SK_D3D9_FormatToStr (Format).c_str (),
                      SK_D3D9_UsageToStr  (Usage).c_str (),
                      SK_D3D9_PoolToStr   (Pool) );
  }

  bool create_msaa_surf = config.render.msaa_samples > 0 &&
                          tsf::RenderFix::draw_state.has_msaa;

  // Resize the primary framebuffer
  if (Width == 1280 && Height == 720) {
    if (((Usage & D3DUSAGE_RENDERTARGET) && (Format == D3DFMT_A8R8G8B8 ||
                                             Format == D3DFMT_R32F))   ||
                                             Format == D3DFMT_D24S8) {
      Width  = tsf::RenderFix::width;
      Height = tsf::RenderFix::height;

      // Don't waste bandwidth on the HDR post-process
      if (Format == D3DFMT_R32F)
        create_msaa_surf = false;
    } else {
      // Not a rendertarget!
      create_msaa_surf = false;
    }
  }

  else if (Width == 512 && Height == 256 && (Usage & D3DUSAGE_RENDERTARGET)) {
    Width  = tsf::RenderFix::width  * config.render.postproc_ratio;
    Height = tsf::RenderFix::height * config.render.postproc_ratio;

    // No geometry is rendered in this surface, it's a weird ghetto motion blur pass
    create_msaa_surf = false;
  }

  else if (Width == 256 && Height == 256 && (Usage & D3DUSAGE_RENDERTARGET)) {
    Width  = Width;//  * config.render.postproc_ratio;
    Height = Height;// * config.render.postproc_ratio;

    // I don't even know what this surface is for, much less why you'd
    //   want to multisample it.
    create_msaa_surf = false;
  }

  else {
    create_msaa_surf = false;
  }

  HRESULT result =
    D3D9CreateTexture_Original ( This, Width, Height,
                                   Levels, Usage, Format,
                                     Pool, ppTexture, pSharedHandle );

  if (create_msaa_surf) {
    IDirect3DSurface9* pSurf;

    HRESULT hr = E_FAIL;

    if (! (Usage & D3DUSAGE_DEPTHSTENCIL)) {
      hr = 
        D3D9CreateRenderTarget_Original ( This,
                                            Width, Height, Format,
                                              (D3DMULTISAMPLE_TYPE)config.render.msaa_samples,
                                                                   config.render.msaa_quality,
                                                FALSE,
                                                  &pSurf, nullptr);
    } else {
      hr = 
        D3D9CreateDepthStencilSurface_Original ( This,
                                                   Width, Height, Format,
                                                     (D3DMULTISAMPLE_TYPE)config.render.msaa_samples,
                                                                          config.render.msaa_quality,
                                                       FALSE,
                                                         &pSurf, nullptr);
    }

    if (SUCCEEDED (hr)) {
      msaa_surfs.insert       (pSurf);
      msaa_backing_map.insert (
        std::pair <IDirect3DTexture9*, IDirect3DSurface9*> (
          *ppTexture, pSurf
        )
      );
      msaa_backing_map_rev.insert (
        std::pair <IDirect3DSurface9*,IDirect3DTexture9*> (
          pSurf, *ppTexture
        )
      );

      IDirect3DSurface9* pFakeSurf = nullptr;
      (*ppTexture)->GetSurfaceLevel (0, &pFakeSurf);
      rt_msaa.insert (
        std::pair <IDirect3DSurface9*, IDirect3DSurface9*> (
          pFakeSurf, pSurf
        )
      );
    } else {
      tex_log.Log ( L"[ MSAA Mgr ] >> ERROR: Unable to Create MSAA Surface for Render Target: "
                    L"(%d x %d), Format: %s, Usage: [%s], Pool: %s",
                        Width, Height,
                          SK_D3D9_FormatToStr (Format).c_str (),
                          SK_D3D9_UsageToStr  (Usage).c_str (),
                          SK_D3D9_PoolToStr   (Pool) );
    }
  }

  if (Pool == D3DPOOL_DEFAULT)
    last_tex = *ppTexture;

  return result;
}

static uint32_t crc32_tab[] = { 
   0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f, 
   0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988, 
   0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2, 
   0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7, 
   0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9, 
   0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172, 
   0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c, 
   0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59, 
   0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423, 
   0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924, 
   0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106, 
   0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433, 
   0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d, 
   0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e, 
   0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950, 
   0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65, 
   0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7, 
   0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0, 
   0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa, 
   0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f, 
   0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81, 
   0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a, 
   0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84, 
   0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1, 
   0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb, 
   0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc, 
   0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e, 
   0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b, 
   0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55, 
   0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236, 
   0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28, 
   0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d, 
   0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f, 
   0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38, 
   0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242, 
   0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777, 
   0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69, 
   0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2, 
   0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc, 
   0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9, 
   0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693, 
   0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94, 
   0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d 
 };

uint32_t
crc32 (uint32_t crc, const void *buf, size_t size)
{
  const uint8_t *p;

  p = (uint8_t *)buf;
  crc = crc ^ ~0U;

  while (size--)
    crc = crc32_tab[(crc ^ *p++) & 0xFF] ^ (crc >> 8);

  return crc ^ ~0U;
}

typedef HRESULT (WINAPI *D3DXGetImageInfoFromFileInMemory_pfn)
(
  _In_ LPCVOID        pSrcData,
  _In_ UINT           SrcDataSize,
  _In_ D3DXIMAGE_INFO *pSrcInfo
);

D3DXGetImageInfoFromFileInMemory_pfn
  D3DXGetImageInfoFromFileInMemory = nullptr;

typedef HRESULT (WINAPI *D3DXGetImageInfoFromFile_pfn)
(
  _In_ LPCWSTR         pSrcFile,
  _In_ D3DXIMAGE_INFO *pSrcInfo
);

D3DXGetImageInfoFromFile_pfn
  D3DXGetImageInfoFromFile = nullptr;


typedef HRESULT (WINAPI *D3DXCreateTextureFromFile_pfn)
(
  _In_  LPDIRECT3DDEVICE9   pDevice,
  _In_  LPCWSTR             pSrcFile,
  _Out_ LPDIRECT3DTEXTURE9 *ppTexture
);

D3DXCreateTextureFromFile_pfn
  D3DXCreateTextureFromFile = nullptr;

typedef HRESULT (WINAPI *D3DXCreateTextureFromFileEx_pfn)
(
  _In_    LPDIRECT3DDEVICE9  pDevice,
  _In_    LPCWSTR            pSrcFile,
  _In_    UINT               Width,
  _In_    UINT               Height,
  _In_    UINT               MipLevels,
  _In_    DWORD              Usage,
  _In_    D3DFORMAT          Format,
  _In_    D3DPOOL            Pool,
  _In_    DWORD              Filter,
  _In_    DWORD              MipFilter,
  _In_    D3DCOLOR           ColorKey,
  _Inout_ D3DXIMAGE_INFO     *pSrcInfo,
  _Out_   PALETTEENTRY       *pPalette,
  _Out_   LPDIRECT3DTEXTURE9 *ppTexture
);

D3DXCreateTextureFromFileEx_pfn
  D3DXCreateTextureFromFileEx = nullptr;

#define FONT_CRC32 0xef2d9b55

#define D3DX_FILTER_NONE             0x00000001
#define D3DX_FILTER_POINT            0x00000002
#define D3DX_FILTER_LINEAR           0x00000003
#define D3DX_FILTER_TRIANGLE         0x00000004
#define D3DX_FILTER_BOX              0x00000005
#define D3DX_FILTER_MIRROR_U         0x00010000
#define D3DX_FILTER_MIRROR_V         0x00020000
#define D3DX_FILTER_MIRROR_W         0x00040000
#define D3DX_FILTER_MIRROR           0x00070000
#define D3DX_FILTER_DITHER           0x00080000
#define D3DX_FILTER_DITHER_DIFFUSION 0x00100000
#define D3DX_FILTER_SRGB_IN          0x00200000
#define D3DX_FILTER_SRGB_OUT         0x00400000
#define D3DX_FILTER_SRGB             0x00600000


#define D3DX_SKIP_DDS_MIP_LEVELS_MASK 0x1f
#define D3DX_SKIP_DDS_MIP_LEVELS_SHIFT 26
#define D3DX_SKIP_DDS_MIP_LEVELS(l, f) ((((l) & D3DX_SKIP_DDS_MIP_LEVELS_MASK) \
<< D3DX_SKIP_DDS_MIP_LEVELS_SHIFT) | ((f) == D3DX_DEFAULT ? D3DX_FILTER_BOX : (f)))

__declspec (thread) bool injecting = false;

typedef struct tsf_tex_load_s {
  enum {
    Stream,    // This load will be streamed
    Immediate, // This load must finish immediately   (pSrc is unused)
    Resample   // Change image properties             (pData is supplied)
  } type;

  LPDIRECT3DDEVICE9   pDevice;

  // Resample only
  LPVOID              pSrcData;
  UINT                SrcDataSize;

  uint32_t            checksum;
  uint32_t            size;

  // Stream / Immediate
  wchar_t             wszFilename [MAX_PATH];

  LPDIRECT3DTEXTURE9  pDest = nullptr;
  LPDIRECT3DTEXTURE9  pSrc  = nullptr;

  LARGE_INTEGER       start = { 0LL };
  LARGE_INTEGER       end   = { 0LL };
  LARGE_INTEGER       freq  = { 0LL };
};

#include <queue>
std::queue <tsf_tex_load_s *> textures_to_stream;
std::set   <uint32_t>         textures_in_flight;
std::queue <tsf_tex_load_s *> textures_to_resample;

std::queue <tsf_tex_load_s *> finished_loads;

CRITICAL_SECTION              cs_tex_stream;
CRITICAL_SECTION              cs_tex_resample;
CRITICAL_SECTION              cs_tex_inject;

#define D3DX_DEFAULT            ((UINT) -1)
#define D3DX_DEFAULT_NONPOW2    ((UINT) -2)
#define D3DX_DEFAULT_FLOAT      FLT_MAX
#define D3DX_FROM_FILE          ((UINT) -3)
#define D3DFMT_FROM_FILE        ((D3DFORMAT) -3)

#include <set>
std::set <DWORD> inject_tids;

int      streaming       = 0;
uint32_t streaming_bytes = 0UL;

int resampling = 0;

bool
pending_loads (void)
{
  bool ret = false;

  EnterCriticalSection (&cs_tex_inject);
  ret = (! finished_loads.empty ());
  LeaveCriticalSection (&cs_tex_inject);

  return ret;
}

void
start_load (void)
{
  EnterCriticalSection (&cs_tex_inject);

  inject_tids.insert (GetCurrentThreadId ());

  LeaveCriticalSection (&cs_tex_inject);
}

void
cancel_load (void)
{
  EnterCriticalSection (&cs_tex_inject);

  inject_tids.erase (GetCurrentThreadId ());

  LeaveCriticalSection (&cs_tex_inject);
}

void
add_finished_load (tsf_tex_load_s* load)
{
  EnterCriticalSection (&cs_tex_inject);

  // Now, queue up a copy from this texture to the original
   finished_loads.push (load);

  inject_tids.erase (GetCurrentThreadId ());

  LeaveCriticalSection (&cs_tex_inject);
}

bool
pending_streams (void)
{
  bool ret = false;

  //EnterCriticalSection (&cs_tex_stream);

  if (streaming)
    ret = true;
  //if (textures_to_stream.size ())
    //ret = true;

  //LeaveCriticalSection (&cs_tex_stream);

  return ret;
}

bool
is_streaming (uint32_t checksum)
{
  bool ret = false;

  EnterCriticalSection (&cs_tex_stream);

  if (textures_in_flight.count (checksum))
    ret = true;

  LeaveCriticalSection (&cs_tex_stream);

  return ret;

}

void
finished_streaming (uint32_t checksum)
{
  EnterCriticalSection (&cs_tex_stream);

  if (textures_in_flight.count (checksum))
    textures_in_flight.erase (checksum);

  LeaveCriticalSection (&cs_tex_stream);
}

tsf_tex_load_s*
start_next_stream (void)
{
  EnterCriticalSection (&cs_tex_stream);

  tsf_tex_load_s*
    stream =
      textures_to_stream.front ();

  textures_in_flight.insert (stream->checksum);

  ++streaming;
  streaming_bytes += stream->SrcDataSize;

  QueryPerformanceFrequency        (&stream->freq);
  QueryPerformanceCounter_Original (&stream->start);

  textures_to_stream.pop ();

  LeaveCriticalSection (&cs_tex_stream);

  return stream;
}

void
finish_stream (tsf_tex_load_s* stream)
{
  EnterCriticalSection (&cs_tex_stream);

  streaming_bytes -= stream->SrcDataSize;
  --streaming;

  LeaveCriticalSection (&cs_tex_stream);
}

tsf_tex_load_s*
get_next_load (void)
{
  EnterCriticalSection (&cs_tex_inject);

  tsf_tex_load_s*
    load =
      finished_loads.front ();

  finished_loads.pop ();

  LeaveCriticalSection (&cs_tex_inject);

  return load;
}

DWORD
WINAPI
TextureStreamThread (LPVOID user)
{
  tsf_tex_load_s* pStream;

  pStream = start_next_stream ();
  {
    start_load ();
    {
      D3DXIMAGE_INFO img_info;

      HRESULT hr =
        D3DXCreateTextureFromFileEx (
          pStream->pDevice,
            pStream->wszFilename,
              D3DX_DEFAULT, D3DX_DEFAULT, D3DX_DEFAULT,
                0, D3DFMT_FROM_FILE,
                  D3DPOOL_DEFAULT,
                    D3DX_DEFAULT, D3DX_DEFAULT,
                      0,
                        &img_info, nullptr,
                          &pStream->pSrc );

      if (SUCCEEDED (hr)) {
        add_finished_load (pStream);
      } else {
        cancel_load ();
      }
    }
  }
  finish_stream (pStream);

  return 0;
}

DWORD
WINAPI
TextureResampleThread (LPVOID user)
{
  EnterCriticalSection (&cs_tex_resample);

  tsf_tex_load_s* load_params =
    textures_to_resample.front ();

  ++resampling;

  textures_to_resample.pop ();

  LeaveCriticalSection (&cs_tex_resample);

  QueryPerformanceFrequency        (&load_params->freq);
  QueryPerformanceCounter_Original (&load_params->start);

  HRESULT hr =
    D3DXCreateTextureFromFileInMemoryEx_Original (
      load_params->pDevice,
          load_params->pSrcData, load_params->SrcDataSize,
            D3DX_DEFAULT, D3DX_DEFAULT, 0,//D3DX_DEFAULT,
              0, D3DFMT_FROM_FILE,
                D3DPOOL_DEFAULT,
                  D3DX_FILTER_TRIANGLE | D3DX_FILTER_DITHER, D3DX_FILTER_TRIANGLE | D3DX_FILTER_DITHER,
                    0,
                      nullptr, nullptr,
                        &load_params->pSrc );

  if (SUCCEEDED (hr)) {
    EnterCriticalSection (&cs_tex_inject);

    // Now, queue up a copy from this texture to the original
    finished_loads.push (load_params);

    LeaveCriticalSection (&cs_tex_inject);
  }

  delete [] load_params->pSrcData;

  --resampling;

  return 0;
}

#include <mmsystem.h>

void
TSFix_LoadQueuedTextures (void)
{
  extern std::string mod_text;
  mod_text = "";

  static DWORD         dwTime = timeGetTime ();
  static unsigned char spin    = 193;

  if (dwTime < timeGetTime ()-100UL)
    spin++;

  if (spin > 199)
    spin = 193;

  if (resampling) {
    mod_text += spin;

    char szFormatted [64];
    sprintf (szFormatted, "  Resampling: %lu textures", resampling);

    mod_text += szFormatted;

    if (textures_to_resample.size ()) {
      sprintf (szFormatted, " (%lu queued)", textures_to_resample.size ());
      mod_text += szFormatted;
    }

    mod_text += "\n\n";
  }

  if (streaming) {
    mod_text += spin;

    char szFormatted [64];
    sprintf (szFormatted, "  Streaming: %lu texture", streaming);

    mod_text += szFormatted;

    if (streaming > 1)
      mod_text += 's';

    sprintf (szFormatted, " [%7.2f MiB]", (double)streaming_bytes / (1024.0f * 1024.0f));
    mod_text += szFormatted;

    if (textures_to_stream.size ()) {
      sprintf (szFormatted, " (%lu outstanding)", textures_to_stream.size ());
      mod_text += szFormatted;
    }

    mod_text += "\n\n";
  }

  if (config.textures.cache) {
    mod_text += "Texture Cache\n";
    mod_text += "-------------\n";
    mod_text += tsf::RenderFix::tex_mgr.osdStats ();
  }

  LARGE_INTEGER start, now;

  int loads = 0;

  while (pending_loads ()) {
    tsf_tex_load_s* load =
      get_next_load ();

    QueryPerformanceCounter_Original (&load->end);

    if (true) {
      tex_log.Log ( L"[%s] Finished %s texture %08x (%5.2f MiB in %9.4f ms)",
                      (load->type == tsf_tex_load_s::Stream) ? L"Custom Tex" :
                                                               L" Resample ",
                      (load->type == tsf_tex_load_s::Stream) ? L"streaming" :
                                                               L"filtering",
                        load->checksum,
                          (double)load->SrcDataSize / (1024.0f * 1024.0f),
                            1000.0f * (double)(load->end.QuadPart - load->start.QuadPart) /
                                      (double)load->freq.QuadPart );
    }

    tsf::RenderFix::Texture* pTex =
      tsf::RenderFix::tex_mgr.getTexture (load->checksum);

    if (pTex != nullptr) {
      pTex->load_time = 1000.0f * (double)(load->end.QuadPart - load->start.QuadPart) /
                                      (double)load->freq.QuadPart;
    }

    ISKTextureD3D9* pSKTex =
      (ISKTextureD3D9 *)load->pDest;

    if (pSKTex->refs == 0 && load->pSrc != nullptr) {
      tex_log.Log (L"[ Tex. Mgr ] >> Original texture no longer referenced, discarding new one!");
      load->pSrc->Release ();
    } else {
      int refs = 1;

      // Share the same number of references as the original texture
      //for (int i = 0; i < pSKTex->refs - 1; i++) {
        //load->pSrc->AddRef ();
      //}

      //if (refs != pSKTex->refs) {
        //if (refs == pSKTex->refs + 1)
          //load->pSrc->Release ();
        //else
          //tex_log.Log ( L"[ Tex Mgr ] # Override texture has different ref. count (%lu) than original (%lu)?!",
                          //refs, pSKTex->refs );
      //}

      pSKTex->pTexOverride  = load->pSrc;
      pSKTex->override_size = load->SrcDataSize;

      finished_streaming (load->checksum);

      tsf::RenderFix::tex_mgr.addCustom (load->SrcDataSize);
    }

    tsf::RenderFix::tex_mgr.updateOSD ();

    ++loads;

    delete load;

    Sleep (1);
  }
}

COM_DECLSPEC_NOTHROW
HRESULT
STDMETHODCALLTYPE
D3DXCreateTextureFromFileInMemoryEx_Detour (
  _In_    LPDIRECT3DDEVICE9  pDevice,
  _In_    LPCVOID            pSrcData,
  _In_    UINT               SrcDataSize,
  _In_    UINT               Width,
  _In_    UINT               Height,
  _In_    UINT               MipLevels,
  _In_    DWORD              Usage,
  _In_    D3DFORMAT          Format,
  _In_    D3DPOOL            Pool,
  _In_    DWORD              Filter,
  _In_    DWORD              MipFilter,
  _In_    D3DCOLOR           ColorKey,
  _Inout_ D3DXIMAGE_INFO     *pSrcInfo,
  _Out_   PALETTEENTRY       *pPalette,
  _Out_   LPDIRECT3DTEXTURE9 *ppTexture
)
{
  bool inject_thread = false;

  EnterCriticalSection (&cs_tex_inject);

  if (inject_tids.count (GetCurrentThreadId ())) {
    inject_thread = true;
    //resample      = false;
  }

  LeaveCriticalSection (&cs_tex_inject);

  // Injection would recurse slightly and cause impossible to diagnose reference counting problems
  //   with texture caching if we did not check for this!
  if (inject_thread) {
    return D3DXCreateTextureFromFileInMemoryEx_Original (pDevice, pSrcData, SrcDataSize, Width, Height, MipLevels, Usage, Format, Pool, Filter, MipFilter, ColorKey, pSrcInfo, pPalette, ppTexture);
  }

  // Performance statistics for caching system
  LARGE_INTEGER start, end;

  static LARGE_INTEGER freq = { 0LL };

  if (freq.QuadPart == 0LL)
    QueryPerformanceFrequency (&freq);

  QueryPerformanceCounter_Original (&start);

  uint32_t checksum =
    crc32 (0, pSrcData, SrcDataSize);

  // Don't dump or cache these
  if (Usage == D3DUSAGE_DYNAMIC || Usage == D3DUSAGE_RENDERTARGET)
    checksum = 0x00;

  if (config.textures.cache && checksum != 0x00) {
    tsf::RenderFix::Texture* pTex =
      tsf::RenderFix::tex_mgr.getTexture (checksum);

    if (pTex != nullptr) {
      tsf::RenderFix::tex_mgr.refTexture (pTex);

      *ppTexture = pTex->d3d9_tex;

      return S_OK;
    }
  }

  bool resample = false;

  // Necessary to make D3DX texture write functions work
  if ( Pool == D3DPOOL_DEFAULT && config.textures.dump &&
        (! dumped_textures.count (checksum))           //&&
        /*(! custom_textures.count (checksum))*/ )
    Usage = D3DUSAGE_DYNAMIC;

#if 0
  //
  // Generating full mipmaps is MUCH faster if we don't re-compress everything
  //
  if ((Pool == D3DPOOL_DEFAULT && Usage != D3DUSAGE_DYNAMIC) &&
      (config.textures.uncompressed || config.textures.full_mipmaps)) {
    //if (Format == D3DFMT_DXT1 ||
        //Format == D3DFMT_DXT3 ||
        //Format == D3DFMT_DXT5) {
      Format = D3DFMT_A8R8G8B8;

      //MipLevels = D3DX_DEFAULT;
    //}
  }
#endif

  if ((Pool == D3DPOOL_DEFAULT && Usage != D3DUSAGE_DYNAMIC) &&
      (! config.textures.dump) && config.textures.optimize_ui) {
    // Generating mipmaps adds a lot of overhead, don't do it for
    //   UI textures and that will speed things up.
    if (Width < 128 || Height < 128)
      MipLevels = 1;
  }

  // Generate complete mipmap chains for best image quality
  //  (will increase load-time on uncached textures)
  if ((Pool == D3DPOOL_DEFAULT) && config.textures.full_mipmaps) {
    resample = true;
  }

  HRESULT         hr      = E_FAIL;
  tsf_tex_load_s* load_op = nullptr;

  wchar_t wszInjectFileName [MAX_PATH] = { L'\0' };

  //
  // Custom font
  //
  if (   checksum == FONT_CRC32 &&
      (! inject_thread) &&
         GetFileAttributes (L"font.dds") != INVALID_FILE_ATTRIBUTES ) {
    tex_log.LogEx (true, L"[   Font   ] Loading user-defined font... ");

    load_op           = new tsf_tex_load_s;
    load_op->pDevice  = pDevice;
    load_op->checksum = checksum;

    if (! is_streaming (load_op->checksum))
      load_op->type     = tsf_tex_load_s::Stream;
    else
      load_op->type     = tsf_tex_load_s::Immediate;

    wcscpy (load_op->wszFilename, L"font.dds");

    if (load_op->type == tsf_tex_load_s::Stream) {
      tex_log.LogEx ( false, L"streaming\n" );
    }
  }

  //
  // Generic custom textures
  //
  else if ((! inject_thread) && custom_textures.find (checksum) != custom_textures.end ()) {
    tex_log.LogEx ( true, L"[Custom Tex] Custom texture for checksum (%08x)... ",
                      checksum );

    _swprintf ( wszInjectFileName, L"%s\\custom\\%08x%s",
                  TSFIX_TEXTURE_DIR,
                    checksum,
                      TSFIX_TEXTURE_EXT );

    load_op           = new tsf_tex_load_s;
    load_op->pDevice  = pDevice;
    load_op->checksum = checksum;
#if 0
    load_op->type     = tsf_tex_load_s::Immediate;
#else
    if (! is_streaming (load_op->checksum))
      load_op->type     = tsf_tex_load_s::Stream;
    else
      load_op->type     = tsf_tex_load_s::Immediate;
#endif

    wcscpy (load_op->wszFilename, wszInjectFileName);

    if (load_op->type == tsf_tex_load_s::Stream) {
      tex_log.LogEx ( false, L"streaming\n" );
    } else {
      LARGE_INTEGER start_inject, end_inject;

      QueryPerformanceCounter_Original (&start_inject);

      // TODO: Critical Section Guard
      inject_tids.insert (GetCurrentThreadId ());

      hr = D3DXCreateTextureFromFileEx (
        pDevice,
          load_op->wszFilename,//wszFileName,
            0, 0, 0,
              Usage, D3DFMT_UNKNOWN,
                Pool,
                  D3DX_DEFAULT, D3DX_DEFAULT,
                    ColorKey,
                      nullptr,
                        pPalette,
                          ppTexture );

      inject_tids.erase (GetCurrentThreadId ());

      QueryPerformanceCounter_Original (&end_inject);
      //tex_log.LogEx (false, L"failed (%s)\n", hr);
      delete load_op;
      load_op = nullptr;
    }
  }

  // Any previous attempts to load a custom texture failed, so load it the normal way
  if (hr == E_FAIL) {
    //tex_log.Log (L"D3DXCreateTextureFromFileInMemoryEx (... MipLevels=%lu ...)", MipLevels);
    hr =
      D3DXCreateTextureFromFileInMemoryEx_Original ( pDevice,
                                                       pSrcData,         SrcDataSize,
                                                         Width,          Height,    MipLevels,
                                                           Usage,        Format,    Pool,
                                                             Filter,     MipFilter, ColorKey,
                                                               pSrcInfo, pPalette,
                                                                 ppTexture );

    if (SUCCEEDED (hr)) {
      if (load_op != nullptr) {
        new ISKTextureD3D9 (ppTexture, SrcDataSize);

        load_op->SrcDataSize =
          custom_sizes [checksum];

        load_op->pDest = *ppTexture;
        EnterCriticalSection      (&cs_tex_stream);
        textures_in_flight.insert (load_op->checksum);
        textures_to_stream.push   (load_op);
        CreateThread              (nullptr, 0, TextureStreamThread, nullptr, 0, nullptr);
        LeaveCriticalSection      (&cs_tex_stream);
      }

// Temporarily disabled while mipmap-related issues are debugged...
#if 0
      else if (resample) {
        load_op              = new tsf_tex_load_s;
        load_op->pDevice     = pDevice;
        load_op->checksum    = checksum;
        load_op->type        = tsf_tex_load_s::Resample;
        load_op->pSrcData    = new uint8_t [SrcDataSize];
        load_op->SrcDataSize = SrcDataSize;
        memcpy (load_op->pSrcData, pSrcData, SrcDataSize);

        load_op->pDest = *ppTexture;
        load_op->pDest->AddRef (); // Don't delete this texture until we
                                   //   load the real one!

        EnterCriticalSection      (&cs_tex_resample);
        textures_to_resample.push (load_op);
        CreateThread              (nullptr, 0, TextureResampleThread, nullptr, 0, nullptr);
        LeaveCriticalSection      (&cs_tex_resample);
      }
#endif
    }

    else if (load_op != nullptr) {
      delete load_op;
      load_op = nullptr;
    }
  }

  QueryPerformanceCounter_Original (&end);

  if (SUCCEEDED (hr)) {
    if (config.textures.cache && checksum != 0x00) {
      tsf::RenderFix::Texture* pTex =
        new tsf::RenderFix::Texture ();

      pTex->crc32 = checksum;

      pTex->d3d9_tex = (*ppTexture);
      pTex->d3d9_tex->AddRef ();
      pTex->refs++;

      pTex->load_time = 1000.0f * (double)(end.QuadPart - start.QuadPart) / (double)freq.QuadPart;

      tsf::RenderFix::tex_mgr.addTexture (checksum, pTex, SrcDataSize);
    }

    if (config.textures.log) {
      tex_log.Log ( L"[Load Trace] Texture:   (%lu x %lu) * <LODs: %lu> - FAST_CRC32: %X",
                      Width, Height, (*ppTexture)->GetLevelCount (), checksum );
      tex_log.Log ( L"[Load Trace]              Usage: %-20s - Format: %-20s",
                      SK_D3D9_UsageToStr    (Usage).c_str (),
                        SK_D3D9_FormatToStr (Format).c_str () );
      tex_log.Log ( L"[Load Trace]                Pool: %s",
                      SK_D3D9_PoolToStr (Pool) );
      tex_log.Log ( L"[Load Trace]      Load Time: %6.4f ms", 
                    1000.0f * (double)(end.QuadPart - start.QuadPart) / (double)freq.QuadPart );
    }
  }

  if ( config.textures.dump && (! injecting) && (! inject_thread) /*&& (! custom_textures.count (checksum))*/ &&
                          (! dumped_textures.count (checksum)) ) {
    D3DXIMAGE_INFO info;
    D3DXGetImageInfoFromFileInMemory (pSrcData, SrcDataSize, &info);

    D3DFORMAT fmt_real = info.Format;

    bool compressed = (fmt_real >= D3DFMT_DXT1 && fmt_real <= D3DFMT_DXT5);

    wchar_t wszPath [MAX_PATH];
    _swprintf ( wszPath, L"%s\\dump\\%s",
                  TSFIX_TEXTURE_DIR, SK_D3D9_FormatToStr (fmt_real, false).c_str () );

    if (GetFileAttributesW (wszPath) != FILE_ATTRIBUTE_DIRECTORY)
      CreateDirectoryW (wszPath, nullptr);

    wchar_t wszFileName [MAX_PATH] = { L'\0' };
    _swprintf ( wszFileName, L"%s\\dump\\%s\\%08x%s",
                  TSFIX_TEXTURE_DIR,
                    SK_D3D9_FormatToStr (fmt_real, false).c_str (),
                      checksum,
                        L".png" );//TSFIX_TEXTURE_EXT );

    D3DXSaveTextureToFile (wszFileName, D3DXIFF_PNG, (*ppTexture), NULL);
  }

  return hr;
}


tsf::RenderFix::Texture*
tsf::RenderFix::TextureManager::getTexture (uint32_t checksum)
{
  if (textures.find (checksum) != textures.end ())
    return textures [checksum];

  return nullptr;
}

void
tsf::RenderFix::TextureManager::addTexture (uint32_t checksum, tsf::RenderFix::Texture* pTex, size_t size)
{
  pTex->size = size;
  basic_size += pTex->size;

  textures [checksum] = pTex;
  updateOSD ();
}

void
tsf::RenderFix::TextureManager::refTexture (tsf::RenderFix::Texture* pTex)
{
  pTex->d3d9_tex->AddRef ();
  pTex->refs++;

  ++hits;

  if (config.textures.log) {
    tex_log.Log ( L"[CacheTrace] Cache hit (%X), saved %2.1f ms",
                    pTex->crc32,
                      pTex->load_time );
  }

  time_saved += pTex->load_time;

  updateOSD ();
}

LPVOID dontcare_DebugSetMute;

void
WINAPI
DebugSetMute_Override (void)
{
  return;
}

void
tsf::RenderFix::TextureManager::Init (void)
{
  // Create the directory to store dumped textures
  if (config.textures.dump)
    CreateDirectoryW (TSFIX_TEXTURE_DIR, nullptr);

  tex_log.silent = false;
  tex_log.init ("logs/textures.log", "w+");

  //
  // Walk custom textures so we don't have to query the filesystem on every
  //   texture load to check if a custom one exists.
  //
  if ( GetFileAttributesW (TSFIX_TEXTURE_DIR L"\\custom") !=
         INVALID_FILE_ATTRIBUTES ) {
    WIN32_FIND_DATA fd;
    HANDLE          hFind  = INVALID_HANDLE_VALUE;
    int             files  = 0;
    LARGE_INTEGER   liSize = { 0 };

    tex_log.LogEx ( true, L"[Custom Tex] Enumerating custom textures..." );

    hFind = FindFirstFileW (TSFIX_TEXTURE_DIR L"\\custom\\*", &fd);

    if (hFind != INVALID_HANDLE_VALUE) {
      do {
        if (fd.dwFileAttributes != INVALID_FILE_ATTRIBUTES) {
          if (wcsstr (_wcslwr (fd.cFileName), TSFIX_TEXTURE_EXT)) {
            uint32_t checksum;
            swscanf (fd.cFileName, L"%x" TSFIX_TEXTURE_EXT, &checksum);

            ++files;

            LARGE_INTEGER fsize;

            fsize.HighPart = fd.nFileSizeHigh;
            fsize.LowPart  = fd.nFileSizeLow;

            liSize.QuadPart += fsize.QuadPart;

            custom_textures.insert (checksum);
            custom_sizes.insert    (
              std::pair <uint32_t, uint32_t> (
                checksum, (uint32_t)fsize.QuadPart
              )
            );
          }
        }
      } while (FindNextFileW (hFind, &fd) != 0);

      FindClose (hFind);
    }

    tex_log.LogEx ( false, L" %lu files (%3.1f MiB)\n",
                      files, (double)liSize.QuadPart / (1024.0 * 1024.0) );
  }

  if ( GetFileAttributesW (TSFIX_TEXTURE_DIR L"\\dump") !=
         INVALID_FILE_ATTRIBUTES ) {
    WIN32_FIND_DATA fd;
    WIN32_FIND_DATA fd_sub;
    HANDLE          hSubFind = INVALID_HANDLE_VALUE;
    HANDLE          hFind    = INVALID_HANDLE_VALUE;
    int             files    = 0;
    LARGE_INTEGER   liSize   = { 0 };

    tex_log.LogEx ( true, L"[Custom Tex] Enumerating dumped textures..." );

    hFind = FindFirstFileW (TSFIX_TEXTURE_DIR L"\\dump\\*", &fd);

    if (hFind != INVALID_HANDLE_VALUE) {
      do {
        if (fd.dwFileAttributes != INVALID_FILE_ATTRIBUTES) {
          wchar_t wszSubDir [MAX_PATH];
          _swprintf (wszSubDir, L"%s\\dump\\%s\\*", TSFIX_TEXTURE_DIR, fd.cFileName);

          hSubFind = FindFirstFileW (wszSubDir, &fd_sub);

          if (hSubFind != INVALID_HANDLE_VALUE) {
            do {
              if (wcsstr (_wcslwr (fd_sub.cFileName), L".png")) {
                uint32_t checksum;
                swscanf (fd_sub.cFileName, L"%08x.png", &checksum);

                ++files;

                LARGE_INTEGER fsize;

                fsize.HighPart = fd_sub.nFileSizeHigh;
                fsize.LowPart  = fd_sub.nFileSizeLow;

                liSize.QuadPart += fsize.QuadPart;

                dumped_textures.insert (checksum);
              }
            } while (FindNextFileW (hSubFind, &fd_sub) != 0);

            FindClose (hSubFind);
          }
        }
      } while (FindNextFileW (hFind, &fd) != 0);

      FindClose (hFind);
    }

    tex_log.LogEx ( false, L" %lu files (%3.1f MiB)\n",
                      files, (double)liSize.QuadPart / (1024.0 * 1024.0) );
  }


  TSFix_CreateDLLHook ( config.system.injector.c_str (),
                        "D3D9StretchRect_Override",
                         D3D9StretchRect_Detour,
               (LPVOID*)&D3D9StretchRect_Original );
#if 0
  TSFix_CreateDLLHook ( config.system.injector.c_str (),
                        "D3D9CreateDepthStencilSurface_Override",
                         D3D9CreateDepthStencilSurface_Detour,
               (LPVOID*)&D3D9CreateDepthStencilSurface_Original );
#endif

  TSFix_CreateDLLHook ( config.system.injector.c_str (),
                        "D3D9CreateTexture_Override",
                         D3D9CreateTexture_Detour,
               (LPVOID*)&D3D9CreateTexture_Original );

  TSFix_CreateDLLHook ( config.system.injector.c_str (),
                        "D3D9SetTexture_Override",
                         D3D9SetTexture_Detour,
               (LPVOID*)&D3D9SetTexture_Original );

  TSFix_CreateDLLHook ( config.system.injector.c_str (),
                        "D3D9SetRenderTarget_Override",
                         D3D9SetRenderTarget_Detour,
               (LPVOID*)&D3D9SetRenderTarget_Original );

  TSFix_CreateDLLHook ( config.system.injector.c_str (),
                        "D3D9SetDepthStencilSurface_Override",
                         D3D9SetDepthStencilSurface_Detour,
               (LPVOID*)&D3D9SetDepthStencilSurface_Original );

  TSFix_CreateDLLHook ( L"d3d9.dll",
                        "DebugSetMute",
                         DebugSetMute_Override,
               (LPVOID*)&dontcare_DebugSetMute );

  TSFix_CreateDLLHook ( L"D3DX9_43.DLL",
                         "D3DXCreateTextureFromFileInMemoryEx",
                          D3DXCreateTextureFromFileInMemoryEx_Detour,
               (LPVOID *)&D3DXCreateTextureFromFileInMemoryEx_Original );

  D3DXSaveTextureToFile =
    (D3DXSaveTextureToFile_pfn)
      GetProcAddress ( tsf::RenderFix::d3dx9_43_dll,
                         "D3DXSaveTextureToFileW" );

  D3DXCreateTextureFromFile =
    (D3DXCreateTextureFromFile_pfn)
      GetProcAddress ( tsf::RenderFix::d3dx9_43_dll,
                         "D3DXCreateTextureFromFileW" );

  D3DXCreateTextureFromFileEx =
    (D3DXCreateTextureFromFileEx_pfn)
      GetProcAddress ( tsf::RenderFix::d3dx9_43_dll,
                         "D3DXCreateTextureFromFileExW" );

  D3DXGetImageInfoFromFileInMemory =
    (D3DXGetImageInfoFromFileInMemory_pfn)
      GetProcAddress ( tsf::RenderFix::d3dx9_43_dll,
                         "D3DXGetImageInfoFromFileInMemory" );

  D3DXGetImageInfoFromFile =
    (D3DXGetImageInfoFromFile_pfn)
      GetProcAddress ( tsf::RenderFix::d3dx9_43_dll,
                         "D3DXGetImageInfoFromFileW" );

  // We don't hook this, but we still use it...
  if (D3D9CreateRenderTarget_Original == nullptr) {
    static HMODULE hModD3D9 =
      GetModuleHandle (config.system.injector.c_str ());
    D3D9CreateRenderTarget_Original =
      (CreateRenderTarget_pfn)
        GetProcAddress (hModD3D9, "D3D9CreateRenderTarget_Override");
  }

  // We don't hook this, but we still use it...
  if (D3D9CreateDepthStencilSurface_Original == nullptr) {
    static HMODULE hModD3D9 =
      GetModuleHandle (config.system.injector.c_str ());
    D3D9CreateDepthStencilSurface_Original =
      (CreateDepthStencilSurface_pfn)
        GetProcAddress (hModD3D9, "D3D9CreateDepthStencilSurface_Override");
  }

  time_saved = 0.0f;

  InitializeCriticalSectionAndSpinCount (&cs_tex_inject,   10000000);
  InitializeCriticalSectionAndSpinCount (&cs_tex_resample, 100000);
  InitializeCriticalSectionAndSpinCount (&cs_tex_stream,   100000);
}


void
tsf::RenderFix::TextureManager::Shutdown (void)
{
  while (! textures_to_resample.empty ())
    textures_to_resample.pop ();

  while (! textures_to_stream.empty ())
    textures_to_stream.pop ();

  tex_mgr.reset ();

  DeleteCriticalSection (&cs_tex_stream);
  DeleteCriticalSection (&cs_tex_resample);
  DeleteCriticalSection (&cs_tex_inject);

  // 33.3 ms per-frame (30 FPS)
  const float frame_time = 33.3f;

  tex_log.Log ( L"[Perf Stats] At shutdown: %7.2f seconds (%7.2f frames)"
                L" saved by cache",
                  time_saved / 1000.0f,
                    time_saved / frame_time );
  tex_log.close ();
}

void
tsf::RenderFix::TextureManager::purge (void)
{
  return;

  int released = 0;

  tex_log.Log (L"[ Tex. Mgr ] -- TextureManager::purge (...) -- ");

  while (pending_streams ())
    Sleep (10);

  if (pending_loads ())
    TSFix_LoadQueuedTextures ();

  tex_log.Log (L"[ Tex. Mgr ]   Releasing textures...");

  std::unordered_map <uint32_t, tsf::RenderFix::Texture *>::iterator it =
    textures.begin ();

  while (it != textures.end ()) {
    int tex_refs = 0;
    // This change resolves outstanding references
    for (int i = 0; i < (*it).second->refs; i++) {
      tex_refs = (*it).second->d3d9_tex->Release ();
      (*it).second->refs--;

      if (tex_refs == 0) {
      void* dontcare;
      if ( (*it).second->d3d9_tex != nullptr &&
           (*it).second->d3d9_tex->QueryInterface (IID_SKTextureD3D9, &dontcare) == S_OK ) {
        if (((ISKTextureD3D9 *)(*it).second->d3d9_tex)->pTexOverride != nullptr) {
          if (((ISKTextureD3D9 *)(*it).second->d3d9_tex)->pTexOverride->Release () == 0) {
            ((ISKTextureD3D9 *)(*it).second->d3d9_tex)->pTexOverride = nullptr;
            custom_count--;
            custom_size -= ((ISKTextureD3D9 *)(*it).second->d3d9_tex)->override_size;
          }
          //((ISKTextureD3D9 *)(*it).second->d3d9_tex)->pTexOverride->Release ();
        }
      }
      }
    }

    if (tex_refs == 0) {
      ++released;
      basic_size -= (*it).second->size;
      it = textures.erase (it);
    } else {
      (*it).second->refs = 1;
      (*it).second->d3d9_tex->AddRef ();
      ++it;
    }
  }

  tex_log.Log ( L"[ Tex. Mgr ]   %4d textures (%4d remain)",
                  released,
                    textures.size () );

  updateOSD ();

  tex_log.Log (L"[ Tex. Mgr ] ----------- Finished ------------ ");
}

void
tsf::RenderFix::TextureManager::reset (void)
{
  last_tex = nullptr;

  int underflows    = 0;

  int ext_refs      = 0;
  int ext_textures  = 0;

  int release_count = 0;
  int ref_count     = 0;

  tex_log.Log (L"[ Tex. Mgr ] -- TextureManager::reset (...) -- ");

  while (pending_streams ())
    Sleep (0);

  if (pending_loads ())
    TSFix_LoadQueuedTextures ();

  tex_log.Log (L"[ Tex. Mgr ]   Releasing textures...");

  std::unordered_map <uint32_t, tsf::RenderFix::Texture *>::iterator it =
    textures.begin ();

  while (it != textures.end ()) {
    int tex_refs = 0;
    release_count++;

    // This change resolves outstanding references
    for (int i = 0; i <= (*it).second->refs; i++) {
    //for (int i = 0; i < (*it).second->refs; i++) {
      ref_count++;
      tex_refs = (*it).second->d3d9_tex->Release ();

      if (tex_refs == 0) {
        void* dontcare;
        if ( (*it).second->d3d9_tex != nullptr &&
             (*it).second->d3d9_tex->QueryInterface (IID_SKTextureD3D9, &dontcare) == S_OK ) {
          //if (((ISKTextureD3D9 *)(*it).second->d3d9_tex)->pTexOverride != nullptr) {
          if (((ISKTextureD3D9 *)(*it).second->d3d9_tex)->override_size != 0) {//((ISKTextureD3D9 *)(*it).second->d3d9_tex)->pTexOverride->Release () == 0) {
              custom_count--;
              custom_size -= ((ISKTextureD3D9 *)(*it).second->d3d9_tex)->override_size;
              //((ISKTextureD3D9 *)(*it).second->d3d9_tex)->pTexOverride  = nullptr;
              ((ISKTextureD3D9 *)(*it).second->d3d9_tex)->override_size = 0;
            //((ISKTextureD3D9 *)(*it).second->d3d9_tex)->pTexOverride->Release ();
          }
        }
      }
    }

    if (tex_refs > 0) {
      ext_refs     += tex_refs;
      ext_textures ++;
      ++it;
    } else {
      basic_size -= (*it).second->size;
      it = textures.erase (it);
    }

    if (tex_refs < 0) {
      ++underflows;
    }
  }

  tex_log.Log ( L"[ Tex. Mgr ]   %4d textures (%4d references)",
                  release_count,
                    ref_count );

  if (ext_refs > 0) {
    tex_log.Log ( L"[ Tex. Mgr ] >> WARNING: The game is still holding references (%d) to %d textures !!!",
                    ext_refs, ext_textures );
  }

  if (underflows) {
    tex_log.Log ( L"[ Tex. Mgr ] >> WARNING: Reference counting sanity check failed: "
                  L"Reference Underflow (%d times) !!!",
                    underflows );
  }

  // If there are extra references, chances are this will fail as well -- let's
  //   skip it.
  if ((! ext_refs) && config.render.msaa_samples > 0) {
    tex_log.Log ( L"[ MSAA Mgr ]   Releasing MSAA surfaces...");

    int count = 0,
        refs  = 0;

    std::unordered_map <IDirect3DSurface9*, IDirect3DSurface9*>::iterator it =
      rt_msaa.begin ();

    while (it != rt_msaa.end ()) {
      ++count;

      if ((*it).first != nullptr)
        refs += (*it).first->Release ();
      else
        refs++;

      if ((*it).second != nullptr)
        refs += (*it).second->Release ();
      else
        refs++;

      it = rt_msaa.erase (it);
    }

    dirty_surfs.clear          ();
    msaa_surfs.clear           ();
    msaa_backing_map.clear     ();
    msaa_backing_map_rev.clear ();

    tex_log.Log ( L"[ MSAA Mgr ]   %4d surfaces (%4d zombies)",
                      count, refs );
  }

  //custom_count = 0;
  //custom_size  = 0;
  //basic_size   = 0;

  updateOSD ();

  tex_log.Log (L"[ Tex. Mgr ] ----------- Finished ------------ ");
}

void
tsf::RenderFix::TextureManager::updateOSD (void)
{
  double cache_basic  = (double)cacheSizeBasic  () / (1024.0f * 1024.0f);
  double cache_custom = (double)cacheSizeCustom () / (1024.0f * 1024.0f);
  double cache_total  = cache_basic + cache_custom;

  osd_stats = "";

  char szFormatted [64];
  sprintf ( szFormatted, "%6lu  Total Textures : %8.2f MiB\n",
              numTextures () + numCustomTextures (),
                cache_total );
  osd_stats += szFormatted;

  sprintf ( szFormatted, "%6lu  Basic Textures : %8.2f MiB\n",
              numTextures (),
                cache_basic );

  osd_stats += szFormatted;

  sprintf ( szFormatted, "%6lu Custom Textures : %8.2f MiB\n",
              numCustomTextures (),
                cache_custom );

  osd_stats += szFormatted;

  sprintf ( szFormatted, "%6lu  Cache Hits     : %8.2f Seconds Saved",
              hits,
                time_saved / 1000.0f );

  osd_stats += szFormatted;

}