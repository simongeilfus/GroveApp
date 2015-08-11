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
#include "cinder/Rand.h"
#include "cinder/Utilities.h"
#include "cinder/Timeline.h"
#include "cinder/Log.h"

using namespace ci;
using namespace ci::app;
using namespace std;


void Grove::randomizeSunColor()
{
	Rand rnd( mRandomSeed );
	timeline().applyPtr( &mSunColor, Color( ColorModel::CM_HSV, rnd.randFloat( 0.0f, 0.25f ), rnd.randFloat( 0.4f, 0.7f ), 0.75f ), rnd.randFloat( 0.75f, 1.0f ), EaseOutInQuad() )
	.updateFn( [this](){
		mTerrain->setSunColor( mSunColor );
	} );
}
void Grove::randomizeFogColor()
{
	Rand rnd( mRandomSeed );
	timeline().applyPtr( &mFogColor, Color( ColorModel::CM_HSV, rnd.randFloat( 0.25f, 1.0f ), rnd.randFloat( 0.4f, 0.7f ), rnd.randFloat( 0.25f, 0.8f ) ), 0.5f, EaseOutInQuad() )
	.updateFn( [this](){
		mTerrain->setFogColor( mFogColor );
	} );
}
void Grove::randomizeScatteringColor()
{
	Rand rnd( mRandomSeed );
	timeline().applyPtr( &mSunScatteringCoeffs, vec3( Color( ColorModel::CM_HSV, rnd.randFloat( 0.0f, 1.0f ), rnd.randFloat( 0.4f, 0.7f ), rnd.randFloat( 0.25f, 0.6f ) ) ), 0.5f, EaseOutInQuad() )
	.updateFn( [this](){
		mTerrain->setSunScatteringCoeffs( mSunScatteringCoeffs );
	} );
}
void Grove::editTrees()
{
	
}
void Grove::editTerrain()
{
	
}

void Grove::tap( const Touch &touch )
{
	if( mIntro || mModal ) {
		// if we're in the info panel check if the touch happen
		// over a link
		if( getWindowBounds().contains( ( mInfoRect + mInfoOffset ).getCenter() ) ){
			auto offset				= ( mInfoRect + mInfoOffset ).getUpperLeft();
			auto simonGeilfusDotCom = Rectf( vec2( 380, 490 ) + offset, vec2( 640, 550 ) + offset );
			auto brainsDotCom		= Rectf( vec2( 660, 320 ) + offset, vec2( 900, 370 ) + offset );
			
			if( simonGeilfusDotCom.contains( touch.mPos2d ) ){
				ci::launchWebBrowser( Url( "http://simongeilfus.com/" ) );
			}
			if( brainsDotCom.contains( touch.mPos2d ) ){
				ci::launchWebBrowser( Url( "http://brains-agency.be/" ) );
			}
		}
		
		// disable gestures when intro or modal
		return;
	}
	
	if( mGyroEnabled )
		edit( touch );
}
void Grove::doubleTap( const Touch &touch )
{
	// disable gestures when intro or modal
	if( mIntro || mModal ) return;
	
	if( !mGyroEnabled )
		edit( touch );
}
void Grove::edit( const Touch &touch )
{
	// disable gestures when intro or modal
	if( mIntro || mModal ) return;
	
	static bool populationDeleted = false;
	
	// get the pixel data
	vec4 data			= getPixelData( toPixels( touch.mPos2d ), mCamera );
	vec3 position		= vec3( data );
	Touch::Type type	= getPixelDataType( data );
	
	
	// MARK: edit terrain
	//---------------------------
	if( type == Touch::TOUCH_TERRAIN ){
		mModal = true;
		if( position.y > mTerrain->getElevation() / 5.0f ){
			float elevation = mTerrain->getElevation();
			//auto spline = mTerrain->getRoadSpline3d();
			mWidget = Widget::create( Widget::Format()
									 .position( touch.mPos2d )
									 .items( { "Elevation" } )
									 .onReleased( []( WidgetEvent event ) {} )
									 .onMoved( [this, elevation]( WidgetEvent event ) {
				auto item	= event.getItem();
				vec2 diff	= ( event.getPos() - item->getDownPosition() );
				vec2 newPos = item->getPressPosition() + diff;
				item->setPosition( vec2( item->getPressPosition().x, newPos.y ) );
				
				// adjust terrain elevation
				mElevation = glm::clamp( elevation - diff.y, 30.0f, 380.0f );
				mTerrain->setElevation( mElevation );
			} )
									 .onPressed( [this]( WidgetEvent event ){
				//mTerrain->removeTilesPopulation();
			} )
									 .onReleased( [this]( WidgetEvent event ){
				/*mTerrain->generateHeightMap();
				 mTerrain->generateTriangleHeightMap();
				 mTerrain->createTiles();*/
				event.getWidget()->close();
			} ) );
			
			mWidget->onCloseEnded( [this]( WidgetEvent event ){
				mModal = false;
			} );
		}
		else {
			mTerrain->setTileExplosionCenter( position );
			app::timeline().appendToPtr( &mTerrain->getTileExplosionSize(), 15.0f, 1.25f, EaseInOutQuad() );
			
			vector<string> items = { "Quit", "Mountains", "SoftMountains", "Plains" };
			if( populationDeleted ) items.push_back( "MountainsAndTrees" );
			
			mWidget = Widget::create( Widget::Format()
									 .position( touch.mPos2d )
									 .items( items )
									 .direction( touch.mPos2d.x - ( items.size() * 130 ) > 0 ? vec2( -1, 0 ) : vec2( 1, 0 ) ));
			
			mWidget->onClose( [this]( WidgetEvent event ){
				bool found = false;
				app::timeline().findEndTimeOf( &mTerrain->getTileExplosionSize(), &found );
				if( !found )
					app::timeline().appendToPtr( &mTerrain->getTileExplosionSize(), 0.001f, 1.5f, EaseOutInQuad() );
				
				// cancel camera animaiton
				timeline().removeTarget( &mCameraSplinePos );
				timeline().applyPtr( &mCameraSplineDestination, mCameraSplinePos, 1.0f, EaseInOutQuad() );
				
				// disable terrain editing
				timeline().add( [this](){
					mEditingTerrain = false;
				}, timeline().getCurrentTime() + 0.5f );
			} );
			mWidget->onCloseEnded( [this]( WidgetEvent event ){
				mModal = false;
			} );
			
			if( populationDeleted ) {
				mWidget->onPressed( "MountainsAndTrees", [this]( WidgetEvent event ){
					mRandomSeed = randFloat( -200, 200 );
					mTerrain->setNoiseSeed( mRandomSeed );
					mTerrain->regenerateTilesPopulation();
					populationDeleted = false;
					event.getWidget()->close();
				} );
			}
			
			mWidget->onPressed( "Mountains", [this]( WidgetEvent event ){
				mEditingTerrain = true;
				event.getWidget()->close();
				mTerrain->setNoiseSeed( randFloat( -200.0f, 200.0f ) );
				mTerrain->setNumNoiseOctaves( 8 );
				timeline().applyPtr( &mElevation, 400.0f, 2.0f, EaseInCubic() ).updateFn( [this](){
					mTerrain->setElevation( mElevation );
				} );
				mTerrain->generateHeightMap();
				mTerrain->generateTriangleHeightMap();
			} );
			mWidget->onPressed( "SoftMountains", [this]( WidgetEvent event ){
				mEditingTerrain = true;
				event.getWidget()->close();
				mTerrain->setNoiseSeed( randFloat( -200.0f, 200.0f ) );
				mTerrain->setNumNoiseOctaves( 1 );
				timeline().applyPtr( &mElevation, 130.0f, 2.0f, EaseInCubic() ).updateFn( [this](){
					mTerrain->setElevation( mElevation );
				} );
				mTerrain->generateHeightMap();
				mTerrain->generateTriangleHeightMap();
			} );
			mWidget->onPressed( "Plains", [this]( WidgetEvent event ){
				mEditingTerrain = true;
				event.getWidget()->close();
				mTerrain->setNoiseSeed( randFloat( -200.0f, 200.0f ) );
				mTerrain->setNumNoiseOctaves( 1 );
				timeline().applyPtr( &mElevation, 40.0f, 2.0f, EaseInCubic() ).updateFn( [this](){
					mTerrain->setElevation( mElevation );
				} );
				mTerrain->generateHeightMap();
				mTerrain->generateTriangleHeightMap();
			} );
			
		}
	}
	// MARK: Move around
	//---------------------------
	else if( type == Touch::Type::TOUCH_ROAD ){
		mModal = true;
		mWidget = Widget::create( Widget::Format()
								 .position( touch.mPos2d )
								 .direction( touch.mPos2d.x - 530 > 0 ? vec2( -1, 0 ) : vec2( 1, 0 ) )
								 .items( { "Arrow" } ) );
		
		mWidget->onPressed( "Arrow", [this, position]( WidgetEvent event ){
			
			mCameraDestination = position;
			
			const auto& spline	= mTerrain->getRoadSpline3d();
			float time			= mCameraSplinePos;
			vec3 pos			= spline.getPosition( time ) * vec3( 1.0f, mTerrain->getElevation(), 1.0f );
			float step			= 0.0005f;
			float dist			= length( pos - mCameraDestination );
			vec3 eyeNextTarget	= spline.getPosition( time + step ) * vec3( 1.0f, mTerrain->getElevation(), 1.0f );
			vec3 eyePrevTarget	= spline.getPosition( time - step ) * vec3( 1.0f, mTerrain->getElevation(), 1.0f );
			bool front			= length( eyeNextTarget - mCameraDestination ) < length( eyePrevTarget - mCameraDestination );
			bool newFront		= front;
			
			vector<vec3> samples = { pos };
			while( dist > 0.5f && front == newFront ){
				if( front ) {
					time += step;
				}
				else time -= step;
				eyeNextTarget				= spline.getPosition( time + step ) * vec3( 1.0f, mTerrain->getElevation(), 1.0f );
				eyePrevTarget				= spline.getPosition( time - step ) * vec3( 1.0f, mTerrain->getElevation(), 1.0f );
				pos							= spline.getPosition( time ) * vec3( 1.0f, mTerrain->getElevation(), 1.0f );
				mCameraSplineDestination	= time;
				dist						= length( pos - mCameraDestination );
				newFront					= length( eyeNextTarget - mCameraDestination ) < length( eyePrevTarget - mCameraDestination );
				
				samples.push_back( pos );
			}
			
			if( front ) {
				mCameraSplineDestination += step * 35.0f;
			}
			else mCameraSplineDestination -= step * 35.0f;
			
			if( samples.size() > 3 ){
				auto spline = BSpline3f( samples, 3, false, true );
				float length = spline.getLength( 0, 1 );
				timeline().applyPtr( &mCameraSplinePos, mCameraSplineDestination, length / 10.0f, EaseInOutQuad() );
			}
			event.getWidget()->close();
		} );
		mWidget->onCloseEnded( [this]( WidgetEvent event ){
			mModal = false;
		} );
	}
	
	// MARK: randomize sky colors
	//---------------------------
	else if( type == Touch::Type::TOUCH_SKY ){
		mModal = true;
		
		// detect whether the sun is currently visible
		vec3 view = mCamera.getViewDirection();
		vec3 sun = mTerrain->getSunDirection();
		bool sunVisible = glm::dot( view, sun ) > 0.5f;
		
		// and add the sun icon only if it is
		vector<string> items = { "Quit", "Fog", "Sky" };
		if( sunVisible ) items.push_back( "Sun" );
		
		// create the color widget
		mWidget = Widget::create( Widget::Format()
								 .position( touch.mPos2d )
								 .direction( touch.mPos2d.x - 530 > 0 ? vec2( -1, 0 ) : vec2( 1, 0 ) )
								 .items( items ) );
		mWidget->onPressed( "Fog", [this]( WidgetEvent event ){
			randomizeScatteringColor();
		} );
		mWidget->onPressed( "Sky", [this]( WidgetEvent event ){
			randomizeFogColor();
		} );
		if( sunVisible ){
			mWidget->onPressed( "Sun", [this]( WidgetEvent event ){
				randomizeSunColor();
			} );
		}
		mWidget->onCloseEnded( [this]( WidgetEvent event ){
			mModal = false;
		} );
	}
	
	// move sun around
	else if( type == Touch::Type::TOUCH_SUN ){
		mModal = true;
		mWidget = Widget::create( Widget::Format()
								 .position( touch.mPos2d )
								 .items( { "Move" } )
								 .onReleased( []( WidgetEvent event ) {} )
								 .onMoved( [this]( WidgetEvent event ) {
			auto item = event.getItem();
			vec2 newPos = item->getPressPosition() + ( event.getPos() - item->getDownPosition() );
			item->setPosition( newPos );
			if( !mGyroEnabled ) {
				mCameraUi.mouseDrag( event.getPos(), true, false, false );
				mCameraOrientation = mCameraUiCam.getOrientation();
			}
			
			// shoot a ray from the camera to get the 3d direction
			vec2 normPos = vec2( event.getPos() ) / vec2( getWindowSize() );
			normPos.y = 1.0f - normPos.y;
			Ray ray = mCamera.generateRay( normPos.x, normPos.y, getWindowAspectRatio() );
			mSunDirection = normalize( ( mCamera.getViewDirection() + ray.getDirection() ) * 0.5f );
		} )
								 .onPressed( [this]( WidgetEvent event ) {
			mCameraUi.mouseDown( event.getPos() );
		} ) );
		mWidget->onCloseEnded( [this]( WidgetEvent event ){
			mModal = false;
		} );
	}
	
	
	// MARK: edit trees
	//---------------------------
	else if( type == Touch::Type::TOUCH_TREE ){
		mModal = true;
		mTerrain->setTilePopulationExplosionCenter( position );
		app::timeline().appendToPtr( &mTerrain->getTilePopulationExplosionSize(), 12.0f, 1.25f, EaseInOutQuad() );
		mWidget = Widget::create( Widget::Format()
								 .position( touch.mPos2d )
								 .items( { "Quit", "NoTree", "TreeMix", "PineTree", "Tree" } )
								 .direction( touch.mPos2d.x - 650 > 0 ? vec2( -1, 0 ) : vec2( 1, 0 ) ) );
		mWidget->onClose( [this]( WidgetEvent event ){
			bool found = false;
			app::timeline().findEndTimeOf( &mTerrain->getTilePopulationExplosionSize(), &found );
			if( !found )
				app::timeline().appendToPtr( &mTerrain->getTilePopulationExplosionSize(), 0.001f, 1.5f, EaseOutInQuad() );
		} );
		mWidget->onCloseEnded( [this]( WidgetEvent event ){
			mModal = false;
		} );
		mWidget->onPressed( "NoTree", [this]( WidgetEvent event ){
			event.getWidget()->close();
			mTerrain->removeTilesPopulation();
			populationDeleted = true;
		} );
		mWidget->onPressed( "PineTree", [this]( WidgetEvent event ){
			event.getWidget()->close();
			mRandomSeed = randFloat( -200, 200 );
			mTerrain->setNoiseSeed( mRandomSeed );
			mTerrain->setTilePopulationBalance( 1.0f );
			mTerrain->regenerateTilesPopulation();
			populationDeleted = false;
		} );
		mWidget->onPressed( "Tree", [this]( WidgetEvent event ){
			event.getWidget()->close();
			mRandomSeed = randFloat( -200, 200 );
			mTerrain->setNoiseSeed( mRandomSeed );
			mTerrain->setTilePopulationBalance( -1.0f );
			mTerrain->regenerateTilesPopulation();
			populationDeleted = false;
		} );
		mWidget->onPressed( "TreeMix", [this]( WidgetEvent event ){
			event.getWidget()->close();
			mRandomSeed = randFloat( -200, 200 );
			mTerrain->setNoiseSeed( mRandomSeed );
			Rand rnd( mRandomSeed );
			mTerrain->setTilePopulationBalance( rnd.nextFloat( -0.1f, 0.15f ) );
			mTerrain->regenerateTilesPopulation();
			populationDeleted = false;
		} );
	}
}
void Grove::pan( const Touch &touch )
{
	// disable gestures when intro or modal
	if( mModal ) return;
	
	// update camera orientation
	if( touch.mId == 0 || touch.mId == 1 ){
		vec2 pos		= toPixels( touch.mPos2d );
		vec2 posNorm	= pos / vec2( toPixels( getWindowSize() ) );
		Ray ray			= mCamera.generateRay( posNorm.x, 1.0f - posNorm.y, getWindowAspectRatio() );
		
		vec3 dirDiff	= mCameraInitialViewDir - ray.getDirection();
		vec3 viewDir	= mCamera.getViewDirection() + dirDiff * 4.0f;
		
		// constrain the view y axis and update the view direction
		if( viewDir.y > -0.25 && viewDir.y < 0.25 ) {
			mCameraOrientation = glm::toQuat( alignZAxisWithTarget( -viewDir, vec3( 0, 1, 0 ) ) );
		}
	}
}
void Grove::panBegan( const Touch &touch )
{
	// set the initial view direction
	vec2 pos				= toPixels( touch.mPos2d );
	vec2 startPosNorm		= pos / vec2( toPixels( getWindowSize() ) );
	Ray ray					= mCamera.generateRay( startPosNorm.x, 1.0f - startPosNorm.y, getWindowAspectRatio() );
	mCameraInitialViewDir	= ray.getDirection();
}
void Grove::panEnded( const Touch &touch )
{
}
vec4 Grove::getPixelData( const ci::vec2 &position, const CameraPersp &cam )
{
	// bring window space position to fbo space
	vec2 pos = position / vec2( getWindowSize() ) * vec2( mFbo->getSize() );
	
	// sample the fbo to get the pixel data
	gl::ScopedFramebuffer scopedReadFbo( mFbo, GL_READ_FRAMEBUFFER );
	
	Area pickingArea( pos, ivec2( pos ) + ivec2( 1 ) );
	
	gl::readBuffer( GL_COLOR_ATTACHMENT1 );
	
	GLubyte pixels[4];
	//GLint oldPackAlignment;
	//glGetIntegerv( GL_PACK_ALIGNMENT, &oldPackAlignment );
	//glPixelStorei( GL_PACK_ALIGNMENT, 1 );
	gl::readPixels( pickingArea.getX1(), mFbo->getHeight() - pickingArea.getY2(), pickingArea.getWidth(), pickingArea.getHeight(), GL_RGBA, GL_UNSIGNED_BYTE, &pixels[0] );
	//glPixelStorei( GL_PACK_ALIGNMENT, oldPackAlignment );
	
	//float unpackedDepth = glm::dot( vec3( pixels[1], pixels[2], pixels[3] ) / 255.0f, vec3( 1.0, 1.0 / 255.0, 1.0 / 65025.0 ) );
	float unpackedDepth = static_cast<float>( pixels[0] ) / 255.0f + ( static_cast<float>( pixels[1] ) / 255.0f * 0.00390625 );
	float data			= static_cast<float>( pixels[2] );
	
	mat4 viewProj = cam.getProjectionMatrix() * cam.getViewMatrix();
	mat4 invViewProj = inverse( viewProj );
	
	vec4 clipSpace;
	clipSpace.x		= ( (float) pos.x / (float) mFbo->getWidth() ) * 2.0f - 1.0f;
	clipSpace.y		= ( 1.0f - (float) pos.y / (float) mFbo->getHeight() ) * 2.0f - 1.0f;
	clipSpace.z		= unpackedDepth * 2.0f - 1.0f;
	clipSpace.w		= 1.0f;
	vec4 worldSpace = invViewProj * clipSpace;
	return vec4( vec3( worldSpace ) / worldSpace.w, data );
}

Touch::Type Grove::getPixelDataType( const vec4 &data )
{
	Touch::Type type;
	
	if( data.w == 0.0 )
		type = Touch::Type::TOUCH_TERRAIN;
	else if( data.w == 1.0 )
		type = Touch::Type::TOUCH_ROAD;
	else if( data.w == 2.0 )
		type = Touch::Type::TOUCH_TREE;
	else if( data.w == 3.0 )
		type = Touch::Type::TOUCH_SKY;
	else if( data.w == 4.0 )
		type = Touch::Type::TOUCH_SUN;
	return type;
}