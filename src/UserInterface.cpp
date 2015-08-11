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

#include "UserInterface.h"

#include "cinder/Timeline.h"
#include "cinder/gl/draw.h"
#include "cinder/gl/scoped.h"

using namespace ci;
using namespace ci::app;
using namespace std;

WidgetItemRef WidgetItem::create( const WidgetRef &parent, const std::string &name, const ci::vec2 &position, float delay, const WidgetCallback &callback )
{
	return make_shared<WidgetItem>( parent, name, position, delay, callback );
}
WidgetItem::WidgetItem( const WidgetRef &parent, const std::string &name, const ci::vec2 &position, float delay, const WidgetCallback &callback )
: mParent( parent ),
mOnReleased( callback ),
mPosition( 0.0f ),
mOpacity( 0.0f ),
mIsPressed( false )
{
	mTexture	= gl::Texture2d::create( loadImage( loadAsset( "Ui/" + name + ".png" ) ), gl::Texture2d::Format().minFilter( GL_LINEAR_MIPMAP_LINEAR ).magFilter( GL_LINEAR ).mipmap().immutableStorage() );
	mSize		= mOriginalSize = mTexture->getSize();
	
	if( parent->getItems().size() ){
		app::timeline().applyPtr( &mPosition, position, 0.5f, EaseOutQuad() ).delay( delay );
		app::timeline().applyPtr( &mOpacity, 1.0f, 0.95f, EaseOutQuint() ).delay( delay );
	}
	else {
		mPosition = position;
		app::timeline().applyPtr( &mOpacity, 1.0f, 0.35f, EaseOutQuad() );
		app::timeline().applyPtr( &mSize, vec2(0), mSize, 0.35f, EaseOutBounce(4.0f) );
	}
}

void WidgetItem::draw()
{
	gl::color( ColorA::gray( 1.0f, mOpacity ) );
	gl::draw( mTexture, getBounds() );
}

ci::Rectf WidgetItem::getBounds() const
{
	return ci::Rectf( mPosition - mSize * 0.5f, mPosition + mSize * 0.5f );
}

void WidgetItem::setOpacity( float opacity )
{
	app::timeline().applyPtr( &mOpacity, opacity, 0.25f );
}
void WidgetItem::pressed( const WidgetEvent &event )
{
	if( mOnPressed ){
		mOnPressed( event );
	}
	mIsPressed		= true;
	mDownPosition	= event.getPos();
	mPressPosition	= mPosition;
}
void WidgetItem::released( const WidgetEvent &event )
{
	app::timeline().applyPtr( &mSize, mOriginalSize * 1.1f, 0.05f, EaseInQuad() );
	app::timeline().appendToPtr( &mSize, mOriginalSize, 0.05f, EaseOutQuad() );
	if( mOnReleased ){
		mOnReleased( event );
	}
	mIsPressed = false;
}
void WidgetItem::moved( const WidgetEvent &event )
{
	if( mOnMoved ){
		mOnMoved( event );
	}
}

void WidgetItem::open( float delay )
{
	app::timeline().applyPtr( &mOpacity, 1.0f, 0.35f, EaseOutQuad() ).delay( delay );
	app::timeline().applyPtr( &mSize, vec2(0), mOriginalSize, 0.35f, EaseOutBounce(4.0f) ).delay( delay );
}

void WidgetItem::close( float delay )
{
	if( delay >= 0.0f ){
		app::timeline().applyPtr( &mSize, vec2(0), 0.35f, EaseInBounce() ).delay( delay );
	}
	else {
		app::timeline().applyPtr( &mOpacity, 0.0f, 0.35f, EaseOutQuad() ).delay( -delay );
		app::timeline().applyPtr( &mSize, vec2(0), 0.35f, EaseInBounce() ).delay( -delay );
	}
}

void Widget::Close::operator()( const WidgetEvent &event ){
	event.getWidget()->close();
}


WidgetRef Widget::create( const Format &format )
{
	auto widget = make_shared<Widget>( format );
	
	float delay = 0.0f;
	vec2 pos = vec2(0);
	for( auto itemName : format.getItems() ){
		auto item = WidgetItem::create( widget, itemName, pos, ( format.getItems().size() - delay ) * 0.25f );
		widget->mItems.push_back( item );
		widget->mItemsMap[itemName] = item;
		
		timeline().add( [format,item](){
			if( format.getOnPressed() )
				item->onPressed( format.getOnPressed() );
			if( format.getOnReleased() )
				item->onReleased( format.getOnReleased() );
			if( format.getOnMoved() )
				item->onMoved( format.getOnMoved() );
		}, timeline().getCurrentTime() + 1.0f );
		
		pos += ( format.getSize() + format.getPadding() ) * format.getDirection();
		delay += 1.0f;
	}
	return widget;
}
Widget::Widget( const Format &format )
: mPosition( format.getPosition() ),
mOpen( true ),
mAutoClose( format.getAutoCloseEnabled() )
{
	format.getWindow()->getSignalMouseUp().connect( format.getSignalPriority(), std::bind( &Widget::mouseUp, this, std::placeholders::_1 ) );
	format.getWindow()->getSignalMouseDown().connect( format.getSignalPriority(), std::bind( &Widget::mouseDown, this, std::placeholders::_1 ) );
	format.getWindow()->getSignalMouseDrag().connect( format.getSignalPriority(), std::bind( &Widget::mouseDrag, this, std::placeholders::_1 ) );
	format.getWindow()->getSignalTouchesBegan().connect( format.getSignalPriority(), std::bind( &Widget::touchesBegan, this, std::placeholders::_1 ) );
	format.getWindow()->getSignalTouchesEnded().connect( format.getSignalPriority(), std::bind( &Widget::touchesEnded, this, std::placeholders::_1 ) );
	format.getWindow()->getSignalTouchesMoved().connect( format.getSignalPriority(), std::bind( &Widget::touchesMoved, this, std::placeholders::_1 ) );
	
	timeline().add( [this,format](){
		if( mItems.size() && !mItems[0]->getOnReleased() && !format.getOnReleased() )
			mItems[0]->onReleased( Widget::Close() );
		
	}, timeline().getCurrentTime() + 1.0f );
}

Widget::~Widget()
{
	mMouseUp.disconnect();
	mMouseDown.disconnect();
	mTouchesBegan.disconnect();
	mTouchesEnded.disconnect();
}

void Widget::open()
{
	if( mItems.size() ){
		for( size_t i = 0; i < mItems.size(); ++i ){
			mItems[i]->open( 0.001f + i * 0.05f );
		}
		mOpen = true;
	}
}
void Widget::close( bool animate, const WidgetItemRef &item )
{
	if( mItems.size() ){
		mItems[0]->close( - (float) mItems.size() * 0.25 );
		for( size_t i = mItems.size() - 1; i >= 1; --i ){
			mItems[i]->close( i * 0.25f );
		}
		if( animate ){
			timeline().add( [this,item](){
				mOpen = false;
				if( mCloseEndedCallback ) {
					mCloseEndedCallback( WidgetEvent( vec2(0), shared_from_this(), item ) );
				}
			}, timeline().getCurrentTime() + (float) mItems.size() * 0.25f + 0.35f );
		}
		else {
			mOpen = false;
		}
		if( mCloseCallback ) {
			mCloseCallback( WidgetEvent( vec2(0), shared_from_this(), item ) );
		}
	}
}

void Widget::draw()
{
	//gl::ScopedBlendAlpha alphaBlending;
#if defined( CINDER_COCOA_TOUCH )
	gl::enableAlphaBlending( true );
#else
	gl::enableAlphaBlending();
#endif
	gl::ScopedModelMatrix scopedMat;
	gl::translate( mPosition );
	for( auto it = mItems.rbegin(); it != mItems.rend(); ++it ){
		(*it)->draw();
	}
}
void Widget::add( const WidgetItemRef &item )
{
	mItems.push_back( item );
}
WidgetItemRef Widget::getItem( const std::string &name )
{
	return mItemsMap[name];
}

void Widget::onPressed( const std::string &name, const WidgetCallback& callback )
{
	mItemsMap[name]->onPressed( callback );
}
void Widget::onReleased( const std::string &name, const WidgetCallback& callback )
{
	mItemsMap[name]->onReleased( callback );
}
void Widget::onMoved( const std::string &name, const WidgetCallback& callback )
{
	mItemsMap[name]->onMoved( callback );
}
void Widget::onClose( const WidgetCallback& callback )
{
	mCloseCallback = callback;
}
void Widget::onCloseEnded( const WidgetCallback& callback )
{
	mCloseEndedCallback = callback;
}

void Widget::touchesBegan( ci::app::TouchEvent &event )
{
	if( !mOpen || event.isHandled() ) return;
	
	for( auto touch : event.getTouches() ){
		for( auto item : mItems ){
			if( item->getBounds().contains( toPixels( touch.getPos() ) - mPosition ) ){
				item->pressed( WidgetEvent( toPixels( touch.getPos() ), shared_from_this(), item ) );
				event.setHandled();
				return;
			}
		}
	}
	
	static bool firstPress = true;
	if( mAutoClose && !firstPress )
		close();
	firstPress = false;
}
void Widget::touchesMoved( ci::app::TouchEvent &event )
{
	if( !mOpen || event.isHandled() ) return;
	for( auto touch : event.getTouches() ){
		for( auto item : mItems ){
			if( item->isPressed() ){
				item->moved( WidgetEvent( toPixels( touch.getPos() ), shared_from_this(), item ) );
				event.setHandled();
				return;
			}
		}
	}
}
void Widget::touchesEnded( ci::app::TouchEvent &event )
{
	if( !mOpen || event.isHandled() ) return;
	for( auto touch : event.getTouches() ){
		for( auto item : mItems ){
			if( item->getBounds().contains( toPixels( touch.getPos() ) - mPosition ) ){
				item->released( WidgetEvent( toPixels( touch.getPos() ), shared_from_this(), item ) );
				event.setHandled();
				return;
			}
		}
	}
}

void Widget::mouseDown( ci::app::MouseEvent &event )
{
	if( !mOpen ) return;
	for( auto item : mItems ){
		if( item->getBounds().contains( event.getPos() - ivec2( mPosition ) ) ){
			item->pressed( WidgetEvent( event.getPos(), shared_from_this(), item ) );
			event.setHandled();
			return;
		}
	}
	if( mAutoClose )
		close();
}

void Widget::mouseDrag( ci::app::MouseEvent &event )
{
	if( !mOpen ) return;
	for( auto item : mItems ){
		if( item->isPressed() ){
			item->moved( WidgetEvent( event.getPos(), shared_from_this(), item ) );
			event.setHandled();
			return;
		}
	}
}
void Widget::mouseUp( ci::app::MouseEvent &event )
{
	if( !mOpen ) return;
	for( auto item : mItems ){
		if( item->getBounds().contains( event.getPos() - ivec2( mPosition ) ) ){
			item->released( WidgetEvent( event.getPos(), shared_from_this(), item ) );
			event.setHandled();
			return;
		}
	}
}