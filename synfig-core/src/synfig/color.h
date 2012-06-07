/* === S Y N F I G ========================================================= */
/*!	\file color.h
**	\brief Color Class Implementation
**
**	$Id$
**
**	\legal
**	Copyright (c) 2002-2005 Robert B. Quattlebaum Jr., Adrian Bentley
**	Copyright (c) 2007, 2008 Chris Moore
**	Copyright (c) 2012 Diego Barrios Romero
**
**	This package is free software; you can redistribute it and/or
**	modify it under the terms of the GNU General Public License as
**	published by the Free Software Foundation; either version 2 of
**	the License, or (at your option) any later version.
**
**	This package is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
**	General Public License for more details.
**	\endlegal
*/
/* ========================================================================= */

/* === S T A R T =========================================================== */

#ifndef __SYNFIG_COLOR_H
#define __SYNFIG_COLOR_H

/* === H E A D E R S ======================================================= */


#include <math.h>
#include <cassert>
#include "gamma.h"
#include "colorbase.h"
#include <synfig/string.h>
# include "angle.h"

#ifdef USE_HALF_TYPE
#include <OpenEXR/half.h>
#endif

/* === M A C R O S ========================================================= */

#define use_colorspace_gamma()	App::use_colorspace_gamma
#define colorspace_gamma()		(2.2f)
#define gamma_in(x)				((x>=0) ? pow((float)x,1.0f/colorspace_gamma()) : -pow((float)-x,1.0f/colorspace_gamma()))
#define gamma_out(x)			((x>=0) ? pow((float)x,     colorspace_gamma()) : -pow((float)-x,     colorspace_gamma()))

#ifdef WIN32
#include <float.h>
#ifndef isnan
extern "C" { int _isnan(double x); }
#define isnan _isnan
#endif
#endif

// For some reason isnan() isn't working on macosx any more.
// This is a quick fix.
#if defined(__APPLE__) && !defined(SYNFIG_ISNAN_FIX)
#ifdef isnan
#undef isnan
#endif
inline bool isnan(double x) { return x != x; }
inline bool isnan(float x) { return x != x; }
#define SYNFIG_ISNAN_FIX 1
#else
#define isnan std::isnan
#endif

namespace synfig {

#ifdef USE_HALF_TYPE
typedef half ColorReal;
#else
typedef float ColorReal;
#endif

static const float EncodeYUV[3][3]=
{
	{ 0.299f, 0.587f, 0.114f },
	{ -0.168736f, -0.331264f, 0.5f },
	{ 0.5f, -0.418688f, -0.081312f }
};

static const float DecodeYUV[3][3]=
{
	{ 1.0f, 0.0f, 1.402f },
	{ 1.0f, -0.344136f, -0.714136f },
	{ 1.0f, 1.772f, 0.0f }
};

/* === T Y P E D E F S ===================================================== */

/* === C L A S S E S & S T R U C T S ======================================= */

#ifdef USE_HALF_TYPE
class ColorAccumulator;
#endif


class CairoColor;

/*!	\class Color
**	\ ARGB 128 bits Color class implementation 
**	Future optimizations: lookup table for sqrt()?
*/
class Color : public synfig::ColorBase<ColorReal>
{
public:
	typedef ColorReal value_type;

public:
	
	static const value_type ceil()
	{ return (value_type)(1);}
	
	static const value_type floor()
	{ return (value_type)(0);}
	
	const String get_string(void)const;

	Color &
	operator+=(const Color &rhs)
	{
		r_+=rhs.r_;
		g_+=rhs.g_;
		b_+=rhs.b_;
		a_+=rhs.a_;
		return *this;
	}

	Color &
	operator-=(const Color &rhs)
	{
		r_-=rhs.r_;
		g_-=rhs.g_;
		b_-=rhs.b_;
		a_-=rhs.a_;
		return *this;
	}

	Color &
	operator*=(const float &rhs)
	{
		r_*=rhs;
		g_*=rhs;
		b_*=rhs;
		a_*=rhs;
		return *this;
	}

	Color &
	operator/=(const float &rhs)
	{
		const float temp(value_type(1)/rhs);
		r_*=temp;
		g_*=temp;
		b_*=temp;
		a_*=temp;
		return *this;
	}

	Color
	operator+(const Color &rhs)const
	{ return Color(*this)+=rhs; }

	Color
	operator-(const Color &rhs)const
	{ return Color(*this)-=rhs; }

	Color
	operator*(const float &rhs)const
	{ return Color(*this)*=rhs; }

	Color
	operator/(const float &rhs)const
	{ return Color(*this)/=rhs; }

	bool
	operator==(const Color &rhs)const
	{ return r_==rhs.r_ && g_==rhs.g_ && b_==rhs.b_ && a_==rhs.a_; }

	bool
	operator!=(const Color &rhs)const
	{ return r_!=rhs.r_ || g_!=rhs.g_ || b_!=rhs.b_ || a_!=rhs.a_; }

	Color
	operator-()const
	{ return Color(-r_,-g_,-b_,-a_); }

	//! Effectively 1.0-color
	Color
	operator~()const
	{ return Color(1.0f-r_,1.0f-g_,1.0f-b_,a_); }

	bool is_valid()const
	{ return !isnan(r_) && !isnan(g_) && !isnan(b_) && !isnan(a_); }

	Color premult_alpha() const
	{
		return Color (r_*a_, g_*a_, b_*a_, a_);
	}

	Color demult_alpha() const
	{
		if(a_)
		{
			const value_type inva = 1/a_;
			return Color (r_*inva, g_*inva, b_*inva, a_);
		}else return alpha();
	}

public:
	// ETL/trunk/ETL/_gaussian.h does:
	//   SR1=SR2=SR3=typename T::value_type();
	// and expects that to give it initialized colors
	// Otherwise the 'gaussian' blur type is random.
	Color() :ColorBase<value_type>() { }
	Color(const value_type &f) :ColorBase<value_type>(f) { }
	Color(int f) :ColorBase<value_type>(f,f,f,f) { }

	/*!	\param R Red
	**	\param G Green
	**	\param B Blue
	**	\param A Opacity(alpha) */
	Color(const value_type& R, const value_type& G, const value_type& B, const value_type& A=1):
		ColorBase<value_type>(R, G, B, A) { }

	/*!	\param c Source for color components
	**	\param A Opacity(alpha) */
	Color(const Color& c, const value_type& A):
		ColorBase<value_type> (c.r_, c.g_, c.b_, A) { }

	//!	Copy constructor
	Color(const Color& c):
		ColorBase<value_type> (c.r_, c.g_, c.b_, c.a_) { }

	//! Convert from CairoColor to Color
	Color(const CairoColor& c);
	
#ifdef USE_HALF_TYPE
	friend class ColorAccumulator;
	//!	Convert constructor
	Color(const ColorAccumulator& c);
#endif

	//!	Copy constructor
	//Color(const Color &c) { memcpy((void*)this, (const void*)&c, sizeof(Color)); }

	/*const Color &operator=(const value_type &i)
	{
		r_ = g_ = b_ = a_ = i;
		return *this;
	}*/
	//Color& operator=(const Color &c) { memcpy((void*)this, (const void*)&c, sizeof(Color)); return *this; }

	//! Converts a 2 character hex string \a s (00-ff) into a ColorReal (0.0-1.0)
	static ColorReal hex2real(String s);

	//! Converts a ColorReal \a c (0.0-1.0) into a 2 character hex string (00-ff)
	static const String real2hex(ColorReal c);

	//! Returns the color as a 6 character hex sting
	const String get_hex()const { return String(real2hex(r_)+real2hex(g_)+real2hex(b_)); }

	//! Sets the color's R, G, and B from a 3 or 6 character hex string
	void set_hex(String& hex);

	//! Returns color's luminance
	float
	get_y() const
	{
		return
			(float)get_r()*EncodeYUV[0][0]+
			(float)get_g()*EncodeYUV[0][1]+
			(float)get_b()*EncodeYUV[0][2];
	}


	//! Returns U component of chromanance
	float
	get_u() const
	{
		return
			(float)get_r()*EncodeYUV[1][0]+
			(float)get_g()*EncodeYUV[1][1]+
			(float)get_b()*EncodeYUV[1][2];
	}


	//! Returns V component of chromanance
	float
	get_v() const
	{
		return
			(float)get_r()*EncodeYUV[2][0]+
			(float)get_g()*EncodeYUV[2][1]+
			(float)get_b()*EncodeYUV[2][2];
	}

	//! Returns the color's saturation
	/*!	This is is the magnitude of the U and V components.
	**	\see set_s() */
	float
	get_s() const
	{
		const float u(get_u()), v(get_v());
		return sqrt(u*u+v*v);
	}

	//! Sets the luminance (\a y) and chromanance (\a u and \a v)
	Color&
	set_yuv(const float &y, const float &u, const float &v)
	{
		set_r(y*DecodeYUV[0][0]+u*DecodeYUV[0][1]+v*DecodeYUV[0][2]);
		set_g(y*DecodeYUV[1][0]+u*DecodeYUV[1][1]+v*DecodeYUV[1][2]);
		set_b(y*DecodeYUV[2][0]+u*DecodeYUV[2][1]+v*DecodeYUV[2][2]);
		return *this;
	}

	//! Sets color luminance
	Color& set_y(const float &y) { return set_yuv(y,get_u(),get_v()); }

	//! Set U component of chromanance
	Color& set_u(const float &u) { return set_yuv(get_y(),u,get_v()); }

	//! Set V component of chromanance
	Color& set_v(const float &v) { return set_yuv(get_y(),get_u(),v); }

	//! Set the U and V components of chromanance
	Color& set_uv(const float& u, const float& v) { return set_yuv(get_y(),u,v); }

	//! Sets the color's saturation
	/*!	\see get_s() */
	Color&
	set_s(const float &x)
	{
		float u(get_u()), v(get_v());
		const float s(sqrt(u*u+v*v));
		if(s)
		{
			u=(u/s)*x;
			v=(v/s)*x;
			return set_uv(u,v);
		}
		return *this;
	}

	//! YUV Color constructor
	static Color YUV(const float& y, const float& u, const float& v, const value_type& a=1)
	{
		Color c;
		c.set_yuv(y,u,v);
		c.set_a(a);
		return c;
	}

	//! Returns the hue of the chromanance
	/*!	This is the angle of the U and V components.
	**	\see set_hue() */
	Angle
	get_hue() const
		{ return Angle::tan(get_u(),get_v()); }

	//! Synonym for get_hue(). \see get_hue()
	Angle get_uv_angle() const { return get_hue(); }

	//! Sets the color's hue
	/*!	\see get_hue() */
	Color&
	set_hue(const Angle& theta)
	{
		const float s(get_s());
		const float
			u(s*(float)Angle::sin(theta).get()),
			v(s*(float)Angle::cos(theta).get());
		return set_uv(u,v);
	}

	//! Synonym for set_hue(). \see set_hue()
	Color& set_uv_angle(const Angle& theta) { return set_hue(theta); }

	//! Rotates the chromanance vector by amount specified by \a theta
	Color& rotate_uv(const Angle& theta)
	{
		const float	a(Angle::sin(theta).get()),	b(Angle::cos(theta).get());
		const float	u(get_u()),	v(get_v());

		return set_uv(b*u-a*v,a*u+b*v);
	}

	//! Sets the luminance (\a y) and chromanance (\a s and \a theta).
	/*!	\param y Luminance
	**	\param s Saturation
	**	\param theta Hue */
	Color& set_yuv(const float& y, const float& s, const Angle& theta)
	{
		return
			set_yuv(
				y,
				s*(float)Angle::sin(theta).get(),
				s*(float)Angle::cos(theta).get()
			);
	}

	//! YUV color constructor where the chroma is in the saturation/hue form.
	/*!	\param y Luminance
	**	\param s Saturation
	**	\param theta Hue
	**	\param a Opacity (alpha) */
	static Color YUV(const float& y, const float& s, const Angle& theta, const value_type& a=1)
	{
		Color c;
		c.set_yuv(y, s, theta);
		c.set_a(a);
		return c;
	}

	//! Clamps a color so that its values are in range. Ignores attempting to visualize negative colors.
	Color clamped()const;

	//! Clamps a color so that its values are in range.
	Color clamped_negative()const;

	/* Preset Colors */

	//! Preset Color Constructors
	//@{
#ifdef HAS_VIMAGE
	static inline Color alpha() { return Color(0,0,0,0.0000001f); }
#else
	static inline Color alpha() { return Color(0,0,0,0); }
#endif
	static inline Color black() { return Color(0,0,0); }
	static inline Color white() { return Color(1,1,1); }
	static inline Color gray() { return Color(0.5f,0.5f,0.5f); }
	static inline Color magenta() { return Color(1,0,1); }
	static inline Color red() { return Color(1,0,0); }
	static inline Color green() { return Color(0,1,0); }
	static inline Color blue() { return Color(0,0,1); }
	static inline Color cyan() { return Color(0,1,1); }
	static inline Color yellow() { return Color(1,1,0); }
	//@}

	/* Other */
	static Color blend(Color a, Color b,float amount,BlendMethod type=BLEND_COMPOSITE);

	static bool is_onto(BlendMethod x)
	{
		return x==BLEND_BRIGHTEN
			|| x==BLEND_DARKEN
			|| x==BLEND_ADD
			|| x==BLEND_SUBTRACT
			|| x==BLEND_MULTIPLY
			|| x==BLEND_DIVIDE
			|| x==BLEND_COLOR
			|| x==BLEND_HUE
			|| x==BLEND_SATURATION
			|| x==BLEND_LUMINANCE
			|| x==BLEND_ONTO
			|| x==BLEND_STRAIGHT_ONTO
			|| x==BLEND_SCREEN
			|| x==BLEND_OVERLAY
			|| x==BLEND_DIFFERENCE
			|| x==BLEND_HARD_LIGHT
		;
	}

	//! a blending method is considered 'straight' if transparent pixels in the upper layer can affect the result of the blend
	static bool is_straight(BlendMethod x)
	{
		return x==BLEND_STRAIGHT
			|| x==BLEND_STRAIGHT_ONTO
			|| x==BLEND_ALPHA_BRIGHTEN
		;
	}
/*protected:

	value_type& operator[](const int i)
	{
		assert(i>=0);
		assert(i<(signed)(sizeof(Color)/sizeof(value_type)));
		return (&r_)[i];
	}

	const value_type& operator[](const int i)const
	{
		assert(i>=0);
		assert(i<(signed)(sizeof(Color)/sizeof(value_type)));
		return (&r_)[i];
	}
*/
}; // END of class Color

	
/*!  \class CairoColor
** \ ARGB 32 bits Color class implementation
** \ for Cairo Image usage.
** \ Color channels are one byte length (unsigned char)
** \ Operations over color channels
** \ that overflow or underflow the unsigned char value
** \ (get the value out of 0-255) aren't allowed and the
** \ value will remain at 0 or 255. Otherwise, there could
** \ be color cycling what will produce artifacts, since 
** \ values outside 0-255 aren't allowed.
** 
** \ In this class color channels aren't alpha premultiplied
** \ When used on a alpha premultiplied surface the premultiplication
** \ and demultiplication has to be explicitly done by the user before 
** \ and after being used on the surface.
*/
class CairoColor : public synfig::ColorBase<unsigned char>
{
public:
	typedef unsigned char value_type;

	static const value_type ceil()
	{ return (value_type)(255);}
	
	static const value_type floor()
	{ return (value_type)(0);}

	// Operators
	inline value_type ceil_clamp(int x)
	{
		if(x>ceil()) return ceil();
		else return (value_type)(x);
	}
	inline value_type floor_clamp(int x)
	{
		if(x<floor()) return floor();
		else return (value_type)(x);
	}
	inline value_type clamp(int x)
	{
		if(x > ceil()) return ceil();
		else if (x < floor()) return floor();
		else return (value_type)(x);
	}
	inline value_type clamp(float x)
	{
		return clamp((int) (x));
	}

	CairoColor&
	operator+=(const CairoColor &rhs)
	{
		r_=ceil_clamp((int)(r_) + rhs.r_);
		g_=ceil_clamp((int)(g_) + rhs.g_);
		r_=ceil_clamp((int)(b_) + rhs.b_);
		r_=ceil_clamp((int)(a_) + rhs.a_);
		return *this;
	}

	CairoColor&
	operator-=(const CairoColor &rhs)
	{
		r_=floor_clamp((int)(r_) - rhs.r_);
		g_=floor_clamp((int)(g_) - rhs.g_);
		r_=floor_clamp((int)(b_) - rhs.b_);
		r_=floor_clamp((int)(a_) - rhs.a_);
		return *this;
	}
	
	CairoColor &
	operator*=(const float &rhs)
	{
		r_=clamp(r_*rhs);
		g_=clamp(g_*rhs);
		b_=clamp(b_*rhs);
		a_=clamp(a_*rhs);
		return *this;
	}

	CairoColor &
	operator/=(const float &rhs)
	{
		const float temp(1.0f/rhs);
		r_=clamp(r_*temp);
		g_=clamp(g_*temp);
		b_=clamp(b_*temp);
		a_=clamp(a_*temp);
		return *this;
	}

	CairoColor
	operator+(const CairoColor &rhs)const
	{ return CairoColor(*this)+=rhs; }

	CairoColor
	operator-(const CairoColor &rhs)const
	{ return CairoColor(*this)-=rhs; }

	CairoColor
	operator*(const float &rhs)const
	{ return CairoColor(*this)*=rhs; }

	CairoColor
	operator/(const float &rhs)const
	{ return CairoColor(*this)/=rhs; }

	bool
	operator==(const CairoColor &rhs)const
	{ return r_==rhs.r_ && g_==rhs.g_ && b_==rhs.b_ && a_==rhs.a_; }

	bool
	operator!=(const CairoColor &rhs)const
	{ return r_!=rhs.r_ || g_!=rhs.g_ || b_!=rhs.b_ || a_!=rhs.a_; }

// Not suitable for unsigned char
//	CairoColor
//	operator-()const
//	{ return CairoColor(-r_,-g_,-b_,-a_); }

	CairoColor
	operator~()const
	{ return CairoColor(ceil()-r_,ceil()-g_,ceil()-b_,a_); }

	bool is_valid()const
	{ return true; }


	CairoColor premult_alpha() const
	{
		return CairoColor (r_*a_, g_*a_, b_*a_, a_);
	}

	CairoColor demult_alpha() const
	{
		if(a_)
		{
			const float inva = 1.0f/a_;
			return CairoColor (r_*inva, g_*inva, b_*inva, a_);
		}else return alpha();
	}

	// Constructors
public:
	CairoColor() : ColorBase<value_type>() { }
	CairoColor(const value_type u) : ColorBase<value_type>(u) { }
	//CairoColor(int f) :a_(f),r_(f), g_(f), b_(f) { }
	CairoColor(const value_type R, const value_type G, const value_type B, const value_type A=ceil()):
		ColorBase<value_type>(R, G, B, A) { }
	CairoColor(const CairoColor& c, const value_type A):
		ColorBase<value_type>(c.r_, c.g_, c.b_, A) { }
	CairoColor(const CairoColor& c) : ColorBase<value_type>(c.r_, c.g_, c.b_, c.a_) { }
	// Conversor constructor
	CairoColor(const Color& c) : ColorBase<value_type>()
	{
		set_r((ceil()-floor())*c.get_r()/(Color::ceil()-Color::floor()));
		set_g((ceil()-floor())*c.get_g()/(Color::ceil()-Color::floor()));
		set_b((ceil()-floor())*c.get_b()/(Color::ceil()-Color::floor()));
		set_a((ceil()-floor())*c.get_a()/(Color::ceil()-Color::floor()));
	}
	
	const String get_string(void)const;

	static const String char2hex(value_type c);
	static value_type hex2char(String s);
	
	void set_hex( String& str);
	const String get_hex()const { return String(char2hex(r_)+char2hex(g_)+char2hex(b_)); }

	float
	get_y() const
	{
		return
		(float)get_r()*EncodeYUV[0][0]+
		(float)get_g()*EncodeYUV[0][1]+
		(float)get_b()*EncodeYUV[0][2];
	}

	float
	get_u() const
	{
		return
		(float)get_r()*EncodeYUV[1][0]+
		(float)get_g()*EncodeYUV[1][1]+
		(float)get_b()*EncodeYUV[1][2];
	}

	float
	get_v() const
	{
		return
		(float)get_r()*EncodeYUV[2][0]+
		(float)get_g()*EncodeYUV[2][1]+
		(float)get_b()*EncodeYUV[2][2];
	}

	float
	get_s() const
	{
		const float u(get_u()), v(get_v());
		return sqrt(u*u+v*v);
	}
	
	CairoColor&
	set_yuv(const float &y, const float &u, const float &v)
	{
		set_r(y*DecodeYUV[0][0]+u*DecodeYUV[0][1]+v*DecodeYUV[0][2]);
		set_g(y*DecodeYUV[1][0]+u*DecodeYUV[1][1]+v*DecodeYUV[1][2]);
		set_b(y*DecodeYUV[2][0]+u*DecodeYUV[2][1]+v*DecodeYUV[2][2]);
		return *this;
	}
	
	CairoColor& set_y(const float &y) { return set_yuv(y,get_u(),get_v()); }
	
	CairoColor& set_u(const float &u) { return set_yuv(get_y(),u,get_v()); }
	
	CairoColor& set_v(const float &v) { return set_yuv(get_y(),get_u(),v); }
	
	CairoColor& set_uv(const float& u, const float& v) { return set_yuv(get_y(),u,v); }
	
	CairoColor&	set_s(const float &x)
	{
		float u(get_u()), v(get_v());
		const float s(sqrt(u*u+v*v));
		if(s)
		{
			u=(u/s)*x;
			v=(v/s)*x;
			return set_uv(u,v);
		}
		return *this;
	}

	static CairoColor YUV(const float& y, const float& u, const float& v, const value_type a=ceil())
	{
		CairoColor c;
		c.set_yuv(y,u,v);
		c.set_a(a);
		return c;
	}
	
	Angle get_hue() const	{ return Angle::tan(get_u(),get_v()); }
	
	Angle get_uv_angle() const { return get_hue(); }
	
	CairoColor& set_hue(const Angle& theta)
	{
		const float s(get_s());
		const float
		u(s*(float)Angle::sin(theta).get()),
		v(s*(float)Angle::cos(theta).get());
		return set_uv(u,v);
	}
	
	CairoColor& set_uv_angle(const Angle& theta) { return set_hue(theta); }
	
	CairoColor& rotate_uv(const Angle& theta)
	{
		const float	a(Angle::sin(theta).get()),	b(Angle::cos(theta).get());
		const float	u(get_u()),	v(get_v());
		return set_uv(b*u-a*v,a*u+b*v);
	}

	CairoColor& set_yuv(const float& y, const float& s, const Angle& theta)
	{
		return
		set_yuv(
				y,
				s*(float)Angle::sin(theta).get(),
				s*(float)Angle::cos(theta).get()
				);
	}
	
	static CairoColor YUV(const float& y, const float& s, const Angle& theta, const value_type a=ceil())
	{
		CairoColor c;
		c.set_yuv(y,s,theta);
		c.set_a(a);
		return c;
	}

	static inline CairoColor alpha() { return CairoColor(floor(),floor(),floor(),floor()); }
	static inline CairoColor black() { return CairoColor(floor(),floor(),floor()); }
	static inline CairoColor white() { return CairoColor(ceil(),ceil(),ceil()); }
	static inline CairoColor gray() { return CairoColor(ceil()/2,ceil()/2,ceil()/2); }
	static inline CairoColor magenta() { return CairoColor(ceil(),floor(),ceil()); }
	static inline CairoColor red() { return CairoColor(ceil(),floor(), floor()); }
	static inline CairoColor green() { return CairoColor(floor(), ceil(),floor()); }
	static inline CairoColor blue() { return CairoColor(floor(),floor(),ceil()); }
	static inline CairoColor cyan() { return CairoColor(floor(),ceil(),ceil()); }
	static inline CairoColor yellow() { return CairoColor(ceil(),ceil(),floor()); }

	// Use Color::BlenMethods for the enum value
	static CairoColor blend(CairoColor a, CairoColor b, float amount, Color::BlendMethod type=Color::BLEND_COMPOSITE);

	static bool is_onto(Color::BlendMethod x)
	{
		return Color::is_onto(x);
	}
	
	static bool is_straight(Color::BlendMethod x)
	{
		return Color::is_straight(x);
	}
	
}; // End of CairoColor class


#ifndef USE_HALF_TYPE
typedef Color ColorAccumulator;
#else
class ColorAccumulator
{
	friend class Color;
public:
	typedef float value_type;

private:
	value_type a_, r_, g_, b_;

public:

	ColorAccumulator &
	operator+=(const ColorAccumulator &rhs)
	{
		r_+=rhs.r_;
		g_+=rhs.g_;
		b_+=rhs.b_;
		a_+=rhs.a_;
		return *this;
	}

	ColorAccumulator &
	operator-=(const ColorAccumulator &rhs)
	{
		r_-=rhs.r_;
		g_-=rhs.g_;
		b_-=rhs.b_;
		a_-=rhs.a_;
		return *this;
	}

	ColorAccumulator &
	operator*=(const float &rhs)
	{
		r_*=rhs;
		g_*=rhs;
		b_*=rhs;
		a_*=rhs;
		return *this;
	}

	ColorAccumulator &
	operator/=(const float &rhs)
	{
		const float temp(value_type(1)/rhs);
		r_*=temp;
		g_*=temp;
		b_*=temp;
		a_*=temp;
		return *this;
	}

	ColorAccumulator
	operator+(const ColorAccumulator &rhs)const
	{ return Color(*this)+=rhs; }

	ColorAccumulator
	operator-(const ColorAccumulator &rhs)const
	{ return Color(*this)-=rhs; }

	ColorAccumulator
	operator*(const float &rhs)const
	{ return Color(*this)*=rhs; }

	ColorAccumulator
	operator/(const float &rhs)const
	{ return Color(*this)/=rhs; }

	bool
	operator==(const ColorAccumulator &rhs)const
	{ return r_==rhs.r_ && g_==rhs.g_ && b_==rhs.b_ && a_!=rhs.a_; }

	bool
	operator!=(const ColorAccumulator &rhs)const
	{ return r_!=rhs.r_ || g_!=rhs.g_ || b_!=rhs.b_ || a_!=rhs.a_; }

	Color
	operator-()const
	{ return ColorAccumulator(-r_,-g_,-b_,-a_); }

	bool is_valid()const
	{ return !isnan(r_) && !isnan(g_) && !isnan(b_) && !isnan(a_); }

public:
	ColorAccumulator() { }

	/*!	\param R Red
	**	\param G Green
	**	\param B Blue
	**	\param A Opacity(alpha) */
	ColorAccumulator(const value_type& R, const value_type& G, const value_type& B, const value_type& A=1):
		a_(A),
		r_(R),
		g_(G),
		b_(B) { }

	//!	Copy constructor
	ColorAccumulator(const ColorAccumulator& c):
		a_(c.a_),
		r_(c.r_),
		g_(c.g_),
		b_(c.b_) { }

	//!	Converter
	ColorAccumulator(const Color& c):
		a_(c.a_),
		r_(c.r_),
		g_(c.g_),
		b_(c.b_) { }

	//! Converter
	ColorAccumulator(int c): a_(c),r_(c), g_(c), b_(c) { }

	//! Returns the RED component
	const value_type& get_r()const { return r_; }

	//! Returns the GREEN component
	const value_type& get_g()const { return g_; }

	//! Returns the BLUE component
	const value_type& get_b()const { return b_; }

	//! Returns the amount of opacity (alpha)
	const value_type& get_a()const { return a_; }

	//! Synonym for get_a(). \see get_a()
	const value_type& get_alpha()const { return get_a(); }

	//! Sets the RED component to \a x
	ColorAccumulator& set_r(const value_type& x) { r_ = x; return *this; }

	//! Sets the GREEN component to \a x
	ColorAccumulator& set_g(const value_type& x) { g_ = x; return *this; }

	//! Sets the BLUE component to \a x
	ColorAccumulator& set_b(const value_type& x) { b_ = x; return *this; }

	//! Sets the opacity (alpha) to \a x
	ColorAccumulator& set_a(const value_type& x) { a_ = x; return *this; }

	//! Synonym for set_a(). \see set_a()
	ColorAccumulator& set_alpha(const value_type& x) { return set_a(x); }
};

inline
Color::Color(const ColorAccumulator& c):
	a_(c.a_),
	r_(c.r_),
	g_(c.g_),
	b_(c.b_) { }

#endif





enum PixelFormat
{
/* Bit	Descriptions (ON/OFF)
** ----+-------------
** 0	Color Channels (Gray/RGB)
** 1	Alpha Channel (WITH/WITHOUT)
** 2	ZDepth	(WITH/WITHOUT)
** 3	Endian (BGR/RGB)
** 4	Alpha Location (Start/End)
** 5	ZDepth Location (Start/End)
** 6	Alpha/ZDepth Arrangement (ZA,AZ)
** 7	Alpha Range (Inverted,Normal)
** 8	Z Range (Inverted,Normal)
*/
	PF_RGB=0,
	PF_GRAY=(1<<0),			//!< If set, use one grayscale channel. If clear, use three channels for RGB
	PF_A=(1<<1),			//!< If set, include alpha channel
	PF_Z=(1<<2),			//!< If set, include ZDepth channel
	PF_BGR=(1<<3),			//!< If set, reverse the order of the RGB channels
	PF_A_START=(1<<4),		//!< If set, alpha channel is before the color data. If clear, it is after.
	PF_Z_START=(1<<5),		//!< If set, ZDepth channel is before the color data. If clear, it is after.
	PF_ZA=(1<<6),			//!< If set, the ZDepth channel will be in front of the alpha channel. If clear, they are reversed.

	PF_A_INV=(1<<7),		//!< If set, the alpha channel is stored as 1.0-a
	PF_Z_INV=(1<<8),		//!< If set, the ZDepth channel is stored as 1.0-z
	PF_RAW_COLOR=(1<<9)+(1<<1)	//!< If set, the data represents a raw Color data structure, and all other bits are ignored.
};

inline PixelFormat operator|(PixelFormat lhs, PixelFormat rhs)
	{ return static_cast<PixelFormat>((int)lhs|(int)rhs); }

inline PixelFormat operator&(PixelFormat lhs, PixelFormat rhs)
	{ return static_cast<PixelFormat>((int)lhs&(int)rhs); }
#define FLAGS(x,y)		(((x)&(y))==(y))

//! Returns the number of channels that the given PixelFormat calls for
inline int
channels(PixelFormat x)
{
	int chan=0;
	if(FLAGS(x,PF_GRAY))
		++chan;
	else
		chan+=3;
	if(FLAGS(x,PF_A))
		++chan;
	if(FLAGS(x,PF_Z))
		++chan;
	if(FLAGS(x,PF_RAW_COLOR))
		chan=sizeof(Color);

	return chan;
}

inline unsigned char *
Color2PixelFormat(const Color &color, const PixelFormat &pf, unsigned char *out, const Gamma &gamma)
{
	if(FLAGS(pf,PF_RAW_COLOR))
	{
		Color *outcol=reinterpret_cast<Color *>(out);
		*outcol=color;
		out+=sizeof(color);
		return out;
	}

	int alpha=(int)((FLAGS(pf,PF_A_INV)?(-(float)color.get_a()+1):(float)color.get_a())*255);
	if(alpha<0)alpha=0;
	if(alpha>255)alpha=255;

	if(FLAGS(pf,PF_ZA|PF_A_START|PF_Z_START))
	{
		if(FLAGS(pf,PF_Z_START))
			out++;
		if(FLAGS(pf,PF_A_START))
			*out++=static_cast<unsigned char>(alpha);
	}
	else
	{
		if(FLAGS(pf,PF_A_START))
			*out++=static_cast<unsigned char>(alpha);
		if(FLAGS(pf,PF_Z_START))
			out++;
	}

	if(FLAGS(pf,PF_GRAY))
		*out++=static_cast<unsigned char>(gamma.g_F32_to_U8(color.get_y()));
	else
	{
		if(FLAGS(pf,PF_BGR))
		{
			*out++=static_cast<unsigned char>(gamma.r_F32_to_U8(color.get_b()));
			*out++=static_cast<unsigned char>(gamma.g_F32_to_U8(color.get_g()));
			*out++=static_cast<unsigned char>(gamma.b_F32_to_U8(color.get_r()));
		}
		else
		{
			*out++=static_cast<unsigned char>(gamma.r_F32_to_U8(color.get_r()));
			*out++=static_cast<unsigned char>(gamma.g_F32_to_U8(color.get_g()));
			*out++=static_cast<unsigned char>(gamma.b_F32_to_U8(color.get_b()));
		}
	}

	if(FLAGS(pf,PF_ZA))
	{
		if(!FLAGS(pf,PF_Z_START) && FLAGS(pf,PF_Z))
			out++;
		if(!FLAGS(pf,PF_A_START) && FLAGS(pf,PF_A))
			*out++=static_cast<unsigned char>(alpha);
	}
	else
	{
		if(!FLAGS(pf,PF_Z_START) && FLAGS(pf,PF_Z))
			out++;
		if(!FLAGS(pf,PF_A_START) && FLAGS(pf,PF_A))
			*out++=static_cast<unsigned char>(alpha);
	}
	return out;
}

inline void
convert_color_format(unsigned char *dest, const Color *src, int w, PixelFormat pf,const Gamma &gamma)
{
	assert(w>=0);
	while(w--)
		dest=Color2PixelFormat((*(src++)).clamped(),pf,dest,gamma);
}

inline const unsigned char *
PixelFormat2Color(Color &color, const PixelFormat &pf,const unsigned char *out)
{
	if(FLAGS(pf,PF_ZA|PF_A_START|PF_Z_START))
	{
		if(FLAGS(pf,PF_Z_START))
			out++;
		if(FLAGS(pf,PF_A_START))
			color.set_a((float)*out++/255);
	}
	else
	{
		if(FLAGS(pf,PF_A_START))
			color.set_a((float)*out++/255);
		if(FLAGS(pf,PF_Z_START))
			out++;
	}

	if(FLAGS(pf,PF_GRAY))
		color.set_yuv((float)*out++/255,0,0);
	else
	{
		if(FLAGS(pf,PF_BGR))
		{
			color.set_b((float)*out++/255);
			color.set_g((float)*out++/255);
			color.set_r((float)*out++/255);
		}
		else
		{
			color.set_r((float)*out++/255);
			color.set_g((float)*out++/255);
			color.set_b((float)*out++/255);
		}
	}

	if(FLAGS(pf,PF_ZA))
	{
		if(!FLAGS(pf,PF_Z_START) && FLAGS(pf,PF_Z))
			out++;
		if(!FLAGS(pf,PF_A_START) && FLAGS(pf,PF_A))
			color.set_a((float)*out++/255);
	}
	else
	{
		if(!FLAGS(pf,PF_A_START) && FLAGS(pf,PF_A))
			color.set_a((float)*out++/255);
		if(!FLAGS(pf,PF_Z_START) && FLAGS(pf,PF_Z))
			out++;
	}
	return out;
}



}; // END of namespace synfig

/* === E N D =============================================================== */

#endif
