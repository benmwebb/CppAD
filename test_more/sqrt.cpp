/* --------------------------------------------------------------------------
CppAD: C++ Algorithmic Differentiation: Copyright (C) 2003-06 Bradley M. Bell

CppAD is distributed under multiple licenses. This distribution is under
the terms of the 
                    Common Public License Version 1.0.

A copy of this license is included in the COPYING file of this distribution.
Please visit http://www.coin-or.org/CppAD/ for information on other licenses.
-------------------------------------------------------------------------- */

/*
Two old sqrt examples now used just for validation testing.
*/
# include <CppAD/cppad.hpp>
# include <cmath>

namespace { // BEGIN empty namespace

bool SqrtTestOne(void)
{	bool ok = true;

	using CppAD::sqrt;
	using CppAD::pow;
	using namespace CppAD;

	// independent variable vector, indices, values, and declaration
	CppADvector< AD<double> > U(1);
	size_t s = 0;
	U[s]     = 4.;
	Independent(U);

	// dependent variable vector, indices, and values
	CppADvector< AD<double> > Z(2);
	size_t x = 0;
	size_t y = 1;
	Z[x]     = sqrt(U[s]);
	Z[y]     = sqrt(Z[x]);

	// define f : U -> Z and vectors for derivative calculations
	ADFun<double> f(U, Z);
	CppADvector<double> v( f.Domain() );
	CppADvector<double> w( f.Range() );

	// check values
	ok &= NearEqual(Z[x] , 2.,        1e-10 , 1e-10);
	ok &= NearEqual(Z[y] , sqrt(2.),  1e-10 , 1e-10);

	// forward computation of partials w.r.t. s
	v[s] = 1.;
	w    = f.Forward(1, v);
	ok &= NearEqual(w[x], .5  * pow(4., -.5),   1e-10 , 1e-10); // dx/ds
	ok &= NearEqual(w[y], .25 * pow(4., -.75),  1e-10 , 1e-10); // dy/ds

	// reverse computation of partials of y
	w[x] = 0.;
	w[y] = 1.;
	v    = f.Reverse(1,w);
	ok &= NearEqual(v[s], .25 * pow(4., -.75),  1e-10 , 1e-10); // dy/ds

	// forward computation of second partials w.r.t s 
	v[s] = 1.;
	w    = f.Forward(1, v);
	v[s] = 0.;
	w    = f.Forward(2, v);
	ok &= NearEqual(       // d^2 y / (ds ds)
		2. * w[y] , 
		-.75 * .25 * pow(4., -1.75),
		1e-10 ,
		1e-10 
	); 

	// reverse computation of second partials of y
	CppADvector<double> r( f.Domain() * 2 );
	w[x] = 0.;
	w[y] = 1.;
	r    = f.Reverse(2, w);
	ok &= NearEqual(      // d^2 y / (ds ds)
		r[2 * s + 1] , 
		-.75 * .25 * pow(4., -1.75),
		1e-10 ,
		1e-10 
	); 

	return ok;

}
bool SqrtTestTwo(void)
{	bool ok = true;
	using namespace CppAD;

	// independent variable vector
	CppADvector< AD<double> > U(1);
	U[0]     = 2.;
	Independent(U);

	// a temporary values
	AD<double> x = U[0] * U[0]; 

	// dependent variable vector 
	CppADvector< AD<double> > Z(1);
	Z[0] =  sqrt( x ); // z = sqrt( u * u )

	// create f: U -> Z and vectors used for derivative calculations
	ADFun<double> f(U, Z); 
	CppADvector<double> v(1);
	CppADvector<double> w(1);

	// check value 
	ok &= NearEqual(U[0] , Z[0],  1e-10 , 1e-10);

	// forward computation of partials w.r.t. u
	size_t j;
	size_t p     = 5;
	double jfac  = 1.;
	double value = 1.;
	v[0]         = 1.;
	for(j = 1; j < p; j++)
	{	jfac *= j;
		w     = f.Forward(j, v);	
		ok &= NearEqual(jfac*w[0], value, 1e-10 , 1e-10); // d^jz/du^j
		v[0]  = 0.;
		value = 0.;
	}

	// reverse computation of partials of Taylor coefficients
	CppADvector<double> r(p); 
	w[0]  = 1.;
	r     = f.Reverse(p, w);
	jfac  = 1.;
	value = 1.;
	for(j = 0; j < p; j++)
	{	ok &= NearEqual(jfac*r[j], value, 1e-10 , 1e-10); // d^jz/du^j
		jfac *= (j + 1);
		value = 0.;
	}

	return ok;
}

} // END empty namespace

bool Sqrt(void)
{	bool ok = true;
	ok &= SqrtTestOne();
	ok &= SqrtTestTwo(); 
	return ok;
}
