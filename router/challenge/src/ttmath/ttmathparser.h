/*
 * This file is a part of TTMath Bignum Library
 * and is distributed under the 3-Clause BSD Licence.
 * Author: Tomasz Sowa <t.sowa@ttmath.org>
 */

/* 
 * Copyright (c) 2006-2017, Tomasz Sowa
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *    
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *    
 *  * Neither the name Tomasz Sowa nor the names of contributors to this
 *    project may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */



#ifndef headerfilettmathparser
#define headerfilettmathparser

/*!
	\file ttmathparser.h
    \brief A mathematical parser
*/

#include <cstdio>
#include <vector>
#include <map>
#include <set>

#include "ttmath.h"
#include "ttmathobjects.h"
#include "ttmathmisc.h"



namespace ttmath
{

/*! 
	\brief Mathematical parser

	let x will be an input string meaning an expression for converting:
	
	x = [+|-]Value[operator[+|-]Value][operator[+|-]Value]...
	where:
		an operator can be:
			^ (pow)   (the heighest priority)

			* (mul)   (or multiplication without an operator -- short mul)
			/ (div)   (* and / have the same priority)

			+ (add)
			- (sub)   (+ and - have the same priority)

			< (lower than)
			> (greater than)
			<= (lower or equal than)
			>= (greater or equal than)
			== (equal)
			!= (not equal)   (all above logical operators have the same priority)
			
			&& (logical and)

			|| (logical or) (the lowest priority)

		short mul:
 		 if the second Value (Var below) is either a variable or function there might not be 
		 an operator between them, e.g.
	        "[+|-]Value Var" is treated as "[+|-]Value * Var" and the multiplication
	        has the same priority as a normal multiplication:
			4x       = 4 * x
			2^3m     = (2^3)* m
			6h^3     = 6 * (h^3)
	        2sin(pi) = 2 * sin(pi)
			etc.

		Value can be:
			constant e.g. 100, can be preceded by operators for changing the base (radix): [#|&]
			                   # - hex
							   & - bin
							   sample: #10  = 16
							           &10  = 2
			variable e.g. pi
			another expression between brackets e.g (x)
			function e.g. sin(x)

	for example a correct input string can be:
		"1"
		"2.1234"
		"2,1234"    (they are the same, by default we can either use a comma or a dot)
		"1 + 2"
		"(1 + 2) * 3"
		"pi"
		"sin(pi)"
		"(1+2)*(2+3)"
		"log(2;1234)"    there's a semicolon here (not a comma), we use it in functions
		                 for separating parameters
	    "1 < 2"  (the result will be: 1)
	    "4 < 3"  (the result will be: 0)
		"2+x"    (of course if the variable 'x' is defined)
		"4x+10"
		"#20+10"     = 32 + 10 = 42
		"10 ^ -&101" = 10 ^ -5 = 0.00001
		"8 * -&10"   = 8 * -2  = -16
		etc.

	we can also use a semicolon for separating any 'x' input strings
	for example:
		"1+2;4+5"
	the result will be on the stack as follows:
		stack[0].value=3
		stack[1].value=9
*/
template<class ValueType>
class Parser
{
protected:

/*!
	there are 5 mathematical operators as follows (with their standard priorities):
		add (+)
		sub (-)
		mul (*)
		div (/)
		pow (^)
		and 'shortmul' used when there is no any operators between
		a first parameter and a variable or function
		(the 'shortmul' has the same priority as the normal multiplication )
*/
	class MatOperator
	{
	public:

		enum Type
		{
			none,add,sub,mul,div,pow,lt,gt,let,get,eq,neq,lor,land,shortmul,assign
		};

		enum Assoc
		{
			right,		// right-associative
			non_right	// associative or left-associative
		};

		Type  GetType()     const { return type; }
		int   GetPriority() const { return priority; }
		Assoc GetAssoc()    const { return assoc; }

		void SetType(Type t)
		{
			type  = t;
			assoc = non_right;

			switch( type )
			{		
			case assign:
				priority = 3;
				assoc = right;
				break;

			case lor:
				priority = 4;
				break;

			case land:
				priority = 5;
				break;

			case eq:
			case neq:
			case lt:
			case gt:
			case let:
			case get:
				priority = 7;
				break;

			case add:
			case sub:
				priority = 10;
				break;

			case mul:
			case shortmul:
			case div:
				priority = 12;
				break;

			case pow:
				priority = 14;
				assoc    = right;
				break;

			case none:
				priority = 0;
				assoc = non_right;
				break;

			default:
				Error( err_internal_error );
				break;
			}
		}

		MatOperator(): type(none), priority(0), assoc(non_right)
		{
		}

	private:

		Type  type;
		int   priority;
		Assoc assoc;
	}; // end of MatOperator class



public:



	/*!
		Objects of type 'Item' we are keeping on our stack
	*/
	struct Item
	{
		enum Type
		{
			none, numerical_value, mat_operator, first_bracket,
			last_bracket, variable, semicolon
		};

		// The kind of type which we're keeping
		Type type;

		// if type == numerical_value
		ValueType value;

		// if type == mat_operator
		MatOperator moperator;

		/*
			if type == first_bracket

			if 'function' is set to true it means that the first recognized bracket
			was the bracket from function in other words we must call a function when
			we'll find the 'last' bracket
		*/
		bool function;

		// if type == first_bracket and if function is true
		// or if type == variable
		std::string function_name;

		/*
			the sign of value

			it can be for type==numerical_value or type==first_bracket
			when it's true it means e.g. that value is equal -value
		*/
		bool sign;

		Item()
		{
			Clear();
		}

		void Clear()
		{
			type = none;
			value.SetNan();
			moperator.SetType(MatOperator::none);
			function = false;
			function_name.clear();
			sign = false;
		}

	}; // end of Item struct


/*!
	stack on which we're keeping the Items

	at the end of parsing we'll have the result here
	the result don't have to be one value, it can be
	more than one if we have used a semicolon in the global space
	e.g. such input string "1+2;3+4" will generate a result:
	 stack[0].value=3
	 stack[1].value=7

	you should check if the stack is not empty, because if there was
	a syntax error in the input string then we do not have any results
	on the stack 
*/
std::vector<Item> stack;


protected:


/*!
	size of the stack when we're starting parsing of the string

	if it's to small while parsing the stack will be automatically resized
*/
const int default_stack_size;



/*!
	index of an object in our stack
	it's pointing on the place behind the last element
	for example at the beginning of parsing its value is zero
*/
unsigned int stack_index;


/*!
	code of the last error
*/
ErrorCode error;


/*!
	pointer to the currently reading char
	when an error has occured it may be used to count the index of the wrong character
*/
const char * pstring;


/*!
	the base (radix) of the mathematic system (for example it may be '10')
*/
int base;


/*!
	the unit of angles used in: sin,cos,tan,cot,asin,acos,atan,acot
	0 - deg
	1 - rad (default)
	2 - grad
*/
int deg_rad_grad;



/*!
	a pointer to an object which tell us whether we should stop calculating or not
*/
const volatile StopCalculating * pstop_calculating;



/*!
	a pointer to the user-defined variables' table
*/
const Objects * puser_variables;

/*!
	a pointer to the user-defined functions' table
*/
const Objects * puser_functions;


typedef std::map<std::string, ValueType> FunctionLocalVariables;

/*!
	a pointer to the local variables of a function
*/
const FunctionLocalVariables * pfunction_local_variables;


/*!
	a temporary set using during parsing user defined variables
*/
std::set<std::string> visited_variables;


/*!
	a temporary set using during parsing user defined functions
*/
std::set<std::string> visited_functions;




/*!
	pfunction is the type of pointer to a mathematic function

	these mathematic functions are private members of this class,
	they are the wrappers for standard mathematics function

	'pstack' is the pointer to the first argument on our stack
	'amount_of_arg' tell us how many argument there are in our stack
	'result' is the reference for result of function 
*/
typedef void (Parser<ValueType>::*pfunction)(unsigned int pstack, int amount_of_arg, ValueType & result);


/*!
	pfunction is the type of pointer to a method which returns value of variable
*/
typedef void (ValueType::*pfunction_var)();


/*!
	table of mathematic functions

	this map consists of:
		std::string - function's name
		pfunction - pointer to specific function
*/
typedef std::map<std::string, pfunction> FunctionsTable;
FunctionsTable functions_table;


/*!
	table of mathematic operators

	this map consists of:
		std::string - operators's name
		MatOperator::Type - type of the operator
*/
typedef std::map<std::string, typename MatOperator::Type> OperatorsTable;
OperatorsTable operators_table;


/*!
	table of mathematic variables

	this map consists of:
		std::string     - variable's name
		pfunction_var - pointer to specific function which returns value of variable
*/
typedef std::map<std::string, pfunction_var> VariablesTable;
VariablesTable variables_table;


/*!
	some coefficients used when calculating the gamma (or factorial) function
*/
CGamma<ValueType> cgamma;


/*!
	temporary object for a whole string when Parse(std::wstring) is used
*/
std::string wide_to_ansi;


/*!
	group character (used when parsing)
	default zero (not used)
*/
int group;


/*!
	characters used as a comma
	default: '.' and ','
	comma2 can be zero (it means it is not used)
*/
int comma, comma2;


/*!
	an additional character used as a separator between function parameters
	(semicolon is used always)
*/
int param_sep;


/*!
	true if something was calculated (at least one mathematical operator was used or a function or a variable)
*/
bool calculated;



/*!
	we're using this method for reporting an error
*/
static void Error(ErrorCode code)
{
	throw code;
}


/*!
	this method skips the white character from the string

	it's moving the 'pstring' to the first no-white character
*/
void SkipWhiteCharacters()
{
	while( (*pstring==' ' ) || (*pstring=='\t') )
		++pstring;
}



/*!
	make sure there is a space for a new item at position stack_index
*/
virtual void EnsurePlaceOnStack()
{
	if( stack_index >= stack.size() )
	{
		stack.push_back(Item());
	}
}


/*!
	add a new item (numerical value) to the stack
*/
void AddToStack(ValueType & value)
{
	EnsurePlaceOnStack();

	stack[stack_index].Clear();
	stack[stack_index].type = Item::numerical_value;
	stack[stack_index].value = value;
	stack_index += 1;
}


/*!
	add a new item to the stack
*/
void AddToStack(typename Item::Type type, bool is_sign)
{
	EnsurePlaceOnStack();

	stack[stack_index].Clear();
	stack[stack_index].type = type;
	stack[stack_index].function = false;
	stack[stack_index].sign = is_sign;
	stack_index += 1;
}


/*!
	add a new item (math operator) to the stack
*/
void AddToStack(typename MatOperator::Type mat_operator_type)
{
	EnsurePlaceOnStack();

	stack[stack_index].Clear();
	stack[stack_index].type = Item::mat_operator;
	stack[stack_index].moperator.SetType(mat_operator_type);
	stack_index += 1;
}


/*!
	add a new item (function or variable) to the stack

	if is_function is true then we add a function name
	when false then we add a variable (such a case is only when an assigment operator = is used e.g. a = 10)
*/
void AddToStack(typename Item::Type type, const std::string & name, bool is_function, bool is_sign)
{
	EnsurePlaceOnStack();

	stack[stack_index].Clear();
	stack[stack_index].type = type;
	stack[stack_index].function_name = name;
	stack[stack_index].function = is_function;
	stack[stack_index].sign = is_sign;
	stack_index += 1;
}



/*!
	an auxiliary method for RecurrenceParsingVariablesOrFunction(...)
*/
void RecurrenceParsingVariablesOrFunction_CheckStopCondition(bool variable, const std::string & name)
{
	if( variable )
	{
		if( visited_variables.find(name) != visited_variables.end() )
			Error( err_variable_loop );
	}
	else
	{
		if( visited_functions.find(name) != visited_functions.end() )
			Error( err_functions_loop );
	}
}


/*!
	an auxiliary method for RecurrenceParsingVariablesOrFunction(...)
*/
void RecurrenceParsingVariablesOrFunction_AddName(bool variable, const std::string & name)
{
	if( variable )
		visited_variables.insert( name );
	else
		visited_functions.insert( name );
}


/*!
	an auxiliary method for RecurrenceParsingVariablesOrFunction(...)
*/
void RecurrenceParsingVariablesOrFunction_DeleteName(bool variable, const std::string & name)
{
	if( variable )
		visited_variables.erase( name );
	else
		visited_functions.erase( name );
}


/*!
	this method returns the value of a variable or function
	by creating a new instance of the mathematical parser 
	and making the standard parsing algorithm on the given string

	this method is used only during parsing user defined variables or functions

	(there can be a recurrence here therefore we're using 'visited_variables'
	and 'visited_functions' sets to make a stop condition)
*/
ValueType RecurrenceParsingVariablesOrFunction(bool variable, const std::string & name, const char * new_string,
											   FunctionLocalVariables * local_variables = 0)
{
	RecurrenceParsingVariablesOrFunction_CheckStopCondition(variable, name);
	RecurrenceParsingVariablesOrFunction_AddName(variable, name);

	Parser<ValueType> NewParser(*this);
	ErrorCode err;

	NewParser.pfunction_local_variables = local_variables;

	try
	{
		err = NewParser.Parse(new_string);
	}
	catch(...)
	{
		RecurrenceParsingVariablesOrFunction_DeleteName(variable, name);

	throw;
	}

	RecurrenceParsingVariablesOrFunction_DeleteName(variable, name);

	if( err != err_ok )
		Error( err );

	if( NewParser.stack.size() != 1 )
		Error( err_must_be_only_one_value );

	if( NewParser.stack[0].type != Item::numerical_value )
		// I think there shouldn't be this error here
		Error( err_incorrect_value );

return NewParser.stack[0].value;
}


public:


/*!
	this method returns the user-defined value of a variable
*/
bool GetValueOfUserDefinedVariable(const std::string & variable_name,ValueType & result)
{
	if( !puser_variables )
		return false;

	const char * string_value;

	if( puser_variables->GetValue(variable_name, &string_value) != err_ok )
		return false;

	result = RecurrenceParsingVariablesOrFunction(true, variable_name, string_value);
	calculated = true;

return true;
}


/*!
	this method returns the value of a local variable of a function
*/
bool GetValueOfFunctionLocalVariable(const std::string & variable_name, ValueType & result)
{
	if( !pfunction_local_variables )
		return false;

	typename FunctionLocalVariables::const_iterator i = pfunction_local_variables->find(variable_name);

	if( i == pfunction_local_variables->end() )
		return false;

	result = i->second;

return true;
}


/*!
	this method returns the value of a variable from variables' table

	we make an object of type ValueType then call a method which 
	sets the correct value in it and finally we'll return the object
*/
virtual ValueType GetValueOfVariable(const std::string & variable_name)
{
ValueType result;

	if( GetValueOfFunctionLocalVariable(variable_name, result) )
		return result;

	if( GetValueOfUserDefinedVariable(variable_name, result) )
		return result;


	typename std::map<std::string, pfunction_var>::iterator i =
													variables_table.find(variable_name);

	if( i == variables_table.end() )
		Error( err_unknown_variable );

	(result.*(i->second))();
	calculated = true;

return result;
}


protected:

/*!
	wrappers for mathematic functions

	'index' is pointing on the first argument on the stack, the second argument has 'index+1' and so on
	'result' will be the result of the function

	(we're using exceptions here for example when function gets an improper argument)
*/


/*!
	used by: sin,cos,tan,cot
*/
ValueType ConvertAngleToRad(const ValueType & input)
{
	if( deg_rad_grad == 1 ) // rad
		return input;

	ValueType result;
	ErrorCode err;

	if( deg_rad_grad == 0 ) // deg
		result = ttmath::DegToRad(input, &err);
	else // grad
		result = ttmath::GradToRad(input, &err);

	if( err != err_ok )
		Error( err );

return result;
}


/*!
	used by: asin,acos,atan,acot
*/
ValueType ConvertRadToAngle(const ValueType & input)
{
	if( deg_rad_grad == 1 ) // rad
		return input;

	ValueType result;
	ErrorCode err;

	if( deg_rad_grad == 0 ) // deg
		result = ttmath::RadToDeg(input, &err);
	else // grad
		result = ttmath::RadToGrad(input, &err);

	if( err != err_ok )
		Error( err );

return result;
}


void Gamma(unsigned int index, int amount_of_args, ValueType & result)
{
	if( amount_of_args != 1 )
		Error( err_improper_amount_of_arguments );

	ErrorCode err;
	
	result = ttmath::Gamma(stack[index].value, cgamma, &err, pstop_calculating);

	if(err != err_ok)
		Error( err );
}


/*!
	factorial
	result = 1 * 2 * 3 * 4 * .... * x
*/
void Factorial(unsigned int index, int amount_of_args, ValueType & result)
{
	if( amount_of_args != 1 )
		Error( err_improper_amount_of_arguments );

	ErrorCode err;

	result = ttmath::Factorial(stack[index].value, cgamma, &err, pstop_calculating);

	if(err != err_ok)
		Error( err );
}


void Abs(unsigned int index, int amount_of_args, ValueType & result)
{
	if( amount_of_args != 1 )
		Error( err_improper_amount_of_arguments );

	result = ttmath::Abs(stack[index].value);
}

void Sin(unsigned int index, int amount_of_args, ValueType & result)
{
	if( amount_of_args != 1 )
		Error( err_improper_amount_of_arguments );

	ErrorCode err;
	result = ttmath::Sin( ConvertAngleToRad(stack[index].value), &err );

	if(err != err_ok)
		Error( err );
}

void Cos(unsigned int index, int amount_of_args, ValueType & result)
{
	if( amount_of_args != 1 )
		Error( err_improper_amount_of_arguments );

	ErrorCode err;
	result = ttmath::Cos( ConvertAngleToRad(stack[index].value), &err );

	if(err != err_ok)
		Error( err );
}

void Tan(unsigned int index, int amount_of_args, ValueType & result)
{
	if( amount_of_args != 1 )
		Error( err_improper_amount_of_arguments );

	ErrorCode err;
	result = ttmath::Tan(ConvertAngleToRad(stack[index].value), &err);

	if(err != err_ok)
		Error( err );
}

void Cot(unsigned int index, int amount_of_args, ValueType & result)
{
	if( amount_of_args != 1 )
		Error( err_improper_amount_of_arguments );

	ErrorCode err;
	result = ttmath::Cot(ConvertAngleToRad(stack[index].value), &err);

	if(err != err_ok)
		Error( err );
}

void Int(unsigned int index, int amount_of_args, ValueType & result)
{
	if( amount_of_args != 1 )
		Error( err_improper_amount_of_arguments );

	result = ttmath::SkipFraction(stack[index].value);
}


void Round(unsigned int index, int amount_of_args, ValueType & result)
{
	if( amount_of_args != 1 )
		Error( err_improper_amount_of_arguments );

	result = stack[index].value;

	if( result.Round() )
		Error( err_overflow );
}


void Ln(unsigned int index, int amount_of_args, ValueType & result)
{
	if( amount_of_args != 1 )
		Error( err_improper_amount_of_arguments );

	ErrorCode err;
	result = ttmath::Ln(stack[index].value, &err);

	if(err != err_ok)
		Error( err );
}

void Log(unsigned int index, int amount_of_args, ValueType & result)
{
	if( amount_of_args != 2 )
		Error( err_improper_amount_of_arguments );

	ErrorCode err;
	result = ttmath::Log(stack[index].value, stack[index+1].value, &err);

	if(err != err_ok)
		Error( err );
}

void Exp(unsigned int index, int amount_of_args, ValueType & result)
{
	if( amount_of_args != 1 )
		Error( err_improper_amount_of_arguments );

	ErrorCode err;
	result = ttmath::Exp(stack[index].value, &err);

	if(err != err_ok)
		Error( err );
}


void Max(unsigned int index, int amount_of_args, ValueType & result)
{
	if( amount_of_args == 0 )
	{
		result.SetMax();

	return;
	}

	result = stack[index].value;

	for(int i=1 ; i < amount_of_args ; ++i)
	{
		if( result < stack[index + i].value )
			result = stack[index + i].value;
	}
}


void Min(unsigned int index, int amount_of_args, ValueType & result)
{
	if( amount_of_args == 0 )
	{
		result.SetMin();

	return;
	}

	result = stack[index].value;

	for(int i=1 ; i<amount_of_args ; ++i)
	{
		if( result > stack[index + i].value )
			result = stack[index + i].value;
	}
}


void ASin(unsigned int index, int amount_of_args, ValueType & result)
{
	if( amount_of_args != 1 )
		Error( err_improper_amount_of_arguments );

	ErrorCode err;
	ValueType temp = ttmath::ASin(stack[index].value, &err);

	if(err != err_ok)
		Error( err );

	result = ConvertRadToAngle(temp);
}


void ACos(unsigned int index, int amount_of_args, ValueType & result)
{
	if( amount_of_args != 1 )
		Error( err_improper_amount_of_arguments );

	ErrorCode err;
	ValueType temp = ttmath::ACos(stack[index].value, &err);

	if(err != err_ok)
		Error( err );

	result = ConvertRadToAngle(temp);
}


void ATan(unsigned int index, int amount_of_args, ValueType & result)
{
	if( amount_of_args != 1 )
		Error( err_improper_amount_of_arguments );

	result = ConvertRadToAngle(ttmath::ATan(stack[index].value));
}


void ACot(unsigned int index, int amount_of_args, ValueType & result)
{
	if( amount_of_args != 1 )
		Error( err_improper_amount_of_arguments );

	result = ConvertRadToAngle(ttmath::ACot(stack[index].value));
}


void Sgn(unsigned int index, int amount_of_args, ValueType & result)
{
	if( amount_of_args != 1 )
		Error( err_improper_amount_of_arguments );

	result = ttmath::Sgn(stack[index].value);
}


void Mod(unsigned int index, int amount_of_args, ValueType & result)
{
	if( amount_of_args != 2 )
		Error( err_improper_amount_of_arguments );

	if( stack[index+1].value.IsZero() )
		Error( err_improper_argument );

	result = stack[index].value;
	uint c = result.Mod(stack[index+1].value);

	if( c )
		Error( err_overflow );
}


void If(unsigned int index, int amount_of_args, ValueType & result)
{
	if( amount_of_args != 3 )
		Error( err_improper_amount_of_arguments );


	if( !stack[index].value.IsZero() )
		result = stack[index+1].value;
	else
		result = stack[index+2].value;
}


void Or(unsigned int index, int amount_of_args, ValueType & result)
{
	if( amount_of_args < 2 )
		Error( err_improper_amount_of_arguments );

	for(int i=0 ; i < amount_of_args ; ++i)
	{
		if( !stack[index+i].value.IsZero() )
		{
			result.SetOne();
			return;
		}
	}

	result.SetZero();
}


void And(unsigned int index, int amount_of_args, ValueType & result)
{
	if( amount_of_args < 2 )
		Error( err_improper_amount_of_arguments );

	for(int i=0 ; i<amount_of_args ; ++i)
	{
		if( stack[index+i].value.IsZero() )
		{
			result.SetZero();
			return;
		}
	}

	result.SetOne();
}


void Not(unsigned int index, int amount_of_args, ValueType & result)
{
	if( amount_of_args != 1 )
		Error( err_improper_amount_of_arguments );


	if( stack[index].value.IsZero() )
		result.SetOne();
	else
		result.SetZero();
}


void DegToRad(unsigned int index, int amount_of_args, ValueType & result)
{
	ErrorCode err = err_ok;

	if( amount_of_args == 1 )
	{
		result = ttmath::DegToRad(stack[index].value, &err);
	}
	else
	if( amount_of_args == 3 )
	{
		result = ttmath::DegToRad(	stack[index].value, stack[index+1].value,
									stack[index+2].value, &err);
	}
	else
		Error( err_improper_amount_of_arguments );


	if( err != err_ok )
		Error( err );
}


void RadToDeg(unsigned int index, int amount_of_args, ValueType & result)
{
	ErrorCode err;

	if( amount_of_args != 1 )
		Error( err_improper_amount_of_arguments );
	
	result = ttmath::RadToDeg(stack[index].value, &err);

	if( err != err_ok )
		Error( err );
}


void DegToDeg(unsigned int index, int amount_of_args, ValueType & result)
{
	if( amount_of_args != 3 )
		Error( err_improper_amount_of_arguments );

	ErrorCode err;
	result = ttmath::DegToDeg(	stack[index].value, stack[index+1].value,
								stack[index+2].value, &err);

	if( err != err_ok )
		Error( err );
}


void GradToRad(unsigned int index, int amount_of_args, ValueType & result)
{
	ErrorCode err;

	if( amount_of_args != 1 )
		Error( err_improper_amount_of_arguments );
	
	result = ttmath::GradToRad(stack[index].value, &err);

	if( err != err_ok )
		Error( err );
}


void RadToGrad(unsigned int index, int amount_of_args, ValueType & result)
{
	ErrorCode err;

	if( amount_of_args != 1 )
		Error( err_improper_amount_of_arguments );
	
	result = ttmath::RadToGrad(stack[index].value, &err);

	if( err != err_ok )
		Error( err );
}


void DegToGrad(unsigned int index, int amount_of_args, ValueType & result)
{
	ErrorCode err = err_ok;

	if( amount_of_args == 1 )
	{
		result = ttmath::DegToGrad(stack[index].value, &err);
	}
	else
	if( amount_of_args == 3 )
	{
		result = ttmath::DegToGrad(	stack[index].value, stack[index+1].value,
									stack[index+2].value, &err);
	}
	else
		Error( err_improper_amount_of_arguments );


	if( err != err_ok )
		Error( err );
}


void GradToDeg(unsigned int index, int amount_of_args, ValueType & result)
{
	ErrorCode err;

	if( amount_of_args != 1 )
		Error( err_improper_amount_of_arguments );
	
	result = ttmath::GradToDeg(stack[index].value, &err);

	if( err != err_ok )
		Error( err );
}


void Ceil(unsigned int index, int amount_of_args, ValueType & result)
{
	if( amount_of_args != 1 )
		Error( err_improper_amount_of_arguments );

	ErrorCode err;
	result = ttmath::Ceil(stack[index].value, &err);

	if( err != err_ok )
		Error( err );
}


void Floor(unsigned int index, int amount_of_args, ValueType & result)
{
	if( amount_of_args != 1 )
		Error( err_improper_amount_of_arguments );

	ErrorCode err;
	result = ttmath::Floor(stack[index].value, &err);

	if( err != err_ok )
		Error( err );
}

void Sqrt(unsigned int index, int amount_of_args, ValueType & result)
{
	if( amount_of_args != 1 )
		Error( err_improper_amount_of_arguments );

	ErrorCode err;
	result = ttmath::Sqrt(stack[index].value, &err);

	if( err != err_ok )
		Error( err );
}


void Sinh(unsigned int index, int amount_of_args, ValueType & result)
{
	if( amount_of_args != 1 )
		Error( err_improper_amount_of_arguments );

	ErrorCode err;
	result = ttmath::Sinh(stack[index].value, &err);

	if( err != err_ok )
		Error( err );
}


void Cosh(unsigned int index, int amount_of_args, ValueType & result)
{
	if( amount_of_args != 1 )
		Error( err_improper_amount_of_arguments );

	ErrorCode err;
	result = ttmath::Cosh(stack[index].value, &err);

	if( err != err_ok )
		Error( err );
}


void Tanh(unsigned int index, int amount_of_args, ValueType & result)
{
	if( amount_of_args != 1 )
		Error( err_improper_amount_of_arguments );

	ErrorCode err;
	result = ttmath::Tanh(stack[index].value, &err);

	if( err != err_ok )
		Error( err );
}


void Coth(unsigned int index, int amount_of_args, ValueType & result)
{
	if( amount_of_args != 1 )
		Error( err_improper_amount_of_arguments );

	ErrorCode err;
	result = ttmath::Coth(stack[index].value, &err);

	if( err != err_ok )
		Error( err );
}


void Root(unsigned int index, int amount_of_args, ValueType & result)
{
	if( amount_of_args != 2 )
		Error( err_improper_amount_of_arguments );

	ErrorCode err;
	result = ttmath::Root(stack[index].value, stack[index+1].value, &err);

	if( err != err_ok )
		Error( err );
}



void ASinh(unsigned int index, int amount_of_args, ValueType & result)
{
	if( amount_of_args != 1 )
		Error( err_improper_amount_of_arguments );

	ErrorCode err;
	result = ttmath::ASinh(stack[index].value, &err);

	if( err != err_ok )
		Error( err );
}


void ACosh(unsigned int index, int amount_of_args, ValueType & result)
{
	if( amount_of_args != 1 )
		Error( err_improper_amount_of_arguments );

	ErrorCode err;
	result = ttmath::ACosh(stack[index].value, &err);

	if( err != err_ok )
		Error( err );
}


void ATanh(unsigned int index, int amount_of_args, ValueType & result)
{
	if( amount_of_args != 1 )
		Error( err_improper_amount_of_arguments );

	ErrorCode err;
	result = ttmath::ATanh(stack[index].value, &err);

	if( err != err_ok )
		Error( err );
}


void ACoth(unsigned int index, int amount_of_args, ValueType & result)
{
	if( amount_of_args != 1 )
		Error( err_improper_amount_of_arguments );

	ErrorCode err;
	result = ttmath::ACoth(stack[index].value, &err);

	if( err != err_ok )
		Error( err );
}


void BitAnd(unsigned int index, int amount_of_args, ValueType & result)
{
	if( amount_of_args != 2 )
		Error( err_improper_amount_of_arguments );

	uint err;
	result = stack[index].value;
	err = result.BitAnd(stack[index+1].value);

	switch(err)
	{
	case 1:
		Error( err_overflow );
		break;
	case 2:
		Error( err_improper_argument );
		break;
	}
}

void BitOr(unsigned int index, int amount_of_args, ValueType & result)
{
	if( amount_of_args != 2 )
		Error( err_improper_amount_of_arguments );

	uint err;
	result = stack[index].value;
	err = result.BitOr(stack[index+1].value);

	switch(err)
	{
	case 1:
		Error( err_overflow );
		break;
	case 2:
		Error( err_improper_argument );
		break;
	}
}


void BitXor(unsigned int index, int amount_of_args, ValueType & result)
{
	if( amount_of_args != 2 )
		Error( err_improper_amount_of_arguments );

	uint err;
	result = stack[index].value;
	err = result.BitXor(stack[index+1].value);

	switch(err)
	{
	case 1:
		Error( err_overflow );
		break;
	case 2:
		Error( err_improper_argument );
		break;
	}
}


void Sum(unsigned int index, int amount_of_args, ValueType & result)
{
	if( amount_of_args == 0 )
		Error( err_improper_amount_of_arguments );

	result = stack[index].value;

	for(int i=1 ; i < amount_of_args ; ++i )
		if( result.Add( stack[ index + i ].value ) )
			Error( err_overflow );
}	

void Avg(unsigned int index, int amount_of_args, ValueType & result)
{
	if( amount_of_args == 0 )
		Error( err_improper_amount_of_arguments );

	result = stack[index].value;

	for(int i=1 ; i < amount_of_args ; ++i )
		if( result.Add( stack[ index + i ].value ) )
			Error( err_overflow );

	if( result.Div( amount_of_args ) )
		Error( err_overflow );
}	


void Frac(unsigned int index, int amount_of_args, ValueType & result)
{
	if( amount_of_args != 1 )
		Error( err_improper_amount_of_arguments );

	result = stack[index].value;
	result.RemainFraction();
}




/*!
	we use such a method because 'wvsprintf' is not everywhere defined
*/
void Sprintf(char * buffer, int par)
{
char buf[30]; // char, not wchar_t
int i;

	#ifdef _MSC_VER
	#pragma warning( disable: 4996 )
	//warning C4996: 'sprintf': This function or variable may be unsafe.
	#endif

	sprintf(buf, "%d", par);
	for(i=0 ; buf[i] != 0 ; ++i)
		buffer[i] = buf[i];

	buffer[i] = 0;

	#ifdef _MSC_VER
	#pragma warning( default: 4996 )
	#endif
}




/*!
	this method returns the value from a user-defined function

	(look at the description in 'CallFunction(...)')
*/
bool GetValueOfUserDefinedFunction(const std::string & function_name, int amount_of_args, unsigned int index)
{
	if( !puser_functions )
		return false;

	const char * string_value;
	int param;

	if( puser_functions->GetValueAndParam(function_name, &string_value, &param) != err_ok )
		return false;

	if( param != amount_of_args )
		Error( err_improper_amount_of_arguments );


	FunctionLocalVariables local_variables;

	if( amount_of_args > 0 )
	{
		char buffer[30];

		// x = x1
		buffer[0] = 'x';
		buffer[1] = 0;
		local_variables.insert( std::make_pair(buffer, stack[index].value) );

		for(int i=0 ; i<amount_of_args ; ++i)
		{
			buffer[0] = 'x';
			Sprintf(buffer+1, i+1);
			local_variables.insert( std::make_pair(buffer, stack[index + i].value) );
		}
	}

	stack[index-1].value = RecurrenceParsingVariablesOrFunction(false, function_name, string_value, &local_variables);
	calculated = true;

return true;
}


/*
	we're calling a specific function

	function_name  - name of the function
	amount_of_args - how many arguments there are on our stack
					 (function must check whether this is a correct value or not)
	index          - index of the first argument on the stack (index is greater than zero)
  					 if there are no arguments on the stack then 'index' is pointing to
					 a non existend element (after the first bracket)

	result will be stored in 'stack[index-1].value'
	(we don't have to set the correct type of this element, it'll be set later)
*/
virtual void CallFunction(const std::string & function_name, int amount_of_args, unsigned int index)
{
	if( GetValueOfUserDefinedFunction(function_name, amount_of_args, index) )
		return;

	typename FunctionsTable::iterator i = functions_table.find( function_name );

	if( i == functions_table.end() )
		Error( err_unknown_function );

	/*
		calling the specify function
	*/
	(this->*(i->second))(index, amount_of_args, stack[index-1].value);
	calculated = true;
}





/*!
	inserting a function to the functions' table

	function_name - name of the function
	pf - pointer to the function (to the wrapper)
*/
void InsertFunctionToTable(const char * function_name, pfunction pf)
{
	std::string str;
	Misc::AssignString(str, function_name);

	functions_table.insert( std::make_pair(str, pf) );
}



/*!
	inserting a function to the variables' table
	(this function returns value of variable)

	variable_name - name of the function
	pf - pointer to the function
*/
void InsertVariableToTable(const char * variable_name, pfunction_var pf)
{
	std::string str;
	Misc::AssignString(str, variable_name);

	variables_table.insert( std::make_pair(str, pf) );
}


/*!
	this method creates the table of functions
*/
void CreateFunctionsTable()
{
	InsertFunctionToTable("gamma",		&Parser<ValueType>::Gamma);
	InsertFunctionToTable("factorial",	&Parser<ValueType>::Factorial);
	InsertFunctionToTable("abs",   		&Parser<ValueType>::Abs);
	InsertFunctionToTable("sin",   		&Parser<ValueType>::Sin);
	InsertFunctionToTable("cos",   		&Parser<ValueType>::Cos);
	InsertFunctionToTable("tan",   		&Parser<ValueType>::Tan);
	InsertFunctionToTable("tg",			&Parser<ValueType>::Tan);
	InsertFunctionToTable("cot",  		&Parser<ValueType>::Cot);
	InsertFunctionToTable("ctg",  		&Parser<ValueType>::Cot);
	InsertFunctionToTable("int",	   	&Parser<ValueType>::Int);
	InsertFunctionToTable("round",	 	&Parser<ValueType>::Round);
	InsertFunctionToTable("ln",			&Parser<ValueType>::Ln);
	InsertFunctionToTable("log",	   	&Parser<ValueType>::Log);
	InsertFunctionToTable("exp",	   	&Parser<ValueType>::Exp);
	InsertFunctionToTable("max",	   	&Parser<ValueType>::Max);
	InsertFunctionToTable("min",	   	&Parser<ValueType>::Min);
	InsertFunctionToTable("asin",   	&Parser<ValueType>::ASin);
	InsertFunctionToTable("acos",   	&Parser<ValueType>::ACos);
	InsertFunctionToTable("atan",   	&Parser<ValueType>::ATan);
	InsertFunctionToTable("atg",	   	&Parser<ValueType>::ATan);
	InsertFunctionToTable("acot",   	&Parser<ValueType>::ACot);
	InsertFunctionToTable("actg",   	&Parser<ValueType>::ACot);
	InsertFunctionToTable("sgn",   		&Parser<ValueType>::Sgn);
	InsertFunctionToTable("mod",   		&Parser<ValueType>::Mod);
	InsertFunctionToTable("if",   		&Parser<ValueType>::If);
	InsertFunctionToTable("or",   		&Parser<ValueType>::Or);
	InsertFunctionToTable("and",  		&Parser<ValueType>::And);
	InsertFunctionToTable("not",  		&Parser<ValueType>::Not);
	InsertFunctionToTable("degtorad",	&Parser<ValueType>::DegToRad);
	InsertFunctionToTable("radtodeg",	&Parser<ValueType>::RadToDeg);
	InsertFunctionToTable("degtodeg",	&Parser<ValueType>::DegToDeg);
	InsertFunctionToTable("gradtorad",	&Parser<ValueType>::GradToRad);
	InsertFunctionToTable("radtograd",	&Parser<ValueType>::RadToGrad);
	InsertFunctionToTable("degtograd",	&Parser<ValueType>::DegToGrad);
	InsertFunctionToTable("gradtodeg",	&Parser<ValueType>::GradToDeg);
	InsertFunctionToTable("ceil",		&Parser<ValueType>::Ceil);
	InsertFunctionToTable("floor",		&Parser<ValueType>::Floor);
	InsertFunctionToTable("sqrt",		&Parser<ValueType>::Sqrt);
	InsertFunctionToTable("sinh",		&Parser<ValueType>::Sinh);
	InsertFunctionToTable("cosh",		&Parser<ValueType>::Cosh);
	InsertFunctionToTable("tanh",		&Parser<ValueType>::Tanh);
	InsertFunctionToTable("tgh",		&Parser<ValueType>::Tanh);
	InsertFunctionToTable("coth",		&Parser<ValueType>::Coth);
	InsertFunctionToTable("ctgh",		&Parser<ValueType>::Coth);
	InsertFunctionToTable("root",		&Parser<ValueType>::Root);
	InsertFunctionToTable("asinh",		&Parser<ValueType>::ASinh);
	InsertFunctionToTable("acosh",		&Parser<ValueType>::ACosh);
	InsertFunctionToTable("atanh",		&Parser<ValueType>::ATanh);
	InsertFunctionToTable("atgh",		&Parser<ValueType>::ATanh);
	InsertFunctionToTable("acoth",		&Parser<ValueType>::ACoth);
	InsertFunctionToTable("actgh",		&Parser<ValueType>::ACoth);
	InsertFunctionToTable("bitand",		&Parser<ValueType>::BitAnd);
	InsertFunctionToTable("bitor",		&Parser<ValueType>::BitOr);
	InsertFunctionToTable("bitxor",		&Parser<ValueType>::BitXor);
	InsertFunctionToTable("band",		&Parser<ValueType>::BitAnd);
	InsertFunctionToTable("bor",		&Parser<ValueType>::BitOr);
	InsertFunctionToTable("bxor",		&Parser<ValueType>::BitXor);
	InsertFunctionToTable("sum",		&Parser<ValueType>::Sum);
	InsertFunctionToTable("avg",		&Parser<ValueType>::Avg);
	InsertFunctionToTable("frac",		&Parser<ValueType>::Frac);
}


/*!
	this method creates the table of variables
*/
void CreateVariablesTable()
{
	InsertVariableToTable("pi", &ValueType::SetPi);
	InsertVariableToTable("e",  &ValueType::SetE);
}


/*!
	converting from a big letter to a small one
*/
int ToLowerCase(int c)
{
	if( c>='A' && c<='Z' )
		return c - 'A' + 'a';

return c;
}




/*!
	return true if c is a valid function or variable name character
*/
virtual bool IsValidNameCharacter(int c)
{
	return(	(c>='a' && c<='z') ||
			(c>='A' && c<='Z') ||
			(c>='0' && c<='9') ||
			c=='_' );
}


/*!
	this method read the name of a variable or a function
	'result' will be the name of a variable or a function
*/
virtual void ReadName(std::string & result)
{
	result.erase();

	while( IsValidNameCharacter(*pstring) )
	{
		result += *pstring;
		pstring += 1;
	}
}



/*!
	we're reading the name of a variable or a function
	if is there a function we'll return 'true'
	also we return true when we read the name of a variable with an assignment operator
*/
virtual bool ReadVariableOrFunction(bool was_sign, bool was_minus_sign)
{
	std::string name;
	bool read_next_token = false;

	ReadName(name);
	SkipWhiteCharacters();

	if( *pstring == '(' )
	{
		/*
			if there's a character '(' that means this name is a name of a function
		*/
		++pstring;
		AddToStack(Item::first_bracket, name, true, was_minus_sign);
		read_next_token = true;
	}
	else
	if( *pstring == '=' && *(pstring+1) != '=' )
	{
		/*
			if there's a character '=' that means this name is a name of a variable with an assigment operator
		*/
		++pstring;

		if( !was_sign )
		{
			AddToStack(Item::variable, name, false, false);
			AddToStack(MatOperator::assign);
			read_next_token = true;
		}
		else
		{
			// something like: '-a = 10' or '+a = 10'
			Error( err_assignment_requires_variable );
		}
	}
	else
	{
		/*
			we've read the name of a variable and we're getting its value now
		*/
		ValueType value = GetValueOfVariable(name);

		if( was_minus_sign )
			value.ChangeSign();

		AddToStack(value);
	}

	return read_next_token;
}






/*!
	we're reading a numerical value directly from the string
*/
virtual void ReadValue(int reading_base, bool change_sign)
{
const char * after_value_string;
bool value_read;
Conv conv;

	conv.base   = reading_base;
	conv.comma  = comma;
	conv.comma2 = comma2;
	conv.group  = group;

	ValueType value;
	uint carry = value.FromString(pstring, conv, &after_value_string, &value_read);
	pstring    = after_value_string;

	if( carry )
		Error( err_overflow );

	if( !value_read )
		Error( err_unknown_character );

	if( change_sign )
	{
		value.ChangeSign();
	}

	AddToStack(value);
}


/*!
	this method returns true if 'character' is a proper first digit for the value (or a comma -- can be first too)
*/
virtual bool ValueStarts(int character, int character_base)
{
	if( character == comma )
		return true;

	if( comma2!=0 && character==comma2 )
		return true;

	if( Misc::CharToDigit(character, character_base) != -1 )
		return true;

return false;
}


virtual void CheckSignBeforeExpression(bool & was_sign, bool & was_minus_sign)
{
	was_sign = false;
	was_minus_sign = false;

	SkipWhiteCharacters();

	if( *pstring == '-' )
	{
		was_sign = true;
		was_minus_sign = true;
		++pstring;
	}

	if( *pstring == '+' )
	{
		was_sign = true;
		++pstring;
	}
}



virtual bool ReadToken()
{
	bool was_sign, was_minus_sign;
	bool read_next_token = false;

	CheckSignBeforeExpression(was_sign, was_minus_sign);
	SkipWhiteCharacters();

	int character = ToLowerCase( *pstring );

	if( character == 0 )
	{
		if( was_sign )
		{
			Error( err_unexpected_end ); // at the end of the string a character like '-' or '+' has left
		}
	}
	else
	if( character == '(' )
	{
		AddToStack(Item::first_bracket, was_minus_sign); // we've got a normal bracket (not a function)
		++pstring;
		read_next_token = true;
	}
	else
	if( character == '#' )
	{
		++pstring;

		// after '#' character must be directly value (we do not allow '-' or '+' or white characters)
		if(	ValueStarts(*pstring, 16) )
			ReadValue(16, was_minus_sign);
		else
			Error( err_unknown_character );
	}
	else
	if( character == '&' )
	{
		++pstring;

		// after '&' character must be directly value (we do not allow '-' or '+' or white characters)
		if(	ValueStarts(*pstring, 2) )
			ReadValue(2, was_minus_sign);
		else
			Error( err_unknown_character );
	}
	else
	if(	ValueStarts(character, base) )
	{
		ReadValue(base, was_minus_sign);
	}
	else
	if( character>='a' && character<='z' )
	{
		read_next_token = ReadVariableOrFunction(was_sign, was_minus_sign);
	}
	else
	{
		Error( err_unknown_character );
	}

	return read_next_token;
}


/*!
	we're reading the item
  
	return values:
		0 - all ok, the item is successfully read
		1 - the end of the string (the item is not read)
		2 - the final bracket ')'
*/
virtual void ReadTokens()
{
	bool continue_reading;

	do
	{
		continue_reading = false;
		SkipWhiteCharacters();

		if( *pstring )
			continue_reading = ReadToken();

	} while( continue_reading );
}


void InsertOperatorToTable(const char * name, typename MatOperator::Type type)
{
	operators_table.insert( std::make_pair(std::string(name), type) );
}


/*!
	this method creates the table of operators
*/
void CreateMathematicalOperatorsTable()
{
	InsertOperatorToTable("||", MatOperator::lor);
	InsertOperatorToTable("&&", MatOperator::land);
	InsertOperatorToTable("!=", MatOperator::neq);
	InsertOperatorToTable("==", MatOperator::eq);
	InsertOperatorToTable(">=", MatOperator::get);
	InsertOperatorToTable("<=", MatOperator::let);
	InsertOperatorToTable(">",  MatOperator::gt);
	InsertOperatorToTable("<",  MatOperator::lt);
	InsertOperatorToTable("-",  MatOperator::sub);
	InsertOperatorToTable("+",  MatOperator::add);
	InsertOperatorToTable("/",  MatOperator::div);
	InsertOperatorToTable("*",  MatOperator::mul);
	InsertOperatorToTable("^",  MatOperator::pow);
	InsertOperatorToTable("=",  MatOperator::assign);
}


/*!
	returns true if 'short_str' is the substring of long_str

	e.g.
	true when short_str="te" and long_str="test"
*/
virtual bool IsSubstring(const std::string & short_str, const std::string & long_str)
{
	if( short_str.length() > long_str.length() )
		return false;

	for(typename std::string::size_type i=0 ; i<short_str.length() ; ++i)
		if( short_str[i] != long_str[i] )
			return false;

return true;
}


/*!
	this method reads a mathematical (or logical) operator
*/
void ReadMathematicalOperator()
{
std::string oper;
typename OperatorsTable::iterator iter_old, iter_new;

	iter_old = operators_table.end();

	for( ; true ; ++pstring )
	{
		oper += *pstring;
		iter_new = operators_table.lower_bound(oper);
		
		if( iter_new == operators_table.end() || !IsSubstring(oper, iter_new->first) )
		{
			oper.erase(oper.begin() + oper.size() - 1); // we've got minimum one element

			if( iter_old != operators_table.end() && iter_old->first == oper )
			{
				AddToStack(iter_old->second);
				break;
			}
			
			Error( err_unknown_operator );
		}
	
		iter_old = iter_new;
	}
}


/*!
	this method makes a calculation for the percentage operator
	e.g.
	1000-50% = 1000-(1000*0,5) = 500
*/
void OperatorPercentage()
{
	if( stack_index < 3										||
		stack[stack_index-1].type != Item::numerical_value	||
		stack[stack_index-2].type != Item::mat_operator		||
		stack[stack_index-3].type != Item::numerical_value	)
	{
		Error( err_percent_from );
	}
	else
	{
		uint c = 0;
		c += stack[stack_index-1].value.Div(100);
		c += stack[stack_index-1].value.Mul(stack[stack_index-3].value);

		if( c )
		{
			Error( err_overflow );
		}
	}
}


/*!
	this method reads a mathematic operators
	or the final bracket or the semicolon operator

	return values:
		true - if next operator should be read
*/
virtual bool ReadOperator()
{
	bool read_next_operator = false;

	if( *pstring == '%' )
	{
		OperatorPercentage();
		++pstring;
		// now we can read another operator
	}

	if( *pstring == ')' )
	{
		++pstring;
		RollingUpFinalBracket();
		read_next_operator = true;
	}
	else
	if( *pstring == ';' || (param_sep!=0 && *pstring==param_sep) )
	{
		RollUPStack();
		++pstring;
	}
	else
	if( (*pstring>='a' && *pstring<='z') || (*pstring>='A' && *pstring<='Z') )
	{
		// short mul (without any operators)
		// short mul is available only if the next item is a variable or a function
		AddToStack(MatOperator::shortmul);
	}
	else
	{
		ReadMathematicalOperator();
	}

	return read_next_operator;
}



/*!
	this method is reading a operator and if it's a final bracket
	it's calling RollingUpFinalBracket() and reading an operator again
*/
virtual void ReadOperators()
{
	bool continue_reading;

	do
	{
		continue_reading = false;
		SkipWhiteCharacters();

		if( *pstring )
			continue_reading = ReadOperator();
	}
	while( continue_reading );
}




/*!
	this method is making the standard mathematic operation like '-' '+' '*' '/' and '^'

	the operation is made between 'value1' and 'value2'
	the result of this operation is stored in the 'value1'
*/
virtual void MakeStandardMathematicOperation(Item & item1, typename MatOperator::Type mat_operator, const Item & item2)
{
uint res;

	calculated = true;
	ValueType & value1  = item1.value;
	const ValueType & value2 = item2.value;

	switch( mat_operator )
	{
	case MatOperator::land:
		(!value1.IsZero() && !value2.IsZero()) ? value1.SetOne() : value1.SetZero();
		break;

	case MatOperator::lor:
		(!value1.IsZero() || !value2.IsZero()) ? value1.SetOne() : value1.SetZero();
		break;

	case MatOperator::eq:
		(value1 == value2) ? value1.SetOne() : value1.SetZero();
		break;

	case MatOperator::neq:
		(value1 != value2) ? value1.SetOne() : value1.SetZero();
		break;

	case MatOperator::lt:
		(value1 < value2) ? value1.SetOne() : value1.SetZero();
		break;

	case MatOperator::gt:
		(value1 > value2) ? value1.SetOne() : value1.SetZero();
		break;

	case MatOperator::let:
		(value1 <= value2) ? value1.SetOne() : value1.SetZero();
		break;

	case MatOperator::get:
		(value1 >= value2) ? value1.SetOne() : value1.SetZero();
		break;

	case MatOperator::sub:
		if( value1.Sub(value2) ) Error( err_overflow );
		break;

	case MatOperator::add:
		if( value1.Add(value2) ) Error( err_overflow );
		break;

	case MatOperator::mul:
	case MatOperator::shortmul:
		if( value1.Mul(value2) ) Error( err_overflow );
		break;

	case MatOperator::div:
		if( value2.IsZero() )    Error( err_division_by_zero );
		if( value1.Div(value2) ) Error( err_overflow );
		break;

	case MatOperator::pow:
		res = value1.Pow( value2 );

		if( res == 1 ) Error( err_overflow );
		else
		if( res == 2 ) Error( err_improper_argument );

		break;

	default:
		/*
			on the stack left an unknown operator but we had to recognize its before
			that means there's an error in our algorithm
		*/
		Error( err_internal_error );
	}
}




/*!
	this method is trying to roll the stack up with the operator's priority

	for example if there are:
		"1 - 2 +" 
	we can subtract "1-2" and the result store on the place where is '1' and copy the last
	operator '+', that means there'll be '-1+' on our stack

	but if there are:
		"1 - 2 *"
	we can't roll the stack up because the operator '*' has greater priority than '-'
*/
void RollUpStackWithOperatorsPriority()
{
	while(	stack_index>=4 &&
			(stack[stack_index-4].type == Item::numerical_value || stack[stack_index-4].type == Item::variable) &&
			stack[stack_index-3].type == Item::mat_operator    &&
			stack[stack_index-2].type == Item::numerical_value &&
			stack[stack_index-1].type == Item::mat_operator    &&
			(
				(
					// the first operator has greater priority
					stack[stack_index-3].moperator.GetPriority() > stack[stack_index-1].moperator.GetPriority()
				) ||
				(
					// or both operators have the same priority and the first operator is not right associative
					stack[stack_index-3].moperator.GetPriority() == stack[stack_index-1].moperator.GetPriority() &&
					stack[stack_index-3].moperator.GetAssoc()    == MatOperator::non_right
				)
			)
		 )
	{
		MakeStandardMathematicOperation(stack[stack_index-4],
										stack[stack_index-3].moperator.GetType(),
										stack[stack_index-2]);


		/*
			copying the last operator and setting the stack pointer to the correct value
		*/
		stack[stack_index-3] = stack[stack_index-1];
		stack_index -= 2;
	}
}


/*!
	this method is trying to roll the stack up without testing any operators

	for example if there are:
		"1 - 2" 
	there'll be "-1" on our stack
*/
void RollUPStack()
{
	while(	stack_index >= 3 &&
			(stack[stack_index-3].type == Item::numerical_value || stack[stack_index-3].type == Item::variable) &&
			stack[stack_index-2].type == Item::mat_operator &&
			stack[stack_index-1].type == Item::numerical_value )
	{
		MakeStandardMathematicOperation(	stack[stack_index-3],
											stack[stack_index-2].moperator.GetType(),
											stack[stack_index-1] );

		stack_index -= 2;
	}
}



/*!
	this method calculate how many parameters there are on the stack
	and the index of the first parameter

	if there are no parameters on the stack this method returns
	'size' equals zero and 'index' pointing after the first bracket
	(on non-existend element)
*/
void HowManyParameters(unsigned int & size, unsigned int & index)
{
	size  = 0;
	index = stack_index;

	for( ; index > 0 && stack[index-1].type != Item::first_bracket ; --index )
	{
		if( stack[index-1].type != Item::numerical_value )
		{
			/*
				this element must be 'numerical_value', if not that means 
				there's an error in our algorithm
			*/
			Error( err_internal_error );
		}

		++size;
	}

	if( index<1 || stack[index-1].type != Item::first_bracket )
	{
		/*
			we haven't put a first bracket on the stack
		*/
		Error( err_unexpected_final_bracket );
	}
}


/*!
	this method is being called when the final bracket ')' is being found

	this method's rolling the stack up, counting how many parameters there are
	on the stack and if there was a function it's calling the function
*/
void RollingUpFinalBracket()
{
	unsigned int amount_of_parameters;
	unsigned int index;

	
	if( stack_index<1 ||
		(stack[stack_index-1].type != Item::numerical_value &&
		 stack[stack_index-1].type != Item::first_bracket)
	  )
	{
		Error( err_unexpected_final_bracket );
		return;
	}
	
	RollUPStack();
	HowManyParameters(amount_of_parameters, index);

	// 'index' will be greater than zero
	// 'amount_of_parameters' can be zero

	if( amount_of_parameters==0 && !stack[index-1].function )
		Error( err_unexpected_final_bracket );


	bool was_sign = stack[index-1].sign;


	if( stack[index-1].function )
	{
		// the result of a function will be on 'stack[index-1]'
		// and then at the end we'll set the correct type (numerical value) of this element
		CallFunction(stack[index-1].function_name, amount_of_parameters, index);
	}
	else
	{
		/*
			there was a normal bracket (not a funcion)
		*/
		if( amount_of_parameters != 1 )
			Error( err_unexpected_semicolon_operator );


		/*
			in the place where is the bracket we put the result
		*/
		stack[index-1] = stack[index];
	}


	/*
		if there was a '-' character before the first bracket
		we change the sign of the expression
	*/
	stack[index-1].sign = false;

	if( was_sign )
		stack[index-1].value.ChangeSign();

	stack[index-1].type = Item::numerical_value;


	/*
		the pointer of the stack will be pointing on the next (non-existing now) element
	*/
	stack_index = index;
}





/*!
	we check wheter there are only numerical value's on the stack
*/
virtual void CheckStackIntegrity()
{
	for(unsigned int i=0 ; i<stack_index; ++i)
	{
		if( stack[i].type != Item::numerical_value )
		{
			/*
				on the stack we must only have 'numerical_value'
				if there is something another that means
				we probably didn't close any of the 'first' brackets
			*/
			Error( err_stack_not_clear );
		}
	}
}



/*!
	the main loop of parsing
*/
virtual void Parse()
{
	while( *pstring )
	{
		const char * old_pstring = pstring;

		if( pstop_calculating && pstop_calculating->WasStopSignal() )
			Error( err_interrupt );

		ReadTokens();
		ReadOperators();
		RollUpStackWithOperatorsPriority();

		if( old_pstring == pstring )
			Error( err_internal_error ); // pstring should be incremented in ReadTokens() or ReadOperators()
	}

	if( stack_index == 0 )
		Error( err_nothing_has_read );

	RollUPStack();
	CheckStackIntegrity();
}



/*!
	this method is called at the end of the parsing process
*/
void NormalizeStack()
{
	if( error!=err_ok || stack_index==0 )
	{
		stack.clear();
		return;
	}
	
	/*
		'stack_index' tell us how many elements there are on the stack,
		we must resize the stack now because 'stack_index' is using only for parsing
		and stack has more (or equal) elements than value of 'stack_index'
	*/
	stack.resize( stack_index );
}


public:


/*!
	the default constructor
*/
Parser(): default_stack_size(100)
{
	pstop_calculating = 0;
	puser_variables   = 0;
	puser_functions   = 0;
	pfunction_local_variables = 0;
	base              = 10;
	deg_rad_grad      = 1;
	error             = err_ok;
	group             = 0;
	comma             = '.';
	comma2            = ',';
	param_sep         = 0;

	CreateFunctionsTable();
	CreateVariablesTable();
	CreateMathematicalOperatorsTable();
}


/*!
	the assignment operator
*/
Parser<ValueType> & operator=(const Parser<ValueType> & p)
{
	pstop_calculating = p.pstop_calculating;
	puser_variables   = p.puser_variables;
	puser_functions   = p.puser_functions;
	pfunction_local_variables = 0;
	base              = p.base;
	deg_rad_grad      = p.deg_rad_grad;
	error             = p.error;
	group             = p.group;
	comma             = p.comma;
	comma2            = p.comma2;
	param_sep         = p.param_sep;

	/*
		we don't have to call 'CreateFunctionsTable()' etc.
		we can only copy these tables
	*/
	functions_table   = p.functions_table;
	variables_table   = p.variables_table;
	operators_table   = p.operators_table;

	visited_variables = p.visited_variables;
	visited_functions = p.visited_functions;

return *this;
}


/*!
 	dctor
 */
virtual ~Parser()
{
}



/*!
	the copying constructor
*/
Parser(const Parser<ValueType> & p): default_stack_size(p.default_stack_size)
{
	operator=(p);
}


/*!
	the new base of mathematic system
	default is 10
*/
void SetBase(int b)
{
	if( b>=2 && b<=16 )
		base = b;
}


/*!
	the unit of angles used in: sin,cos,tan,cot,asin,acos,atan,acot
	0 - deg
	1 - rad (default)
	2 - grad
*/
void SetDegRadGrad(int angle)
{
	if( angle >= 0 && angle <= 2 )
		deg_rad_grad = angle;
}

/*!
	this method sets a pointer to the object which tell us whether we should stop
	calculations
*/
void SetStopObject(const volatile StopCalculating * ps)
{
	pstop_calculating = ps;
}


/*!
	this method sets the new table of user-defined variables
	if you don't want any other variables just put zero value into the 'puser_variables' variable

	(you can have only one table at the same time)
*/
void SetVariables(const Objects * pv)
{
	puser_variables = pv;
}


/*!
	this method sets the new table of user-defined functions
	if you don't want any other functions just put zero value into the 'puser_functions' variable

	(you can have only one table at the same time)
*/
void SetFunctions(const Objects * pf)
{
	puser_functions = pf;
}


/*!
	setting the group character
	default zero (not used)
*/
void SetGroup(int g)
{
	group = g;
}


/*!
	setting the main comma operator and the additional comma operator
	the additional operator can be zero (which means it is not used)
	default are: '.' and ','
*/
void SetComma(int c, int c2 = 0)
{
	comma  = c;
	comma2 = c2;
}


/*!
	setting an additional character which is used as a parameters separator
	the main parameters separator is a semicolon (is used always)

	this character is used also as a global separator
*/
void SetParamSep(int s)
{
	param_sep = s;
}


/*!
	the main method using for parsing string
*/
ErrorCode Parse(const char * str)
{
	stack_index  = 0;
	pstring      = str;
	error        = err_ok;
	calculated   = false;

	try
	{
		stack.resize(default_stack_size);
		Parse();
	}
	catch(ErrorCode c)
	{
		error = c;
		calculated = false;
	}

	NormalizeStack();

return error;
}


/*!
	the main method using for parsing string
*/
ErrorCode Parse(const std::string & str)
{
	return Parse(str.c_str());
}


#ifndef TTMATH_DONT_USE_WCHAR

/*!
	the main method using for parsing string
*/
ErrorCode Parse(const wchar_t * str)
{
	Misc::AssignString(wide_to_ansi, str);

return Parse(wide_to_ansi.c_str());

	// !! wide_to_ansi clearing can be added here
}


/*!
	the main method using for parsing string
*/
ErrorCode Parse(const std::wstring & str)
{
	return Parse(str.c_str());
}

#endif


/*!
	this method returns true is something was calculated
	(at least one mathematical operator was used or a function or variable)
	e.g. true if the string to Parse() looked like this:
	"1+1"
	"2*3"
	"sin(5)"

	if the string was e.g. "678" the result is false
*/
bool Calculated()
{
	return calculated;
}


/*!
	initializing coefficients used when calculating the gamma (or factorial) function
	this speed up the next calculations
	you don't have to call this method explicitly
	these coefficients will be calculated when needed
*/
void InitCGamma()
{
	cgamma.InitAll();
}


};



} // namespace


#endif
