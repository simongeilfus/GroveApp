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

#include <memory>

#include "cinder/Rect.h"
#include "cinder/Vector.h"
#include "cinder/Signals.h"
#include "cinder/app/App.h"
#include "cinder/gl/Texture.h"

typedef std::shared_ptr<class WidgetItem> WidgetItemRef;
typedef std::shared_ptr<class Widget> WidgetRef;
typedef std::function<void(const class WidgetEvent &event)> WidgetCallback;

class WidgetEvent {
public:
	WidgetEvent( const ci::vec2 &pos, const WidgetRef &widget, const WidgetItemRef &item )
	: mPos( pos ), mWidget( widget ), mItem( item ) {}
	
	ci::vec2		getPos() const { return mPos; }
	WidgetItemRef	getItem() const { return mItem; }
	WidgetRef		getWidget() const { return mWidget; }
	
protected:
	ci::vec2		mPos;
	WidgetItemRef	mItem;
	WidgetRef		mWidget;
};


class WidgetItem : public std::enable_shared_from_this<WidgetItem> {
public:
	static WidgetItemRef create( const WidgetRef &parent, const std::string &name, const ci::vec2 &position, float delay = 0.0f, const WidgetCallback &callback = WidgetCallback() );
	WidgetItem( const WidgetRef &parent, const std::string &name, const ci::vec2 &position, float delay, const WidgetCallback &callback );
	
	void draw();	
	ci::Rectf getBounds() const;
	void setPosition( const ci::vec2 &position ) { mPosition = position; }
	
	void pressed( const WidgetEvent &event );
	void released( const WidgetEvent &event );
	void moved( const WidgetEvent &event );
	
	void setParent( const WidgetRef &parent ) { mParent = parent; }
	WidgetRef getParent() const { return mParent; }
	
	void onReleased( const WidgetCallback &callback ) { mOnReleased = callback; }
	void onPressed( const WidgetCallback &callback ) { mOnPressed = callback; }
	void onMoved( const WidgetCallback &callback ) { mOnMoved = callback; }
	
	void setOpacity( float opacity );
	
	const WidgetCallback& getOnReleased() const { return mOnReleased; }
	const WidgetCallback& getOnPressed() const { return mOnPressed; }
	const WidgetCallback& getOnMoved() const { return mOnMoved; }
	
	bool isPressed() const { return mIsPressed; }
	ci::vec2 getDownPosition() const { return mDownPosition; }
	ci::vec2 getPressPosition() const { return mPressPosition; }
	
	void open( float delay );
	void close( float delay );
	
protected:
	WidgetCallback			mOnReleased;
	WidgetCallback			mOnPressed;
	WidgetCallback			mOnMoved;
	bool					mIsPressed;
	
	ci::vec2				mDownPosition;
	ci::vec2				mPressPosition;
	WidgetRef				mParent;
	ci::vec2				mPosition;
	ci::vec2				mSize;
	float					mOpacity;
	ci::vec2				mOriginalSize;
	ci::gl::Texture2dRef	mTexture;
};

class Widget : public std::enable_shared_from_this<Widget> {
public:
	struct Format {
		Format() : mWindow( ci::app::getWindow() ), mSignalPriority( -1 ), mPosition( 0.0f ), mSize( 128.0f ), mPadding( 10.0f ), mDirection( -1.0f, 0.0f ), mAutoClose( true ) {}
		
		Format& position( const ci::vec2 &position ) { mPosition = position; return *this; }
		Format& size( const ci::vec2 &size ) { mSize = size; return *this; }
		Format& padding( const ci::vec2 &padding ) { mPadding = padding; return *this; }
		Format& direction( const ci::vec2 &dir ) { mDirection = dir; return *this; }
		Format& items( const std::vector<std::string> &items ) { mItems = items; return *this; }
		
		Format& window( const ci::app::WindowRef &window ) { mWindow = window; return *this; }
		Format& signalPriority( int priority ) { mSignalPriority = priority; return *this; }
		
		Format& onReleased( const WidgetCallback& callback ) { mOnReleased = callback; return *this; }
		Format& onPressed( const WidgetCallback& callback ) { mOnPressed = callback; return *this; }
		Format& onMoved( const WidgetCallback& callback ) { mOnMoved = callback; return *this; }
		
		Format& autoClose( bool autoClose ) { mAutoClose = autoClose; return *this; }
		
		ci::vec2 getPosition() const { return mPosition; }
		ci::vec2 getSize() const { return mSize; }
		ci::vec2 getPadding() const { return mPadding; }
		ci::vec2 getDirection() const { return mDirection; }
		std::vector<std::string> getItems() const { return mItems; }
		
		ci::app::WindowRef	getWindow() const { return mWindow; }
		int					getSignalPriority() const { return mSignalPriority; }
		
		const WidgetCallback& getOnReleased() const { return mOnReleased; }
		const WidgetCallback& getOnPressed() const { return mOnPressed; }
		const WidgetCallback& getOnMoved() const { return mOnMoved; }
		bool getAutoCloseEnabled() const {  return mAutoClose; }
		
	protected:
		bool						mAutoClose;
		int							mSignalPriority;
		ci::app::WindowRef			mWindow;
		ci::vec2					mPosition, mSize, mPadding, mDirection;
		std::vector<std::string>	mItems;
		WidgetCallback				mOnReleased, mOnPressed, mOnMoved;
	};
	
	static WidgetRef create( const Format &format = Format() );
	Widget( const Format &format );
	~Widget();
	
	struct Close {
		void operator()( const WidgetEvent &event );
	};
	
	void draw();
	
	void open();
	void close( bool animate = true, const WidgetItemRef &item = nullptr );
	bool isOpen() const { return mOpen; }
	
	void add( const WidgetItemRef &item );
	void mouseDown( ci::app::MouseEvent &event );
	void mouseUp( ci::app::MouseEvent &event );
	void mouseDrag( ci::app::MouseEvent &event );
	void touchesBegan( ci::app::TouchEvent &event );
	void touchesMoved( ci::app::TouchEvent &event );
	void touchesEnded( ci::app::TouchEvent &event );
	
	void onReleased( const std::string &name, const WidgetCallback& callback );
	void onPressed( const std::string &name, const WidgetCallback& callback );
	void onMoved( const std::string &name, const WidgetCallback& callback );
	void onClose( const WidgetCallback& callback );
	void onCloseEnded( const WidgetCallback& callback );
	
	void setPosition( const ci::vec2 &position ) { mPosition = position; }
	ci::vec2 getPosition() const { return mPosition; }
	const std::vector<WidgetItemRef>& getItems() const { return mItems; }
	WidgetItemRef getItem( const std::string &name );
	
protected:
	std::vector<WidgetItemRef>	mItems;
	std::map<std::string,WidgetItemRef>	mItemsMap;
	ci::signals::Connection		mMouseDown;
	ci::signals::Connection		mMouseUp;
	ci::signals::Connection		mMouseDrag;
	ci::signals::Connection		mTouchesBegan;
	ci::signals::Connection		mTouchesEnded;
	ci::signals::Connection		mTouchesMoved;
	ci::vec2					mPosition;
	WidgetCallback				mCloseCallback;
	WidgetCallback				mCloseEndedCallback;
	bool						mOpen;
	bool						mAutoClose;
};