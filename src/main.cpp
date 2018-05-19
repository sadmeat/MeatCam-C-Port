#include <windows.h>
#include <winuser.h>
#include <Windef.h>
#include <Windowsx.h>
#include <time.h>
#include <thread>
#include <chrono>

LRESULT CALLBACK WindowProc(HWND, UINT, WPARAM, LPARAM);
void EnableOpenGL(HWND hwnd, HDC*, HGLRC*);
void EnableOpenGLAA(HWND hwnd, HDC*, HGLRC*);
void DisableOpenGL(HWND, HDC, HGLRC);
void CreateFakeContext(HINSTANCE hInstance);
void ProcessMicData();

float width = 0;
float height = 0;
float mouseX = 0;
float mouseY = 0;
float prevvol = 0;
float volume = 0;

bool prevMousePressed = false;
bool mousePressed = false;
bool mouseInWindow = false;

int fps = 120;
float sensitivity = 0.5;
float micThreshold = 0.03;
bool displayHat = true;
bool displayBlink = true;
bool enableAntiAliasing = false;
int antiAliasingSamples = 0;

#define RECORD_BUFFER_SIZE 256
#define FFT_SIZE 128
unsigned int deviceId = 0;
PSHORT  pBufferCopy;
PUSHORT pBufferFFT;
bool threadRun = true;
std::thread audioThread;

#include <stdio.h>
#include "dtft.h"
#include "main.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    WNDCLASSEX wcex;
    HWND hwnd;
    HDC hDC;
    HGLRC hRC;
    MSG msg;
    BOOL bQuit = FALSE;

    sensitivity = (float) GetPrivateProfileInt("Settings", "micSensitivity", 50, ".\\settings.ini") / 100.0;
    micThreshold = (float) GetPrivateProfileInt("Settings", "micThreshold", 30, ".\\settings.ini") / 1000.0;
    displayHat =  GetPrivateProfileInt("Settings", "displayHat", 1, ".\\settings.ini") == 1;
    displayBlink =  GetPrivateProfileInt("Settings", "blink", 1, ".\\settings.ini") == 1;
    fps = GetPrivateProfileInt("Settings", "fps", 120, ".\\settings.ini");
    enableAntiAliasing = GetPrivateProfileInt("Settings", "enableAntiAliasing", 0, ".\\settings.ini") == 1;
    antiAliasingSamples = GetPrivateProfileInt("Settings", "antiAliasingSamples", 0, ".\\settings.ini") == 1;

    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_OWNDC;
    wcex.lpfnWndProc = WindowProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = "MeatCam";
    wcex.hIconSm = LoadIcon(NULL, IDI_APPLICATION);;


    if (!RegisterClassEx(&wcex))
        return 0;

    if(enableAntiAliasing)
        CreateFakeContext(hInstance);

    hwnd = CreateWindowEx(0, "MeatCam", "Meat Cam", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 
                          1000, 800, NULL, NULL, hInstance, NULL);

    ShowWindow(hwnd, nCmdShow);


    if(enableAntiAliasing)
        EnableOpenGLAA(hwnd, &hDC, &hRC);
    else
        EnableOpenGL(hwnd, &hDC, &hRC);

    init();

    clock_t begin_time = clock();
    while (!bQuit)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
                bQuit = TRUE;
            else
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
        else
        {
            clock_t now = clock();
            float delta = float(now-begin_time)/CLOCKS_PER_SEC;

            loop(delta);
            SwapBuffers(hDC);

            if(fps != 0) {
                float s = 1000.0/fps - float(clock()-begin_time)/CLOCKS_PER_SEC;
                Sleep((int) s/2);
            }

            begin_time = now;
        }

        Sleep(1);
    }

    dispose();

    DisableOpenGL(hwnd, hDC, hRC);
    DestroyWindow(hwnd);

    return msg.wParam;
}


LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    RECT rect;
    GetClientRect(hwnd, &rect);
    width = rect.right - rect.left;
    height = rect.bottom - rect.top;

    static HWAVEIN      hWaveIn;
    static PSHORT       pBuffer;
    static PWAVEHDR     pWaveHdr1;
    static WAVEFORMATEX waveform;

    switch(uMsg)
    {
        case WM_CLOSE:
            waveInClose(hWaveIn);
            threadRun = false;
            audioThread.join();
            PostQuitMessage(0);
        break;

        case WM_DESTROY:
            return 0;

        case WM_KEYDOWN:{
            switch(wParam){
                case VK_ESCAPE:
                    waveInClose (hWaveIn);
                    threadRun = false;
                    audioThread.join();
                    PostQuitMessage(0);
                break;
            }
        break;}


        case WM_LBUTTONDOWN:
            mousePressed = true;
        case WM_MOUSEMOVE:
            if(!mouseInWindow) {
                TRACKMOUSEEVENT tme;
                tme.cbSize = sizeof(TRACKMOUSEEVENT);
                tme.dwFlags = TME_LEAVE;
                tme.hwndTrack = hwnd;
                TrackMouseEvent(&tme);
            }
            mouseInWindow = true;
            mouseX = (float) GET_X_LPARAM(lParam);
            mouseY = (float) GET_Y_LPARAM(lParam);
        break;

        case WM_LBUTTONUP:
            mousePressed = false;
        break;

        case WM_MOUSELEAVE:
            mouseInWindow = false;
        break;

        case WM_CREATE: {
            CREATESTRUCT *pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);

            if(strcmp(pCreate->lpszName, "Fake")) {
                pWaveHdr1 =  reinterpret_cast <PWAVEHDR>(malloc (sizeof (WAVEHDR)));
                pBuffer = reinterpret_cast <PSHORT>(malloc(RECORD_BUFFER_SIZE*sizeof(SHORT)));
                pBufferCopy = reinterpret_cast <PSHORT>(malloc(RECORD_BUFFER_SIZE*sizeof(SHORT)));
                pBufferFFT = reinterpret_cast <PUSHORT>(malloc(FFT_SIZE*sizeof(SHORT)));

                cout << "--------------- Meat Cam V4 -------------------" << endl << endl;
                cout << "  Which microphone do you want to use?" << endl << endl;
                UINT devs = waveInGetNumDevs();
                for (UINT dev = 0; dev < devs; dev++) {
                    WAVEINCAPS caps = {};
                    MMRESULT mmr = waveInGetDevCaps(dev, &caps, sizeof(caps));

                    if (MMSYSERR_NOERROR != mmr) {
                        cout << "waveInGetDevCaps failed: mmr = " << mmr << endl;
                        return 0;
                    }
                    cout << "  #" << dev << ": " << caps.szPname << endl;
                }
                cout << endl << "  >> #";

                do {
                    cin >> deviceId;
                    if(deviceId >= devs || deviceId < 0) {
                        cout << "  #" << deviceId << " is a wrong input" << endl << endl << "  >> #";
                    }
                } while (deviceId >= devs || deviceId < 0);

                WAVEINCAPS caps = {};
                waveInGetDevCaps(deviceId, &caps, sizeof(caps));

                cout << endl << "  \"" << caps.szPname << "\" selected!" << endl << endl;
                cout << "-----------------------------------------------" << endl;
                Sleep(500);


                AllocConsole();
                HWND Stealth = FindWindowA("ConsoleWindowClass", NULL);
                ShowWindow(Stealth,0);
                buildSamplePoints();
                audioThread = thread(ProcessMicData);

                waveInReset (hWaveIn);

                waveform.wFormatTag      = WAVE_FORMAT_PCM;
                waveform.nChannels       = 1;
                waveform.nSamplesPerSec  = 44100;
                waveform.nAvgBytesPerSec = 44100;
                waveform.nBlockAlign     = 2;
                waveform.wBitsPerSample  = 16;
                waveform.cbSize          = 0;
                waveInOpen(&hWaveIn, deviceId, &waveform, (DWORD) hwnd, 0, CALLBACK_WINDOW | WAVE_FORMAT_DIRECT);
                //waveInOpen(&hWaveIn, deviceId, &waveform, (DWORD) hwnd, 0, CALLBACK_WINDOW);

                pWaveHdr1->lpData          = reinterpret_cast <CHAR*>(pBuffer);
                pWaveHdr1->dwBufferLength  = RECORD_BUFFER_SIZE*sizeof(SHORT);
                pWaveHdr1->dwBytesRecorded = 0;
                pWaveHdr1->dwUser          = 0;
                pWaveHdr1->dwFlags         = 0;
                pWaveHdr1->dwLoops         = 1;
                pWaveHdr1->lpNext          = NULL;
                pWaveHdr1->reserved        = 0;
                waveInPrepareHeader(hWaveIn, pWaveHdr1, sizeof(WAVEHDR));
            }

        break;}

        case MM_WIM_OPEN:
            waveInAddBuffer(hWaveIn, pWaveHdr1, sizeof(WAVEHDR));
            waveInStart(hWaveIn);
        break;

        case MM_WIM_DATA: {
            memcpy(pBufferCopy, pBuffer, RECORD_BUFFER_SIZE*sizeof(SHORT));
            waveInAddBuffer (hWaveIn, (PWAVEHDR) lParam, sizeof(WAVEHDR));
        break;}

        case MM_WIM_CLOSE:
            cout << "close" << endl;
            waveInUnprepareHeader(hWaveIn, pWaveHdr1, sizeof(WAVEHDR));
            free(pBuffer);
            free(pBufferCopy);
            free(pBufferFFT);
        break;

        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    return 0;
}


void EnableOpenGL(HWND hwnd, HDC* hDC, HGLRC* hRC)
{
    PIXELFORMATDESCRIPTOR pfd;

    int iFormat;

    *hDC = GetDC(hwnd);

    ZeroMemory(&pfd, sizeof(pfd));

    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 24;
    pfd.cDepthBits = 16;
    pfd.iLayerType = PFD_MAIN_PLANE;

    iFormat = ChoosePixelFormat(*hDC, &pfd);

    SetPixelFormat(*hDC, iFormat, &pfd);

    *hRC = wglCreateContext(*hDC);

    wglMakeCurrent(*hDC, *hRC);

    glewInit();
}

void CreateFakeContext(HINSTANCE hInstance) {
    HWND fakeWND = CreateWindow("MeatCam", "Fake", WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
        0, 0, 1, 1, NULL, NULL, hInstance, NULL);

    HDC hDC = GetDC(fakeWND);
    PIXELFORMATDESCRIPTOR pfd;
    SetPixelFormat(hDC, ChoosePixelFormat(hDC, &pfd), &pfd);
    HGLRC hRC = wglCreateContext(hDC);
    wglMakeCurrent(hDC, hRC);
    glewInit();
    wglDeleteContext(hRC);
    ReleaseDC(fakeWND, hDC);
    DestroyWindow(fakeWND);
}


void EnableOpenGLAA(HWND hwnd, HDC* hDC, HGLRC* hRC)
{
    PIXELFORMATDESCRIPTOR pfd;
    int pixelFormat;

    *hDC = GetDC(hwnd);

    ZeroMemory(&pfd, sizeof(pfd));

    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cAlphaBits = 8;
    pfd.cDepthBits = 24;
    pfd.iLayerType = PFD_MAIN_PLANE;

    UINT numFormats;
    float fAttributes[] = {0,0};
    int iAttributes[] = { WGL_DRAW_TO_WINDOW_ARB,GL_TRUE,
        WGL_SUPPORT_OPENGL_ARB,GL_TRUE,
        WGL_COLOR_BITS_ARB,32,
        WGL_ALPHA_BITS_ARB,8,
        WGL_DEPTH_BITS_ARB,24,
        WGL_STENCIL_BITS_ARB,0,
        WGL_DOUBLE_BUFFER_ARB,GL_TRUE,
        WGL_SAMPLE_BUFFERS_ARB,GL_TRUE,
        WGL_SAMPLES_ARB, antiAliasingSamples,
        0,0};

    pixelFormat = ChoosePixelFormat(*hDC, &pfd);
    wglChoosePixelFormatARB(*hDC,iAttributes,fAttributes,1,&pixelFormat,&numFormats);

    SetPixelFormat(*hDC, pixelFormat, &pfd);

    *hRC = wglCreateContext(*hDC);
    wglMakeCurrent(*hDC, *hRC);
}



void DisableOpenGL(HWND hwnd, HDC hDC, HGLRC hRC)
{
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(hRC);
    ReleaseDC(hwnd, hDC);
}

void ProcessMicData()
{
    while(threadRun) {
        dtft(pBufferCopy, pBufferFFT);

        float mean = 0;
        for(unsigned int i=0; i<FFT_SIZE; i++)
            mean += (float) pBufferFFT[i]*pBufferFFT[i] / FFT_SIZE;
        mean = glm::sqrt(mean)/SHRT_MAX;
        prevvol = prevvol * 0.7 + 0.3 * mean;
        volume = glm::pow(glm::clamp((prevvol-micThreshold)/(1.0f-micThreshold), 0.0f, 1.0f), 1-sensitivity);

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

