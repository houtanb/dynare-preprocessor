/*
 * Copyright (C) 2010 Dynare Team
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

#include <cassert>
#include <algorithm>

#include "SteadyStateModel.hh"

SteadyStateModel::SteadyStateModel(SymbolTable &symbol_table_arg, NumericalConstants &num_constants, ExternalFunctionsTable &external_functions_table_arg) :
  DataTree(symbol_table_arg, num_constants, external_functions_table)
{
}

void
SteadyStateModel::addDefinition(int symb_id, NodeID expr) throw (UndefinedVariableException, AlreadyDefinedException)
{
  assert(symbol_table.getType(symb_id) == eEndogenous
         || symbol_table.getType(symb_id) == eModFileLocalVariable);

  // Check that symbol is not already defined
  if (find(recursive_order.begin(), recursive_order.end(), symb_id)
      != recursive_order.end())
    throw AlreadyDefinedException(symbol_table.getName(symb_id));

  // Check that expression has no undefined symbol
  set<pair<int, int> > used_symbols;
  expr->collectVariables(eEndogenous, used_symbols);
  expr->collectVariables(eModFileLocalVariable, used_symbols);
  for(set<pair<int, int> >::const_iterator it = used_symbols.begin();
      it != used_symbols.end(); it++)
    if (find(recursive_order.begin(), recursive_order.end(), it->first)
        == recursive_order.end())
      throw UndefinedVariableException(symbol_table.getName(it->first));

  // Add the variable
  recursive_order.push_back(symb_id);
  def_table[symb_id] = AddEqual(AddVariable(symb_id), expr);
}

void
SteadyStateModel::writeSteadyStateFile(const string &basename) const
{
  if (recursive_order.size() == 0)
    return;

  string filename = basename + "_steadystate.m";

  ofstream output;
  output.open(filename.c_str(), ios::out | ios::binary);
  if (!output.is_open())
    {
      cerr << "ERROR: Can't open file " << filename << " for writing" << endl;
      exit(EXIT_FAILURE);
    }

  output << "function [ys_, check_] = " << basename << "_steadystate(ys_orig_, exo_)" << endl
         << "% Steady state generated by Dynare preprocessor" << endl
         << "    global M_" << endl
         << "    ys_=zeros(" << symbol_table.orig_endo_nbr() << ",1);" << endl;

  for(size_t i = 0; i < recursive_order.size(); i++)
    {
      output << "    ";
      map<int, NodeID>::const_iterator it = def_table.find(recursive_order[i]);
      it->second->writeOutput(output, oSteadyStateFile);
      output << ";" << endl;
    }
  output << "    check_=0;" << endl
         << "end" << endl;
}

