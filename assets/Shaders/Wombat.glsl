//
//  Wombat
//  An efficient texture-free GLSL procedural noise library
//  Source: https://github.com/BrianSharpe/Wombat
//  Derived from: https://github.com/BrianSharpe/GPU-Noise-Lib
//
//  I'm not one for copyrights.  Use the code however you wish.
//  All I ask is that credit be given back to the blog or myself when appropriate.
//  And also to let me know if you come up with any changes, improvements, thoughts or interesting uses for this stuff. :)
//  Thanks!
//
//  Brian Sharpe
//  brisharpe CIRCLE_A yahoo DOT com
//  http://briansharpe.wordpress.com
//  https://github.com/BrianSharpe
//

//
//  This is a modified version of Stefan Gustavson's and Ian McEwan's work at http://github.com/ashima/webgl-noise
//  Modifications are...
//  - faster random number generation
//  - analytical final normalization
//  - space scaled can have an approx feature size of 1.0
//

//
//  Simplex Perlin Noise 2D Deriv
//  Return value range of -1.0->1.0, with format vec3( value, xderiv, yderiv )
//

vec3 SimplexPerlin2D_Deriv( vec2 P )
{
    //  https://github.com/BrianSharpe/Wombat/blob/master/SimplexPerlin2D_Deriv.glsl

    //  simplex math constants
    const float SKEWFACTOR = 0.36602540378443864676372317075294;            // 0.5*(sqrt(3.0)-1.0)
    const float UNSKEWFACTOR = 0.21132486540518711774542560974902;          // (3.0-sqrt(3.0))/6.0
    const float SIMPLEX_TRI_HEIGHT = 0.70710678118654752440084436210485;    // sqrt( 0.5 )	height of simplex triangle
    const vec3 SIMPLEX_POINTS = vec3( 1.0-UNSKEWFACTOR, -UNSKEWFACTOR, 1.0-2.0*UNSKEWFACTOR );  //  simplex triangle geo

    //  establish our grid cell.
    P *= SIMPLEX_TRI_HEIGHT;    // scale space so we can have an approx feature size of 1.0
    vec2 Pi = floor( P + dot( P, vec2( SKEWFACTOR ) ) );

    // calculate the hash
    vec4 Pt = vec4( Pi.xy, Pi.xy + 1.0 );
    Pt = Pt - floor(Pt * ( 1.0 / 71.0 )) * 71.0;
    Pt += vec2( 26.0, 161.0 ).xyxy;
    Pt *= Pt;
    Pt = Pt.xzxz * Pt.yyww;
    vec4 hash_x = fract( Pt * ( 1.0 / 951.135664 ) );
    vec4 hash_y = fract( Pt * ( 1.0 / 642.949883 ) );

    //  establish vectors to the 3 corners of our simplex triangle
    vec2 v0 = Pi - dot( Pi, vec2( UNSKEWFACTOR ) ) - P;
    vec4 v1pos_v1hash = (v0.x < v0.y) ? vec4(SIMPLEX_POINTS.xy, hash_x.y, hash_y.y) : vec4(SIMPLEX_POINTS.yx, hash_x.z, hash_y.z);
    vec4 v12 = vec4( v1pos_v1hash.xy, SIMPLEX_POINTS.zz ) + v0.xyxy;

    //  calculate the dotproduct of our 3 corner vectors with 3 random normalized vectors
    vec3 grad_x = vec3( hash_x.x, v1pos_v1hash.z, hash_x.w ) - 0.49999;
    vec3 grad_y = vec3( hash_y.x, v1pos_v1hash.w, hash_y.w ) - 0.49999;
    vec3 norm = inversesqrt( grad_x * grad_x + grad_y * grad_y );
    grad_x *= norm;
    grad_y *= norm;
    vec3 grad_results = grad_x * vec3( v0.x, v12.xz ) + grad_y * vec3( v0.y, v12.yw );

    //	evaluate the kernel
    vec3 m = vec3( v0.x, v12.xz ) * vec3( v0.x, v12.xz ) + vec3( v0.y, v12.yw ) * vec3( v0.y, v12.yw );
    m = max(0.5 - m, 0.0);
    vec3 m2 = m*m;
    vec3 m4 = m2*m2;

    //  calc the derivatives
    vec3 temp = 8.0 * m2 * m * grad_results;
    float xderiv = dot( temp, vec3( v0.x, v12.xz ) ) - dot( m4, grad_x );
    float yderiv = dot( temp, vec3( v0.y, v12.yw ) ) - dot( m4, grad_y );

    //  Normalization factor to scale the final result to a strict 1.0->-1.0 range
    //  http://briansharpe.wordpress.com/2012/01/13/simplex-noise/#comment-36
    const float FINAL_NORMALIZATION = 99.204334582718712976990005025589;

    //  sum and return all results as a vec3
    return vec3( dot( m4, grad_results ), xderiv, yderiv ) * FINAL_NORMALIZATION;
}


//
//  Simplex Perlin Noise 3D Deriv
//  Return value range of -1.0->1.0, with format vec4( value, xderiv, yderiv, zderiv )
//
vec4 SimplexPerlin3D_Deriv(vec3 P)
{
    //  https://github.com/BrianSharpe/Wombat/blob/master/SimplexPerlin3D_Deriv.glsl

    //  simplex math constants
    const float SKEWFACTOR = 1.0/3.0;
    const float UNSKEWFACTOR = 1.0/6.0;
    const float SIMPLEX_CORNER_POS = 0.5;
    const float SIMPLEX_TETRAHADRON_HEIGHT = 0.70710678118654752440084436210485;    // sqrt( 0.5 )

    //  establish our grid cell.
    P *= SIMPLEX_TETRAHADRON_HEIGHT;    // scale space so we can have an approx feature size of 1.0
    vec3 Pi = floor( P + dot( P, vec3( SKEWFACTOR) ) );

    //  Find the vectors to the corners of our simplex tetrahedron
    vec3 x0 = P - Pi + dot(Pi, vec3( UNSKEWFACTOR ) );
    vec3 g = step(x0.yzx, x0.xyz);
    vec3 l = 1.0 - g;
    vec3 Pi_1 = min( g.xyz, l.zxy );
    vec3 Pi_2 = max( g.xyz, l.zxy );
    vec3 x1 = x0 - Pi_1 + UNSKEWFACTOR;
    vec3 x2 = x0 - Pi_2 + SKEWFACTOR;
    vec3 x3 = x0 - SIMPLEX_CORNER_POS;

    //  pack them into a parallel-friendly arrangement
    vec4 v1234_x = vec4( x0.x, x1.x, x2.x, x3.x );
    vec4 v1234_y = vec4( x0.y, x1.y, x2.y, x3.y );
    vec4 v1234_z = vec4( x0.z, x1.z, x2.z, x3.z );

    // clamp the domain of our grid cell
    Pi.xyz = Pi.xyz - floor(Pi.xyz * ( 1.0 / 69.0 )) * 69.0;
    vec3 Pi_inc1 = step( Pi, vec3( 69.0 - 1.5 ) ) * ( Pi + 1.0 );

    //  generate the random vectors
    vec4 Pt = vec4( Pi.xy, Pi_inc1.xy ) + vec2( 50.0, 161.0 ).xyxy;
    Pt *= Pt;
    vec4 V1xy_V2xy = mix( Pt.xyxy, Pt.zwzw, vec4( Pi_1.xy, Pi_2.xy ) );
    Pt = vec4( Pt.x, V1xy_V2xy.xz, Pt.z ) * vec4( Pt.y, V1xy_V2xy.yw, Pt.w );
    const vec3 SOMELARGEFLOATS = vec3( 635.298681, 682.357502, 668.926525 );
    const vec3 ZINC = vec3( 48.500388, 65.294118, 63.934599 );
    vec3 lowz_mods = vec3( 1.0 / ( SOMELARGEFLOATS.xyz + Pi.zzz * ZINC.xyz ) );
    vec3 highz_mods = vec3( 1.0 / ( SOMELARGEFLOATS.xyz + Pi_inc1.zzz * ZINC.xyz ) );
    Pi_1 = ( Pi_1.z < 0.5 ) ? lowz_mods : highz_mods;
    Pi_2 = ( Pi_2.z < 0.5 ) ? lowz_mods : highz_mods;
    vec4 hash_0 = fract( Pt * vec4( lowz_mods.x, Pi_1.x, Pi_2.x, highz_mods.x ) ) - 0.49999;
    vec4 hash_1 = fract( Pt * vec4( lowz_mods.y, Pi_1.y, Pi_2.y, highz_mods.y ) ) - 0.49999;
    vec4 hash_2 = fract( Pt * vec4( lowz_mods.z, Pi_1.z, Pi_2.z, highz_mods.z ) ) - 0.49999;

    //  normalize random gradient vectors
    vec4 norm = inversesqrt( hash_0 * hash_0 + hash_1 * hash_1 + hash_2 * hash_2 );
    hash_0 *= norm;
    hash_1 *= norm;
    hash_2 *= norm;

    //  evaluate gradients
    vec4 grad_results = hash_0 * v1234_x + hash_1 * v1234_y + hash_2 * v1234_z;

    //  evaulate the kernel weights ( use (0.5-x*x)^3 instead of (0.6-x*x)^4 to fix discontinuities )
    vec4 m = v1234_x * v1234_x + v1234_y * v1234_y + v1234_z * v1234_z;
    m = max(0.5 - m, 0.0);
    vec4 m2 = m*m;
    vec4 m3 = m*m2;

    //  calc the derivatives
    vec4 temp = -6.0 * m2 * grad_results;
    float xderiv = dot( temp, v1234_x ) + dot( m3, hash_0 );
    float yderiv = dot( temp, v1234_y ) + dot( m3, hash_1 );
    float zderiv = dot( temp, v1234_z ) + dot( m3, hash_2 );

    //  Normalization factor to scale the final result to a strict 1.0->-1.0 range
    //  http://briansharpe.wordpress.com/2012/01/13/simplex-noise/#comment-36
    const float FINAL_NORMALIZATION = 37.837227241611314102871574478976;

    //  sum and return all results as a vec3
    return vec4( dot( m3, grad_results ), xderiv, yderiv, zderiv ) * FINAL_NORMALIZATION;
}

//
//	Quintic Hermite Interpolation
//	http://www.rose-hulman.edu/~finn/CCLI/Notes/day09.pdf
//
//  NOTE: maximum value of a hermitequintic interpolation with zero acceleration at the endpoints would be...
//        f(x=0.5) = MAXPOS + MAXVELOCITY * ( ( x - 6x^3 + 8x^4 - 3x^5 ) - ( -4x^3 + 7x^4 -3x^5 ) ) = MAXPOS + MAXVELOCITY * 0.3125
//
//	variable naming conventions:
//	val = value ( position )
//	grad = gradient ( velocity )
//	x = 0.0->1.0 ( time )
//	i = interpolation = a value to be interpolated
//	e = evaluation = a value to be used to calculate the interpolation
//	0 = start
//	1 = end
//
float QuinticHermite( float x, float ival0, float ival1, float egrad0, float egrad1 )		// quintic hermite with start/end acceleration of 0.0
{
    const vec3 C0 = vec3( -15.0, 8.0, 7.0 );
    const vec3 C1 = vec3( 6.0, -3.0, -3.0 );
    const vec3 C2 = vec3( 10.0, -6.0, -4.0 );
    vec3 h123 = ( ( ( C0 + C1 * x ) * x ) + C2 ) * ( x*x*x );
    return ival0 + dot( vec3( (ival1 - ival0), egrad0, egrad1 ), h123.xyz + vec3( 0.0, x, 0.0 ) );
}
vec4 QuinticHermite( float x, vec4 ival0, vec4 ival1, vec4 egrad0, vec4 egrad1 )		// quintic hermite with start/end acceleration of 0.0
{
    const vec3 C0 = vec3( -15.0, 8.0, 7.0 );
    const vec3 C1 = vec3( 6.0, -3.0, -3.0 );
    const vec3 C2 = vec3( 10.0, -6.0, -4.0 );
    vec3 h123 = ( ( ( C0 + C1 * x ) * x ) + C2 ) * ( x*x*x );
    return ival0 + (ival1 - ival0) * h123.xxxx + egrad0 * vec4( h123.y + x ) + egrad1 * h123.zzzz;
}
vec4 QuinticHermite( float x, vec2 igrad0, vec2 igrad1, vec2 egrad0, vec2 egrad1 )		// quintic hermite with start/end position and acceleration of 0.0
{
    const vec3 C0 = vec3( -15.0, 8.0, 7.0 );
    const vec3 C1 = vec3( 6.0, -3.0, -3.0 );
    const vec3 C2 = vec3( 10.0, -6.0, -4.0 );
    vec3 h123 = ( ( ( C0 + C1 * x ) * x ) + C2 ) * ( x*x*x );
    return vec4( egrad1, igrad0 ) * vec4( h123.zz, 1.0, 1.0 ) + vec4( egrad0, h123.xx ) * vec4( vec2( h123.y + x ), (igrad1 - igrad0) );	//	returns vec4( out_ival.xy, out_igrad.xy )
}
void QuinticHermite( 	float x,
                        vec4 ival0, vec4 ival1,			//	values are interpolated using the gradient arguments
                        vec4 igrad_x0, vec4 igrad_x1, 	//	gradients are interpolated using eval gradients of 0.0
                        vec4 igrad_y0, vec4 igrad_y1,
                        vec4 egrad0, vec4 egrad1, 		//	our evaluation gradients
                        out vec4 out_ival, out vec4 out_igrad_x, out vec4 out_igrad_y )	// quintic hermite with start/end acceleration of 0.0
{
    const vec3 C0 = vec3( -15.0, 8.0, 7.0 );
    const vec3 C1 = vec3( 6.0, -3.0, -3.0 );
    const vec3 C2 = vec3( 10.0, -6.0, -4.0 );
    vec3 h123 = ( ( ( C0 + C1 * x ) * x ) + C2 ) * ( x*x*x );
    out_ival = ival0 + (ival1 - ival0) * h123.xxxx + egrad0 * vec4( h123.y + x ) + egrad1 * h123.zzzz;
    out_igrad_x = igrad_x0 + (igrad_x1 - igrad_x0) * h123.xxxx;	//	NOTE: gradients of 0.0
    out_igrad_y = igrad_y0 + (igrad_y1 - igrad_y0) * h123.xxxx;	//	NOTE: gradients of 0.0
}
void QuinticHermite( 	float x,
                        vec4 igrad_x0, vec4 igrad_x1, 	//	gradients are interpolated using eval gradients of 0.0
                        vec4 igrad_y0, vec4 igrad_y1,
                        vec4 egrad0, vec4 egrad1, 		//	our evaluation gradients
                        out vec4 out_ival, out vec4 out_igrad_x, out vec4 out_igrad_y )	// quintic hermite with start/end position and acceleration of 0.0
{
    const vec3 C0 = vec3( -15.0, 8.0, 7.0 );
    const vec3 C1 = vec3( 6.0, -3.0, -3.0 );
    const vec3 C2 = vec3( 10.0, -6.0, -4.0 );
    vec3 h123 = ( ( ( C0 + C1 * x ) * x ) + C2 ) * ( x*x*x );
    out_ival = egrad0 * vec4( h123.y + x ) + egrad1 * h123.zzzz;
    out_igrad_x = igrad_x0 + (igrad_x1 - igrad_x0) * h123.xxxx;	//	NOTE: gradients of 0.0
    out_igrad_y = igrad_y0 + (igrad_y1 - igrad_y0) * h123.xxxx;	//	NOTE: gradients of 0.0
}
float QuinticHermiteDeriv( float x, float ival0, float ival1, float egrad0, float egrad1 )	// gives the derivative of quintic hermite with start/end acceleration of 0.0
{
    const vec3 C0 = vec3( 30.0, -15.0, -15.0 );
    const vec3 C1 = vec3( -60.0, 32.0, 28.0 );
    const vec3 C2 = vec3( 30.0, -18.0, -12.0 );
    vec3 h123 = ( ( ( C1 + C0 * x ) * x ) + C2 ) * ( x*x );
    return dot( vec3( (ival1 - ival0), egrad0, egrad1 ), h123.xyz + vec3( 0.0, 1.0, 0.0 ) );
}

//
//	FAST32_hash
//	A very fast hashing function.  Requires 32bit support.
//	http://briansharpe.wordpress.com/2011/11/15/a-fast-and-simple-32bit-floating-point-hash-function/
//
//	The 2D hash formula takes the form....
//	hash = mod( coord.x * coord.x * coord.y * coord.y, SOMELARGEFLOAT ) / SOMELARGEFLOAT
//	We truncate and offset the domain to the most interesting part of the noise.
//	SOMELARGEFLOAT should be in the range of 400.0->1000.0 and needs to be hand picked.  Only some give good results.
//	A 3D hash is achieved by offsetting the SOMELARGEFLOAT value by the Z coordinate
//

void FAST32_hash_2D( vec2 gridcell, out vec4 hash_0, out vec4 hash_1 )	//	generates 2 random numbers for each of the 4 cell corners
{
    //    gridcell is assumed to be an integer coordinate
    const vec2 OFFSET = vec2( 26.0, 161.0 );
    const float DOMAIN = 71.0;
    const vec2 SOMELARGEFLOATS = vec2( 951.135664, 642.949883 );
    vec4 P = vec4( gridcell.xy, gridcell.xy + 1.0 );
    P = P - floor(P * ( 1.0 / DOMAIN )) * DOMAIN;
    P += OFFSET.xyxy;
    P *= P;
    P = P.xzxz * P.yyww;
    hash_0 = fract( P * ( 1.0 / SOMELARGEFLOATS.x ) );
    hash_1 = fract( P * ( 1.0 / SOMELARGEFLOATS.y ) );
}

//
//	Hermite2D_Deriv
//	Hermite2D noise with derivatives
//	returns vec3( value, xderiv, yderiv )
//
vec3 Hermite2D_Deriv( vec2 P )
{
    //	establish our grid cell and unit position
    vec2 Pi = floor(P);
    vec2 Pf = P - Pi;

    //	calculate the hash.
    //	( various hashing methods listed in order of speed )
    vec4 hash_gradx, hash_grady;
    FAST32_hash_2D( Pi, hash_gradx, hash_grady );

    //	scale the hash values
    hash_gradx = ( hash_gradx - 0.49999);
    hash_grady = ( hash_grady - 0.49999);

#if 1
    //	normalize gradients
    vec4 norm = inversesqrt( hash_gradx * hash_gradx + hash_grady * hash_grady );
    hash_gradx *= norm;
    hash_grady *= norm;
    const float FINAL_NORM_VAL = 2.2627416997969520780827019587355;
#else
    //	unnormalized gradients
    const float FINAL_NORM_VAL = 3.2;  // 3.2 = 1.0 / ( 0.5 * 0.3125 * 2.0 )
#endif

    //
    //	NOTE:  This stuff can be optimized further.
    //	But it also appears the compiler is doing a lot of that automatically for us anyway
    //

    vec4 qh_results_x = QuinticHermite( Pf.y, hash_gradx.xy, hash_gradx.zw, hash_grady.xy, hash_grady.zw );
    vec4 qh_results_y = QuinticHermite( Pf.x, hash_grady.xz, hash_grady.yw, hash_gradx.xz, hash_gradx.yw );
    float finalpos = QuinticHermite( Pf.x, qh_results_x.x, qh_results_x.y, qh_results_x.z, qh_results_x.w );
    float deriv_x = QuinticHermiteDeriv( Pf.x, qh_results_x.x, qh_results_x.y, qh_results_x.z, qh_results_x.w );
    float deriv_y = QuinticHermiteDeriv( Pf.y, qh_results_y.x, qh_results_y.y, qh_results_y.z, qh_results_y.w );
    return vec3( finalpos, deriv_x, deriv_y ) * FINAL_NORM_VAL;
}