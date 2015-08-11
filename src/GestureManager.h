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

#include "cinder/app/App.h"
#include "cinder/gl/Fbo.h"
#include "cinder/Vector.h"
#include "cinder/Signals.h"

struct Touch {
	enum Type { TOUCH_TERRAIN, TOUCH_ROAD, TOUCH_TREE, TOUCH_SKY, TOUCH_SUN, TOUCH_UNDEFINED };
	
	Touch();
	
	uint32_t	mId;
	Type		mType;
	double		mTimeOfBirth;
	double		mTimeOfDeath;
	float		mDistanceTraveled;
	ci::vec2	mPos2d, mPrevPos2d, mInitialPos2d;
	ci::vec3	mPos3d;
	bool		mIsPan, mIsDoubleTap;
};

typedef std::shared_ptr<class GestureManager> GestureManagerRef;

class GestureManager {
public:
	static GestureManagerRef create( const ci::app::WindowRef &window );
	GestureManager( const ci::app::WindowRef &window );
	
	typedef ci::signals::Signal<void(const Touch&)> GestureSignal;
	
	GestureSignal&	getTapSignal() { return mTapSignal; }
	GestureSignal&	getDoubleTapSignal() { return mDoubleTapSignal; }
	GestureSignal&	getPanSignal() { return mPanSignal; }
	GestureSignal&	getPanBeganSignal() { return mPanBeganSignal; }
	GestureSignal&	getPanEndedSignal() { return mPanEndedSignal; }
	
	bool isPanning() const { return mIsPanning; }
	
protected:
	void update();
	void touchesBegan( ci::app::TouchEvent &event );
	void touchesMoved( ci::app::TouchEvent &event );
	void touchesEnded( ci::app::TouchEvent &event );
	
	GestureSignal				mTapSignal;
	GestureSignal				mDoubleTapSignal;
	GestureSignal				mPanSignal;
	GestureSignal				mPanBeganSignal;
	GestureSignal				mPanEndedSignal;
	std::map<uint32_t, Touch>	mTouches;
	bool						mIsPanning;
};