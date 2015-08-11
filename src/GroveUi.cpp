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

#include "Grove.h"

#include "cinder/Rand.h"
#include "cinder/Timeline.h"
#include "cinder/Log.h"

#if defined( CINDER_ANDROID )
#include "cinder/MotionManager.h"
#endif

using namespace ci;
using namespace ci::app;
using namespace std;


void Grove::setupUi()
{
	// MARK: Setup intro UI
	// load intro textures
	mIntroTexture[0] = gl::Texture2d::create( loadImage( loadAsset( "Ui/Intro0.png" ) ) );
	mIntroTexture[1] = gl::Texture2d::create( loadImage( loadAsset( "Ui/Intro1.png" ) ) );
	mIntroTexture[2] = gl::Texture2d::create( loadImage( loadAsset( "Ui/Intro2.png" ) ) );
	mIntroTexture[3] = gl::Texture2d::create( loadImage( loadAsset( "Ui/Intro3.png" ) ) );

	// get the bounds of our textures
	mIntroRect[0]		= ( mIntroTexture[0]->getBounds() );
	mIntroRect[1]		= ( mIntroTexture[1]->getBounds() );
	mIntroRect[2]		= ( mIntroTexture[2]->getBounds() );
	mIntroRect[3]		= ( mIntroTexture[3]->getBounds() );
	
	// get the window bounds
#if defined( CINDER_COCOA_TOUCH )
	Rectf windowBounds = toPixels( Rectf( vec2( 0.0f ), vec2( getWindowHeight(), getWindowWidth() ) ) );
#else
	Rectf windowBounds = toPixels( getWindowBounds() );
#endif
	
	// fit the intro rects in the main rectangle
	mIntroRect[0] = mIntroRect[0].getCenteredFit( windowBounds, false );
	mIntroRect[1] = mIntroRect[1].getCenteredFit( windowBounds, false );
	mIntroRect[2] = mIntroRect[2].getCenteredFit( windowBounds, false );
	mIntroRect[3] = mIntroRect[3].getCenteredFit( windowBounds, false );
	
	mIntroOpacity[0] = 0.0f;
	mIntroOpacity[1] = 0.0f;
	mIntroOpacity[2] = 0.0f;
	mIntroOpacity[3] = 0.0f;
	
	// start logo
	mWidget = Widget::create( Widget::Format()
							 .position( vec2( getWindowSize() ) / 2.0f )
							 .items( { "Grove" } )
							 );
	mWidget->onClose( [=]( WidgetEvent event ){
		
		mIntro = true;
		
		// animate the different elements
		timeline().applyPtr( &mIntroOpacity[0], 0.0f, 1.0f, 3.0f, EaseInOutAtan() );
		timeline().appendToPtr( &mIntroOpacity[0], 0.0f, 3.0f, EaseInOutAtan() )
		.delay( 2.0f );
		
		timeline().applyPtr( &mIntroOpacity[1], 0.0f, 1.0f, 3.0f, EaseInOutAtan() ).delay( 7.0f )
		.finishFn( [=](){
			
			// generate the terrain
			mTerrain->generateHeightMap();
			mTerrain->createTiles();
			
			// start animation
			timeline().applyPtr( &mSunDirection, vec3( 0, 0.3, -1 ), 22.0f, EaseInOutQuart() ).delay( 2.0f );
			timeline().applyPtr( &mSunDispertion, 0.3f, 22.0f, EaseInOutQuart() ).delay( 2.0f )
			.updateFn( [this](){
				mTerrain->setSunDispertion( mSunDispertion );
			} );
			timeline().applyPtr( &mFogDensity, 0.129f, 22.0f, EaseInOutQuart() ).delay( 2.0f )
			.updateFn( [this](){
				mTerrain->setFogDensity( mFogDensity );
			} );
			timeline().applyPtr( &mFogColor, Color( 0.25, 0.29, 0.47 ), 22.0f, EaseInOutQuart() ).delay( 2.0f )
			.updateFn( [this](){
				mTerrain->setFogColor( mFogColor );
			} );
			timeline().applyPtr( &mSunColor, Color( 1.0f, 0.77, 0.60 ), 25.0f, EaseInOutQuart() ).delay( 2.0f )
			.updateFn( [this](){
				mTerrain->setSunColor( mSunColor );
			} );
			timeline().applyPtr( &mSunScatteringCoeffs, vec3( 0.35, 0.18, 0.29 ), 25.0f, EaseInOutQuart() ).delay( 2.0f )
			.updateFn( [this](){
				mTerrain->setSunScatteringCoeffs( mSunScatteringCoeffs );
			} );
		} );
		
		timeline().appendToPtr( &mIntroOpacity[1], 0.0f, 2.0f, EaseInOutAtan() ).delay( 1.0f );
		timeline().applyPtr( &mIntroOpacity[2], 0.0f, 1.0f, 3.0f, EaseInOutAtan() ).delay( 13.0f );
		timeline().appendToPtr( &mIntroOpacity[2], 0.0f, 3.0f, EaseInOutAtan() ).delay( 2.0f );
		timeline().applyPtr( &mIntroOpacity[3], 0.0f, 1.0f, 3.0f, EaseInOutAtan() ).delay( 20.0f );
		timeline().appendToPtr( &mIntroOpacity[3], 0.0f, 2.0f, EaseInOutAtan() ).delay( 1.75f )
		.finishFn( [=](){
			mIntro = false;
		} );
	
	} );
	
	// MARK: Create in-app Menu
	mOpenMenuButton = Widget::create( Widget::Format()
									 .position( vec2( 32 ) )
									 .items( { "Plus" } )
									 .onReleased( []( WidgetEvent event ){} )
									 .autoClose( false )
									 .size( vec2( 64 ) ) );
	
	mMenu = Widget::create( Widget::Format()
						   .position( vec2( getWindowWidth() - ( getWindowWidth() - ( 4 * 128 + 4 * 10 ) ) / 2.0f - 74, getWindowHeight() - 84 ) )
						   .items( { "Infos", /*"Compass",*/ "Wallpaper", "Twitter"/*, "Regenerate"*/ } )
						   .onReleased( []( WidgetEvent event ){} )
						   .signalPriority( 1 ) );
	
	mMenu->onCloseEnded( [this]( WidgetEvent event ) {
		if( !event.getItem() ) {
			mModal = false;
			mOpenMenuButton->open();
		}
	} );
	mMenu->close( false );
	
	mOpenMenuButton->onPressed( "Plus", [this]( WidgetEvent event ){
		if( mWidget && !mWidget->isOpen() ){
			mMenu->open();
			event.getWidget()->close();
			mModal = true;
		}
	} );
	
	// MARK: Info Panel
	mInfoTexture	= gl::Texture2d::create( loadImage( loadAsset( "Ui/InfosText.png" ) ) );
	mInfoRect	= mInfoTexture->getBounds();
	
	createMenuModal( "Infos", &mInfoRect, &mInfoOffset, mMenu );
	
	// MARK: Regenerate Panel
	//mRegenerateTexture	= gl::Texture2d::create( loadImage( loadAsset( "Ui/RegenerateText.png" ) ) );
	//mRegenerateRect		= mRegenerateTexture->getBounds();
	
	/*createMenuModal( "Regenerate", &mRegenerateRect, &mRegenerateOffset, mMenu, nullptr,
					[this](){
						CI_LOG_V( "Confirm Regeneration" );
						
						//timeline().clear();
						mRandomSeed = randFloat( -200, 200 );
						mTerrain->setNoiseSeed( mRandomSeed );
						mTerrain->createTiles();
						
					} );*/
	
	// MARK: Twitter Panel
	mTwitterTexture	= gl::Texture2d::create( loadImage( loadAsset( "Ui/TwitterText.png" ) ) );
	mTwitterRect		= mTwitterTexture->getBounds();
	
	createMenuModal( "Twitter", &mTwitterRect, &mTwitterOffset, mMenu, nullptr,
					[this](){
						CI_LOG_V( "Confirm Twitter" );
						mCopySurfaceToTwitter = true;
					}, false );
	
	// MARK: Wallpaper Panel
	mWallpaperTexture	= gl::Texture2d::create( loadImage( loadAsset( "Ui/WallpaperText.png" ) ) );
	mWallpaperRect		= mWallpaperTexture->getBounds();
	
	createMenuModal( "Wallpaper", &mWallpaperRect, &mWallpaperOffset, mMenu, nullptr,
					[this](){
						CI_LOG_V( "Confirm Wallpaper" );
						mCopySurfaceToWallpaper = true;
					}, false );
	
	// MARK: Compass Panel
	/*mCompassTexture[0]	= gl::Texture2d::create( loadImage( loadAsset( "Ui/CompassEnableText.png" ) ) );
	mCompassTexture[1]	= gl::Texture2d::create( loadImage( loadAsset( "Ui/CompassDisableText.png" ) ) );
	mCompassRect		= mCompassTexture[0]->getBounds();*/
	
#if defined( CINDER_ANDROID )
	if( MotionManager::isGyroAvailable() ) {
#endif
		/*createMenuModal( "Compass", &mCompassRect, &mCompassOffset, mMenu, nullptr,
					[this](){
						CI_LOG_V( "Confirm Compass" );
						mGyroEnabled =! mGyroEnabled;
					} );*/
#if defined( CINDER_ANDROID )
	}
#endif
	
}

void Grove::createMenuModal( const std::string &name, ci::Rectf *rect, ci::vec2 *offset, const WidgetRef &menu, const std::function<void()> &cancel, const std::function<void()> &confirm, bool adaptBackgroundColor )
{
	// create the layout
	Rectf fullRect	= *rect;
	fullRect.include( vec2( fullRect.getCenter().x, fullRect.getLowerRight().y + 180 ) );
	fullRect = fullRect.getCenteredFit( getWindowBounds(), false );
	
	// get the window bounds
#if defined( CINDER_COCOA_TOUCH )
	Rectf windowBounds = toPixels( Rectf( vec2( 0.0f ), vec2( getWindowHeight(), getWindowWidth() ) ) );
#else
	Rectf windowBounds = toPixels( getWindowBounds() );
#endif
	
	// if the screen is really small, add some extra margins
	if( abs( fullRect.getWidth() - windowBounds.getWidth() ) < 300 )
		fullRect.scaleCentered( (float) ( windowBounds.getWidth() - 300 ) / (float) fullRect.getWidth() );
	if( abs( fullRect.getHeight() - windowBounds.getHeight() ) < 300 )
		fullRect.scaleCentered( (float) ( windowBounds.getHeight() - 300 ) / (float) fullRect.getHeight() );
	
	*rect = (*rect).getCenteredFit( fullRect, false );
	(*rect).offset( vec2( 0.0f, -( fullRect.getHeight() - (*rect).getHeight() ) / 2.0f ) );
	*offset = vec2( 0.0f, -fullRect.getLowerLeft().y );
	
	auto fadeIn = [this](){
		timeline().applyPtr( &mFogDensity, 1.0f, 2.0f, EaseInAtan() );
		timeline().applyPtr( &mSunIntensity, 0.0f, 2.0f, EaseInAtan() )
		.updateFn( [this](){
			mTerrain->setFogDensity( mFogDensity );
			mTerrain->setSunIntensity( mSunIntensity );
		} );
	};
	auto fadeOut = [this](){
		timeline().applyPtr( &mFogDensity, 0.129f, 6.0f, EaseOutAtan() );
		timeline().applyPtr( &mSunIntensity, 0.166f, 6.0f, EaseOutAtan() )
		
		.updateFn( [this](){
			mTerrain->setFogDensity( mFogDensity );
			mTerrain->setSunIntensity( mSunIntensity );
		} );
	};
	
	// add events
	if( menu ){
		
		menu->onPressed( name, [=]( WidgetEvent event ) {
			if( adaptBackgroundColor ) fadeIn();
			
			mModal = true;
			event.getWidget()->close( true, event.getItem() );
			
			// wait for the menu to close
			timeline().add( [=](){
				timeline().applyPtr( offset, vec2(0), 1.0f, EaseInQuad() )
				.finishFn( [=]() {
					
					vector<string> items;
					if( confirm ) items = { "ConfirmWire", "QuitWire" };
					else items = { "QuitWire" };
					
					mWidget = Widget::create( Widget::Format()
											 .items( items )
											 .position( (*rect).getLowerLeft() + vec2( (*rect).getWidth() / 2 + ( confirm ? 69 : 0 ), ( fullRect.getHeight() - (*rect).getHeight() ) / 2 ) )
											 .autoClose( false ) );
					mWidget->onClose( [=]( WidgetEvent event ){
						timeline().applyPtr( offset, vec2( 0, -fullRect.getLowerLeft().y ), 1.0f, EaseOutQuad() );
						if( adaptBackgroundColor ) fadeOut();
					} );
					mWidget->onCloseEnded( [=]( WidgetEvent event ){
						mOpenMenuButton->open();
					} );
					if( confirm ){
						mWidget->onPressed( "ConfirmWire", [=]( WidgetEvent event ) {
							event.getWidget()->close();
							timeline().add( [=](){ mModal = false; }, timeline().getCurrentTime() + 1.5f );
							confirm();
						} );
					}
					mWidget->onPressed( "QuitWire", [=]( WidgetEvent event ) {
						event.getWidget()->close();
						timeline().add( [=](){ mModal = false; }, timeline().getCurrentTime() + 1.5f );
						if( cancel )
							cancel();
					} );
				} );
			}, timeline().getCurrentTime() + 1.0f );
		} );
	}
	else {
		
		vector<string> items;
		if( confirm ) items = { "ConfirmWire", "QuitWire" };
		else items = { "QuitWire" };
		
		mWidget = Widget::create( Widget::Format()
								 .items( items )
								 .position( (*rect).getLowerLeft() + vec2( (*rect).getWidth() / 2 + ( confirm ? 69 : 0 ), ( fullRect.getHeight() - (*rect).getHeight() ) / 2 ) )
								 .autoClose( false ) );
		mWidget->onClose( [=]( WidgetEvent event ){
			timeline().applyPtr( offset, vec2( 0, -fullRect.getLowerLeft().y ), 1.0f, EaseOutQuad() );
			if( adaptBackgroundColor ) fadeOut();
		} );
		mWidget->onCloseEnded( [=]( WidgetEvent event ){
			mOpenMenuButton->open();
		} );
		if( confirm ){
			mWidget->onPressed( "ConfirmWire", [=]( WidgetEvent event ) {
				event.getWidget()->close();
				timeline().add( [=](){ mModal = false; }, timeline().getCurrentTime() + 1.5f );
				confirm();
			} );
		}
		mWidget->onPressed( "QuitWire", [=]( WidgetEvent event ) {
			event.getWidget()->close();
			timeline().add( [=](){ mModal = false; }, timeline().getCurrentTime() + 1.5f );
			if( cancel )
				cancel();
		} );
		
		if( adaptBackgroundColor ) fadeIn();
		mModal = true;
		timeline().applyPtr( offset, vec2(0), 1.0f, EaseInQuad() );
	}
}