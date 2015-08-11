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

#include "cinder/gl/Texture.h"
#include "cinder/gl/Shader.h"
#include "cinder/gl/GlslProg.h"
#include "cinder/gl/Batch.h"
#include "cinder/BSpline.h"
#include "cinder/Channel.h"
#include "cinder/ConcurrentCircularBuffer.h"
#include "cinder/Signals.h"
#include "cinder/TriMesh.h"
#include "cinder/Timeline.h"

//#define HIGH_QUALITY_ANIMATIONS
//#define WIP

typedef std::shared_ptr<class Terrain> TerrainRef;

class Terrain : public std::enable_shared_from_this<Terrain>{
public:

	struct Format {
		Format() : mSize( 850 ), mElevation( 120.0f ), mNoiseOctaves( 8 ), mNoiseScale( 5.0f ), mNoiseSeed( 1 ), mRoadBlurIterations( 4 ), mBlurIterations( 15 ), mSobelBlurIterations( 5 ), mNumTilesPerRow( 5 ), mNumWorkingThreads( 8 ) {}
		
		//! specifies the size and resolution of the terrain
		Format&	size( const ci::vec2 &size ) { mSize = size; return *this; }
		//! specifies the terrain elevation
		Format&	elevation( const float &aElevation ) { mElevation = aElevation; return *this; }
		
		//! specifies the number of octaves to be used in the noise sum generation
		Format&	noiseOctaves( int octaves ) { mNoiseOctaves = octaves; return *this; }
		//! specifies the scale to be used in the noise sum generation
		Format&	noiseScale( float scale ) { mNoiseScale = scale; return *this; }
		//! specifies the random seed to be used in the noise sum generation
		Format&	noiseSeed( float seed ) { mNoiseSeed = seed; return *this; }
		
		//! specifies the number of working threads for the triangulation, mesh and object distribution
		Format&	workingThreads( size_t threads ) { mNumWorkingThreads = threads; return *this; }
		//! specifies the number of tiles per row, impacts both the performances and the threading efficiency
		Format&	tilesPerRow( size_t tiles ) { mNumTilesPerRow = tiles; return *this; }
		
		//! specifies the number of times the road has to be blurred before being integrated in the heightmap
		Format&	roadBlurIterations( int iterations ) { mRoadBlurIterations = iterations; return *this; }
		//! specifies the random seed to be used in the noise sum generation
		Format&	blurIterations( int iterations ) { mBlurIterations = iterations; return *this; }
		//! specifies the random seed to be used in the noise sum generation
		Format&	sobelBlurIterations( int iterations ) { mSobelBlurIterations = iterations; return *this; }
		
		//! returns the size and resolution of the terrain
		ci::vec2	getSize() const { return mSize; }
		//! returns the terrain elevation
		float		getElevation() const { return mElevation * 1.3333f; }
		
		//! returns the number of octaves to be used in the noise sum generation
		int			getNoiseOctaves() const { return mNoiseOctaves; }
		//! returns the scale to be used in the noise sum generation
		float		getNoiseScale() const { return mNoiseScale; }
		//! returns the random seed to be used in the noise sum generation
		float		getNoiseSeed() const { return mNoiseSeed; }
		
		//! returns the number of working threads for the triangulation, mesh and object distribution
		size_t		getNumWorkingThreads() const { return mNumWorkingThreads; }
		//! returns the number of tiles per row, impacts both the performances and the threading efficiency
		size_t		getNumTilesPerRow() const { return mNumTilesPerRow; }
		
		//! returns the random seed to be used in the noise sum generation
		int			getRoadBlurIterations() const { return mRoadBlurIterations; }
		//! returns the random seed to be used in the noise sum generation
		int			getBlurIterations() const { return mBlurIterations; }
		//! returns the random seed to be used in the noise sum generation
		int			getSobelBlurIterations() const { return mSobelBlurIterations; }
	protected:
		size_t		mNumTilesPerRow;
		size_t		mNumWorkingThreads;
		ci::vec2	mSize;
		float		mElevation;
		int			mNoiseOctaves;
		float		mNoiseScale;
		float		mNoiseSeed;
		int			mRoadBlurIterations;
		int			mBlurIterations;
		int			mSobelBlurIterations;
	};
	
	//! constructs and returns a new terrain
	static TerrainRef create( const Format &format = Format() );
	
	//! renders the terrain from the camera point of view
	void render( const ci::CameraPersp &camera );
	
	//! starts the generation process
	void start();
	
	//! regenerate the population content of each tiles
	void generateTilesPopulation();
	//! removes the population content of each tiles
	void removeTilesPopulation();
	//! removes the old population and regenerate with new population for of each tiles
	void regenerateTilesPopulation();
	
	void createTiles();
	void generateHeightMap();
	void generateTriangleHeightMap();
	
	//! sets the terrain elevation
	void		setElevation( float elevation );
	
	//! sets the fog density
	void		setFogDensity( float density );
	//! sets the fog color
	void		setFogColor( const ci::Color &color );
	//! sets the dispertion of the color of the sun
	void		setSunDispertion( float dispertion );
	//! sets the intensity of the sun
	void		setSunIntensity( float intensity );
	//! sets the sun color
	void		setSunColor( const ci::Color &color );
	//! sets the sun position in the sky
	void		setSunDirection( const ci::vec3 &direction );
	//! sets the atmospheric scattering coefficients
	void		setSunScatteringCoeffs( const ci::vec3 &coeffs );
	
	//! returns the size and resolution of the terrain
	ci::vec2	getSize() const { return mSize; }
	//! returns the terrain elevation
	float		getElevation() const { return mElevation; }
	
	//! returns the fog density
	float		getFogDensity() const;
	//! returns the fog color
	ci::Color	getFogColor() const;
	//! returns the dispertion of the color of the sun
	float		getSunDispertion() const;
	//! returns the intensity of the sun
	float		getSunIntensity() const;
	//! returns the sun color
	ci::Color	getSunColor() const;
	//! returns the sun position in the sky
	ci::vec3	getSunDirection() const;
	//! return the atmospheric scattering coefficients
	ci::vec3	getSunScatteringCoeffs() const;
	
	//! returns the height map texture
	ci::gl::Texture2dRef	getHeightMap() const { return mHeightMap[mHeightMapCurrent]; }
	//! downloads and returns the cpu version of the height map texture
	ci::Channel32fRef		getHeightChannel();
	//! returns the mesh density texture
	ci::gl::Texture2dRef	getMeshDensityMap() const { return mMeshDensityMap; }
	//! downloads and returns the cpu version of the mesh density texture
	ci::Channel32fRef		getMeshDensityChannel();
	//! returns the tree distribution map
	ci::gl::Texture2dRef	getFloraDensityMap() const { return mFloraDensityMap; }
	//! downloads and returns the cpu version of the tree distribution map
	ci::Channel32fRef		getFloraDensityChannel();
	//! returns the actual triangle height map
	ci::gl::Texture2dRef	getTrianglesHeightMap() const { return mTrianglesHeightMap[mHeightMapCurrent]; }
	//! downloads and returns the cpu version of the actual triangle height map
	ci::Channel32fRef		getTrianglesHeightChannel();
	
	//! returns the 2d b-spline representing the road
	ci::BSpline2f getRoadSpline2d() const { return mRoadSpline2d; }
	//! returns the 3d b-spline representing the road
	ci::BSpline3f getRoadSpline3d( int spline = -1 ) const { return mRoadSpline3d[spline == -1 ? mHeightMapCurrent : spline ]; }
	//! returns the 3d b-spline total length
	float getRoadSpline3dLength() const { return mRoadSpline3dLength; }
	
	//! returns the number of working threads for the triangulation, mesh and object distribution
	size_t	getNumWorkingThreads() const { return mNumWorkingThreads; }
	//! returns the number of tiles per row, impacts both the performances and the threading efficiency
	size_t	getNumTilesPerRow() const { return mNumTilesPerRow; }
	
	//! represents a single tile of the terrain
	class Tile {
	public:
		static std::shared_ptr<Terrain::Tile> create( size_t tileId, const ci::Area &tileArea, const ci::Area &fullArea, float contentScale, float randomSeed, const ci::Channel32fRef &heightMap, const ci::Channel32fRef &densityMap, const ci::BSpline2f &spline, size_t tilesPerRow, float elevation );
		
		size_t					getTileId() const { return mTileId; }
		
		ci::gl::BatchRef		getBatch() const { return mBatch; }
		ci::gl::BatchRef		getPopulationBatch() const { return mPopulation[mPopulationCurrent]; }
		
		ci::Area				getArea() const { return mArea; }
		ci::vec2				getSize() const { return mSize; }
		ci::AxisAlignedBox	getBounds( float elevation = 1.0f, float interpolation = 1.0f ) const;
		
		//! returns whether this tile has been occluded for a certain amount of frames
		bool isOccluded( size_t numFrames = 5 );
		
		Tile( size_t tileId, const ci::Area &tileArea, const ci::Area &fullArea, float contentScale, float randomSeed, const ci::Channel32fRef &heightMap, const ci::Channel32fRef &densityMap, const ci::BSpline2f &spline, size_t tilesPerRow, float elevation );
		
		~Tile();
		
		void clear();
		void clearPopulation();
		
		void updateBounds( const std::vector<ci::vec2> &samples, const ci::Channel32fRef &heightMap, const ci::Area &fullArea );
		void swapBounds();
		
	protected:
		void buildMeshes( const ci::gl::GlslProgRef &shader );
		void buildOcclusionMesh();
		void buildPopulationMeshes( const ci::TriMeshRef &triMesh, const ci::AxisAlignedBox &bounds, const ci::gl::GlslProgRef &shader );
		void resetOccludedFrameCount();
		void checkOcclusion();
		void queryOcclusionResults();
		
		size_t							mTileId;
		ci::AxisAlignedBox			mBounds[2];
		ci::vec2						mHeightRange[2];
		
		ci::gl::BatchRef				mBatch;
		ci::gl::BatchRef				mPopulation[2];
		size_t							mPopulationCurrent, mPopulationTemp;
		ci::gl::BatchRef				mOccluderBatch;
		ci::vec3						mPosition;
		
		struct OcclusionQuery {
			OcclusionQuery() : mId(0), mUsed(false) {}
			GLuint	mId;
			bool	mUsed;
		};
		std::vector<OcclusionQuery>		mOcclusionQueries;
		
		float							mTerrainCompletion;
		float							mPopulationCompletion[2];
		size_t							mNumFramesOccluded;
		ci::Area						mArea;
		ci::vec2						mSize;
		ci::TriMesh						mTriMesh;
		
		int numTrees = 0;
		
		friend class Terrain;
	};
	
	typedef std::shared_ptr<Tile> TileRef;
	
	void		setTileExplosionCenter( const ci::vec3 &center ) { mTileExplosionCenter = center; }
	ci::vec3&	getTileExplosionCenter() { return mTileExplosionCenter; }
	void		setTilePopulationExplosionCenter( const ci::vec3 &center ) { mTilePopulationExplosionCenter = center; }
	ci::vec3&	getTilePopulationExplosionCenter() { return mTilePopulationExplosionCenter; }
	float&		getTileExplosionSize() { return mTileExplosionSize; }
	float&		getTilePopulationExplosionSize() { return mTilePopulationExplosionSize; }
	float		getTilePopulationBalance() const { return mTilePopulationBalance; }
	void		setTilePopulationBalance( float balance ) { mTilePopulationBalance = balance; }
	
	//! returns the terrain's tiles
	const std::vector<TileRef>& getTiles() const { return mTiles; }

	//! returns the shader that takes care of rendering the actual terrain
	ci::gl::GlslProgRef getTileShader() const { return mTileShader; }
	//! returns the shader that takes care of rendering the tiles instanced content
	ci::gl::GlslProgRef getTileContentShader() const { return mTileContentShader; }
	
	//! returns the percentage of tiles that have been built already (between 0 and 1)
	float getTilesThreadsCompletion();
	
	//! TODO: Comment
	float getHeightMapProgression() const { return mHeightMapProgression; }
	
	//! sets the number of octaves to be used in the noise sum generation
	void setNumNoiseOctaves( int octaves ) { mNoiseOctaves = octaves; }
	//! sets the scale to be used in the noise sum generation
	void setNoiseScale( float scale ) { mNoiseScale = scale; }
	//! sets the random seed to be used in the noise sum generation
	void setNoiseSeed( float seed ) { mNoiseSeed = seed; }
	//! sets the number of times the road has to be blurred before being integrated in the heightmap
	void setNumRoadBlurIterations( int iterations ) { mRoadBlurIterations = iterations; }
	//! sets the random seed to be used in the noise sum generation
	void setNumBlurIterations( int iterations ) { mBlurIterations = iterations; }
	//! sets the random seed to be used in the noise sum generation
	void setNumSobelBlurIterations( int iterations ) { mSobelBlurIterations = iterations; }
	
	//! returns whether the occlusion culling pass is enabled or not
	bool isOcclusionCullingEnabled() const { return mOcclusionCullingEnabled; }
	//! sets whether the occlusion culling pass is enabled or not
	void setOcclusionCullingEnabled( bool enabled = true ) { mOcclusionCullingEnabled = enabled; }
	
	//! returns the total number of instances rendered in the last frame for debug
	size_t getNumRenderedInstances() const { return mNumRenderedInstanced; }
	
	// keep the constructor public but make it unacessible
	// solves the private constructor std::make_shared issue
protected:
	struct LeaveMeAlone {};
public:
	Terrain( const Format &format, LeaveMeAlone access );
	~Terrain();
//protected:
	
	void updateTiles();
	void buildTilesThreaded( size_t start, size_t end, size_t numTilesPerRow, const ci::vec2 &tileSize, const ci::Area &area, float scale, const ci::Channel32fRef &heightMap, const ci::Channel32fRef &densityMap );
	
	void populateTiles();
	void updateTilePopulating();
	void populateTilesThreaded( size_t start, size_t end, size_t numTilesPerRow, const ci::vec2 &tileSize, const ci::Area &area, const ci::Channel32fRef &floraMap );
	
	void updateTilesBounds();
	
	struct PopulationData {
		size_t					mTileId;
		ci::TriMeshRef			mTriMesh;
		ci::AxisAlignedBox	mBounds;
	};
	
	typedef std::shared_ptr<PopulationData> PopulationDataRef;
	
	// a few useful type aliases
	using CircularTileBuffer			= ci::ConcurrentCircularBuffer<TileRef>;
	using CircularTileBufferRef			= std::unique_ptr<CircularTileBuffer>;
	using TileWorkThreads				= std::vector<std::unique_ptr<std::thread>>;
	using CircularPopulationBuffer		= ci::ConcurrentCircularBuffer<PopulationDataRef>;
	using CircularPopulationBufferRef	= std::unique_ptr<CircularPopulationBuffer>;
	
	ci::Area					mArea;
	ci::vec2					mSize;
	
	float						mElevation;
	int							mNoiseOctaves;
	float						mNoiseScale;
	float						mNoiseSeed;
	int							mRoadBlurIterations;
	int							mBlurIterations;
	int							mSobelBlurIterations;
	
	float						mFogDensity;
	ci::Color					mFogColor;
	float						mSunDispertion;
	float						mSunIntensity;
	ci::Color					mSunColor;
	ci::vec3					mSunDirection;
	ci::vec3					mSunScatteringCoeffs;
	
	size_t						mNumTilesPerRow;
	size_t						mNumTilePopulated;
	size_t						mNumWorkingThreads;
	
	CircularTileBufferRef		mTilesBuffer;
	CircularPopulationBufferRef	mTilesPopulationBuffer;
	TileWorkThreads				mWorkThreads;
	ci::signals::Connection		mUpdateTilesConnection;
	std::vector<TileRef>		mTiles;
	
	ci::BSpline2f				mRoadSpline2d;
	ci::BSpline3f				mRoadSpline3d[2];
	float						mRoadSpline3dLength;
	
	ci::gl::Texture2dRef		mHeightMap[2];
	ci::gl::Texture2dRef		mTrianglesHeightMap[2];
	size_t						mHeightMapCurrent;
	size_t						mHeightMapTemp;
	float						mHeightMapProgression;
	
	ci::gl::Texture2dRef		mMeshDensityMap;
	ci::gl::Texture2dRef		mFloraDensityMap;
	ci::gl::Texture2dRef		mNoiseLookupTable;
	
	ci::gl::GlslProgRef			mTileShader;
	ci::gl::GlslProgRef			mTileContentShader;
	ci::gl::GlslProgRef			mSkyShader;
	//ci::gl::GlslProgRef			mClearingObjectsShader;
	ci::gl::BatchRef			mSkyBatch;
	std::vector<ci::TriMesh>	mPopulationMeshes;
	
#ifdef WIP
	//std::vector<ci::vec3>			mClearings;
	//std::vector<ci::gl::BatchRef>	mClearingsBatches;
#endif
	
	ci::TimelineRef				mTimeline;
	
	bool						mBuildingTiles;
	bool						mPopulatingTiles;
	
	ci::vec3					mTileExplosionCenter;
	ci::vec3					mTilePopulationExplosionCenter;
	float						mTileExplosionSize;
	float						mTilePopulationExplosionSize;
	float						mTilePopulationBalance;
	
	bool						mOcclusionCullingEnabled;
	size_t						mNumRenderedInstanced;
};