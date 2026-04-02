// 방향키 분리 시킨 것

#include <iostream>
#include <chrono>
#include <thread>
#include <windows.h>
#include <vector>
#include <string>

// 콘솔의 특정 좌표로 커서를 이동시키는 함수
void MoveCursor(int x, int y)
{
    // \033[y;xH  (y와 x는 1부터 시작함)
    printf("\033[%d;%dH", y, x);
}

// [1단계: 컴포넌트 기저 클래스]
// 모든 기능(이동, 렌더링 등)은 이 클래스를 상속받아야 함.
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
class PlayerControl2 : public Component {
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
        moveUp = (GetAsyncKeyState(VK_UP) & 0x8000);
        moveDown = (GetAsyncKeyState(VK_DOWN) & 0x8000);
        moveLeft = (GetAsyncKeyState(VK_LEFT) & 0x8000);
        moveRight = (GetAsyncKeyState(VK_RIGHT) & 0x8000);
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

        if (x < 10.0f)
            x = 10.0f;
        if (y < 45.0f)
            y = 45.0f;

        int py = (int)(y / 15.0f);
        int px = (int)(x / 10.0f);


        MoveCursor(px, py);
        printf("☆");
    }
};

class PlayerControl1 : public Component {
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

        if (x < 10.0f)
            x = 10.0f;
        if (y < 45.0f)
            y = 45.0f;

        int py = (int)(y / 15.0f);
        int px = (int)(x / 10.0f);


        MoveCursor(px, py);
        printf("★");
    }
};

// 기능 2: 시스템 정보 출력 (위치 정보 없음)
class InfoDisplay : public Component
{
public:
    float totalTime = 0.0f;

    void Start() override
    {
        totalTime = 0.0f;
        printf("[%s] InfoDisplay 기능 시작!\n", pOwner->name.c_str());
    }

    void Update(float dt) override
    {
        totalTime += dt;
    }

    void Render() override {
        // 화면 최상단에 정보 출력
        MoveCursor(0, 0);
        printf("System Time: %.2f sec\n", totalTime);
        printf("Control: W, A, S, D | Exit: ESC\n");
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

// --- [4단계: 메인 엔진 루프] ---
int main()
{
    //게임루프
    GameLoop gLoop;
    gLoop.Initialize();

    // 시스템 정보 객체 조립
    GameObject* sysInfo = new GameObject("SystemManager");
    InfoDisplay* pInfo = new InfoDisplay();
    sysInfo->AddComponent(pInfo);
    gLoop.gameWorld.push_back(sysInfo);

    // 플레이어 객체 조립
    GameObject* player1 = new GameObject("Player1");
    GameObject* player2 = new GameObject("Player2");
    PlayerControl1* pControl1 = new PlayerControl1();
    PlayerControl2* pControl2 = new PlayerControl2();
    player1->AddComponent(pControl1);
    player2->AddComponent(pControl2);
    gLoop.gameWorld.push_back(player1);
    gLoop.gameWorld.push_back(player2);

    //게임루프 실행
    gLoop.Run();


    //메모리 해제 안 한 것 해야함

    return 0;
}