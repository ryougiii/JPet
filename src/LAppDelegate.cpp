﻿/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#include "LAppDelegate.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <queue>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <windows.h>
#include <WinUser.h>
#include <shellapi.h>
#include <CommCtrl.h>
#include <VersionHelpers.h>

#include "resource.h"
#include "LAppView.hpp"
#include "LAppPal.hpp"
#include "LAppDefine.hpp"
#include "LAppLive2DManager.hpp"
#include "LAppTextureManager.hpp"
#include "TouchManager.hpp"
#include "PartStateManager.h"
#include "ini.h"
#include "StateMessage.hpp"

#define WM_IAWENTRAY WM_USER + 5

using namespace Csm;
using namespace std;
using namespace LAppDefine;
using namespace WinToastLib;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK PreWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
WNDPROC DefaultProc;

namespace
{
    LAppDelegate *s_instance = NULL;
}

std::wstring StringToWString(const std::string &str)
{
    int num = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
    wchar_t *wide = new wchar_t[num];
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, wide, num);
    std::wstring w_str(wide);
    delete[] wide;
    return w_str;
}

LAppDelegate *LAppDelegate::GetInstance()
{
    if (s_instance == NULL)
    {
        s_instance = new LAppDelegate();
    }

    return s_instance;
}

void LAppDelegate::ReleaseInstance()
{
    if (s_instance != NULL)
    {
        delete s_instance;
    }

    s_instance = NULL;
}

bool LAppDelegate::Initialize()
{
    if (DebugLogEnable)
    {
        LAppPal::PrintLog("[LAppDelegate]START");
    }
    // 设置初始化
    CHAR documents[MAX_PATH];
    HRESULT result = SHGetFolderPathA(NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, documents);
    string inipath(documents);
    INIReader reader(inipath + "\\JPetConfig.ini");

    if (reader.ParseError() == 0)
    {
        _iposX = reader.GetInteger("position", "x", 400);
        _iposY = reader.GetInteger("position", "y", 400);
        _leftUrl = reader.Get("shortcut", "left", "https://t.bilibili.com/");
        _upUrl = reader.Get("shortcut", "up", "https://space.bilibili.com/61639371/dynamic");
        _rightUrl = reader.Get("shortcut", "right", "https://live.bilibili.com/21484828");
        _volume = reader.GetInteger("audio", "volume", 20);
        _mute = reader.GetBoolean("audio", "mute", false);
        _scale = reader.GetFloat("display", "scale", 1.0f);
        _followlist = reader.Get("follow", "list", "61639371;544832401;475210;");
        isLimit = reader.GetBoolean("display", "limit", false);
        Green = reader.GetBoolean("display", "green", false);
        LiveNotify = reader.GetBoolean("notify", "live", true);
        DynamicNotify = reader.GetBoolean("notify", "dynamic", true);
        UpdateNotify = reader.GetBoolean("notify", "update", true);
    }
    else
        LAppPal::PrintLog("[LAppDelegate]INI Reader: %d", reader.ParseError());
    RenderTargetWidth = _scale * DRenderTargetWidth;
    RenderTargetHeight = _scale * DRenderTargetHeight;
    // 音频初始化
    _au = AudioManager::GetInstance();
    _au->Initialize();
    _au->SetMute(_mute);
    _au->SetVolume(static_cast<float>(_volume) / 10);
    if (DebugLogEnable)
        LAppPal::PrintLog("[LAppDelegate]AudioManager Init");

    // 用户状态管理初始化
    _us = new UserStateManager();
    _us->Init(_followlist);

    // GLFWの初期化
    if (glfwInit() == GL_FALSE)
    {
        if (DebugLogEnable)
        {
            LAppPal::PrintLog("[LAppDelegate]Can't initilize GLFW");
        }
        return GL_FALSE;
    }
    // 记录显示器分辨率尺寸
    GLFWmonitor *pr = glfwGetPrimaryMonitor();
    const GLFWvidmode *mode = glfwGetVideoMode(pr);
    _mHeight = mode->height;
    _mWidth = mode->width;

    // 获取当前路径，发送通知时图片地址需要为绝对路径
    char curPath[256];
    GetModuleFileName(GetModuleHandle(NULL), curPath, sizeof(curPath));
    std::string path = curPath;
    _exePath = path.substr(0, path.find_last_of("\\") + 1);
    if (DebugLogEnable)
        LAppPal::PrintLog("[LAppDelegate]Get Execute Path");

    // Windowの生成_
    // 使用GLFW_DECORATED实现边框，会导致1703版本及以前，整个窗口鼠标穿透
    glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
    glfwWindowHint(GLFW_MAXIMIZED, GL_FALSE);
    glfwWindowHint(GLFW_ICONIFIED, GL_FALSE);
    glfwWindowHint(GLFW_FLOATING, GL_TRUE);
    glfwWindowHint(GLFW_DEPTH_BITS, 16);
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
    _window = glfwCreateWindow(RenderTargetWidth, RenderTargetHeight, "JPet", NULL, NULL);

    if (_window == NULL)
    {
        if (DebugLogEnable)
        {
            LAppPal::PrintLog("[LAppDelegate]Can't create GLFW window.");
        }
        glfwTerminate();
        return GL_FALSE;
    }

    // 为了避免1703版本前鼠标穿透的问题，在窗口创建完成后再修改为无边框
    glfwSetWindowAttrib(_window, GLFW_DECORATED, GLFW_FALSE);

    GLFWcursor *cursor = glfwCreateStandardCursor(GLFW_HAND_CURSOR);
    glfwSetCursor(_window, cursor);
    glfwSetWindowPos(_window, _iposX, _iposY);

    HWND hwnd = glfwGetWin32Window(_window);
    _mainHwnd = hwnd;

    // 解决Win7下会在任务栏显示的bug
    HWND phwnd = CreateWindow(NULL,                     // window class name
                              TEXT("JPetParentWindow"), // window caption
                              WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                              CW_USEDEFAULT, // initial x position
                              CW_USEDEFAULT, // initial y position
                              CW_USEDEFAULT, // initial x size
                              CW_USEDEFAULT, // initial y size
                              NULL,          // parent window handle
                              NULL,          // window menu handle
                              NULL,          // program instance handle
                              NULL);         // creation parameters
    SetParent(hwnd, phwnd);

    SetWindowLong(hwnd, GWL_EXSTYLE, WS_EX_ACCEPTFILES | WS_EX_LAYERED | WS_EX_TOOLWINDOW);

    if (Green)
    {
        DWORD exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
        exStyle &= ~WS_EX_TOOLWINDOW;
        SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);
    }
    else
    {
        DWORD exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
        exStyle |= WS_EX_TOOLWINDOW;
        SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);
    }

    // SetLayeredWindowAttributes(hwnd,
    //     RGB(0, 0, 0), 255, LWA_COLORKEY);

    // 音频设定3d位置
    int x, y;
    glfwGetWindowPos(_window, &x, &y);
    _au->Update(x, y, RenderTargetWidth, RenderTargetHeight, _mWidth, _mHeight);

    // 通知初始化
    WinToast::instance()->setAppName(L"JPet");
    const auto aumi = WinToast::configureAUMI(L"JoiGroup", L"JPetProject", L"JPet", WVERSION);
    WinToast::instance()->setAppUserModelId(aumi);
    WinToast::instance()->initialize();

    _LiveHandler = new WinToastEventHandler("https://live.bilibili.com/21484828");
    _DynamicHandler = new WinToastEventHandler("https://space.bilibili.com/61639371/dynamic");
    _UpdateHandler = new WinToastEventHandler("https://pet.joi-club.cn");
    if (DebugLogEnable)
        LAppPal::PrintLog("[LAppDelegate]Notification Init");

    // Windowのコンテキストをカレントに設定
    glfwMakeContextCurrent(_window);
    if (isLimit)
        glfwSwapInterval(2);
    else
        glfwSwapInterval(1);

    if (glewInit() != GLEW_OK)
    {
        if (DebugLogEnable)
        {
            LAppPal::PrintLog("[LAppDelegate]Can't Initilize Glew.");
        }
        glfwTerminate();
        return GL_FALSE;
    }

    //テクスチャサンプリング設定
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    //透過設定
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    //コールバック関数の登録
    glfwSetMouseButtonCallback(_window, EventHandler::OnMouseCallBack);
    glfwSetCursorPosCallback(_window, EventHandler::OnMouseCallBack);
    glfwSetDropCallback(_window, EventHandler::OnDropCallBack);
    glfwSetWindowPosCallback(_window, EventHandler::OnWindowPosCallBack);
    glfwSetTrayClickCallback(_window, EventHandler::OnTrayClickCallBack);

    // ウィンドウサイズ記憶
    int width, height;
    glfwGetWindowSize(LAppDelegate::GetInstance()->GetWindow(), &width, &height);
    _windowWidth = width;
    _windowHeight = height;

    // 托盘图标初始化
    appIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON1));
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hwnd;
    nid.uID = IDI_ICON1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_IAWENTRAY;
    nid.hIcon = appIcon;
    strcpy(nid.szTip, TEXT("天雨雨"));
    Shell_NotifyIcon(NIM_ADD, &nid);

    // 设置窗口图标（绿幕模式下会显示在任务栏）
    SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)appIcon);

    // AppView 初始化
    _view->Initialize();

    // Cubism SDK 初始化
    InitializeCubism();

    // 初始化模型参数
    map<string, float> initState;
    for (int i = 0; i < PartStateManager::GetInstance()->ParamNum; i++)
    {
        initState[PartStateManager::GetInstance()->ParamList[i]] = reader.GetFloat("parts", PartStateManager::GetInstance()->ParamList[i], PartStateManager::GetInstance()->ParamDefault[i]);
    }
    PartStateManager::GetInstance()->ImportState(initState);
    PartStateManager::GetInstance()->SetState();

    // 检查版本状况
    srand(time(NULL));
    if (UpdateNotify && _us->CheckUpdate())
        Notify(L"桌宠阿轴有新版本了", L"点击前往主页查看更新", _UpdateHandler);

    // 随机播放启动语音
    _au->Play3dSound("Resources/Audio/s0" + to_string(rand() % StartAudioNum + 1) + ".mp3");

    // 用于测试的通知
    // Notify(L"阿轴有新动态了", L"点击查看动态", _DynamicHandler);

    return GL_TRUE;
}

void LAppDelegate::SetGreen(bool green)
{
    Green = green;
    HWND hwnd = glfwGetWin32Window(_window);
    if (Green)
    {
        DWORD exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
        exStyle &= ~WS_EX_TOOLWINDOW;
        SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);
    }
    else
    {
        DWORD exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
        exStyle |= WS_EX_TOOLWINDOW;
        SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);
    }
}

void LAppDelegate::SetLimit(bool limit)
{
    isLimit = limit;
    if (limit)
        glfwSwapInterval(2);
    else
        glfwSwapInterval(1);
}

void LAppDelegate::Release()
{
    // Windowの削除
    glfwDestroyWindow(_window);

    glfwTerminate();

    delete _textureManager;
    delete _view;
    _au->Release();
    _au->ReleaseInstance();
    // リソースを解放
    LAppLive2DManager::ReleaseInstance();

    // Cubism SDK の解放
    CubismFramework::Dispose();
}

void LAppDelegate::Run()
{
    //メインループ
    bool noskip = false;
    while (glfwWindowShouldClose(_window) == GL_FALSE && !_isEnd)
    {
        noskip = !noskip;
        int width, height;
        glfwGetWindowSize(LAppDelegate::GetInstance()->GetWindow(), &width, &height);

        static int x, y;
        if (noskip)
        {
            glfwGetWindowPos(_window, &x, &y);
            _au->Update(x, y, width, height, _mWidth, _mHeight);
        }

        if ((_windowWidth != width || _windowHeight != height) && width > 0 && height > 0)
        {
            // AppViewの初期化
            _view->Initialize();
            // スプライトサイズを再設定
            _view->ResizeSprite();
            // サイズを保存しておく
            _windowWidth = width;
            _windowHeight = height;

            // ビューポート変更
            glViewport(0, 0, width, height);
        }

        // 闲置状态更新
        if (IsCount)
        {
            IdleCount++;
        }
        if (IdleCount > 60 * 6) // 10s under 60fps
        {
            SetIdle();
            if (DebugLogEnable)
                LAppPal::PrintLog("[LAppDelegate] Idle On");
        }

        //鼠标捕捉
        static double cx, cy;
        if (noskip)
        {
            glfwGetCursorPos(_window, &cx, &cy);
            // 非拖动状态下，跟随鼠标位置；拖动状态下，通过OnTouchMoved模拟物理效果
            if (!_captured && !InMotion)
                _view->OnTouchesMoved(static_cast<float>(cx), static_cast<float>(cy));
        }

        // 画面の初期化
        if (!Green)
        {
            glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        }
        else
            glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glClearDepth(1.0);

        // 時間更新
        LAppPal::UpdateTime();

        //描画更新
        _view->Render();

        // バッファの入れ替え
        glfwSwapBuffers(_window);

        // Poll for and process events
        glfwPollEvents();

        static bool lastLive = false;
        static float scale = _scale;
        static bool isShowing = false;
        // 直播提醒
        auto &liveQueue = _us->GetLiveState();
        if (!liveQueue.empty())
        {
            auto liveInfo = liveQueue.front();
            liveQueue.pop();
            if (LiveNotify)
                Notify((liveInfo.name + L"开播了").c_str(), (liveInfo.title).c_str(), new WinToastEventHandler("https://live.bilibili.com/" + liveInfo.roomid));
            if (liveInfo.uid == "863287")
                _au->Play3dSound("Resources/Audio/n01.mp3");
        }

        // 动态提醒
        auto &dynamicQueue = _us->GetDynamicState();
        if (!dynamicQueue.empty())
        {
            auto dynamicInfo = dynamicQueue.front();
            dynamicQueue.pop();
            Notify((dynamicInfo.name + L"有新动态了").c_str(), L"点击查看动态", new WinToastEventHandler("https://t.bilibili.com/" + dynamicInfo.content));
        }

        // 设置界面
        if (_isSetting && !isShowing)
        {
            isShowing = true;
            std::thread settingThread = SettingWindowThread();
            settingThread.detach();
        }
        if (!_isSetting)
            isShowing = false;

        // 启动时刷新缩放，用于消除可能存在的边框残留
        // static bool scaleRefresh = true;
        // if (scaleRefresh) {
        //    if (!IsWindows8Point1OrGreater())glfwSetWindowSize(_window, RenderTargetWidth, RenderTargetHeight);
        //    scaleRefresh = false;
        //}

        if (scale != _scale)
        {
            scale = _scale;
            RenderTargetHeight = _scale * DRenderTargetHeight;
            RenderTargetWidth = _scale * DRenderTargetWidth;
            glfwSetWindowSize(_window, RenderTargetWidth, RenderTargetHeight);
            if (DebugLogEnable)
                LAppPal::PrintLog("[LAppDelegate] New Window Size");
        }

        _au->SetVolume(static_cast<float>(_volume) / 10);
        _au->SetMute(_mute);
    }
    _us->Stop();
    // Release前保存配置
    SaveSettings();

    Shell_NotifyIcon(NIM_DELETE, &nid);
    if (DebugLogEnable)
        LAppPal::PrintLog("[LAppDelegate]TrayICON Delete");
    Release();

    LAppDelegate::ReleaseInstance();
}

void LAppDelegate::SaveSettings()
{
    int x, y;
    glfwGetWindowPos(_window, &x, &y);
    PartStateManager::GetInstance()->SaveState();
    auto modelState = PartStateManager::GetInstance()->GetAllState();
    ofstream of;
    of.open(documentPath + "\\JPetConfig.ini", ios::trunc);
    of << "[position]\n"
       << "x=" << x << "\ny=" << y << "\n[shortcut]\n"
       << "left=" << _leftUrl << "\nup=" << _upUrl << "\nright=" << _rightUrl << "\n[follow]\n"
       << "list=" << _followlist << "\n[audio]\n"
       << "volume=" << _volume << "\nmute=" << (_mute ? "true" : "false")
       << "\n[display]\nscale=" << _scale
       << "\ngreen=" << (Green ? "true" : "false")
       << "\nlimit=" << (isLimit ? "true" : "false")
       << "\n[notify]\nlive=" << (LiveNotify ? "true" : "false")
       << "\ndynamic=" << (DynamicNotify ? "true" : "false")
       << "\nupdate=" << (UpdateNotify ? "true" : "false")
       << "\n[parts]";
    // 保存模型状态
    for (auto i : modelState)
    {
        of << "\n" + i.first + "=" << (i.second > 0.5) ? 1 : 0;
    }
    of.close();
    if (DebugLogEnable)
        LAppPal::PrintLog("[LAppDelegate]Setting Saved");
}

LAppDelegate::LAppDelegate() : _cubismOption(),
                               _window(NULL),
                               _captured(false),
                               _mouseX(0.0f),
                               _mouseY(0.0f),
                               _pX(0),
                               _pY(0),
                               _isEnd(false),
                               _isMsg(false),
                               _iposX(400),
                               _iposY(400),
                               _cX(0),
                               _cY(0),
                               _mHeight(0),
                               _mWidth(0),
                               _leftUrl("https://t.bilibili.com/"),
                               _upUrl("https://space.bilibili.com/61639371/dynamic"),
                               _rightUrl("https://live.bilibili.com/21484828"),
                               _au(NULL),
                               _us(NULL),
                               _isLive(false),
                               _isSetting(false),
                               _timeSetting(1),
                               _holdTime(0),
                               _volume(20),
                               _mute(false),
                               _windowWidth(0),
                               _windowHeight(0)
{
    _view = new LAppView();
    _textureManager = new LAppTextureManager();
}

LAppDelegate::~LAppDelegate() = default;

void LAppDelegate::InitializeCubism()
{
    // setup cubism
    _cubismOption.LogFunction = LAppPal::PrintMessage;
    _cubismOption.LoggingLevel = LAppDefine::CubismLoggingLevel;
    Csm::CubismFramework::StartUp(&_cubismAllocator, &_cubismOption);

    // Initialize cubism
    CubismFramework::Initialize();

    // load model
    LAppLive2DManager::GetInstance();

    // default proj
    CubismMatrix44 projection;

    LAppPal::UpdateTime();

    _view->InitializeSprite();
}

void LAppDelegate::OnMouseCallBack(GLFWwindow *window, int button, int action, int modify)
{
    if (_view == NULL)
    {
        return;
    }
    if (GLFW_MOUSE_BUTTON_LEFT == button)
    {
        SetNotIdle();
        if (GLFW_PRESS == action)
        {
            _captured = true;
            glfwGetCursorPos(window, &_cX, &_cY);
            _view->OnTouchesBegan(_mouseX, _mouseY);
        }
        else if (GLFW_RELEASE == action)
        {
            if (_captured)
            {
                _captured = false;
                _view->OnTouchesEnded(_mouseX, _mouseY);
            }
        }
    }
    if (GLFW_MOUSE_BUTTON_RIGHT == button)
    {
        SetNotIdle();
        if (GLFW_PRESS == action)
        {
            _holdTime = glfwGetTime();
            _isMsg = true;
            _pX = _mouseX;
            _pY = _mouseY;
        }
        else if (GLFW_RELEASE == action)
        {
            float dx = fabs(_mouseX - _pX);
            float dy = fabs(_mouseY - _pY);
            if (dx < 60 && dy < 60)
            {
                // 鼠标小范围移动
                double now = glfwGetTime();
                if (now - _holdTime > _timeSetting)
                    _isSetting = true;
            }
            else if (_mouseX > _pX && dx > dy)
                ShellExecute(NULL, "open", _rightUrl.c_str(), NULL, NULL, SW_SHOWNORMAL); //向右滑动
            else if (_mouseX < _pX && dx > dy)
                ShellExecute(NULL, "open", _leftUrl.c_str(), NULL, NULL, SW_SHOWNORMAL); //向左滑动
            else if (_mouseY < _pY && dy > dx)
                ShellExecute(NULL, "open", _upUrl.c_str(), NULL, NULL, SW_SHOWNORMAL); //向上滑动
            else if (_mouseY > _pY && dy > dx)
            {
                _isShowing = false; //向下滑动
                glfwHideWindow(_window);
            }

            _isMsg = false;
        }
    }
    return;
}

void LAppDelegate::OnMouseCallBack(GLFWwindow *window, double x, double y)
{
    _mouseX = static_cast<float>(x);
    _mouseY = static_cast<float>(y);
    if (_captured)
    {
        int xpos, ypos;
        glfwGetWindowPos(window, &xpos, &ypos);
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glfwSetWindowPos(window, xpos + x - _cX, ypos + y - _cY);
        // 简单模拟拖动时的物理效果
        double dx = x - _cX;
        if (dx > 0)
        {
            _view->OnTouchesMoved(x - 30 * dx, ypos - 2 * height / 3);
        }
        if (dx < 0)
        {
            _view->OnTouchesMoved(x - 30 * dx, ypos - 2 * height / 3);
        }
        return;
    }
}

void LAppDelegate::OnDropCallBack(GLFWwindow *window, int path_count, const WCHAR *paths[])
{
    for (int i = 0; i < path_count; i++)
    {
        WCHAR p[MAX_PATH];
        wmemset(p, 0, MAX_PATH);
        StrCpyW(p, paths[i]);
        SHFILEOPSTRUCTW op;
        op.hwnd = NULL;
        op.pTo = NULL;
        op.wFunc = FO_DELETE;
        op.fFlags = FOF_ALLOWUNDO | FOF_SIMPLEPROGRESS;
        op.pFrom = p;
        SHFileOperationW(&op);
        if (DebugLogEnable)
            LAppPal::PrintLog("[LAppDelegate]Delete File Complete");
    }
}

void LAppDelegate::OnWindowPosCallBack(GLFWwindow *window, int x, int y)
{
}

// 托盘菜单设置
#define IDM_HIDE 2004
#define IDM_SET 2001
#define IDM_RESET 2002
#define IDM_EXIT 2003

void LAppDelegate::OnTrayClickCallBack(GLFWwindow *window, int b, unsigned w)
{
    if (DebugLogEnable)
        LAppPal::PrintLog("[LAppDelegate]Tray Clicked: %d", b);
    if (b == 2)
    {
        Menu();
    }
    else
    {
        switch (w)
        {
        case IDM_HIDE:
        {
            if (_isShowing)
                glfwHideWindow(_window);
            else
                glfwShowWindow(_window);
            _isShowing = !_isShowing;
            break;
        }
        case IDM_SET:
        {
            _isSetting = true;
            break;
        }
        case IDM_EXIT:
        {
            _isEnd = true;
            break;
        }
        case IDM_RESET:
        {
            glfwSetWindowPos(window, 0, 0);
            break;
        }
        default:
            _isShowing = true;
            glfwShowWindow(_window);
            break;
        }
    }
}

GLuint LAppDelegate::CreateShader()
{
    //バーテックスシェーダのコンパイル
    GLuint vertexShaderId = glCreateShader(GL_VERTEX_SHADER);
    const char *vertexShader =
        "#version 120\n"
        "attribute vec3 position;"
        "attribute vec2 uv;"
        "varying vec2 vuv;"
        "void main(void){"
        "    gl_Position = vec4(position, 1.0);"
        "    vuv = uv;"
        "}";
    glShaderSource(vertexShaderId, 1, &vertexShader, NULL);
    glCompileShader(vertexShaderId);
    if (!CheckShader(vertexShaderId))
    {
        return 0;
    }

    //フラグメントシェーダのコンパイル
    GLuint fragmentShaderId = glCreateShader(GL_FRAGMENT_SHADER);
    const char *fragmentShader =
        "#version 120\n"
        "varying vec2 vuv;"
        "uniform sampler2D texture;"
        "uniform vec4 baseColor;"
        "void main(void){"
        "    gl_FragColor = texture2D(texture, vuv) * baseColor;"
        "}";
    glShaderSource(fragmentShaderId, 1, &fragmentShader, NULL);
    glCompileShader(fragmentShaderId);
    if (!CheckShader(fragmentShaderId))
    {
        return 0;
    }

    //プログラムオブジェクトの作成
    GLuint programId = glCreateProgram();
    glAttachShader(programId, vertexShaderId);
    glAttachShader(programId, fragmentShaderId);

    // リンク
    glLinkProgram(programId);

    glUseProgram(programId);

    return programId;
}

bool LAppDelegate::CheckShader(GLuint shaderId)
{
    GLint status;
    GLint logLength;
    glGetShaderiv(shaderId, GL_INFO_LOG_LENGTH, &logLength);
    if (logLength > 0)
    {
        GLchar *log = reinterpret_cast<GLchar *>(CSM_MALLOC(logLength));
        glGetShaderInfoLog(shaderId, logLength, &logLength, log);
        CubismLogError("Shader compile log: %s", log);
        CSM_FREE(log);
    }

    glGetShaderiv(shaderId, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE)
    {
        glDeleteShader(shaderId);
        return false;
    }

    return true;
}

void LAppDelegate::Notify(const WCHAR *title, const WCHAR *content, WinToastEventHandler *handler)
{
    WinToastTemplate templ = WinToastTemplate(WinToastTemplate::ImageAndText02);

    templ.setTextField(title, WinToastTemplate::FirstLine);
    templ.setTextField(content, WinToastTemplate::SecondLine);
    std::string img = _exePath + "Resources/Img/Avatar.png";
    templ.setImagePath(StringToWString(img));
    WinToast::instance()->showToast(templ, handler, nullptr);
}

LRESULT CALLBACK SettingProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    static float TimeSetting;
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        string title = string("检查更新(") + VERSION + ")";
        SendMessage(GetDlgItem(hwnd, IDC_CHECK), WM_SETTEXT, true, (LPARAM)title.c_str());
        SendMessage(GetDlgItem(hwnd, IDC_MUTE), BM_SETCHECK, LAppDelegate::GetInstance()->GetMute() ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessage(GetDlgItem(hwnd, IDC_VOLUME), WM_ENABLE, !LAppDelegate::GetInstance()->GetMute(), 0);
        SendMessage(GetDlgItem(hwnd, IDC_VOLUME), TBM_SETPOS, true, LAppDelegate::GetInstance()->GetVolume());
        SendMessage(GetDlgItem(hwnd, IDC_SCALE), TBM_SETRANGE, true, MAKELONG(5, 15));
        SendMessage(GetDlgItem(hwnd, IDC_SCALE), TBM_SETPOS, true, static_cast<double>(LAppDelegate::GetInstance()->GetScale()) * 10);
        SendMessage(GetDlgItem(hwnd, IDC_LEFT), WM_SETTEXT, true, (LPARAM)LAppDelegate::GetInstance()->GetLURL().c_str());
        SendMessage(GetDlgItem(hwnd, IDC_UP), WM_SETTEXT, true, (LPARAM)LAppDelegate::GetInstance()->GetUURL().c_str());
        SendMessage(GetDlgItem(hwnd, IDC_RIGHT), WM_SETTEXT, true, (LPARAM)LAppDelegate::GetInstance()->GetRURL().c_str());
        SendMessage(GetDlgItem(hwnd, IDC_FOLLOW), WM_SETTEXT, true, (LPARAM)LAppDelegate::GetInstance()->GetFollowList().c_str());

        SendMessage(GetDlgItem(hwnd, IDC_LIVENOTIFY), BM_SETCHECK, LAppDelegate::GetInstance()->LiveNotify ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessage(GetDlgItem(hwnd, IDC_DYNAMICNOTIFY), BM_SETCHECK, LAppDelegate::GetInstance()->DynamicNotify ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessage(GetDlgItem(hwnd, IDC_GREEN), BM_SETCHECK, LAppDelegate::GetInstance()->Green ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessage(GetDlgItem(hwnd, IDC_LIMIT), BM_SETCHECK, LAppDelegate::GetInstance()->isLimit ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessage(GetDlgItem(hwnd, IDC_STARTCHECK), BM_SETCHECK, LAppDelegate::GetInstance()->UpdateNotify ? BST_CHECKED : BST_UNCHECKED, 0);
    }
        return true;

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDC_APPLY:
        {
            float scale = static_cast<float>(SendMessage(GetDlgItem(hwnd, IDC_SCALE), TBM_GETPOS, 0, 0)) / 10;
            if (DebugLogEnable)
                LAppPal::PrintLog("Volume: %d; scale: %f", SendMessage(GetDlgItem(hwnd, IDC_VOLUME), TBM_GETPOS, 0, 0), scale);
            LAppDelegate::GetInstance()->SetMute(SendMessage(GetDlgItem(hwnd, IDC_MUTE), BM_GETCHECK, 0, 0));
            LAppDelegate::GetInstance()->SetVolume(SendMessage(GetDlgItem(hwnd, IDC_VOLUME), TBM_GETPOS, 0, 0));
            LAppDelegate::GetInstance()->SetScale(scale);
            // left
            int len = SendMessage(GetDlgItem(hwnd, IDC_LEFT), WM_GETTEXTLENGTH, 0, 0);
            TCHAR *buff = new TCHAR[len + 1];
            SendMessage(GetDlgItem(hwnd, IDC_LEFT), WM_GETTEXT, len + 1, (LPARAM)buff);
            LAppDelegate::GetInstance()->SetLURL(std::string(buff));
            free(buff);
            // up
            len = SendMessage(GetDlgItem(hwnd, IDC_UP), WM_GETTEXTLENGTH, 0, 0);
            buff = new TCHAR[len + 1];
            SendMessage(GetDlgItem(hwnd, IDC_UP), WM_GETTEXT, len + 1, (LPARAM)buff);
            LAppDelegate::GetInstance()->SetUURL(std::string(buff));
            free(buff);
            // right
            len = SendMessage(GetDlgItem(hwnd, IDC_RIGHT), WM_GETTEXTLENGTH, 0, 0);
            buff = new TCHAR[len + 1];
            SendMessage(GetDlgItem(hwnd, IDC_RIGHT), WM_GETTEXT, len + 1, (LPARAM)buff);
            LAppDelegate::GetInstance()->SetRURL(std::string(buff));
            free(buff);

            // followlist
            len = SendMessage(GetDlgItem(hwnd, IDC_FOLLOW), WM_GETTEXTLENGTH, 0, 0);
            buff = new TCHAR[len + 1];
            SendMessage(GetDlgItem(hwnd, IDC_FOLLOW), WM_GETTEXT, len + 1, (LPARAM)buff);
            LAppDelegate::GetInstance()->SetFollowList(std::string(buff));
            free(buff);

            LAppDelegate::GetInstance()->LiveNotify = SendMessage(GetDlgItem(hwnd, IDC_LIVENOTIFY), BM_GETCHECK, 0, 0);
            LAppDelegate::GetInstance()->DynamicNotify = SendMessage(GetDlgItem(hwnd, IDC_DYNAMICNOTIFY), BM_GETCHECK, 0, 0);
            LAppDelegate::GetInstance()->SetGreen(SendMessage(GetDlgItem(hwnd, IDC_GREEN), BM_GETCHECK, 0, 0));
            LAppDelegate::GetInstance()->SetLimit(SendMessage(GetDlgItem(hwnd, IDC_LIMIT), BM_GETCHECK, 0, 0));
            LAppDelegate::GetInstance()->UpdateNotify = SendMessage(GetDlgItem(hwnd, IDC_STARTCHECK), BM_GETCHECK, 0, 0);

            // 即时保存设置
            LAppDelegate::GetInstance()->SaveSettings();

            EndDialog(hwnd, 0);

            break;
        }
        case IDC_MUTE:
        {
            bool checked = SendMessage(GetDlgItem(hwnd, IDC_MUTE), BM_GETCHECK, 0, 0);
            if (checked)
                SendMessage(GetDlgItem(hwnd, IDC_VOLUME), WM_ENABLE, false, 0);
            else
                SendMessage(GetDlgItem(hwnd, IDC_VOLUME), WM_ENABLE, true, 0);
            break;
        }
        case IDC_CHECK:
        {
            ShellExecute(NULL, "open", "https://pet.joi-club.cn", NULL, NULL, SW_SHOWNORMAL);
            break;
        }
        }
    }
        return 0;

    case WM_DESTROY:
    {
        LAppDelegate::GetInstance()->SetIsSetting(false);
        EndDialog(hwnd, 0);
    }
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void LAppDelegate::SettingWindow()
{
    DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_SETTING), glfwGetWin32Window(_window),
              SettingProc);
}

std::thread LAppDelegate::SettingWindowThread()
{
    return std::thread(&LAppDelegate::SettingWindow, this);
}

void LAppDelegate::Menu()
{
    POINT p;
    GetCursorPos(&p);
    HMENU hMenu;
    hMenu = CreatePopupMenu();
    if (_isShowing)
        AppendMenu(hMenu, MF_STRING, IDM_HIDE, TEXT("隐藏"));
    else
        AppendMenu(hMenu, MF_STRING, IDM_HIDE, TEXT("显示"));
    AppendMenu(hMenu, MF_STRING, IDM_RESET, TEXT("重置位置"));
    AppendMenu(hMenu, MF_STRING, IDM_SET, TEXT("设置"));
    AppendMenu(hMenu, MF_STRING, IDM_EXIT, TEXT("退出"));
    SetForegroundWindow(glfwGetWin32Window(_window));
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, p.x, p.y, NULL, glfwGetWin32Window(_window), NULL);
}

std::thread LAppDelegate::MenuThread()
{
    return std::thread(&LAppDelegate::Menu, this);
}