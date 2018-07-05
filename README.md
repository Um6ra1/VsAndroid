# VsAndroid
Native Activity with VisualStudio2017.
Overview

## Description
Visual studio 2017で作れるAndroidアプリのサンプル．C++のみのNative activityで書かれています．
基本的にはOpenGL ESで画面表示を行うみたいです．
一応Visual studioのサンプルでは傾きを検知して画面色を塗り替えるのものが
付属ていますが，図形描画はないみたいでしたので，三角形を描けるように改造してみました．

## 改造箇所
描画やイベント処理はEngineという構造体で管理されていました．
これをクラス化(class Engine)し，より扱いやすくしました．
