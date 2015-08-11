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


#define glCheckError() {			\
GLenum err = gl::getError();	\
if( err != GL_NO_ERROR )		\
CI_LOG_E( gl::getErrorString( err ) ); \
} \

#include "Terrain.h"

#include "cinder/gl/Fbo.h"
#include "cinder/gl/Batch.h"
#include "cinder/gl/scoped.h"
#include "cinder/gl/draw.h"

#include "ShaderPreprocessor.h"

#include "cinder/ip/Flip.h"
#include "cinder/Log.h"
#include "cinder/Perlin.h"
#include "cinder/Rand.h"
#include "cinder/Frustum.h"
#include "cinder/Utilities.h"

#include "glm/gtc/noise.hpp"

#include "PoissonDiskDistribution.h"
#include "Triangulation.h"

#if !defined( CINDER_ANDROID ) || !defined( CINDER_COCOA_TOUCH )
#include "utils/Watchdog.h"
#endif

using namespace std;
using namespace ci;



// MARK: rendering, fbo and shader utils
namespace {
	
	gl::Texture2dRef blitFromFbo( const gl::FboRef &fbo, const gl::Texture2d::Format &texFormat )
	{
		// create a new texture and a temporary fbo
		auto tex = gl::Texture2d::create( fbo->getWidth(), fbo->getHeight(), texFormat );
		gl::Fbo::Format fboFormat;
		fboFormat.attachment( GL_COLOR_ATTACHMENT0, tex );
		auto blitFbo = gl::Fbo::create( fbo->getWidth(), fbo->getHeight(), fboFormat );
		auto area = Area( ivec2(0), ivec2(fbo->getWidth(), fbo->getHeight()) );
		
		// blit the framebuffer to the temp one
		glBindFramebuffer( GL_READ_FRAMEBUFFER, fbo->getId() );
		glBindFramebuffer( GL_DRAW_FRAMEBUFFER, blitFbo->getId() );
		glBlitFramebuffer( area.getX1(), area.getY1(), area.getX2(), area.getY2(), area.getX1(), area.getY1(), area.getX2(), area.getY2(), GL_COLOR_BUFFER_BIT, GL_NEAREST );
		glBindFramebuffer( GL_READ_FRAMEBUFFER, 0 );
		glBindFramebuffer( GL_DRAW_FRAMEBUFFER, 0 );
		return tex;
	}
	void blitToFbo( const gl::FboRef &source, const gl::FboRef &destination )
	{
		
		// blit the framebuffer to the temp one
		auto area = Area( ivec2(0), ivec2(source->getWidth(), source->getHeight()) );
		glBindFramebuffer( GL_READ_FRAMEBUFFER, source->getId() );
		glBindFramebuffer( GL_DRAW_FRAMEBUFFER, destination->getId() );
		glBlitFramebuffer( area.getX1(), area.getY1(), area.getX2(), area.getY2(), area.getX1(), area.getY1(), area.getX2(), area.getY2(), GL_COLOR_BUFFER_BIT, GL_NEAREST );
		glBindFramebuffer( GL_READ_FRAMEBUFFER, 0 );
		glBindFramebuffer( GL_DRAW_FRAMEBUFFER, 0 );
	}
	void blitToFbo( const gl::Texture2dRef &source, const gl::FboRef &destination )
	{
		// create a temporary fbo
		gl::Fbo::Format fboFormat;
		fboFormat.attachment( GL_COLOR_ATTACHMENT0, source );
		auto blitFbo = gl::Fbo::create( destination->getWidth(), destination->getHeight(), fboFormat );
		auto area = Area( ivec2(0), ivec2(destination->getWidth(), destination->getHeight()) );
		
		// blit the framebuffer to the temp one
		glBindFramebuffer( GL_READ_FRAMEBUFFER, blitFbo->getId() );
		glBindFramebuffer( GL_DRAW_FRAMEBUFFER, destination->getId() );
		glBlitFramebuffer( area.getX1(), area.getY1(), area.getX2(), area.getY2(), area.getX1(), area.getY1(), area.getX2(), area.getY2(), GL_COLOR_BUFFER_BIT, GL_NEAREST );
		glBindFramebuffer( GL_READ_FRAMEBUFFER, 0 );
		glBindFramebuffer( GL_DRAW_FRAMEBUFFER, 0 );
	}
	
	//! returns the cpu copy of a texture / opengl-es & iphone compatible
	Channel32fRef getTextureAsChannel( const gl::Texture2dRef &texture )
	{
		// create a temp fbo with the texture as attachment
		gl::Fbo::Format fboFormat;
		fboFormat.attachment( GL_COLOR_ATTACHMENT0, texture );
		auto readFbo = gl::Fbo::create( texture->getSize().x, texture->getSize().y, fboFormat );
		
		// bind the fbo
		gl::ScopedFramebuffer scopedReadFbo( readFbo, GL_READ_FRAMEBUFFER );
		gl::readBuffer( GL_COLOR_ATTACHMENT0 );
		
		// read the pixels and return the converted Channel
		GLubyte* pixels = new GLubyte[ readFbo->getWidth() * readFbo->getHeight() * 4 ];
		gl::readPixels( 0, 0, readFbo->getWidth(), readFbo->getHeight(), GL_RGBA, GL_UNSIGNED_BYTE, &pixels[0] );
		auto channel = Channel32f::create( readFbo->getWidth(), readFbo->getHeight() );
		
		auto iter = channel->getIter();
		while( iter.line() ){
			while( iter.pixel() ){
				auto pos = iter.getPos();
				int i = pos.y * readFbo->getWidth() + pos.x;
				iter.v() = static_cast<float>( static_cast<int>( pixels[i*4] ) ) / 255.0f;
			}
		}
		
		delete [] pixels;
		
		return channel;
	}
	
	gl::GlslProgRef loadShader( const string &vertex, const string &fragment = "", const gl::GlslProg::Format &format = gl::GlslProg::Format() )
	{
		string fragmentName = fragment.empty() ? vertex : fragment;
		
		// prepare preprocessor
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
	
		
		// try to compile the shader
		gl::GlslProgRef shader;
		try {
			auto shaderFormat = format;
			auto vertSourcePath = app::getAssetPath( "Shaders/" + vertex + ".vert" );
			auto fragSourcePath = app::getAssetPath( "Shaders/" + fragmentName + ".frag" );
			shaderFormat.vertex( pp.parse( vertSourcePath ).c_str() )
						.fragment( pp.parse( fragSourcePath ).c_str() );
			
			shader = gl::GlslProg::create( shaderFormat );
		}
		catch( const gl::GlslProgExc &exc ){
			CI_LOG_E( "Error trying to compile " << fragmentName << " : " << exc.what() );
		}
		return shader;
	}
	
	gl::GlslProgRef loadShader( const string &name, const gl::GlslProg::Format &format )
	{
		return loadShader( name, name, format );
	}
	
} // anonymous namespace


// MARK: Terrain
TerrainRef Terrain::create( const Format &format )
{
	return make_shared<Terrain>( format, LeaveMeAlone() );
}

Terrain::Terrain( const Format &format, LeaveMeAlone access )
: mSize( format.getSize() ),
mElevation( format.getElevation() ),
mArea( ivec2(0), ivec2( format.getSize() ) ),
mNoiseOctaves( format.getNoiseOctaves() ),
mNoiseScale( format.getNoiseScale() ),
mNoiseSeed( format.getNoiseSeed() ),
mRoadBlurIterations( format.getRoadBlurIterations() ),
mBlurIterations( format.getBlurIterations() ),
mSobelBlurIterations( format.getSobelBlurIterations() ),
mNumTilesPerRow( format.getNumTilesPerRow() ),
mNumWorkingThreads( format.getNumWorkingThreads() ),
mFogDensity( 0.129 ),
mFogColor( 0.25, 0.29, 0.47 ),
mSunColor( 1.0f, 0.77, 0.60 ),
mSunScatteringCoeffs( 0.35, 0.18, 0.29 ),
mSunDispertion( 0.3f ),
mSunIntensity( 0.166 ),
mSunDirection( 0, 0.3, -1 ),
mOcclusionCullingEnabled( true ),
mTileExplosionSize( 0.001 ),
mTilePopulationExplosionSize( 0.001f ),
mBuildingTiles( false ),
mPopulatingTiles( false ),
mTilePopulationBalance( -0.05f ),
mTimeline( Timeline::create() ),
mHeightMapCurrent( 0 ),
mHeightMapTemp( 1 ),
mHeightMapProgression( 0.0f )
{
	// load the main shaders
#if !defined( CINDER_ANDROID ) && !defined( CINDER_COCOA_TOUCH )
	wd::watch( "Shaders/Terrain.*", [=]( const fs::path &p ) {
#endif
#ifdef HIGH_QUALITY_ANIMATIONS
		auto shader0 = loadShader( "TerrainHigh" );
#else
		auto shader0 = loadShader( "Terrain" );
#endif
		if( shader0 )
			mTileShader			= shader0;
#if !defined( CINDER_ANDROID ) && !defined( CINDER_COCOA_TOUCH )
	} );
#endif	
	
#if !defined( CINDER_ANDROID ) && !defined( CINDER_COCOA_TOUCH )
	wd::watch( "Shaders/Sky.*", [=]( const fs::path &p ) {
#endif
		auto shader = loadShader( "Sky" );
		if( shader )
		mSkyShader			= shader;
#if !defined( CINDER_ANDROID ) && !defined( CINDER_COCOA_TOUCH )
	} );
#endif
	
#if !defined( CINDER_ANDROID ) && !defined( CINDER_COCOA_TOUCH )
	wd::watch( "Shaders/Trees.*", [=]( const fs::path &p ) {
#endif
#ifdef HIGH_QUALITY_ANIMATIONS
		auto shader1 = loadShader( "TreesHigh" );
#else
		auto shader1 = loadShader( "Trees" );
#endif
		if( shader1 )
			mTileContentShader	= shader1;
#if !defined( CINDER_ANDROID ) && !defined( CINDER_COCOA_TOUCH )
	} );
#endif
	/*
#if !defined( CINDER_ANDROID ) && !defined( CINDER_COCOA_TOUCH )
	wd::watch( "Shaders/ClearingsObjects.*", [=]( const fs::path &p ) {
#endif
		auto shader2 = loadShader( "ClearingsObjects" );
		if( shader2 )
			mClearingObjectsShader	= shader2;
#if !defined( CINDER_ANDROID ) && !defined( CINDER_COCOA_TOUCH )
	} );
#endif
	*/
	
	
	// create the noise lookup table
	Perlin p( 6, mNoiseSeed );
	Surface32f surface( 512, 512, true );
	auto iter = surface.getIter();
	while( iter.line() ){
		while( iter.pixel() ){
			vec2 pos = vec2( iter.getPos().x, iter.getPos().y ) * 0.1f;
			iter.r() = p.fBm( pos ) * 0.5f + 0.5f;
			/*pos += vec3( 1.23f, 4.56f, 7.89f ) + vec3( mNoiseSeed );
			iter.g() = glm::simplex( pos ) * 0.5f + 0.5f;
			pos -= vec3( 7.89f, 4.56f, 1.23f ) + vec3( mNoiseSeed );
			iter.b() = glm::simplex( pos ) * 0.5f + 0.5f;
			pos -= vec3( 17.98f, 24.65f, 31.32f ) + vec3( mNoiseSeed );
			iter.a() = glm::simplex( pos ) * 0.5f + 0.5f;*/
		}
	}
	mNoiseLookupTable = gl::Texture2d::create( Surface( surface ), gl::Texture2d::Format().wrap( GL_REPEAT ).minFilter( GL_LINEAR ).magFilter( GL_LINEAR ) );
	//glCheckError();
	
	// setup the skybox mesh
	auto sphereMesh	= gl::VboMesh::create( geom::Sphere().radius( 7000 ) );
	mSkyBatch		= gl::Batch::create( sphereMesh, mSkyShader );
	//glCheckError();
	
	// add our timeline to the main app timeline
	app::timeline().add( mTimeline );
	
	// load and prepare the base 3d models
	auto models = { "Tree01", "Tree02", "Tree03", "PineTree01", "PineTree02", "PineTree03"/*, "Cube", "Crystal"*/ };
	
#ifdef HIGH_QUALITY_ANIMATIONS
	for( auto model : models ){
		// prepare trimesh
		TriMesh mesh( TriMesh::Format().positions().texCoords0().texCoords1(4) );
		// load the model
		mesh.read( app::loadAsset( "Models/" + string( model ) + ".trimesh" ) );
		// copy the triangle center and triangle id to the texcoords1 slot
		const vector<uint32_t> indices = mesh.getIndices();
		vec3* vertices = mesh.getPositions<3>();
		vector<vec4> extras( mesh.getNumVertices(), vec4(0) );
		for( size_t i = 0; i < indices.size(); i+=3 ){
			float indice		= ( (float) indices[i] / 3.0f ) / (float) mesh.getNumTriangles();
			vec3 center			= ( vertices[indices[i]] + vertices[indices[i+1]] + vertices[indices[i+2]] ) / 3.0f;
			extras[indices[i]]	= vec4( center, indice );
			extras[indices[i+1]]	= vec4( center, indice );
			extras[indices[i+2]]	= vec4( center, indice );
		}
		mesh.appendTexCoords1( &extras[0], extras.size() );
		mPopulationMeshes.push_back( mesh );
	}
#else
	for( auto model : models ){
		// prepare trimesh
		TriMesh mesh( TriMesh::Format().positions().texCoords0().texCoords1(4) );
		// load the model
		mesh.read( app::loadAsset( "Models/" + string( model ) + ".trimesh" ) );
		mPopulationMeshes.push_back( mesh );
	}
#endif
	
	//glCheckError();
	/*auto clearingModels = { "BigTree", "Cubes", "Crystals" };
	for( auto model : clearingModels ){
		// prepare trimesh
		TriMesh mesh( TriMesh::Format().positions().texCoords0().texCoords1(4) );
		// load the model
		mesh.read( app::loadAsset( "Models/" + string( model ) + ".trimesh" ) );
		// copy the triangle center and triangle id to the texcoords1 slot
		const vector<uint32_t> indices = mesh.getIndices();
		vec3* vertices = mesh.getPositions<3>();
		vector<vec4> extras( mesh.getNumVertices(), vec4(0) );
		for( size_t i = 0; i < indices.size(); i+=3 ){
			float indice		= ( (float) indices[i] / 3.0f ) / (float) mesh.getNumTriangles();
			vec3 center			= ( vertices[indices[i]] + vertices[indices[i+1]] + vertices[indices[i+2]] ) / 3.0f;
			extras[indices[i]]	= vec4( center, indice );
			extras[indices[i+1]]	= vec4( center, indice );
			extras[indices[i+2]]	= vec4( center, indice );
		}
		mesh.appendTexCoords1( &extras[0], extras.size() );
		
		mClearingsBatches.push_back( gl::Batch::create( mesh, mClearingObjectsShader ) );
	}*/
	
}

void Terrain::start()
{
	// generate the different maps
	generateHeightMap();
	// launch the triangulisation
	createTiles();
}

Terrain::~Terrain()
{
	// make sure we cancel and join all thread activities
	mUpdateTilesConnection.disconnect();
	
	if( mTilesBuffer )
		mTilesBuffer->cancel();
	
	if( mTilesPopulationBuffer )
		mTilesPopulationBuffer->cancel();
	
	for( const auto &t : mWorkThreads ){
		if( t->joinable() )
			t->join();
	}
}


// MARK: Terrain rendering
//! renders the terrain from the camera point of view
void Terrain::render( const CameraPersp &camera )
{
	
#if !defined( CINDER_ANDROID ) && !defined( CINDER_COCOA_TOUCH )
	if( !timer0 ) {
		timer0 = gl::QueryTimeSwapped::create();
		timer1 = gl::QueryTimeSwapped::create();
		timer2 = gl::QueryTimeSwapped::create();
		timer3 = gl::QueryTimeSwapped::create();
	}
#endif
	//glCheckError();
	
	cpuTimer4.start();
	// set the camera matrices
	gl::setMatrices( camera );
	
	// MARK: Culling
	
	// create a camera slightly larger to make sure our
	// culling algorithms get everything
	CameraPersp frustumCam = camera;
	frustumCam.setNearClip( 0.1f );
	frustumCam.setFov( camera.getFov() + 2 );
	//frustumCam.setEyePoint( frustumCam.getEyePoint() - frustumCam.getViewDirection() * 10.0f );
	
	//glCheckError();
	// use the new camera to create a Frustum
	Frustumf frustum( camera );
	
	vector<Terrain::TileRef> tiles = mTiles;
	
	int wtf = 0;
	// start by removing every tiles that are not in the frustum
	auto frutumRange = std::remove_if( tiles.begin(), tiles.end(), [&frustum,this,&wtf]( const Terrain::TileRef &tile ){
		bool outsideFrustum = !frustum.intersects( tile->getBounds( getElevation() ) );
		if( outsideFrustum ) wtf++;
		return outsideFrustum;
	});
	tiles.erase( frutumRange, tiles.end() );
	
	//CI_LOG_V( "Tiles after frustum culling " << tiles.size() << " " << wtf );
	
	//glCheckError();
	// then tiles that are too far away to be seen
	/*auto frutumFarRange = std::remove_if( tiles.begin(), tiles.end(), [&camera,&wtf]( const Terrain::TileRef &tile ){
		bool farFarAway = camera.worldToEyeDepth( tile->getBounds().getCenter() ) < -800;
		if( farFarAway ) wtf++;
		return farFarAway;
	});
	tiles.erase( frutumFarRange, tiles.end() );
	
	CI_LOG_V( "Tiles after farclip " << tiles.size() << " " << wtf );*/
	
	// and finally, sort the remaining tiles from front to back
	std::sort( tiles.begin(), tiles.end(), [&camera,this]( const Terrain::TileRef &lhs, const Terrain::TileRef &rhs ) {
		return camera.worldToEyeDepth( lhs->getBounds( getElevation() ).getCenter() ) < camera.worldToEyeDepth( rhs->getBounds( getElevation() ).getCenter() );
	} );
	
	cpuTimer4.stop();
	
	//glCheckError();
	// MARK: Update uniforms
	
	// update shaders that needs fog uniform data
	vec3 sun = normalize( vec3( camera.getViewMatrix() * vec4( mSunDirection,0 ) ) );
	auto shadersWithFog = { mTileShader, mTileContentShader, mSkyShader/*, mClearingObjectsShader*/ };
	for( const auto &shader : shadersWithFog ){
		gl::ScopedGlslProg scopedShader( shader );
		shader->uniform( "uInscatteringCoeffs", mSunScatteringCoeffs );
		shader->uniform( "uSunDispertion", mSunDispertion );
		shader->uniform( "uSunIntensity", mSunIntensity );
		shader->uniform( "uFogColor", mFogColor );
		shader->uniform( "uFogDensity", mFogDensity );
		shader->uniform( "uSunColor", mSunColor );
		shader->uniform( "uSunDirection", sun );
	}

#ifdef HIGH_QUALITY_ANIMATIONS
	mTileShader->uniform( "uTime", (float) cinder::app::getElapsedSeconds() );
	mTileShader->uniform( "uTouch", mTileExplosionCenter );
	mTileShader->uniform( "uTouchSize", mTileExplosionSize );
	mTileContentShader->uniform( "uTime", (float) cinder::app::getElapsedSeconds() );
	mTileContentShader->uniform( "uTouch", mTilePopulationExplosionCenter );
	mTileContentShader->uniform( "uTouchSize", mTilePopulationExplosionSize );
#endif
	/*mClearingObjectsShader->uniform( "uTime", (float) cinder::app::getElapsedSeconds() );
	mClearingObjectsShader->uniform( "uTouch", mTileExplosionCenter );
	mClearingObjectsShader->uniform( "uTouchSize", mTileExplosionSize );*/
	
	// enable backface culling, disable blending and enable depth testing
	gl::ScopedFaceCulling cullBackFaces( true, GL_BACK );
	gl::ScopedBlend disableBlending( false );
	gl::ScopedDepth scopedDepth( true );
	
	// MARK: Render terrain tiles
#if !defined( CINDER_ANDROID ) && !defined( CINDER_COCOA_TOUCH )
	timer0->begin();
#endif
	cpuTimer0.start();
	
	// render tiles
	if( renderTerrain && mTileShader && mHeightMap[0] && mFloraDensityMap ){
		gl::ScopedGlslProg scopedShader( mTileShader );
		gl::ScopedModelMatrix scopedModelMatrix;
		gl::ScopedTextureBind scopedTexture0( mHeightMap[0], 0 );
		gl::ScopedTextureBind scopedTexture1( mHeightMap[1] ? mHeightMap[1] : mHeightMap[0], 1 );
		gl::ScopedTextureBind scopedTexture2( mFloraDensityMap, 2 );
#ifdef HIGH_QUALITY_ANIMATIONS
		gl::ScopedTextureBind scopedTexture3( mNoiseLookupTable, 3 );
		mTileShader->uniform( "uNoiseLookupTable", 3 );
#endif
		mTileShader->uniform( "uHeightMap", 0 );
		mTileShader->uniform( "uHeightMapTemp", 1 );
		mTileShader->uniform( "uFlora", 2 );
		mTileShader->uniform( "uElevation", getElevation() );
		mTileShader->uniform( "uHeightMapProgression", mHeightMapProgression );
		
		gl::color( ColorA::black() );
		for( auto tile : tiles ){
			if( !mOcclusionCullingEnabled || !tile->isOccluded() ){
				if( tile->mBatch ){
					
					// update tile animation uniforms
					mTileShader->uniform( "uProgress", tile->mTerrainCompletion );
					//glCheckError();
					
					// as we have created this batch with a dummy shader
					// makes sure we have the right one
					if( tile->mBatch->getGlslProg() != mTileShader ) {
						tile->mBatch->replaceGlslProg( mTileShader );
					}
					//glCheckError();
					
					// render the batch
					tile->mBatch->draw();
					//glCheckError();
				}
				
				////glCheckError();
				
			}
		}
	}
	if( showBounds ) {
		for( auto tile : tiles ){
			gl::drawStrokedCube( tile->getBounds( getElevation(), mHeightMapProgression ) );
		}
	}
	//glCheckError();
	
	cpuTimer0.stop();
	cpuTimer1.start();
#if !defined( CINDER_ANDROID ) && !defined( CINDER_COCOA_TOUCH )
	timer0->end();
	timer1->begin();
#endif
	// MARK: Render trees tiles
	
	// render tile content
	if( renderTrees && mTileContentShader && mTrianglesHeightMap[0] ){
		gl::ScopedGlslProg scopedShader( mTileContentShader );
		gl::ScopedTextureBind scopedTexture0( mTrianglesHeightMap[0], 0 );
		gl::ScopedTextureBind scopedTexture1( mTrianglesHeightMap[1] ? mTrianglesHeightMap[1] : mTrianglesHeightMap[0], 1 );
#ifdef HIGH_QUALITY_ANIMATIONS
		gl::ScopedTextureBind scopedTexture2( mNoiseLookupTable, 2 );
		mTileContentShader->uniform( "uNoiseLookupTable", 2 );
#endif
		
		mTileContentShader->uniform( "uHeightMap", 0 );
		mTileContentShader->uniform( "uHeightMapTemp", 1 );
		mTileContentShader->uniform( "uHeightMapSize", mSize );
		mTileContentShader->uniform( "uHeightMapProgression", mHeightMapProgression );
		mTileContentShader->uniform( "uElevation", getElevation() );
		
		mNumRenderedInstanced = 0;
		gl::color( ColorA::black() );
		
		for( auto tile : tiles ){
			if( !mOcclusionCullingEnabled || !tile->isOccluded() ){
				for( size_t i = 0; i < 2; ++i ){
					if( tile->mPopulation[i] ){
						
						// update tile animation uniforms
						mTileContentShader->uniform( "uProgress", tile->mPopulationCompletion[i] );
						
						// as we have created this batch with a dummy shader
						// we need to make sure we have the right shader
						if( tile->mPopulation[i]->getGlslProg() != mTileContentShader )
							tile->mPopulation[i]->replaceGlslProg( mTileContentShader );
						
						// render the batch
						tile->mPopulation[i]->draw();
						
						mNumRenderedInstanced++;
					}
				}
			}
		}
	}
	//glCheckError();
	
	cpuTimer1.stop();
	cpuTimer2.start();
#if !defined( CINDER_ANDROID ) && !defined( CINDER_COCOA_TOUCH )
	timer1->end();
	timer2->begin();
#endif
	/*
	// MARK: Render Clearing Objects
	if( mClearingObjectsShader && mTrianglesHeightMap[0] ){
		gl::ScopedGlslProg scopedShader( mClearingObjectsShader );
		gl::ScopedTextureBind scopedTexture0( mTrianglesHeightMap[0], 0 );
		gl::ScopedTextureBind scopedTexture1( mTrianglesHeightMap[1] ? mTrianglesHeightMap[1] : mTrianglesHeightMap[0], 1 );
		gl::ScopedTextureBind scopedTexture2( mNoiseLookupTable, 2 );
		
		mClearingObjectsShader->uniform( "uHeightMap", 0 );
		mClearingObjectsShader->uniform( "uHeightMapTemp", 1 );
		mClearingObjectsShader->uniform( "uNoiseLookupTable", 2 );
		mClearingObjectsShader->uniform( "uHeightMapProgression", mHeightMapProgression );
		mClearingObjectsShader->uniform( "uElevation", getElevation() );
		
		gl::color( ColorA::black() );
		for( size_t i = 0; i < mClearings.size(); ++i ){
			if( mClearingsBatches[i] ){
				// update tile animation uniforms
				mClearingObjectsShader->uniform( "uProgress", 1.0f );
				mClearingObjectsShader->uniform( "uPosition", mClearings[i] );
				mClearingObjectsShader->uniform( "uId", (float) i );
			
				// as we have created this batch with a dummy shader
				// we need to make sure we have the right shader
				if( mClearingsBatches[i]->getGlslProg() != mClearingObjectsShader )
					mClearingsBatches[i]->replaceGlslProg( mClearingObjectsShader );
			
				// render the batch
				mClearingsBatches[i]->draw();
			}
		}
	}*/
	
	// MARK: Render Skybox
	
	// render skybox
	if( renderSky && mSkyShader ) {
		// we are inside the sphere so enable frontface culling instead
		gl::ScopedFaceCulling cullFrontFaces( true, GL_FRONT );
		gl::color( ColorA::black() );
		
		if( mSkyBatch->getGlslProg() != mSkyShader )
			mSkyBatch->replaceGlslProg( mSkyShader );
		
		gl::ScopedTextureBind scopedTexture2( mNoiseLookupTable, 0 );
		mSkyShader->uniform( "uNoiseLookupTable", 0 );
		mSkyShader->uniform( "uTime", (float) cinder::app::getElapsedSeconds() );
		mSkyBatch->draw();
	}
	//glCheckError();
	
	cpuTimer2.stop();
	cpuTimer3.start();
#if !defined( CINDER_ANDROID ) && !defined( CINDER_COCOA_TOUCH )
	timer2->end();
	timer3->begin();
#endif
	
	// MARK: Occlusion culling
	if( mOcclusionCullingEnabled ){
		
		// we don't actually need to render anything here, we just need
		// to do if any fragment pass the different tests, so disable everything.
		gl::ScopedFaceCulling disableFaceCulling( false );
		//gl::ScopedFaceCulling cullFrontFaces( true, GL_FRONT );
		gl::colorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE );
		gl::depthMask( GL_FALSE );
		for( auto tile : tiles ){
			gl::ScopedModelMatrix modelMatrix;
			
			auto bounds			= tile->getBounds();
			auto scaledBounds	= tile->getBounds( getElevation() );
			auto center			= vec3( bounds.getCenter().x, scaledBounds.getCenter().y, bounds.getCenter().z );
			auto scale			= vec3( 1.0f, scaledBounds.getSize().y, 1.0f );
			
			gl::translate( center + tile->mPosition );
			gl::scale( scale );
			
			tile->checkOcclusion();
		}
		gl::colorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
		gl::depthMask( GL_TRUE );
		
		//glFlush();
		
	
		// query the occlusion test results
		for( auto tile : tiles ){
			tile->queryOcclusionResults();
			// makes sure the tile we are currently in doesn't get culled
			vec3 offset( tile->getArea().getUL().x, 0, tile->getArea().getUL().y );
			if( glm::all( glm::greaterThanEqual( camera.getEyePoint(), tile->getBounds( getElevation() ).getMin() ) ) && glm::all( glm::lessThan( camera.getEyePoint(), tile->getBounds( getElevation() ).getMax() ) ) ){
				tile->resetOccludedFrameCount();
			}
		}
	}
	//glCheckError();
	cpuTimer3.stop();
#if !defined( CINDER_ANDROID ) && !defined( CINDER_COCOA_TOUCH )
	timer3->end();
#endif
	
	gl::disableDepthRead();
	gl::disableDepthWrite();
}

void Terrain::generateTilesPopulation()
{
	// reset touchSize
	//mTimeline->applyPtr( &mTilePopulationExplosionSize, 0.001f, 2.0f, EaseInOutQuad() );
	// start the tile populating process
	populateTiles();
}
void Terrain::removeTilesPopulation()
{
	//mTimeline->applyPtr( &mTilePopulationExplosionSize, (float) ( mSize.x + mSize.y ), 25.0f, EaseInOutQuad() );
	// clear all the meshes
	for( auto tile : mTiles ){
		tile->clearPopulation();
		
		// start animation
		size_t currentBatch = tile->mPopulationCurrent;
#ifdef HIGH_QUALITY_ANIMATIONS
		mTimeline->applyPtr( &tile->mPopulationCompletion[tile->mPopulationCurrent], 0.0f, 8.0f, EaseOutQuad() )
#else
		mTimeline->applyPtr( &tile->mPopulationCompletion[tile->mPopulationCurrent], 0.0f, 3.5f, EaseOutQuad() )
#endif
		.finishFn( [currentBatch,tile](){
			tile->mPopulation[currentBatch].reset();
		} );
	}
	
	// reset mTileExplosionSize
	//mTimeline->applyPtr( &mTileExplosionSize, 0.001f, 2.0f, EaseInOutQuad() );
}


void Terrain::regenerateTilesPopulation()
{
	populateTiles();
	/*mTimeline->applyPtr( &mTilePopulationExplosionSize, (float) ( mSize.x * 0.5f ), 25.0f, EaseInOutQuad() );
	for( size_t i = 0; i < mTiles.size(); ++i ) {
		if( i < mTiles.size() - 1 ){
			mTimeline->applyPtr( &mTiles[i]->mPopulationCompletion[mTiles[i]->mPopulationCurrent], 0.0f, 10.0f, EaseInOutQuad() );
		}
		else {
			mTimeline->applyPtr( &mTiles[i]->mPopulationCompletion[mTiles[i]->mPopulationCurrent], 0.0f, 10.0f, EaseInOutQuad() )
			.finishFn( [this](){
				// reset mTileExplosionSize
				mTimeline->applyPtr( &mTilePopulationExplosionSize, 0.001f, 2.0f, EaseInOutQuad() );
				// start the tile populating process
				populateTiles();
			} );
		}
	}*/
}

// MARK: Tile

std::shared_ptr<Terrain::Tile> Terrain::Tile::create( size_t tileId, const Area &tileArea, const Area &fullArea, float contentScale, float randomSeed, const Channel32fRef &heightMap, const Channel32fRef &densityMap, const BSpline2f &spline, size_t tilesPerRow, float elevation )
{
	return make_shared<Terrain::Tile>( tileId, tileArea, fullArea, contentScale, randomSeed, heightMap, densityMap, spline, tilesPerRow, elevation );
}

Terrain::Tile::Tile( size_t tileId, const Area &tileArea, const Area &fullArea, float contentScale, float randomSeed, const Channel32fRef &heightMap, const Channel32fRef &densityMap, const BSpline2f &spline, size_t tilesPerRow, float elevation ) :
mTileId( tileId ),
mArea( tileArea ),
mSize( tileArea.getSize() ),
#ifdef HIGH_QUALITY_ANIMATIONS
mTriMesh( TriMesh::Format().positions(3).texCoords().texCoords1(4) ),
#else
mTriMesh( TriMesh( TriMesh::Format().positions(3).texCoords() ) ),
#endif
mNumFramesOccluded( 0 ),
mPosition( 0.0f ),
mPopulationCurrent( 0 ),
mPopulationTemp( 1 )
{
	// prepare data
	// add some margins so our poisson disk distribution doesn't get wrong at the edges
	vec2 margins				= vec2( 20.0f );
	Rectf areaWithMargins		= Rectf( mArea.getUL() - ivec2( margins ), mArea.getLR() + ivec2( margins ) );
	Rectf localArea				= Rectf( vec2(0), tileArea.getSize() );
	
	float minDist				= 0.75f;
	float maxDist				= 45.0f;
	
	std::vector<ci::vec2> meshSamples;
	std::vector<ci::vec3> meshVertices;
	std::vector<uint32_t> meshIndices;
	
	// make sure the area with margins is within the main heightmap bounds
	areaWithMargins.clipBy( fullArea );
	
	// start growing the mesh distribution from the road
	// by using its spline to find the poisson disk initial points
	vector<vec2> initialPoints;
	for( float i = 0; i < 1.0f; i+= 0.0005f ){
		vec2 p = spline.getPosition( i );
		//p.y = fullArea.getHeight() - p.y;
		p -= vec2( mArea.getUL() );
		if( localArea.contains( p ) ){
			initialPoints.push_back( p );
		}
	}
	
	// sample sobel map and create the mesh distribution using poisson disk sampling
	meshSamples = poissonDiskDistribution( [&]( const vec2& p ){
		float s = densityMap->getValue( p + vec2( mArea.getUL() ) );
		return glm::clamp( maxDist - s * maxDist, minDist, maxDist );
	}, localArea, initialPoints, 80 );
	
	// removes samples that are not in this tile
	auto outOfBoundRange = std::remove_if( meshSamples.begin(), meshSamples.end(), [localArea]( const vec2& p ){
		return !localArea.contains( p );
	});
	meshSamples.erase( outOfBoundRange, meshSamples.end() );
	
	// add points on the tile borders
	float numVerticesPerEdge = 75.0f;
	for( float i = 0.0f; i <= numVerticesPerEdge; ++i ){
		float j = i / numVerticesPerEdge;
		// top border
		meshSamples.push_back( vec2( glm::mix( 0.0f, mSize.x, j ), 0 ) );
		// left border
		meshSamples.push_back( vec2( 0, glm::mix( 0.0f, mSize.y, j ) ) );
		// right border
		meshSamples.push_back( vec2( mSize.x, glm::mix( 0.0f, mSize.y, j ) ) );
		// bottom border
		meshSamples.push_back( vec2( glm::mix( 0.0f, mSize.x, j ), mSize.y ) );
	}
	meshSamples.push_back( mSize );
	
	// triangulate samples
	meshIndices = Delaunay::getTriangleIndices( Rectf( vec2(0), mSize + vec2(1) ), meshSamples );
	
	// normalize and convert samples to 3d coordinates
	float minHeight = 10000.0f, maxHeight = -10000.0f;
	for( auto sample : meshSamples ){
		vec2 ceilSample = glm::floor( sample + vec2( mArea.getUL() ) );// - vec2( 0.5 );
		ceilSample = glm::clamp( ceilSample, vec2( fullArea.getUL() ), vec2( fullArea.getLR() ) );
		float height	= heightMap->getValue( ceilSample );
		if( height > maxHeight ) maxHeight = height;
		if( height < minHeight ) minHeight = height;
		
		vec3 pos = vec3( sample.x, 0, sample.y );
		meshVertices.push_back( pos );
	}
	
	// calculate bounding box
	vec3 min( tileArea.getUL().x, 0, tileArea.getUL().y );
	vec3 max( tileArea.getLR().x, 0, tileArea.getLR().y );
	mBounds[0]		= AxisAlignedBox( min, max );
	mHeightRange[0] = vec2( minHeight - 0.15f, maxHeight + 0.2f );
	
	// generate triangle mesh
	
	// create meshes
	vector<vec2> texcoords;
	vec2 scl = 1.0f / glm::ceil( vec2( mSize ) * vec2( tilesPerRow ) );
	for( auto &p : meshVertices ){
		auto uv = vec2( p.x, p.z ) * scl ;
		auto offset = vec2( getArea().getUL() ) * scl;
		
		//mTriMesh.appendTexCoord( uv + offset );
		texcoords.push_back( uv + offset );
		
		p += vec3( tileArea.getUL().x, 0.0f, tileArea.getUL().y );
	}
	
	
#ifdef HIGH_QUALITY_ANIMATIONS
	// we have data per triangles so we need to rebuild the whole mesh
	// we do lose the dense packing of the triangulation with triangles
	// sharing vertices but I don't think there's so much choises here
	mTriMesh = TriMesh( TriMesh::Format().positions(3).texCoords().texCoords1(4) );
	for( size_t i = 0; i < meshIndices.size(); i+=3 ){
		float indice				= (float) i / (float) meshIndices.size();
		vec3 center					= ( meshVertices[meshIndices[i]] + meshVertices[meshIndices[i+1]] + meshVertices[meshIndices[i+2]] ) / 3.0f;
		mTriMesh.appendTexCoord1( vec4( center, indice ) );
		mTriMesh.appendTexCoord1( vec4( center, indice ) );
		mTriMesh.appendTexCoord1( vec4( center, indice ) );
		mTriMesh.appendTexCoord0( texcoords[ meshIndices[i] ] );
		mTriMesh.appendTexCoord0( texcoords[ meshIndices[i+1] ] );
		mTriMesh.appendTexCoord0( texcoords[ meshIndices[i+2] ] );
		mTriMesh.appendVertex( meshVertices[meshIndices[i]] );
		mTriMesh.appendVertex( meshVertices[meshIndices[i+1]] );
		mTriMesh.appendVertex( meshVertices[meshIndices[i+2]] );
	}
#else
	mTriMesh = TriMesh( TriMesh::Format().positions(3).texCoords() );
	mTriMesh.appendPositions( &meshVertices[0], meshVertices.size() );
	mTriMesh.appendIndices( &meshIndices[0], meshIndices.size() );
	mTriMesh.appendTexCoords0( &texcoords[0], texcoords.size() );
#endif
	
	//mTriMesh.recalculateNormals();
	
	//cout << meshIndices.size() / 3 << " triangles(" << meshSamples.size() << " vertices)" << endl;
	//CI_LOG_V( vec2( mArea.getUL() ) << " " << mSize );
	//CI_LOG_V( mTileId << " done(geom: "
	//		 << (int) ( geomTimer.getSeconds() * 1000.0 ) << "ms)." );
}

Terrain::Tile::~Tile()
{
	// delete occlusion query
	for( auto query : mOcclusionQueries ){
		glDeleteQueries( 1, &query.mId );
	}
}

ci::AxisAlignedBox Terrain::Tile::getBounds( float elevation, float interpolation ) const
{
	vec3 min = glm::mix( mBounds[1].getMin(), mBounds[0].getMin(), interpolation );
	vec3 max = glm::mix( mBounds[1].getMax(), mBounds[0].getMax(), interpolation );
	min.y = min.y + elevation * glm::mix( mHeightRange[1].x, mHeightRange[0].x, interpolation );
	max.y = max.y + elevation * glm::mix( mHeightRange[1].y, mHeightRange[0].y, interpolation );
	return AxisAlignedBox( min, max );
}

// MARK: Tile Meshes

void Terrain::Tile::buildMeshes( const ci::gl::GlslProgRef &shader )
{
	// create the terrain mesh
	// geom::Cube(), gl::getStockShader( gl::ShaderDef().color().texture() )
	mBatch = gl::Batch::create( mTriMesh, shader );//, { { geom::Attrib::POSITION, "ciPosition" }, { geom::Attrib::TEX_COORD_0, "ciTexCoord0" } } );
	
	// and the occluder mesh
	buildOcclusionMesh();
	
	// free the mesh data
	//mTriMesh.clear();
	
	// create a few occlusion queries
	for( size_t i = 0; i < 5; i++ ){
		OcclusionQuery query;
		glGenQueries( 1, &query.mId );
		mOcclusionQueries.push_back( query );
	}
}

void Terrain::Tile::buildOcclusionMesh()
{
	// create the occluder box mesh
	auto stockShader = gl::getStockShader( gl::ShaderDef().color() );
	mat4 occluderTransform = glm::scale( mat4(1), vec3( getBounds().getSize().x, 1.0f, getBounds().getSize().z ) );
	mOccluderBatch = gl::Batch::create( geom::Cube() >> geom::Transform( occluderTransform ), stockShader, { { geom::Attrib::POSITION, "ciPosition" } } );
}
void Terrain::Tile::buildPopulationMeshes( const TriMeshRef &triMesh, const ci::AxisAlignedBox &bounds, const ci::gl::GlslProgRef &shader )
{
	// swap the population batch flags
	swap( mPopulationCurrent, mPopulationTemp );
	
	if( triMesh->getNumTriangles() > 0 ){
		
		// create the main population batch
		mPopulation[mPopulationCurrent] = gl::Batch::create( *triMesh.get(), shader );
		
		// update the old bounds
		mBounds[0].include( bounds );
		
		// update the old occluder mesh
		buildOcclusionMesh();
		
		// make sure we clear the temp TriMesh
		triMesh->clear();
	}
}
void Terrain::Tile::updateBounds( const std::vector<ci::vec2> &samples, const ci::Channel32fRef &heightMap, const ci::Area &fullArea )
{
	// normalize and convert samples to 3d coordinates
	float minHeight = 10000.0f, maxHeight = -10000.0f;
	for( auto sample : samples ){
		vec2 ceilSample = glm::floor( sample + vec2( mArea.getUL() ) );// - vec2( 0.5 );
		ceilSample = glm::clamp( ceilSample, vec2( fullArea.getUL() ), vec2( fullArea.getLR() ) );
		float height	= heightMap->getValue( ceilSample );
		if( height > maxHeight ) maxHeight = height;
		if( height < minHeight ) minHeight = height;
	}
	
	// calculate bounding box
	vec3 min( mArea.getUL().x, 0, mArea.getUL().y );
	vec3 max( mArea.getLR().x, 0, mArea.getLR().y );
	
	mBounds[0]			= AxisAlignedBox( min, max );
	mHeightRange[0]	= vec2( minHeight - 0.15f, maxHeight + 0.2f );
}
void Terrain::Tile::swapBounds()
{
	swap( mBounds[0], mBounds[1] );
	swap( mHeightRange[0], mHeightRange[1] );
}

void Terrain::Tile::clear()
{
	//clearPopulation();
}

void Terrain::Tile::clearPopulation()
{
	//mTrees.reset();
}

// MARK: Tile Occlusion Culling

bool Terrain::Tile::isOccluded( size_t numFrames )
{
	return mNumFramesOccluded >= numFrames;
}
void Terrain::Tile::resetOccludedFrameCount()
{
	mNumFramesOccluded = 0;
}
void Terrain::Tile::checkOcclusion()
{
	// no need to do anything if the occlusion mesh is not ready
	if( mOccluderBatch ){
		// check if we have query object ready to be used
		int current = -1;
		for( size_t i = 0; i < mOcclusionQueries.size(); ++i ){
			if( !mOcclusionQueries[i].mUsed ){
				current = i;
				break;
			}
		}
		
		// if we're still waiting for the result of all queries
		// no need to add more, we'll just wait.
		if( current != -1 ){
			glBeginQuery( GL_ANY_SAMPLES_PASSED_CONSERVATIVE, mOcclusionQueries[current].mId );
			mOccluderBatch->draw();
			glEndQuery( GL_ANY_SAMPLES_PASSED_CONSERVATIVE );
			mOcclusionQueries[current].mUsed = true;
		}
	}
}
void Terrain::Tile::queryOcclusionResults()
{
	// check all queries to see if we have any result ready
	for( auto& query : mOcclusionQueries ){
		// check query validity
		if( glIsQuery( query.mId ) ){
			// check if the result is available
			GLuint ready = 0;
			glGetQueryObjectuiv( query.mId, GL_QUERY_RESULT_AVAILABLE, &ready );
			if( ready != 0 ){
				// if it is check the result
				GLuint nonOccluded;
				glGetQueryObjectuiv( query.mId, GL_QUERY_RESULT, &nonOccluded );
				query.mUsed = false;
				
				if( nonOccluded == 0 ){
					mNumFramesOccluded++;
				}
				else mNumFramesOccluded = 0;
				
				return;
			}
		}
	}
}

// MARK: Tiles Distribution / Triangulation Threads

void Terrain::createTiles()
{
	// skip if we're already creating new tiles
	if( mBuildingTiles || mPopulatingTiles )
		return;
	
	// clear previous data
	// join the threads and clear the whole thread vector
	mUpdateTilesConnection.disconnect();
	
	for( const auto &t : mWorkThreads ){
		if( t->joinable() )
			t->join();
	}
	mWorkThreads.clear();
	mTiles.clear();
	
	if( mTilesBuffer )
		mTilesBuffer->cancel();
	
	if( mTilesPopulationBuffer )
		mTilesPopulationBuffer->cancel();
	
	// setup tiles triangulation threads
	// prepare tiles data
	int numTiles			= getNumTilesPerRow() * getNumTilesPerRow();
	vec2 size				= vec2( mSize );
	vec2 tileSize			= vec2( ( size / (float) getNumTilesPerRow() ) );
	int numTilesPerThread	= ceil( (float) numTiles / (float) getNumWorkingThreads() );
	Area area				= Area( ivec2(0), ivec2(size) );
	float scale				= 1.0f / size.x * 512.0f;
	mTilesBuffer			= CircularTileBufferRef( new CircularTileBuffer( numTiles ) );
	mBuildingTiles			= true;
	
	
	// download the map to the cpu
	auto heightMap = getHeightChannel();
	auto densityMap = getMeshDensityChannel();
	
	// setup worker threads to build the different tiles
	for( int i = 0; i < getNumWorkingThreads(); i++ ){
		int start = i * numTilesPerThread;
		int end = min( ( i + 1 ) * numTilesPerThread, numTiles ) - 1;
		mWorkThreads.emplace_back( new thread( bind( &Terrain::buildTilesThreaded, this, start, end, getNumTilesPerRow(), tileSize, area, scale, heightMap, densityMap ) ) );
	}
	
	// start watching for tile updates
	mUpdateTilesConnection = app::App::get()->getSignalUpdate().connect( bind( &Terrain::updateTiles, this ) );
}

float Terrain::getTilesThreadsCompletion()
{
	return static_cast<float>( mTiles.size() ) / static_cast<float>( getNumTilesPerRow() * getNumTilesPerRow() );
}

void Terrain::updateTiles()
{
	while( mTilesBuffer->isNotEmpty() ) {
		// pop back new tile
		TileRef tile;
		mTilesBuffer->popBack( &tile );
		
		// look for old tile and remove it if found
		size_t tileId = tile->getTileId();
		auto oldTile = std::find_if( mTiles.begin(), mTiles.end(), [tileId]( const TileRef &t ) {
			return t->getTileId() == tileId;
		} );
		if( oldTile != mTiles.end() ){
			mTiles.erase( oldTile );
		}
		
		// push back new tile and build opengl objects
		mTiles.push_back( tile );
		tile->buildMeshes( mTileShader );
		
		// and start animation
#ifdef HIGH_QUALITY_ANIMATIONS
		mTimeline->applyPtr( &tile->mTerrainCompletion, 0.0f, 1.0f, 10.0f, EaseOutQuad() );
#else
		mTimeline->applyPtr( &tile->mTerrainCompletion, 0.0f, 1.0f, 3.5f, EaseOutQuad() );
#endif
	}
	
	// when all the threads have returned
	if( mTiles.size() >= getNumTilesPerRow() * getNumTilesPerRow() ){
		// disconnect this signal
		mUpdateTilesConnection.disconnect();
		
		// join the threads and clear the whole thread vector
		for( const auto &t : mWorkThreads ){
			t->join();
		}
		mWorkThreads.clear();
		
		// generate the triangle height map from the actual displaced triangles
		generateTriangleHeightMap();
		
		// now that we have the right elevation fix the 3d spline control points
		auto heightChannel = getTrianglesHeightChannel();
		for( size_t i = 0; i < mRoadSpline3d[mHeightMapCurrent].getNumControlPoints(); ++i ){
			vec3 controlPoint = mRoadSpline3d[mHeightMapCurrent].getControlPoint( i );
			controlPoint.y = heightChannel->getValue( ivec2( controlPoint.x, controlPoint.z ) );
			mRoadSpline3d[mHeightMapCurrent].setControlPoint( i, controlPoint );
		}
		
		mRoadSpline3dLength = mRoadSpline3d[mHeightMapCurrent].getLength( 0.0f, 1.0f );
		
		// flag the tile process as complete
		mBuildingTiles = false;
		
		/*// and resample the spline using the spline arclength
		float splineTotalLength = mRoadSpline3d.getLength( 0.0f, 1.0f );
		vector<vec3> splineSamples;
		for( float i = 0; i <= 1.0f; i+= 0.005f ){
			splineSamples.push_back( mRoadSpline3d.getPosition( mRoadSpline3d.getTime( i * splineTotalLength ) ) );
		}
		mRoadSpline3d	= BSpline3f( splineSamples, 3, true, false );*/
		
		// start the tile populating process
		populateTiles();
	}
}

void Terrain::buildTilesThreaded( size_t start, size_t end, size_t numTilesPerRow, const vec2 &tileSize, const Area &area, float scale, const Channel32fRef &heightMap, const Channel32fRef &densityMap )
{
	ThreadSetup threadSetup;
	
	for( size_t i = start; i <= end; i++ ){
		vec2 pos		= vec2( i % numTilesPerRow, i / numTilesPerRow );
		vec2 ul			= pos * ceil( tileSize );
		vec2 lr			= pos * ceil( tileSize ) + ceil( tileSize );
		Area tileArea	= Area( ul, lr );
		tileArea.clipBy( area );
		
		auto tile = Tile::create( i, tileArea, Area( ivec2(0), mSize ), scale, mNoiseSeed, heightMap, densityMap, getRoadSpline2d(), numTilesPerRow, getElevation() );
		
		// add a small delay to make sure all threads don't return at the same time
		this_thread::sleep_for( chrono::milliseconds( 2 * ( start + 20 ) ) );
		
		mTilesBuffer->pushFront( tile );
	}
};


void Terrain::populateTiles()
{
	// skip if we're already populating
	if( mPopulatingTiles )
		return;
	
	// remove the old population
	for( auto tile : mTiles ){
		tile->clearPopulation();
		
		// start animation
		size_t currentBatch = tile->mPopulationCurrent;
#ifdef HIGH_QUALITY_ANIMATIONS
		mTimeline->applyPtr( &tile->mPopulationCompletion[tile->mPopulationCurrent], 0.0f, 8.0f, EaseOutQuad() )
#else
		mTimeline->applyPtr( &tile->mPopulationCompletion[tile->mPopulationCurrent], 0.0f, 3.5f, EaseOutQuad() )
#endif
		.finishFn( [currentBatch,tile](){
			tile->mPopulation[currentBatch].reset();
		} );
	}
	
	// setup tiles triangulation threads
	// prepare tiles data
	int numTiles			= getNumTilesPerRow() * getNumTilesPerRow();
	vec2 size				= vec2( mSize );
	vec2 tileSize			= vec2( ( size / (float) getNumTilesPerRow() ) );
	Area area				= Area( ivec2(0), ivec2(size) );
	int numTilesPerThread	= ceil( (float) numTiles / (float) getNumWorkingThreads() );
	mTilesPopulationBuffer	= CircularPopulationBufferRef( new CircularPopulationBuffer( numTiles ) );
	mNumTilePopulated		= 0;
	mTileExplosionSize		= 0.001f;
	mPopulatingTiles		= true;
	
	// download the maps to the cpu
	auto flora	= getFloraDensityChannel();
	auto height = getTrianglesHeightChannel();
	
	// setup worker threads to build the different tiles
	for( int i = 0; i < getNumWorkingThreads(); i++ ){
		int start = i * numTilesPerThread;
		int end = min( ( i + 1 ) * numTilesPerThread, numTiles ) - 1;
		mWorkThreads.emplace_back( new thread( bind( &Terrain::populateTilesThreaded, this, start, end, getNumTilesPerRow(), tileSize, area, flora ) ) );
	}
	
	// start watching for tile updates
	mUpdateTilesConnection = app::App::get()->getSignalUpdate().connect( bind( &Terrain::updateTilePopulating, this ) );
	
}
void Terrain::updateTilePopulating()
{
	while( mTilesPopulationBuffer->isNotEmpty() ){
		PopulationDataRef data;
		mTilesPopulationBuffer->popBack( &data );
		
		size_t tileId = data->mTileId;
		
		// find the corresponding tile and updates its population
		auto tileLookup = std::find_if( mTiles.begin(), mTiles.end(), [tileId] ( const TileRef& tile ) { return tile->getTileId() == tileId; } );
		if( tileLookup != mTiles.end() ){

			// build opengl meshes
			(*tileLookup)->buildPopulationMeshes( data->mTriMesh, data->mBounds, mTileContentShader );
			
			// start animation
#ifdef HIGH_QUALITY_ANIMATIONS
			mTimeline->applyPtr( &(*tileLookup)->mPopulationCompletion[(*tileLookup)->mPopulationCurrent], 0.0f, 1.0f, 8.0f, EaseOutQuad() );
#else
			mTimeline->applyPtr( &(*tileLookup)->mPopulationCompletion[(*tileLookup)->mPopulationCurrent], 0.0f, 1.0f, 3.5f, EaseOutQuad() );
#endif
		}
		
		mNumTilePopulated++;
	}
	
	if( mNumTilePopulated >= getNumTilesPerRow() * getNumTilesPerRow() ){
		
		// disconnect this signal
		mUpdateTilesConnection.disconnect();
		
		// join the threads and clear the whole thread vector
		for( const auto &t : mWorkThreads ){
			t->join();
		}
		mWorkThreads.clear();
		
		// flag the tile populating process as complete
		mPopulatingTiles = false;
	}
}

void Terrain::populateTilesThreaded( size_t start, size_t end, size_t numTilesPerRow, const ci::vec2 &tileSize, const ci::Area &area, const ci::Channel32fRef &floraMap )
{
	ThreadSetup threadSetup;
	
	for( size_t i = start; i <= end; i++ ){
		vec2 pos		= vec2( i % numTilesPerRow, i / numTilesPerRow );
		vec2 ul			= pos * ceil( tileSize );
		vec2 lr			= pos * ceil( tileSize ) + ceil( tileSize );
		Area tileArea	= Area( ul, lr );
		tileArea.clipBy( area );
		Rectf localArea = Rectf( vec2(0), tileArea.getSize() );
		
		// create a new population data for this tile
		PopulationDataRef data = PopulationDataRef( new PopulationData() );
		data->mTileId = i;
		
		// find a good initial point for the samples
		int limit = 0;
		vector<vec2> initialSamples;
		for( int i = 0; i < 5; i++ ){
			vec2 initialSample = vec2( randFloat( localArea.getX1(), localArea.getX2() ), randFloat( localArea.getY1(), localArea.getY2() ) );
			while ( floraMap->getValue( initialSample + vec2( tileArea.getUL() ) ) > 0.3f && limit < 100 ) {
				initialSample = vec2( randFloat( localArea.getX1(), localArea.getX2() ), randFloat( localArea.getY1(), localArea.getY2() ) );
				limit++;
			}
			initialSamples.push_back( initialSample );
		}
	
		// use the flora map to generate poisson disk samples
		vector<vec2> samples = poissonDiskDistribution( [&]( const vec2& p ){
			float s = floraMap->getValue( p + vec2( tileArea.getUL() ) );
			//if( s > 0.95 ) s *= 30.0f;
			//return 1.0f + s * 10.0f;
			return 2.5f + s * 10.0f;
		}, [&]( const vec2& p ){
			vec2 sample = p + vec2( tileArea.getUL() );
			float s = floraMap->getValue( sample );
			/*
			bool insideClearing = false;
			for( auto clearing : mClearings ){
				vec2 clearing2d( clearing.x, clearing.z );
				if( length( sample - clearing2d ) < 50.0f ){
					insideClearing = true;
					break;
				}
			}
			*/
			return s < 0.5f;// && !insideClearing;
		}, localArea, initialSamples );
		
		// removes samples that are not in this tile
		auto outOfBoundRange = std::remove_if( samples.begin(), samples.end(), [localArea]( const vec2& p ){
			return !localArea.contains( p );
		});
		samples.erase( outOfBoundRange, samples.end() );
		
		// convert 2d samples to 3d points. skip initial sample that might be wrong
		vector<vec3> positions;
		for( size_t i = 1; i < samples.size(); ++i ) {
			 vec2 mapPos = samples[i] + vec2( tileArea.getUL() );
			 if( floraMap->getValue( mapPos ) < 0.5f ) {
				positions.push_back( vec3( samples[i].x, 0.0f, samples[i].y ) );
			 }
		 }
		
		Rand rnd;
		rnd.seed( mNoiseSeed );
		Perlin perlin( 3, mNoiseSeed );
		vec3 offset( tileArea.getUL().x, 0, tileArea.getUL().y );
		
		// make the trees global scale random
		float scale0 = rnd.nextFloat( 0.9f, 1.25f );
		float scale1 = rnd.nextFloat( 0.7f, 0.8f );
		
		// sometimes make them really big
		if( rnd.nextFloat( 0.0f, 100.0f ) < 7.0f ){
			scale0 = rnd.nextFloat( 2.8f, 3.3f );
		}
	
		
		// prepare min&max to calculate the new bounds
		vec3 min = vec3( 10000000.0f ), max = vec3( -10000000.0f );
		
		// Combine all the instances into one model
		// opengl instancing is not always a win, in this case it's not because
		// of the high amount of data and instructions per vertex
		
#ifdef HIGH_QUALITY_ANIMATIONS
		data->mTriMesh = TriMesh::create( TriMesh::Format().positions().texCoords0(2).texCoords1(4) );
#else
		data->mTriMesh = TriMesh::create( TriMesh::Format().positions().texCoords0(3) );
#endif
		int j = 0;
		for( auto p : positions ){
			// extra density to influence the scale of objects
			vec2 mapPos			= vec2( p.x, p.z ) + vec2( tileArea.getUL() );
			float floraDensity	= 1.0 - glm::clamp( floraMap->getValue( mapPos ), 0.0f, 1.0f );
			floraDensity		= glm::clamp( floraDensity + 0.5f, 0.0f, 1.0f );
			
			// get translation
#ifdef HIGH_QUALITY_ANIMATIONS
			mat4 translation	= glm::translate( mat4(1.0f), p + offset );
#else
			mat4 translation	= glm::translate( mat4(1.0f), vec3(0) );
#endif
			
			float k = perlin.fBm( ( vec3( mNoiseSeed ) + p + offset ) * 0.001f );
			if( k > mTilePopulationBalance ){
				size_t k			= rnd.nextInt( 0, 2 );
				mat4 rotation		= glm::toMat4( normalize( quat( glm::eulerAngleYXZ( rnd.nextFloat(0.1,4.0), rnd.nextFloat(0.01,0.1), rnd.nextFloat(0.01,0.1) ) ) ) );
				mat4 scale			= glm::scale( mat4(1.0f), floraDensity * scale0 * vec3( rnd.randFloat( 5, 20 ) ) );//  15, 60 ) ) );
				mat4 transform		= translation * rotation * scale;
				
				if( rnd.nextFloat( 0.0f, 100.0f ) < 0.5f ){
					k		= 2;
					scale	= glm::scale( mat4(1.0f), vec3(0.5f) );
				}
				
				// transform vertices
				vec3* vertices = mPopulationMeshes[k].getPositions<3>();
#ifdef HIGH_QUALITY_ANIMATIONS
				vec4* centers = mPopulationMeshes[k].getTexCoords1<4>();
#endif
				vector<vec3> texcoords;
				vector<vec3> transformedVertices;
				vector<vec4> transformedTrianglesCenters;
				size_t indiceOffset = data->mTriMesh->getNumVertices();
				for( size_t i = 0; i < mPopulationMeshes[k].getNumVertices(); ++i ){
					vec2 uv = ( vec2( p.x, p.z ) + vec2( offset.x, offset.z ) ) / vec2( mSize );
					texcoords.push_back( vec3( uv.x, uv.y, (float) j / (float) positions.size() ) );
					vec3 pos = vec3( transform * vec4( vertices[i], 1.0f ) );
					transformedVertices.push_back( pos );
					if( p.x + pos.x < min.x ) min.x = p.x + pos.x;
					if( p.y + pos.y < min.y ) min.y = p.y + pos.y;
					if( p.z + pos.z < min.z ) min.z = p.z + pos.z;
					if( p.x + pos.x > max.x ) max.x = p.x + pos.x;
					if( p.y + pos.y > max.y ) max.y = p.y + pos.y;
					if( p.z + pos.z > max.z ) max.z = p.z + pos.z;
					
#ifdef HIGH_QUALITY_ANIMATIONS
					transformedTrianglesCenters.push_back( vec4( vec3( transform * vec4( vec3( centers[i] ), 1.0f ) ), ( centers[i].w + indiceOffset / (float) mPopulationMeshes[k].getNumVertices() ) / (float) positions.size() ) );
#endif
				}
				// offset indices
				const vector<uint32_t> indices = mPopulationMeshes[k].getIndices();
				vector<uint32_t> transformedIndices;
				for( size_t i = 0; i < mPopulationMeshes[k].getNumIndices(); ++i ){
					transformedIndices.push_back( indiceOffset + indices[i] );
				}
				// combine mesh with the main one
				data->mTriMesh->appendIndices( &transformedIndices[0], transformedIndices.size() );
				data->mTriMesh->appendPositions( &transformedVertices[0], transformedVertices.size() );
				data->mTriMesh->appendTexCoords0( &texcoords[0], texcoords.size() );
#ifdef HIGH_QUALITY_ANIMATIONS
				data->mTriMesh->appendTexCoords1( &transformedTrianglesCenters[0], transformedTrianglesCenters.size() );
#endif
				//data->numTrees++;
			}
			else {
			 size_t k			= rnd.nextInt( 3, 5 );
			 mat4 rotation		= glm::toMat4( normalize( quat( glm::eulerAngleYXZ( rnd.nextFloat(0.1,4.0), rnd.nextFloat(0.01,0.075), rnd.nextFloat(0.01,0.075) ) ) ) );
			 mat4 scale			= glm::scale( mat4(1.0f),floraDensity * scale1 * vec3( rnd.randFloat( 8, 15 ) ) );
			 mat4 transform		= translation * rotation * scale;
			 
			 
			 if( rnd.nextFloat( 0.0f, 100.0f ) < 2.5f ){
				k		= 5;
				scale	= glm::scale( mat4(1.0f), vec3(1.0f) );
			}
			 
			 // transform vertices
			 vec3* vertices = mPopulationMeshes[k].getPositions<3>();
#ifdef HIGH_QUALITY_ANIMATIONS
			 vec4* centers = mPopulationMeshes[k].getTexCoords1<4>();
#endif
			 vector<vec3> texcoords;
			 vector<vec3> transformedVertices;
			 vector<vec4> transformedTrianglesCenters;
			 size_t indiceOffset = data->mTriMesh->getNumVertices();
			 for( size_t i = 0; i < mPopulationMeshes[k].getNumVertices(); ++i ){
					vec2 uv = ( vec2( p.x, p.z ) + vec2( offset.x, offset.z ) ) / vec2( mSize );
					texcoords.push_back( vec3( uv.x, uv.y, (float) j / (float) positions.size() ) );
					vec3 pos = vec3( transform * vec4( vertices[i], 1.0f ) );
					transformedVertices.push_back( pos );
					if( p.x + pos.x < min.x ) min.x = p.x + pos.x;
					if( p.y + pos.y < min.y ) min.y = p.y + pos.y;
					if( p.z + pos.z < min.z ) min.z = p.z + pos.z;
					if( p.x + pos.x > max.x ) max.x = p.x + pos.x;
					if( p.y + pos.y > max.y ) max.y = p.y + pos.y;
					if( p.z + pos.z > max.z ) max.z = p.z + pos.z;
				 
#ifdef HIGH_QUALITY_ANIMATIONS
					transformedTrianglesCenters.push_back( vec4( vec3( transform * vec4( vec3( centers[i] ), 1.0f ) ), ( centers[i].w + indiceOffset / (float) mPopulationMeshes[k].getNumVertices() ) / (float) positions.size() ) );
#endif
			 }
			 // offset indices
			 const vector<uint32_t> indices = mPopulationMeshes[k].getIndices();
			 vector<uint32_t> transformedIndices;
			 for( size_t i = 0; i < mPopulationMeshes[k].getNumIndices(); ++i ){
				 transformedIndices.push_back( indiceOffset + indices[i] );
			 }
			 // combine mesh with the main one
			 data->mTriMesh->appendIndices( &transformedIndices[0], transformedIndices.size() );
			 data->mTriMesh->appendPositions( &transformedVertices[0], transformedVertices.size() );
			 data->mTriMesh->appendTexCoords0( &texcoords[0], texcoords.size() );
#ifdef HIGH_QUALITY_ANIMATIONS
			 data->mTriMesh->appendTexCoords1( &transformedTrianglesCenters[0], transformedTrianglesCenters.size() );
#endif
		 }
			j++;
		}
		
		data->mBounds = AxisAlignedBox( min + offset, max + offset );
		
		// add a small delay to make sure all threads don't come back at the same time
		this_thread::sleep_for( chrono::milliseconds( 10 * ( start + 20 ) ) );
		
		// send back to the main thread
		mTilesPopulationBuffer->pushFront( data );
	}
}
void Terrain::updateTilesBounds()
{
	auto heightMap			= getHeightChannel();
	Area tileArea			= Area( ivec2(0), ivec2( vec2( mSize ) / (float) getNumTilesPerRow() ) );
	vector<vec2> samples	= poissonDiskDistribution( 8.0f / 1024.0f * mSize.x, tileArea );
	for( auto tile : mTiles ){
		tile->updateBounds( samples, heightMap, mArea );
	}
}

// MARK: Heightmap / terrain generation
void Terrain::generateHeightMap()
{
	// swap the things we need to interpolate between ( textures, bounds and splines )
	for( auto tile : mTiles ) tile->swapBounds();
	swap( mHeightMap[mHeightMapCurrent], mHeightMap[mHeightMapTemp] );
	swap( mRoadSpline3d[mHeightMapCurrent], mRoadSpline3d[mHeightMapTemp] );
	
	
	// transition from old heightmap to new heightmap
	mTimeline->applyPtr( &mHeightMapProgression, 0.0f, 1.0f, 2.0f, EaseInCubic() );
	
	// setup the shared color format
	auto textureFormat = gl::Texture2d::Format().internalFormat( GL_RGBA ).minFilter( GL_LINEAR ).magFilter( GL_LINEAR );
	
	// MARK: Load and compile shaders
	//-----------------------------------------------------
	auto heightMapShader		= loadShader( "Passtrough", "Heightmap" );
	auto sobelShader			= loadShader( "Passtrough", "Sobel" );
	auto kawaseBlurShader		= loadShader( "Passtrough", "KawaseBlur" );
	auto composeHeightMapShader	= loadShader( "Passtrough", "ComposeHeightMap" );
	auto meshDensityShader		= loadShader( "Passtrough", "DensityMap" );
	auto floraDensityShader		= loadShader( "Passtrough", "FloraDensity" );
	
	// MARK: Fbo setup
	//-----------------------------------------------------
	// setup ping pong fbo. we use two fbo instead of one to make it ES3 compatible
	gl::Texture2dRef pingPongTextures[2];
	gl::FboRef pingPongFbo[2];
	size_t readBuffer = 0;
	size_t writeBuffer = 1;
	for( size_t i = 0; i < 2; ++i ){
		// create the texture
		pingPongTextures[i] = gl::Texture2d::create( mSize.x, mSize.y, textureFormat );
		
		// and attach it to a new fbo
		gl::Fbo::Format fboFormat;
		fboFormat.attachment( GL_COLOR_ATTACHMENT0, pingPongTextures[i] );
		pingPongFbo[i] = gl::Fbo::create( mSize.x, mSize.y, fboFormat );
	}
	
	// clear targets
	for( size_t i = 0; i < 2; ++i ){
		gl::ScopedFramebuffer scopedFbo( pingPongFbo[i] );
		gl::clear( ColorA( 0.0f, 0.0f, 0.0f, 0.0f ) );
	}
	
	// MARK: Base Height Map generation
	//-----------------------------------------------------
	// start by creating the base height map with a sum of noises
	gl::Texture2dRef baseHeightMap;
	if( heightMapShader ){
		auto currentFbo = pingPongFbo[writeBuffer];
		
		gl::ScopedGlslProg scopedShader( heightMapShader );
		gl::ScopedFramebuffer scopedFbo( currentFbo );
		gl::ScopedViewport viewport( ivec2(0), currentFbo->getSize() );
		gl::ScopedMatrices matrices;
		gl::setMatricesWindow( currentFbo->getSize() );
		
		heightMapShader->uniform( "uOctaves", mNoiseOctaves );
		heightMapShader->uniform( "uNoiseScale", mNoiseScale );
		heightMapShader->uniform( "uNoiseSeed", (float) mNoiseSeed );
		
		gl::drawSolidRect( currentFbo->getBounds() );
		
		// save texture
		baseHeightMap = blitFromFbo( currentFbo, textureFormat );
	}
	
	// MARK: Height Map weighted road blur / fake erosion
	//-----------------------------------------------------
	// cheap way to fake erosion by smoothing out the part of the terrain that doesn't have a high slope
	
	// apply a sobel filter to get an idea of the terrain slopes
	gl::Texture2dRef tempSobel;
	if( sobelShader ){
		std::swap( readBuffer, writeBuffer );
		
		// calculate the slope of the current terrain using a sobel filter
		auto currentFbo = pingPongFbo[writeBuffer];
		auto texture = pingPongTextures[readBuffer];
		gl::ScopedGlslProg scopedShader( sobelShader );
		gl::ScopedFramebuffer scopedFbo( currentFbo );
		gl::ScopedTextureBind scopedTexture( texture, 0 );
		gl::ScopedViewport viewport( ivec2(0), currentFbo->getSize() );
		gl::ScopedMatrices matrices;
		gl::setMatricesWindow( currentFbo->getSize() );
		
		sobelShader->uniform( "uInvSize", vec2( 1.0f ) / vec2( currentFbo->getSize() ) );
		sobelShader->uniform( "uReadTexture", 0 );
		
		gl::drawSolidRect( currentFbo->getBounds() );
		
		// blur the sobelmap
		for( size_t i = 0; i < 8; ++i ){
			std::swap( readBuffer, writeBuffer );
			
			auto currentFbo = pingPongFbo[writeBuffer];
			auto texture = pingPongTextures[readBuffer];
			gl::ScopedGlslProg scopedShader( kawaseBlurShader );
			gl::ScopedFramebuffer scopedFbo( currentFbo );
			gl::ScopedTextureBind scopedTexture0( texture, 0 );
			gl::ScopedViewport viewport( ivec2(0), currentFbo->getSize() );
			gl::ScopedMatrices matrices;
			gl::setMatricesWindow( currentFbo->getSize() );
			
			kawaseBlurShader->uniform( "uInvSize", vec2( 1.0f ) / vec2( currentFbo->getSize() ) );
			kawaseBlurShader->uniform( "uReadTexture", 0 );
			kawaseBlurShader->uniform( "uIteration", static_cast<float>( i ) );
			
			gl::drawSolidRect( currentFbo->getBounds() );
		}
		
		// save texture
		tempSobel = blitFromFbo( pingPongFbo[writeBuffer], textureFormat );
	}
	
	// MARK: Road map / Camera path
	//-----------------------------------------------------
	// create the base road texture
	
	gl::Texture2dRef baseRoadMap, blurredRoadMap;
	if( kawaseBlurShader ){
		
		// bind the fbo & shader and set the viewport.
		auto currentFbo = pingPongFbo[writeBuffer];
		gl::ScopedFramebuffer scopedFbo( currentFbo );
		gl::ScopedGlslProg scopedShader( gl::getStockShader( gl::ShaderDef().color() ) );
		gl::ScopedViewport viewport( ivec2(0), currentFbo->getSize() );
		gl::ScopedMatrices matrices;
		gl::setMatricesWindow( currentFbo->getSize(), false );
		
		// clear the texture with black
		gl::clearColor( ColorA::black() );
		gl::clear();
		
		// creates random position from a noisy circle
		vec2 center = mSize * 0.5f;
		float radius = length( mSize ) / 3.5f;
		vector<vec2> positions2d;
		vector<vec3> positions3d;
		Perlin perlin( 4, mNoiseSeed );
		
		// we don't want the spline to be re-generated
		
		
		if( mRoadSpline3d[0].getNumControlPoints() < 1 ){
			for( float i = 0; i <= M_PI * 2.0f; i+= 0.05f ) {//( perlin.noise( i - 987.654f + mNoiseSeed ) + 0.8 ) * 0.15f ){
				float a = perlin.fBm( i * 2.0f + 123.456f + mNoiseSeed ) * 0.1f;
				float dist = radius + ( 0.3f + 0.5f * perlin.fBm( i * 1.25f + mNoiseSeed ) ) * radius;
				vec2 p = center + 0.5f * dist * vec2( cos( i + a ), sin( i + a ) );
				positions2d.push_back( p );
				positions3d.push_back( vec3( p.x, 0, p.y ) );
			}
			
			// use the noisy circle positions to create a spline and draw it to the texture
			mRoadSpline2d		= BSpline2f( positions2d, 3, true, false );
			mRoadSpline3d[0]	= BSpline3f( positions3d, 3, true, false );
			mRoadSpline3d[1]	= BSpline3f( positions3d, 3, true, false );
		}
		
		float step		= 0.0025f;
		float lineWidth = 1.0f;
		float invScale = 1.0f / 512.0f * mSize.x;
		gl::color( Color::white() );
		gl::VertBatch batch( GL_TRIANGLE_STRIP );
		for( float i = 0; i <= 1 + step; i+= step ){
			vec2 a		= mRoadSpline2d.getPosition( i > 0 ? i - step : 1.0f );
			vec2 b		= mRoadSpline2d.getPosition( i <= 1 ? i : 0.0f );
			vec2 diff	= a - b;
			vec2 perp	= glm::normalize( vec2( diff.y, -diff.x ) );
			float width = lineWidth + ( perlin.noise( mNoiseSeed + i * 50.1f ) * 0.5f + 0.5f ) * lineWidth * invScale;
			batch.vertex( a + perp * width );
			batch.vertex( a - perp * width );
			batch.vertex( b + perp * width );
			batch.vertex( b - perp * width );
		}
		batch.draw();
		
		// save texture
		baseRoadMap = blitFromFbo( currentFbo, textureFormat );
		
		//gl::Fbo::unbindFramebuffer();
		
		// blur the road
		for( size_t i = 0; i < mRoadBlurIterations; ++i ){
			std::swap( readBuffer, writeBuffer );
			
			auto currentFbo = pingPongFbo[writeBuffer];
			auto texture = pingPongTextures[readBuffer];
			gl::ScopedGlslProg scopedShader( kawaseBlurShader );
			gl::ScopedFramebuffer scopedFbo( currentFbo );
			gl::ScopedTextureBind scopedTexture0( texture, 0 );
			gl::ScopedViewport viewport( ivec2(0), currentFbo->getSize() );
			gl::ScopedMatrices matrices;
			gl::setMatricesWindow( currentFbo->getSize() );
			
			kawaseBlurShader->uniform( "uInvSize", vec2( 1.0f ) / vec2( currentFbo->getSize() ) );
			kawaseBlurShader->uniform( "uReadTexture", 0 );
			kawaseBlurShader->uniform( "uIteration", static_cast<float>( i ) );
			
			gl::drawSolidRect( currentFbo->getBounds() );
		}
		
		// save texture
		blurredRoadMap = blitFromFbo( pingPongFbo[writeBuffer], textureFormat );
	}
	
	// create a blurred version of the heightmap
	gl::Texture2dRef blurredHeightMap;
	if( kawaseBlurShader ){
		
		// copy back the base height map
		blitToFbo( baseHeightMap, pingPongFbo[writeBuffer] );
		
		// apply a blur
		for( size_t i = 0; i < mBlurIterations; ++i ){
			std::swap( readBuffer, writeBuffer );
			
			auto currentFbo = pingPongFbo[writeBuffer];
			auto texture = pingPongTextures[readBuffer];
			gl::ScopedGlslProg scopedShader( kawaseBlurShader );
			gl::ScopedFramebuffer scopedFbo( currentFbo );
			gl::ScopedTextureBind scopedTexture0( texture, 0 );
			gl::ScopedViewport viewport( ivec2(0), currentFbo->getSize() );
			gl::ScopedMatrices matrices;
			gl::setMatricesWindow( currentFbo->getSize() );
			
			kawaseBlurShader->uniform( "uInvSize", vec2( 1.0f ) / vec2( currentFbo->getSize() ) );
			kawaseBlurShader->uniform( "uReadTexture", 0 );
			kawaseBlurShader->uniform( "uIteration", static_cast<float>( i ) );
			
			gl::drawSolidRect( currentFbo->getBounds() );
		}
	}
	
	// create blur weight map that we're going to use later to control the heightmap blur
	if( composeHeightMapShader ){
		std::swap( readBuffer, writeBuffer );
		
		auto currentFbo = pingPongFbo[writeBuffer];
		
		gl::ScopedGlslProg scopedShader( composeHeightMapShader );
		gl::ScopedFramebuffer scopedFbo( currentFbo );
		gl::ScopedTextureBind scopedTexture0( baseHeightMap, 0 );
		gl::ScopedTextureBind scopedTexture1( pingPongTextures[readBuffer], 1 );
		gl::ScopedTextureBind scopedTexture2( baseRoadMap, 2 );
		gl::ScopedTextureBind scopedTexture3( blurredRoadMap, 3 );
		gl::ScopedTextureBind scopedTexture4( tempSobel, 4 );
		gl::ScopedViewport viewport( ivec2(0), currentFbo->getSize() );
		gl::ScopedMatrices matrices;
		gl::setMatricesWindow( currentFbo->getSize() );
		
		composeHeightMapShader->uniform( "uHeightMap", 0 );
		composeHeightMapShader->uniform( "uBlurredHeightMap", 1 );
		composeHeightMapShader->uniform( "uRoadTexture", 2 );
		composeHeightMapShader->uniform( "uBlurredRoadTexture", 3 );
		composeHeightMapShader->uniform( "uTerrainSlope", 4 );
		
		gl::drawSolidRect( currentFbo->getBounds() );
		
		// save texture
		mHeightMap[mHeightMapCurrent] = blitFromFbo( pingPongFbo[writeBuffer], textureFormat );
	}
	
	
	// MARK: Road spline height correction
	//-----------------------------------------------------
	auto heightChannel = getTextureAsChannel( mHeightMap[mHeightMapCurrent] );
	for( size_t i = 0; i < mRoadSpline3d[mHeightMapCurrent].getNumControlPoints(); ++i ){
		vec3 controlPoint = mRoadSpline3d[mHeightMapCurrent].getControlPoint( i );
		//controlPoint.z = mSize.y - controlPoint.z;
		controlPoint.y = heightChannel->getValue( ivec2( controlPoint.x, controlPoint.z ) );
		mRoadSpline3d[mHeightMapCurrent].setControlPoint( i, controlPoint );
	}
	
	
	// MARK: Mesh density Map
	//-----------------------------------------------------
	gl::Texture2dRef finalSobelMap;
	if( sobelShader ){
		std::swap( readBuffer, writeBuffer );
		
		// calculate the slope of the current terrain using a sobel filter
		auto currentFbo = pingPongFbo[writeBuffer];
		auto texture = pingPongTextures[readBuffer];
		gl::ScopedGlslProg scopedShader( sobelShader );
		gl::ScopedFramebuffer scopedFbo( currentFbo );
		gl::ScopedTextureBind scopedTexture( texture, 0 );
		gl::ScopedViewport viewport( ivec2(0), currentFbo->getSize() );
		gl::ScopedMatrices matrices;
		gl::setMatricesWindow( currentFbo->getSize() );
		
		sobelShader->uniform( "uInvSize", vec2( 1.0f ) / vec2( currentFbo->getSize() ) );
		sobelShader->uniform( "uReadTexture", 0 );
		
		gl::drawSolidRect( currentFbo->getBounds() );
		
		// blur the sobelmap
		for( size_t i = 0; i < mSobelBlurIterations; ++i ){
			std::swap( readBuffer, writeBuffer );
			
			auto currentFbo = pingPongFbo[writeBuffer];
			auto texture = pingPongTextures[readBuffer];
			gl::ScopedGlslProg scopedShader( kawaseBlurShader );
			gl::ScopedFramebuffer scopedFbo( currentFbo );
			gl::ScopedTextureBind scopedTexture0( texture, 0 );
			gl::ScopedViewport viewport( ivec2(0), currentFbo->getSize() );
			gl::ScopedMatrices matrices;
			gl::setMatricesWindow( currentFbo->getSize() );
			
			kawaseBlurShader->uniform( "uInvSize", vec2( 1.0f ) / vec2( currentFbo->getSize() ) );
			kawaseBlurShader->uniform( "uReadTexture", 0 );
			kawaseBlurShader->uniform( "uIteration", static_cast<float>( i ) );
			
			gl::drawSolidRect( currentFbo->getBounds() );
		}
		
		// save texture
		finalSobelMap = blitFromFbo( pingPongFbo[writeBuffer], textureFormat );
	}
	
	
	// MARK: Flora density Map
	//-----------------------------------------------------
	if( floraDensityShader ){
		
		// calculate the slope of the current terrain using a sobel filter
		auto currentFbo = pingPongFbo[writeBuffer];
		auto texture = pingPongTextures[readBuffer];
		gl::ScopedGlslProg scopedShader( floraDensityShader );
		gl::ScopedFramebuffer scopedFbo( currentFbo );
		gl::ScopedTextureBind scopedTexture0( baseRoadMap, 0 );
		gl::ScopedTextureBind scopedTexture1( blurredRoadMap, 1 );
		gl::ScopedTextureBind scopedTexture2( tempSobel, 2 );
		gl::ScopedTextureBind scopedTexture3( mHeightMap[mHeightMapCurrent], 3 );
		gl::ScopedViewport viewport( ivec2(0), currentFbo->getSize() );
		gl::ScopedMatrices matrices;
		gl::setMatricesWindow( currentFbo->getSize() );
		
		floraDensityShader->uniform( "uRoadTexture", 0 );
		floraDensityShader->uniform( "uBlurredRoadTexture", 1 );
		floraDensityShader->uniform( "uTerrainSlope", 2 );
		floraDensityShader->uniform( "uHeightMap", 3 );
		floraDensityShader->uniform( "uNoiseSeed", vec2( mNoiseSeed ) );
		
		Rand rnd( mNoiseSeed );
		//float density = rnd.nextFloat( -0.5, 1.5f );
		float density = rnd.nextFloat( 0.05f, 1.5f );
		floraDensityShader->uniform( "uDensity", density );
		
		gl::drawSolidRect( currentFbo->getBounds() );
		
		// save texture
		mFloraDensityMap = blitFromFbo( pingPongFbo[writeBuffer], textureFormat );
	}
	
	// create density map
	if( meshDensityShader ){
		auto currentFbo = pingPongFbo[writeBuffer];
		
		gl::ScopedGlslProg scopedShader( meshDensityShader );
		gl::ScopedFramebuffer scopedFbo( currentFbo );
		gl::ScopedTextureBind scopedTexture0( baseRoadMap, 0 );
		gl::ScopedTextureBind scopedTexture1( blurredRoadMap, 1 );
		gl::ScopedTextureBind scopedTexture2( finalSobelMap, 2 );
		gl::ScopedViewport viewport( ivec2(0), currentFbo->getSize() );
		gl::ScopedMatrices matrices;
		gl::setMatricesWindow( currentFbo->getSize() );
		
		meshDensityShader->uniform( "uRoadTexture", 0 );
		meshDensityShader->uniform( "uBlurredRoadTexture", 1 );
		meshDensityShader->uniform( "uTerrainSlope", 2 );
		
		gl::drawSolidRect( currentFbo->getBounds() );
		
		// save texture
		mMeshDensityMap = blitFromFbo( currentFbo, textureFormat );
	}
	
	// update bounds
	updateTilesBounds();
	
	/*
	// use the flora map to generate poisson disk samples
	auto floraMap			= getTextureAsChannel( mFloraDensityMap );
	//auto heightMap			= getTextureAsChannel( mHeightMap[mHeightMapCurrent] );
	vector<vec2> samples	= poissonDiskDistribution( []( const vec2& p ){
		return 50.0f;
	}, [&]( const vec2& p ){
		float s = floraMap->getValue( p + vec2( 100 ) );
		return s < 0.5f;
	}, Area( ivec2( 0 ), ivec2( mSize ) - ivec2( 200 ) ) );
	
	
	// shuffle the vector so we can get the 3 first one
	std::random_shuffle( samples.begin(), samples.end() );
	
	// copy and convert to 3d position vector
	mClearings.clear();
	int k = std::min( (int) samples.size(), 3 );
	for( int i = 0; i < k; i++ ){
		//float y = heightMap->getValue( vec2( samples[i].x, samples[i].y ) );
		mClearings.push_back( vec3( samples[i].x + 100.0f, 0, samples[i].y + 100.0f ) );
	}*/
}

// MARK: Triangle Heightmap rendering
void Terrain::generateTriangleHeightMap()
{
	// swap texture buffers
	swap( mTrianglesHeightMap[mHeightMapCurrent], mTrianglesHeightMap[mHeightMapTemp] );
	
	// create a shader that will output the actual height at each pixel
	auto shader = loadShader( "TriangleHeightMap" );
	
	if( shader ){
		// create texture to render our triangles. Make sure we have good precision.
		mTrianglesHeightMap[mHeightMapCurrent] = gl::Texture2d::create( mSize.x, mSize.y, gl::Texture2d::Format().internalFormat( GL_RGBA ) );
		
		// create a new fbo with the texture we just created as the only attachment.
		gl::Fbo::Format fboFormat;
		fboFormat.attachment( GL_COLOR_ATTACHMENT0, mTrianglesHeightMap[mHeightMapCurrent] );
		gl::Fbo::unbindFramebuffer();
		gl::FboRef fbo = gl::Fbo::create( mSize.x, mSize.y, fboFormat );
		
		// bind the fboand set the viewport.
		gl::ScopedFramebuffer	scopedFbo( fbo );
		gl::ScopedViewport		scopedViewport( ivec2(0), mSize );
		gl::clear();
		
		// bind shader and heightmap texture
		gl::ScopedGlslProg scopedShader( shader );
		gl::ScopedTextureBind scopedTexture0( getHeightMap(), 0 );
		shader->uniform( "uHeightMap", 0 );
		
		// save the current matrices and setup a new one with orthographic projection
		gl::ScopedMatrices scopedMatrices;
		gl::ScopedFaceCulling disableCulling( false );
		gl::ScopedDepth	scopedDepth( true );
		
		CameraOrtho camOrtho( 0, mSize.x, 0, -mSize.y, -1000.0, 1000 );
		gl::setMatrices( camOrtho );
		
		gl::rotate( M_PI_2, vec3( 1, 0, 0 ) );
		
		for( auto tile : mTiles ){
			if( tile->mTriMesh.getNumTriangles() ){
				auto batch = gl::Batch::create( tile->mTriMesh, shader );
				batch->draw();
			}
		}
	}
}


// MARK: Textures / Channels getters
Channel32fRef Terrain::getHeightChannel()
{
	return getTextureAsChannel( mHeightMap[mHeightMapCurrent] );
}
Channel32fRef Terrain::getMeshDensityChannel()
{
	return getTextureAsChannel( mMeshDensityMap );
}
Channel32fRef Terrain::getFloraDensityChannel()
{
	return getTextureAsChannel( mFloraDensityMap );
}
Channel32fRef Terrain::getTrianglesHeightChannel()
{
	return getTextureAsChannel( mTrianglesHeightMap[mHeightMapCurrent] );
}

// MARK: getters/setters

void Terrain::setElevation( float elevation )
{
	mElevation = elevation;
}
void Terrain::setFogDensity( float density )
{
	mFogDensity = density;
}
void Terrain::setFogColor( const ci::Color &color )
{
	mFogColor = color;
}
void Terrain::setSunDispertion( float dispertion )
{
	mSunDispertion = dispertion;
}
void Terrain::setSunIntensity( float intensity )
{
	mSunIntensity = intensity;
}
void Terrain::setSunColor( const ci::Color &color )
{
	mSunColor = color;
}
void Terrain::setSunDirection( const ci::vec3 &direction )
{
	mSunDirection = direction;
}
void Terrain::setSunScatteringCoeffs( const ci::vec3 &coeffs )
{
	mSunScatteringCoeffs = coeffs;
}

float Terrain::getFogDensity() const
{
	return mFogDensity;
}
ci::Color Terrain::getFogColor() const
{
	return mFogColor;
}
float Terrain::getSunDispertion() const
{
	return mSunDispertion;
}
float Terrain::getSunIntensity() const
{
	return mSunIntensity;
}
ci::Color Terrain::getSunColor() const
{
	return mSunColor;
}
ci::vec3 Terrain::getSunDirection() const
{
	return mSunDirection;
}
ci::vec3 Terrain::getSunScatteringCoeffs() const
{
	return mSunScatteringCoeffs;
}