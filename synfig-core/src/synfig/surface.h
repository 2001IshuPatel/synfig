/* === S Y N F I G ========================================================= */
/*!	\file surface.h
**	\brief Surface and Pen Definitions
**
**	$Id$
**
**	\legal
**	Copyright (c) 2002-2005 Robert B. Quattlebaum Jr., Adrian Bentley
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

#ifndef __SYNFIG_SURFACE_H
#define __SYNFIG_SURFACE_H

/* === H E A D E R S ======================================================= */

#include "color.h"
#include "renddesc.h"
#include "target.h"
#include <ETL/pen>
#include <ETL/surface>
#include <ETL/handle>

/* === M A C R O S ========================================================= */

/* === T Y P E D E F S ===================================================== */

/* === C L A S S E S & S T R U C T S ======================================= */

namespace synfig {

class Target;
class Target_Scanline;

class ColorPrep
{
public:
	ColorAccumulator cook(Color x)const
	{
		x.set_r(x.get_r()*x.get_a());
		x.set_g(x.get_g()*x.get_a());
		x.set_b(x.get_b()*x.get_a());
		return x;
	}
	Color uncook(ColorAccumulator x)const
	{
		if(!x.get_a())
			return Color::alpha();

		const float a(1.0f/x.get_a());

		x.set_r(x.get_r()*a);
		x.set_g(x.get_g()*a);
		x.set_b(x.get_b()*a);
		return x;
	}
};

class CairoColorPrep
{
public:
	CairoColor cook(CairoColor x)const
	{
		return x.premult_alpha();
	}
	CairoColor uncook(CairoColor x)const
	{
		return x.demult_alpha();
	}
};

/*!	\class Surface
**	\brief Bitmap Surface
**	\todo writeme
*/
class Surface : public etl::surface<Color, ColorAccumulator, ColorPrep>
{
private: 
	RenderMethod method_;
public:
	typedef Color value_type;
	class alpha_pen;

	Surface():method_(SOFTWARE) { }

	Surface(const size_type::value_type &w, const size_type::value_type &h, RenderMethod m=SOFTWARE):
		etl::surface<Color, ColorAccumulator,ColorPrep>(w,h), method_(m) { }

	Surface(const size_type &s, RenderMethod m=SOFTWARE):
		etl::surface<Color, ColorAccumulator,ColorPrep>(s), method_(m) { }

	template <typename _pen>
	Surface(const _pen &_begin, const _pen &_end, RenderMethod m=SOFTWARE):
		etl::surface<Color, ColorAccumulator,ColorPrep>(_begin,_end), method_(m) { }

	template <class _pen> void blit_to(_pen &pen)
	{ return blit_to(pen,0,0, get_w(),get_h()); }

	template <class _pen> void
	blit_to(_pen& DEST_PEN,	int x, int y, int w, int h)
	{
		etl::surface<Color, ColorAccumulator, ColorPrep>::blit_to(DEST_PEN,x,y,w,h);
	}

	void clear();

	void blit_to(alpha_pen& DEST_PEN, int x, int y, int w, int h);

	RenderMethod get_render_method(){ return method_; }
};	// END of class Surface


/*!	\class CairoSurface
 **	\brief Generic Cairo backed surface. It allows to create a image surface
 ** equivalent to the current backend for custom modifications purposes.
 **	\todo writeme
 */
class CairoSurface : public etl::surface<CairoColor, CairoColor, CairoColorPrep>
{
public:
	typedef CairoColor value_type;
	class alpha_pen;
	
	CairoSurface() { }
		
	void clear();
	
	void set_wh(int w, int h, int pitch=0);
	
};	// END of class Surface


#ifndef DOXYGEN_SKIP

/*! \internal Used by Pen_Alpha */
template <class C, typename A=Color::value_type>
struct _BlendFunc
{
	Color::BlendMethod blend_method;

	_BlendFunc(typename Color::BlendMethod b= Color::BLEND_COMPOSITE):blend_method(b) { }

	C operator()(const C &a,const C &b,const A &t)const
	{
		return C::blend(b,a,t,blend_method);
	}
};	// END of class _BlendFunc

#endif

/*!	\class Surface::alpha_pen
**	\brief Alpha-Blending Pen
**
**	This pen works like a normal alpha pen, except that it supports
**	a variety of blending methods. Use set_blend_method() to select
**	which blending method you want to use.
**	The default blending method is Color::BLEND_COMPOSITE.
**	\see Color::BlendMethod
*/
class Surface::alpha_pen : public etl::alpha_pen< etl::generic_pen<Color, ColorAccumulator>, Color::value_type, _BlendFunc<Color> >
{
public:
	alpha_pen() { }
	alpha_pen(const etl::alpha_pen< etl::generic_pen<Color, ColorAccumulator>, Color::value_type, _BlendFunc<Color> > &x):
		etl::alpha_pen< etl::generic_pen<Color, ColorAccumulator>, Color::value_type, _BlendFunc<Color> >(x)
	{ }

	alpha_pen(const etl::generic_pen<Color, ColorAccumulator>& pen, const Color::value_type &a = 1, const _BlendFunc<Color> &func = _BlendFunc<Color>()):
		etl::alpha_pen< etl::generic_pen<Color, ColorAccumulator>, Color::value_type, _BlendFunc<Color> >(pen,a,func)
	{ }

	//! Sets the blend method to that described by \a method
	void set_blend_method(Color::BlendMethod method) { affine_func_.blend_method=method; }

	//! Returns the blend method being used for this pen
	Color::BlendMethod get_blend_method()const { return affine_func_.blend_method; }
};	// END of class Surface::alpha_pen



/*!	\class CairoSurface::alpha_pen
 **	\brief Alpha-Blending Pen
 **
 **	This pen works like a normal alpha pen, except that it supports
 **	a variety of blending methods. Use set_blend_method() to select
 **	which blending method you want to use.
 **	The default blending method is Color::BLEND_COMPOSITE.
 **	\see Color::BlendMethod
 */
class CairoSurface::alpha_pen : public etl::alpha_pen< etl::generic_pen<CairoColor, CairoColor>, float, _BlendFunc<CairoColor> >
{
public:
	alpha_pen() { }
	alpha_pen(const etl::alpha_pen< etl::generic_pen<CairoColor, CairoColor>, float, _BlendFunc<CairoColor> > &x):
	etl::alpha_pen< etl::generic_pen<CairoColor, CairoColor>, float, _BlendFunc<CairoColor> >(x)
	{ }
	
	alpha_pen(const etl::generic_pen<CairoColor, CairoColor>& pen, const float &a = 1, const _BlendFunc<CairoColor> &func = _BlendFunc<CairoColor>()):
	etl::alpha_pen< etl::generic_pen<CairoColor, CairoColor>, float, _BlendFunc<CairoColor> >(pen,a,func)
	{ }
	
	//! Sets the blend method to that described by \a method
	void set_blend_method(Color::BlendMethod method) { affine_func_.blend_method=method; }
	
	//! Returns the blend method being used for this pen
	Color::BlendMethod get_blend_method()const { return affine_func_.blend_method; }
};	// END of class CairoSurface::alpha_pen



//! Creates a target that will render to \a surface
etl::handle<Target_Scanline> surface_target(Surface *surface);

}; // END of namespace synfig

/* === E N D =============================================================== */

#endif
