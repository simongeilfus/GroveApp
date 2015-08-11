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

#include "ShaderPreprocessor.h"

#include "glm/gtx/vector_angle.hpp"

#if defined( CINDER_ANDROID ) || defined( CINDER_COCOA_TOUCH )
#include "cinder/MotionManager.h"
#define CINDER_MOBILE
#else
#include "cinder/gl/TextureFont.h"
#include "cinder/gl/Query.h"
#include "utils/Watchdog.h"
#include "CinderImGui.h"
#endif

#define glCheckError() {			\
GLenum err = gl::getError();	\
if( err != GL_NO_ERROR )		\
CI_LOG_E( gl::getErrorString( err ) ); \
} \

using namespace ci;
using namespace ci::app;
using namespace std;

// Theme.NoTitleBar.OverlayActionModes

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
	CI_LOG_V( "Start Grove" );
	/*ObjLoader loader( loadFile( getOpenFilePath() ) );
	 TriMesh mesh( loader );
	 TriMesh mesh2( TriMesh::Format().positions().texCoords() );
	 for( int i = 0; i < mesh.getNumTriangles(); i++ ){
		vec3 a, b ,c;
		mesh.getTriangleVertices( i, &a, &b, &c );
		mesh2.appendVertex( a );
		mesh2.appendTexCoord( vec2(0) );
		mesh2.appendVertex( b );
		mesh2.appendTexCoord( vec2(0) );
		mesh2.appendVertex( c );
		mesh2.appendTexCoord( vec2(0) );
		mesh2.appendTriangle( i * 3, i * 3 + 1, i * 3 + 2 );
	 }
	 mesh2.write( writeFile( getSaveFilePath() ) );
	 quit();*/

#if defined( CINDER_ANDROID ) || defined( CINDER_COCOA_TOUCH )
	if( MotionManager::isGyroAvailable() ){

		MotionManager::enable();
		mGyroEnabled = true;
		CI_LOG_W( "MotionManagerEnabled." );
	}
#endif
	Timer startTimer( true );
	setupUi();
	console() << "setupUi " << startTimer.getSeconds() << endl;
	startTimer.stop();
	startTimer.start();
	setupRendering();
	console() << "setupRendering " << startTimer.getSeconds() << endl;

	// setup ui
#if !defined( CINDER_ANDROID ) && !defined( CINDER_COCOA_TOUCH )
	ui::initialize();
#endif

	// create a random seed from the current time_since_epoch
	using namespace std::chrono;
	auto now		= system_clock::now().time_since_epoch();
	seconds nows	= duration_cast<seconds>( now );

	randSeed( nows.count() );
	mRandomSeed		= randFloat( -200, 200 ); //30.0f;//

	CI_LOG_V( "Random seed: " << mRandomSeed );

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

	/*mDebugMenu = Widget::create( Widget::Format()
								.position( vec2( 120, 32 ) )
								.direction( vec2(1,0) )
								.autoClose( false )
								.items( { "Debug_TerrainRender", "Debug_TerrainTiles", "Debug_TerrainTrees", "Debug_TerrainSky", "Debug_TerrainOcclusionCulling", "Debug_Update", "Debug_Draw" } )
							 );

	mDebugMenu->onReleased( "Debug_TerrainRender", [this]( const WidgetEvent &event ) { renderTerrain = !renderTerrain; } );
	mDebugMenu->onReleased( "Debug_TerrainTiles", [this]( const WidgetEvent &event ) { mTerrain->renderTerrain = !mTerrain->renderTerrain; } );
	mDebugMenu->onReleased( "Debug_TerrainTrees", [this]( const WidgetEvent &event ) { mTerrain->renderTrees = !mTerrain->renderTrees; } );
	mDebugMenu->onReleased( "Debug_TerrainSky", [this]( const WidgetEvent &event ) { mTerrain->renderSky = !mTerrain->renderSky; } );
	mDebugMenu->onReleased( "Debug_TerrainOcclusionCulling", [this]( const WidgetEvent &event ) { mTerrain->setOcclusionCullingEnabled( !mTerrain->isOcclusionCullingEnabled() ); } );
	mDebugMenu->onReleased( "Debug_Update", [this]( const WidgetEvent &event ) { skipUpdate = !skipUpdate; } );
	mDebugMenu->onReleased( "Debug_Draw", [this]( const WidgetEvent &event ) { skipDraw = !skipDraw; } );*/
	
	timeline().stepTo( mLastTime );
}

Grove::~Grove()
{
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

#if !defined( CINDER_ANDROID ) && !defined( CINDER_COCOA_TOUCH )
	wd::watch( "Shaders/PostProcessing.frag", [=]( const fs::path &p ) {
#endif
		try {
			gl_temp::ShaderPreprocessor pp;
			pp.addDefine( "CINDER_DESKTOP", "0" );
			pp.addDefine( "CINDER_GL_ES_3",	"1" );
#if !defined( CINDER_ANDROID ) && !defined( CINDER_COCOA_TOUCH )
			pp.addDefine( "CINDER_GL_PLATFORM", "CINDER_DESKTOP" );
			pp.setVersion( 150 );
#else
			pp.addDefine( "CINDER_GL_PLATFORM", "CINDER_GL_ES_3" );
			pp.setVersion( 300 );
#endif
			mPostProcessing = gl::GlslProg::create( gl::GlslProg::Format().vertex( pp.parse( getAssetPath( "Shaders/Passtrough.vert" ) ).c_str() ).fragment( pp.parse( getAssetPath( "Shaders/PostProcessing.frag" ) ).c_str() ) );
		}
		catch( gl::GlslProgExc exc ){
			CI_LOG_E( exc.what() );
		}
#if !defined( CINDER_ANDROID ) && !defined( CINDER_COCOA_TOUCH )
	} );
#endif
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
#if !defined( CINDER_ANDROID ) && !defined( CINDER_COCOA_TOUCH )
	getWindow()->getSignalMouseDown().connect( [this]( MouseEvent& event ){
		//mCameraUi.mouseDown( event.getPos() );
	});
	getWindow()->getSignalMouseDrag().connect( [this]( MouseEvent& event ){
		//mCameraUi.mouseDrag( event.getPos(), event.isLeftDown(), event.isMiddleDown(), event.isRightDown() );
	});
#endif

}


// MARK: Update / Draw
Timer mainTimer0, mainTimer1, mainTimer2, mainTimer3;
static bool animate = true;
void Grove::update()
{
	if( skipUpdate ) return;

	// update frame time
	mCurrentTime	= getElapsedSeconds();
	mFrameTime		= mCurrentTime - mLastTime;

	mainTimer0.start();


	// animate camera orientation
#if defined( CINDER_ANDROID ) || defined( CINDER_COCOA_TOUCH )
	if( mGyroEnabled == true && MotionManager::isGyroAvailable() ) {
		//camOrientation = MotionManager::getRotation( getOrientation() );//glm::slerp( MotionManager::getRotation( getOrientation() ), mCameraOrientation, glm::clamp( mFrameTime * 2.0f, 0.0f, 1.0f ) );
		mCamera.setOrientation( glm::slerp( mCamera.getOrientation(), MotionManager::getRotation( getOrientation() ), glm::clamp( mFrameTime * 10.0f, 0.0f, 1.0f ) ) );
	}
	else
#endif
	mCamera.setOrientation( glm::slerp( mCamera.getOrientation(), mCameraOrientation, glm::clamp( mFrameTime * 5.0f, 0.0f, 1.0f ) ) );

	mainTimer0.stop();


	mainTimer1.start();
	// animate camera along spline
	const auto& spline0	= mTerrain->getRoadSpline3d( 0 );
	const auto& spline1	= mTerrain->getRoadSpline3d( 1 );
	if( spline0.getNumControlPoints() > 3 ) {
		vec3 eyeTarget0			= spline0.getPosition( mCameraSplinePos );
		vec3 eyeTarget1			= spline1.getPosition( mCameraSplinePos );
		vec3 eyeTarget			= glm::mix( eyeTarget1, eyeTarget0, mTerrain->getHeightMapProgression() );
		eyeTarget.y				*= mTerrain->getElevation();
		eyeTarget.y				+= 4.0f;

		if( !mEditingTerrain ) {
			mCamera.setEyePoint( eyeTarget );// glm::mix( mCamera.getEyePoint(), eyeTarget, mFrameTime * 20.0f ) );
		}
		else {
			mCameraSplineDestination += ( mCameraSplinePos - mCameraSplineDestination ) * 0.4f;
		}
	}

	mainTimer1.stop();

	mainTimer2.start();
	// animate other stuffs
	mTerrain->setSunDirection( normalize( mTerrain->getSunDirection() + ( mSunDirection - mTerrain->getSunDirection() ) * mFrameTime * 5.0f ) );
	mainTimer2.stop();

	disableFrameRate();
	gl::enableVerticalSync(false);

	mLastTime = mCurrentTime;
}



#if !defined( CINDER_ANDROID ) && !defined( CINDER_COCOA_TOUCH )
static gl::QueryTimeSwappedRef gpuTimer;
#endif
void Grove::draw()
{
	gl::clear( ColorA::black() );

	if( skipDraw ){
		debugUi();
		//mDebugMenu->draw();

		if( getAverageFps() < 20.0f ) gl::color( 1, 0, 0 );
		else if( getAverageFps() < 30.0f ) gl::color( 1, 0.4, 0.08 );
		else if( getAverageFps() > 30.0f ) gl::color( 0.36, 1.0, 0.19 );
		gl::drawSolidRect( Rectf( vec2( 0, getWindowHeight() - 3.0f ), vec2( getAverageFps() / 60.0f * getWindowWidth(), getWindowHeight() ) ) );

#if defined( CINDER_ANDROID ) || defined( CINDER_COCOA_TOUCH )
		static int every99Frames = 0;
		if( every99Frames == 0 ) {
			console() << getAverageFps() << endl;
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

		return;
	}
	// MARK: Render the world offscreen to the fbo
	if( mFbo ){
		gl::ScopedFramebuffer scopedFbo( mFbo );
		gl::ScopedViewport scopedViewport( ivec2(0), mFbo->getSize() );
		gl::ScopedMatrices scopedMatrices;
		glCheckError();

		// we render to two attachments to be able to save info about each pixels
		GLenum drawBuffers[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
		gl::drawBuffers( 2, &drawBuffers[0] );
		glCheckError();

		gl::clear( ColorA( 0, 0, 0, 0 ) );
		glCheckError();
		
#if !defined( CINDER_ANDROID ) && !defined( CINDER_COCOA_TOUCH )
		CameraPersp cam = ( !animate ? mCameraUiCam : mCamera );
#else
		CameraPersp cam = mCamera;
#endif
		gl::setMatrices( cam );
		glCheckError();
		if( mTerrain && renderTerrain ){
			mTerrain->render( cam );
		}
		glCheckError();
	}

#if !defined( CINDER_ANDROID ) && !defined( CINDER_COCOA_TOUCH )
	if( !gpuTimer ){
		gpuTimer = gl::QueryTimeSwapped::create();
	}
	gpuTimer->begin();
#endif

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
			//mPostProcessing->uniform( "uInvSize", vec2( 1.0 ) / vec2( mFbo->getSize() ) );

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

#if defined( CINDER_ANDROID )

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
#else
			auto p = getSaveFilePath();
			writeImage( writeFile( p ), surface );
#endif

			mFrameTime = 1.0f / 60.0f;
			mCopySurfaceToTwitter = mCopySurfaceToWallpaper = false;
		}

		// MARK: render the intro modal
		if( mIntro ){

			static gl::GlslProgRef texShader = gl::getStockShader( gl::ShaderDef().color().texture() );
			gl::ScopedGlslProg scopedShader( texShader );

#if defined( CINDER_COCOA_TOUCH )
			gl::enableAlphaBlending( true );
#else
			gl::enableAlphaBlending();
#endif

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
			{
				//gl::ScopedColor scopedColor2( ColorA::white() );
				//gl::draw( mDirectivesTexture, mDirectivesRect + mDirectivesOffset );
			}

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
			/*// regenerate panel
			if( getWindowBounds().contains( ( mRegenerateRect + mRegenerateOffset ).getCenter() ) ){
				gl::ScopedColor scopedColor( ColorA::white() );
				gl::draw( mRegenerateTexture, mRegenerateRect + mRegenerateOffset );
			}*/
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
			// compass panel
			/*if( getWindowBounds().contains( ( mCompassRect + mCompassOffset ).getCenter() ) ){
				gl::ScopedColor scopedColor( ColorA::white() );
				gl::draw( mCompassTexture[mGyroEnabled], mCompassRect + mCompassOffset );
			}*/

		}

	}
#if !defined( CINDER_ANDROID ) && !defined( CINDER_COCOA_TOUCH )
	gpuTimer->end();
#endif

	
	// Debug
	if( showUi )
		debugUi();
	//mDebugMenu->draw();

	
#if defined( DEBUG_OUTPUT )
	if( getAverageFps() < 20.0f ) gl::color( 1, 0, 0 );
	else if( getAverageFps() < 30.0f ) gl::color( 1, 0.4, 0.08 );
	else if( getAverageFps() > 30.0f ) gl::color( 0.36, 1.0, 0.19 );
	gl::drawSolidRect( Rectf( vec2( 0, getWindowHeight() - 3.0f ), vec2( getAverageFps() / 60.0f * getWindowWidth(), getWindowHeight() ) ) );

#if defined( CINDER_ANDROID )
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
#endif
	// check for opengl errors
	glCheckError();
}
void Grove::mouseDown( MouseEvent event )
{
	if( !animate ) return;
		Touch t;
		t.mId				= 1;
		t.mPos2d			= event.getPos();
		t.mInitialPos2d		= event.getPos();
		vec4 data			= getPixelData( event.getPos(), mCamera );
		t.mPos3d			= vec3( data );
		t.mType				= getPixelDataType( data );
		t.mDistanceTraveled = 0.0f;
	t.mTimeOfBirth		= getElapsedSeconds();
	if( event.isShiftDown() ){
		doubleTap( t );
	}
	else if( event.isAltDown() ){
		tap( t );
	}
	else {
		panBegan( t );
	}
}
void Grove::mouseDrag( MouseEvent event )
{
	if( !animate ) return;
	Touch t;
	t.mId				= 1;
	t.mPos2d			= event.getPos();
	t.mInitialPos2d		= event.getPos();
	t.mDistanceTraveled = 0.0f;
	t.mIsPan			= true;
	t.mTimeOfBirth		= getElapsedSeconds();
	pan( t );
}



void Grove::keyDown( KeyEvent event )
{
	switch( event.getCode() ){
		case KeyEvent::KEY_f : {
			setFullScreen( !isFullScreen() );
		} break;
			
		case KeyEvent::KEY_u : {
			showUi = !showUi;
		} break;
		case KeyEvent::KEY_h : {
			hideCursor();
		} break;
		case KeyEvent::KEY_s : {
			showCursor();
		} break;
		case KeyEvent::KEY_r : {
			mTerrain->regenerateTilesPopulation();
		} break;
		case KeyEvent::KEY_c : {
			mTerrain->removeTilesPopulation();
		} break;
		case KeyEvent::KEY_SPACE : {
			cout << "SPACE" << endl;
			mTerrain->setNoiseSeed( randFloat( -200.0f, 200.0f ) );
			mTerrain->generateHeightMap();
			mTerrain->generateTriangleHeightMap();
			//mTerrain->regenerateTilesPopulation();
		} break;
		case KeyEvent::KEY_0 : {
			mTerrain->setNoiseSeed( 0.0f );
			mTerrain->generateHeightMap();
			mTerrain->generateTriangleHeightMap();
			mTerrain->regenerateTilesPopulation();
		} break;
		case KeyEvent::KEY_1 : {
			mTerrain->setNoiseSeed( 1.0f );
			mTerrain->generateHeightMap();
			mTerrain->generateTriangleHeightMap();
			//mTerrain->regenerateTilesPopulation();
		} break;
		case KeyEvent::KEY_2 : {
			mTerrain->setNoiseSeed( 2.0f );
			mTerrain->generateHeightMap();
			mTerrain->generateTriangleHeightMap();
			//mTerrain->regenerateTilesPopulation();
		} break;
		case KeyEvent::KEY_3 : {
			mTerrain->setNoiseSeed( -3.0f );
			mTerrain->generateHeightMap();
			mTerrain->generateTriangleHeightMap();
			//mTerrain->regenerateTilesPopulation();
		} break;
		case KeyEvent::KEY_4 : {
			mTerrain->setNoiseSeed( 4.5f );
			mTerrain->generateHeightMap();
			mTerrain->generateTriangleHeightMap();
			//mTerrain->regenerateTilesPopulation();
		} break;
		case KeyEvent::KEY_5 : {
			mTerrain->setNoiseSeed( 5.123423f );
			mTerrain->generateHeightMap();
			mTerrain->generateTriangleHeightMap();
			//mTerrain->regenerateTilesPopulation();
		} break;
	}
}

void Grove::debugUi()
{
#if !defined( CINDER_ANDROID ) && !defined( CINDER_COCOA_TOUCH )
	float completion = mTerrain->getTilesThreadsCompletion();
	ui::SliderFloat( "Completion", &completion, 0.0f, 1.0f, "%.3f%" );

	/*if( ui::Checkbox( "Occlusion Culling", &checkOcclusion ) ){
		mTerrain->setOcclusionCullingEnabled( checkOcclusion );
	 }*/
	ui::Checkbox( "RenderTerrain", &renderTerrain );
	ui::Checkbox( "Animate", &animate );
	ui::Checkbox( "Bounds", &mTerrain->showBounds );
	ui::Checkbox( "SkipUpdate", &skipUpdate );
	ui::Checkbox( "SkipDraw", &skipDraw );
	//ui::Checkbox( "Paused", &paused );
	//ui::SliderFloat( "Speed", &speed, -0.0001f, 0.0001f, "%.5f" );
	if( true ) {
		ui::Value( "GPU main: ", (int) gpuTimer->getElapsedMilliseconds() );
		ui::Value( "CPU update0: ", (int) ( mainTimer0.getSeconds() * 1000.0f ) );
		ui::Value( "CPU update1: ", (int) ( mainTimer1.getSeconds() * 1000.0f ) );
		ui::Value( "CPU update2: ", (int) ( mainTimer2.getSeconds() * 1000.0f ) );
	ui::Value( "GPU terrain: ", (int) mTerrain->timer0->getElapsedMilliseconds() );
	ui::Value( "GPU trees: ", (int) mTerrain->timer1->getElapsedMilliseconds() );
	ui::Value( "GPU skybox: ", (int) mTerrain->timer2->getElapsedMilliseconds() );
		ui::Value( "GPU occlusion: ", (int) mTerrain->timer3->getElapsedMilliseconds() ); }
	ui::Value( "CPU terrain: ", (int) ( mTerrain->cpuTimer0.getSeconds() * 1000.0f ) );
	ui::Value( "CPU trees: ", (int) ( mTerrain->cpuTimer1.getSeconds() * 1000.0f ) );
	ui::Value( "CPU skybox: ", (int) ( mTerrain->cpuTimer2.getSeconds() * 1000.0f ) );
	ui::Value( "CPU occlusion: ", (int) ( mTerrain->cpuTimer3.getSeconds() * 1000.0f ) );

	ui::Value( "Rendered instances", (int) mTerrain->getNumRenderedInstances() );
	//ui::Value( "cpu", (int) ( timer.getSeconds() * 1000.0f ), "%.0fms" );
	//ui::Value( "gpu", (int) glTimerDraw->getElapsedMilliseconds(), "%.0fms" );

	// fog ui
	{
		ui::ScopedWindow window( "Fog" );
		if( ui::SliderFloat( "density", &mFogDensity, 0, 1.0f ) ||
		   ui::ColorEdit3( "color", &mFogColor[0] ) ||
		   ui::SliderFloat( "sunDispertion", &mSunDispertion, 0, 1 ) ||
		   ui::SliderFloat( "sunIntensity", &mSunIntensity, 0, 1 ) ||
		   ui::ColorEdit3( "sunColor", &mSunColor[0] ) ||
		   ui::ColorEdit3( "scatteringCoeffs", &mSunScatteringCoeffs[0] ) ) {

			mTerrain->setFogDensity( mFogDensity );
			mTerrain->setFogColor( mFogColor );
			mTerrain->setSunDispertion( mSunDispertion );
			mTerrain->setSunIntensity( mSunIntensity );
			mTerrain->setSunColor( mSunColor );
			mTerrain->setSunScatteringCoeffs( mSunScatteringCoeffs );
		}
	}

	ui::Value( "Fps", (int) getAverageFps() );
	ui::Value( "Tiles", (int) mTerrain->getTiles().size() );

	static float fov = mCameraUiCam.getFov();
	if( ui::SliderFloat( "Fov", &fov, 1, 120 ) ){
		//CameraPersp camFov = mCameraUi.getCamera();
		//camFov.setFov( fov );
		mCameraUiCam.setFov( fov );
		mCamera.setFov( fov );
		//mCameraUi.setCamera( &camFov );
	}

	if( ui::Button( "Regenerate" ) ) {
		mTerrain = Terrain::create();/* Terrain::Format()
								   .noiseOctaves( octaves )
								   .noiseScale( noiseScale )
								   .noiseSeed( noiseSeed )
								   .roadBlurIterations( roadBlurIterations )
								   .sobelBlurIterations( sobelBlurIterations )
								   .blurIterations( blurIterations ) );*/
	}

	/*if( ui::InputInt( "octaves", &octaves, 1, 1, 16 ) ||
	   ui::InputFloat( "noiseScale", &noiseScale, 0.1f ) ||
	   ui::InputInt( "noiseSeed", &noiseSeed ) ||
	   ui::InputInt( "roadBlurIterations", &roadBlurIterations ) ||
	   ui::InputInt( "sobelBlurIterations", &sobelBlurIterations ) ||
	   ui::InputInt( "blurIterations", &blurIterations ) ){

		if( mAutoRegenerate ){
			mTerrain = Terrain::create( Terrain::Format()
									   .noiseOctaves( octaves )
									   .noiseScale( noiseScale )
									   .noiseSeed( noiseSeed )
									   .roadBlurIterations( roadBlurIterations )
									   .sobelBlurIterations( sobelBlurIterations )
									   .blurIterations( blurIterations ) );
		}
		else {
			mTerrain->setNumNoiseOctaves( octaves );
			mTerrain->setNoiseScale( noiseScale );
			mTerrain->setNoiseSeed( noiseSeed );
			mTerrain->setNumRoadBlurIterations( roadBlurIterations );
			mTerrain->setNumSobelBlurIterations( sobelBlurIterations );
			mTerrain->setNumBlurIterations( blurIterations );
			mTerrain->generateHeightMap();
		}
	}

	roadBlurIterations = glm::clamp( roadBlurIterations, 1, 100 );
	sobelBlurIterations = glm::clamp( sobelBlurIterations, 1, 100 );
	blurIterations = glm::clamp( blurIterations, 1, 100 );
	octaves = glm::clamp( octaves, 3, 50 );
	ui::SliderFloat( "elevation", &elevation, 0.0f, 1000.0f );

	ui::Checkbox( "Auto-Regenerate", &mAutoRegenerate );*/
	static bool showTextures = false;
	ui::Checkbox( "Show Textures", &showTextures );

	if( showTextures ){
		vector<string> textureNames = {
			"HeightMap",
			"MeshDensityMap",
			"FloraDensityMap",
			"TrianglesHeightMap",
			"TestFloraDensityMap"
		};
		vector<gl::Texture2dRef> textures = {
			mTerrain->getHeightMap(),
			mTerrain->getMeshDensityMap(),
			mTerrain->getFloraDensityMap(),
			mTerrain->getTrianglesHeightMap(),
			gl::Texture2d::create( *mTerrain->getFloraDensityChannel() )
		};

		string texturesNames;
		for( auto name : textureNames ){
			texturesNames += name + '\0';
		}

		static int currentMap = 0;
		ui::Combo( "Maps", &currentMap, texturesNames.c_str() );
		gl::ScopedViewport viewport( ivec2(0), getWindowSize() );
		gl::ScopedMatrices matrices;
		gl::setMatricesWindow( getWindowSize() );
		gl::color( ColorA::white() );
		if( textures[currentMap] )
			gl::draw( textures[currentMap], getWindowBounds() );
	}
#endif


}

CINDER_APP( Grove, RendererGl( RendererGl::Options().msaa(0) ), []( App::Settings* settings ){
	settings->setMultiTouchEnabled();
#if !defined( CINDER_ANDROID ) && !defined( CINDER_COCOA_TOUCH )
	settings->setWindowSize( 1920, 1080 );
	settings->setWindowPos( 0, 40 );
#endif
#if defined( CINDER_ANDROID )
	settings->setFullScreen( true );
#endif
#if defined( CINDER_COCOA_TOUCH )
	settings->setStatusBarEnabled( false );
	//settings->setHighDensityDisplayEnabled( false );
#endif
})
