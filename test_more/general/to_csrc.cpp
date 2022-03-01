/* --------------------------------------------------------------------------
CppAD: C++ Algorithmic Differentiation: Copyright (C) 2003-22 Bradley M. Bell

CppAD is distributed under the terms of the
             Eclipse Public License Version 2.0.

This Source Code may also be made available under the following
Secondary License when the conditions for such availability set forth
in the Eclipse Public License, Version 2.0 are satisfied:
      GNU General Public License, Version 2.0 or later.
---------------------------------------------------------------------------- */
# include <cppad/cppad.hpp>
# include <dlfcn.h>

namespace{ // BEGIN_EMPTY_NAMESPACE

void (*call_test_to_csrc)(
    size_t        call_id,
    size_t        nx,
    const double* x,
    size_t        ny,
    double*       y,
    size_t*       compare_change
);

std::string create_dynamic_library(
    const std::string& library_name, const std::string& library_csrc
)
{   bool ok = true;
    //
    // c_file_name
    std::string c_file_name = "/tmp/" + library_name + ".c";
    //
    // o_file_name
    std::string o_file_name = "/tmp/" + library_name + ".o";
    //
    // so_file_name
    std::string so_file_name = "/tmp/" + library_name + ".so";
    //
    // library_name.c
    {   std::ofstream file;
        file.open(c_file_name, std::ios::out);
        file << library_csrc;
        file.close();
    }
    //
    // library_name.so
    int flag = std::system(nullptr);
    ok      &= flag != 0;
    if( flag != 0 )
    {   std::string command =
            "gcc -c -g -fPIC " + c_file_name + " -o " + o_file_name;
        flag = std::system( command.c_str() );
        ok  &= flag == 0;
        if( flag == 0 )
        {   command =
                "gcc -shared " + o_file_name + " -o " + so_file_name;
            flag = std::system( command.c_str() );
            ok  &= flag == 0;
        }
    }
    if( ok )
        return so_file_name;
    return "";
 }
// --------------------------------------------------------------------------
bool simple_cases(void)
{   // ok
    bool ok = true;
    //
    // AD, NearEqual
    using CppAD::AD;
    using CppAD::NearEqual;
    //
    // eps99
    double eps99 = 99.0 * std::numeric_limits<double>::epsilon();
    //
    // funciton_name
    std::string function_name = "test_to_csrc";
    //
    // nx, ax
    size_t nx = 2;
    CPPAD_TESTVECTOR( AD<double> ) ax(nx);
    ax[0] = 0.5;
    ax[1] = 2.0;
    CppAD::Independent(ax);
    //
    // ny, ay
    size_t ny = 26;
    CPPAD_TESTVECTOR( AD<double> ) ay(ny);
    //
    // binary operators
    ay[0] = ax[0] + ax[1]; // add
    ay[1] = ax[0] / ax[1]; // div
    ay[2] = ax[0] * ax[1]; // mul
    ay[3] = ax[0] - ax[1]; // sub
    //
    // unary functions
    ay[4]  = abs(   ax[0] );
    ay[5]  = acos(  ax[0] ); // ax[0] < 1
    ay[6]  = acosh( ax[1] ); // ax[1] > 1
    ay[7]  = asin(  ax[0] ); // ax[0] < 1
    ay[8]  = asinh( ax[0] );
    ay[9]  = atan(  ax[0] );
    ay[10] = atanh( ax[0] );
    ay[11] = cos(   ax[0] );
    ay[12] = cosh(  ax[0] );
    ay[13] = erf(   ax[0] );
    ay[14] = erfc(  ax[0] );
    ay[15] = exp(   ax[0] );
    ay[16] = expm1( ax[0] );
    ay[17] = log1p( ax[0] );
    ay[18] = log(   ax[0] );
    ay[19] = sin(   ax[0] );
    ay[20] = sinh(  ax[0] );
    ay[21] = sqrt(  ax[0] );
    ay[22] = tan(   ax[0] );
    ay[23] = tanh(  ax[0] );
    //
    // binary functions
    ay[24] = azmul( ax[0], ax[1] );
    ay[25] = pow(   ax[0], ax[1] ); // ax[0] > 0
    //
    // f
    CppAD::ADFun<double> f(ax, ay);
    f.function_name_set(function_name);
    //
    // csrc
    std::string csrc = f.to_csrc();
    //
    // os_file_name
    std::string so_file_name = create_dynamic_library(function_name, csrc);
    //
    if( so_file_name == "" )
        ok = false;
    //
    // handle
    void* handle = dlopen(so_file_name.c_str(), RTLD_LAZY);
    if( handle == nullptr )
    {   char *errstr;
        errstr = dlerror();
        if( errstr != nullptr )
            std::cout << "Dynamic linking error = " << errstr << "\n";
        ok = false;
    }
    else
    {   // call test_to_csrc
        std::string complete_name = "cppad_forward_zero_" + function_name;
        *(void**)(&call_test_to_csrc) = dlsym(handle, complete_name.c_str());
        size_t call_id = 0;
        CppAD::vector<double> x(nx), y(ny);
        x[0] = Value( ax[0] );
        x[1] = Value( ax[1] );
        for(size_t i = 0; i < ny; ++i)
            y[i] = std::numeric_limits<double>::quiet_NaN();
        size_t compare_change = 0;
        call_test_to_csrc(
            call_id, nx, x.data(), ny, y.data(), &compare_change
        );
        //
        // check
        for(size_t i = 0; i < ny; ++i)
        {   // std::cout << "y = " << y[i] << ", ay = " << ay[i] << "\n";
            ok &= CppAD::NearEqual( y[i], Value(ay[i]), eps99, eps99);
        }
    }
    dlclose(handle);
    return ok;
}
// --------------------------------------------------------------------------
bool compare_cases(void)
{   // ok
    bool ok = true;
    //
    // AD, NearEqual
    using CppAD::AD;
    //
    // funciton_name
    std::string function_name = "test_to_csrc";
    //
    // nx, ax
    size_t nx = 1;
    CPPAD_TESTVECTOR( AD<double> ) ax(nx);
    double x0  = 0.5;
    ax[0] = x0;
    CppAD::Independent(ax);
    //
    // ny, ay
    size_t ny = 4;
    CPPAD_TESTVECTOR( AD<double> ) ay(ny);
    //
    // comp_eq_graph_op
    if( ax[0] == x0 )
        ay[0] = 1.0;
    else
        ay[0] = 0.0;
    //
    // comp_le_graph_op
    if( ax[0] <= x0 )
        ay[1] = 1.0;
    else
        ay[1] = 0.0;
    //
    // comp_lt_graph_op
    if( ax[0] <= x0 )
        ay[2] = 1.0;
    else
        ay[2] = 0.0;
    //
    // comp_ne_graph_op
    if( ax[0] != 2.0 * x0 )
        ay[3] = 1.0;
    else
        ay[3] = 0.0;
    //
    // f
    CppAD::ADFun<double> f(ax, ay);
    f.function_name_set(function_name);
    //
    // csrc
    std::string csrc = f.to_csrc();
    //
    // so_file_name
    std::string so_file_name = create_dynamic_library(function_name, csrc);
    //
    if( so_file_name == "" )
        ok = false;
    //
    // handle
    void* handle = dlopen(so_file_name.c_str(), RTLD_LAZY);
    if( handle == nullptr )
    {   char *errstr;
        errstr = dlerror();
        if( errstr != nullptr )
            std::cout << "Dynamic linking error = " << errstr << "\n";
        ok = false;
    }
    else
    {   // call_test_to_csrc
        std::string complete_name = "cppad_forward_zero_" + function_name;
        *(void**)(&call_test_to_csrc) = dlsym(handle, complete_name.c_str());
        //
        // ok
        // no change
        size_t call_id = 0;
        CppAD::vector<double> x(nx), y(ny);
        x[0] = x0;
        for(size_t i = 0; i < ny; ++i)
            y[i] = std::numeric_limits<double>::quiet_NaN();
        size_t compare_change = 0;
        call_test_to_csrc(
            call_id, nx, x.data(), ny, y.data(), &compare_change
        );
        ok &= compare_change == 0;
        for(size_t i = 0; i < ny; ++i)
            ok &= y[i] == Value( ay[i] );
        //
        // ok
        // check all change
        x[0] = 2.0 * x0;
        call_test_to_csrc(
            call_id, nx, x.data(), ny, y.data(), &compare_change
        );
        for(size_t i = 0; i < ny; ++i)
            ok &= y[i] == Value( ay[i] );
        ok &= compare_change == 4;
    }
    dlclose(handle);
    return ok;
}
// ---------------------------------------------------------------------------
} // END_EMPTY_NAMESPACE
// ---------------------------------------------------------------------------
bool to_csrc(void)
{   bool ok = true;
    ok     &= simple_cases();
    ok     &= compare_cases();
    return ok;
}
