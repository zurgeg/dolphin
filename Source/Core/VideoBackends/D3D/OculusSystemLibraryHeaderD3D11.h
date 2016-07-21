#pragma once
#include "VideoCommon/VROculus.h"
#include "d3d11.h"

#if !defined(HAVE_OCULUSSDK) || OVR_PRODUCT_VERSION >= 1

typedef struct
{
  ovrTextureHeader5 Header;
  ID3D11Texture2D* pTexture;
  ID3D11ShaderResourceView* pSRView;
} ovrD3D11TextureData5;

typedef union {
  ovrD3D11TextureData5 D3D11;
  ovrTexture5 Texture;
} ovrD3D11Texture5;

typedef struct ALIGN_TO_POINTER_BOUNDARY
{
  ovrTextureHeader6 Header;
#ifdef _WIN64
  unsigned padding;
#endif
  ID3D11Texture2D* pTexture;
  ID3D11ShaderResourceView* pSRView;
} ovrD3D11TextureData6;

typedef union {
  ovrD3D11TextureData6 D3D11;
  ovrTexture6 Texture;
} ovrD3D11Texture6;

#else

#define ovrD3D11Texture5 ovrD3D11Texture
#define ovrD3D11Texture6 ovrD3D11Texture

#endif

#if !defined(HAVE_OCULUSSDK) || OVR_MAJOR_VERSION > 5 || OVR_PRODUCT_VERSION > 0

typedef struct
{
  ovrRenderAPIConfigHeader Header;
  ID3D11Device* pDevice;
  ID3D11DeviceContext* pDeviceContext;
  ID3D11RenderTargetView* pBackBufferRT;
  ID3D11UnorderedAccessView* pBackBufferUAV;
  IDXGISwapChain* pSwapChain;
} ovrD3D11ConfigData;

typedef union {
  ovrD3D11ConfigData D3D11;
  ovrRenderAPIConfig Config;
} ovrD3D11Config;

#endif