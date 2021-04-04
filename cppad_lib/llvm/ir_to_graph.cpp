/* --------------------------------------------------------------------------
CppAD: C++ Algorithmic Differentiation: Copyright (C) 2003-21 Bradley M. Bell

CppAD is distributed under the terms of the
             Eclipse Public License Version 2.0.

This Source Code may also be made available under the following
Secondary License when the conditions for such availability set forth
in the Eclipse Public License, Version 2.0 are satisfied:
      GNU General Public License, Version 2.0 or later.
---------------------------------------------------------------------------- */
/*
---------------------------------------------------------------------------
$begin llvm_ir_to_graph$$
$spell
    llvm_ir
    obj
    vec
    op
    Ptr
    Mul
    Div
$$

$section Convert an LLVM Intermediate Representation to a C++ AD Graph$$

$head Syntax$$
$icode%msg% = %ir_obj%.to_graph(%graph_obj%$$

$head Prototype$$
$srcthisfile%0%// BEGIN_TO_GRAPH%// END_TO_GRAPH%1%$$

$head graph_obj$$
The input value of $icode graph_obj$$ does not matter.
Upon return, it is a $cref cpp_ad_graph$$ representation of the function.

$head ir_obj$$
This is a $cref/llvm_ir/llvm_ir_ctor/$$ object.
It contains an LLVM Intermediate Representation (IR)
of the function that is convert to a C++ AD graph representation.

$subhead Restrictions$$
Only the following $code llvm::Instruction$$ operator codes are supported
so far (more are  expected in the future):

$subhead Arithmetic$$
$code FAdd$$,
$code FSub$$,
$code FMul$$,
$code FDiv$$,

$subhead Memory Access$$
$code Load$$,
$code GetElementPtr$$,
$code Store$$.

$subhead Other$$
$code Ret$$,

$head msg$$
If the return value $icode msg$$ is the empty string,
no error was detected.
Otherwise, $icode msg$$ describes the error and the return value
of $icode graph_obj$$ is unspecified.

$comment example/llvm/from_to_graph.cpp was included by llvm_from_graph$$
$head Example$$
The file $cref llvm_from_to_graph.cpp$$ contains an example and test
of this operation.

$end
*/
# include <cppad/core/llvm/ir.hpp>
//
# include <llvm/IR/Instructions.h>
# include <llvm/IR/InstIterator.h>
# include <llvm/IR/Constants.h>
# include <llvm/IR/InstrTypes.h>

namespace {
    //
    // get_type_id
    llvm::Type::TypeID get_type_id(const llvm::Value* value)
    {   return value->getType()->getTypeID(); }
    //
    // get_int_constant
    size_t get_int_constant(llvm::Value* value)
    {   const llvm::ConstantInt* cint =
            llvm::dyn_cast<const llvm::ConstantInt>(value);
        const llvm::APInt*       apint  = &cint->getValue();
        const uint64_t*          uint64 = apint->getRawData();
        return size_t( *uint64 );
    }
}


//
namespace CppAD { // BEGIN_CPPAD_NAMESPACE

// BEGIN_TO_GRAPH
std::string llvm_ir::to_graph(CppAD::cpp_graph&  graph_obj) const
// END_TO_GRAPH
{   using graph::graph_op_enum;
    //
    // initialize return value with name of this routine
    std::string msg = "llvm_ir::to_graph: ";
    //
    // function_ir
    const llvm::Function* function_ir = module_ir_->getFunction(function_name_);
    CPPAD_ASSERT_UNKNOWN( function_ir  != nullptr );
    //
    // map and llvm::Value* for a value to graph node index
    llvm::DenseMap<const llvm::Value*, size_t>  llvm_value2graph_node;
    //
    // map llvm::Value* for a pointer to graph node index
    llvm::DenseMap<const llvm::Value*, size_t>  llvm_ptr2graph_node;
    //
    // map compare operator result to operands
    struct compare_info {
        llvm::CmpInst::Predicate pred;
        llvm::Value*             left;
        llvm::Value*             right;
    };
    llvm::DenseMap<const llvm::Value*, compare_info>  llvm_compare2info;
    //
    // map llvm::Value* element pointer to base pointer and index value
    struct element_info {
        const llvm::Value*  base;
        size_t              index;
    };
    llvm::DenseMap<const llvm::Value*, element_info>  llvm_element2info;
    //
    // The maps above assume the default constructor for size_t yeilds zero
    CPPAD_ASSERT_UNKNOWN( size_t() == 0 );
    //
    // map from a name to a graph operator
    llvm::StringMap<size_t> name2graph_op;
    //
    // types used by interface to DenseMap
    typedef std::pair<const llvm::Value*, size_t>         value_size;
    typedef std::pair<llvm::StringRef,    size_t>         string_size;
    typedef std::pair<const llvm::Value*, compare_info>   value_compare_info;
    typedef std::pair<const llvm::Value*, element_info>   value_element_info;
    //
    // name2graph_op
    // map function name in IR to correspond to operators in graph
    size_t n_graph_op = size_t( graph::n_graph_op );
    for(size_t i_op = 0; i_op < n_graph_op; ++i_op)
    {   graph_op_enum op_enum = graph_op_enum( i_op );
        const char* name = local::graph::op_enum2name[op_enum];
        switch( op_enum )
        {   // functions that call name
            case graph::acos_graph_op:
            case graph::acosh_graph_op:
            case graph::asin_graph_op:
            case graph::asinh_graph_op:
            case graph::atan_graph_op:
            case graph::atanh_graph_op:
            case graph::cos_graph_op:
            case graph::cosh_graph_op:
            case graph::erf_graph_op:
            case graph::erfc_graph_op:
            case graph::exp_graph_op:
            case graph::expm1_graph_op:
            case graph::log1p_graph_op:
            case graph::log_graph_op:
            case graph::pow_graph_op:
            case graph::sin_graph_op:
            case graph::sinh_graph_op:
            case graph::sqrt_graph_op:
            case graph::tan_graph_op:
            case graph::tanh_graph_op:
            // add one to the operaotor value so we can use zero for not found
            name2graph_op.insert( string_size(name, i_op + 1) );
            break;

            case graph::azmul_graph_op:
            name2graph_op.insert( string_size("cppad_link_azmul", i_op + 1) );
            break;

            case graph::abs_graph_op:
            name2graph_op.insert( string_size("cppad_link_fabs", i_op + 1) );
            break;

            case graph::sign_graph_op:
            name2graph_op.insert( string_size("cppad_link_sign", i_op + 1) );
            break;

            default:
            break;
        }
    }
    //
    // No longer used for error checking
    // const llvm::Argument* len_input  = function_ir->arg_begin() + 0;
    // const llvm::Argument* len_output = function_ir->arg_begin() + 2;
    //
    // input_ptr
    const llvm::Argument* input_ptr  = function_ir->arg_begin() + 1;
    //
    // output_ptr
    const llvm::Argument* output_ptr = function_ir->arg_begin() + 3;
    //
    /// begin_inst
    const llvm::const_inst_iterator begin_inst = llvm::inst_begin(function_ir);
    //
    // end_inst
    const llvm::const_inst_iterator end_inst   = llvm::inst_end(function_ir);
    //
    // drop any inforamtion in this graph object
    graph_obj.initialize();
    //
    // set scalars
    graph_obj.function_name_set(function_name_);
    graph_obj.n_dynamic_ind_set(n_dynamic_ind_);
    graph_obj.n_variable_ind_set(n_variable_ind_);
    //
    // The operands and corresponding type for each instruction
    // Used by both passes
    CppAD::vector<llvm::Value*>       operand;
    CppAD::vector<llvm::Type::TypeID> type_id;
    //
    // The zero floating point constant
    // llvm::Value* fp_zero = llvm::ConstantFP::get(
    //   *context_ir_, llvm::APFloat(0.0)
    //  );
    //
    // First Pass
    // determine the floating point constants in the graph
    CPPAD_ASSERT_UNKNOWN( graph_obj.constant_vec_size() == 0 );
    for(llvm::const_inst_iterator itr = begin_inst; itr != end_inst; ++itr)
    {   unsigned n_operand = itr->getNumOperands();
        operand.resize(n_operand);
        for(unsigned i = 0; i < n_operand; ++i)
            operand[i] = itr->getOperand(i);
        //
        // constant_vec in graph.obj
        for(unsigned i = 0; i < n_operand; ++i)
        {   if( llvm::isa<llvm::ConstantFP>(operand[i]) )
            {   size_t node = llvm_value2graph_node.lookup(operand[i]);
                if( node == 0 )
                {   // First occurance of this constant
                    // dbl
                    const llvm::ConstantFP* constant_fp =
                        llvm::dyn_cast<llvm::ConstantFP>(operand[i]);
                    const llvm::APFloat* apfloat = &constant_fp->getValue();
                    double  dbl = apfloat->convertToDouble();
                    //
                    // node
                    size_t n_constant = graph_obj.constant_vec_size();
                    node = 1 + n_dynamic_ind_ + n_variable_ind_ + n_constant;
                    //
                    // add this constant do data structure
                    llvm_value2graph_node.insert(value_size(operand[i], node));
                    graph_obj.constant_vec_push_back(dbl);
                }
            }
        }
    }
    // n_constant
    size_t n_constant = graph_obj.constant_vec_size();
    //
    // node index 1 corresponds to first element of input vector
    llvm_ptr2graph_node.insert( value_size(input_ptr, 1) );
    //
    // Index 0 corresponds to first lement of output vector.
    {   element_info info;
        info.base = output_ptr;
        info.index = 0;
        llvm_element2info.insert( value_element_info(output_ptr, info) );
    }
    //
    // mapping from dependent variable index to graph node index
    CppAD::vector<size_t> dependent(n_variable_dep_);
# ifndef NDEBUG
    for(size_t i = 0; i < n_variable_dep_; ++i)
        CPPAD_ASSERT_UNKNOWN( dependent[i] == 0 );
    //
    // initialize counter for ZExt instructions
    size_t count_zext = 0;
# endif
    //
    // initial result_node corresponds to first result
    size_t result_node = n_dynamic_ind_ + n_variable_ind_ + n_constant;
    //
    // Second Pass
    for(llvm::const_inst_iterator itr = begin_inst; itr != end_inst; ++itr)
    {
        const llvm::Value* result = llvm::dyn_cast<llvm::Value>( &(*itr) );
# ifndef NDEBUG
        llvm::Type::TypeID result_type_id = get_type_id(result);
# endif
        unsigned op_code                  = itr->getOpcode();
        unsigned n_operand                = itr->getNumOperands();
        operand.resize(n_operand);
        type_id.resize(n_operand);
        for(unsigned i = 0; i < n_operand; ++i)
        {   operand[i] = itr->getOperand(i);
            type_id[i] = get_type_id(operand[i]);
        }
        //
        // temporaries used in switch cases
        const llvm::Value*       compare;
        compare_info             cmp_info;
        element_info             ele_info;
        size_t                   node;
        size_t                   index;
        size_t                   i_op;
        std::string              str;
        //
        // op_code values are defined in llvm/IR/Instructions.def
        graph_op_enum op_enum;
        switch( op_code )
        {
            // --------------------------------------------------------------
            case llvm::Instruction::Load:
            // This instruction is only used to load the first element
            // in the input vector.
            CPPAD_ASSERT_UNKNOWN( n_operand == 1 );
            CPPAD_ASSERT_UNKNOWN( type_id[0] == llvm::Type::PointerTyID );
            node = llvm_ptr2graph_node.lookup(operand[0]);
            CPPAD_ASSERT_UNKNOWN( node != 0 );
            // result is the value that operand[0] points to
            llvm_value2graph_node.insert( value_size(result , node) );
            break;
            //
            // --------------------------------------------------------------
            case llvm::Instruction::Br:
            // branch used to abort and return error_no
            CPPAD_ASSERT_UNKNOWN( n_operand == 3 );
            CPPAD_ASSERT_UNKNOWN( type_id[0] == llvm::Type::IntegerTyID );
            CPPAD_ASSERT_UNKNOWN( type_id[1] == llvm::Type::LabelTyID );
            CPPAD_ASSERT_UNKNOWN( type_id[2] == llvm::Type::LabelTyID );
            break;
            //
            // --------------------------------------------------------------
            case llvm::Instruction::Call:
            if( n_operand == 2 )
            {   CPPAD_ASSERT_UNKNOWN( type_id[0] == llvm::Type::DoubleTyID );
                CPPAD_ASSERT_UNKNOWN( type_id[1] == llvm::Type::PointerTyID );
                str  = operand[1]->getName().str();
            }
            else
            {   CPPAD_ASSERT_UNKNOWN( n_operand == 3 );
                CPPAD_ASSERT_UNKNOWN( type_id[0] == llvm::Type::DoubleTyID );
                CPPAD_ASSERT_UNKNOWN( type_id[1] == llvm::Type::DoubleTyID );
                CPPAD_ASSERT_UNKNOWN( type_id[2] == llvm::Type::PointerTyID );
                str  = operand[2]->getName().str();
            }
            if( str.size() > 9 && str.substr(0, 9) == "discrete_" )
            {   str = str.substr(9, std::string::npos);
                CPPAD_ASSERT_UNKNOWN( n_operand == 2);
                //
                // This must be a discrete function
                graph_obj.operator_vec_push_back( graph::discrete_graph_op );
                // mapping this result to the correspondign new node in graph
                llvm_value2graph_node.insert(
                    value_size(result , ++result_node)
                );
                // determine index of this function in discrete_name_vec
                index = graph_obj.discrete_name_vec_size();
                for(size_t i = 0; i < index; ++i)
                    if( graph_obj.discrete_name_vec_get(i) == str )
                        index = i;
                if( index == graph_obj.discrete_name_vec_size() )
                    graph_obj.discrete_name_vec_push_back( str );
                //
                // put arguments for this operator in the graph
                graph_obj.operator_arg_push_back( index );
                node = llvm_value2graph_node.lookup(operand[0]);
                CPPAD_ASSERT_UNKNOWN( node != 0 );
                graph_obj.operator_arg_push_back( node );
            }
            else
            {
                i_op = name2graph_op.lookup( str.c_str() );
                if( i_op == 0 )
                {   msg += "Cannot call the function " + str;
                    return msg;
                }
                // subtract one that was added so zero means not defined
                graph_obj.operator_vec_push_back( graph_op_enum(i_op - 1) );
                //
                // mapping this result to the correspondign new node in graph
                llvm_value2graph_node.insert(
                    value_size(result , ++result_node)
                );
                //
                // put this operator in the graph
                node = llvm_value2graph_node.lookup(operand[0]);
                CPPAD_ASSERT_UNKNOWN( node != 0 );
                graph_obj.operator_arg_push_back( node );
                if( n_operand == 3 )
                {   node = llvm_value2graph_node.lookup(operand[1]);
                    CPPAD_ASSERT_UNKNOWN( node != 0 );
                    graph_obj.operator_arg_push_back( node );
                }
            }
            break;
            //
            // --------------------------------------------------------------
            case llvm::Instruction::FAdd:
            case llvm::Instruction::FSub:
            case llvm::Instruction::FMul:
            case llvm::Instruction::FDiv:
            // This instruction creates a new node in the graph that corresonds
            // to the sum of two other nodes.
            CPPAD_ASSERT_UNKNOWN( n_operand == 2 );
            CPPAD_ASSERT_UNKNOWN( result_type_id == llvm::Type::DoubleTyID );
            CPPAD_ASSERT_UNKNOWN( type_id[0]     == llvm::Type::DoubleTyID );
            CPPAD_ASSERT_UNKNOWN( type_id[1]     == llvm::Type::DoubleTyID );
            //
            // mapping from this result to the correspondign new node in graph
            llvm_value2graph_node.insert( value_size(result , ++result_node) );
            //
            // put this operator in the graph
            switch( op_code )
            {   case llvm::Instruction::FAdd:
                graph_obj.operator_vec_push_back( graph::add_graph_op );
                break;

                case llvm::Instruction::FSub:
                graph_obj.operator_vec_push_back( graph::sub_graph_op );
                break;

                case llvm::Instruction::FMul:
                graph_obj.operator_vec_push_back( graph::mul_graph_op );
                break;

                case llvm::Instruction::FDiv:
                graph_obj.operator_vec_push_back( graph::div_graph_op );
                break;

                default:
                break;
            }
            // add node index correspnding to left and right operands
            for(size_t i = 0; i < 2; ++i)
            {   node = llvm_value2graph_node.lookup(operand[i]);
                CPPAD_ASSERT_UNKNOWN( node != 0 );
                graph_obj.operator_arg_push_back( node );
            }
            break;
            // --------------------------------------------------------------
            case llvm::Instruction::FCmp:
            CPPAD_ASSERT_UNKNOWN( n_operand == 2 );
            CPPAD_ASSERT_UNKNOWN( type_id[0] == llvm::Type::DoubleTyID );
            CPPAD_ASSERT_UNKNOWN( type_id[1] == llvm::Type::DoubleTyID );
            {   const llvm::CmpInst* cmp_inst =
                    llvm::dyn_cast<llvm::CmpInst>(&*itr);
                cmp_info = {
                    cmp_inst->getPredicate(), operand[0], operand[1]
                };
                llvm_compare2info.insert(
                    value_compare_info(result, cmp_info)
                );
            }
            break;
            //
            // --------------------------------------------------------------
            case llvm::Instruction::GetElementPtr:
            CPPAD_ASSERT_UNKNOWN( n_operand == 2 );
            CPPAD_ASSERT_UNKNOWN( result_type_id == llvm::Type::PointerTyID );
            CPPAD_ASSERT_UNKNOWN( type_id[0]     == llvm::Type::PointerTyID );
            CPPAD_ASSERT_UNKNOWN( type_id[1]     == llvm::Type::IntegerTyID );
            index  = get_int_constant(operand[1]);
            if( operand[0] == output_ptr )
            {   // The only use of output_ptr is to store values.
                element_info info;
                info.base  = output_ptr;
                info.index = index;
                llvm_element2info.insert( value_element_info(result, info) );
            }
            else
            {    CPPAD_ASSERT_UNKNOWN( operand[0] == input_ptr );
                // The only use of input_ptr is load values
                //
                // first element of input_ptr corresponds to node index 1
                node = index + 1;
                llvm_ptr2graph_node.insert( value_size(result, node) );
            }
            break;
            //
            // --------------------------------------------------------------
            case llvm::Instruction::ICmp:
            // This operand is used to check len_input and len_output
            // and to set returned error number
            CPPAD_ASSERT_UNKNOWN( n_operand == 2 );
            CPPAD_ASSERT_UNKNOWN( result_type_id == llvm::Type::IntegerTyID );
            CPPAD_ASSERT_UNKNOWN( type_id[0]     == llvm::Type::IntegerTyID );
            CPPAD_ASSERT_UNKNOWN( type_id[1]     == llvm::Type::IntegerTyID );
            {   const llvm::CmpInst* cmp_inst =
                    llvm::dyn_cast<llvm::CmpInst>(&*itr);
                cmp_info = {
                    cmp_inst->getPredicate(), operand[0], operand[1]
                };
                llvm_compare2info.insert(
                    value_compare_info(result, cmp_info)
                );
            }
            break;
            //
            // --------------------------------------------------------------
            case llvm::Instruction::FNeg:
            CPPAD_ASSERT_UNKNOWN( n_operand == 1 );
            CPPAD_ASSERT_UNKNOWN( result_type_id == llvm::Type::DoubleTyID );
            CPPAD_ASSERT_UNKNOWN( type_id[0]     == llvm::Type::DoubleTyID );
            //
            // mapping from this result to the correspondign new node in graph
            llvm_value2graph_node.insert( value_size(result , ++result_node) );
            //
            // put this operator in the graph
            graph_obj.operator_vec_push_back( graph::neg_graph_op );
            node = llvm_value2graph_node.lookup(operand[0]);
            CPPAD_ASSERT_UNKNOWN( node != 0 );
            graph_obj.operator_arg_push_back( node );
            break;
            //
            // --------------------------------------------------------------
            // Compare instructions
            case llvm::Instruction::ZExt:
            // There is only on ZExt that defines return value
            CPPAD_ASSERT_UNKNOWN( ++count_zext == 1 );
            case llvm::Instruction::Or:
            CPPAD_ASSERT_UNKNOWN( n_operand <= 2 );
            for(size_t i_operand = 0; i_operand < n_operand; ++i_operand)
            {   compare = operand[i_operand];
                cmp_info = llvm_compare2info.lookup(compare);
                if( cmp_info.left != nullptr )
                {   // This operand is a compare operator
                    // op_enum
                    switch( cmp_info.pred )
                    {
                        case llvm::CmpInst::FCMP_ONE:
                        op_enum = graph::comp_eq_graph_op;
                        break;
                        //
                        case llvm::CmpInst::FCMP_OLT:
                        op_enum = graph::comp_le_graph_op;
                        break;
                        //
                        case llvm::CmpInst::FCMP_OLE:
                        op_enum = graph::comp_lt_graph_op;
                        break;
                        //
                        case llvm::CmpInst::FCMP_OEQ:
                        op_enum = graph::comp_ne_graph_op;
                        break;
                        //
                        default:
                        CPPAD_ASSERT_UNKNOWN(false);
                        // only used to avoid compiler warning
                        op_enum = graph::n_graph_op;
                        break;
                    }
                    // comparison operator with order of operands switched
                    graph_obj.operator_vec_push_back( op_enum );
                    // right
                    node = llvm_value2graph_node.lookup( cmp_info.right );
                    graph_obj.operator_arg_push_back(node);
                    // left
                    node = llvm_value2graph_node.lookup( cmp_info.left );
                    graph_obj.operator_arg_push_back(node);
                    // no node in graph for this operation
                }
            }
            break;
            //
            // --------------------------------------------------------------
            case llvm::Instruction::Ret:
            // returns int32_t error_no
            CPPAD_ASSERT_UNKNOWN( n_operand == 1 );
            break;
            // --------------------------------------------------------------
            case llvm::Instruction::Select:
            CPPAD_ASSERT_UNKNOWN( n_operand == 3 );
            // cmp_info
            compare  = operand[0];
            cmp_info = llvm_compare2info.lookup(compare);
            CPPAD_ASSERT_UNKNOWN( cmp_info.left  != nullptr ); // not found ?
            CPPAD_ASSERT_UNKNOWN( cmp_info.right != nullptr );
            //
            if( type_id[1] == llvm::Type::DoubleTyID )
            {   // This is a conditional expression
                CPPAD_ASSERT_UNKNOWN( type_id[2] == llvm::Type::DoubleTyID );
                CPPAD_ASSERT_UNKNOWN(
                    get_type_id(cmp_info.left)  == llvm::Type::DoubleTyID &&
                    get_type_id(cmp_info.right) == llvm::Type::DoubleTyID
                );
                //
                // op_enum
                switch( cmp_info.pred )
                {
                    case llvm::CmpInst::FCMP_OEQ:
                    op_enum = graph::cexp_eq_graph_op;
                    break;
                    //
                    case llvm::CmpInst::FCMP_OLE:
                    op_enum = graph::cexp_le_graph_op;
                    break;
                    //
                    case llvm::CmpInst::FCMP_OLT:
                    op_enum = graph::cexp_lt_graph_op;
                    break;
                    //
                    default:
                    CPPAD_ASSERT_UNKNOWN(false);
                    // only used avoid a compiler warning
                    op_enum = graph::n_graph_op;
                    break;
                }
                // conditional expression operator
                graph_obj.operator_vec_push_back( op_enum );
                // left
                node = llvm_value2graph_node.lookup( cmp_info.left );
                graph_obj.operator_arg_push_back(node);
                // right
                node = llvm_value2graph_node.lookup( cmp_info.right );
                graph_obj.operator_arg_push_back(node);
                // if_true
                node = llvm_value2graph_node.lookup( operand[1] );
                graph_obj.operator_arg_push_back(node);
                // if_false
                node = llvm_value2graph_node.lookup( operand[2] );
                graph_obj.operator_arg_push_back(node);
                //
                // mapping from this result to new node in graph
                llvm_value2graph_node.insert(
                    value_size(result , ++result_node)
                );
            }
            break;
            //
            // --------------------------------------------------------------
            case llvm::Instruction::Store:
            CPPAD_ASSERT_UNKNOWN( n_operand == 2 );
            CPPAD_ASSERT_UNKNOWN( type_id[0] == llvm::Type::DoubleTyID );
            CPPAD_ASSERT_UNKNOWN( type_id[1] == llvm::Type::PointerTyID );
            node     = llvm_value2graph_node.lookup(operand[0]);
            ele_info = llvm_element2info.lookup(operand[1]);
            CPPAD_ASSERT_UNKNOWN( ele_info.base == output_ptr );
            CPPAD_ASSERT_UNKNOWN( node != 0 );
            dependent[ele_info.index] = node;
            break;
            //
            // --------------------------------------------------------------
            default:
            {   std::string op_name = itr->getOpcodeName();
                msg += "Cannot handle the llvm instruction " + op_name;
                return msg;
            }
            break;
        }
    }
    for(size_t i = 0; i < n_variable_dep_; ++i)
    {   if( dependent[i] == 0 )
        {   msg += "No store instruction for dependent variable index ";
            msg += std::to_string(i);
            return msg;
        }
        graph_obj.dependent_vec_push_back( dependent[i] );
    }
    // No error
    msg = "";
    return msg;
}

} // END_CPPAD_NAMESPACE
