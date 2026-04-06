
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <stdio.h>
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <string>

#pragma comment(linker, "/entry:WinMainCRTStartup /subsystem:console")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")


struct VideoConfig
{
  int Width = 800;
  int Height = 600;
  bool IsFullscreen = false;
  bool NeedsResize = false;
  int VSync = 1;
} g_Config;


// --- [전역 객체 관리] ---
// DirectX 객체들은 GPU 메모리를 직접 사용함. 
// 사용 후 'Release()'를 호출하지 않으면 프로그램 종료 후에도 메모리가 점유될 수 있음(메모리 누수).
ID3D11Device* g_pd3dDevice = nullptr;          // 리소스 생성자 (공장)
ID3D11DeviceContext* g_pImmediateContext = nullptr;   // 그리기 명령 수행 (일꾼)
IDXGISwapChain* g_pSwapChain = nullptr;          // 화면 전환 (더블 버퍼링)
ID3D11RenderTargetView* g_pRenderTargetView = nullptr;   // 그림을 그릴 도화지(View)
ID3D11Buffer* pVBuffer = nullptr;
ID3D11VertexShader* vShader = nullptr;
ID3D11PixelShader* pShader = nullptr;
ID3D11InputLayout* pInputLayout = nullptr;
MSG msg = { 0 };
HWND hWnd = nullptr;

struct Vertex {
  float x, y, z;
  float r, g, b, a;
};


class Component
{
public:
  class GameObject* pOwner = nullptr; // 이 기능이 누구의 것인지 저장
  bool isStarted = 0;           // Start()가 실행되었는지 체크

  virtual void Start() = 0;              // 초기화
  virtual void Input() {}                // 입력 (선택사항)
  virtual void Update(float dt) = 0;     // 로직 (필수)
  virtual void Render() {}               // 그리기 (선택사항)

  virtual ~Component() {}
};

void RebuildVideoResources(HWND hWnd)
{
  if (!g_pSwapChain) return;

  // 1. 기존 렌더 타겟 뷰 해제 (안 하면 ResizeBuffers 실패함)
  if (g_pRenderTargetView)
  {
    g_pRenderTargetView->Release();
    g_pRenderTargetView = nullptr;
  }

  // 2. 백버퍼 크기 재설정
  g_pSwapChain->ResizeBuffers(0, g_Config.Width, g_Config.Height, DXGI_FORMAT_UNKNOWN, 0);

  // 3. 새 백버퍼로부터 렌더 타겟 뷰 다시 생성
  ID3D11Texture2D* pBackBuffer = nullptr;
  g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
  if (pBackBuffer == nullptr)
  {
    printf("GETBUFFER ERROR\n");
    return;
  }
  g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTargetView);
  pBackBuffer->Release();

  // 4. 윈도우 창 크기 실제 조정 (전체화면이 아닐 때만)
  if (!g_Config.IsFullscreen)
  {
    RECT rc = { 0, 0, g_Config.Width, g_Config.Height };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    SetWindowPos(hWnd, nullptr, 0, 0, rc.right - rc.left, rc.bottom - rc.top, SWP_NOMOVE | SWP_NOZORDER);
  }

  g_Config.NeedsResize = false;
  printf("[Video] Changed: %d x %d\n", g_Config.Width, g_Config.Height);
}

class GameObject {
public:
  std::string name;
  std::vector<Component*> components;

  GameObject(std::string n)
  {
    name = n;
  }

  // 객체가 죽을 때 담고 있던 컴포넌트들도 메모리에서 해제함
  ~GameObject() {
    for (int i = 0; i < (int)components.size(); i++)
    {
      delete components[i];
    }
  }



  // 새로운 기능을 추가하는 함수
  void AddComponent(Component* pComp)
  {
    pComp->pOwner = this;
    pComp->isStarted = false;
    components.push_back(pComp);
  }
};

class PlayerControl : public Component {
public:
  float x, y, speed;
  bool moveUp, moveDown, moveLeft, moveRight;
  int playerType = 0;

  float cr, cg, cb;

  PlayerControl(int type, float r, float g, float b)
    : playerType(type), cr(r), cg(g), cb(b) {
  }

  void Start() override
  {
    x = 0.0f; y = 0.0f; speed = 0.5f;
    moveUp = moveDown = moveLeft = moveRight = false;
  }

  // [입력 단계] 키 상태만 체크함
  void Input() override
  {
    if (playerType == 0)
    {
      moveUp = (GetAsyncKeyState('W') & 0x8000);
      moveDown = (GetAsyncKeyState('S') & 0x8000);
      moveLeft = (GetAsyncKeyState('A') & 0x8000);
      moveRight = (GetAsyncKeyState('D') & 0x8000);
    }
    if (playerType == 1)
    {
      moveUp = (GetAsyncKeyState(VK_UP) & 0x8000);
      moveDown = (GetAsyncKeyState(VK_DOWN) & 0x8000);
      moveLeft = (GetAsyncKeyState(VK_LEFT) & 0x8000);
      moveRight = (GetAsyncKeyState(VK_RIGHT) & 0x8000);
    }


  }

  // [업데이트 단계] 체크된 키 상태로 좌표만 계산함
  void Update(float dt) override
  {
    if (moveUp)    y += speed * dt;
    if (moveDown)  y -= speed * dt;
    if (moveLeft)  x -= speed * dt;
    if (moveRight) x += speed * dt;

  }

  // [렌더링 단계] 계산된 좌표를 화면에 그림
  void Render() override
  {
    // 실제 엔진이라면 여기서 DirectX Draw를 부름
    // 지금은 좌표 시각화로 대체

    Vertex vertices[] = {
           {  x + 0.0f,y + 0.5f,  0.5f,cr, cg, cb, 1.0f  },
       {  x + 0.32475f, y - 0.25f, 0.5f,cr, cg, cb, 1.0f},
       {x - 0.32475f, y - 0.25f, 0.5f, cr, cg, cb, 1.0f},
    };
    D3D11_BUFFER_DESC bd = { sizeof(vertices), D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0 };
    D3D11_SUBRESOURCE_DATA initData = { vertices, 0, 0 };
    g_pd3dDevice->CreateBuffer(&bd, &initData, &pVBuffer);

    if (GetAsyncKeyState('F') & 0x0001) {
      g_Config.IsFullscreen = !g_Config.IsFullscreen;
      g_pSwapChain->SetFullscreenState(g_Config.IsFullscreen, nullptr);
    }
    if (GetAsyncKeyState('1') & 0x0001) { g_Config.Width = 800; g_Config.Height = 600; g_Config.NeedsResize = true; }
    if (GetAsyncKeyState('2') & 0x0001) { g_Config.Width = 1280; g_Config.Height = 720; g_Config.NeedsResize = true; }


    g_pImmediateContext->IASetInputLayout(pInputLayout);
    UINT stride = sizeof(Vertex), offset = 0;
    g_pImmediateContext->IASetVertexBuffers(0, 1, &pVBuffer, &stride, &offset);
    g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    g_pImmediateContext->VSSetShader(vShader, nullptr, 0);
    g_pImmediateContext->PSSetShader(pShader, nullptr, 0);
    g_pImmediateContext->Draw(3, 0);





  }

};


class GameLoop
{
public:
  bool isRunning;
  std::vector<GameObject*> gameWorld;
  std::chrono::high_resolution_clock::time_point prevTime;
  float deltaTime;   //delta time;

  //초기화
  void Initialize()
  {
    //초기화시 동작준비됨
    isRunning = true;

    gameWorld.clear();

    // 시간 측정 준비
    prevTime = std::chrono::high_resolution_clock::now();
    deltaTime = 0.0f;


  }

  void Input()
  {
    // esc 누르면 종료
    if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) isRunning = false;

    // B. 입력 단계 (Input Phase)
    for (int i = 0; i < (int)gameWorld.size(); i++)
    {
      for (int j = 0; j < (int)gameWorld[i]->components.size(); j++)
      {
        gameWorld[i]->components[j]->Input();
      }
    }
  }

  void Update()
  {
    // C. 스타트 실행
    for (int i = 0; i < (int)gameWorld.size(); i++)
    {
      for (int j = 0; j < (int)gameWorld[i]->components.size(); j++)
      {
        // Start()가 호출된 적 없다면 여기서 호출 (유니티 방식)
        if (gameWorld[i]->components[j]->isStarted == false)
        {
          gameWorld[i]->components[j]->Start();
          gameWorld[i]->components[j]->isStarted = true;
        }
      }
    }

    // D. 업데이트 단계 (Update Phase)
    for (int i = 0; i < (int)gameWorld.size(); i++)
    {
      for (int j = 0; j < (int)gameWorld[i]->components.size(); j++)
      {
        gameWorld[i]->components[j]->Update(deltaTime);
      }
    }
  }

  void Render()
  {
    // E. 렌더링 단계 (Render Phase)
    float clearColor[] = { 0.1f, 0.2f, 0.3f, 1.0f };
    g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, clearColor);
    g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);

    D3D11_VIEWPORT vp = { 0, 0, (float)g_Config.Width, (float)g_Config.Height, 0.0f, 1.0f };
    g_pImmediateContext->RSSetViewports(1, &vp);
    for (int i = 0; i < (int)gameWorld.size(); i++)
    {
      for (int j = 0; j < (int)gameWorld[i]->components.size(); j++)
      {
        gameWorld[i]->components[j]->Render();
      }
    }
    if (g_Config.NeedsResize) RebuildVideoResources(hWnd);
    g_pSwapChain->Present(g_Config.VSync, 0);
  }



  void Run()
  {
    // --- [무한 게임 루프] ---
    while (WM_QUIT != msg.message) {
      // (1) 입력 단계: PeekMessage는 메시지가 없어도 바로 리턴함 (Non-blocking)
      if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }
      else {
        while (isRunning) {

          // A. 시간 관리 (DeltaTime 계산)
          std::chrono::high_resolution_clock::time_point currentTime = std::chrono::high_resolution_clock::now();
          std::chrono::duration<float> elapsed = currentTime - prevTime;
          deltaTime = elapsed.count();
          prevTime = currentTime;

          Input();
          Update();
          Render();

          // CPU 과부하 방지 (약 60~100 FPS 유지 시도)
          std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

      }
    }

  }

  GameLoop()
  {
    Initialize();
  }
  ~GameLoop()
  {
    // [정리] 메모리 해제
    for (int i = 0; i < (int)gameWorld.size(); i++)
    {
      delete gameWorld[i]; // GameObject 소멸자가 컴포넌트들도 지움
    }
  }

};



// HLSL (High-Level Shading Language) 소스
const char* shaderSource = R"(
struct VS_INPUT { float3 pos : POSITION; float4 col : COLOR; };
struct PS_INPUT { float4 pos : SV_POSITION; float4 col : COLOR; };

PS_INPUT VS(VS_INPUT input) {
    PS_INPUT output;
    output.pos = float4(input.pos, 1.0f); // 3D 좌표를 4D로 확장
    output.col = input.col;
    return output;
}

float4 PS(PS_INPUT input) : SV_Target {
    return input.col; // 정점에서 계산된 색상을 픽셀에 그대로 적용
}
)";



LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
  if (message == WM_DESTROY) { PostQuitMessage(0); return 0; }
  return DefWindowProc(hWnd, message, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
  // 1. 윈도우 등록 및 생성
  WNDCLASSEXW wcex = { sizeof(WNDCLASSEX) };
  wcex.lpfnWndProc = WndProc;
  wcex.hInstance = hInstance;
  wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wcex.lpszClassName = L"DX11GameLoopClass";
  RegisterClassExW(&wcex);

  hWnd = CreateWindowW(L"DX11GameLoopClass", L"과제: 움직이는 육망성 만들기",
    WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 800, 600, nullptr, nullptr, hInstance, nullptr);
  if (!hWnd) return -1;
  ShowWindow(hWnd, nCmdShow);

  // 2. DX11 디바이스 및 스왑 체인 초기화
  DXGI_SWAP_CHAIN_DESC sd = {};
  sd.BufferCount = 1;
  sd.BufferDesc.Width = 800; sd.BufferDesc.Height = 600;
  sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  sd.OutputWindow = hWnd;
  sd.SampleDesc.Count = 1;
  sd.Windowed = TRUE;

  // GPU와 통신할 통로(Device)와 화면(SwapChain)을 생성함.
  D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
    D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, nullptr, &g_pImmediateContext);

  // 렌더 타겟 설정 (도화지 준비)
  ID3D11Texture2D* pBackBuffer = nullptr;
  g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
  g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTargetView);
  pBackBuffer->Release(); // 뷰를 생성했으므로 원본 텍스트는 바로 해제 (중요!)

  RebuildVideoResources(hWnd);

  // 3. 셰이더 컴파일 및 생성
  ID3DBlob* vsBlob, * psBlob;
  D3DCompile(shaderSource, strlen(shaderSource), nullptr, nullptr, nullptr, "VS", "vs_4_0", 0, 0, &vsBlob, nullptr);
  D3DCompile(shaderSource, strlen(shaderSource), nullptr, nullptr, nullptr, "PS", "ps_4_0", 0, 0, &psBlob, nullptr);


  g_pd3dDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vShader);
  g_pd3dDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &pShader);

  D3D11_INPUT_ELEMENT_DESC layout[] = {
      { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
      { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
  };

  g_pd3dDevice->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &pInputLayout);
  vsBlob->Release(); psBlob->Release();




  GameLoop gLoop;
  gLoop.Initialize();


  // 플레이어 객체 조립
  GameObject* player1 = new GameObject("Player1");
  player1->AddComponent(new PlayerControl(0, 1.0f, 0.3f, 0.3f));
  gLoop.gameWorld.push_back(player1);

  // 플레이어 객체 조립
  GameObject* player2 = new GameObject("Player2");
  player2->AddComponent(new PlayerControl(1, 0.3f, 0.3f, 1.0f));
  gLoop.gameWorld.push_back(player2);


  //게임루프 실행
  gLoop.Run();


  // --- [6. 자원 해제 (Release)] ---
  // 생성(Create)한 모든 객체는 프로그램 종료 전 반드시 Release 해야 함.
  // 생성의 역순으로 해제하는 것이 관례임.
  if (pVBuffer) pVBuffer->Release();
  if (pInputLayout) pInputLayout->Release();
  if (vShader) vShader->Release();
  if (pShader) pShader->Release();
  if (g_pRenderTargetView) g_pRenderTargetView->Release();
  if (g_pSwapChain) g_pSwapChain->Release();
  if (g_pImmediateContext) g_pImmediateContext->Release();
  if (g_pd3dDevice) g_pd3dDevice->Release();

  return (int)msg.wParam;
}