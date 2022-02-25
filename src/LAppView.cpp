﻿/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#include "LAppView.hpp"
#include <math.h>
#include <string>
#include "LAppPal.hpp"
#include "LAppDelegate.hpp"
#include "LAppLive2DManager.hpp"
#include "LAppTextureManager.hpp"
#include "LAppDefine.hpp"
#include "TouchManager.hpp"
#include "LAppSprite.hpp"
#include "LAppModel.hpp"

using namespace std;
using namespace LAppDefine;

LAppView::LAppView() : _programId(0),
                       _msg(NULL),
                       _watermsg(NULL),
                       _gear(NULL),
                       _power(NULL),
                       _renderSprite(NULL),
                       _renderTarget(SelectTarget_None)
{
    _clearColor[0] = 1.0f;
    _clearColor[1] = 1.0f;
    _clearColor[2] = 1.0f;
    _clearColor[3] = 0.0f;

    // タッチ関係のイベント管理
    _touchManager = new TouchManager();

    // デバイス座標からスクリーン座標に変換するための
    _deviceToScreen = new CubismMatrix44();

    // 画面の表示の拡大縮小や移動の変換を行う行列
    _viewMatrix = new CubismViewMatrix();
}

LAppView::~LAppView()
{
    _renderBuffer.DestroyOffscreenFrame();
    delete _renderSprite;
    delete _viewMatrix;
    delete _deviceToScreen;
    delete _touchManager;
    delete _msg;
    delete _watermsg;
    delete _gear;
    delete _power;
}

void LAppView::Initialize()
{
    int width, height;
    glfwGetWindowSize(LAppDelegate::GetInstance()->GetWindow(), &width, &height);

    if (width == 0 || height == 0)
    {
        return;
    }

    float ratio = static_cast<float>(height) / static_cast<float>(width);
    float left = ViewLogicalMaxLeft / 4;
    float right = ViewLogicalMaxRight / 4;
    float bottom = ViewLogicalMaxBottom / 4;
    float top = ViewLogicalMaxBottom / 4;

    _viewMatrix->SetScreenRect(left, right, bottom, top); // デバイスに対応する画面の範囲。 Xの左端, Xの右端, Yの下端, Yの上端

    float screenW = fabsf(left - right);
    _deviceToScreen->LoadIdentity(); // サイズが変わった際などリセット必須
    _deviceToScreen->ScaleRelative(screenW / width, -screenW / width);
    _deviceToScreen->TranslateRelative(-width * 0.5f, -height * 0.5f);

    // 表示範囲の設定
    _viewMatrix->SetMaxScale(ViewMaxScale); // 限界拡大率
    _viewMatrix->SetMinScale(ViewMinScale); // 限界縮小率

    // 表示できる最大範囲
    _viewMatrix->SetMaxScreenRect(
        ViewLogicalMaxLeft,
        ViewLogicalMaxRight,
        ViewLogicalMaxBottom,
        ViewLogicalMaxTop);
}

void LAppView::Render()
{
    LAppLive2DManager *Live2DManager = LAppLive2DManager::GetInstance();

    // Cubism更新・描画
    Live2DManager->OnUpdate();
    if (LAppDelegate::GetInstance()->GetIsMsg())
    {
        _msg->Render();
        // _watermsg->Render();
    }
}

void LAppView::InitializeSprite()
{
    _programId = LAppDelegate::GetInstance()->CreateShader();

    int width, height;
    glfwGetWindowSize(LAppDelegate::GetInstance()->GetWindow(), &width, &height);

    LAppTextureManager *textureManager = LAppDelegate::GetInstance()->GetTextureManager();
    const string resourcesPath = ResourcesPath;

    string imageName = OptionImg;
    string waterImageName = WaterImg;
    LAppTextureManager::TextureInfo *msgTexture = textureManager->CreateTextureFromPngFile(resourcesPath + imageName);
    LAppTextureManager::TextureInfo *waterTexture = textureManager->CreateTextureFromPngFile(resourcesPath + waterImageName);
    float x = width * 0.5f;
    float y = height * 0.5f;
    float fWidth = static_cast<float>(width);
    float fHeight = static_cast<float>(height);
    _msg = new LAppSprite(x, y, fWidth, fHeight, msgTexture->id, _programId);
    _watermsg = new LAppSprite(x, y, fWidth, fHeight, waterTexture->id, _programId);
}

TouchManager *LAppView::GetTouchManager()
{
    return _touchManager;
}

void LAppView::OnTouchesBegan(float px, float py) const
{
    _touchManager->TouchesBegan(px, py);
    {

        // シングルタップ
        float x = _deviceToScreen->TransformX(_touchManager->GetX()); // 論理座標変換した座標を取得。
        float y = _deviceToScreen->TransformY(_touchManager->GetY()); // 論理座標変換した座標を取得。
        LAppLive2DManager *live2DManager = LAppLive2DManager::GetInstance();
        if (DebugTouchLogEnable)
        {
            LAppPal::PrintLog("[APP]touchesEnded x:%.2f y:%.2f", x, y);
        }
        live2DManager->OnTap(x, y);
    }
}

void LAppView::OnTouchesMoved(float px, float py) const
{
    float viewX = this->TransformViewX(_touchManager->GetX());
    float viewY = this->TransformViewY(_touchManager->GetY());

    _touchManager->TouchesMoved(px, py);

    LAppLive2DManager *Live2DManager = LAppLive2DManager::GetInstance();
    Live2DManager->OnDrag(viewX, viewY);
}

void LAppView::OnTouchesEnded(float px, float py) const
{
    // タッチ終了
    LAppLive2DManager *live2DManager = LAppLive2DManager::GetInstance();
    live2DManager->OnDrag(0.0f, 0.0f);
}

float LAppView::TransformViewX(float deviceX) const
{
    float screenX = _deviceToScreen->TransformX(deviceX); // 論理座標変換した座標を取得。
    return _viewMatrix->InvertTransformX(screenX);        // 拡大、縮小、移動後の値。
}

float LAppView::TransformViewY(float deviceY) const
{
    float screenY = _deviceToScreen->TransformY(deviceY); // 論理座標変換した座標を取得。
    return _viewMatrix->InvertTransformY(screenY);        // 拡大、縮小、移動後の値。
}

float LAppView::TransformScreenX(float deviceX) const
{
    return _deviceToScreen->TransformX(deviceX);
}

float LAppView::TransformScreenY(float deviceY) const
{
    return _deviceToScreen->TransformY(deviceY);
}

void LAppView::PreModelDraw(LAppModel &refModel)
{
    // 別のレンダリングターゲットへ向けて描画する場合の使用するフレームバッファ
    Csm::Rendering::CubismOffscreenFrame_OpenGLES2 *useTarget = NULL;
}

void LAppView::PostModelDraw(LAppModel &refModel)
{
    // 別のレンダリングターゲットへ向けて描画する場合の使用するフレームバッファ
    Csm::Rendering::CubismOffscreenFrame_OpenGLES2 *useTarget = NULL;
}

void LAppView::SwitchRenderingTarget(SelectTarget targetType)
{
    _renderTarget = targetType;
}

void LAppView::SetRenderTargetClearColor(float r, float g, float b)
{
    _clearColor[0] = r;
    _clearColor[1] = g;
    _clearColor[2] = b;
}

float LAppView::GetSpriteAlpha(int assign) const
{
    // assignの数値に応じて適当に決定
    float alpha = 0.25f + static_cast<float>(assign) * 0.5f; // サンプルとしてαに適当な差をつける
    if (alpha > 1.0f)
    {
        alpha = 1.0f;
    }
    if (alpha < 0.1f)
    {
        alpha = 0.1f;
    }

    return alpha;
}

void LAppView::ResizeSprite()
{
    LAppTextureManager *textureManager = LAppDelegate::GetInstance()->GetTextureManager();
    if (!textureManager)
    {
        return;
    }

    // 描画領域サイズ
    int width, height;
    glfwGetWindowSize(LAppDelegate::GetInstance()->GetWindow(), &width, &height);
    LAppPal::PrintLog("wsize: %d %d", width, height);
    float x = 0.0f;
    float y = 0.0f;

    if (_msg)
    {
        GLuint id = _msg->GetTextureId();
        LAppTextureManager::TextureInfo *texInfo = textureManager->GetTextureInfoById(id);
        if (texInfo)
        {
            float ty = (static_cast<float>(height) - modelHeight) / height;
            x = width * 0.5f;
            y = 0.5f * height;
            float fWidth = static_cast<float>(width);
            float fHeight = static_cast<float>(height);
            _msg->ResetRect(x, y, fWidth, fHeight);
        }
    }
    if (_watermsg)
    {
        GLuint id = _watermsg->GetTextureId();
        LAppTextureManager::TextureInfo *texInfo = textureManager->GetTextureInfoById(id);
        if (texInfo)
        {
            float ty = (static_cast<float>(height) - modelHeight) / height;
            x = width * 0.5f;
            y = 0.5f * height;
            float fWidth = static_cast<float>(width);
            float fHeight = static_cast<float>(height);
            _watermsg->ResetRect(x, y, fWidth, fHeight);
        }
    }
}
