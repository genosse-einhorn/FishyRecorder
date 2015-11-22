#include "screenviewrenderer.h"

#include "screenviewshaders.h"

#include <QDebug>
#include <QtWin>
#include <QSysInfo>
#include <QTime>
#include <QTimer>
#include <QThread>
#include <QCoreApplication>
#include <memory>
#include <algorithm>
#include <cstring>

//FIXME: Workaround for MXE's incomplete winerror.h
#ifndef DXGI_ERROR_WAIT_TIMEOUT
#define DXGI_ERROR_WAIT_TIMEOUT                 _HRESULT_TYPEDEF_(0x887A0027)
#endif
#ifndef DXGI_ERROR_ACCESS_LOST
#define DXGI_ERROR_ACCESS_LOST                  _HRESULT_TYPEDEF_(0x887A0026)
#endif

/**
 * return the pixel value in an indexed pixel format with a bpp <= 8
 */
template<unsigned bpp>
static inline uint8_t get_pixel_from_row(uint8_t *row, int x) {
    return (row[x * bpp / 8] >> (8 - bpp - (x % (8 / bpp)) * bpp)) & ((1 << bpp) - 1);
}

namespace Presentation {

ScreenViewRenderer::ScreenViewRenderer(HWND window, const QRect &screen, QObject *parent) : QObject(parent)
{
    m_desktopWidth  = screen.width();
    m_desktopHeight = screen.height();
    m_desktopX      = screen.x();
    m_desktopY      = screen.y();

    m_resetTimer = new QTimer(this);
    QObject::connect(m_resetTimer, &QTimer::timeout, this, &ScreenViewRenderer::reset);

    if (!setupDxgiAndD3DDevice(window))
        return;

    if (!setupShaders())
        return;

    if (!setupInputLayout())
        return;

    if (!setupSamplers())
        return;

    if (!setupBlendState())
        return;

    // sets render target and viewport
    RECT cr;
    GetClientRect(window, &cr);
    this->windowResized(QRect(cr.left, cr.top, cr.right - cr.left, cr.bottom - cr.top));

    m_isGood = reset();
}

ScreenViewRenderer::~ScreenViewRenderer()
{
    if (m_context)
        m_context->ClearState();
}

bool ScreenViewRenderer::updateScreen(const QRect &screen)
{
    m_desktopWidth  = screen.width();
    m_desktopHeight = screen.height();
    m_desktopX      = screen.x();
    m_desktopY      = screen.y();

    return reset();
}

bool ScreenViewRenderer::reset()
{
    m_resetTimer->stop();

    m_duplication.reset();
    m_duplDesktopImage.reset();
    m_frameAcquired = false;

    if (!m_device)
        return false;

    // find the matching output
    com::ptr<IDXGIDevice>  dev;
    com::ptr<IDXGIAdapter> adp;

    dev = m_device.query<IDXGIDevice>();
    dev->GetAdapter(com::out_arg(adp));

    com::ptr<IDXGIOutput> output;
    for (UINT i = 0; SUCCEEDED(adp->EnumOutputs(i, com::out_arg(output))); ++i) {
        DXGI_OUTPUT_DESC desc;
        output->GetDesc(&desc);

        if (desc.AttachedToDesktop
            && desc.DesktopCoordinates.left == m_desktopX
            && desc.DesktopCoordinates.top  == m_desktopY
            && desc.DesktopCoordinates.right == m_desktopX + m_desktopWidth
            && desc.DesktopCoordinates.bottom == m_desktopY + m_desktopHeight)
        {
            qWarning() << "Attempting to duplicate display " << i;

            if (QSysInfo::WindowsVersion == QSysInfo::WV_WINDOWS7) {
                // Win7
                if (!m_dd4seven_duplicate) {
                    qWarning() << "DuplicationSource: Missing compatible dd4seven-api.dll :(";
                    break;
                }

                HRESULT hr = m_dd4seven_duplicate(output, m_device, com::out_arg(m_duplication));
                if FAILED(hr)
                    qWarning() << "Attempted to duplicate display " << i << " but: " << QtWin::stringFromHresult(hr);

                break;
            } else {
                // Win8
                com::ptr<IDXGIOutput1> output1 = output.query<IDXGIOutput1>();

                if (!output1)
                    continue;

                HRESULT hr = output1->DuplicateOutput(m_device, com::out_arg(m_duplication));
                if FAILED(hr)
                    qWarning() << "Attempted to duplicate display " << i << " but: " << QtWin::stringFromHresult(hr);

                break;
            }
        }
    }

    if (!m_duplication) {
        qWarning() << "WARNING: Couldn't find display:" << QRect(m_desktopX, m_desktopY, m_desktopWidth, m_desktopHeight);

        m_resetTimer->start(2000);

        return false;
    }

    setupDesktopTextureAndVertices();
    setupCursorTextureAndVertices();

    return true;
}

void ScreenViewRenderer::windowResized(const QRect &size)
{
    HRESULT hr;

    if (!m_device)
        return;

    // reset view
    m_context->OMSetRenderTargets(0, nullptr, nullptr);
    m_renderTarget.reset();

    hr = m_swap->ResizeBuffers(0, UINT(size.width()), UINT(size.height()), DXGI_FORMAT_UNKNOWN, 0);
    if FAILED(hr) {
        qWarning() << "Failed to resize buffers :( " << QtWin::stringFromHresult(hr);
    }

    // reset render target
    com::ptr<ID3D11Texture2D> backBuffer;
    hr = m_swap->GetBuffer(0, IID_PPV_ARGS(com::out_arg(backBuffer)));
    if FAILED(hr) {
        qWarning() << "Failed to get back buffer :( " << QtWin::stringFromHresult(hr);
    }

    hr = m_device->CreateRenderTargetView(backBuffer, nullptr, com::out_arg(m_renderTarget));
    if FAILED(hr) {
        qWarning() << "Failed to create new render target view :( " << QtWin::stringFromHresult(hr);
    }

    m_context->OMSetRenderTargets(1, com::single_item_array(m_renderTarget), nullptr);

    // Create and set a viewport, preserving the original aspect ratio
    float desktopRatio = float(m_desktopWidth)/float(m_desktopHeight);
    float windowRatio = float(size.width())/float(size.height());

    int w, h;
    if (desktopRatio > windowRatio) {
        // desktop is wider than the window -> crop height
        w = size.width();
        h = int(float(size.width()) / desktopRatio);
    } else {
        // window is wider than the desktop -> crop width
        h = size.height();
        w = int(float(size.height()) * desktopRatio);
    }


    D3D11_VIEWPORT viewport = {
        .TopLeftX = FLOAT((size.width() - w)/2),
        .TopLeftY = FLOAT((size.height() - h)/2),
        .Width    = FLOAT(w),
        .Height   = FLOAT(h),
        .MinDepth = 0,
        .MaxDepth = 0
    };
    m_context->RSSetViewports(1, &viewport);
}

void ScreenViewRenderer::startRendering()
{
    m_requestStop = false;

    QTime watch;
    watch.start();

    while (!m_requestStop) {
        // Do all the fancy stuff
        render();

        // As a safety net against broken vsync, we cap the FPS at 100
        int elapsed = watch.restart();
        if ((elapsed) < 10)
            QThread::msleep(10 - elapsed);

        // Do event stuff
        QCoreApplication::processEvents();
    }

    emit stopped();
}

void ScreenViewRenderer::stop()
{
    m_requestStop = true;
}

bool ScreenViewRenderer::setupDxgiAndD3DDevice(HWND hwnd)
{
    HRESULT hr;

    if (!m_d3dCreator)
        return false;

    // We create the device and swap chain in one go
    DXGI_SWAP_CHAIN_DESC desc;
    memset(&desc, 0, sizeof(desc));
    desc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count  = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 1;
    desc.OutputWindow = hwnd;
    desc.Windowed = true;

    D3D_FEATURE_LEVEL requestedLevels[] = { D3D_FEATURE_LEVEL_9_1 };
    D3D_FEATURE_LEVEL receivedLevel;
    hr = m_d3dCreator(nullptr,
                      D3D_DRIVER_TYPE_HARDWARE,
                      nullptr,
                      D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                      requestedLevels, sizeof(requestedLevels)/sizeof(requestedLevels[0]),
                      D3D11_SDK_VERSION,
                      &desc,
                      com::out_arg(m_swap),
                      com::out_arg(m_device),
                      &receivedLevel,
                      com::out_arg(m_context)
                     );
    if FAILED(hr) {
        qWarning() << "Failed to create device and swap chain :( " << QtWin::stringFromHresult(hr);
        return false;
    }

    // make vsync possible
    auto dxgiDevice = m_device.query<IDXGIDevice1>();
    if (dxgiDevice)
        dxgiDevice->SetMaximumFrameLatency(1);

    return true;
}

bool ScreenViewRenderer::setupShaders()
{
    HRESULT hr;

    // Load the shaders
    hr = m_device->CreatePixelShader(ScreenViewShaders::pshader, sizeof(ScreenViewShaders::pshader), nullptr, com::out_arg(m_pshader));
    if FAILED(hr) {
        qWarning() << "Failed to create pixel shader :( " << QtWin::stringFromHresult(hr);
        return false;
    }

    hr = m_device->CreateVertexShader(ScreenViewShaders::vshader, sizeof(ScreenViewShaders::vshader), nullptr, com::out_arg(m_vshader));
    if FAILED(hr) {
        qWarning() << "Failed to create vertex shader :( " << QtWin::stringFromHresult(hr);
        return false;
    }

    m_context->VSSetShader(m_vshader, nullptr, 0);
    m_context->PSSetShader(m_pshader, nullptr, 0);

    return true;
}

bool ScreenViewRenderer::setupInputLayout()
{
    HRESULT hr;

    // Setup the input layout
    D3D11_INPUT_ELEMENT_DESC ied[] =
    {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    hr = m_device->CreateInputLayout(ied, 2, ScreenViewShaders::vshader, sizeof(ScreenViewShaders::vshader), com::out_arg(m_ilayout));
    if FAILED(hr) {
        qWarning() << "Failed to create input layout " << QtWin::stringFromHresult(hr);
        return false;
    }

    m_context->IASetInputLayout(m_ilayout);

    return true;
}

bool ScreenViewRenderer::setupSamplers()
{
    HRESULT hr;

    // Create and set a sampler
    D3D11_SAMPLER_DESC samplerdsc = {
        .Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR,
        .AddressU = D3D11_TEXTURE_ADDRESS_CLAMP,
        .AddressV = D3D11_TEXTURE_ADDRESS_CLAMP,
        .AddressW = D3D11_TEXTURE_ADDRESS_CLAMP,
        .MipLODBias = 0.0f,
        .MaxAnisotropy = 1,
        .ComparisonFunc = D3D11_COMPARISON_ALWAYS,
        .BorderColor = { 0.0f, 0.0f, 0.0f, 1.0f },
        .MinLOD = 0.0f,
        .MaxLOD = D3D11_FLOAT32_MAX
    };
    hr = m_device->CreateSamplerState(&samplerdsc, com::out_arg(m_sampler));
    if FAILED(hr) {
        qWarning() << "Failed to create sampler state: " << QtWin::stringFromHresult(hr);
        return false;
    }
    m_context->PSSetSamplers(0, 1, com::single_item_array(m_sampler));

    return true;
}

bool ScreenViewRenderer::setupBlendState()
{
    HRESULT hr;

    // setup the blend state
    D3D11_BLEND_DESC blenddsc;
    memset(&blenddsc, 0, sizeof(D3D11_BLEND_DESC));

    blenddsc.AlphaToCoverageEnable = FALSE;
    blenddsc.IndependentBlendEnable = FALSE;
    blenddsc.RenderTarget[0] = D3D11_RENDER_TARGET_BLEND_DESC({
        .BlendEnable = TRUE,
        .SrcBlend = D3D11_BLEND_SRC_ALPHA,
        .DestBlend = D3D11_BLEND_INV_SRC_ALPHA,
        .BlendOp = D3D11_BLEND_OP_ADD,
        .SrcBlendAlpha = D3D11_BLEND_ZERO,
        .DestBlendAlpha = D3D11_BLEND_ZERO,
        .BlendOpAlpha = D3D11_BLEND_OP_ADD,
        .RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL
    });

    hr = m_device->CreateBlendState(&blenddsc, com::out_arg(m_blendState));
    if FAILED(hr) {
        qWarning() << "Failed to create blend state: " << QtWin::stringFromHresult(hr);
        return false;
    }
    m_context->OMSetBlendState(m_blendState, nullptr, 0xFFFFFFFF);

    return true;
}

bool ScreenViewRenderer::setupDesktopTextureAndVertices()
{
    HRESULT hr;

    if (!m_duplication || !m_device)
        return false;

    DXGI_OUTDUPL_DESC dpldesc;
    m_duplication->GetDesc(&dpldesc);

    D3D11_TEXTURE2D_DESC texdsc = D3D11_TEXTURE2D_DESC({
        .Width = dpldesc.ModeDesc.Width,
        .Height = dpldesc.ModeDesc.Height,
        .MipLevels = 1,
        .ArraySize = 1,
        .Format = DXGI_FORMAT_B8G8R8A8_UNORM,
        .SampleDesc = DXGI_SAMPLE_DESC({
            .Count = 1,
            .Quality = 0
        }),
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_SHADER_RESOURCE,
        .CPUAccessFlags = 0,
        .MiscFlags = 0
    });

    hr = m_device->CreateTexture2D(&texdsc, nullptr, com::out_arg(m_desktopTexture));
    if FAILED(hr) {
        qWarning() << "Failed: CreateTexture2D: " << QtWin::stringFromHresult(hr);
        return false;
    }

    hr = m_device->CreateShaderResourceView(m_desktopTexture, nullptr, com::out_arg(m_desktopSrv));
    if FAILED(hr) {
        qWarning() << "Failed: CreateShaderResourceView: " << QtWin::stringFromHresult(hr);
        return false;
    }

    // create vertex buffers
    VERTEX desktopVertices[] = {
        //  X  |   Y  |  Z  |  U  |  V   |
        { -1.0f,  1.0f, 0.0f, 0.0f, 0.0f }, // LEFT TOP
        {  1.0f, -1.0f, 0.0f, 1.0f, 1.0f }, // RIGHT BOTTOM
        { -1.0f, -1.0f, 0.0f, 0.0f, 1.0f }, // LEFT BOTTOM
        { -1.0f,  1.0f, 0.0f, 0.0f, 0.0f }, // LEFT TOP
        {  1.0f,  1.0f, 0.0f, 1.0f, 0.0f }, // RIGHT TOP
        {  1.0f, -1.0f, 0.0f, 1.0f, 1.0f }  // RIGHT BOTTOM
    };
    D3D11_BUFFER_DESC desktopVBufferDesc = {
        .ByteWidth = sizeof(desktopVertices),
        .Usage = D3D11_USAGE_IMMUTABLE,
        .BindFlags = D3D11_BIND_VERTEX_BUFFER,
        .CPUAccessFlags = 0,
        .MiscFlags = 0,
        .StructureByteStride = sizeof(desktopVertices[0])
    };
    D3D11_SUBRESOURCE_DATA desktopVBufferData = {
        .pSysMem = desktopVertices,
        .SysMemPitch = 0,
        .SysMemSlicePitch = 0
    };
    hr = m_device->CreateBuffer(&desktopVBufferDesc, &desktopVBufferData, com::out_arg(m_desktopVBuffer));
    if FAILED(hr) {
        qWarning() << "FAILED: CreateBuffer (desktopVBuffer): " << QtWin::stringFromHresult(hr);
        return false;
    }

    return true;
}

bool ScreenViewRenderer::setupCursorTextureAndVertices()
{
    HRESULT hr;

    D3D11_TEXTURE2D_DESC texdsc = D3D11_TEXTURE2D_DESC({
        .Width = CURSOR_TEX_SIZE,
        .Height = CURSOR_TEX_SIZE,
        .MipLevels = 1,
        .ArraySize = 1,
        .Format = DXGI_FORMAT_B8G8R8A8_UNORM,
        .SampleDesc = DXGI_SAMPLE_DESC({
            .Count = 1,
            .Quality = 0
        }),
        .Usage = D3D11_USAGE_DYNAMIC,
        .BindFlags = D3D11_BIND_SHADER_RESOURCE,
        .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
        .MiscFlags = 0
    });

    hr = m_device->CreateTexture2D(&texdsc, nullptr, com::out_arg(m_cursorTexture));
    if FAILED(hr) {
        qWarning() << "Failed:CreateTexture2D: " << QtWin::stringFromHresult(hr);
        return false;
    }


    hr = m_device->CreateShaderResourceView(m_cursorTexture, nullptr, com::out_arg(m_cursorSrv));
    if FAILED(hr) {
        qWarning() << "Fail: CreateShaderResourceView: " << QtWin::stringFromHresult(hr);
        return false;
    }

    // the cursor needs a vertex buffer, too!
    VERTEX vertices[6] = {
        //  X  |   Y  |  Z  |  U  |  V   |
        {  0.0f,  0.0f, 0.0f, 0.0f, 0.0f }, // LEFT TOP
        {  0.0f,  0.0f, 0.0f, 1.0f, 1.0f }, // RIGHT BOTTOM
        {  0.0f,  0.0f, 0.0f, 0.0f, 1.0f }, // LEFT BOTTOM
        {  0.0f,  0.0f, 0.0f, 0.0f, 0.0f }, // LEFT TOP
        {  0.0f,  0.0f, 0.0f, 1.0f, 0.0f }, // RIGHT TOP
        {  0.0f,  0.0f, 0.0f, 1.0f, 1.0f }  // RIGHT BOTTOM
    };
    D3D11_BUFFER_DESC vbufferDesc = {
        .ByteWidth = sizeof(vertices),
        .Usage = D3D11_USAGE_DYNAMIC,
        .BindFlags = D3D11_BIND_VERTEX_BUFFER,
        .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
        .MiscFlags = 0,
        .StructureByteStride = sizeof(vertices[0])
    };
    D3D11_SUBRESOURCE_DATA vbufferData = {
        .pSysMem = vertices,
        .SysMemPitch = 0,
        .SysMemSlicePitch = 0
    };
    hr = m_device->CreateBuffer(&vbufferDesc, &vbufferData, com::out_arg(m_cursorVBuffer));
    if FAILED(hr) {
        qWarning() << "FAILED: CreateBuffer (cursorVBuffer): " << QtWin::stringFromHresult(hr);
        return false;
    }

    return true;
}

void ScreenViewRenderer::updateCursorPosition()
{
    if (!m_cursorVBuffer)
        return;

    float left   = -1.0f + 2.0f*static_cast<float>(m_cursorX)/static_cast<float>(m_desktopWidth);
    float top    =  1.0f - 2.0f*static_cast<float>(m_cursorY)/static_cast<float>(m_desktopHeight);
    float right  =  left + 2.0f*static_cast<float>(CURSOR_TEX_SIZE)/static_cast<float>(m_desktopWidth);
    float bottom =  top  - 2.0f*static_cast<float>(CURSOR_TEX_SIZE)/static_cast<float>(m_desktopHeight);
    float uleft   = 0.0f;
    float vtop    = 0.0f;
    float uright  = 1.0f;
    float vbottom = 1.0f;

    // now write this into the vertex buffer
    VERTEX *vertices = nullptr;

    D3D11_MAPPED_SUBRESOURCE map;

    HRESULT hr = m_context->Map(m_cursorVBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
    if FAILED(hr) {
        qWarning() << "FAILED: ID3D11Buffer::Map: " << QtWin::stringFromHresult(hr);
        return;
    }

    vertices = (VERTEX*)map.pData;

    //  X   |   Y   |  Z  |   U   |  V     |
    vertices[0] = { left,  top,    0.0f, uleft,  vtop    }; // LEFT TOP
    vertices[1] = { right, bottom, 0.0f, uright, vbottom }; // RIGHT BOTTOM
    vertices[2] = { left,  bottom, 0.0f, uleft,  vbottom }; // LEFT BOTTOM
    vertices[3] = { left,  top,    0.0f, uleft,  vtop    }; // LEFT TOP
    vertices[4] = { right, top,    0.0f, uright, vtop    }; // RIGHT TOP
    vertices[5] = { right, bottom, 0.0f, uright, vbottom }; // RIGHT BOTTOM

    m_context->Unmap(m_cursorVBuffer, 0);
}

void ScreenViewRenderer::acquireFrame()
{
    HRESULT hr;

    if (!m_duplication || !m_device)
        return;

    hr = m_duplication->AcquireNextFrame(100, &m_duplInfo, com::out_arg(m_duplDesktopImage));

    if (hr == DXGI_ERROR_WAIT_TIMEOUT)
        return; // This can happen if the screen is idle or whatever, it's not really fatal enough to log

    if (!(m_frameAcquired = SUCCEEDED(hr)))
        qWarning() << "Failed: AcquireNextFrame: " << QtWin::stringFromHresult(hr);

    // if the access has been lost, we might get away with just recreating it again
    if (FAILED(hr) && hr == DXGI_ERROR_ACCESS_LOST) {
        qWarning() << "Recreating the IDXGIOutputDuplication interface because of DXGI_ERROR_ACCESS_LOST=" << hr;

        m_resetTimer->start(2000);
    }
}

void ScreenViewRenderer::updateDesktop()
{
    if (!m_desktopTexture || !m_frameAcquired || !m_device)
        return;

    if (!m_duplInfo.LastPresentTime.QuadPart || !m_duplDesktopImage)
        return;

    auto d3dresource = m_duplDesktopImage.query<ID3D11Resource>();
    m_context->CopyResource(m_desktopTexture, d3dresource);
}

void ScreenViewRenderer::updateCursor()
{
    HRESULT hr;

    if (!m_cursorTexture || !m_frameAcquired)
        return;

    if (!m_duplInfo.LastMouseUpdateTime.QuadPart)
        return;

    if ((m_cursorVisible = m_duplInfo.PointerPosition.Visible)) {
        m_cursorX = m_duplInfo.PointerPosition.Position.x;
        m_cursorY = m_duplInfo.PointerPosition.Position.y;
    }

    if (!m_duplInfo.PointerShapeBufferSize)
        return;

    std::unique_ptr<uint8_t[]> buffer(new uint8_t[m_duplInfo.PointerShapeBufferSize]);

    DXGI_OUTDUPL_POINTER_SHAPE_INFO pointer;
    UINT dummy;
    hr = m_duplication->GetFramePointerShape(m_duplInfo.PointerShapeBufferSize, reinterpret_cast<void*>(buffer.get()), &dummy, &pointer);
    if FAILED(hr) {
        qWarning() << "Failed: GetFramePointerShape: " << QtWin::stringFromHresult(hr);
        return;
    }

    // we can now update the pointer shape
    D3D11_MAPPED_SUBRESOURCE info;
    hr = m_context->Map(m_cursorTexture, 0, D3D11_MAP_WRITE_DISCARD, 0, &info);
    if FAILED(hr) {
        qWarning() << "Failed: ID3D10Texture2D::Map: " << QtWin::stringFromHresult(hr);
        return;
    }

    // first, make it black and transparent
    memset(info.pData, 0, 4 * CURSOR_TEX_SIZE * CURSOR_TEX_SIZE);

    // then, fill it with the new cursor
    if (pointer.Type == DXGI_OUTDUPL_POINTER_SHAPE_TYPE_COLOR) {
        for (UINT row = 0; row < std::min(pointer.Height, CURSOR_TEX_SIZE); ++row) {
            std::memcpy((char*)info.pData + row*info.RowPitch, buffer.get() + row*pointer.Pitch, std::min(pointer.Width, CURSOR_TEX_SIZE)*4);
        }
    } else if (pointer.Type == DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MASKED_COLOR) {
        //FIXME: We don't want to read the desktop image back into the CPU, so we apply the mask
        // onto a black background. This is not correct.
        //FIXME: I haven't found a way yet to trigger this codepath at runtime.
        for (UINT row = 0; row < std::min(pointer.Height, CURSOR_TEX_SIZE); ++row) {
            for (UINT col = 0; col < std::min(pointer.Width, CURSOR_TEX_SIZE); ++col) {
                uint8_t *target = &reinterpret_cast<uint8_t*>(info.pData)[row*info.RowPitch + col*4];
                uint8_t *source = &buffer[row*pointer.Pitch + col*4];

                // the mask value doesn't matter because
                //  mask==0     => Use source RGB values
                //  mask==0xFF  => Use source RGB XOR target RGB = source RGB if the target is black
                target[0] = source[0];
                target[1] = source[1];
                target[2] = source[2];
                target[3] = 0xFF;
            }
        }
    } else {
        //FIXME: We don't want to read the desktop image back into the CPU, so we pretend to
        //       apply the AND mask onto a black surface. This is incorrect, but doesn't look too bad.

        uint8_t *and_map = buffer.get();
        uint8_t *xor_map = and_map + pointer.Pitch*pointer.Height/2;

        for (UINT row = 0; row < std::min(pointer.Height/2, CURSOR_TEX_SIZE); ++row)
        {
            uint8_t *and_row = &and_map[row * pointer.Pitch];
            uint8_t *xor_row = &xor_map[row * pointer.Pitch];

            for (UINT col = 0; col < std::min(pointer.Width, CURSOR_TEX_SIZE); ++col) {
                uint8_t *target = &reinterpret_cast<uint8_t*>(info.pData)[row*info.RowPitch + col*4];

                uint8_t alpha = get_pixel_from_row<1>(and_row, col) ? 0 : 0xFF;
                uint8_t rgb   = get_pixel_from_row<1>(xor_row, col) ? 0xFF : 0;

                target[0] = rgb;
                target[1] = rgb;
                target[2] = rgb;
                target[3] = alpha;
            }
        }
    }

    m_context->Unmap(m_cursorTexture, 0);
}

void ScreenViewRenderer::releaseFrame()
{
    if (m_frameAcquired && m_duplication)
        m_duplication->ReleaseFrame();

    m_frameAcquired = false;
}

void ScreenViewRenderer::render()
{
    if (!m_device || !m_renderTarget || !m_context)
        return;

    // acquire and copy desktop texture
    acquireFrame();

    updateDesktop();
    updateCursor();

    updateCursorPosition();

    // draw the scene
    float gray[4] = { 0.5, 0.5, 0.5, 1.0 };
    m_context->ClearRenderTargetView(m_renderTarget, gray);

    UINT stride = sizeof(VERTEX);
    UINT offset = 0;
    m_context->IASetVertexBuffers(0, 1, com::single_item_array(m_desktopVBuffer), &stride, &offset);
    m_context->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_context->PSSetShaderResources(0, 1, com::single_item_array(m_desktopSrv));
    m_context->Draw(6, 0);

    if (m_cursorVisible) {
        m_context->IASetVertexBuffers(0, 1, com::single_item_array(m_cursorVBuffer), &stride, &offset);
        m_context->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_context->PSSetShaderResources(0, 1, com::single_item_array(m_cursorSrv));
        m_context->Draw(6, 0);
    }

    m_swap->Present(1, 0);

    // MSDN recommends releasing the frame as late as possible so that the DWM
    // doesn't spend too much time accumulating frames for us
    releaseFrame();
}

} // namespace Presentation

