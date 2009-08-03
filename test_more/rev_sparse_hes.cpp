/* $Id$ */
/* --------------------------------------------------------------------------
CppAD: C++ Algorithmic Differentiation: Copyright (C) 2003-09 Bradley M. Bell

CppAD is distributed under multiple licenses. This distribution is under
the terms of the 
                    Common Public License Version 1.0.

A copy of this license is included in the COPYING file of this distribution.
Please visit http://www.coin-or.org/CppAD/ for information on other licenses.
-------------------------------------------------------------------------- */


# include <cppad/cppad.hpp>
 
namespace { // Begin empty namespace

bool case_one(void)
{	bool ok = true;
	using namespace CppAD;

	// dimension of the domain space
	size_t n = 10; 

	// dimension of the range space
	size_t m = 2;

	// temporary indices
	size_t i, j;

	// initialize check values to false
	CPPAD_TEST_VECTOR<bool> Check(n * n);
	for(j = 0; j < n * n; j++)
		Check[j] = false;

	// independent variable vector 
	CPPAD_TEST_VECTOR< AD<double> > X(n);
	for(j = 0; j < n; j++)
		X[j] = AD<double>(j);
	Independent(X);

	// accumulate sum here
	AD<double> sum;

	// variable * variable
	sum += X[2] * X[3];
	Check[2 * n + 3] = Check[3 * n + 2] = true;

	// variable / variable
	sum += X[4] / X[5];
	Check[4 * n + 5] = Check[5 * n + 4] = Check[5 * n + 5] = true;

	// CondExpLt(variable, variable, variable, variable)
	sum += CondExpLt(X[1], X[2], sin(X[6]), cos(X[7]) );
	Check[6 * n + 6] = true;
	Check[7 * n + 7] = true;
	
	// pow(variable, variable)
	sum += pow(X[8], X[9]);
	Check[8 * n + 8] = Check[8 * n + 9] = true;
	Check[9 * n + 8] = Check[9 * n + 9] = true;

	// dependent variable vector
	CPPAD_TEST_VECTOR< AD<double> > Y(m);
	Y[0] = sum;

	// variable - variable
	Y[1]  = X[0] - X[1];

	// create function object F : X -> Y
	ADFun<double> F(X, Y);

	// sparsity pattern for the identity function U(x) = x
	CPPAD_TEST_VECTOR<bool> Px(n * n);
	for(i = 0; i < n; i++)
	{	for(j = 0; j < n; j++)
			Px[ i * n + j ] = false;
		Px[ i * n + i ] = true;
	}

	// compute sparsity pattern for Jacobian of F(U(x))
	F.ForSparseJac(n, Px);

	// compute sparsity pattern for Hessian of F_0 ( U(x) ) 
	CPPAD_TEST_VECTOR<bool> Py(m);
	Py[0] = true;
	Py[1] = false;
	CPPAD_TEST_VECTOR<bool> Pxx(n * n);
	Pxx = F.RevSparseHes(n, Py);

	// check values
	for(j = 0; j < n * n; j++)
		ok &= (Pxx[j] == Check[j]);

	// compute sparsity pattern for Hessian of F_1 ( U(x) )
	Py[0] = false;
	Py[1] = true;
	Pxx = F.RevSparseHes(n, Py);
	for(j = 0; j < n * n; j++)
		ok &= (! Pxx[j]);  // Hessian is identically zero

	return ok;
}

bool case_two(void)
{	bool ok = true;
	using namespace CppAD;

	// dimension of the domain space
	size_t n = 4; 

	// dimension of the range space
	size_t m = 1;

	// temporary indices
	size_t i, j;

	// initialize check values to false
	CPPAD_TEST_VECTOR<bool> Check(n * n);
	for(j = 0; j < n * n; j++)
		Check[j] = false;

	// independent variable vector 
	CPPAD_TEST_VECTOR< AD<double> > X(n);
	for(j = 0; j < n; j++)
		X[j] = AD<double>(j);
	Independent(X);

	// Test the case where dependent variable is a non-linear function
	// of the result of a conditional expression. 
	CPPAD_TEST_VECTOR< AD<double> > Y(m);
	Y[0] = CondExpLt(X[0], X[1], X[2], X[3]);
	Y[0] = cos(Y[0]) + X[0] + X[1];

	// Hessian with respect to x[0] and x[1] is zero.
	// Hessian with respect to x[2] and x[3] is full
	// (although we know that there are no cross terms, this is an
	// inefficiency of the conditional expression operator).
	Check[2 * n + 2] = Check[ 2 * n + 3 ] = true;
	Check[3 * n + 2] = Check[ 3 * n + 3 ] = true;
	
	// create function object F : X -> Y
	ADFun<double> F(X, Y);

	// sparsity pattern for the identity function U(x) = x
	CPPAD_TEST_VECTOR<bool> Px(n * n);
	for(i = 0; i < n; i++)
	{	for(j = 0; j < n; j++)
			Px[ i * n + j ] = false;
		Px[ i * n + i ] = true;
	}

	// compute sparsity pattern for Jacobian of F(U(x))
	F.ForSparseJac(n, Px);

	// compute sparsity pattern for Hessian of F_0 ( U(x) ) 
	CPPAD_TEST_VECTOR<bool> Py(m);
	Py[0] = true;
	CPPAD_TEST_VECTOR<bool> Pxx(n * n);
	Pxx = F.RevSparseHes(n, Py);

	// check values
	for(j = 0; j < n * n; j++)
		ok &= (Pxx[j] == Check[j]);

	return ok;
}

bool case_three(void)
{	bool ok = true;
	using CppAD::AD;

	// domain space vector
	size_t n = 1; 
	CPPAD_TEST_VECTOR< AD<double> > X(n);
	X[0] = 0.; 

	// declare independent variables and start recording
	CppAD::Independent(X);

	// range space vector
	size_t m = 1;
	CPPAD_TEST_VECTOR< AD<double> > Y(m);

	// make sure reverse jacobian is propogating dependency to
	// intermediate values (not just final ones).
	Y[0] = X[0] * X[0] + 2;

	// create f: X -> Y and stop tape recording
	CppAD::ADFun<double> f(X, Y);

	// sparsity pattern for the identity matrix
	CppAD::vector<bool> r(n * n);
	size_t i, j;
	for(i = 0; i < n; i++)
	{	for(j = 0; j < n; j++)
			r[ i * n + j ] = false;
		r[ i * n + i ] = true;
	}

	// compute sparsity pattern for J(x) = F^{(1)} (x)
	f.ForSparseJac(n, r);

	// compute sparsity pattern for H(x) = F_0^{(2)} (x)
	CppAD::vector<bool> s(m);
	for(i = 0; i < m; i++)
		s[i] = false;
	s[0] = true;
	CppAD::vector<bool> h(n * n);
	h    = f.RevSparseHes(n, s);

	// check values
	ok  &= (h[ 0 * n + 0 ] == true);  // second partial w.r.t x[0], x[0]

	return ok;
}

} // End of empty namespace

bool rev_sparse_hes(void)
{	bool ok = true;
	ok &= case_one();
	ok &= case_two();
	ok &= case_three();

	return ok;
}
