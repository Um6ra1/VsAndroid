# VsAndroid
Native Activity with VisualStudio2017.
Overview

## DrawTriangle
Visual studio 2017で作れるAndroidアプリのサンプル．C++のみのNative activityで書かれています．
基本的にはOpenGL ESで画面表示を行うみたいです．
一応Visual studioのサンプルでは傾きを検知して画面色を塗り替えるのものが
付属ていますが，図形描画はないみたいでしたので，三角形を描けるように改造してみました．

## NuklearTest
NuklearというGUIライブラリをOpen GL ES 2で動くようにしました．
NkGLES.hがNuklearを描画する足回りでして，本家のsdl_opengles2/nuklear_sdl_gles2.hをもとに作られています．
本家はSDLが使われているので，それを使わないようにしています．
ここでは，overview.cのデモが動きます．

I made the GUI library named Nuklear work on Open GL ES 2.
NkGLES.h draws Nuklear GUI and is also made from sdl_opengles2 / nuklear_sdl_gles2.h.
Here, demo of overview.c works.

## ??
描画やイベント処理はEngineという構造体で管理されていました．
これをクラス化(class Engine)しています．

