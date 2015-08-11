// Platform specific extensions and preprocessors
#if ( CINDER_GL_PLATFORM == CINDER_DESKTOP )
	#extension GL_ARB_explicit_attrib_location : enable

	// ignore opengl es precision specifiers
	#define lowp
	#define mediump
	#define highp

#elif ( CINDER_GL_PLATFORM == CINDER_GL_ES_3 )
	#ifdef MEDIUM_PRECISION
		precision mediump float;
	#else
		precision highp float;
	#endif
#endif

// constants
#define PI 3.1415926535897932384626433832795

// usefull shortcuts
#define saturate(x) clamp( x, 0.0, 1.0 )