#ifndef PRESENTATION_SCREENVIEWRENDERER_H
#define PRESENTATION_SCREENVIEWRENDERER_H

#include <QObject>

#include "util/com.h"
#include "util/misc.h"
#include "util/win32.h"
#include "dxgi1_2.h"
#include "d3d11.h"

class QTimer;

namespace Presentation {

class ScreenViewRenderer : public QObject
{
    Q_OBJECT
public:
    explicit ScreenViewRenderer(HWND window, const QRect &screen, QObject *parent = 0);
    ~ScreenViewRenderer();

    bool good() { return m_isGood; }

signals:
    void stopped();

public slots:
    bool updateScreen(const QRect &screen);
    bool reset();
    void windowResized(const QRect &size);
    void startRendering();
    void stop();

private:
    const UINT CURSOR_TEX_SIZE = 256;

    win32::dll_func<HRESULT (IDXGIAdapter *,
                             D3D_DRIVER_TYPE,
                             HMODULE,
                             UINT,
                             const D3D_FEATURE_LEVEL *,
                             UINT,
                             UINT,
                             const DXGI_SWAP_CHAIN_DESC *,
                             IDXGISwapChain **,
                             ID3D11Device **,
                             D3D_FEATURE_LEVEL *,
                             ID3D11DeviceContext **)> m_d3dCreator { L"d3d11.dll", "D3D11CreateDeviceAndSwapChain" };

    win32::dll_func<HRESULT(IDXGIOutput*,
                            IUnknown*,
                            IDXGIOutputDuplication**)> m_dd4seven_duplicate { L"dd4seven-api.dll", "DuplicateOutput" };

    com::ptr<ID3D11Device>           m_device;
    com::ptr<ID3D11DeviceContext>    m_context;
    com::ptr<IDXGISwapChain>         m_swap;
    com::ptr<ID3D11RenderTargetView> m_renderTarget;
    com::ptr<ID3D11PixelShader>      m_pshader;
    com::ptr<ID3D11VertexShader>     m_vshader;
    com::ptr<ID3D11InputLayout>      m_ilayout;
    com::ptr<ID3D11SamplerState>     m_sampler;
    com::ptr<ID3D11BlendState>       m_blendState;

    com::ptr<ID3D11Texture2D>          m_desktopTexture;
    com::ptr<ID3D11ShaderResourceView> m_desktopSrv;
    com::ptr<ID3D11Texture2D>          m_cursorTexture;
    com::ptr<ID3D11ShaderResourceView> m_cursorSrv;
    com::ptr<ID3D11Buffer>             m_desktopVBuffer;
    com::ptr<ID3D11Buffer>             m_cursorVBuffer;

    com::ptr<IDXGIOutputDuplication> m_duplication;

    bool                     m_frameAcquired = false;
    DXGI_OUTDUPL_FRAME_INFO  m_duplInfo;
    com::ptr<IDXGIResource>  m_duplDesktopImage;

    bool m_cursorVisible = true;
    LONG m_cursorX       = 0;
    LONG m_cursorY       = 0;
    int  m_desktopWidth  = 0;
    int  m_desktopHeight = 0;
    int  m_desktopX      = 0;
    int  m_desktopY      = 0;

    struct VERTEX { float x; float y; float z; float u; float v; };

    bool m_isGood { false };
    bool m_requestStop { false };

    QTimer *m_resetTimer = nullptr;

    bool setupDxgiAndD3DDevice(HWND hwnd);
    bool setupShaders();
    bool setupInputLayout();
    bool setupSamplers();
    bool setupBlendState();
    bool setupDesktopTextureAndVertices();
    bool setupCursorTextureAndVertices();
    void updateCursorPosition();
    void acquireFrame();
    void updateDesktop();
    void updateCursor();
    void releaseFrame();
    void render();
};

} // namespace Presentation

#endif // PRESENTATION_SCREENVIEWRENDERER_H
