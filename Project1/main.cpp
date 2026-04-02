#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <stdio.h>
#include <chrono>
#include <thread>
#include <vector>
#include <string>

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

struct VideoConfig
{
    int Width = 800;
    int Height = 600;
    bool IsFullscreen = false;
    bool NeedsResize = false;
    int VSync = 1; 
} g_Config;

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

class CPPGameTimer {
public:
    CPPGameTimer() {
        // 현재 시점으로 초기화
        prevTime = std::chrono::steady_clock::now();
    }

    // 매 프레임마다 호출하여 델타 타임을 반환함.
    float Update() {
        // 1. 현재 시점 기록
        auto currentTime = std::chrono::steady_clock::now();

        /*
         * [C++ 스타일 강의 포인트 2: Duration 계산]
         * - 두 시점(time_point)을 빼면 기간(duration)이 나옴.
         * - duration_cast를 통해 우리가 원하는 단위(초, 밀리초 등)로 변환함.
         */
        std::chrono::duration<float> frameTime = currentTime - prevTime;

        // 2. 계산된 시간 간격을 멤버 변수에 저장
        deltaTime = frameTime.count();

        // 3. 이전 시점 갱신
        prevTime = currentTime;

        return deltaTime;
    }

    float GetDeltaTime() const { return deltaTime; }

private:
    std::chrono::steady_clock::time_point prevTime;
    float deltaTime = 0.0f;
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

// [2단계: 게임 오브젝트 클래스]
// 컴포넌트들을 담는 바구니 역할을 함.
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

// --- [3단계: 실제 구현할 기능 컴포넌트들] ---

// 기능 1: 플레이어 조종 및 이동
class PlayerControl : public Component {
public:
    float x, y, speed;
    bool moveUp, moveDown, moveLeft, moveRight;

    void Start() override
    {
        x = 50.0f; y = 50.0f; speed = 150.0f;
        moveUp = moveDown = moveLeft = moveRight = false;
        printf("[%s] PlayerControl 기능 시작!\n", pOwner->name.c_str());
    }

    // [입력 단계] 키 상태만 체크함
    void Input() override
    {
        moveUp = (GetAsyncKeyState('W') & 0x8000);
        moveDown = (GetAsyncKeyState('S') & 0x8000);
        moveLeft = (GetAsyncKeyState('A') & 0x8000);
        moveRight = (GetAsyncKeyState('D') & 0x8000);
    }

    // [업데이트 단계] 체크된 키 상태로 좌표만 계산함
    void Update(float dt) override
    {
        if (moveUp)    y -= speed * dt;
        if (moveDown)  y += speed * dt;
        if (moveLeft)  x -= speed * dt;
        if (moveRight) x += speed * dt;
    }

    // [렌더링 단계] 계산된 좌표를 화면에 그림
    void Render() override
    {
        // 실제 엔진이라면 여기서 DirectX Draw를 부름
        // 지금은 좌표 시각화로 대체

       


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
                    gameWorld[i]->components[j]->Start(); // 객체가 살아난 다음 딱 한번만 되게 
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
        system("cls");
        for (int i = 0; i < (int)gameWorld.size(); i++)
        {
            for (int j = 0; j < (int)gameWorld[i]->components.size(); j++)
            {
                gameWorld[i]->components[j]->Render();
            }
        }
    }



    void Run()
    {
        // --- [무한 게임 루프] ---
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

  RebuildVideoResources(hWnd);


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
  CPPGameTimer timer;

  while (WM_QUIT != msg.message) {
    if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
    else {
        float dt = timer.Update();
        if (GetAsyncKeyState('F') & 0x0001) {
            g_Config.IsFullscreen = !g_Config.IsFullscreen;
            g_pSwapChain->SetFullscreenState(g_Config.IsFullscreen, nullptr);
        }
        if (GetAsyncKeyState('1') & 0x0001) { g_Config.Width = 800; g_Config.Height = 600; g_Config.NeedsResize = true; }
        if (GetAsyncKeyState('2') & 0x0001) { g_Config.Width = 1280; g_Config.Height = 720; g_Config.NeedsResize = true; }

        // [해상도 변경 적용]
        if (g_Config.NeedsResize) RebuildVideoResources(hWnd);
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