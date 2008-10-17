/*
 * Copyright (C) 2003-2008 Dynare Team
 *
 * This file is part of Dynare.
 *
 * Dynare is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Dynare is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Dynare.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <cstdlib>
#include <iostream>

#include "DataTree.hh"

DataTree::DataTree(SymbolTable &symbol_table_arg, NumericalConstants &num_constants_arg) :
  symbol_table(symbol_table_arg),
  num_constants(num_constants_arg),
  node_counter(0),
  variable_table(symbol_table_arg)
{
  Zero = AddNumConstant("0");
  One = AddNumConstant("1");

  MinusOne = AddUMinus(One);
}

DataTree::~DataTree()
{
  for (node_list_type::iterator it = node_list.begin(); it != node_list.end(); it++)
    delete *it;
}

NodeID
DataTree::AddNumConstant(const string &value)
{
  int id = num_constants.AddConstant(value);

  num_const_node_map_type::iterator it = num_const_node_map.find(id);
  if (it != num_const_node_map.end())
    return it->second;
  else
    return new NumConstNode(*this, id);
}

NodeID
DataTree::AddVariable(const string &name, int lag)
{
  int symb_id = symbol_table.getID(name);
  SymbolType type = symbol_table.getType(name);

  variable_node_map_type::iterator it = variable_node_map.find(make_pair(make_pair(symb_id, type), lag));
  if (it != variable_node_map.end())
    return it->second;
  else
    return new VariableNode(*this, symb_id, type, lag);
}

NodeID
DataTree::AddPlus(NodeID iArg1, NodeID iArg2)
{
  if (iArg1 != Zero && iArg2 != Zero)
    {
      // Simplify x+(-y) in x-y
      UnaryOpNode *uarg2 = dynamic_cast<UnaryOpNode *>(iArg2);
      if (uarg2 != NULL && uarg2->op_code == oUminus)
        return AddMinus(iArg1, uarg2->arg);

      // To treat commutativity of "+"
      // Nodes iArg1 and iArg2 are sorted by index
      if (iArg1->idx > iArg2->idx)
        {
          NodeID tmp = iArg1;
          iArg1 = iArg2;
          iArg2 = tmp;
        }
      return AddBinaryOp(iArg1, oPlus, iArg2);
    }
  else if (iArg1 != Zero)
    return iArg1;
  else if (iArg2 != Zero)
    return iArg2;
  else
    return Zero;
}

NodeID
DataTree::AddMinus(NodeID iArg1, NodeID iArg2)
{
  if (iArg1 != Zero && iArg2 != Zero)
    return AddBinaryOp(iArg1, oMinus, iArg2);
  else if (iArg1 != Zero)
    return iArg1;
  else if (iArg2 != Zero)
    return AddUMinus(iArg2);
  else
    return Zero;
}

NodeID
DataTree::AddUMinus(NodeID iArg1)
{
  if (iArg1 != Zero)
    {
      // Simplify -(-x) in x
      UnaryOpNode *uarg = dynamic_cast<UnaryOpNode *>(iArg1);
      if (uarg != NULL && uarg->op_code == oUminus)
        return uarg->arg;

      return AddUnaryOp(oUminus, iArg1);
    }
  else
    return Zero;
}

NodeID
DataTree::AddTimes(NodeID iArg1, NodeID iArg2)
{
  if (iArg1 == MinusOne)
    return AddUMinus(iArg2);
  else if (iArg2 == MinusOne)
    return AddUMinus(iArg1);
  else if (iArg1 != Zero && iArg1 != One && iArg2 != Zero && iArg2 != One)
    {
      // To treat commutativity of "*"
      // Nodes iArg1 and iArg2 are sorted by index
      if (iArg1->idx > iArg2->idx)
        {
          NodeID tmp = iArg1;
          iArg1 = iArg2;
          iArg2 = tmp;
        }
      return AddBinaryOp(iArg1, oTimes, iArg2);
    }
  else if (iArg1 != Zero && iArg1 != One && iArg2 == One)
    return iArg1;
  else if (iArg2 != Zero && iArg2 != One && iArg1 == One)
    return iArg2;
  else if (iArg2 == One && iArg1 == One)
    return One;
  else
    return Zero;
}

NodeID
DataTree::AddDivide(NodeID iArg1, NodeID iArg2)
{
  if (iArg1 != Zero && iArg2 != Zero && iArg2 != One)
    return AddBinaryOp(iArg1, oDivide, iArg2);
  else if (iArg2 == One)
    return iArg1;
  else if (iArg1 == Zero && iArg2 != Zero)
    return Zero;
  else
    {
      cerr << "Division by zero!" << endl;
      exit(-1);
    }
}

NodeID
DataTree::AddLess(NodeID iArg1, NodeID iArg2)
{
  return AddBinaryOp(iArg1, oLess, iArg2);
}

NodeID
DataTree::AddGreater(NodeID iArg1, NodeID iArg2)
{
  return AddBinaryOp(iArg1, oGreater, iArg2);
}

NodeID
DataTree::AddLessEqual(NodeID iArg1, NodeID iArg2)
{
  return AddBinaryOp(iArg1, oLessEqual, iArg2);
}

NodeID
DataTree::AddGreaterEqual(NodeID iArg1, NodeID iArg2)
{
  return AddBinaryOp(iArg1, oGreaterEqual, iArg2);
}

NodeID
DataTree::AddEqualEqual(NodeID iArg1, NodeID iArg2)
{
  return AddBinaryOp(iArg1, oEqualEqual, iArg2);
}

NodeID
DataTree::AddDifferent(NodeID iArg1, NodeID iArg2)
{
  return AddBinaryOp(iArg1, oDifferent, iArg2);
}

NodeID
DataTree::AddPower(NodeID iArg1, NodeID iArg2)
{
  if (iArg1 != Zero && iArg2 != Zero && iArg2 != One)
    return AddBinaryOp(iArg1, oPower, iArg2);
  else if (iArg2 == One)
    return iArg1;
  else if (iArg2 == Zero)
    return One;
  else
    return Zero;
}

NodeID
DataTree::AddExp(NodeID iArg1)
{
  if (iArg1 != Zero)
    return AddUnaryOp(oExp, iArg1);
  else
    return One;
}

NodeID
DataTree::AddLog(NodeID iArg1)
{
  if (iArg1 != Zero && iArg1 != One)
    return AddUnaryOp(oLog, iArg1);
  else if (iArg1 == One)
    return Zero;
  else
    {
      cerr << "log(0) isn't available" << endl;
      exit(-1);
    }
}

NodeID DataTree::AddLog10(NodeID iArg1)
{
  if (iArg1 != Zero && iArg1 != One)
    return AddUnaryOp(oLog10, iArg1);
  else if (iArg1 == One)
    return Zero;
  else
    {
      cerr << "log10(0) isn't available" << endl;
      exit(-1);
    }
}

NodeID
DataTree::AddCos(NodeID iArg1)
{
  if (iArg1 != Zero)
    return AddUnaryOp(oCos, iArg1);
  else
    return One;
}

NodeID
DataTree::AddSin(NodeID iArg1)
{
  if (iArg1 != Zero)
    return AddUnaryOp(oSin, iArg1);
  else
    return Zero;
}

NodeID
DataTree::AddTan(NodeID iArg1)
{
  if (iArg1 != Zero)
    return AddUnaryOp(oTan, iArg1);
  else
    return Zero;
}

NodeID
DataTree::AddACos(NodeID iArg1)
{
  if (iArg1 != One)
    return AddUnaryOp(oAcos, iArg1);
  else
    return Zero;
}

NodeID
DataTree::AddASin(NodeID iArg1)
{
  if (iArg1 != Zero)
    return AddUnaryOp(oAsin, iArg1);
  else
    return Zero;
}

NodeID
DataTree::AddATan(NodeID iArg1)
{
  if (iArg1 != Zero)
    return AddUnaryOp(oAtan, iArg1);
  else
    return Zero;
}

NodeID
DataTree::AddCosH(NodeID iArg1)
{
  if (iArg1 != Zero)
    return AddUnaryOp(oCosh, iArg1);
  else
    return One;
}

NodeID
DataTree::AddSinH(NodeID iArg1)
{
  if (iArg1 != Zero)
    return AddUnaryOp(oSinh, iArg1);
  else
    return Zero;
}

NodeID
DataTree::AddTanH(NodeID iArg1)
{
  if (iArg1 != Zero)
    return AddUnaryOp(oTanh, iArg1);
  else
    return Zero;
}

NodeID
DataTree::AddACosH(NodeID iArg1)
{
  if (iArg1 != One)
    return AddUnaryOp(oAcosh, iArg1);
  else
    return Zero;
}

NodeID
DataTree::AddASinH(NodeID iArg1)
{
  if (iArg1 != Zero)
    return AddUnaryOp(oAsinh, iArg1);
  else
    return Zero;
}

NodeID
DataTree::AddATanH(NodeID iArg1)
{
  if (iArg1 != Zero)
    return AddUnaryOp(oAtanh, iArg1);
  else
    return Zero;
}

NodeID
DataTree::AddSqRt(NodeID iArg1)
{
  if (iArg1 != Zero)
    return AddUnaryOp(oSqrt, iArg1);
  else
    return Zero;
}

NodeID
DataTree::AddMaX(NodeID iArg1, NodeID iArg2)
{
    return AddBinaryOp(iArg1, oMax, iArg2);
}

NodeID
DataTree::AddMin(NodeID iArg1, NodeID iArg2)
{
    return AddBinaryOp(iArg1, oMin, iArg2);
}

NodeID
DataTree::AddNormcdf(NodeID iArg1, NodeID iArg2, NodeID iArg3)
{
  return AddTrinaryOp(iArg1, oNormcdf, iArg2, iArg3);
}

NodeID
DataTree::AddEqual(NodeID iArg1, NodeID iArg2)
{
  return AddBinaryOp(iArg1, oEqual, iArg2);
}

void
DataTree::AddLocalParameter(const string &name, NodeID value) throw (LocalParameterException)
{
  int id = symbol_table.getID(name);

  // Throw an exception if symbol already declared
  map<int, NodeID>::iterator it = local_variables_table.find(id);
  if (it != local_variables_table.end())
    throw LocalParameterException(name);

  local_variables_table[id] = value;
}

NodeID
DataTree::AddUnknownFunction(const string &function_name, const vector<NodeID> &arguments)
{
  if (symbol_table.getType(function_name) != eUnknownFunction)
    {
      cerr << "Symbol " << function_name << " is not a function name!";
      exit(-1);
    }

  int id = symbol_table.getID(function_name);

  return new UnknownFunctionNode(*this, id, arguments);
}
