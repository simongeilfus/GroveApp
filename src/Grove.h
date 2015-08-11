/*
 Copyright (c) 2015 Simon Geilfus
 
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.
 */

#pragma once

//#define RES_CAPPING
//#define DEBUG_OUTPUT

#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/Batch.h"
#include "cinder/gl/Texture.h"
#include "cinder/gl/Shader.h"
#include "cinder/gl/Fbo.h"
#include "cinder/Camera.h"
#include "cinder/CameraUI.h"

#include "Terrain.h"
#include "UserInterface.h"
#include "GestureManager.h"

class Grove : public ci::app::App {
public:
	// initialization
	Grove();
	void setupUi();
	void setupRendering();
	void setupFbo();
	void setupCamera();
	
	// update & draw
	void update() override;
	void draw() override;
	void resize() override;
	
	// interactions
	void edit( const Touch &touch );
	void editTrees();
	void editTerrain();
	void randomizeSunColor();
	void randomizeFogColor();
	void randomizeScatteringColor();
	
	// gestures
	void tap( const Touch &touch );
	void doubleTap( const Touch &touch );
	void pan( const Touch &touch );
	void panBegan( const Touch &touch );
	void panEnded( const Touch &touch );
	
	// picking and pixel data methods
	ci::vec4	getPixelData( const ci::vec2 &position, const ci::CameraPersp &cam );
	Touch::Type getPixelDataType( const ci::vec4 &data );
	
	// ui
	void createMenuModal( const std::string &name, ci::Rectf *rectangle, ci::vec2 *offset, const WidgetRef &menu = nullptr, const std::function<void()> &cancel = nullptr, const std::function<void()> &confirm = nullptr, bool adaptBackgroundColor = true );
	
	// camera
	ci::CameraPersp				mCamera;
	ci::CameraUi				mCameraUi;
	ci::CameraPersp				mCameraUiCam;
	ci::vec3					mCameraInitialViewDir;
	float						mCameraSplinePos;
	float						mCameraSplineDestination;
	ci::vec3					mCameraDestination;
	ci::quat					mCameraOrientation;
	ci::quat					mGyroInitialOrientation;
	
	// rendering
	ci::gl::FboRef				mFbo;
	ci::gl::Texture2dRef		mFboColorAtt;
	ci::gl::Texture2dRef		mFboDepthAtt;
	ci::gl::GlslProgRef			mPostProcessing;
	
	// frame time
	float						mLastTime;
	float						mCurrentTime;
	float						mFrameTime;
	
	// terrain
	TerrainRef					mTerrain;
	float						mElevation;
	ci::vec3					mSunDirection;
	float						mFogDensity;
	ci::Color					mFogColor;
	ci::Color					mSunColor;
	ci::vec3					mSunScatteringCoeffs;
	float						mSunDispertion;
	float						mSunIntensity;
	float						mRandomSeed;
	bool						mEditingTerrain;
	
	// touches, gestures and gyro
	GestureManagerRef			mGestureManager;
	bool						mGyroEnabled;
	
	// ui related stuffs
	WidgetRef					mWidget, mOpenMenuButton, mMenu, mDebugMenu;
	ci::gl::Texture2dRef		mIntroTexture[4], mInfoTexture, mTwitterTexture, mWallpaperTexture, mCompassTexture[2];
	ci::vec2					mInfoOffset, mTwitterOffset, mWallpaperOffset, mCompassOffset;
	ci::Rectf					mIntroRect[4], mInfoRect, mTwitterRect, mWallpaperRect, mCompassRect;
	float						mIntroOpacity[4];
	bool						mIntro, mModal;
	bool						mCopySurfaceToTwitter, mCopySurfaceToWallpaper;
};