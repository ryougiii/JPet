/**
 * Copyright(c) Live2D Inc. All rights reserved.
 *
 * Use of this source code is governed by the Live2D Open Software license
 * that can be found at https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html.
 */

#include "LAppDefine.hpp"
#include <CubismFramework.hpp>
#include <string>

namespace LAppDefine {

    using namespace Csm;

    // 画面
    const csmFloat32 ViewMaxScale = 2.0f;
    const csmFloat32 ViewMinScale = 0.8f;

    const csmFloat32 ViewLogicalLeft = -1.0f;
    const csmFloat32 ViewLogicalRight = 1.0f;

    const csmFloat32 ViewLogicalMaxLeft = -2.0f;
    const csmFloat32 ViewLogicalMaxRight = 2.0f;
    const csmFloat32 ViewLogicalMaxBottom = -2.0f;
    const csmFloat32 ViewLogicalMaxTop = 2.0f;

    // 相対パス
    const csmChar* ResourcesPath = "Resources/";
    const csmChar* OptionImg = "Img/message.png";
    const csmChar* WaterImg = "Img/water.png";

    // モデル定義------------------------------------------
    // モデルを配置したディレクトリ名の配列
    // ディレクトリ名とmodel3.jsonの名前を一致させておくこと
    const csmChar* DefaultModelName = "Joi001";
    const csmChar* hiyoriModelName = "Hiyori";
    const csmChar* ModelDir[] = {
        "Joi001"
    };
    const csmInt32 ModelDirSize = sizeof(ModelDir) / sizeof(const csmChar*);

    const csmInt32 StartAudioNum = 9;
    const csmInt32 IdleAudioNum = 7;
    const csmInt32 TouchAudioNum = 5;

    // モーションの優先度定数
    const csmInt32 PriorityNone = 0;
    const csmInt32 PriorityIdle = 1;
    const csmInt32 PriorityNormal = 2;
    const csmInt32 PriorityForce = 3;

    // デバッグ用ログの表示オプション
    const csmBool DebugLogEnable = true;
    const csmBool DebugTouchLogEnable = false;

    // Frameworkから出力するログのレベル設定
    const CubismFramework::Option::LogLevel CubismLoggingLevel = CubismFramework::Option::LogLevel_Verbose;

    // デフォルトのレンダーターゲットサイズ
    const csmInt32 DRenderTargetWidth = 512;
    const csmInt32 DRenderTargetHeight = 512;
    csmInt32 RenderTargetWidth = 512;
    csmInt32 RenderTargetHeight = 512;
    const csmInt32 modelWidth = 1024;
    const csmInt32 modelHeight = 1024;

    const csmFloat32 AudioSpace = 3.0f;
    const csmFloat32 AudioDepth = 2.0f;

    // 音频
    const csmChar* startAudioFile = "Resources/Audio/start.mp3";
    const csmChar* dragAudioFile = "Resources/Audio/drag.mp3";
    const csmChar* endAudioFile = "Resources/Audio/end.mp3";

    std::string documentPath = "";
}
