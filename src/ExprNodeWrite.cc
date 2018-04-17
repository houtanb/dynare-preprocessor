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
    case oUminus:
      cout << "-";
      break;
    case oExp:
      cout << "exp";
      break;
    case oLog:
      cout << "log";
      break;
    case oLog10:
      cout << "log10";
      break;
    case oCos:
      cout << "cos";
      break;
    case oSin:
      cout << "sin";
      break;
    case oTan:
      cout << "tan";
      break;
    case oAcos:
      cout << "acos";
      break;
    case oAsin:
      cout << "asin";
      break;
    case oAtan:
      cout << "atan";
      break;
    case oCosh:
      cout << "cosh";
      break;
    case oSinh:
      cout << "sinh";
      break;
    case oTanh:
      cout << "tanh";
      break;
    case oAcosh:
      cout << "acosh";
      break;
    case oAsinh:
      cout << "asinh";
      break;
    case oAtanh:
      cout << "atanh";
      break;
    case oSqrt:
      cout << "sqrt";
      break;
    case oAbs:
      cout << "abs";
      break;
    case oSign:
      cout << "sign";
      break;
    case oSteadyState:
      cout << "steady_state";
      break;
    case oSteadyStateParamDeriv:
      cout << "steady_state_param_deriv";
      break;
    case oSteadyStateParam2ndDeriv:
      cout << "steady_state_2nd_param_deriv";
      break;
    case oExpectation:
      cout << "expectation";
      break;
    case oErf:
      cout << "erf";
      break;
    case oDiff:
      cout << "diff";
      break;
    case oAdl:
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
  if (op_code == oMax || op_code == oMin)
    {
      switch (op_code)
        {
        case oMax:
          cout << "max(";
          break;
        case oMin:
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

  if (op_code == oPowerDeriv)
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
    case oPlus:
      cout << "+";
      break;
    case oMinus:
      cout << "-";
      break;
    case oTimes:
      cout << "*";
      break;
    case oDivide:
      cout << "/";
      break;
    case oPower:
      cout << "^";
      break;
    case oLess:
      cout << "<";
      break;
    case oGreater:
      cout << ">";
      break;
    case oLessEqual:
      cout << "<=";
      break;
    case oGreaterEqual:
      cout << ">=";
      break;
    case oEqualEqual:
      cout << "==";
      break;
    case oDifferent:
      cout << "~=";
      break;
    case oEqual:
      cout << "=";
      break;
    case oPowerDeriv:
    case oMax:
    case oMin:
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
