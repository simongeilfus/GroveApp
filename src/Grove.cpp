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

#include "cinder/gl/scoped.h"
#include "cinder/gl/draw.h"
#include "cinder/BSplineFit.h"
#include "cinder/Rand.h"
#include "cinder/Utilities.h"
#include "cinder/Timeline.h"
#include "cinder/Log.h"
#include "cinder/ip/Flip.h"
#include "cinder/android/CinderAndroid.h"
#include "cinder/ObjLoader.h"
#include "cinder/MotionManager.h"

#include "ShaderPreprocessor.h"

#include "glm/gtx/vector_angle.hpp"

using namespace ci;
using namespace ci::app;
using namespace std;

Grove::Grove() :
mLastTime( getElapsedSeconds() ),
mCurrentTime( getElapsedSeconds() ),
mIntro( true ),
mModal( false ),
mElevation( 150.0f ),
mFogDensity( 10.0f ),
mFogColor( 0.25, 0.29, 0.47 ),
mSunColor( 1.0f, 0.77, 0.60 ),
mSunScatteringCoeffs( 0 ),
mSunDispertion( 3.0f ),
mSunIntensity( 0.166 ),
mSunDirection( 0, 0.3, -1 ),
mCopySurfaceToTwitter( false ),
mCopySurfaceToWallpaper( false ),
mGyroEnabled( false ),
mEditingTerrain( false )
{

	if( MotionManager::isGyroAvailable() ){
		MotionManager::enable();
		mGyroEnabled = true;
	}

	setupUi();
	setupRendering();

	// create a random seed from the current time_since_epoch
	using namespace std::chrono;
	auto now		= system_clock::now().time_since_epoch();
	seconds nows	= duration_cast<seconds>( now );

	randSeed( nows.count() );
	mRandomSeed		= randFloat( -200, 200 );

	// create the terrain
	mTerrain = Terrain::create( Terrain::Format().noiseSeed( mRandomSeed ) );
	mTerrain->setSunDirection( vec3( randFloat( 0.5, 1.0 ) * ( randFloat( 0, 1 ) > 0.5f ? -1.0f : 1.0f ), randFloat( -0.3, 0.8f ), randFloat( 0.0f, -0.3f ) ) );
	float hue = randFloat( 0.0f, 1.0f );
	mTerrain->setSunColor( Color( ColorModel::CM_HSV, hue, randFloat( 0.4f, 0.7f ), 0.75f ) );
	mTerrain->setFogColor( Color( ColorModel::CM_HSV, glm::clamp( hue + randFloat( -0.1f, 0.1f ), 0.0f, 1.0f ), randFloat( 0.4f, 0.7f ), 0.75f ) );
	
	mTerrain->setFogDensity( mFogDensity );
	mTerrain->setSunScatteringCoeffs( mSunScatteringCoeffs );
	mTerrain->setSunDispertion( mSunDispertion );

	mSunColor		= mTerrain->getSunColor();
	mSunDirection	= mTerrain->getSunDirection();
	mFogColor		= mTerrain->getFogColor();

	setupCamera();

	// setup gesture manager and connect its signals
	mGestureManager = GestureManager::create( getWindow() );
	mGestureManager->getTapSignal().connect( bind( &Grove::tap, this, placeholders::_1 ) );
	mGestureManager->getDoubleTapSignal().connect( bind( &Grove::doubleTap, this, placeholders::_1 ) );
	mGestureManager->getPanBeganSignal().connect( bind( &Grove::panBegan, this, placeholders::_1 ) );
	mGestureManager->getPanSignal().connect( bind( &Grove::pan, this, placeholders::_1 ) );
	mGestureManager->getPanEndedSignal().connect( bind( &Grove::panEnded, this, placeholders::_1 ) );

	disableFrameRate();
	gl::enableVerticalSync(false);
	
	timeline().stepTo( mLastTime );
}

// MARK: Setup
void Grove::resize()
{
	// adapt cameras to new aspect ratio
	mCamera.setAspectRatio( getWindowAspectRatio() );
	mCameraUi.setCamera( &mCamera );

	// re-create texture and fbo
	setupFbo();
}

void Grove::setupFbo()
{
	vec2 resolution		= toPixels( getWindowSize() );
	float aspectRatio	= getWindowAspectRatio();
	
#if defined( RES_CAPPING )
	if( glm::any( glm::greaterThan( resolution, vec2( 1920, 1080 ) ) ) ){
		float width		= 1920;
		float height	= width / aspectRatio;
		resolution		= vec2( width, height );
	}
#endif
	
	mFboColorAtt		= gl::Texture2d::create( resolution.x, resolution.y, gl::Texture2d::Format().internalFormat( GL_RGB ).minFilter( GL_LINEAR ).magFilter( GL_LINEAR ) );
	mFboDepthAtt		= gl::Texture2d::create( resolution.x, resolution.y, gl::Texture2d::Format().internalFormat( GL_RGB ).minFilter( GL_LINEAR ).magFilter( GL_LINEAR ) );

	auto fboFormat		= gl::Fbo::Format();
	fboFormat.attachment( GL_COLOR_ATTACHMENT0, mFboColorAtt );
	fboFormat.attachment( GL_COLOR_ATTACHMENT1, mFboDepthAtt );
	fboFormat.depthBuffer();
	
	mFbo				= gl::Fbo::create( resolution.x, resolution.y, fboFormat );
}

void Grove::setupRendering()
{
	// create the textures we're using for offscreen rendering
	setupFbo();

	// load the postprocessing shader
	try {
		gl_temp::ShaderPreprocessor pp;
		pp.addDefine( "CINDER_DESKTOP", "0" );
		pp.addDefine( "CINDER_GL_ES_3",	"1" );
		pp.addDefine( "CINDER_GL_PLATFORM", "CINDER_GL_ES_3" );
		pp.setVersion( 300 );
		mPostProcessing = gl::GlslProg::create( gl::GlslProg::Format().vertex( pp.parse( getAssetPath( "Shaders/Passtrough.vert" ) ).c_str() ).fragment( pp.parse( getAssetPath( "Shaders/PostProcessing.frag" ) ).c_str() ) );
	}
	catch( gl::GlslProgExc exc ){
		CI_LOG_E( exc.what() );
	}
}

void Grove::setupCamera()
{

	// set initial camera position and direction
	mCamera				= CameraPersp( getWindowWidth(), getWindowHeight(), 80, 0.1, 15000 );
	mCameraSplinePos	= 0.0f;
	vec3 eye			= vec3(0);
	vec3 target			= vec3(0,0,-1);
	eye.y				+= 4.0f;
	target.y			+= 4.0f;
	mCameraDestination	= eye;
	mCamera.lookAt( eye, target );
	//mCamera.setCenterOfInterestPoint( target );
	mCameraOrientation	= mCamera.getOrientation();

	// setup the camera ui and connect it to the window's signals
	mCameraUiCam = mCamera;
	mCameraUi = CameraUi( &mCameraUiCam );
}


// MARK: Update / Draw
void Grove::update()
{
	// update frame time
	mCurrentTime	= getElapsedSeconds();
	mFrameTime		= mCurrentTime - mLastTime;

	// animate camera orientation
	if( mGyroEnabled == true && MotionManager::isGyroAvailable() ) {
		//camOrientation = MotionManager::getRotation( getOrientation() );//glm::slerp( MotionManager::getRotation( getOrientation() ), mCameraOrientation, glm::clamp( mFrameTime * 2.0f, 0.0f, 1.0f ) );
		mCamera.setOrientation( glm::slerp( mCamera.getOrientation(), MotionManager::getRotation( getOrientation() ), glm::clamp( mFrameTime * 10.0f, 0.0f, 1.0f ) ) );
	}
	else
		mCamera.setOrientation( glm::slerp( mCamera.getOrientation(), mCameraOrientation, glm::clamp( mFrameTime * 5.0f, 0.0f, 1.0f ) ) );

	const auto& spline0	= mTerrain->getRoadSpline3d( 0 );
	const auto& spline1	= mTerrain->getRoadSpline3d( 1 );
	if( spline0.getNumControlPoints() > 3 ) {
		vec3 eyeTarget0			= spline0.getPosition( mCameraSplinePos );
		vec3 eyeTarget1			= spline1.getPosition( mCameraSplinePos );
		vec3 eyeTarget			= glm::mix( eyeTarget1, eyeTarget0, mTerrain->getHeightMapProgression() );
		eyeTarget.y				*= mTerrain->getElevation();
		eyeTarget.y				+= 4.0f;

		if( !mEditingTerrain ) {
			mCamera.setEyePoint( eyeTarget );
		}
		else {
			mCameraSplineDestination += ( mCameraSplinePos - mCameraSplineDestination ) * 0.4f;
		}
	}

	// animate other stuffs
	mTerrain->setSunDirection( normalize( mTerrain->getSunDirection() + ( mSunDirection - mTerrain->getSunDirection() ) * mFrameTime * 5.0f ) );

	disableFrameRate();
	gl::enableVerticalSync(false);

	mLastTime = mCurrentTime;
}


void Grove::draw()
{
	gl::clear( ColorA::black() );

	// MARK: Render the world offscreen to the fbo
	if( mFbo ){
		gl::ScopedFramebuffer scopedFbo( mFbo );
		gl::ScopedViewport scopedViewport( ivec2(0), mFbo->getSize() );
		gl::ScopedMatrices scopedMatrices;

		// we render to two attachments to be able to save info about each pixels
		GLenum drawBuffers[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
		gl::drawBuffers( 2, &drawBuffers[0] );

		gl::clear( ColorA( 0, 0, 0, 0 ) );

		CameraPersp cam = mCamera;
		gl::setMatrices( cam );

		if( mTerrain ){
			mTerrain->render( cam );
		}
	}

	// MARK: Render the fbo texture and ui elements
	{
		gl::ScopedViewport scopedViewport( ivec2(0), toPixels( getWindowSize() ) );
		gl::ScopedMatrices scopedMatrices;
		gl::setMatricesWindow( toPixels( getWindowSize() ) );


		// draw the result to the screen and apply post processing
		gl::ScopedColor scopedColor( ColorA::white() );
		gl::ScopedTextureBind scopedTexture( mFbo->getColorTexture(), 0 );
		if( mPostProcessing ) {
			gl::ScopedGlslProg scopedShader( mPostProcessing );
			mPostProcessing->uniform( "uTexture", 0 );

			gl::drawSolidRect( toPixels( getWindowBounds() ) );
		}


		// MARK: Output to Twitter or to Wallpaper
		if( mCopySurfaceToTwitter || mCopySurfaceToWallpaper ) {

			Surface8u surface8u( mFbo->getWidth(), mFbo->getHeight(), true );

			// read the content from the gpu
			GLint oldPackAlignment;
			glGetIntegerv( GL_PACK_ALIGNMENT, &oldPackAlignment );
			glPixelStorei( GL_PACK_ALIGNMENT, 1 );
			glReadBuffer( GL_BACK );
			glReadPixels( 0, 0, mFbo->getWidth(), mFbo->getHeight(), GL_RGBA, GL_UNSIGNED_BYTE, surface8u.getData() );
			glPixelStorei( GL_PACK_ALIGNMENT, oldPackAlignment );

			// copy the result to a surface
			Surface surface = surface8u;
			ip::flipVertical( &surface );

			if( mCopySurfaceToTwitter ) {
				try {
					ci::android::launchTwitter( "Picture taken in Grove from @simongeilfus @androidexperiment", &surface );
				}
				catch( const ci::Exception &exc ){
					CI_LOG_E( "Can't launch twitter" );
				}
			}
			else {
				try {
					ci::android::setWallpaper( surface );
				}
				catch( const ci::Exception &exc ){
					CI_LOG_E( "Can't set Wallapaer" );
				}
			}

			mFrameTime = 1.0f / 60.0f;
			mCopySurfaceToTwitter = mCopySurfaceToWallpaper = false;
		}

		// MARK: render the intro modal
		if( mIntro ){

			static gl::GlslProgRef texShader = gl::getStockShader( gl::ShaderDef().color().texture() );
			gl::ScopedGlslProg scopedShader( texShader );

			gl::enableAlphaBlending();

			gl::ScopedColor scopedColor0( ColorA::gray( 1.0f, mIntroOpacity[0] ) );
			gl::draw( mIntroTexture[0], mIntroRect[0] );
			gl::ScopedColor scopedColor1( ColorA::gray( 1.0f, mIntroOpacity[1] ) );
			gl::draw( mIntroTexture[1], mIntroRect[1] );
			gl::ScopedColor scopedColor2( ColorA::gray( 1.0f, mIntroOpacity[2] ) );
			gl::draw( mIntroTexture[2], mIntroRect[2] );
			gl::ScopedColor scopedColor3( ColorA::gray( 1.0f, mIntroOpacity[3] ) );
			gl::draw( mIntroTexture[3], mIntroRect[3] );

			gl::disableAlphaBlending();

			// render the intro menu
			if( mWidget && mWidget->isOpen() )
				mWidget->draw();
		}
		// MARK: render the menu modals
		else {

			// render the contextual menu
			if( mWidget && mWidget->isOpen() )
				mWidget->draw();

			// and the menu
			if( mOpenMenuButton && mOpenMenuButton->isOpen() )
				mOpenMenuButton->draw();

			if( mMenu && mMenu->isOpen() )
				mMenu->draw();

			// infos panel
			if( getWindowBounds().contains( ( mInfoRect + mInfoOffset ).getCenter() ) ){
				gl::ScopedColor scopedColor( ColorA::white() );
				gl::draw( mInfoTexture, mInfoRect + mInfoOffset );
			}
			// twitter panel
			if( getWindowBounds().contains( ( mTwitterRect + mTwitterOffset ).getCenter() ) ){
				gl::ScopedColor scopedColor( ColorA::white() );
				gl::draw( mTwitterTexture, mTwitterRect + mTwitterOffset );
			}
			// wallpaper panel
			if( getWindowBounds().contains( ( mWallpaperRect + mWallpaperOffset ).getCenter() ) ){
				gl::ScopedColor scopedColor( ColorA::white() );
				gl::draw( mWallpaperTexture, mWallpaperRect + mWallpaperOffset );
			}

		}

	}

	// Debug stuff
#if defined( DEBUG_OUTPUT )
	if( getAverageFps() < 20.0f ) gl::color( 1, 0, 0 );
	else if( getAverageFps() < 30.0f ) gl::color( 1, 0.4, 0.08 );
	else if( getAverageFps() > 30.0f ) gl::color( 0.36, 1.0, 0.19 );
	gl::drawSolidRect( Rectf( vec2( 0, getWindowHeight() - 3.0f ), vec2( getAverageFps() / 60.0f * getWindowWidth(), getWindowHeight() ) ) );

	static int every99Frames = 0;
	if( every99Frames == 0 ) {
		console() << getAverageFps() << endl;
		console() << "CPU culling: " << (int) ( mTerrain->cpuTimer4.getSeconds() * 1000.0f ) << endl;
		console() << "CPU terrain: " << (int) ( mTerrain->cpuTimer0.getSeconds() * 1000.0f ) << endl;
		console() << "CPU trees: " << (int) ( mTerrain->cpuTimer1.getSeconds() * 1000.0f ) << endl;
		console() << "CPU skybox: " << (int) ( mTerrain->cpuTimer2.getSeconds() * 1000.0f ) << endl;
		console() << "CPU occlusion: " << (int) ( mTerrain->cpuTimer3.getSeconds() * 1000.0f ) << endl;
		console() << "CPU update0: " << (int) ( mainTimer0.getSeconds() * 1000.0f ) << endl;
		console() << "CPU update1: " << (int) ( mainTimer1.getSeconds() * 1000.0f ) << endl;
		console() << "CPU update2: " << (int) ( mainTimer2.getSeconds() * 1000.0f ) << endl;
	}
	every99Frames = ( every99Frames + 1 ) % 99;

#endif
}

CINDER_APP( Grove, RendererGl( RendererGl::Options().msaa(0) ), []( App::Settings* settings ){
	settings->setMultiTouchEnabled();
	settings->setFullScreen( true );
})