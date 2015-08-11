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

#include "GestureManager.h"

using namespace std;
using namespace ci;
using namespace ci::app;


Touch::Touch() :
mType( TOUCH_UNDEFINED ),
mTimeOfBirth( getElapsedSeconds() ),
mTimeOfDeath( -1.0f ),
mIsPan( false ),
mIsDoubleTap( false )
{
}

GestureManagerRef GestureManager::create( const ci::app::WindowRef &window )
{
	return make_shared<GestureManager>( window );
}

GestureManager::GestureManager( const ci::app::WindowRef &window )
: mIsPanning( false )
{
	App::get()->getSignalUpdate().connect( bind( &GestureManager::update, this ) );
	window->getSignalTouchesBegan().connect( -5, bind( &GestureManager::touchesBegan, this, placeholders::_1 ) );
	window->getSignalTouchesMoved().connect( -5, bind( &GestureManager::touchesMoved, this, placeholders::_1 ) );
	window->getSignalTouchesEnded().connect( -5, bind( &GestureManager::touchesEnded, this, placeholders::_1 ) );
}
void GestureManager::update()
{
	// update touches
	for( auto touch = mTouches.begin(); touch != mTouches.end(); ){
		
		// check for gestures
		float distTraveled	= (*touch).second.mDistanceTraveled;
		bool dead			= (*touch).second.mTimeOfDeath > 0.0f;
		float duration		= getElapsedSeconds() - (*touch).second.mTimeOfBirth;
		
		// we have a dead touch
		if( dead && getElapsedSeconds() - (*touch).second.mTimeOfDeath > 0.25 ){
			
			// marked as double tap
			if( (*touch).second.mIsDoubleTap ){
				mDoubleTapSignal.emit( (*touch).second );
			}
			// marked as pan
			else if( (*touch).second.mIsPan ){
				mIsPanning = false;
				mPanEndedSignal.emit( (*touch).second );
			}
			// it's neither marked as a pan or as a double tap so if it's short it's a tap
			else if( duration > 0.15f && duration < 0.5f && !(*touch).second.mIsPan ){
				mTapSignal.emit( (*touch).second );
			}
			// erase the touch
			touch = mTouches.erase( touch );
			continue;
		}
		
		// panning
		if( duration > 0.25f && distTraveled > 10.0f ){
			if( !(*touch).second.mIsPan ) {
				mTouches[(*touch).second.mId].mIsPan = true;
				mIsPanning = true;
				mPanBeganSignal.emit( (*touch).second );
			}
			else {
				mPanSignal.emit( (*touch).second );
			}
		}
		++touch;
	}
}

void GestureManager::touchesBegan( ci::app::TouchEvent &event )
{
	if( event.isHandled() ) return;
	
	for( auto touch : event.getTouches() ){
		
		// check if we have a existing touch that is close enough
		int closestId = -1;
		float min = 10000000;
		for( auto others : mTouches ){
			float dist = length( others.second.mPos2d - toPixels( touch.getPos() ) );
			if( dist < min ){
				closestId	= others.second.mId;
				min			= dist;
			}
		}
		
		// this is probably the same touch and a double tap
		if( min < 30.0f && !mTouches[closestId].mIsPan ){
			float elapsed = getElapsedSeconds() - mTouches[closestId].mTimeOfDeath;
			if( elapsed < 0.3f ) {
				mTouches[closestId].mIsDoubleTap = true;
				continue;
			}
		}
		
		// update the touch
		if( !mTouches.count( touch.getId() ) ) {
			mTouches[touch.getId()]				= Touch();
		}
		mTouches[touch.getId()].mId				= touch.getId();
		mTouches[touch.getId()].mPos2d			= toPixels( touch.getPos() );
		mTouches[touch.getId()].mInitialPos2d	= toPixels( touch.getPos() );
		mTouches[touch.getId()].mPrevPos2d		= touch.getPrevPos();
	}
}
void GestureManager::touchesMoved( ci::app::TouchEvent &event )
{
	if( event.isHandled() ) return;
	for( auto touch : event.getTouches() ){
		auto touchIt = mTouches.find( touch.getId() );
		if( touchIt != mTouches.end() ) {
			// update touch
			mTouches[touch.getId()].mPos2d = toPixels( touch.getPos() );
			mTouches[touch.getId()].mPrevPos2d = touch.getPrevPos();
			float dist = length( mTouches[touch.getId()].mPos2d - mTouches[touch.getId()].mPrevPos2d );
			mTouches[touch.getId()].mDistanceTraveled += dist;
		}
	}
}
void GestureManager::touchesEnded( ci::app::TouchEvent &event )
{
	if( event.isHandled() ) return;
	// mark touches for deletion
	for( auto touch : event.getTouches() ){
		auto touchIt = mTouches.find( touch.getId() );
		if( touchIt != mTouches.end() ) {
			uint32_t touchId = (*touchIt).second.mId;
			mTouches[touchId].mTimeOfDeath = getElapsedSeconds();
		}
	}
}