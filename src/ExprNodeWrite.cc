#include "ExprNode.hh"
#include "DataTree.hh"
#include "ModFile.hh"

void
ExprNode::write() const
{
}

void
NumConstNode::write() const
{
  cout << datatree.num_constants.get(id);
}

void
VariableNode::write() const
{
  cout << datatree.symbol_table.getName(symb_id);
  if (lag != 0)
    cout << "(" << lag << ")";
}

void
UnaryOpNode::write() const
{
   switch (op_code)
    {
    case UnaryOpcode::uminus:
      cout << "-";
      break;
    case UnaryOpcode::exp:
      cout << "exp";
      break;
    case UnaryOpcode::log:
      cout << "log";
      break;
    case UnaryOpcode::log10:
      cout << "log10";
      break;
    case UnaryOpcode::cos:
      cout << "cos";
      break;
    case UnaryOpcode::sin:
      cout << "sin";
      break;
    case UnaryOpcode::tan:
      cout << "tan";
      break;
    case UnaryOpcode::acos:
      cout << "acos";
      break;
    case UnaryOpcode::asin:
      cout << "asin";
      break;
    case UnaryOpcode::atan:
      cout << "atan";
      break;
    case UnaryOpcode::cosh:
      cout << "cosh";
      break;
    case UnaryOpcode::sinh:
      cout << "sinh";
      break;
    case UnaryOpcode::tanh:
      cout << "tanh";
      break;
    case UnaryOpcode::acosh:
      cout << "acosh";
      break;
    case UnaryOpcode::asinh:
      cout << "asinh";
      break;
    case UnaryOpcode::atanh:
      cout << "atanh";
      break;
    case UnaryOpcode::sqrt:
      cout << "sqrt";
      break;
    case UnaryOpcode::abs:
      cout << "abs";
      break;
    case UnaryOpcode::sign:
      cout << "sign";
      break;
    case UnaryOpcode::steadyState:
      cout << "steady_state";
      break;
    case UnaryOpcode::steadyStateParamDeriv:
      cout << "steady_state_param_deriv";
      break;
    case UnaryOpcode::steadyStateParam2ndDeriv:
      cout << "steady_state_2nd_param_deriv";
      break;
    case UnaryOpcode::expectation:
      cout << "expectation";
      break;
    case UnaryOpcode::erf:
      cout << "erf";
      break;
    case UnaryOpcode::diff:
      cout << "diff";
      break;
    case UnaryOpcode::adl:
      cout << "adl";
      break;
    }

   cout << "(";

   // Write argument
   arg->write();

   cout << ")";
}


void
BinaryOpNode::write() const
{
  if (op_code == BinaryOpcode::max || op_code == BinaryOpcode::min)
    {
      switch (op_code)
        {
        case BinaryOpcode::max:
          cout << "max(";
          break;
        case BinaryOpcode::min:
          cout << "min(";
          break;
        default:
          ;
        }
      arg1->write();
      cout << ",";
      arg2->write();
      cout << ")";
      return;
    }

  if (op_code == BinaryOpcode::powerDeriv)
    {
      cout << "power_deriv(";
      arg1->write();
      cout << ",";
      arg2->write();
      cout << ")";
      return;
    }

  cout << "(";

  // Write left argument
  arg1->write();

  switch (op_code)
    {
    case BinaryOpcode::plus:
      cout << "+";
      break;
    case BinaryOpcode::minus:
      cout << "-";
      break;
    case BinaryOpcode::times:
      cout << "*";
      break;
    case BinaryOpcode::divide:
      cout << "/";
      break;
    case BinaryOpcode::power:
      cout << "^";
      break;
    case BinaryOpcode::less:
      cout << "<";
      break;
    case BinaryOpcode::greater:
      cout << ">";
      break;
    case BinaryOpcode::lessEqual:
      cout << "<=";
      break;
    case BinaryOpcode::greaterEqual:
      cout << ">=";
      break;
    case BinaryOpcode::equalEqual:
      cout << "==";
      break;
    case BinaryOpcode::different:
      cout << "~=";
      break;
    case BinaryOpcode::equal:
      cout << "=";
      break;
    case BinaryOpcode::powerDeriv:
    case BinaryOpcode::max:
    case BinaryOpcode::min:
      // handled above...
      break;
      ;
    }

  // Write right argument
  arg2->write();

  cout << ")";
}

void
TrinaryOpNode::write() const
{
}

void
AbstractExternalFunctionNode::write() const
{
}
/*
void
ExternalFunctionNode::write() const
{
}

void
FirstDerivExternalFunctionNode::write() const
{
}

void
SecondDerivExternalFunctionNode::write() const
{
}
*/

void
PacExpectationNode::write() const
{
}

void
VarExpectationNode::write() const
{
}
