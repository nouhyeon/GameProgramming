#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <stdio.h>

#pragma comment(linker, "/entry:WinMainCRTStartup /subsystem:console")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

// --- [전역 변수: 렌더링 파이프라인 자원] ---
ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pImmediateContext = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
ID3D11RenderTargetView* g_pRenderTargetView = nullptr;

// Render() 함수에서 접근할 수 있도록 버퍼와 셰이더를 전역 변수로 승격합니다.
ID3D11Buffer* g_pVBuffer = nullptr;
ID3D11InputLayout* g_pInputLayout = nullptr;
ID3D11VertexShader* g_pVShader = nullptr;
ID3D11PixelShader* g_pPixelShader = nullptr;

typedef struct {
  float playerPosX;
  float playerPosY;
  int isRunning;
  int isMove;
} GameContext;

GameContext game = { 0.0f, 0.0f, 1, 0 };

struct Vertex {
  float x, y, z;
  float r, g, b, a;
};

const Vertex g_originalVertices[6] = {
  {  0.0f, 0.5f,  0.5f,1.0f, 0.0f, 0.0f, 1.0f  },
    {  0.32475f, -0.25f, 0.5f,0.0f, 1.0f, 0.0f, 1.0f},
    { -0.32475f, -0.25f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f},
    {  0.0f, -0.5f,  0.5f, 1.0f, 0.0f, 0.0f, 1.0f },
    { -0.32475f, 0.25f, 0.5f, 0.0f, 1.0f, 0.0f, 1.0f},
    {  0.32475f, 0.25f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f  },
};

const char* shaderSource = R"(
struct VS_INPUT { float3 pos : POSITION; float4 col : COLOR; };
struct PS_INPUT { float4 pos : SV_POSITION; float4 col : COLOR; };

PS_INPUT VS(VS_INPUT input) {
    PS_INPUT output;
    output.pos = float4(input.pos, 1.0f);
    output.col = input.col;
    return output;
}

float4 PS(PS_INPUT input) : SV_Target {
    return input.col; 
}
)";


void Update(GameContext* ctx) {

  ctx->isMove = 0;
  if (GetAsyncKeyState(VK_LEFT) & 0x8000) { ctx->playerPosX -= 0.005f; ctx->isMove = 1; }
  if (GetAsyncKeyState(VK_RIGHT) & 0x8000) { ctx->playerPosX += 0.005f; ctx->isMove = 1; }
  if (GetAsyncKeyState(VK_UP) & 0x8000) { ctx->playerPosY += 0.005f; ctx->isMove = 1; }
  if (GetAsyncKeyState(VK_DOWN) & 0x8000) { ctx->playerPosY -= 0.005f; ctx->isMove = 1; }
}

void Render(GameContext* ctx) {

  Vertex currentVertices[6];
  for (int i = 0; i < 6; i++) {
    currentVertices[i] = g_originalVertices[i];
    currentVertices[i].x += ctx->playerPosX;
    currentVertices[i].y += ctx->playerPosY;
  }


  g_pImmediateContext->UpdateSubresource(g_pVBuffer, 0, nullptr, currentVertices, 0, 0);


  float clearColor[] = { 0.1f, 0.2f, 0.3f, 1.0f };
  g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, clearColor);


  g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);
  D3D11_VIEWPORT vp = { 0, 0, 800, 600, 0.0f, 1.0f };
  g_pImmediateContext->RSSetViewports(1, &vp);

  g_pImmediateContext->IASetInputLayout(g_pInputLayout);
  UINT stride = sizeof(Vertex), offset = 0;
  g_pImmediateContext->IASetVertexBuffers(0, 1, &g_pVBuffer, &stride, &offset);
  g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  g_pImmediateContext->VSSetShader(g_pVShader, nullptr, 0);
  g_pImmediateContext->PSSetShader(g_pPixelShader, nullptr, 0);


  g_pImmediateContext->Draw(6, 0);


  g_pSwapChain->Present(0, 0);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
  switch (message)
  {
  case WM_DESTROY:
    printf("[SYSTEM] 윈도우 파괴 메시지 수신. 루프를 탈출합니다.\n");
    PostQuitMessage(0);
    break;
  default:
    return DefWindowProc(hWnd, message, wParam, lParam);
  }
  return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {

  WNDCLASSEXW wcex = { sizeof(WNDCLASSEX) };
  wcex.lpfnWndProc = WndProc;
  wcex.hInstance = hInstance;
  wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wcex.lpszClassName = L"DX11GameLoopClass";
  RegisterClassExW(&wcex);

  HWND hWnd = CreateWindowW(L"DX11GameLoopClass", L"과제: 움직이는 육망성 만들기",
    WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 800, 600, nullptr, nullptr, hInstance, nullptr);
  if (!hWnd) return -1;
  ShowWindow(hWnd, nCmdShow);


  DXGI_SWAP_CHAIN_DESC sd = {};
  sd.BufferCount = 1;
  sd.BufferDesc.Width = 800; sd.BufferDesc.Height = 600;
  sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  sd.OutputWindow = hWnd;
  sd.SampleDesc.Count = 1;
  sd.Windowed = TRUE;

  D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
    D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, nullptr, &g_pImmediateContext);

  ID3D11Texture2D* pBackBuffer = nullptr;
  g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
  g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTargetView);
  pBackBuffer->Release();


  ID3DBlob* vsBlob, * psBlob;
  D3DCompile(shaderSource, strlen(shaderSource), nullptr, nullptr, nullptr, "VS", "vs_4_0", 0, 0, &vsBlob, nullptr);
  D3DCompile(shaderSource, strlen(shaderSource), nullptr, nullptr, nullptr, "PS", "ps_4_0", 0, 0, &psBlob, nullptr);

  g_pd3dDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &g_pVShader);
  g_pd3dDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &g_pPixelShader);

  D3D11_INPUT_ELEMENT_DESC layout[] = {
      { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
      { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
  };
  g_pd3dDevice->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &g_pInputLayout);
  vsBlob->Release(); psBlob->Release();

  D3D11_BUFFER_DESC bd = { sizeof(g_originalVertices), D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0 };
  D3D11_SUBRESOURCE_DATA initData = { g_originalVertices, 0, 0 };
  g_pd3dDevice->CreateBuffer(&bd, &initData, &g_pVBuffer);


  MSG msg = { 0 };

  while (WM_QUIT != msg.message) {
    if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
    else {
      if (game.isRunning) {
        Update(&game);
        Render(&game);
      }
    }
  }


  if (g_pVBuffer) g_pVBuffer->Release();
  if (g_pInputLayout) g_pInputLayout->Release();
  if (g_pVShader) g_pVShader->Release();
  if (g_pPixelShader) g_pPixelShader->Release();
  if (g_pRenderTargetView) g_pRenderTargetView->Release();
  if (g_pSwapChain) g_pSwapChain->Release();
  if (g_pImmediateContext) g_pImmediateContext->Release();
  if (g_pd3dDevice) g_pd3dDevice->Release();

  return (int)msg.wParam;
}