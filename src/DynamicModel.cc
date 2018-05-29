/*
 * Copyright (C) 2003-2018 Dynare Team
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

#include <iostream>
#include <cmath>
#include <cstdlib>
#include <cassert>
#include <cstdio>
#include <cerrno>
#include <algorithm>
#include <iterator>
#include "DynamicModel.hh"

// For mkdir() and chdir()
#ifdef _WIN32
# include <direct.h>
#else
# include <unistd.h>
# include <sys/stat.h>
# include <sys/types.h>
#endif

DynamicModel::DynamicModel(SymbolTable &symbol_table_arg,
                           NumericalConstants &num_constants_arg,
                           ExternalFunctionsTable &external_functions_table_arg) :
  ModelTree(symbol_table_arg, num_constants_arg, external_functions_table_arg),
  max_lag(0), max_lead(0),
  max_endo_lag(0), max_endo_lead(0),
  max_exo_lag(0), max_exo_lead(0),
  max_exo_det_lag(0), max_exo_det_lead(0),
  max_lag_orig(0), max_lead_orig(0),
  max_endo_lag_orig(0), max_endo_lead_orig(0),
  max_exo_lag_orig(0), max_exo_lead_orig(0),
  max_exo_det_lag_orig(0), max_exo_det_lead_orig(0),
  dynJacobianColsNbr(0),
  global_temporary_terms(true)
{
}

VariableNode *
DynamicModel::AddVariable(int symb_id, int lag)
{
  return AddVariableInternal(symb_id, lag);
}

void
DynamicModel::compileDerivative(ofstream &code_file, unsigned int &instruction_number, int eq, int symb_id, int lag, const map_idx_t &map_idx) const
{
  first_derivatives_t::const_iterator it = first_derivatives.find(make_pair(eq, getDerivID(symbol_table.getID(eEndogenous, symb_id), lag)));
  if (it != first_derivatives.end())
    (it->second)->compile(code_file, instruction_number, false, temporary_terms, map_idx, true, false);
  else
    {
      FLDZ_ fldz;
      fldz.write(code_file, instruction_number);
    }
}

void
DynamicModel::compileChainRuleDerivative(ofstream &code_file, unsigned int &instruction_number, int eqr, int varr, int lag, const map_idx_t &map_idx) const
{
  map<pair<int, pair<int, int> >, expr_t>::const_iterator it = first_chain_rule_derivatives.find(make_pair(eqr, make_pair(varr, lag)));
  if (it != first_chain_rule_derivatives.end())
    (it->second)->compile(code_file, instruction_number, false, temporary_terms, map_idx, true, false);
  else
    {
      FLDZ_ fldz;
      fldz.write(code_file, instruction_number);
    }
}

void
DynamicModel::computeTemporaryTermsOrdered()
{
  map<expr_t, pair<int, int> > first_occurence;
  map<expr_t, int> reference_count;
  BinaryOpNode *eq_node;
  first_derivatives_t::const_iterator it;
  first_chain_rule_derivatives_t::const_iterator it_chr;
  ostringstream tmp_s;
  v_temporary_terms.clear();
  map_idx.clear();

  unsigned int nb_blocks = getNbBlocks();
  v_temporary_terms = vector<vector<temporary_terms_t> >(nb_blocks);
  v_temporary_terms_inuse = vector<temporary_terms_inuse_t>(nb_blocks);
  temporary_terms.clear();

  if (!global_temporary_terms)
    {
      for (unsigned int block = 0; block < nb_blocks; block++)
        {
          reference_count.clear();
          temporary_terms.clear();
          unsigned int block_size = getBlockSize(block);
          unsigned int block_nb_mfs = getBlockMfs(block);
          unsigned int block_nb_recursives = block_size - block_nb_mfs;
          v_temporary_terms[block] = vector<temporary_terms_t>(block_size);
          for (unsigned int i = 0; i < block_size; i++)
            {
              if (i < block_nb_recursives && isBlockEquationRenormalized(block, i))
                getBlockEquationRenormalizedExpr(block, i)->computeTemporaryTerms(reference_count, temporary_terms, first_occurence, block, v_temporary_terms,  i);
              else
                {
                  eq_node = (BinaryOpNode *) getBlockEquationExpr(block, i);
                  eq_node->computeTemporaryTerms(reference_count, temporary_terms, first_occurence, block, v_temporary_terms,  i);
                }
            }
          for (block_derivatives_equation_variable_laglead_nodeid_t::const_iterator it = blocks_derivatives[block].begin(); it != (blocks_derivatives[block]).end(); it++)
            {
              expr_t id = it->second.second;
              id->computeTemporaryTerms(reference_count, temporary_terms, first_occurence, block, v_temporary_terms,  block_size-1);
            }
          for (derivative_t::const_iterator it = derivative_endo[block].begin(); it != derivative_endo[block].end(); it++)
            it->second->computeTemporaryTerms(reference_count, temporary_terms, first_occurence, block, v_temporary_terms,  block_size-1);
          for (derivative_t::const_iterator it = derivative_other_endo[block].begin(); it != derivative_other_endo[block].end(); it++)
            it->second->computeTemporaryTerms(reference_count, temporary_terms, first_occurence, block, v_temporary_terms,  block_size-1);
          set<int> temporary_terms_in_use;
          temporary_terms_in_use.clear();
          v_temporary_terms_inuse[block] = temporary_terms_in_use;
        }
    }
  else
    {
      for (unsigned int block = 0; block < nb_blocks; block++)
        {
          // Compute the temporary terms reordered
          unsigned int block_size = getBlockSize(block);
          unsigned int block_nb_mfs = getBlockMfs(block);
          unsigned int block_nb_recursives = block_size - block_nb_mfs;
          v_temporary_terms[block] = vector<temporary_terms_t>(block_size);
          for (unsigned int i = 0; i < block_size; i++)
            {
              if (i < block_nb_recursives && isBlockEquationRenormalized(block, i))
                getBlockEquationRenormalizedExpr(block, i)->computeTemporaryTerms(reference_count, temporary_terms, first_occurence, block, v_temporary_terms,  i);
              else
                {
                  eq_node = (BinaryOpNode *) getBlockEquationExpr(block, i);
                  eq_node->computeTemporaryTerms(reference_count, temporary_terms, first_occurence, block, v_temporary_terms, i);
                }
            }
          for (block_derivatives_equation_variable_laglead_nodeid_t::const_iterator it = blocks_derivatives[block].begin(); it != (blocks_derivatives[block]).end(); it++)
            {
              expr_t id = it->second.second;
              id->computeTemporaryTerms(reference_count, temporary_terms, first_occurence, block, v_temporary_terms, block_size-1);
            }
          for (derivative_t::const_iterator it = derivative_endo[block].begin(); it != derivative_endo[block].end(); it++)
            it->second->computeTemporaryTerms(reference_count, temporary_terms, first_occurence, block, v_temporary_terms, block_size-1);
          for (derivative_t::const_iterator it = derivative_other_endo[block].begin(); it != derivative_other_endo[block].end(); it++)
            it->second->computeTemporaryTerms(reference_count, temporary_terms, first_occurence, block, v_temporary_terms, block_size-1);
        }
      for (unsigned int block = 0; block < nb_blocks; block++)
        {
          // Collect the temporary terms reordered
          unsigned int block_size = getBlockSize(block);
          unsigned int block_nb_mfs = getBlockMfs(block);
          unsigned int block_nb_recursives = block_size - block_nb_mfs;
          set<int> temporary_terms_in_use;
          for (unsigned int i = 0; i < block_size; i++)
            {
              if (i < block_nb_recursives && isBlockEquationRenormalized(block, i))
                getBlockEquationRenormalizedExpr(block, i)->collectTemporary_terms(temporary_terms, temporary_terms_in_use, block);
              else
                {
                  eq_node = (BinaryOpNode *) getBlockEquationExpr(block, i);
                  eq_node->collectTemporary_terms(temporary_terms, temporary_terms_in_use, block);
                }
            }
          for (block_derivatives_equation_variable_laglead_nodeid_t::const_iterator it = blocks_derivatives[block].begin(); it != (blocks_derivatives[block]).end(); it++)
            {
              expr_t id = it->second.second;
              id->collectTemporary_terms(temporary_terms, temporary_terms_in_use, block);
            }
          for (derivative_t::const_iterator it = derivative_endo[block].begin(); it != derivative_endo[block].end(); it++)
            it->second->collectTemporary_terms(temporary_terms, temporary_terms_in_use, block);
          for (derivative_t::const_iterator it = derivative_other_endo[block].begin(); it != derivative_other_endo[block].end(); it++)
            it->second->collectTemporary_terms(temporary_terms, temporary_terms_in_use, block);
          for (derivative_t::const_iterator it = derivative_exo[block].begin(); it != derivative_exo[block].end(); it++)
            it->second->collectTemporary_terms(temporary_terms, temporary_terms_in_use, block);
          for (derivative_t::const_iterator it = derivative_exo_det[block].begin(); it != derivative_exo_det[block].end(); it++)
            it->second->collectTemporary_terms(temporary_terms, temporary_terms_in_use, block);
          v_temporary_terms_inuse[block] = temporary_terms_in_use;
        }
      computeTemporaryTermsMapping();
    }
}

void
DynamicModel::computeTemporaryTermsMapping()
{
  // Add a mapping form node ID to temporary terms order
  int j = 0;
  for (temporary_terms_t::const_iterator it = temporary_terms.begin();
       it != temporary_terms.end(); it++)
    map_idx[(*it)->idx] = j++;
}

void
DynamicModel::writeModelEquationsOrdered_M(const string &dynamic_basename) const
{
  string tmp_s, sps;
  ostringstream tmp_output, tmp1_output, global_output;
  expr_t lhs = NULL, rhs = NULL;
  BinaryOpNode *eq_node;
  ostringstream Ufoss;
  vector<string> Uf(symbol_table.endo_nbr(), "");
  map<expr_t, int> reference_count;
  temporary_terms_t local_temporary_terms;
  ofstream  output;
  int nze, nze_exo, nze_exo_det, nze_other_endo;
  vector<int> feedback_variables;
  ExprNodeOutputType local_output_type;
  Ufoss.str("");

  local_output_type = oMatlabDynamicModelSparse;
  if (global_temporary_terms)
    local_temporary_terms = temporary_terms;

  //----------------------------------------------------------------------
  //For each block
  for (unsigned int block = 0; block < getNbBlocks(); block++)
    {

      //recursive_variables.clear();
      feedback_variables.clear();
      //For a block composed of a single equation determines wether we have to evaluate or to solve the equation
      nze = derivative_endo[block].size();
      nze_other_endo = derivative_other_endo[block].size();
      nze_exo = derivative_exo[block].size();
      nze_exo_det = derivative_exo_det[block].size();
      BlockSimulationType simulation_type = getBlockSimulationType(block);
      unsigned int block_size = getBlockSize(block);
      unsigned int block_mfs = getBlockMfs(block);
      unsigned int block_recursive = block_size - block_mfs;
      deriv_node_temp_terms_t tef_terms;
      local_output_type = oMatlabDynamicModelSparse;
      if (global_temporary_terms)
        local_temporary_terms = temporary_terms;

      int prev_lag;
      unsigned int prev_var, count_col, count_col_endo, count_col_exo, count_col_exo_det, count_col_other_endo;
      map<pair<int, pair<int, int> >, expr_t> tmp_block_endo_derivative;
      for (block_derivatives_equation_variable_laglead_nodeid_t::const_iterator it = blocks_derivatives[block].begin(); it != (blocks_derivatives[block]).end(); it++)
        tmp_block_endo_derivative[make_pair(it->second.first, make_pair(it->first.second, it->first.first))] = it->second.second;
      prev_var = 999999999;
      prev_lag = -9999999;
      count_col_endo = 0;
      for (map<pair<int, pair<int, int> >, expr_t>::const_iterator it = tmp_block_endo_derivative.begin(); it != tmp_block_endo_derivative.end(); it++)
        {
          int lag = it->first.first;
          unsigned int var = it->first.second.first;
          if (var != prev_var || lag != prev_lag)
            {
              prev_var = var;
              prev_lag = lag;
              count_col_endo++;
            }
        }
      map<pair<int, pair<int, int> >, expr_t> tmp_block_exo_derivative;
      for (derivative_t::const_iterator it = derivative_exo[block].begin(); it != (derivative_exo[block]).end(); it++)
        tmp_block_exo_derivative[make_pair(it->first.first, make_pair(it->first.second.second, it->first.second.first))] = it->second;
      prev_var = 999999999;
      prev_lag = -9999999;
      count_col_exo = 0;
      for (map<pair<int, pair<int, int> >, expr_t>::const_iterator it = tmp_block_exo_derivative.begin(); it != tmp_block_exo_derivative.end(); it++)
        {
          int lag = it->first.first;
          unsigned int var = it->first.second.first;
          if (var != prev_var || lag != prev_lag)
            {
              prev_var = var;
              prev_lag = lag;
              count_col_exo++;
            }
        }
      map<pair<int, pair<int, int> >, expr_t> tmp_block_exo_det_derivative;
      for (derivative_t::const_iterator it = derivative_exo_det[block].begin(); it != (derivative_exo_det[block]).end(); it++)
        tmp_block_exo_det_derivative[make_pair(it->first.first, make_pair(it->first.second.second, it->first.second.first))] = it->second;
      prev_var = 999999999;
      prev_lag = -9999999;
      count_col_exo_det = 0;
      for (map<pair<int, pair<int, int> >, expr_t>::const_iterator it = tmp_block_exo_derivative.begin(); it != tmp_block_exo_derivative.end(); it++)
        {
          int lag = it->first.first;
          unsigned int var = it->first.second.first;
          if (var != prev_var || lag != prev_lag)
            {
              prev_var = var;
              prev_lag = lag;
              count_col_exo_det++;
            }
        }
      map<pair<int, pair<int, int> >, expr_t> tmp_block_other_endo_derivative;
      for (derivative_t::const_iterator it = derivative_other_endo[block].begin(); it != (derivative_other_endo[block]).end(); it++)
        tmp_block_other_endo_derivative[make_pair(it->first.first, make_pair(it->first.second.second, it->first.second.first))] = it->second;
      prev_var = 999999999;
      prev_lag = -9999999;
      count_col_other_endo = 0;
      for (map<pair<int, pair<int, int> >, expr_t>::const_iterator it = tmp_block_other_endo_derivative.begin(); it != tmp_block_other_endo_derivative.end(); it++)
        {
          int lag = it->first.first;
          unsigned int var = it->first.second.first;
          if (var != prev_var || lag != prev_lag)
            {
              prev_var = var;
              prev_lag = lag;
              count_col_other_endo++;
            }
        }

      tmp1_output.str("");
      tmp1_output << dynamic_basename << "_" << block+1 << ".m";
      output.open(tmp1_output.str().c_str(), ios::out | ios::binary);
      output << "%\n";
      output << "% " << tmp1_output.str() << " : Computes dynamic model for Dynare\n";
      output << "%\n";
      output << "% Warning : this file is generated automatically by Dynare\n";
      output << "%           from model file (.mod)\n\n";
      output << "%/\n";
      if (simulation_type == EVALUATE_BACKWARD || simulation_type == EVALUATE_FORWARD)
        {
          output << "function [y, g1, g2, g3, varargout] = " << dynamic_basename << "_" << block+1 << "(y, x, params, steady_state, jacobian_eval, y_kmin, periods)\n";
        }
      else if (simulation_type == SOLVE_FORWARD_COMPLETE || simulation_type == SOLVE_BACKWARD_COMPLETE)
        output << "function [residual, y, g1, g2, g3, varargout] = " << dynamic_basename << "_" << block+1 << "(y, x, params, steady_state, it_, jacobian_eval)\n";
      else if (simulation_type == SOLVE_BACKWARD_SIMPLE || simulation_type == SOLVE_FORWARD_SIMPLE)
        output << "function [residual, y, g1, g2, g3, varargout] = " << dynamic_basename << "_" << block+1 << "(y, x, params, steady_state, it_, jacobian_eval)\n";
      else
        output << "function [residual, y, g1, g2, g3, b, varargout] = " << dynamic_basename << "_" << block+1 << "(y, x, params, steady_state, periods, jacobian_eval, y_kmin, y_size, Periods)\n";
      BlockType block_type;
      if (simulation_type == SOLVE_TWO_BOUNDARIES_COMPLETE || simulation_type == SOLVE_TWO_BOUNDARIES_SIMPLE)
        block_type = SIMULTAN;
      else if (simulation_type == SOLVE_FORWARD_COMPLETE || simulation_type == SOLVE_BACKWARD_COMPLETE)
        block_type = SIMULTANS;
      else if ((simulation_type == SOLVE_FORWARD_SIMPLE || simulation_type == SOLVE_BACKWARD_SIMPLE
                || simulation_type == EVALUATE_BACKWARD    || simulation_type == EVALUATE_FORWARD)
               && getBlockFirstEquation(block) < prologue)
        block_type = PROLOGUE;
      else if ((simulation_type == SOLVE_FORWARD_SIMPLE || simulation_type == SOLVE_BACKWARD_SIMPLE
                || simulation_type == EVALUATE_BACKWARD    || simulation_type == EVALUATE_FORWARD)
               && getBlockFirstEquation(block) >= equations.size() - epilogue)
        block_type = EPILOGUE;
      else
        block_type = SIMULTANS;
      output << "  % ////////////////////////////////////////////////////////////////////////" << endl
             << "  % //" << string("                     Block ").substr(int (log10(block + 1))) << block + 1 << " " << BlockType0(block_type)
             << "          //" << endl
             << "  % //                     Simulation type "
             << BlockSim(simulation_type) << "  //" << endl
             << "  % ////////////////////////////////////////////////////////////////////////" << endl;
      //The Temporary terms
      if (simulation_type == EVALUATE_BACKWARD || simulation_type == EVALUATE_FORWARD)
        {
          output << "  if(jacobian_eval)\n";
          output << "    g1 = spalloc(" << block_mfs  << ", " << count_col_endo << ", " << nze << ");\n";
          output << "    g1_x=spalloc(" << block_size << ", " << count_col_exo  << ", " << nze_exo << ");\n";
          output << "    g1_xd=spalloc(" << block_size << ", " << count_col_exo_det  << ", " << nze_exo_det << ");\n";
          output << "    g1_o=spalloc(" << block_size << ", " << count_col_other_endo << ", " << nze_other_endo << ");\n";
          output << "  end;\n";
        }
      else
        {
          output << "  if(jacobian_eval)\n";
          output << "    g1 = spalloc(" << block_size << ", " << count_col_endo << ", " << nze << ");\n";
          output << "    g1_x=spalloc(" << block_size << ", " << count_col_exo  << ", " << nze_exo << ");\n";
          output << "    g1_xd=spalloc(" << block_size << ", " << count_col_exo_det  << ", " << nze_exo_det << ");\n";
          output << "    g1_o=spalloc(" << block_size << ", " << count_col_other_endo << ", " << nze_other_endo << ");\n";
          output << "  else\n";
          if (simulation_type == SOLVE_TWO_BOUNDARIES_COMPLETE || simulation_type == SOLVE_TWO_BOUNDARIES_SIMPLE)
            {
              output << "    g1 = spalloc(" << block_mfs << "*Periods, "
                     << block_mfs << "*(Periods+" << max_leadlag_block[block].first+max_leadlag_block[block].second+1 << ")"
                     << ", " << nze << "*Periods);\n";
            }
          else
            {
              output << "    g1 = spalloc(" << block_mfs
                     << ", " << block_mfs << ", " << nze << ");\n";
            }
          output << "  end;\n";
        }

      output << "  g2=0;g3=0;\n";
      if (v_temporary_terms_inuse[block].size())
        {
          tmp_output.str("");
          for (temporary_terms_inuse_t::const_iterator it = v_temporary_terms_inuse[block].begin();
               it != v_temporary_terms_inuse[block].end(); it++)
            tmp_output << " T" << *it;
          output << "  global" << tmp_output.str() << ";\n";
        }
      if (simulation_type == SOLVE_TWO_BOUNDARIES_COMPLETE || simulation_type == SOLVE_TWO_BOUNDARIES_SIMPLE)
        {
          temporary_terms_t tt2;
          tt2.clear();
          for (int i = 0; i < (int) block_size; i++)
            {
              if (v_temporary_terms[block][i].size() && global_temporary_terms)
                {
                  output << "  " << "% //Temporary variables initialization" << endl
                         << "  " << "T_zeros = zeros(y_kmin+periods, 1);" << endl;
                  for (temporary_terms_t::const_iterator it = v_temporary_terms[block][i].begin();
                       it != v_temporary_terms[block][i].end(); it++)
                    {
                      output << "  ";
                      (*it)->writeOutput(output, oMatlabDynamicModel, local_temporary_terms);
                      output << " = T_zeros;" << endl;
                    }
                }
            }
        }
      if (simulation_type == SOLVE_BACKWARD_SIMPLE || simulation_type == SOLVE_FORWARD_SIMPLE || simulation_type == SOLVE_BACKWARD_COMPLETE || simulation_type == SOLVE_FORWARD_COMPLETE)
        output << "  residual=zeros(" << block_mfs << ",1);\n";
      else if (simulation_type == SOLVE_TWO_BOUNDARIES_COMPLETE || simulation_type == SOLVE_TWO_BOUNDARIES_SIMPLE)
        output << "  residual=zeros(" << block_mfs << ",y_kmin+periods);\n";
      if (simulation_type == EVALUATE_BACKWARD)
        output << "  for it_ = (y_kmin+periods):y_kmin+1\n";
      if (simulation_type == EVALUATE_FORWARD)
        output << "  for it_ = y_kmin+1:(y_kmin+periods)\n";

      if (simulation_type == SOLVE_TWO_BOUNDARIES_COMPLETE || simulation_type == SOLVE_TWO_BOUNDARIES_SIMPLE)
        {
          output << "  b = zeros(periods*y_size,1);" << endl
                 << "  for it_ = y_kmin+1:(periods+y_kmin)" << endl
                 << "    Per_y_=it_*y_size;" << endl
                 << "    Per_J_=(it_-y_kmin-1)*y_size;" << endl
                 << "    Per_K_=(it_-1)*y_size;" << endl;
          sps = "  ";
        }
      else
        if (simulation_type == EVALUATE_BACKWARD || simulation_type == EVALUATE_FORWARD)
          sps = "  ";
        else
          sps = "";
      // The equations
      for (unsigned int i = 0; i < block_size; i++)
        {
          temporary_terms_t tt2;
          tt2.clear();
          if (v_temporary_terms[block].size())
            {
              output << "  " << "% //Temporary variables" << endl;
              for (temporary_terms_t::const_iterator it = v_temporary_terms[block][i].begin();
                   it != v_temporary_terms[block][i].end(); it++)
                {
                  if (dynamic_cast<AbstractExternalFunctionNode *>(*it) != NULL)
                    (*it)->writeExternalFunctionOutput(output, local_output_type, tt2, tef_terms);

                  output << "  " <<  sps;
                  (*it)->writeOutput(output, local_output_type, local_temporary_terms, tef_terms);
                  output << " = ";
                  (*it)->writeOutput(output, local_output_type, tt2, tef_terms);
                  // Insert current node into tt2
                  tt2.insert(*it);
                  output << ";" << endl;
                }
            }

          int variable_ID = getBlockVariableID(block, i);
          int equation_ID = getBlockEquationID(block, i);
          EquationType equ_type = getBlockEquationType(block, i);
          string sModel = symbol_table.getName(symbol_table.getID(eEndogenous, variable_ID));
          eq_node = (BinaryOpNode *) getBlockEquationExpr(block, i);
          lhs = eq_node->get_arg1();
          rhs = eq_node->get_arg2();
          tmp_output.str("");
          lhs->writeOutput(tmp_output, local_output_type, local_temporary_terms);
          switch (simulation_type)
            {
            case EVALUATE_BACKWARD:
            case EVALUATE_FORWARD:
            evaluation:     if (simulation_type == SOLVE_TWO_BOUNDARIES_COMPLETE || simulation_type == SOLVE_TWO_BOUNDARIES_SIMPLE)
                output << "    % equation " << getBlockEquationID(block, i)+1 << " variable : " << sModel
                       << " (" << variable_ID+1 << ") " << c_Equation_Type(equ_type) << endl;
              output << "    ";
              if (equ_type == E_EVALUATE)
                {
                  output << tmp_output.str();
                  output << " = ";
                  rhs->writeOutput(output, local_output_type, local_temporary_terms);
                }
              else if (equ_type == E_EVALUATE_S)
                {
                  output << "%" << tmp_output.str();
                  output << " = ";
                  if (isBlockEquationRenormalized(block, i))
                    {
                      rhs->writeOutput(output, local_output_type, local_temporary_terms);
                      output << "\n    ";
                      tmp_output.str("");
                      eq_node = (BinaryOpNode *) getBlockEquationRenormalizedExpr(block, i);
                      lhs = eq_node->get_arg1();
                      rhs = eq_node->get_arg2();
                      lhs->writeOutput(output, local_output_type, local_temporary_terms);
                      output << " = ";
                      rhs->writeOutput(output, local_output_type, local_temporary_terms);
                    }
                }
              else
                {
                  cerr << "Type mismatch for equation " << equation_ID+1  << "\n";
                  exit(EXIT_FAILURE);
                }
              output << ";\n";
              break;
            case SOLVE_BACKWARD_SIMPLE:
            case SOLVE_FORWARD_SIMPLE:
            case SOLVE_BACKWARD_COMPLETE:
            case SOLVE_FORWARD_COMPLETE:
              if (i < block_recursive)
                goto evaluation;
              feedback_variables.push_back(variable_ID);
              output << "  % equation " << equation_ID+1 << " variable : " << sModel
                     << " (" << variable_ID+1 << ") " << c_Equation_Type(equ_type) << " symb_id=" << symbol_table.getID(eEndogenous, variable_ID) << endl;
              output << "  " << "residual(" << i+1-block_recursive << ") = (";
              goto end;
            case SOLVE_TWO_BOUNDARIES_COMPLETE:
            case SOLVE_TWO_BOUNDARIES_SIMPLE:
              if (i < block_recursive)
                goto evaluation;
              feedback_variables.push_back(variable_ID);
              output << "    % equation " << equation_ID+1 << " variable : " << sModel
                     << " (" << variable_ID+1 << ") " << c_Equation_Type(equ_type) << " symb_id=" << symbol_table.getID(eEndogenous, variable_ID) << endl;
              Ufoss << "    b(" << i+1-block_recursive << "+Per_J_) = -residual(" << i+1-block_recursive << ", it_)";
              Uf[equation_ID] += Ufoss.str();
              Ufoss.str("");
              output << "    residual(" << i+1-block_recursive << ", it_) = (";
              goto end;
            default:
            end:
              output << tmp_output.str();
              output << ") - (";
              rhs->writeOutput(output, local_output_type, local_temporary_terms);
              output << ");\n";
#ifdef CONDITION
              if (simulation_type == SOLVE_TWO_BOUNDARIES_COMPLETE || simulation_type == SOLVE_TWO_BOUNDARIES_SIMPLE)
                output << "  condition(" << i+1 << ")=0;\n";
#endif
            }
        }
      // The Jacobian if we have to solve the block
      if (simulation_type == SOLVE_TWO_BOUNDARIES_SIMPLE || simulation_type == SOLVE_TWO_BOUNDARIES_COMPLETE)
        output << "  " << sps << "% Jacobian  " << endl << "    if jacobian_eval" << endl;
      else
        if (simulation_type == SOLVE_BACKWARD_SIMPLE   || simulation_type == SOLVE_FORWARD_SIMPLE
            || simulation_type == SOLVE_BACKWARD_COMPLETE || simulation_type == SOLVE_FORWARD_COMPLETE)
          output << "  % Jacobian  " << endl << "  if jacobian_eval" << endl;
        else
          output << "    % Jacobian  " << endl << "    if jacobian_eval" << endl;
      prev_var = 999999999;
      prev_lag = -9999999;
      count_col = 0;
      for (map<pair<int, pair<int, int> >, expr_t>::const_iterator it = tmp_block_endo_derivative.begin(); it != tmp_block_endo_derivative.end(); it++)
        {
          int lag = it->first.first;
          unsigned int var = it->first.second.first;
          unsigned int eq = it->first.second.second;
          int eqr = getBlockEquationID(block, eq);
          int varr = getBlockVariableID(block, var);
          if (var != prev_var || lag != prev_lag)
            {
              prev_var = var;
              prev_lag = lag;
              count_col++;
            }

          expr_t id = it->second;

          output << "      g1(" << eq+1 << ", " << count_col << ") = ";
          id->writeOutput(output, local_output_type, local_temporary_terms);
          output << "; % variable=" << symbol_table.getName(symbol_table.getID(eEndogenous, varr))
                 << "(" << lag
                 << ") " << varr+1 << ", " << var+1
                 << ", equation=" << eqr+1 << ", " << eq+1 << endl;
        }
      prev_var = 999999999;
      prev_lag = -9999999;
      count_col = 0;
      for (map<pair<int, pair<int, int> >, expr_t>::const_iterator it = tmp_block_exo_derivative.begin(); it != tmp_block_exo_derivative.end(); it++)
        {
          int lag = it->first.first;
          unsigned int var = it->first.second.first;
          unsigned int eq = it->first.second.second;
          int eqr = getBlockInitialEquationID(block, eq);
          if (var != prev_var || lag != prev_lag)
            {
              prev_var = var;
              prev_lag = lag;
              count_col++;
            }
          expr_t id = it->second;
          output << "      g1_x(" << eqr+1 << ", " << count_col << ") = ";
          id->writeOutput(output, local_output_type, local_temporary_terms);
          output << "; % variable=" << symbol_table.getName(symbol_table.getID(eExogenous, var))
                 << "(" << lag
                 << ") " << var+1
                 << ", equation=" << eq+1 << endl;
        }
      prev_var = 999999999;
      prev_lag = -9999999;
      count_col = 0;
      for (map<pair<int, pair<int, int> >, expr_t>::const_iterator it = tmp_block_exo_det_derivative.begin(); it != tmp_block_exo_det_derivative.end(); it++)
        {
          int lag = it->first.first;
          unsigned int var = it->first.second.first;
          unsigned int eq = it->first.second.second;
          int eqr = getBlockInitialEquationID(block, eq);
          if (var != prev_var || lag != prev_lag)
            {
              prev_var = var;
              prev_lag = lag;
              count_col++;
            }
          expr_t id = it->second;
          output << "      g1_xd(" << eqr+1 << ", " << count_col << ") = ";
          id->writeOutput(output, local_output_type, local_temporary_terms);
          output << "; % variable=" << symbol_table.getName(symbol_table.getID(eExogenous, var))
                 << "(" << lag
                 << ") " << var+1
                 << ", equation=" << eq+1 << endl;
        }
      prev_var = 999999999;
      prev_lag = -9999999;
      count_col = 0;
      for (map<pair<int, pair<int, int> >, expr_t>::const_iterator it = tmp_block_other_endo_derivative.begin(); it != tmp_block_other_endo_derivative.end(); it++)
        {
          int lag = it->first.first;
          unsigned int var = it->first.second.first;
          unsigned int eq = it->first.second.second;
          int eqr = getBlockInitialEquationID(block, eq);
          if (var != prev_var || lag != prev_lag)
            {
              prev_var = var;
              prev_lag = lag;
              count_col++;
            }
          expr_t id = it->second;

          output << "      g1_o(" << eqr+1 << ", " << /*var+1+(lag+block_max_lag)*block_size*/ count_col << ") = ";
          id->writeOutput(output, local_output_type, local_temporary_terms);
          output << "; % variable=" << symbol_table.getName(symbol_table.getID(eEndogenous, var))
                 << "(" << lag
                 << ") " << var+1
                 << ", equation=" << eq+1 << endl;
        }
      output << "      varargout{1}=g1_x;\n";
      output << "      varargout{2}=g1_xd;\n";
      output << "      varargout{3}=g1_o;\n";

      switch (simulation_type)
        {
        case EVALUATE_FORWARD:
        case EVALUATE_BACKWARD:
          output << "    end;" << endl;
          output << "  end;" << endl;
          break;
        case SOLVE_BACKWARD_SIMPLE:
        case SOLVE_FORWARD_SIMPLE:
        case SOLVE_BACKWARD_COMPLETE:
        case SOLVE_FORWARD_COMPLETE:
          output << "  else" << endl;
          for (block_derivatives_equation_variable_laglead_nodeid_t::const_iterator it = blocks_derivatives[block].begin(); it != (blocks_derivatives[block]).end(); it++)
            {
              unsigned int eq = it->first.first;
              unsigned int var = it->first.second;
              unsigned int eqr = getBlockEquationID(block, eq);
              unsigned int varr = getBlockVariableID(block, var);
              expr_t id = it->second.second;
              int lag = it->second.first;
              if (lag == 0)
                {
                  output << "    g1(" << eq+1 << ", " << var+1-block_recursive << ") = ";
                  id->writeOutput(output, local_output_type, local_temporary_terms);
                  output << "; % variable=" << symbol_table.getName(symbol_table.getID(eEndogenous, varr))
                         << "(" << lag
                         << ") " << varr+1
                         << ", equation=" << eqr+1 << endl;
                }

            }
          output << "  end;\n";
          break;
        case SOLVE_TWO_BOUNDARIES_SIMPLE:
        case SOLVE_TWO_BOUNDARIES_COMPLETE:
          output << "    else" << endl;
          for (block_derivatives_equation_variable_laglead_nodeid_t::const_iterator it = blocks_derivatives[block].begin(); it != (blocks_derivatives[block]).end(); it++)
            {
              unsigned int eq = it->first.first;
              unsigned int var = it->first.second;
              unsigned int eqr = getBlockEquationID(block, eq);
              unsigned int varr = getBlockVariableID(block, var);
              ostringstream tmp_output;
              expr_t id = it->second.second;
              int lag = it->second.first;
              if (eq >= block_recursive && var >= block_recursive)
                {
                  if (lag == 0)
                    Ufoss << "+g1(" << eq+1-block_recursive
                          << "+Per_J_, " << var+1-block_recursive
                          << "+Per_K_)*y(it_, " << varr+1 << ")";
                  else if (lag == 1)
                    Ufoss << "+g1(" << eq+1-block_recursive
                          << "+Per_J_, " << var+1-block_recursive
                          << "+Per_y_)*y(it_+1, " << varr+1 << ")";
                  else if (lag > 0)
                    Ufoss << "+g1(" << eq+1-block_recursive
                          << "+Per_J_, " << var+1-block_recursive
                          << "+y_size*(it_+" << lag-1 << "))*y(it_+" << lag << ", " << varr+1 << ")";
                  else
                    Ufoss << "+g1(" << eq+1-block_recursive
                          << "+Per_J_, " << var+1-block_recursive
                          << "+y_size*(it_" << lag-1 << "))*y(it_" << lag << ", " << varr+1 << ")";
                  Uf[eqr] += Ufoss.str();
                  Ufoss.str("");

                  if (lag == 0)
                    tmp_output << "     g1(" << eq+1-block_recursive << "+Per_J_, "
                               << var+1-block_recursive << "+Per_K_) = ";
                  else if (lag == 1)
                    tmp_output << "     g1(" << eq+1-block_recursive << "+Per_J_, "
                               << var+1-block_recursive << "+Per_y_) = ";
                  else if (lag > 0)
                    tmp_output << "     g1(" << eq+1-block_recursive << "+Per_J_, "
                               << var+1-block_recursive << "+y_size*(it_+" << lag-1 << ")) = ";
                  else if (lag < 0)
                    tmp_output << "     g1(" << eq+1-block_recursive << "+Per_J_, "
                               << var+1-block_recursive << "+y_size*(it_" << lag-1 << ")) = ";
                  output << " " << tmp_output.str();
                  id->writeOutput(output, local_output_type, local_temporary_terms);
                  output << ";";
                  output << " %2 variable=" << symbol_table.getName(symbol_table.getID(eEndogenous, varr))
                         << "(" << lag << ") " << varr+1
                         << ", equation=" << eqr+1 << " (" << eq+1 << ")" << endl;
                }

#ifdef CONDITION
              output << "  if (fabs(condition[" << eqr << "])<fabs(u[" << u << "+Per_u_]))\n";
              output << "    condition(" << eqr << ")=u(" << u << "+Per_u_);\n";
#endif
            }
          for (unsigned int i = 0; i < block_size; i++)
            {
              if (i >= block_recursive)
                output << "  " << Uf[getBlockEquationID(block, i)] << ";\n";
#ifdef CONDITION
              output << "  if (fabs(condition(" << i+1 << "))<fabs(u(" << i << "+Per_u_)))\n";
              output << "    condition(" << i+1 << ")=u(" << i+1 << "+Per_u_);\n";
#endif
            }
#ifdef CONDITION
          for (m = 0; m <= ModelBlock->Block_List[block].Max_Lead+ModelBlock->Block_List[block].Max_Lag; m++)
            {
              k = m-ModelBlock->Block_List[block].Max_Lag;
              for (i = 0; i < ModelBlock->Block_List[block].IM_lead_lag[m].size; i++)
                {
                  unsigned int eq = ModelBlock->Block_List[block].IM_lead_lag[m].Equ_Index[i];
                  unsigned int var = ModelBlock->Block_List[block].IM_lead_lag[m].Var_Index[i];
                  unsigned int u = ModelBlock->Block_List[block].IM_lead_lag[m].u[i];
                  unsigned int eqr = ModelBlock->Block_List[block].IM_lead_lag[m].Equ[i];
                  output << "  u(" << u+1 << "+Per_u_) = u(" << u+1 << "+Per_u_) / condition(" << eqr+1 << ");\n";
                }
            }
          for (i = 0; i < ModelBlock->Block_List[block].Size; i++)
            output << "  u(" << i+1 << "+Per_u_) = u(" << i+1 << "+Per_u_) / condition(" << i+1 << ");\n";
#endif
          output << "    end;" << endl;
          output << "  end;" << endl;
          break;
        default:
          break;
        }
      output << "end" << endl;
      output.close();
    }
}

void
DynamicModel::writeModelEquationsCode(string &file_name, const string &bin_basename, const map_idx_t &map_idx) const
{

  ostringstream tmp_output;
  ofstream code_file;
  unsigned int instruction_number = 0;
  bool file_open = false;
  string main_name = file_name;

  main_name += ".cod";
  code_file.open(main_name.c_str(), ios::out | ios::binary | ios::ate);
  if (!code_file.is_open())
    {
      cerr << "Error : Can't open file \"" << main_name << "\" for writing" << endl;
      exit(EXIT_FAILURE);
    }

  int count_u;
  int u_count_int = 0;
  BlockSimulationType simulation_type;
  if ((max_endo_lag > 0) && (max_endo_lead > 0))
    simulation_type = SOLVE_TWO_BOUNDARIES_COMPLETE;
  else if ((max_endo_lag >= 0) && (max_endo_lead == 0))
    simulation_type = SOLVE_FORWARD_COMPLETE;
  else
    simulation_type = SOLVE_BACKWARD_COMPLETE;

  Write_Inf_To_Bin_File(file_name, u_count_int, file_open, simulation_type == SOLVE_TWO_BOUNDARIES_COMPLETE, symbol_table.endo_nbr());
  file_open = true;

  //Temporary variables declaration
  FDIMT_ fdimt(temporary_terms.size());
  fdimt.write(code_file, instruction_number);

  vector<unsigned int> exo, exo_det, other_endo;

  for (int i = 0; i < symbol_table.exo_det_nbr(); i++)
    exo_det.push_back(i);
  for (int i = 0; i < symbol_table.exo_nbr(); i++)
    exo.push_back(i);

  map<pair< int, pair<int, int> >, expr_t> first_derivatives_reordered_endo;
  map<pair< pair<int, int>, pair<int, int> >, expr_t>  first_derivatives_reordered_exo;
  for (first_derivatives_t::const_iterator it = first_derivatives.begin();
       it != first_derivatives.end(); it++)
    {
      int deriv_id = it->first.second;
      unsigned int eq = it->first.first;
      int symb = getSymbIDByDerivID(deriv_id);
      unsigned int var = symbol_table.getTypeSpecificID(symb);
      int lag = getLagByDerivID(deriv_id);
      if (getTypeByDerivID(deriv_id) == eEndogenous)
        first_derivatives_reordered_endo[make_pair(lag, make_pair(var, eq))] = it->second;
      else if (getTypeByDerivID(deriv_id) == eExogenous || getTypeByDerivID(deriv_id) == eExogenousDet)
        first_derivatives_reordered_exo[make_pair(make_pair(lag, getTypeByDerivID(deriv_id)), make_pair(var, eq))] = it->second;
    }
  int prev_var = -1;
  int prev_lag = -999999999;
  int count_col_endo = 0;
  for (map<pair< int, pair<int, int> >, expr_t>::const_iterator it = first_derivatives_reordered_endo.begin();
       it != first_derivatives_reordered_endo.end(); it++)
    {
      int var = it->first.second.first;
      int lag = it->first.first;
      if (prev_var != var || prev_lag != lag)
        {
          prev_var = var;
          prev_lag = lag;
          count_col_endo++;
        }
    }
  prev_var = -1;
  prev_lag = -999999999;
  int prev_type = -1;
  int count_col_exo = 0;
  int count_col_det_exo = 0;

  for (map<pair< pair<int, int>, pair<int, int> >, expr_t>::const_iterator it = first_derivatives_reordered_exo.begin();
       it != first_derivatives_reordered_exo.end(); it++)
    {
      int var = it->first.second.first;
      int lag = it->first.first.first;
      int type = it->first.first.second;
      if (prev_var != var || prev_lag != lag || prev_type != type)
        {
          prev_var = var;
          prev_lag = lag;
          prev_type = type;
          if (type == eExogenous)
            count_col_exo++;
          else if (type == eExogenousDet)
            count_col_det_exo++;
        }
    }

  FBEGINBLOCK_ fbeginblock(symbol_table.endo_nbr(),
                           simulation_type,
                           0,
                           symbol_table.endo_nbr(),
                           variable_reordered,
                           equation_reordered,
                           false,
                           symbol_table.endo_nbr(),
                           max_endo_lag,
                           max_endo_lead,
                           u_count_int,
                           count_col_endo,
                           symbol_table.exo_det_nbr(),
                           count_col_det_exo,
                           symbol_table.exo_nbr(),
                           count_col_exo,
                           0,
                           0,
                           exo_det,
                           exo,
                           other_endo
                           );
  fbeginblock.write(code_file, instruction_number);

  compileTemporaryTerms(code_file, instruction_number, temporary_terms, map_idx, true, false);

  compileModelEquations(code_file, instruction_number, temporary_terms, map_idx, true, false);

  FENDEQU_ fendequ;
  fendequ.write(code_file, instruction_number);

  // Get the current code_file position and jump if eval = true
  streampos pos1 = code_file.tellp();
  FJMPIFEVAL_ fjmp_if_eval(0);
  fjmp_if_eval.write(code_file, instruction_number);
  int prev_instruction_number = instruction_number;

  vector<vector<pair<pair<int, int>, int > > > derivatives;
  derivatives.resize(symbol_table.endo_nbr());
  count_u = symbol_table.endo_nbr();
  for (first_derivatives_t::const_iterator it = first_derivatives.begin();
       it != first_derivatives.end(); it++)
    {
      int deriv_id = it->first.second;
      if (getTypeByDerivID(deriv_id) == eEndogenous)
        {
          expr_t d1 = it->second;
          unsigned int eq = it->first.first;
          int symb = getSymbIDByDerivID(deriv_id);
          unsigned int var = symbol_table.getTypeSpecificID(symb);
          int lag = getLagByDerivID(deriv_id);
          FNUMEXPR_ fnumexpr(FirstEndoDerivative, eq, var, lag);
          fnumexpr.write(code_file, instruction_number);
          if (!derivatives[eq].size())
            derivatives[eq].clear();
          derivatives[eq].push_back(make_pair(make_pair(var, lag), count_u));
          d1->compile(code_file, instruction_number, false, temporary_terms, map_idx, true, false);

          FSTPU_ fstpu(count_u);
          fstpu.write(code_file, instruction_number);
          count_u++;
        }
    }
  for (int i = 0; i < symbol_table.endo_nbr(); i++)
    {
      FLDR_ fldr(i);
      fldr.write(code_file, instruction_number);
      if (derivatives[i].size())
        {
          for (vector<pair<pair<int, int>, int> >::const_iterator it = derivatives[i].begin();
               it != derivatives[i].end(); it++)
            {
              FLDU_ fldu(it->second);
              fldu.write(code_file, instruction_number);
              FLDV_ fldv(eEndogenous, it->first.first, it->first.second);
              fldv.write(code_file, instruction_number);
              FBINARY_ fbinary(oTimes);
              fbinary.write(code_file, instruction_number);
              if (it != derivatives[i].begin())
                {
                  FBINARY_ fbinary(oPlus);
                  fbinary.write(code_file, instruction_number);
                }
            }
          FBINARY_ fbinary(oMinus);
          fbinary.write(code_file, instruction_number);
        }
      FSTPU_ fstpu(i);
      fstpu.write(code_file, instruction_number);
    }

  // Get the current code_file position and jump = true
  streampos pos2 = code_file.tellp();
  FJMP_ fjmp(0);
  fjmp.write(code_file, instruction_number);
  // Set code_file position to previous JMPIFEVAL_ and set the number of instructions to jump
  streampos pos3 = code_file.tellp();
  code_file.seekp(pos1);
  FJMPIFEVAL_ fjmp_if_eval1(instruction_number - prev_instruction_number);
  fjmp_if_eval1.write(code_file, instruction_number);
  code_file.seekp(pos3);
  prev_instruction_number = instruction_number;

  // The Jacobian
  prev_var = -1;
  prev_lag = -999999999;
  count_col_endo = 0;
  for (map<pair< int, pair<int, int> >, expr_t>::const_iterator it = first_derivatives_reordered_endo.begin();
       it != first_derivatives_reordered_endo.end(); it++)
    {
      unsigned int eq = it->first.second.second;
      int var = it->first.second.first;
      int lag = it->first.first;
      expr_t d1 = it->second;
      FNUMEXPR_ fnumexpr(FirstEndoDerivative, eq, var, lag);
      fnumexpr.write(code_file, instruction_number);
      if (prev_var != var || prev_lag != lag)
        {
          prev_var = var;
          prev_lag = lag;
          count_col_endo++;
        }
      d1->compile(code_file, instruction_number, false, temporary_terms, map_idx, true, false);
      FSTPG3_ fstpg3(eq, var, lag, count_col_endo-1);
      fstpg3.write(code_file, instruction_number);
    }
  prev_var = -1;
  prev_lag = -999999999;
  count_col_exo = 0;
  for (map<pair< pair<int, int>, pair<int, int> >, expr_t>::const_iterator it = first_derivatives_reordered_exo.begin();
       it != first_derivatives_reordered_exo.end(); it++)
    {
      unsigned int eq = it->first.second.second;
      int var = it->first.second.first;
      int lag = it->first.first.first;
      expr_t d1 = it->second;
      FNUMEXPR_ fnumexpr(FirstExoDerivative, eq, var, lag);
      fnumexpr.write(code_file, instruction_number);
      if (prev_var != var || prev_lag != lag)
        {
          prev_var = var;
          prev_lag = lag;
          count_col_exo++;
        }
      d1->compile(code_file, instruction_number, false, temporary_terms, map_idx, true, false);
      FSTPG3_ fstpg3(eq, var, lag, count_col_exo-1);
      fstpg3.write(code_file, instruction_number);
    }
  // Set codefile position to previous JMP_ and set the number of instructions to jump
  pos1 = code_file.tellp();
  code_file.seekp(pos2);
  FJMP_ fjmp1(instruction_number - prev_instruction_number);
  fjmp1.write(code_file, instruction_number);
  code_file.seekp(pos1);

  FENDBLOCK_ fendblock;
  fendblock.write(code_file, instruction_number);
  FEND_ fend;
  fend.write(code_file, instruction_number);
  code_file.close();
}

void
DynamicModel::writeModelEquationsCode_Block(string &file_name, const string &bin_basename, const map_idx_t &map_idx) const
{
  struct Uff_l
  {
    int u, var, lag;
    Uff_l *pNext;
  };

  struct Uff
  {
    Uff_l *Ufl, *Ufl_First;
  };

  int i, v;
  string tmp_s;
  ostringstream tmp_output;
  ofstream code_file;
  unsigned int instruction_number = 0;
  expr_t lhs = NULL, rhs = NULL;
  BinaryOpNode *eq_node;
  Uff Uf[symbol_table.endo_nbr()];
  map<expr_t, int> reference_count;
  deriv_node_temp_terms_t tef_terms;
  vector<int> feedback_variables;
  bool file_open = false;

  string main_name = file_name;
  main_name += ".cod";
  code_file.open(main_name.c_str(), ios::out | ios::binary | ios::ate);
  if (!code_file.is_open())
    {
      cerr << "Error : Can't open file \"" << main_name << "\" for writing" << endl;
      exit(EXIT_FAILURE);
    }
  //Temporary variables declaration

  FDIMT_ fdimt(temporary_terms.size());
  fdimt.write(code_file, instruction_number);

  for (unsigned int block = 0; block < getNbBlocks(); block++)
    {
      feedback_variables.clear();
      if (block > 0)
        {
          FENDBLOCK_ fendblock;
          fendblock.write(code_file, instruction_number);
        }
      int count_u;
      int u_count_int = 0;
      BlockSimulationType simulation_type = getBlockSimulationType(block);
      unsigned int block_size = getBlockSize(block);
      unsigned int block_mfs = getBlockMfs(block);
      unsigned int block_recursive = block_size - block_mfs;
      int block_max_lag = max_leadlag_block[block].first;
      int block_max_lead = max_leadlag_block[block].second;

      if (simulation_type == SOLVE_TWO_BOUNDARIES_SIMPLE || simulation_type == SOLVE_TWO_BOUNDARIES_COMPLETE
          || simulation_type == SOLVE_BACKWARD_COMPLETE || simulation_type == SOLVE_FORWARD_COMPLETE)
        {
          Write_Inf_To_Bin_File_Block(file_name, bin_basename, block, u_count_int, file_open,
                                      simulation_type == SOLVE_TWO_BOUNDARIES_COMPLETE || simulation_type == SOLVE_TWO_BOUNDARIES_SIMPLE);
          file_open = true;
        }
      map<pair<int, pair<int, int> >, expr_t> tmp_block_endo_derivative;
      for (block_derivatives_equation_variable_laglead_nodeid_t::const_iterator it = blocks_derivatives[block].begin(); it != (blocks_derivatives[block]).end(); it++)
        tmp_block_endo_derivative[make_pair(it->second.first, make_pair(it->first.second, it->first.first))] = it->second.second;
      map<pair<int, pair<int, int> >, expr_t> tmp_exo_derivative;
      for (derivative_t::const_iterator it = derivative_exo[block].begin(); it != (derivative_exo[block]).end(); it++)
        tmp_exo_derivative[make_pair(it->first.first, make_pair(it->first.second.second, it->first.second.first))] = it->second;
      map<pair<int, pair<int, int> >, expr_t> tmp_exo_det_derivative;
      for (derivative_t::const_iterator it = derivative_exo_det[block].begin(); it != (derivative_exo_det[block]).end(); it++)
        tmp_exo_det_derivative[make_pair(it->first.first, make_pair(it->first.second.second, it->first.second.first))] = it->second;
      map<pair<int, pair<int, int> >, expr_t> tmp_other_endo_derivative;
      for (derivative_t::const_iterator it = derivative_other_endo[block].begin(); it != (derivative_other_endo[block]).end(); it++)
        tmp_other_endo_derivative[make_pair(it->first.first, make_pair(it->first.second.second, it->first.second.first))] = it->second;
      int prev_var = -1;
      int prev_lag = -999999999;
      int count_col_endo = 0;
      for (map<pair<int, pair<int, int> >, expr_t>::const_iterator it = tmp_block_endo_derivative.begin(); it != tmp_block_endo_derivative.end(); it++)
        {
          int lag = it->first.first;
          int var = it->first.second.first;
          if (prev_var != var || prev_lag != lag)
            {
              prev_var = var;
              prev_lag = lag;
              count_col_endo++;
            }
        }
      unsigned int count_col_det_exo = 0;
      vector<unsigned int> exo_det;
      for (lag_var_t::const_iterator it = exo_det_block[block].begin(); it != exo_det_block[block].end(); it++)
        for (var_t::const_iterator it1 = it->second.begin(); it1 != it->second.end(); it1++)
          {
            count_col_det_exo++;
            if (find(exo_det.begin(), exo_det.end(), *it1) == exo_det.end())
              exo_det.push_back(*it1);
          }

      unsigned int count_col_exo = 0;
      vector<unsigned int> exo;
      for (lag_var_t::const_iterator it = exo_block[block].begin(); it != exo_block[block].end(); it++)
        for (var_t::const_iterator it1 = it->second.begin(); it1 != it->second.end(); it1++)
          {
            count_col_exo++;
            if (find(exo.begin(), exo.end(), *it1) == exo.end())
              exo.push_back(*it1);
          }

      vector<unsigned int> other_endo;
      unsigned int count_col_other_endo = 0;
      for (lag_var_t::const_iterator it = other_endo_block[block].begin(); it != other_endo_block[block].end(); it++)
        for (var_t::const_iterator it1 = it->second.begin(); it1 != it->second.end(); it1++)
          {
            count_col_other_endo++;
            if (find(other_endo.begin(), other_endo.end(), *it1) == other_endo.end())
              other_endo.push_back(*it1);
          }

      FBEGINBLOCK_ fbeginblock(block_mfs,
                               simulation_type,
                               getBlockFirstEquation(block),
                               block_size,
                               variable_reordered,
                               equation_reordered,
                               blocks_linear[block],
                               symbol_table.endo_nbr(),
                               block_max_lag,
                               block_max_lead,
                               u_count_int,
                               count_col_endo,
                               exo_det.size(),
                               count_col_det_exo,
                               exo.size(),
                               getBlockExoColSize(block),
                               other_endo.size(),
                               count_col_other_endo,
                               exo_det,
                               exo,
                               other_endo
                               );
      fbeginblock.write(code_file, instruction_number);

      // The equations
      for (i = 0; i < (int) block_size; i++)
        {
          //The Temporary terms
          temporary_terms_t tt2;
          tt2.clear();
          if (v_temporary_terms[block][i].size())
            {
              for (temporary_terms_t::const_iterator it = v_temporary_terms[block][i].begin();
                   it != v_temporary_terms[block][i].end(); it++)
                {
                  if (dynamic_cast<AbstractExternalFunctionNode *>(*it) != NULL)
                    (*it)->compileExternalFunctionOutput(code_file, instruction_number, false, tt2, map_idx, true, false, tef_terms);

                  FNUMEXPR_ fnumexpr(TemporaryTerm, (int)(map_idx.find((*it)->idx)->second));
                  fnumexpr.write(code_file, instruction_number);
                  (*it)->compile(code_file, instruction_number, false, tt2, map_idx, true, false, tef_terms);
                  FSTPT_ fstpt((int)(map_idx.find((*it)->idx)->second));
                  fstpt.write(code_file, instruction_number);
                  // Insert current node into tt2
                  tt2.insert(*it);
#ifdef DEBUGC
                  cout << "FSTPT " << v << "\n";
                  instruction_number++;
                  code_file.write(&FOK, sizeof(FOK));
                  code_file.write(reinterpret_cast<char *>(&k), sizeof(k));
                  ki++;
#endif

                }
            }
#ifdef DEBUGC
          for (temporary_terms_t::const_iterator it = v_temporary_terms[block][i].begin();
               it != v_temporary_terms[block][i].end(); it++)
            {
              map_idx_t::const_iterator ii = map_idx.find((*it)->idx);
              cout << "map_idx[" << (*it)->idx <<"]=" << ii->second << "\n";
            }
#endif

          int variable_ID, equation_ID;
          EquationType equ_type;

          switch (simulation_type)
            {
            evaluation:
            case EVALUATE_BACKWARD:
            case EVALUATE_FORWARD:
              equ_type = getBlockEquationType(block, i);
              {
                FNUMEXPR_ fnumexpr(ModelEquation, getBlockEquationID(block, i));
                fnumexpr.write(code_file, instruction_number);
              }
              if (equ_type == E_EVALUATE)
                {
                  eq_node = (BinaryOpNode *) getBlockEquationExpr(block, i);
                  lhs = eq_node->get_arg1();
                  rhs = eq_node->get_arg2();
                  rhs->compile(code_file, instruction_number, false, temporary_terms, map_idx, true, false);
                  lhs->compile(code_file, instruction_number, true, temporary_terms, map_idx, true, false);
                }
              else if (equ_type == E_EVALUATE_S)
                {
                  eq_node = (BinaryOpNode *) getBlockEquationRenormalizedExpr(block, i);
                  lhs = eq_node->get_arg1();
                  rhs = eq_node->get_arg2();
                  rhs->compile(code_file, instruction_number, false, temporary_terms, map_idx, true, false);
                  lhs->compile(code_file, instruction_number, true, temporary_terms, map_idx, true, false);
                }
              break;
            case SOLVE_BACKWARD_COMPLETE:
            case SOLVE_FORWARD_COMPLETE:
            case SOLVE_TWO_BOUNDARIES_COMPLETE:
            case SOLVE_TWO_BOUNDARIES_SIMPLE:
              if (i < (int) block_recursive)
                goto evaluation;
              variable_ID = getBlockVariableID(block, i);
              equation_ID = getBlockEquationID(block, i);
              feedback_variables.push_back(variable_ID);
              Uf[equation_ID].Ufl = NULL;
              goto end;
            default:
            end:
              FNUMEXPR_ fnumexpr(ModelEquation, getBlockEquationID(block, i));
              fnumexpr.write(code_file, instruction_number);
              eq_node = (BinaryOpNode *) getBlockEquationExpr(block, i);
              lhs = eq_node->get_arg1();
              rhs = eq_node->get_arg2();
              lhs->compile(code_file, instruction_number, false, temporary_terms, map_idx, true, false);
              rhs->compile(code_file, instruction_number, false, temporary_terms, map_idx, true, false);

              FBINARY_ fbinary(oMinus);
              fbinary.write(code_file, instruction_number);
              FSTPR_ fstpr(i - block_recursive);
              fstpr.write(code_file, instruction_number);
            }
        }
      FENDEQU_ fendequ;
      fendequ.write(code_file, instruction_number);

      // Get the current code_file position and jump if eval = true
      streampos pos1 = code_file.tellp();
      FJMPIFEVAL_ fjmp_if_eval(0);
      fjmp_if_eval.write(code_file, instruction_number);
      int prev_instruction_number = instruction_number;
      // The Jacobian if we have to solve the block determinsitic block
      if    (simulation_type != EVALUATE_BACKWARD
             && simulation_type != EVALUATE_FORWARD)
        {
          switch (simulation_type)
            {
            case SOLVE_BACKWARD_SIMPLE:
            case SOLVE_FORWARD_SIMPLE:
              {
                FNUMEXPR_ fnumexpr(FirstEndoDerivative, getBlockEquationID(block, 0), getBlockVariableID(block, 0), 0);
                fnumexpr.write(code_file, instruction_number);
              }
              compileDerivative(code_file, instruction_number, getBlockEquationID(block, 0), getBlockVariableID(block, 0), 0, map_idx);
              {
                FSTPG_ fstpg(0);
                fstpg.write(code_file, instruction_number);
              }
              break;

            case SOLVE_BACKWARD_COMPLETE:
            case SOLVE_FORWARD_COMPLETE:
            case SOLVE_TWO_BOUNDARIES_COMPLETE:
            case SOLVE_TWO_BOUNDARIES_SIMPLE:
              count_u = feedback_variables.size();
              for (block_derivatives_equation_variable_laglead_nodeid_t::const_iterator it = blocks_derivatives[block].begin(); it != (blocks_derivatives[block]).end(); it++)
                {
                  int lag = it->second.first;
                  unsigned int eq = it->first.first;
                  unsigned int var = it->first.second;
                  unsigned int eqr = getBlockEquationID(block, eq);
                  unsigned int varr = getBlockVariableID(block, var);
                  if (eq >= block_recursive and var >= block_recursive)
                    {
                      if (lag != 0 && (simulation_type == SOLVE_FORWARD_COMPLETE || simulation_type == SOLVE_BACKWARD_COMPLETE))
                        continue;
                      if (!Uf[eqr].Ufl)
                        {
                          Uf[eqr].Ufl = (Uff_l *) malloc(sizeof(Uff_l));
                          Uf[eqr].Ufl_First = Uf[eqr].Ufl;
                        }
                      else
                        {
                          Uf[eqr].Ufl->pNext = (Uff_l *) malloc(sizeof(Uff_l));
                          Uf[eqr].Ufl = Uf[eqr].Ufl->pNext;
                        }
                      Uf[eqr].Ufl->pNext = NULL;
                      Uf[eqr].Ufl->u = count_u;
                      Uf[eqr].Ufl->var = varr;
                      Uf[eqr].Ufl->lag = lag;
                      FNUMEXPR_ fnumexpr(FirstEndoDerivative, eqr, varr, lag);
                      fnumexpr.write(code_file, instruction_number);
                      compileChainRuleDerivative(code_file, instruction_number, eqr, varr, lag, map_idx);
                      FSTPU_ fstpu(count_u);
                      fstpu.write(code_file, instruction_number);
                      count_u++;
                    }
                }
              for (i = 0; i < (int) block_size; i++)
                {
                  if (i >= (int) block_recursive)
                    {
                      FLDR_ fldr(i-block_recursive);
                      fldr.write(code_file, instruction_number);

                      FLDZ_ fldz;
                      fldz.write(code_file, instruction_number);

                      v = getBlockEquationID(block, i);
                      for (Uf[v].Ufl = Uf[v].Ufl_First; Uf[v].Ufl; Uf[v].Ufl = Uf[v].Ufl->pNext)
                        {
                          FLDU_ fldu(Uf[v].Ufl->u);
                          fldu.write(code_file, instruction_number);
                          FLDV_ fldv(eEndogenous, Uf[v].Ufl->var, Uf[v].Ufl->lag);
                          fldv.write(code_file, instruction_number);

                          FBINARY_ fbinary(oTimes);
                          fbinary.write(code_file, instruction_number);

                          FCUML_ fcuml;
                          fcuml.write(code_file, instruction_number);
                        }
                      Uf[v].Ufl = Uf[v].Ufl_First;
                      while (Uf[v].Ufl)
                        {
                          Uf[v].Ufl_First = Uf[v].Ufl->pNext;
                          free(Uf[v].Ufl);
                          Uf[v].Ufl = Uf[v].Ufl_First;
                        }
                      FBINARY_ fbinary(oMinus);
                      fbinary.write(code_file, instruction_number);

                      FSTPU_ fstpu(i - block_recursive);
                      fstpu.write(code_file, instruction_number);
                    }
                }
              break;
            default:
              break;
            }
        }
      // Get the current code_file position and jump = true
      streampos pos2 = code_file.tellp();
      FJMP_ fjmp(0);
      fjmp.write(code_file, instruction_number);
      // Set code_file position to previous JMPIFEVAL_ and set the number of instructions to jump
      streampos pos3 = code_file.tellp();
      code_file.seekp(pos1);
      FJMPIFEVAL_ fjmp_if_eval1(instruction_number - prev_instruction_number);
      fjmp_if_eval1.write(code_file, instruction_number);
      code_file.seekp(pos3);
      prev_instruction_number = instruction_number;
      // The Jacobian if we have to solve the block determinsitic block

      prev_var = -1;
      prev_lag = -999999999;
      count_col_endo = 0;
      for (map<pair<int, pair<int, int> >, expr_t>::const_iterator it = tmp_block_endo_derivative.begin(); it != tmp_block_endo_derivative.end(); it++)
        {
          int lag = it->first.first;
          unsigned int eq = it->first.second.second;
          int var = it->first.second.first;
          unsigned int eqr = getBlockEquationID(block, eq);
          unsigned int varr = getBlockVariableID(block, var);
          if (prev_var != var || prev_lag != lag)
            {
              prev_var = var;
              prev_lag = lag;
              count_col_endo++;
            }
          FNUMEXPR_ fnumexpr(FirstEndoDerivative, eqr, varr, lag);
          fnumexpr.write(code_file, instruction_number);
          compileDerivative(code_file, instruction_number, eqr, varr, lag, map_idx);
          FSTPG3_ fstpg3(eq, var, lag, count_col_endo-1);
          fstpg3.write(code_file, instruction_number);
        }
      prev_var = -1;
      prev_lag = -999999999;
      count_col_exo = 0;
      for (map<pair<int, pair<int, int> >, expr_t>::const_iterator it = tmp_exo_derivative.begin(); it != tmp_exo_derivative.end(); it++)
        {
          int lag = it->first.first;
          int eq = it->first.second.second;
          int var = it->first.second.first;
          int eqr = getBlockInitialEquationID(block, eq);
          int varr = getBlockInitialExogenousID(block, var);
          if (prev_var != var || prev_lag != lag)
            {
              prev_var = var;
              prev_lag = lag;
              count_col_exo++;
            }
          expr_t id = it->second;

          FNUMEXPR_ fnumexpr(FirstExoDerivative, eqr, varr, lag);
          fnumexpr.write(code_file, instruction_number);
          id->compile(code_file, instruction_number, false, temporary_terms, map_idx, true, false);
          FSTPG3_ fstpg3(eq, var, lag, /*var*/ count_col_exo-1);
          fstpg3.write(code_file, instruction_number);
        }
      prev_var = -1;
      prev_lag = -999999999;
      int count_col_exo_det = 0;
      for (map<pair<int, pair<int, int> >, expr_t>::const_iterator it = tmp_exo_det_derivative.begin(); it != tmp_exo_det_derivative.end(); it++)
        {
          int lag = it->first.first;
          int eq = it->first.second.second;
          int var = it->first.second.first;
          int eqr = getBlockInitialEquationID(block, eq);
          int varr = getBlockInitialDetExogenousID(block, var);
          if (prev_var != var || prev_lag != lag)
            {
              prev_var = var;
              prev_lag = lag;
              count_col_exo_det++;
            }
          expr_t id = it->second;

          FNUMEXPR_ fnumexpr(FirstExodetDerivative, eqr, varr, lag);
          fnumexpr.write(code_file, instruction_number);
          id->compile(code_file, instruction_number, false, temporary_terms, map_idx, true, false);
          FSTPG3_ fstpg3(eq, var, lag, count_col_exo_det-1);
          fstpg3.write(code_file, instruction_number);
        }
      prev_var = -1;
      prev_lag = -999999999;
      count_col_other_endo = 0;
      for (map<pair<int, pair<int, int> >, expr_t>::const_iterator it = tmp_other_endo_derivative.begin(); it != tmp_other_endo_derivative.end(); it++)
        {
          int lag = it->first.first;
          int eq = it->first.second.second;
          int var = it->first.second.first;
          int eqr = getBlockInitialEquationID(block, eq);
          int varr = getBlockInitialOtherEndogenousID(block, var);;
          if (prev_var != var || prev_lag != lag)
            {
              prev_var = var;
              prev_lag = lag;
              count_col_other_endo++;
            }
          expr_t id = it->second;

          FNUMEXPR_ fnumexpr(FirstOtherEndoDerivative, eqr, varr, lag);
          fnumexpr.write(code_file, instruction_number);
          id->compile(code_file, instruction_number, false, temporary_terms, map_idx, true, false);
          FSTPG3_ fstpg3(eq, var, lag, count_col_other_endo-1);
          fstpg3.write(code_file, instruction_number);
        }

      // Set codefile position to previous JMP_ and set the number of instructions to jump
      pos1 = code_file.tellp();
      code_file.seekp(pos2);
      FJMP_ fjmp1(instruction_number - prev_instruction_number);
      fjmp1.write(code_file, instruction_number);
      code_file.seekp(pos1);
    }
  FENDBLOCK_ fendblock;
  fendblock.write(code_file, instruction_number);
  FEND_ fend;
  fend.write(code_file, instruction_number);
  code_file.close();
}

void
DynamicModel::writeDynamicMFile(const string &dynamic_basename) const
{
  string filename = dynamic_basename + ".m";

  ofstream mDynamicModelFile;
  mDynamicModelFile.open(filename.c_str(), ios::out | ios::binary);
  if (!mDynamicModelFile.is_open())
    {
      cerr << "Error: Can't open file " << filename << " for writing" << endl;
      exit(EXIT_FAILURE);
    }
  mDynamicModelFile << "function [residual, g1, g2, g3] = " << dynamic_basename << "(y, x, params, steady_state, it_)" << endl
                    << "%" << endl
                    << "% Status : Computes dynamic model for Dynare" << endl
                    << "%" << endl
                    << "% Inputs :" << endl
                    << "%   y         [#dynamic variables by 1] double    vector of endogenous variables in the order stored" << endl
                    << "%                                                 in M_.lead_lag_incidence; see the Manual" << endl
                    << "%   x         [nperiods by M_.exo_nbr] double     matrix of exogenous variables (in declaration order)" << endl
                    << "%                                                 for all simulation periods" << endl
                    << "%   steady_state  [M_.endo_nbr by 1] double       vector of steady state values" << endl
                    << "%   params    [M_.param_nbr by 1] double          vector of parameter values in declaration order" << endl
                    << "%   it_       scalar double                       time period for exogenous variables for which to evaluate the model" << endl
                    << "%" << endl
                    << "% Outputs:" << endl
                    << "%   residual  [M_.endo_nbr by 1] double    vector of residuals of the dynamic model equations in order of " << endl
                    << "%                                          declaration of the equations." << endl
                    << "%                                          Dynare may prepend auxiliary equations, see M_.aux_vars" << endl
                    << "%   g1        [M_.endo_nbr by #dynamic variables] double    Jacobian matrix of the dynamic model equations;" << endl
                    << "%                                                           rows: equations in order of declaration" << endl
                    << "%                                                           columns: variables in order stored in M_.lead_lag_incidence followed by the ones in M_.exo_names" << endl
                    << "%   g2        [M_.endo_nbr by (#dynamic variables)^2] double   Hessian matrix of the dynamic model equations;" << endl
                    << "%                                                              rows: equations in order of declaration" << endl
                    << "%                                                              columns: variables in order stored in M_.lead_lag_incidence followed by the ones in M_.exo_names" << endl
                    << "%   g3        [M_.endo_nbr by (#dynamic variables)^3] double   Third order derivative matrix of the dynamic model equations;" << endl
                    << "%                                                              rows: equations in order of declaration" << endl
                    << "%                                                              columns: variables in order stored in M_.lead_lag_incidence followed by the ones in M_.exo_names" << endl
                    << "%" << endl
                    << "%" << endl
                    << "% Warning : this file is generated automatically by Dynare" << endl
                    << "%           from model file (.mod)" << endl << endl;

  writeDynamicModel(mDynamicModelFile, false, false);
  mDynamicModelFile << "end" << endl; // Close *_dynamic function
  mDynamicModelFile.close();
}

void
DynamicModel::fillVarExpectationFunctionsToWrite()
{
  for (var_expectation_node_map_t::const_iterator it = var_expectation_node_map.begin();
       it != var_expectation_node_map.end(); it++)
    var_expectation_functions_to_write[it->first.first].insert(it->first.second.second);
}

map<string, set<int> >
DynamicModel::getVarExpectationFunctionsToWrite() const
{
  return var_expectation_functions_to_write;
}

void
DynamicModel::writeVarExpectationCalls(ostream &output) const
{
  for (map<string, set<int> >::const_iterator it = var_expectation_functions_to_write.begin();
       it != var_expectation_functions_to_write.end(); it++)
    {
      int i = 0;
      output << "dynamic_var_forecast_" << it->first << " = "
             << "var_forecast_" << it->first << "(y);" << endl;

      for (set<int>::const_iterator it1 = it->second.begin(); it1 != it->second.end(); it1++)
        output << "dynamic_var_forecast_" << it->first << "_" << *it1 << " = "
               << "dynamic_var_forecast_" << it->first << "(" << ++i << ", :);" << endl;
    }
}

void
DynamicModel::writeDynamicJuliaFile(const string &basename) const
{
  string filename = basename + "Dynamic.jl";

  ofstream output;
  output.open(filename.c_str(), ios::out | ios::binary);
  if (!output.is_open())
    {
      cerr << "Error: Can't open file " << filename << " for writing" << endl;
      exit(EXIT_FAILURE);
    }

  output << "module " << basename << "Dynamic" << endl
         << "#" << endl
         << "# NB: this file was automatically generated by Dynare" << endl
         << "#     from " << basename << ".mod" << endl
         << "#" << endl
         << "using Utils" << endl << endl
         << "export dynamic!" << endl << endl;
  writeDynamicModel(output, false, true);
  output << "end" << endl;
  output.close();
}

void
DynamicModel::writeDynamicCFile(const string &dynamic_basename, const int order) const
{
  string filename = dynamic_basename + ".c";
  string filename_mex = dynamic_basename + "_mex.c";
  ofstream mDynamicModelFile, mDynamicMexFile;

  mDynamicModelFile.open(filename.c_str(), ios::out | ios::binary);
  if (!mDynamicModelFile.is_open())
    {
      cerr << "Error: Can't open file " << filename << " for writing" << endl;
      exit(EXIT_FAILURE);
    }
  mDynamicModelFile << "/*" << endl
                    << " * " << filename << " : Computes dynamic model for Dynare" << endl
                    << " *" << endl
                    << " * Warning : this file is generated automatically by Dynare" << endl
                    << " *           from model file (.mod)" << endl
                    << " */" << endl
#if defined(_WIN32) || defined(__CYGWIN32__) || defined(__MINGW32__)
                    << "#ifdef _MSC_VER" << endl
                    << "#define _USE_MATH_DEFINES" << endl
                    << "#endif" << endl
#endif
                    << "#include <math.h>" << endl;

  if (external_functions_table.get_total_number_of_unique_model_block_external_functions())
    // External Matlab function, implies Dynamic function will call mex
    mDynamicModelFile << "#include \"mex.h\"" << endl;
  else
    mDynamicModelFile << "#include <stdlib.h>" << endl;

  mDynamicModelFile << "#define max(a, b) (((a) > (b)) ? (a) : (b))" << endl
                    << "#define min(a, b) (((a) > (b)) ? (b) : (a))" << endl;

  // Write function definition if oPowerDeriv is used
  writePowerDerivCHeader(mDynamicModelFile);
  writeNormcdfCHeader(mDynamicModelFile);

  // Writing the function body
  writeDynamicModel(mDynamicModelFile, true, false);

  writePowerDeriv(mDynamicModelFile);
  writeNormcdf(mDynamicModelFile);
  mDynamicModelFile.close();

  mDynamicMexFile.open(filename_mex.c_str(), ios::out | ios::binary);
  if (!mDynamicMexFile.is_open())
    {
      cerr << "Error: Can't open file " << filename_mex << " for writing" << endl;
      exit(EXIT_FAILURE);
    }

  // Writing the gateway routine
  mDynamicMexFile << "/*" << endl
                  << " * " << filename_mex << " : The gateway routine used to call the Dynamic function "
                  << "located in " << filename << endl
                  << " *" << endl
                  << " * Warning : this file is generated automatically by Dynare" << endl
                  << " *           from model file (.mod)" << endl
                  << endl
                  << " */" << endl << endl
                  << "#include \"mex.h\"" << endl << endl
                  << "void Dynamic(double *y, double *x, int nb_row_x, double *params, double *steady_state, int it_, double *residual, double *g1, double *v2, double *v3);" << endl
                  << "void mexFunction(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])" << endl
                  << "{" << endl
                  << "  double *y, *x, *params, *steady_state;" << endl
                  << "  double *residual, *g1, *v2, *v3;" << endl
                  << "  int nb_row_x, it_;" << endl
                  << endl
                  << "  /* Check that no derivatives of higher order than computed are being requested */" << endl
                  << "  if (nlhs > " << order + 1 << ")" << endl
                  << "    mexErrMsgTxt(\"Derivatives of higher order than computed have been requested\");" << endl
                  << "  /* Create a pointer to the input matrix y. */" << endl
                  << "  y = mxGetPr(prhs[0]);" << endl
                  << endl
                  << "  /* Create a pointer to the input matrix x. */" << endl
                  << "  x = mxGetPr(prhs[1]);" << endl
                  << endl
                  << "  /* Create a pointer to the input matrix params. */" << endl
                  << "  params = mxGetPr(prhs[2]);" << endl
                  << endl
                  << "  /* Create a pointer to the input matrix steady_state. */" << endl
                  << "  steady_state = mxGetPr(prhs[3]);" << endl
                  << endl
                  << "  /* Fetch time index */" << endl
                  << "  it_ = (int) mxGetScalar(prhs[4]) - 1;" << endl
                  << endl
                  << "  /* Gets number of rows of matrix x. */" << endl
                  << "  nb_row_x = mxGetM(prhs[1]);" << endl
                  << endl
                  << "  residual = NULL;" << endl
                  << "  if (nlhs >= 1)" << endl
                  << "  {" << endl
                  << "     /* Set the output pointer to the output matrix residual. */" << endl
                  << "     plhs[0] = mxCreateDoubleMatrix(" << equations.size() << ",1, mxREAL);" << endl
                  << "     /* Create a C pointer to a copy of the output matrix residual. */" << endl
                  << "     residual = mxGetPr(plhs[0]);" << endl
                  << "  }" << endl
                  << endl
                  << "  g1 = NULL;" << endl
                  << "  if (nlhs >= 2)" << endl
                  << "  {" << endl
                  << "     /* Set the output pointer to the output matrix g1. */" << endl
                  << "     plhs[1] = mxCreateDoubleMatrix(" << equations.size() << ", " << dynJacobianColsNbr << ", mxREAL);" << endl
                  << "     /* Create a C pointer to a copy of the output matrix g1. */" << endl
                  << "     g1 = mxGetPr(plhs[1]);" << endl
                  << "  }" << endl
                  << endl
                  << "  v2 = NULL;" << endl
                  << " if (nlhs >= 3)" << endl
                  << "  {" << endl
                  << "     /* Set the output pointer to the output matrix v2. */" << endl
                  << "     plhs[2] = mxCreateDoubleMatrix(" << NNZDerivatives[1] << ", " << 3
                  << ", mxREAL);" << endl
                  << "     v2 = mxGetPr(plhs[2]);" << endl
                  << "  }" << endl
                  << endl
                  << "  v3 = NULL;" << endl
                  << " if (nlhs >= 4)" << endl
                  << "  {" << endl
                  << "     /* Set the output pointer to the output matrix v3. */" << endl
                  << "     plhs[3] = mxCreateDoubleMatrix(" << NNZDerivatives[2] << ", " << 3 << ", mxREAL);" << endl
                  << "     v3 = mxGetPr(plhs[3]);" << endl
                  << "  }" << endl
                  << endl
                  << "  /* Call the C subroutines. */" << endl
                  << "  Dynamic(y, x, nb_row_x, params, steady_state, it_, residual, g1, v2, v3);" << endl
                  << "}" << endl;
  mDynamicMexFile.close();
}

string
DynamicModel::reform(const string name1) const
{
  string name = name1;
  int pos = name.find("\\", 0);
  while (pos >= 0)
    {
      if (name.substr(pos + 1, 1) != "\\")
        {
          name = name.insert(pos, "\\");
          pos++;
        }
      pos++;
      pos = name.find("\\", pos);
    }
  return (name);
}

void
DynamicModel::printNonZeroHessianEquations(ostream &output) const
{
  if (nonzero_hessian_eqs.size() !=  1)
    output << "[";
  for (map<int, string>::const_iterator it = nonzero_hessian_eqs.begin();
       it != nonzero_hessian_eqs.end(); it++)
    {
      if (it != nonzero_hessian_eqs.begin())
        output << " ";
      output << it->first;
    }
  if (nonzero_hessian_eqs.size() != 1)
    output << "]";
}

void
DynamicModel::setNonZeroHessianEquations(map<int, string> &eqs)
{
  for (second_derivatives_t::const_iterator it = second_derivatives.begin();
       it != second_derivatives.end(); it++)
    if (nonzero_hessian_eqs.find(it->first.first) == nonzero_hessian_eqs.end())
      {
        nonzero_hessian_eqs[it->first.first] = "";
        for (size_t i = 0; i < equation_tags.size(); i++)
          if (equation_tags[i].first == it->first.first)
            if (equation_tags[i].second.first == "name")
              {
                nonzero_hessian_eqs[it->first.first] = equation_tags[i].second.second;
                break;
              }
      }
  eqs = nonzero_hessian_eqs;
}

void
DynamicModel::Write_Inf_To_Bin_File_Block(const string &dynamic_basename, const string &bin_basename, const int &num,
                                          int &u_count_int, bool &file_open, bool is_two_boundaries) const
{
  int j;
  std::ofstream SaveCode;
  if (file_open)
    SaveCode.open((bin_basename + "_dynamic.bin").c_str(), ios::out | ios::in | ios::binary | ios::ate);
  else
    SaveCode.open((bin_basename + "_dynamic.bin").c_str(), ios::out | ios::binary);
  if (!SaveCode.is_open())
    {
      cerr << "Error : Can't open file \"" << bin_basename << "_dynamic.bin\" for writing" << endl;
      exit(EXIT_FAILURE);
    }
  u_count_int = 0;
  unsigned int block_size = getBlockSize(num);
  unsigned int block_mfs = getBlockMfs(num);
  unsigned int block_recursive = block_size - block_mfs;
  for (block_derivatives_equation_variable_laglead_nodeid_t::const_iterator it = blocks_derivatives[num].begin(); it != (blocks_derivatives[num]).end(); it++)
    {
      unsigned int eq = it->first.first;
      unsigned int var = it->first.second;
      int lag = it->second.first;
      if (lag != 0 && !is_two_boundaries)
        continue;
      if (eq >= block_recursive && var >= block_recursive)
        {
          int v = eq - block_recursive;
          SaveCode.write(reinterpret_cast<char *>(&v), sizeof(v));
          int varr = var - block_recursive + lag * block_mfs;
          SaveCode.write(reinterpret_cast<char *>(&varr), sizeof(varr));
          SaveCode.write(reinterpret_cast<char *>(&lag), sizeof(lag));
          int u = u_count_int + block_mfs;
          SaveCode.write(reinterpret_cast<char *>(&u), sizeof(u));
          u_count_int++;
        }
    }

  if (is_two_boundaries)
    u_count_int += block_mfs;
  for (j = block_recursive; j < (int) block_size; j++)
    {
      unsigned int varr = getBlockVariableID(num, j);
      SaveCode.write(reinterpret_cast<char *>(&varr), sizeof(varr));
    }
  for (j = block_recursive; j < (int) block_size; j++)
    {
      unsigned int eqr = getBlockEquationID(num, j);
      SaveCode.write(reinterpret_cast<char *>(&eqr), sizeof(eqr));
    }
  SaveCode.close();
}

void
DynamicModel::writeSparseDynamicMFile(const string &dynamic_basename, const string &basename) const
{
  string sp;
  ofstream mDynamicModelFile;
  ostringstream tmp, tmp1, tmp_eq;
  bool OK;
  chdir(basename.c_str());
  string filename = dynamic_basename + ".m";
  mDynamicModelFile.open(filename.c_str(), ios::out | ios::binary);
  if (!mDynamicModelFile.is_open())
    {
      cerr << "Error: Can't open file " << filename << " for writing" << endl;
      exit(EXIT_FAILURE);
    }
  mDynamicModelFile << "%\n";
  mDynamicModelFile << "% " << filename << " : Computes dynamic model for Dynare\n";
  mDynamicModelFile << "%\n";
  mDynamicModelFile << "% Warning : this file is generated automatically by Dynare\n";
  mDynamicModelFile << "%           from model file (.mod)\n\n";
  mDynamicModelFile << "%/\n";

  int Nb_SGE = 0;
  bool open_par = false;

  mDynamicModelFile << "function [varargout] = " << dynamic_basename << "(options_, M_, oo_, varargin)\n";
  mDynamicModelFile << "  g2=[];g3=[];\n";
  //Temporary variables declaration
  OK = true;
  ostringstream tmp_output;
  for (temporary_terms_t::const_iterator it = temporary_terms.begin();
       it != temporary_terms.end(); it++)
    {
      if (OK)
        OK = false;
      else
        tmp_output << " ";
      (*it)->writeOutput(tmp_output, oMatlabStaticModelSparse, temporary_terms);
    }
  if (tmp_output.str().length() > 0)
    mDynamicModelFile << "  global " << tmp_output.str() << ";\n";

  mDynamicModelFile << "  T_init=zeros(1,options_.periods+M_.maximum_lag+M_.maximum_lead);\n";
  tmp_output.str("");
  for (temporary_terms_t::const_iterator it = temporary_terms.begin();
       it != temporary_terms.end(); it++)
    {
      tmp_output << "  ";
      (*it)->writeOutput(tmp_output, oMatlabDynamicModel, temporary_terms);
      tmp_output << "=T_init;\n";
    }
  if (tmp_output.str().length() > 0)
    mDynamicModelFile << tmp_output.str();

  mDynamicModelFile << "  y_kmin=M_.maximum_lag;" << endl
                    << "  y_kmax=M_.maximum_lead;" << endl
                    << "  y_size=M_.endo_nbr;" << endl
                    << "  if(length(varargin)>0)" << endl
                    << "    %it is a simple evaluation of the dynamic model for time _it" << endl
                    << "    y=varargin{1};" << endl
                    << "    x=varargin{2};" << endl
                    << "    params=varargin{3};" << endl
                    << "    steady_state=varargin{4};" << endl
                    << "    it_=varargin{5};" << endl
                    << "    dr=varargin{6};" << endl
                    << "    Per_u_=0;" << endl
                    << "    Per_y_=it_*y_size;" << endl
                    << "    ys=y(it_,:);" << endl;
  tmp.str("");
  tmp_eq.str("");
  unsigned int nb_blocks = getNbBlocks();
  unsigned int block = 0;
  for (int count_call = 1; block < nb_blocks; block++, count_call++)
    {
      unsigned int block_size = getBlockSize(block);
      unsigned int block_mfs = getBlockMfs(block);
      unsigned int block_recursive = block_size - block_mfs;
      BlockSimulationType simulation_type = getBlockSimulationType(block);

      if (simulation_type == EVALUATE_FORWARD || simulation_type == EVALUATE_BACKWARD)
        {
          for (unsigned int ik = 0; ik < block_size; ik++)
            {
              tmp << " " << getBlockVariableID(block, ik)+1;
              tmp_eq << " " << getBlockEquationID(block, ik)+1;
            }
        }
      else
        {
          for (unsigned int ik = block_recursive; ik < block_size; ik++)
            {
              tmp << " " << getBlockVariableID(block, ik)+1;
              tmp_eq << " " << getBlockEquationID(block, ik)+1;
            }
        }
      mDynamicModelFile << "    y_index_eq=[" << tmp_eq.str() << "];\n";
      mDynamicModelFile << "    y_index=[" << tmp.str() << "];\n";

      switch (simulation_type)
        {
        case EVALUATE_FORWARD:
        case EVALUATE_BACKWARD:
          mDynamicModelFile << "    [y, dr(" << count_call << ").g1, dr(" << count_call << ").g2, dr(" << count_call << ").g3, dr(" << count_call << ").g1_x, dr(" << count_call << ").g1_xd, dr(" << count_call << ").g1_o]=" << dynamic_basename << "_" << block + 1 << "(y, x, params, steady_state, 1, it_-1, 1);\n";
          mDynamicModelFile << "    residual(y_index_eq)=ys(y_index)-y(it_, y_index);\n";
          break;
        case SOLVE_FORWARD_SIMPLE:
        case SOLVE_BACKWARD_SIMPLE:
          mDynamicModelFile << "    [r, y, dr(" << count_call << ").g1, dr(" << count_call << ").g2, dr(" << count_call << ").g3, dr(" << count_call << ").g1_x, dr(" << count_call << ").g1_xd, dr(" << count_call << ").g1_o]=" << dynamic_basename << "_" << block + 1 << "(y, x, params, steady_state, it_, 1);\n";
          mDynamicModelFile << "    residual(y_index_eq)=r;\n";
          break;
        case SOLVE_FORWARD_COMPLETE:
        case SOLVE_BACKWARD_COMPLETE:
          mDynamicModelFile << "    [r, y, dr(" << count_call << ").g1, dr(" << count_call << ").g2, dr(" << count_call << ").g3, dr(" << count_call << ").g1_x, dr(" << count_call << ").g1_xd, dr(" << count_call << ").g1_o]=" << dynamic_basename << "_" << block + 1 << "(y, x, params, steady_state, it_, 1);\n";
          mDynamicModelFile << "    residual(y_index_eq)=r;\n";
          break;
        case SOLVE_TWO_BOUNDARIES_COMPLETE:
        case SOLVE_TWO_BOUNDARIES_SIMPLE:
          mDynamicModelFile << "    [r, y, dr(" << count_call << ").g1, dr(" << count_call << ").g2, dr(" << count_call << ").g3, b, dr(" << count_call << ").g1_x, dr(" << count_call << ").g1_xd, dr(" << count_call << ").g1_o]=" << dynamic_basename << "_" <<  block + 1 << "(y, x, params, steady_state, it_-" << max_lag << ", 1, " << max_lag << ", " << block_recursive << "," << "options_.periods" << ");\n";
          mDynamicModelFile << "    residual(y_index_eq)=r(:,M_.maximum_lag+1);\n";
          break;
        default:
          break;
        }
      tmp_eq.str("");
      tmp.str("");
    }
  if (tmp1.str().length())
    {
      mDynamicModelFile << tmp1.str();
      tmp1.str("");
    }
  mDynamicModelFile << "    varargout{1}=residual;" << endl
                    << "    varargout{2}=dr;" << endl
                    << "    return;" << endl
                    << "  end;" << endl
                    << "  %it is the deterministic simulation of the block decomposed dynamic model" << endl
                    << "  if(options_.stack_solve_algo==0)" << endl
                    << "    mthd='Sparse LU';" << endl
                    << "  elseif(options_.stack_solve_algo==1)" << endl
                    << "    mthd='Relaxation';" << endl
                    << "  elseif(options_.stack_solve_algo==2)" << endl
                    << "    mthd='GMRES';" << endl
                    << "  elseif(options_.stack_solve_algo==3)" << endl
                    << "    mthd='BICGSTAB';" << endl
                    << "  elseif(options_.stack_solve_algo==4)" << endl
                    << "    mthd='OPTIMPATH';" << endl
                    << "  else" << endl
                    << "    mthd='UNKNOWN';" << endl
                    << "  end;" << endl
                    << "  if options_.verbosity" << endl
                    << "    printline(41)" << endl
                    << "    disp(sprintf('MODEL SIMULATION (method=%s):',mthd))" << endl
                    << "    skipline()" << endl
                    << "  end" << endl
                    << "  periods=options_.periods;" << endl
                    << "  maxit_=options_.simul.maxit;" << endl
                    << "  solve_tolf=options_.solve_tolf;" << endl
                    << "  y=oo_.endo_simul';" << endl
                    << "  x=oo_.exo_simul;" << endl;

  mDynamicModelFile << "  params=M_.params;\n";
  mDynamicModelFile << "  steady_state=oo_.steady_state;\n";
  mDynamicModelFile << "  oo_.deterministic_simulation.status = 0;\n";
  for (block = 0; block < nb_blocks; block++)
    {
      unsigned int block_size = getBlockSize(block);
      unsigned int block_mfs = getBlockMfs(block);
      unsigned int block_recursive = block_size - block_mfs;
      BlockSimulationType simulation_type = getBlockSimulationType(block);

      if ((simulation_type == EVALUATE_FORWARD) && (block_size))
        {
          if (open_par)
            {
              mDynamicModelFile << "  end\n";
            }
          mDynamicModelFile << "  oo_.deterministic_simulation.status = 1;\n";
          mDynamicModelFile << "  oo_.deterministic_simulation.error = 0;\n";
          mDynamicModelFile << "  oo_.deterministic_simulation.iterations = 0;\n";
          mDynamicModelFile << "  if(isfield(oo_.deterministic_simulation,'block'))\n";
          mDynamicModelFile << "    blck_num = length(oo_.deterministic_simulation.block)+1;\n";
          mDynamicModelFile << "  else\n";
          mDynamicModelFile << "    blck_num = 1;\n";
          mDynamicModelFile << "  end;\n";
          mDynamicModelFile << "  oo_.deterministic_simulation.block(blck_num).status = 1;\n";
          mDynamicModelFile << "  oo_.deterministic_simulation.block(blck_num).error = 0;\n";
          mDynamicModelFile << "  oo_.deterministic_simulation.block(blck_num).iterations = 0;\n";
          mDynamicModelFile << "  g1=[];g2=[];g3=[];\n";
          mDynamicModelFile << "  y=" << dynamic_basename << "_" << block + 1 << "(y, x, params, steady_state, 0, y_kmin, periods);\n";
          mDynamicModelFile << "  tmp = y(:,M_.block_structure.block(" << block + 1 << ").variable);\n";
          mDynamicModelFile << "  if any(isnan(tmp) | isinf(tmp))\n";
          mDynamicModelFile << "    disp(['Inf or Nan value during the evaluation of block " << block <<"']);\n";
          mDynamicModelFile << "    oo_.deterministic_simulation.status = 0;\n";
          mDynamicModelFile << "    oo_.deterministic_simulation.error = 100;\n";
          mDynamicModelFile << "    varargout{1} = oo_;\n";
          mDynamicModelFile << "    return;\n";
          mDynamicModelFile << "  end;\n";
        }
      else if ((simulation_type == EVALUATE_BACKWARD) && (block_size))
        {
          if (open_par)
            {
              mDynamicModelFile << "  end\n";
            }
          mDynamicModelFile << "  oo_.deterministic_simulation.status = 1;\n";
          mDynamicModelFile << "  oo_.deterministic_simulation.error = 0;\n";
          mDynamicModelFile << "  oo_.deterministic_simulation.iterations = 0;\n";
          mDynamicModelFile << "  if(isfield(oo_.deterministic_simulation,'block'))\n";
          mDynamicModelFile << "    blck_num = length(oo_.deterministic_simulation.block)+1;\n";
          mDynamicModelFile << "  else\n";
          mDynamicModelFile << "    blck_num = 1;\n";
          mDynamicModelFile << "  end;\n";
          mDynamicModelFile << "  oo_.deterministic_simulation.block(blck_num).status = 1;\n";
          mDynamicModelFile << "  oo_.deterministic_simulation.block(blck_num).error = 0;\n";
          mDynamicModelFile << "  oo_.deterministic_simulation.block(blck_num).iterations = 0;\n";
          mDynamicModelFile << "  g1=[];g2=[];g3=[];\n";
          mDynamicModelFile << "  " << dynamic_basename << "_" << block + 1 << "(y, x, params, steady_state, 0, y_kmin, periods);\n";
          mDynamicModelFile << "  tmp = y(:,M_.block_structure.block(" << block + 1 << ").variable);\n";
          mDynamicModelFile << "  if any(isnan(tmp) | isinf(tmp))\n";
          mDynamicModelFile << "    disp(['Inf or Nan value during the evaluation of block " << block <<"']);\n";
          mDynamicModelFile << "    oo_.deterministic_simulation.status = 0;\n";
          mDynamicModelFile << "    oo_.deterministic_simulation.error = 100;\n";
          mDynamicModelFile << "    varargout{1} = oo_;\n";
          mDynamicModelFile << "    return;\n";
          mDynamicModelFile << "  end;\n";
        }
      else if ((simulation_type == SOLVE_FORWARD_COMPLETE || simulation_type == SOLVE_FORWARD_SIMPLE) && (block_size))
        {
          if (open_par)
            mDynamicModelFile << "  end\n";
          open_par = false;
          mDynamicModelFile << "  g1=0;\n";
          mDynamicModelFile << "  r=0;\n";
          tmp.str("");
          for (unsigned int ik = block_recursive; ik < block_size; ik++)
            {
              tmp << " " << getBlockVariableID(block, ik)+1;
            }
          mDynamicModelFile << "  y_index = [" << tmp.str() << "];\n";
          int nze = blocks_derivatives[block].size();
          mDynamicModelFile << "  if(isfield(oo_.deterministic_simulation,'block'))\n";
          mDynamicModelFile << "    blck_num = length(oo_.deterministic_simulation.block)+1;\n";
          mDynamicModelFile << "  else\n";
          mDynamicModelFile << "    blck_num = 1;\n";
          mDynamicModelFile << "  end;\n";
          mDynamicModelFile << "  y = solve_one_boundary('"  << dynamic_basename << "_" <<  block + 1 << "'"
                            <<", y, x, params, steady_state, y_index, " << nze
                            <<", options_.periods, " << blocks_linear[block]
                            <<", blck_num, y_kmin, options_.simul.maxit, options_.solve_tolf, options_.slowc, " << cutoff << ", options_.stack_solve_algo, 1, 1, 0);\n";
          mDynamicModelFile << "  tmp = y(:,M_.block_structure.block(" << block + 1 << ").variable);\n";
          mDynamicModelFile << "  if any(isnan(tmp) | isinf(tmp))\n";
          mDynamicModelFile << "    disp(['Inf or Nan value during the resolution of block " << block <<"']);\n";
          mDynamicModelFile << "    oo_.deterministic_simulation.status = 0;\n";
          mDynamicModelFile << "    oo_.deterministic_simulation.error = 100;\n";
          mDynamicModelFile << "    varargout{1} = oo_;\n";
          mDynamicModelFile << "    return;\n";
          mDynamicModelFile << "  end;\n";
        }
      else if ((simulation_type == SOLVE_BACKWARD_COMPLETE || simulation_type == SOLVE_BACKWARD_SIMPLE) && (block_size))
        {
          if (open_par)
            mDynamicModelFile << "  end\n";
          open_par = false;
          mDynamicModelFile << "  g1=0;\n";
          mDynamicModelFile << "  r=0;\n";
          tmp.str("");
          for (unsigned int ik = block_recursive; ik < block_size; ik++)
            {
              tmp << " " << getBlockVariableID(block, ik)+1;
            }
          mDynamicModelFile << "  y_index = [" << tmp.str() << "];\n";
          int nze = blocks_derivatives[block].size();

          mDynamicModelFile << "  if(isfield(oo_.deterministic_simulation,'block'))\n";
          mDynamicModelFile << "    blck_num = length(oo_.deterministic_simulation.block)+1;\n";
          mDynamicModelFile << "  else\n";
          mDynamicModelFile << "    blck_num = 1;\n";
          mDynamicModelFile << "  end;\n";
          mDynamicModelFile << "  y = solve_one_boundary('"  << dynamic_basename << "_" <<  block + 1 << "'"
                            <<", y, x, params, steady_state, y_index, " << nze
                            <<", options_.periods, " << blocks_linear[block]
                            <<", blck_num, y_kmin, options_.simul.maxit, options_.solve_tolf, options_.slowc, " << cutoff << ", options_.stack_solve_algo, 1, 1, 0);\n";
          mDynamicModelFile << "  tmp = y(:,M_.block_structure.block(" << block + 1 << ").variable);\n";
          mDynamicModelFile << "  if any(isnan(tmp) | isinf(tmp))\n";
          mDynamicModelFile << "    disp(['Inf or Nan value during the resolution of block " << block <<"']);\n";
          mDynamicModelFile << "    oo_.deterministic_simulation.status = 0;\n";
          mDynamicModelFile << "    oo_.deterministic_simulation.error = 100;\n";
          mDynamicModelFile << "    varargout{1} = oo_;\n";
          mDynamicModelFile << "    return;\n";
          mDynamicModelFile << "  end;\n";
        }
      else if ((simulation_type == SOLVE_TWO_BOUNDARIES_COMPLETE || simulation_type == SOLVE_TWO_BOUNDARIES_SIMPLE) && (block_size))
        {
          if (open_par)
            mDynamicModelFile << "  end\n";
          open_par = false;
          Nb_SGE++;
          int nze = blocks_derivatives[block].size();
          mDynamicModelFile << "  y_index=[";
          for (unsigned int ik = block_recursive; ik < block_size; ik++)
            {
              mDynamicModelFile << " " << getBlockVariableID(block, ik)+1;
            }
          mDynamicModelFile << "  ];\n";
          mDynamicModelFile << "  if(isfield(oo_.deterministic_simulation,'block'))\n";
          mDynamicModelFile << "    blck_num = length(oo_.deterministic_simulation.block)+1;\n";
          mDynamicModelFile << "  else\n";
          mDynamicModelFile << "    blck_num = 1;\n";
          mDynamicModelFile << "  end;\n";
          mDynamicModelFile << "  [y oo_] = solve_two_boundaries('" << dynamic_basename << "_" <<  block + 1 << "'"
                            <<", y, x, params, steady_state, y_index, " << nze
                            <<", options_.periods, " << max_leadlag_block[block].first
                            <<", " << max_leadlag_block[block].second
                            <<", " << blocks_linear[block]
                            <<", blck_num, y_kmin, options_.simul.maxit, options_.solve_tolf, options_.slowc, " << cutoff << ", options_.stack_solve_algo, options_, M_, oo_);\n";
          mDynamicModelFile << "  tmp = y(:,M_.block_structure.block(" << block + 1 << ").variable);\n";
          mDynamicModelFile << "  if any(isnan(tmp) | isinf(tmp))\n";
          mDynamicModelFile << "    disp(['Inf or Nan value during the resolution of block " << block <<"']);\n";
          mDynamicModelFile << "    oo_.deterministic_simulation.status = 0;\n";
          mDynamicModelFile << "    oo_.deterministic_simulation.error = 100;\n";
          mDynamicModelFile << "    varargout{1} = oo_;\n";
          mDynamicModelFile << "    return;\n";
          mDynamicModelFile << "  end;\n";
        }
    }
  if (open_par)
    mDynamicModelFile << "  end;\n";
  open_par = false;
  mDynamicModelFile << "  oo_.endo_simul = y';\n";
  mDynamicModelFile << "  varargout{1} = oo_;\n";
  mDynamicModelFile << "return;\n";
  mDynamicModelFile << "end" << endl;

  mDynamicModelFile.close();

  writeModelEquationsOrdered_M(dynamic_basename);

  chdir("..");
}

void
DynamicModel::writeDynamicModel(ostream &DynamicOutput, bool use_dll, bool julia) const
{
  ostringstream model_local_vars_output;  // Used for storing model local vars
  ostringstream model_output;             // Used for storing model temp vars and equations
  ostringstream jacobian_output;          // Used for storing jacobian equations
  ostringstream hessian_output;           // Used for storing Hessian equations
  ostringstream third_derivatives_output; // Used for storing third order derivatives equations

  ExprNodeOutputType output_type = (use_dll ? oCDynamicModel :
                                    julia ? oJuliaDynamicModel : oMatlabDynamicModel);

  deriv_node_temp_terms_t tef_terms;
  temporary_terms_t temp_term_empty;
  temporary_terms_t temp_term_union = temporary_terms_res;
  temporary_terms_t temp_term_union_m_1;

  writeModelLocalVariables(model_local_vars_output, output_type, tef_terms);

  writeTemporaryTerms(temporary_terms_res, temp_term_union_m_1, model_output, output_type, tef_terms);

  writeModelEquations(model_output, output_type);

  int nrows = equations.size();
  int hessianColsNbr = dynJacobianColsNbr * dynJacobianColsNbr;

  // Writing Jacobian
  temp_term_union_m_1 = temp_term_union;
  temp_term_union.insert(temporary_terms_g1.begin(), temporary_terms_g1.end());
  if (!first_derivatives.empty())
    if (julia)
      writeTemporaryTerms(temp_term_union, temp_term_empty, jacobian_output, output_type, tef_terms);
    else
      writeTemporaryTerms(temp_term_union, temp_term_union_m_1, jacobian_output, output_type, tef_terms);
  for (first_derivatives_t::const_iterator it = first_derivatives.begin();
       it != first_derivatives.end(); it++)
    {
      int eq = it->first.first;
      int var = it->first.second;
      expr_t d1 = it->second;

      jacobianHelper(jacobian_output, eq, getDynJacobianCol(var), output_type);
      jacobian_output << "=";
      d1->writeOutput(jacobian_output, output_type, temp_term_union, tef_terms);
      jacobian_output << ";" << endl;
    }

  // Writing Hessian
  temp_term_union_m_1 = temp_term_union;
  temp_term_union.insert(temporary_terms_g2.begin(), temporary_terms_g2.end());
  if (!second_derivatives.empty())
    if (julia)
      writeTemporaryTerms(temp_term_union, temp_term_empty, hessian_output, output_type, tef_terms);
    else
      writeTemporaryTerms(temp_term_union, temp_term_union_m_1, hessian_output, output_type, tef_terms);
  int k = 0; // Keep the line of a 2nd derivative in v2
  for (second_derivatives_t::const_iterator it = second_derivatives.begin();
       it != second_derivatives.end(); it++)
    {
      int eq = it->first.first;
      int var1 = it->first.second.first;
      int var2 = it->first.second.second;
      expr_t d2 = it->second;

      int id1 = getDynJacobianCol(var1);
      int id2 = getDynJacobianCol(var2);

      int col_nb = id1 * dynJacobianColsNbr + id2;
      int col_nb_sym = id2 * dynJacobianColsNbr + id1;

      ostringstream for_sym;
      if (output_type == oJuliaDynamicModel)
        {
          for_sym << "g2[" << eq + 1 << "," << col_nb + 1 << "]";
          hessian_output << "  @inbounds " << for_sym.str() << " = ";
          d2->writeOutput(hessian_output, output_type, temp_term_union, tef_terms);
          hessian_output << endl;
        }
      else
        {
          sparseHelper(2, hessian_output, k, 0, output_type);
          hessian_output << "=" << eq + 1 << ";" << endl;

          sparseHelper(2, hessian_output, k, 1, output_type);
          hessian_output << "=" << col_nb + 1 << ";" << endl;

          sparseHelper(2, hessian_output, k, 2, output_type);
          hessian_output << "=";
          d2->writeOutput(hessian_output, output_type, temp_term_union, tef_terms);
          hessian_output << ";" << endl;

          k++;
        }

      // Treating symetric elements
      if (id1 != id2)
        if (output_type == oJuliaDynamicModel)
          hessian_output << "  @inbounds g2[" << eq + 1 << "," << col_nb_sym + 1 << "] = "
                         << for_sym.str() << endl;
        else
          {
            sparseHelper(2, hessian_output, k, 0, output_type);
            hessian_output << "=" << eq + 1 << ";" << endl;

            sparseHelper(2, hessian_output, k, 1, output_type);
            hessian_output << "=" << col_nb_sym + 1 << ";" << endl;

            sparseHelper(2, hessian_output, k, 2, output_type);
            hessian_output << "=";
            sparseHelper(2, hessian_output, k-1, 2, output_type);
            hessian_output << ";" << endl;

            k++;
          }
    }

  // Writing third derivatives
  temp_term_union_m_1 = temp_term_union;
  temp_term_union.insert(temporary_terms_g3.begin(), temporary_terms_g3.end());
  if (!third_derivatives.empty())
    if (julia)
      writeTemporaryTerms(temp_term_union, temp_term_empty, third_derivatives_output, output_type, tef_terms);
    else
      writeTemporaryTerms(temp_term_union, temp_term_union_m_1, third_derivatives_output, output_type, tef_terms);
  k = 0; // Keep the line of a 3rd derivative in v3
  for (third_derivatives_t::const_iterator it = third_derivatives.begin();
       it != third_derivatives.end(); it++)
    {
      int eq = it->first.first;
      int var1 = it->first.second.first;
      int var2 = it->first.second.second.first;
      int var3 = it->first.second.second.second;
      expr_t d3 = it->second;

      int id1 = getDynJacobianCol(var1);
      int id2 = getDynJacobianCol(var2);
      int id3 = getDynJacobianCol(var3);

      // Reference column number for the g3 matrix
      int ref_col = id1 * hessianColsNbr + id2 * dynJacobianColsNbr + id3;

      ostringstream for_sym;
      if (output_type == oJuliaDynamicModel)
        {
          for_sym << "g3[" << eq + 1 << "," << ref_col + 1 << "]";
          third_derivatives_output << "  @inbounds " << for_sym.str() << " = ";
          d3->writeOutput(third_derivatives_output, output_type, temp_term_union, tef_terms);
          third_derivatives_output << endl;
        }
      else
        {
          sparseHelper(3, third_derivatives_output, k, 0, output_type);
          third_derivatives_output << "=" << eq + 1 << ";" << endl;

          sparseHelper(3, third_derivatives_output, k, 1, output_type);
          third_derivatives_output << "=" << ref_col + 1 << ";" << endl;

          sparseHelper(3, third_derivatives_output, k, 2, output_type);
          third_derivatives_output << "=";
          d3->writeOutput(third_derivatives_output, output_type, temp_term_union, tef_terms);
          third_derivatives_output << ";" << endl;
        }

      // Compute the column numbers for the 5 other permutations of (id1,id2,id3)
      // and store them in a set (to avoid duplicates if two indexes are equal)
      set<int> cols;
      cols.insert(id1 * hessianColsNbr + id3 * dynJacobianColsNbr + id2);
      cols.insert(id2 * hessianColsNbr + id1 * dynJacobianColsNbr + id3);
      cols.insert(id2 * hessianColsNbr + id3 * dynJacobianColsNbr + id1);
      cols.insert(id3 * hessianColsNbr + id1 * dynJacobianColsNbr + id2);
      cols.insert(id3 * hessianColsNbr + id2 * dynJacobianColsNbr + id1);

      int k2 = 1; // Keeps the offset of the permutation relative to k
      for (set<int>::iterator it2 = cols.begin(); it2 != cols.end(); it2++)
        if (*it2 != ref_col)
          if (output_type == oJuliaDynamicModel)
            third_derivatives_output << "  @inbounds g3[" << eq + 1 << "," << *it2 + 1 << "] = "
                                     << for_sym.str() << endl;
          else
            {
              sparseHelper(3, third_derivatives_output, k+k2, 0, output_type);
              third_derivatives_output << "=" << eq + 1 << ";" << endl;

              sparseHelper(3, third_derivatives_output, k+k2, 1, output_type);
              third_derivatives_output << "=" << *it2 + 1 << ";" << endl;

              sparseHelper(3, third_derivatives_output, k+k2, 2, output_type);
              third_derivatives_output << "=";
              sparseHelper(3, third_derivatives_output, k, 2, output_type);
              third_derivatives_output << ";" << endl;

              k2++;
            }
      k += k2;
    }

  if (output_type == oMatlabDynamicModel)
    {
      // Check that we don't have more than 32 nested parenthesis because Matlab does not suppor this. See Issue #1201
      map<string, string> tmp_paren_vars;
      bool message_printed = false;
      fixNestedParenthesis(model_output, tmp_paren_vars, message_printed);
      fixNestedParenthesis(model_local_vars_output, tmp_paren_vars, message_printed);
      fixNestedParenthesis(jacobian_output, tmp_paren_vars, message_printed);
      fixNestedParenthesis(hessian_output, tmp_paren_vars, message_printed);
      fixNestedParenthesis(third_derivatives_output, tmp_paren_vars, message_printed);

      DynamicOutput << "%" << endl
                    << "% Model equations" << endl
                    << "%" << endl
                    << endl;
      writeVarExpectationCalls(DynamicOutput);
      DynamicOutput << "residual = zeros(" << nrows << ", 1);" << endl
                    << model_local_vars_output.str()
                    << model_output.str()
        // Writing initialization instruction for matrix g1
                    << "if nargout >= 2," << endl
                    << "  g1 = zeros(" << nrows << ", " << dynJacobianColsNbr << ");" << endl
                    << endl
                    << "  %" << endl
                    << "  % Jacobian matrix" << endl
                    << "  %" << endl
                    << endl
                    << jacobian_output.str()
                    << endl

        // Initialize g2 matrix
                    << "if nargout >= 3," << endl
                    << "  %" << endl
                    << "  % Hessian matrix" << endl
                    << "  %" << endl
                    << endl;
      if (second_derivatives.size())
        DynamicOutput << "  v2 = zeros(" << NNZDerivatives[1] << ",3);" << endl
                      << hessian_output.str()
                      << "  g2 = sparse(v2(:,1),v2(:,2),v2(:,3)," << nrows << "," << hessianColsNbr << ");" << endl;
      else // Either hessian is all zero, or we didn't compute it
        DynamicOutput << "  g2 = sparse([],[],[]," << nrows << "," << hessianColsNbr << ");" << endl;

      // Initialize g3 matrix
      DynamicOutput << "if nargout >= 4," << endl
                    << "  %" << endl
                    << "  % Third order derivatives" << endl
                    << "  %" << endl
                    << endl;
      int ncols = hessianColsNbr * dynJacobianColsNbr;
      if (third_derivatives.size())
        DynamicOutput << "  v3 = zeros(" << NNZDerivatives[2] << ",3);" << endl
                      << third_derivatives_output.str()
                      << "  g3 = sparse(v3(:,1),v3(:,2),v3(:,3)," << nrows << "," << ncols << ");" << endl;
      else // Either 3rd derivatives is all zero, or we didn't compute it
        DynamicOutput << "  g3 = sparse([],[],[]," << nrows << "," << ncols << ");" << endl;

      DynamicOutput << "end" << endl
                    << "end" << endl
                    << "end" << endl;
    }
  else if (output_type == oCDynamicModel)
    {
      DynamicOutput << "void Dynamic(double *y, double *x, int nb_row_x, double *params, double *steady_state, int it_, double *residual, double *g1, double *v2, double *v3)" << endl
                    << "{" << endl
                    << "  double lhs, rhs;" << endl
                    << endl
                    << "  /* Residual equations */" << endl
                    << model_local_vars_output.str()
                    << model_output.str()
                    << "  /* Jacobian  */" << endl
                    << "  if (g1 == NULL)" << endl
                    << "    return;" << endl
                    << endl
                    << jacobian_output.str()
                    << endl;

      if (second_derivatives.size())
        DynamicOutput << "  /* Hessian for endogenous and exogenous variables */" << endl
                      << "  if (v2 == NULL)" << endl
                      << "    return;" << endl
                      << endl
                      << hessian_output.str()
                      << endl;

      if (third_derivatives.size())
        DynamicOutput << "  /* Third derivatives for endogenous and exogenous variables */" << endl
                      << "  if (v3 == NULL)" << endl
                      << "    return;" << endl
                      << endl
                      << third_derivatives_output.str()
                      << endl;

      DynamicOutput << "}" << endl << endl;
    }
  else
    {
      ostringstream comments0;
      comments0 << "## Function Arguments" << endl
		<< endl
		<< "## Input" << endl
		<< " 1 y:            Array{Float64, num_dynamic_vars, 1}             Vector of endogenous variables in the order stored" << endl
		<< "                                                                 in model_.lead_lag_incidence; see the manual" << endl
		<< " 2 x:            Array{Float64, nperiods, length(model_.exo)}    Matrix of exogenous variables (in declaration order)" << endl
		<< "                                                                 for all simulation periods" << endl
		<< " 3 params:       Array{Float64, length(model_.param), 1}         Vector of parameter values in declaration order" << endl
		<< " 4 steady_state:" << endl
		<< " 5 it_:          Int                                             Time period for exogenous variables for which to evaluate the model" << endl
		<< endl;
      ostringstream comments1;
      comments1 << comments0.str() << endl;
      comments0	<< "## Output" << endl
		<< " 6 residual:     Array(Float64, model_.eq_nbr, 1)                Vector of residuals of the dynamic model equations in" << endl
		<< "                                                                 order of declaration of the equations." << endl;

      DynamicOutput << "function dynamic!(y::Vector{Float64}, x::Matrix{Float64}, "
                    << "params::Vector{Float64}," << endl
                    << "                  steady_state::Vector{Float64}, it_::Int, "
                    << "residual::Vector{Float64})" << endl
                    << "#=" << endl << comments0.str() << "=#" << endl
                    << "  @assert length(y)+size(x, 2) == " << dynJacobianColsNbr << endl
                    << "  @assert length(params) == " << symbol_table.param_nbr() << endl
                    << "  @assert length(residual) == " << nrows << endl
                    << "  #" << endl
                    << "  # Model equations" << endl
                    << "  #" << endl
                    << model_local_vars_output.str()
                    << model_output.str()
                    << "end" << endl << endl
                    << "function dynamic!(y::Vector{Float64}, x::Matrix{Float64}, "
                    << "params::Vector{Float64}," << endl
                    << "                  steady_state::Vector{Float64}, it_::Int, "
                    << "residual::Vector{Float64}," << endl
                    << "                  g1::Matrix{Float64})" << endl;

      comments0 << " 7 g1:           Array(Float64, model_.eq_nbr, num_dynamic_vars) Jacobian matrix of the dynamic model equations;" << endl
		<< "                                                                 rows: equations in order of declaration" << endl
		<< "                                                                 columns: variables in order stored in model_.lead_lag_incidence" << endl;

      DynamicOutput << "#=" << endl << comments0.str() << "=#" << endl
                    << "  @assert size(g1) == (" << nrows << ", " << dynJacobianColsNbr << ")" << endl
                    << "  fill!(g1, 0.0)" << endl
                    << "  dynamic!(y, x, params, steady_state, it_, residual)" << endl
                    << model_local_vars_output.str()
                    << "  #" << endl
                    << "  # Jacobian matrix" << endl
                    << "  #" << endl
                    << jacobian_output.str()
                    << "end" << endl << endl
                    << "function dynamic!(y::Vector{Float64}, x::Matrix{Float64}, "
                    << "params::Vector{Float64}," << endl
                    << "                  steady_state::Vector{Float64}, it_::Int, "
                    << "g1::Matrix{Float64})" << endl;

      comments1 << " 6 g1:           Array(Float64, model_.eq_nbr, num_dynamic_vars) Jacobian matrix of the dynamic model equations;" << endl
		<< "                                                                 rows: equations in order of declaration" << endl
		<< "                                                                 columns: variables in order stored in model_.lead_lag_incidence" << endl;
       	
      DynamicOutput << "#=" << endl << comments1.str() << "=#" << endl
                    << "  @assert size(g1) == (" << nrows << ", " << dynJacobianColsNbr << ")" << endl
                    << "  fill!(g1, 0.0)" << endl
                    << model_local_vars_output.str()
                    << "  #" << endl
                    << "  # Jacobian matrix" << endl
                    << "  #" << endl
                    << jacobian_output.str()
                    << "end" << endl << endl	
                    << "function dynamic!(y::Vector{Float64}, x::Matrix{Float64}, "
                    << "params::Vector{Float64}," << endl
                    << "                  steady_state::Vector{Float64}, it_::Int, "
                    << "residual::Vector{Float64}," << endl
                    << "                  g1::Matrix{Float64}, g2::Matrix{Float64})" << endl;

      comments0 << " 8 g2:           spzeros(model_.eq_nbr, (num_dynamic_vars)^2)    Hessian matrix of the dynamic model equations;" << endl
		<< "                                                                 rows: equations in order of declaration" << endl
		<< "                                                                 columns: variables in order stored in model_.lead_lag_incidence" << endl;

      DynamicOutput << "#=" << endl << comments0.str() << "=#" << endl
                    << "  @assert size(g2) == (" << nrows << ", " << hessianColsNbr << ")" << endl
                    << "  fill!(g2, 0.0)" << endl
                    << "  dynamic!(y, x, params, steady_state, it_, residual, g1)" << endl;
      if (second_derivatives.size())
        DynamicOutput << model_local_vars_output.str()
                      << "  #" << endl
                      << "  # Hessian matrix" << endl
                      << "  #" << endl
                      << hessian_output.str();

      // Initialize g3 matrix
      int ncols = hessianColsNbr * dynJacobianColsNbr;
      DynamicOutput << "end" << endl << endl
                    << "function dynamic!(y::Vector{Float64}, x::Matrix{Float64}, "
                    << "params::Vector{Float64}," << endl
                    << "                  steady_state::Vector{Float64}, it_::Int, "
                    << "residual::Vector{Float64}," << endl
                    << "                  g1::Matrix{Float64}, g2::Matrix{Float64}, g3::Matrix{Float64})" << endl;

      comments0 << " 9 g3:           spzeros(model_.eq_nbr, (num_dynamic_vars)^3)    Third order derivative matrix of the dynamic model equations;" << endl
		<< "                                                                 rows: equations in order of declaration" << endl
		<< "                                                                 columns: variables in order stored in model_.lead_lag_incidence" << endl;

      DynamicOutput << "#=" << endl << comments0.str() << "=#" << endl
                    << "  @assert size(g3) == (" << nrows << ", " << ncols << ")" << endl
                    << "  fill!(g3, 0.0)" << endl
                    << "  dynamic!(y, x, params, steady_state, it_, residual, g1, g2)" << endl;
      if (third_derivatives.size())
        DynamicOutput << model_local_vars_output.str()
                      << "  #" << endl
                      << "  # Third order derivatives" << endl
                      << "  #" << endl
                      << third_derivatives_output.str();
      DynamicOutput << "end" << endl;
    }
}

void
DynamicModel::writeOutput(ostream &output, const string &basename, bool block_decomposition, bool byte_code, bool use_dll, int order, bool estimation_present, bool compute_xrefs, bool julia) const
{
  /* Writing initialisation for M_.lead_lag_incidence matrix
     M_.lead_lag_incidence is a matrix with as many columns as there are
     endogenous variables and as many rows as there are periods in the
     models (nbr of rows = M_.max_lag+M_.max_lead+1)

     The matrix elements are equal to zero if a variable isn't present in the
     model at a given period.
  */

  string modstruct;
  string outstruct;
  if (julia)
    {
      modstruct = "model_.";
      outstruct = "oo_.";
    }
  else
    {
      modstruct = "M_.";
      outstruct = "oo_.";
    }

  output << modstruct << "orig_maximum_endo_lag = " << max_endo_lag_orig << ";" << endl
         << modstruct << "orig_maximum_endo_lead = " << max_endo_lead_orig << ";" << endl
         << modstruct << "orig_maximum_exo_lag = " << max_exo_lag_orig << ";" << endl
         << modstruct << "orig_maximum_exo_lead = " << max_exo_lead_orig << ";" << endl
         << modstruct << "orig_maximum_exo_det_lag = " << max_exo_det_lag_orig << ";" << endl
         << modstruct << "orig_maximum_exo_det_lead = " << max_exo_det_lead_orig << ";" << endl
         << modstruct << "orig_maximum_lag = " << max_lag_orig << ";" << endl
         << modstruct << "orig_maximum_lead = " << max_lead_orig << ";" << endl
         << modstruct << "lead_lag_incidence = [";
  // Loop on endogenous variables
  int nstatic = 0,
    nfwrd   = 0,
    npred   = 0,
    nboth   = 0;
  for (int endoID = 0; endoID < symbol_table.endo_nbr(); endoID++)
    {
      output << endl;
      int sstatic = 1,
        sfwrd   = 0,
        spred   = 0,
        sboth   = 0;
      // Loop on periods
      for (int lag = -max_endo_lag; lag <= max_endo_lead; lag++)
        {
          // Print variableID if exists with current period, otherwise print 0
          try
            {
              int varID = getDerivID(symbol_table.getID(eEndogenous, endoID), lag);
              output << " " << getDynJacobianCol(varID) + 1;
              if (lag == -1)
                {
                  sstatic = 0;
                  spred = 1;
                }
              else if (lag == 1)
                {
                  if (spred == 1)
                    {
                      sboth = 1;
                      spred = 0;
                    }
                  else
                    {
                      sstatic = 0;
                      sfwrd = 1;
                    }
                }
            }
          catch (UnknownDerivIDException &e)
            {
              output << " 0";
            }
        }
      nstatic += sstatic;
      nfwrd   += sfwrd;
      npred   += spred;
      nboth   += sboth;
      output << ";";
    }
  output << "]';" << endl;
  output << modstruct << "nstatic = " << nstatic << ";" << endl
         << modstruct << "nfwrd   = " << nfwrd   << ";" << endl
         << modstruct << "npred   = " << npred   << ";" << endl
         << modstruct << "nboth   = " << nboth   << ";" << endl
         << modstruct << "nsfwrd   = " << nfwrd+nboth   << ";" << endl
         << modstruct << "nspred   = " << npred+nboth   << ";" << endl
         << modstruct << "ndynamic   = " << npred+nboth+nfwrd << ";" << endl;

  // Write equation tags
  if (julia)
    {
      output << modstruct << "equation_tags = [" << endl;
      for (size_t i = 0; i < equation_tags.size(); i++)
        output << "                       EquationTag("
               << equation_tags[i].first + 1 << " , \""
               << equation_tags[i].second.first << "\" , \""
               << equation_tags[i].second.second << "\")" << endl;
      output << "                      ]" << endl;
    }
  else
    {
      output << modstruct << "equations_tags = {" << endl;
      for (size_t i = 0; i < equation_tags.size(); i++)
        output << "  " << equation_tags[i].first + 1 << " , '"
               << equation_tags[i].second.first << "' , '"
               << equation_tags[i].second.second << "' ;" << endl;
      output << "};" << endl;
    }

  /* Say if static and dynamic models differ (because of [static] and [dynamic]
     equation tags) */
  output << modstruct << "static_and_dynamic_models_differ = "
         << (static_only_equations.size() > 0 ?
             (julia ? "true" : "1") :
             (julia ? "false" : "0"))
         << ";" << endl;

  vector<int> state_var;
  for (int endoID = 0; endoID < symbol_table.endo_nbr(); endoID++)
    // Loop on periods
    for (int lag = -max_endo_lag; lag < 0; lag++)
      try
        {
          getDerivID(symbol_table.getID(eEndogenous, variable_reordered[endoID]), lag);
          if (lag < 0 && find(state_var.begin(), state_var.end(), variable_reordered[endoID]+1) == state_var.end())
            state_var.push_back(variable_reordered[endoID]+1);
        }
      catch (UnknownDerivIDException &e)
        {
        }

  //In case of sparse model, writes the block_decomposition structure of the model
  if (block_decomposition)
    {
      vector<int> state_equ;
      int count_lead_lag_incidence = 0;
      int max_lead, max_lag, max_lag_endo, max_lead_endo, max_lag_exo, max_lead_exo, max_lag_exo_det, max_lead_exo_det;
      unsigned int nb_blocks = getNbBlocks();
      for (unsigned int block = 0; block < nb_blocks; block++)
        {
          //For a block composed of a single equation determines wether we have to evaluate or to solve the equation
          count_lead_lag_incidence = 0;
          BlockSimulationType simulation_type = getBlockSimulationType(block);
          int block_size = getBlockSize(block);
          max_lag  = max_leadlag_block[block].first;
          max_lead = max_leadlag_block[block].second;
          max_lag_endo = endo_max_leadlag_block[block].first;
          max_lead_endo = endo_max_leadlag_block[block].second;
          max_lag_exo = exo_max_leadlag_block[block].first;
          max_lead_exo = exo_max_leadlag_block[block].second;
          max_lag_exo_det = exo_det_max_leadlag_block[block].first;
          max_lead_exo_det = exo_det_max_leadlag_block[block].second;
          ostringstream tmp_s, tmp_s_eq;
          tmp_s.str("");
          tmp_s_eq.str("");
          for (int i = 0; i < block_size; i++)
            {
              tmp_s << " " << getBlockVariableID(block, i)+1;
              tmp_s_eq << " " << getBlockEquationID(block, i)+1;
            }
          set<int> exogenous;
          exogenous.clear();
          for (lag_var_t::const_iterator it = exo_block[block].begin(); it != exo_block[block].end(); it++)
            for (var_t::const_iterator it1 = it->second.begin(); it1 != it->second.end(); it1++)
              exogenous.insert(*it1);
          set<int> exogenous_det;
          exogenous_det.clear();
          for (lag_var_t::const_iterator it = exo_det_block[block].begin(); it != exo_det_block[block].end(); it++)
            for (var_t::const_iterator it1 = it->second.begin(); it1 != it->second.end(); it1++)
              exogenous_det.insert(*it1);
          set<int> other_endogenous;
          other_endogenous.clear();
          for (lag_var_t::const_iterator it = other_endo_block[block].begin(); it != other_endo_block[block].end(); it++)
            for (var_t::const_iterator it1 = it->second.begin(); it1 != it->second.end(); it1++)
              other_endogenous.insert(*it1);
          output << "block_structure.block(" << block+1 << ").Simulation_Type = " << simulation_type << ";\n";
          output << "block_structure.block(" << block+1 << ").maximum_lag = " << max_lag << ";\n";
          output << "block_structure.block(" << block+1 << ").maximum_lead = " << max_lead << ";\n";
          output << "block_structure.block(" << block+1 << ").maximum_endo_lag = " << max_lag_endo << ";\n";
          output << "block_structure.block(" << block+1 << ").maximum_endo_lead = " << max_lead_endo << ";\n";
          output << "block_structure.block(" << block+1 << ").maximum_exo_lag = " << max_lag_exo << ";\n";
          output << "block_structure.block(" << block+1 << ").maximum_exo_lead = " << max_lead_exo << ";\n";
          output << "block_structure.block(" << block+1 << ").maximum_exo_det_lag = " << max_lag_exo_det << ";\n";
          output << "block_structure.block(" << block+1 << ").maximum_exo_det_lead = " << max_lead_exo_det << ";\n";
          output << "block_structure.block(" << block+1 << ").endo_nbr = " << block_size << ";\n";
          output << "block_structure.block(" << block+1 << ").mfs = " << getBlockMfs(block) << ";\n";
          output << "block_structure.block(" << block+1 << ").equation = [" << tmp_s_eq.str() << "];\n";
          output << "block_structure.block(" << block+1 << ").variable = [" << tmp_s.str() << "];\n";
          output << "block_structure.block(" << block+1 << ").exo_nbr = " << getBlockExoSize(block) << ";\n";
          output << "block_structure.block(" << block+1 << ").exogenous = [";
          int i = 0;
          for (set<int>::iterator it_exogenous = exogenous.begin(); it_exogenous != exogenous.end(); it_exogenous++)
            if (*it_exogenous >= 0)
              {
                output << " " << *it_exogenous+1;
                i++;
              }
          output << "];\n";

          output << "block_structure.block(" << block+1 << ").exogenous_det = [";
          i = 0;
          for (set<int>::iterator it_exogenous_det = exogenous_det.begin(); it_exogenous_det != exogenous_det.end(); it_exogenous_det++)
            if (*it_exogenous_det >= 0)
              {
                output << " " << *it_exogenous_det+1;
                i++;
              }
          output << "];\n";
          output << "block_structure.block(" << block+1 << ").exo_det_nbr = " << i << ";\n";

          output << "block_structure.block(" << block+1 << ").other_endogenous = [";
          i = 0;
          for (set<int>::iterator it_other_endogenous = other_endogenous.begin(); it_other_endogenous != other_endogenous.end(); it_other_endogenous++)
            if (*it_other_endogenous >= 0)
              {
                output << " " << *it_other_endogenous+1;
                i++;
              }
          output << "];\n";
          output << "block_structure.block(" << block+1 << ").other_endogenous_block = [";
          i = 0;
          for (set<int>::iterator it_other_endogenous = other_endogenous.begin(); it_other_endogenous != other_endogenous.end(); it_other_endogenous++)
            if (*it_other_endogenous >= 0)
              {
                bool OK = true;
                unsigned int j;
                for (j = 0; j < block && OK; j++)
                  for (unsigned int k = 0; k < getBlockSize(j) && OK; k++)
                    {
                      //printf("*it_other_endogenous=%d, getBlockVariableID(%d, %d)=%d\n",*it_other_endogenous, j, k, getBlockVariableID(j, k));
                      OK = *it_other_endogenous != getBlockVariableID(j, k);
                    }
                if (!OK)
                  output << " " << j;
                i++;
              }
          output << "];\n";

          //vector<int> inter_state_var;
          output << "block_structure.block(" << block+1 << ").tm1 = zeros(" << i << ", " << state_var.size() << ");\n";
          int count_other_endogenous = 1;
          for (set<int>::const_iterator it_other_endogenous = other_endogenous.begin(); it_other_endogenous != other_endogenous.end(); it_other_endogenous++)
            {
              for (vector<int>::const_iterator it = state_var.begin(); it != state_var.end(); it++)
                {
                  //cout << "block = " << block+1 << " state_var = " << *it << " it_other_endogenous=" << *it_other_endogenous + 1 << "\n";
                  if (*it == *it_other_endogenous + 1)
                    {
                      output << "block_structure.block(" << block+1 << ").tm1("
                             << count_other_endogenous << ", "
                             << it - state_var.begin()+1 << ") = 1;\n";
                      /*output << "block_structure.block(" << block+1 << ").tm1("
                        << it - state_var.begin()+1 << ", "
                        << count_other_endogenous << ") = 1;\n";*/
                      //cout << "=>\n";
                    }
                }
              count_other_endogenous++;
            }

          output << "block_structure.block(" << block+1 << ").other_endo_nbr = " << i << ";\n";

          tmp_s.str("");
          count_lead_lag_incidence = 0;
          dynamic_jacob_map_t reordered_dynamic_jacobian;
          for (block_derivatives_equation_variable_laglead_nodeid_t::const_iterator it = blocks_derivatives[block].begin(); it != blocks_derivatives[block].end(); it++)
            reordered_dynamic_jacobian[make_pair(it->second.first, make_pair(it->first.second, it->first.first))] = it->second.second;
          output << "block_structure.block(" << block+1 << ").lead_lag_incidence = [];\n";
          int last_var = -1;
          vector<int> local_state_var;
          vector<int> local_stat_var;
          int n_static = 0, n_backward = 0, n_forward = 0, n_mixed = 0;
          for (int lag = -1; lag < 1+1; lag++)
            {
              last_var = -1;
              for (dynamic_jacob_map_t::const_iterator it = reordered_dynamic_jacobian.begin(); it != reordered_dynamic_jacobian.end(); it++)
                {
                  if (lag == it->first.first && last_var != it->first.second.first)
                    {
                      if (lag == -1)
                        {
                          local_state_var.push_back(getBlockVariableID(block, it->first.second.first)+1);
                          n_backward++;
                        }
                      else if (lag == 0)
                        {
                          if (find(local_state_var.begin(), local_state_var.end(), getBlockVariableID(block, it->first.second.first)+1) == local_state_var.end())
                            {
                              local_stat_var.push_back(getBlockVariableID(block, it->first.second.first)+1);
                              n_static++;
                            }
                        }
                      else
                        {
                          if (find(local_state_var.begin(), local_state_var.end(), getBlockVariableID(block, it->first.second.first)+1) != local_state_var.end())
                            {
                              n_backward--;
                              n_mixed++;
                            }
                          else
                            {
                              if (find(local_stat_var.begin(), local_stat_var.end(), getBlockVariableID(block, it->first.second.first)+1) != local_stat_var.end())
                                n_static--;
                              n_forward++;
                            }
                        }
                      count_lead_lag_incidence++;
                      for (int i = last_var; i < it->first.second.first-1; i++)
                        tmp_s << " 0";
                      if (tmp_s.str().length())
                        tmp_s << " ";
                      tmp_s << count_lead_lag_incidence;
                      last_var = it->first.second.first;
                    }
                }
              for (int i = last_var + 1; i < block_size; i++)
                tmp_s << " 0";
              output << "block_structure.block(" << block+1 << ").lead_lag_incidence = [ block_structure.block(" << block+1 << ").lead_lag_incidence; " << tmp_s.str() << "]; %lag = " << lag << "\n";
              tmp_s.str("");
            }
          vector<int> inter_state_var;
          for (vector<int>::const_iterator it_l = local_state_var.begin(); it_l != local_state_var.end(); it_l++)
            for (vector<int>::const_iterator it = state_var.begin(); it != state_var.end(); it++)
              if (*it == *it_l)
                inter_state_var.push_back(it - state_var.begin()+1);
          output << "block_structure.block(" << block+1 << ").sorted_col_dr_ghx = [";
          for (vector<int>::const_iterator it = inter_state_var.begin(); it != inter_state_var.end(); it++)
            output << *it << " ";
          output << "];\n";
          count_lead_lag_incidence = 0;
          output << "block_structure.block(" << block+1 << ").lead_lag_incidence_other = [];\n";
          for (int lag = -1; lag <= 1; lag++)
            {
              tmp_s.str("");
              for (set<int>::iterator it_other_endogenous = other_endogenous.begin(); it_other_endogenous != other_endogenous.end(); it_other_endogenous++)
                {
                  bool done = false;
                  for (int i = 0; i < block_size; i++)
                    {
                      unsigned int eq = getBlockEquationID(block, i);
                      derivative_t::const_iterator it = derivative_other_endo[block].find(make_pair(lag, make_pair(eq, *it_other_endogenous)));
                      if (it != derivative_other_endo[block].end())
                        {
                          count_lead_lag_incidence++;
                          tmp_s << " " << count_lead_lag_incidence;
                          done = true;
                          break;
                        }
                    }
                  if (!done)
                    tmp_s << " 0";
                }
              output << "block_structure.block(" << block+1 << ").lead_lag_incidence_other = [ block_structure.block(" << block+1 << ").lead_lag_incidence_other; " << tmp_s.str() << "]; %lag = " << lag << "\n";
            }
          output << "block_structure.block(" << block+1 << ").n_static = " << n_static << ";\n";
          output << "block_structure.block(" << block+1 << ").n_forward = " << n_forward << ";\n";
          output << "block_structure.block(" << block+1 << ").n_backward = " << n_backward << ";\n";
          output << "block_structure.block(" << block+1 << ").n_mixed = " << n_mixed << ";\n";
        }
      output << modstruct << "block_structure.block = block_structure.block;\n";
      string cst_s;
      int nb_endo = symbol_table.endo_nbr();
      output << modstruct << "block_structure.variable_reordered = [";
      for (int i = 0; i < nb_endo; i++)
        output << " " << variable_reordered[i]+1;
      output << "];\n";
      output << modstruct << "block_structure.equation_reordered = [";
      for (int i = 0; i < nb_endo; i++)
        output << " " << equation_reordered[i]+1;
      output << "];\n";
      vector<int> variable_inv_reordered(nb_endo);

      for (int i = 0; i < nb_endo; i++)
        variable_inv_reordered[variable_reordered[i]] = i;

      for (vector<int>::const_iterator it = state_var.begin(); it != state_var.end(); it++)
        state_equ.push_back(equation_reordered[variable_inv_reordered[*it - 1]]+1);

      map<pair< int, pair<int, int> >,  int>  lag_row_incidence;
      for (first_derivatives_t::const_iterator it = first_derivatives.begin();
           it != first_derivatives.end(); it++)
        {
          int deriv_id = it->first.second;
          if (getTypeByDerivID(deriv_id) == eEndogenous)
            {
              int eq = it->first.first;
              int symb = getSymbIDByDerivID(deriv_id);
              int var = symbol_table.getTypeSpecificID(symb);
              int lag = getLagByDerivID(deriv_id);
              lag_row_incidence[make_pair(lag, make_pair(eq, var))] = 1;
            }
        }
      int prev_lag = -1000000;
      for (map<pair< int, pair<int, int> >,  int>::const_iterator it = lag_row_incidence.begin(); it != lag_row_incidence.end(); it++)
        {
          if (prev_lag != it->first.first)
            {
              if (prev_lag != -1000000)
                output << "];\n";
              prev_lag = it->first.first;
              output << modstruct << "block_structure.incidence(" << max_endo_lag+it->first.first+1 << ").lead_lag = " << prev_lag << ";\n";
              output << modstruct << "block_structure.incidence(" << max_endo_lag+it->first.first+1 << ").sparse_IM = [";
            }
          output << it->first.second.first+1 << " " << it->first.second.second+1 << ";\n";
        }
      output << "];\n";
      if (estimation_present)
        {
          ofstream KF_index_file;
          string main_name = basename;
          main_name += ".kfi";
          KF_index_file.open(main_name.c_str(), ios::out | ios::binary | ios::ate);
          int n_obs = symbol_table.observedVariablesNbr();
          int n_state = state_var.size();
          for (vector<int>::const_iterator it = state_var.begin(); it != state_var.end(); it++)
            if (symbol_table.isObservedVariable(symbol_table.getID(eEndogenous, *it-1)))
              n_obs--;

          int n = n_obs + n_state;
          output << modstruct << "nobs_non_statevar = " << n_obs << ";" << endl;
          int nb_diag = 0;
          //map<pair<int,int>, int>::const_iterator  row_state_var_incidence_it = row_state_var_incidence.begin();

          vector<int> i_nz_state_var(n);
          for (int i = 0; i < n_obs; i++)
            i_nz_state_var[i] = n;
          unsigned int lp = n_obs;

          for (unsigned int block = 0; block < nb_blocks; block++)
            {
              int block_size = getBlockSize(block);
              int nze = 0;

              for (int i = 0; i < block_size; i++)
                {
                  int var = getBlockVariableID(block, i);
                  vector<int>::const_iterator it_state_var = find(state_var.begin(), state_var.end(), var+1);
                  if (it_state_var != state_var.end())
                    nze++;
                }
              if (block == 0)
                {
                  set<pair<int, int> > row_state_var_incidence;
                  for (block_derivatives_equation_variable_laglead_nodeid_t::const_iterator it = blocks_derivatives[block].begin(); it != (blocks_derivatives[block]).end(); it++)
                    {
                      vector<int>::const_iterator it_state_var = find(state_var.begin(), state_var.end(), getBlockVariableID(block, it->first.second)+1);
                      if (it_state_var != state_var.end())
                        {
                          vector<int>::const_iterator it_state_equ = find(state_equ.begin(), state_equ.end(), getBlockEquationID(block, it->first.first)+1);
                          if (it_state_equ != state_equ.end())
                            row_state_var_incidence.insert(make_pair(it_state_equ - state_equ.begin(), it_state_var - state_var.begin()));
                        }

                    }
                  /*tmp_block_endo_derivative[make_pair(it->second.first, make_pair(it->first.second, it->first.first))] = it->second.second;
                    if (block == 0)
                    {

                    vector<int>::const_iterator it_state_equ = find(state_equ.begin(), state_equ.end(), getBlockEquationID(block, i)+1);
                    if (it_state_equ != state_equ.end())
                    {
                    cout << "row_state_var_incidence[make_pair([" << *it_state_equ << "] " << it_state_equ - state_equ.begin() << ", [" << *it_state_var << "] " << it_state_var - state_var.begin() << ")] =  1;\n";
                    row_state_var_incidence.insert(make_pair(it_state_equ - state_equ.begin(), it_state_var - state_var.begin()));
                    }
                    }*/
                  set<pair<int, int> >::const_iterator  row_state_var_incidence_it = row_state_var_incidence.begin();
                  bool diag = true;
                  int nb_diag_r = 0;
                  while (row_state_var_incidence_it != row_state_var_incidence.end() && diag)
                    {
                      diag = (row_state_var_incidence_it->first == row_state_var_incidence_it->second);
                      if (diag)
                        {
                          int equ = row_state_var_incidence_it->first;
                          row_state_var_incidence_it++;
                          if (equ != row_state_var_incidence_it->first)
                            nb_diag_r++;
                        }

                    }
                  set<pair<int, int> >  col_state_var_incidence;
                  for (set<pair<int, int> >::const_iterator row_state_var_incidence_it = row_state_var_incidence.begin(); row_state_var_incidence_it != row_state_var_incidence.end(); row_state_var_incidence_it++)
                    col_state_var_incidence.insert(make_pair(row_state_var_incidence_it->second, row_state_var_incidence_it->first));
                  set<pair<int, int> >::const_iterator  col_state_var_incidence_it = col_state_var_incidence.begin();
                  diag = true;
                  int nb_diag_c = 0;
                  while (col_state_var_incidence_it != col_state_var_incidence.end() && diag)
                    {
                      diag = (col_state_var_incidence_it->first == col_state_var_incidence_it->second);
                      if (diag)
                        {
                          int var = col_state_var_incidence_it->first;
                          col_state_var_incidence_it++;
                          if (var != col_state_var_incidence_it->first)
                            nb_diag_c++;
                        }
                    }
                  nb_diag = min(nb_diag_r, nb_diag_c);
                  row_state_var_incidence.clear();
                  col_state_var_incidence.clear();
                }
              for (int i = 0; i < nze; i++)
                i_nz_state_var[lp + i] = lp + nze;
              lp += nze;
            }
          output << modstruct << "nz_state_var = [";
          for (unsigned int i = 0; i < lp; i++)
            output << i_nz_state_var[i] << " ";
          output << "];" << endl;
          output << modstruct << "n_diag = " << nb_diag << ";" << endl;
          KF_index_file.write(reinterpret_cast<char *>(&nb_diag), sizeof(nb_diag));

          typedef pair<int, pair<int, int > > index_KF;
          vector<index_KF> v_index_KF;
          for (int i = 0; i < n; i++)
            //int i = 0;
            for (int j = n_obs; j < n; j++)
              {
                int j1 = j - n_obs;
                int j1_n_state = j1 * n_state - n_obs;
                if ((i < n_obs) || (i >= nb_diag + n_obs) || (j1 >= nb_diag))
                  for (int k = n_obs; k < i_nz_state_var[i]; k++)
                    {
                      v_index_KF.push_back(make_pair(i + j1 * n, make_pair(i + k * n, k + j1_n_state)));
                    }
              }
          int size_v_index_KF = v_index_KF.size();

          KF_index_file.write(reinterpret_cast<char *>(&size_v_index_KF), sizeof(size_v_index_KF));
          for (vector<index_KF>::iterator it = v_index_KF.begin(); it != v_index_KF.end(); it++)
            KF_index_file.write(reinterpret_cast<char *>(&(*it)), sizeof(index_KF));

          vector<index_KF> v_index_KF_2;
          int n_n_obs = n * n_obs;
          for (int i = 0; i < n; i++)
            //i = 0;
            for (int j = i; j < n; j++)
              {
                if ((i < n_obs) || (i >= nb_diag + n_obs) || (j < n_obs) || (j >= nb_diag + n_obs))
                  for (int k = n_obs; k < i_nz_state_var[j]; k++)
                    {
                      int k_n = k * n;
                      v_index_KF_2.push_back(make_pair(i * n + j,  make_pair(i + k_n - n_n_obs, j + k_n)));
                    }
              }
          int size_v_index_KF_2 = v_index_KF_2.size();

          KF_index_file.write(reinterpret_cast<char *>(&size_v_index_KF_2), sizeof(size_v_index_KF_2));
          for (vector<index_KF>::iterator it = v_index_KF_2.begin(); it != v_index_KF_2.end(); it++)
            KF_index_file.write(reinterpret_cast<char *>(&(*it)), sizeof(index_KF));
          KF_index_file.close();
        }
    }

  output << modstruct << "state_var = [";
  for (vector<int>::const_iterator it=state_var.begin(); it != state_var.end(); it++)
    output << *it << (julia ? "," : " ");
  output << "];" << endl;

  // Writing initialization for some other variables
  if (!julia)
    output << modstruct << "exo_names_orig_ord = [1:" << symbol_table.exo_nbr() << "];" << endl;
  else
    output << modstruct << "exo_names_orig_ord = collect(1:" << symbol_table.exo_nbr() << ");" << endl;

  output << modstruct << "maximum_lag = " << max_lag << ";" << endl
         << modstruct << "maximum_lead = " << max_lead << ";" << endl;

  output << modstruct << "maximum_endo_lag = " << max_endo_lag << ";" << endl
         << modstruct << "maximum_endo_lead = " << max_endo_lead << ";" << endl
         << outstruct << "steady_state = zeros(" << symbol_table.endo_nbr() << (julia ? ")" : ", 1);") << endl;

  output << modstruct << "maximum_exo_lag = " << max_exo_lag << ";" << endl
         << modstruct << "maximum_exo_lead = " << max_exo_lead << ";" << endl
         << outstruct << "exo_steady_state = zeros(" << symbol_table.exo_nbr() <<  (julia ? ")" : ", 1);")   << endl;

  if (symbol_table.exo_det_nbr())
    {
      output << modstruct << "maximum_exo_det_lag = " << max_exo_det_lag << ";" << endl
             << modstruct << "maximum_exo_det_lead = " << max_exo_det_lead << ";" << endl
             << outstruct << "exo_det_steady_state = zeros(" << symbol_table.exo_det_nbr() << (julia ? ")" : ", 1);") << endl;
    }

  output << modstruct << "params = " << (julia ? "fill(NaN, " : "NaN(")
         << symbol_table.param_nbr() << (julia ? ")" : ", 1);") << endl;

  if (compute_xrefs)
    writeXrefs(output);

  // Write number of non-zero derivatives
  // Use -1 if the derivatives have not been computed
  output << modstruct << (julia ? "nnzderivatives" : "NNZDerivatives")
         << " = [" << NNZDerivatives[0] << "; ";
  if (order > 1)
    output << NNZDerivatives[1] << "; ";
  else
    output << "-1; ";

  if (order > 2)
    output << NNZDerivatives[2];
  else
    output << "-1";
  output << "];" << endl;

  // Write PacExpectationInfo
  deriv_node_temp_terms_t tef_terms;
  temporary_terms_t temp_terms_empty;
  for (set<const PacExpectationNode *>::const_iterator it = pac_expectation_info.begin();
       it != pac_expectation_info.end(); it++)
    (*it)->writeOutput(output, oMatlabDynamicModel, temp_terms_empty, tef_terms);
}

map<pair<int, pair<int, int > >, expr_t>
DynamicModel::collect_first_order_derivatives_endogenous()
{
  map<pair<int, pair<int, int > >, expr_t> endo_derivatives;
  for (first_derivatives_t::iterator it2 = first_derivatives.begin();
       it2 != first_derivatives.end(); it2++)
    {
      if (getTypeByDerivID(it2->first.second) == eEndogenous)
        {
          int eq = it2->first.first;
          int var = symbol_table.getTypeSpecificID(getSymbIDByDerivID(it2->first.second));
          int lag = getLagByDerivID(it2->first.second);
          endo_derivatives[make_pair(eq, make_pair(var, lag))] = it2->second;
        }
    }
  return endo_derivatives;
}

void
DynamicModel::runTrendTest(const eval_context_t &eval_context)
{
  computeDerivIDs();
  testTrendDerivativesEqualToZero(eval_context);
}

void
DynamicModel::getVarModelVariablesFromEqTags(vector<string> &var_model_eqtags,
                                             vector<int> &eqnumber,
                                             vector<int> &lhs,
                                             vector<expr_t> &lhs_expr_t,
                                             vector<set<pair<int, int> > > &rhs,
                                             vector<bool> &nonstationary) const
{
  for (vector<string>::const_iterator itvareqs = var_model_eqtags.begin();
       itvareqs != var_model_eqtags.end(); itvareqs++)
    {
      int eqn = -1;
      set<pair<int, int> > lhs_set, lhs_tmp_set, rhs_set;
      string eqtag (*itvareqs);
      for (vector<pair<int, pair<string, string> > >::const_iterator iteqtag =
             equation_tags.begin(); iteqtag != equation_tags.end(); iteqtag++)
        if (iteqtag->second.first == "name"
            && iteqtag->second.second == eqtag)
          {
            eqn = iteqtag->first;
            break;
          }

      if (eqn == -1)
        {
          cerr << "ERROR: equation tag '" << eqtag << "' not found" << endl;
          exit(EXIT_FAILURE);
        }

      bool nonstationary_bool = false;
      for (vector<pair<int, pair<string, string> > >::const_iterator iteqtag =
             equation_tags.begin(); iteqtag != equation_tags.end(); iteqtag++)
        if (iteqtag->first == eqn)
          if (iteqtag->second.first == "data_type"
              && iteqtag->second.second == "nonstationary")
            {
              nonstationary_bool = true;
              break;
            }
      nonstationary.push_back(nonstationary_bool);

      equations[eqn]->get_arg1()->collectDynamicVariables(eEndogenous, lhs_set);
      equations[eqn]->get_arg1()->collectDynamicVariables(eExogenous, lhs_tmp_set);
      equations[eqn]->get_arg1()->collectDynamicVariables(eParameter, lhs_tmp_set);

      if (lhs_set.size() != 1 || !lhs_tmp_set.empty())
        {
          cerr << "ERROR: in Equation " << eqtag
               << ". A VAR may only have one endogenous variable on the LHS. " << endl;
          exit(EXIT_FAILURE);
        }

      set<pair<int, int> >::const_iterator it = lhs_set.begin();
      if (it->second != 0)
        {
          cerr << "ERROR: in Equation " << eqtag
               << ". The variable on the LHS of a VAR may not appear with a lead or a lag. "
               << endl;
          exit(EXIT_FAILURE);
        }

      eqnumber.push_back(eqn);
      lhs.push_back(it->first);
      lhs_set.clear();
      set<expr_t> lhs_expr_t_set;
      equations[eqn]->get_arg1()->collectVARLHSVariable(lhs_expr_t_set);
      lhs_expr_t.push_back(*(lhs_expr_t_set.begin()));

      equations[eqn]->get_arg2()->collectDynamicVariables(eEndogenous, rhs_set);
      for (it = rhs_set.begin(); it != rhs_set.end(); it++)
        if (it->second > 0)
          {
            cerr << "ERROR: in Equation " << eqtag
                 << ". A VAR may not have leaded or contemporaneous variables on the RHS. " << endl;
            exit(EXIT_FAILURE);
          }
      rhs.push_back(rhs_set);
    }
}

void
DynamicModel::checkVarMinLag(vector<int> &eqnumber) const
{
  int eqn = 1;
  for (vector<int>::const_iterator it = eqnumber.begin();
       it != eqnumber.end(); it++, eqn++)
    {
      int min_lag = -1;
      min_lag = equations[*it]->get_arg2()->VarMinLag();
      if (min_lag <= 0)
        {
          cerr << "ERROR in VAR Equation #" << eqn << ". "
               << "Leaded exogenous variables and leaded or contemporaneous endogenous variables not allowed in VAR";
          exit(EXIT_FAILURE);
        }
    }
}

int
DynamicModel::getVarMaxLag(StaticModel &static_model, vector<int> &eqnumber) const
{
  vector<expr_t> lhs;
  for (vector<int>::const_iterator it = eqnumber.begin();
       it != eqnumber.end(); it++)
    {
      set<expr_t> lhs_set;
      equations[*it]->get_arg1()->collectVARLHSVariable(lhs_set);
      if (lhs_set.size() != 1)
        {
          cerr << "ERROR: in Equation "
               << ". A VAR may only have one endogenous variable on the LHS. " << endl;
          exit(EXIT_FAILURE);
        }
      lhs.push_back(*(lhs_set.begin()));
    }

  set<expr_t> lhs_static;
  for(vector<expr_t>::const_iterator it = lhs.begin();
      it != lhs.end(); it++)
    lhs_static.insert((*it)->toStatic(static_model));

  int max_lag = 0;
  for (vector<int>::const_iterator it = eqnumber.begin();
       it != eqnumber.end(); it++)
    equations[*it]->get_arg2()->VarMaxLag(static_model, lhs_static, max_lag);

  return max_lag;
}

void
DynamicModel::getVarLhsDiffAndInfo(vector<int> &eqnumber, vector<bool> &diff,
                                   vector<int> &orig_diff_var) const
{
  for (vector<int>::const_iterator it = eqnumber.begin();
       it != eqnumber.end(); it++)
    {
      diff.push_back(equations[*it]->get_arg1()->isDiffPresent());
      if (diff.back())
        {
          set<pair<int, int> > diff_set;
          equations[*it]->get_arg1()->collectDynamicVariables(eEndogenous, diff_set);

          if (diff_set.size() != 1)
            {
              cerr << "ERROR: problem getting variable for LHS diff operator in equation " << *it << endl;
              exit(EXIT_FAILURE);
            }
          orig_diff_var.push_back(diff_set.begin()->first);
        }
      else
        orig_diff_var.push_back(-1);
    }
}

void
DynamicModel::setVarExpectationIndices(map<string, pair<SymbolList, int> > &var_model_info)
{
  for (size_t i = 0; i < equations.size(); i++)
    equations[i]->setVarExpectationIndex(var_model_info);
}

void
DynamicModel::addEquationsForVar(map<string, pair<SymbolList, int> > &var_model_info)
{
  // List of endogenous variables and the minimum lag value that must exist in the model equations
  map<string, int> var_endos_and_lags, model_endos_and_lags;
  for (map<string, pair<SymbolList, int> >::const_iterator it = var_model_info.begin();
       it != var_model_info.end(); it++)
    for (size_t i = 0; i < equations.size(); i++)
      if (equations[i]->isVarModelReferenced(it->first))
        {
          vector<string> symbol_list = it->second.first.get_symbols();
          int order = it->second.second;
          for (vector<string>::const_iterator it1 = symbol_list.begin();
               it1 != symbol_list.end(); it1++)
            if (order > 2)
              if (var_endos_and_lags.find(*it1) != var_endos_and_lags.end())
                var_endos_and_lags[*it1] = min(var_endos_and_lags[*it1], -1*order);
              else
                var_endos_and_lags[*it1] = -1*order;
          break;
        }

  if (var_endos_and_lags.empty())
    return;

  // Ensure that the minimum lag value exists in the model equations. If not, add an equation for it
  for (size_t i = 0; i < equations.size(); i++)
    equations[i]->getEndosAndMaxLags(model_endos_and_lags);

  int count = 0;
  for (map<string, int>::const_iterator it = var_endos_and_lags.begin();
       it != var_endos_and_lags.end(); it++)
    {
      map<string, int>::const_iterator it1 = model_endos_and_lags.find(it->first);
      if (it1 == model_endos_and_lags.end())
        cerr << "WARNING: Variable used in VAR that is not used in the model: " << it->first << endl;
      else
        if (it->second < it1->second)
          {
            int symb_id = symbol_table.getID(it->first);
            expr_t newvar = AddVariable(symb_id, it->second);
            expr_t auxvar = AddVariable(symbol_table.addVarModelEndoLagAuxiliaryVar(symb_id, it->second, newvar), 0);
            addEquation(AddEqual(newvar, auxvar), -1);
            addAuxEquation(AddEqual(newvar, auxvar));
            count++;
          }
    }

  if (count > 0)
    cout << "Accounting for var_model lags not in model block: added " << count << " auxiliary variables and equations." << endl;
}

void
DynamicModel::getUndiffLHSForPac(vector<int> &lhs, vector<expr_t> &lhs_expr_t, vector<bool> &diff, vector<int> &orig_diff_var,
                                 vector<int> &eqnumber, map<string, int> &undiff, ExprNode::subst_table_t &diff_subst_table)
{
  if (undiff.empty())
    return;

  for (map<string, int>::const_iterator it = undiff.begin();
       it != undiff.end(); it++)
    {
      int eqn = -1;
      string eqtag (it->first);
      for (vector<pair<int, pair<string, string> > >::const_iterator iteqtag =
             equation_tags.begin(); iteqtag != equation_tags.end(); iteqtag++)
        if (iteqtag->second.first == "name"
            && iteqtag->second.second == eqtag)
          {
            eqn = iteqtag->first;
            break;
          }

      if (eqn == -1)
        {
          cerr << "ERROR: undiff equation tag '" << eqtag << "' not found" << endl;
          exit(EXIT_FAILURE);
        }

      int i = 0;
      for (vector<int>::const_iterator it1 = eqnumber.begin();
           it1 != eqnumber.end(); it1++, i++)
        if (*it1 == eqn)
          break;

      if (eqnumber[i] != eqn)
        {
          cerr << "ERROR: equation " << eqn << " not found in VAR" << endl;
          exit(EXIT_FAILURE);
        }

      if (diff.at(i) != true)
        {
          cerr << "ERROR: the variable on the LHS of equation #" << eqn << " (VAR equation #" << i
               << " with equation tag '" << eqtag
               << "') does not have the diff operator applied to it yet you are trying to undiff it." << endl;
          exit(EXIT_FAILURE);
        }

      bool printerr = false;
      ExprNode::subst_table_t::const_iterator it1;
      expr_t node = NULL;
      expr_t aux_var = lhs_expr_t.at(i);
      for (it1 = diff_subst_table.begin(); it1 != diff_subst_table.end(); it1++)
        if (it1->second == aux_var)
          {
            node = const_cast<expr_t>(it1->first);
            break;
          }

      if (node == NULL)
        {
          cerr << "Unexpected error encountered." << endl;
          exit(EXIT_FAILURE);
        }

      for (int j = it->second; j > 0; j--)
        if (printerr)
          {
            cerr << "You are undiffing the LHS of equation #" << eqn << " "
                 << it->second << " times but it has only been diffed " << j << " time(s)" << endl;
            exit(EXIT_FAILURE);
          }
        else
          {
            node = node->undiff();
            it1 = diff_subst_table.find(node);
            if (it1 == diff_subst_table.end())
              printerr = true;
          }

      if (printerr)
        { // we have undiffed something like diff(x), hence x is not in diff_subst_table
          lhs_expr_t.at(i) = node;
          lhs.at(i) = dynamic_cast<VariableNode *>(node)->get_symb_id();
        }
      else
        {
          lhs_expr_t.at(i) = const_cast<expr_t>(it1->first);
          lhs.at(i) = const_cast<VariableNode *>(it1->second)->get_symb_id();
        }
    }
}

int
DynamicModel::getUndiffMaxLag(StaticModel &static_model, vector<expr_t> &lhs, vector<int> &eqnumber) const
{
  set<expr_t> lhs_static;
  for(vector<expr_t>::const_iterator it = lhs.begin();
      it != lhs.end(); it++)
    lhs_static.insert((*it)->toStatic(static_model));

  int max_lag = 0;
  for (vector<int>::const_iterator it = eqnumber.begin();
       it != eqnumber.end(); it++)
    equations[*it]->get_arg2()->VarMaxLag(static_model, lhs_static, max_lag);

  return max_lag;
}

void
DynamicModel::walkPacParameters()
{
  for (size_t i = 0; i < equations.size(); i++)
    {
      bool pac_encountered = false;
      pair<int, int> lhs (-1, -1);
      set<pair<int, pair<int, int> > > params_and_vars;
      set<pair<int, pair<int, int> > > ecm_params_and_vars;
      equations[i]->walkPacParameters(pac_encountered, lhs, ecm_params_and_vars, params_and_vars);
      if (pac_encountered)
        equations[i]->addParamInfoToPac(lhs, ecm_params_and_vars, params_and_vars);
    }
}

void
DynamicModel::fillPacExpectationVarInfo(string &var_model_name,
                                        vector<int> &lhs,
                                        int max_lag,
                                        vector<bool> &nonstationary,
                                        int growth_symb_id)
{
  for (size_t i = 0; i < equations.size(); i++)
    equations[i]->fillPacExpectationVarInfo(var_model_name, lhs, max_lag, nonstationary, growth_symb_id, i);
}

void
DynamicModel::substitutePacExpectation()
{
  map<const PacExpectationNode *, const BinaryOpNode *> subst_table;
  for (map<int, expr_t>::iterator it = local_variables_table.begin();
       it != local_variables_table.end(); it++)
    it->second = it->second->substitutePacExpectation(subst_table);

  for (size_t i = 0; i < equations.size(); i++)
    {
      BinaryOpNode *substeq = dynamic_cast<BinaryOpNode *>(equations[i]->substitutePacExpectation(subst_table));
      assert(substeq != NULL);
      equations[i] = substeq;
    }

  for (map<const PacExpectationNode *, const BinaryOpNode *>::const_iterator it = subst_table.begin();
       it != subst_table.end(); it++)
    pac_expectation_info.insert(const_cast<PacExpectationNode *>(it->first));
}

void
DynamicModel::computingPass(bool jacobianExo, bool hessian, bool thirdDerivatives, int paramsDerivsOrder,
                            const eval_context_t &eval_context, bool no_tmp_terms, bool block, bool use_dll,
                            bool bytecode, const bool nopreprocessoroutput)
{
  assert(jacobianExo || !(hessian || thirdDerivatives || paramsDerivsOrder));

  initializeVariablesAndEquations();

  // Prepare for derivation
  computeDerivIDs();

  // Computes dynamic jacobian columns, must be done after computeDerivIDs()
  computeDynJacobianCols(jacobianExo);

  // Compute derivatives w.r. to all endogenous, and possibly exogenous and exogenous deterministic
  set<int> vars;
  for (deriv_id_table_t::const_iterator it = deriv_id_table.begin();
       it != deriv_id_table.end(); it++)
    {
      SymbolType type = symbol_table.getType(it->first.first);
      if (type == eEndogenous || (jacobianExo && (type == eExogenous || type == eExogenousDet)))
        vars.insert(it->second);
    }

  // Launch computations
  if (!nopreprocessoroutput)
    cout << "Computing dynamic model derivatives:" << endl
         << " - order 1" << endl;
  computeJacobian(vars);

  if (hessian)
    {
      if (!nopreprocessoroutput)
        cout << " - order 2" << endl;
      computeHessian(vars);
    }

  if (paramsDerivsOrder > 0)
    {
      if (!nopreprocessoroutput)
        cout << " - derivatives of Jacobian/Hessian w.r. to parameters" << endl;
      computeParamsDerivatives(paramsDerivsOrder);

      if (!no_tmp_terms)
        computeParamsDerivativesTemporaryTerms();
    }

  if (thirdDerivatives)
    {
      if (!nopreprocessoroutput)
        cout << " - order 3" << endl;
      computeThirdDerivatives(vars);
    }

  if (block)
    {
      vector<unsigned int> n_static, n_forward, n_backward, n_mixed;
      jacob_map_t contemporaneous_jacobian, static_jacobian;

      // for each block contains pair<Size, Feddback_variable>
      vector<pair<int, int> > blocks;

      evaluateAndReduceJacobian(eval_context, contemporaneous_jacobian, static_jacobian, dynamic_jacobian, cutoff, false);

      computeNonSingularNormalization(contemporaneous_jacobian, cutoff, static_jacobian, dynamic_jacobian);

      computePrologueAndEpilogue(static_jacobian, equation_reordered, variable_reordered);

      map<pair<int, pair<int, int> >, expr_t> first_order_endo_derivatives = collect_first_order_derivatives_endogenous();

      equation_type_and_normalized_equation = equationTypeDetermination(first_order_endo_derivatives, variable_reordered, equation_reordered, mfs);

      if (!nopreprocessoroutput)
        cout << "Finding the optimal block decomposition of the model ...\n";

      lag_lead_vector_t equation_lag_lead, variable_lag_lead;

      computeBlockDecompositionAndFeedbackVariablesForEachBlock(static_jacobian, dynamic_jacobian, equation_reordered, variable_reordered, blocks, equation_type_and_normalized_equation, false, true, mfs, inv_equation_reordered, inv_variable_reordered, equation_lag_lead, variable_lag_lead, n_static, n_forward, n_backward, n_mixed);

      block_type_firstequation_size_mfs = reduceBlocksAndTypeDetermination(dynamic_jacobian, blocks, equation_type_and_normalized_equation, variable_reordered, equation_reordered, n_static, n_forward, n_backward, n_mixed, block_col_type);

      printBlockDecomposition(blocks);

      computeChainRuleJacobian(blocks_derivatives);

      blocks_linear = BlockLinear(blocks_derivatives, variable_reordered);

      collect_block_first_order_derivatives();

      collectBlockVariables();

      global_temporary_terms = true;
      if (!no_tmp_terms)
        computeTemporaryTermsOrdered();
      int k = 0;
      equation_block = vector<int>(equations.size());
      variable_block_lead_lag = vector< pair< int, pair< int, int> > >(equations.size());
      for (unsigned int i = 0; i < getNbBlocks(); i++)
        {
          for (unsigned int j = 0; j < getBlockSize(i); j++)
            {
              equation_block[equation_reordered[k]] = i;
              int l = variable_reordered[k];
              variable_block_lead_lag[l] = make_pair(i, make_pair(variable_lag_lead[l].first, variable_lag_lead[l].second));
              k++;
            }
        }
    }
  else
    if (!no_tmp_terms)
      {
        computeTemporaryTerms(!use_dll);
        if (bytecode)
          computeTemporaryTermsMapping();
      }
}

void
DynamicModel::computeXrefs()
{
  int i = 0;
  for (vector<BinaryOpNode *>::iterator it = equations.begin();
       it != equations.end(); it++)
    {
      ExprNode::EquationInfo ei;
      (*it)->computeXrefs(ei);
      xrefs[i++] = ei;
    }

  i = 0;
  for (map<int, ExprNode::EquationInfo>::const_iterator it = xrefs.begin();
       it != xrefs.end(); it++, i++)
    {
      computeRevXref(xref_param, it->second.param, i);
      computeRevXref(xref_endo, it->second.endo, i);
      computeRevXref(xref_exo, it->second.exo, i);
      computeRevXref(xref_exo_det, it->second.exo_det, i);
    }
}

void
DynamicModel::computeRevXref(map<pair<int, int>, set<int> > &xrefset, const set<pair<int, int> > &eiref, int eqn)
{
  for (set<pair<int, int> >::const_iterator it = eiref.begin();
       it != eiref.end(); it++)
    {
      set<int> eq;
      if (xrefset.find(*it) != xrefset.end())
        eq = xrefset[*it];
      eq.insert(eqn);
      xrefset[*it] = eq;
    }
}

void
DynamicModel::writeXrefs(ostream &output) const
{
  output << "M_.xref1.param = cell(1, M_.eq_nbr);" << endl
         << "M_.xref1.endo = cell(1, M_.eq_nbr);" << endl
         << "M_.xref1.exo = cell(1, M_.eq_nbr);" << endl
         << "M_.xref1.exo_det = cell(1, M_.eq_nbr);" << endl;
  int i = 1;
  for (map<int, ExprNode::EquationInfo>::const_iterator it = xrefs.begin();
       it != xrefs.end(); it++, i++)
    {
      output << "M_.xref1.param{" << i << "} = [ ";
      for (set<pair<int, int> >::const_iterator it1 = it->second.param.begin();
           it1 != it->second.param.end(); it1++)
        output << symbol_table.getTypeSpecificID(it1->first) + 1 << " ";
      output << "];" << endl;

      output << "M_.xref1.endo{" << i << "} = [ ";
      for (set<pair<int, int> >::const_iterator it1 = it->second.endo.begin();
           it1 != it->second.endo.end(); it1++)
        output << "struct('id', " << symbol_table.getTypeSpecificID(it1->first) + 1 << ", 'shift', " << it1->second << ");";
      output << "];" << endl;

      output << "M_.xref1.exo{" << i << "} = [ ";
      for (set<pair<int, int> >::const_iterator it1 = it->second.exo.begin();
           it1 != it->second.exo.end(); it1++)
        output << "struct('id', " << symbol_table.getTypeSpecificID(it1->first) + 1 << ", 'shift', " << it1->second << ");";
      output << "];" << endl;

      output << "M_.xref1.exo_det{" << i << "} = [ ";
      for (set<pair<int, int> >::const_iterator it1 = it->second.exo_det.begin();
           it1 != it->second.exo_det.end(); it1++)
        output << "struct('id', " << symbol_table.getTypeSpecificID(it1->first) + 1 << ", 'shift', " << it1->second << ");";
      output << "];" << endl;
    }

  output << "M_.xref2.param = cell(1, M_.param_nbr);" << endl
         << "M_.xref2.endo = cell(1, M_.endo_nbr);" << endl
         << "M_.xref2.exo = cell(1, M_.exo_nbr);" << endl
         << "M_.xref2.exo_det = cell(1, M_.exo_det_nbr);" << endl;
  writeRevXrefs(output, xref_param, "param");
  writeRevXrefs(output, xref_endo, "endo");
  writeRevXrefs(output, xref_exo, "exo");
  writeRevXrefs(output, xref_exo_det, "exo_det");
}

void
DynamicModel::writeRevXrefs(ostream &output, const map<pair<int, int>, set<int> > &xrefmap, const string &type) const
{
  int last_tsid = -1;
  for (map<pair<int, int>, set<int> >::const_iterator it = xrefmap.begin();
       it != xrefmap.end(); it++)
    {
      int tsid = symbol_table.getTypeSpecificID(it->first.first) + 1;
      output << "M_.xref2." << type << "{" << tsid << "} = [ ";
      if (last_tsid == tsid)
        output << "M_.xref2." << type << "{" << tsid << "}; ";
      else
        last_tsid = tsid;

      for (set<int>::const_iterator it1 = it->second.begin();
           it1 != it->second.end(); it1++)
        if (type == "param")
          output << *it1 + 1 << " ";
        else
          output << "struct('shift', " << it->first.second << ", 'eq', " << *it1+1 << ");";
      output << "];" << endl;
    }
}

map<pair<pair<int, pair<int, int> >, pair<int, int> >, int>
DynamicModel::get_Derivatives(int block)
{
  int max_lag, max_lead;
  map<pair<pair<int, pair<int, int> >, pair<int, int> >, int> Derivatives;
  Derivatives.clear();
  BlockSimulationType simulation_type = getBlockSimulationType(block);
  if (simulation_type == EVALUATE_BACKWARD || simulation_type == EVALUATE_FORWARD)
    {
      max_lag  = 1;
      max_lead = 1;
      setBlockLeadLag(block, max_lag, max_lead);
    }
  else
    {
      max_lag  = getBlockMaxLag(block);
      max_lead = getBlockMaxLead(block);
    }
  int block_size = getBlockSize(block);
  int block_nb_recursive = block_size - getBlockMfs(block);
  for (int lag = -max_lag; lag <= max_lead; lag++)
    {
      for (int eq = 0; eq < block_size; eq++)
        {
          int eqr = getBlockEquationID(block, eq);
          for (int var = 0; var < block_size; var++)
            {
              int varr = getBlockVariableID(block, var);
              if (dynamic_jacobian.find(make_pair(lag, make_pair(eqr, varr))) != dynamic_jacobian.end())
                {
                  bool OK = true;
                  map<pair<pair<int, pair<int, int> >, pair<int, int> >, int>::const_iterator its = Derivatives.find(make_pair(make_pair(lag, make_pair(eq, var)), make_pair(eqr, varr)));
                  if (its != Derivatives.end())
                    {
                      if (its->second == 2)
                        OK = false;
                    }

                  if (OK)
                    {
                      if (getBlockEquationType(block, eq) == E_EVALUATE_S && eq < block_nb_recursive)
                        //It's a normalized equation, we have to recompute the derivative using chain rule derivative function
                        Derivatives[make_pair(make_pair(lag, make_pair(eq, var)), make_pair(eqr, varr))] = 1;
                      else
                        //It's a feedback equation we can use the derivatives
                        Derivatives[make_pair(make_pair(lag, make_pair(eq, var)), make_pair(eqr, varr))] = 0;
                    }
                  if (var < block_nb_recursive)
                    {
                      int eqs = getBlockEquationID(block, var);
                      for (int vars = block_nb_recursive; vars < block_size; vars++)
                        {
                          int varrs = getBlockVariableID(block, vars);
                          //A new derivative needs to be computed using the chain rule derivative function (a feedback variable appears in a recursive equation)
                          if (Derivatives.find(make_pair(make_pair(lag, make_pair(var, vars)), make_pair(eqs, varrs))) != Derivatives.end())
                            Derivatives[make_pair(make_pair(lag, make_pair(eq, vars)), make_pair(eqr, varrs))] = 2;
                        }
                    }
                }
            }
        }
    }
  return (Derivatives);
}

void
DynamicModel::computeChainRuleJacobian(blocks_derivatives_t &blocks_endo_derivatives)
{
  map<int, expr_t> recursive_variables;
  unsigned int nb_blocks = getNbBlocks();
  blocks_endo_derivatives = blocks_derivatives_t(nb_blocks);
  for (unsigned int block = 0; block < nb_blocks; block++)
    {
      block_derivatives_equation_variable_laglead_nodeid_t tmp_derivatives;
      recursive_variables.clear();
      int block_size = getBlockSize(block);
      int block_nb_mfs = getBlockMfs(block);
      int block_nb_recursives = block_size - block_nb_mfs;
      blocks_endo_derivatives.push_back(block_derivatives_equation_variable_laglead_nodeid_t(0));
      for (int i = 0; i < block_nb_recursives; i++)
        {
          if (getBlockEquationType(block, i) == E_EVALUATE_S)
            recursive_variables[getDerivID(symbol_table.getID(eEndogenous, getBlockVariableID(block, i)), 0)] = getBlockEquationRenormalizedExpr(block, i);
          else
            recursive_variables[getDerivID(symbol_table.getID(eEndogenous, getBlockVariableID(block, i)), 0)] = getBlockEquationExpr(block, i);
        }
      map<pair<pair<int, pair<int, int> >, pair<int, int> >, int> Derivatives = get_Derivatives(block);
      map<pair<pair<int, pair<int, int> >, pair<int, int> >, int>::const_iterator it = Derivatives.begin();
      for (int i = 0; i < (int) Derivatives.size(); i++)
        {
          int Deriv_type = it->second;
          pair<pair<int, pair<int, int> >, pair<int, int> > it_l(it->first);
          it++;
          int lag = it_l.first.first;
          int eq = it_l.first.second.first;
          int var = it_l.first.second.second;
          int eqr = it_l.second.first;
          int varr = it_l.second.second;
          if (Deriv_type == 0)
            first_chain_rule_derivatives[make_pair(eqr, make_pair(varr, lag))] = first_derivatives[make_pair(eqr, getDerivID(symbol_table.getID(eEndogenous, varr), lag))];
          else if (Deriv_type == 1)
            first_chain_rule_derivatives[make_pair(eqr, make_pair(varr, lag))] = (equation_type_and_normalized_equation[eqr].second)->getChainRuleDerivative(getDerivID(symbol_table.getID(eEndogenous, varr), lag), recursive_variables);
          else if (Deriv_type == 2)
            {
              if (getBlockEquationType(block, eq) == E_EVALUATE_S && eq < block_nb_recursives)
                first_chain_rule_derivatives[make_pair(eqr, make_pair(varr, lag))] = (equation_type_and_normalized_equation[eqr].second)->getChainRuleDerivative(getDerivID(symbol_table.getID(eEndogenous, varr), lag), recursive_variables);
              else
                first_chain_rule_derivatives[make_pair(eqr, make_pair(varr, lag))] = equations[eqr]->getChainRuleDerivative(getDerivID(symbol_table.getID(eEndogenous, varr), lag), recursive_variables);
            }
          tmp_derivatives.push_back(make_pair(make_pair(eq, var), make_pair(lag, first_chain_rule_derivatives[make_pair(eqr, make_pair(varr, lag))])));
        }
      blocks_endo_derivatives[block] = tmp_derivatives;
    }
}

void
DynamicModel::collect_block_first_order_derivatives()
{
  //! vector for an equation or a variable indicates the block number
  vector<int> equation_2_block, variable_2_block;
  unsigned int nb_blocks = getNbBlocks();
  equation_2_block = vector<int>(equation_reordered.size());
  variable_2_block = vector<int>(variable_reordered.size());
  for (unsigned int block = 0; block < nb_blocks; block++)
    {
      unsigned int block_size = getBlockSize(block);
      for (unsigned int i = 0; i < block_size; i++)
        {
          equation_2_block[getBlockEquationID(block, i)] = block;
          variable_2_block[getBlockVariableID(block, i)] = block;
        }
    }
  other_endo_block = vector<lag_var_t>(nb_blocks);
  exo_block = vector<lag_var_t>(nb_blocks);
  exo_det_block = vector<lag_var_t>(nb_blocks);
  derivative_endo = vector<derivative_t>(nb_blocks);
  derivative_other_endo = vector<derivative_t>(nb_blocks);
  derivative_exo = vector<derivative_t>(nb_blocks);
  derivative_exo_det = vector<derivative_t>(nb_blocks);
  endo_max_leadlag_block = vector<pair<int, int> >(nb_blocks, make_pair(0, 0));
  other_endo_max_leadlag_block = vector<pair<int, int> >(nb_blocks, make_pair(0, 0));
  exo_max_leadlag_block = vector<pair<int, int> >(nb_blocks, make_pair(0, 0));
  exo_det_max_leadlag_block = vector<pair<int, int> >(nb_blocks, make_pair(0, 0));
  max_leadlag_block = vector<pair<int, int> >(nb_blocks, make_pair(0, 0));
  for (first_derivatives_t::iterator it2 = first_derivatives.begin();
       it2 != first_derivatives.end(); it2++)
    {
      int eq = it2->first.first;
      int var = symbol_table.getTypeSpecificID(getSymbIDByDerivID(it2->first.second));
      int lag = getLagByDerivID(it2->first.second);
      int block_eq = equation_2_block[eq];
      int block_var = 0;
      derivative_t tmp_derivative;
      lag_var_t lag_var;
      switch (getTypeByDerivID(it2->first.second))
        {
        case eEndogenous:
          block_var = variable_2_block[var];
          if (block_eq == block_var)
            {
              if (lag < 0 && lag < -endo_max_leadlag_block[block_eq].first)
                endo_max_leadlag_block[block_eq] = make_pair(-lag, endo_max_leadlag_block[block_eq].second);
              if (lag > 0 && lag > endo_max_leadlag_block[block_eq].second)
                endo_max_leadlag_block[block_eq] = make_pair(endo_max_leadlag_block[block_eq].first, lag);
              tmp_derivative = derivative_endo[block_eq];
              tmp_derivative[make_pair(lag, make_pair(eq, var))] = first_derivatives[make_pair(eq, getDerivID(symbol_table.getID(eEndogenous, var), lag))];
              derivative_endo[block_eq] = tmp_derivative;
            }
          else
            {
              if (lag < 0 && lag < -other_endo_max_leadlag_block[block_eq].first)
                other_endo_max_leadlag_block[block_eq] = make_pair(-lag, other_endo_max_leadlag_block[block_eq].second);
              if (lag > 0 && lag > other_endo_max_leadlag_block[block_eq].second)
                other_endo_max_leadlag_block[block_eq] = make_pair(other_endo_max_leadlag_block[block_eq].first, lag);
              tmp_derivative = derivative_other_endo[block_eq];
              {
                map< int, map<int, int> >::const_iterator it = block_other_endo_index.find(block_eq);
                if (it == block_other_endo_index.end())
                  block_other_endo_index[block_eq][var] = 0;
                else
                  {
                    map<int, int>::const_iterator it1 = it->second.find(var);
                    if (it1 == it->second.end())
                      {
                        int size = block_other_endo_index[block_eq].size();
                        block_other_endo_index[block_eq][var] = size;
                      }
                  }
              }
              tmp_derivative[make_pair(lag, make_pair(eq, var))] = first_derivatives[make_pair(eq, getDerivID(symbol_table.getID(eEndogenous, var), lag))];
              derivative_other_endo[block_eq] = tmp_derivative;
              lag_var = other_endo_block[block_eq];
              if (lag_var.find(lag) == lag_var.end())
                lag_var[lag].clear();
              lag_var[lag].insert(var);
              other_endo_block[block_eq] = lag_var;
            }
          break;
        case eExogenous:
          if (lag < 0 && lag < -exo_max_leadlag_block[block_eq].first)
            exo_max_leadlag_block[block_eq] = make_pair(-lag, exo_max_leadlag_block[block_eq].second);
          if (lag > 0 && lag > exo_max_leadlag_block[block_eq].second)
            exo_max_leadlag_block[block_eq] = make_pair(exo_max_leadlag_block[block_eq].first, lag);
          tmp_derivative = derivative_exo[block_eq];
          {
            map< int, map<int, int> >::const_iterator it = block_exo_index.find(block_eq);
            if (it == block_exo_index.end())
              block_exo_index[block_eq][var] = 0;
            else
              {
                map<int, int>::const_iterator it1 = it->second.find(var);
                if (it1 == it->second.end())
                  {
                    int size = block_exo_index[block_eq].size();
                    block_exo_index[block_eq][var] = size;
                  }
              }
          }
          tmp_derivative[make_pair(lag, make_pair(eq, var))] = first_derivatives[make_pair(eq, getDerivID(symbol_table.getID(eExogenous, var), lag))];
          derivative_exo[block_eq] = tmp_derivative;
          lag_var = exo_block[block_eq];
          if (lag_var.find(lag) == lag_var.end())
            lag_var[lag].clear();
          lag_var[lag].insert(var);
          exo_block[block_eq] = lag_var;
          break;
        case eExogenousDet:
          if (lag < 0 && lag < -exo_det_max_leadlag_block[block_eq].first)
            exo_det_max_leadlag_block[block_eq] = make_pair(-lag, exo_det_max_leadlag_block[block_eq].second);
          if (lag > 0 && lag > exo_det_max_leadlag_block[block_eq].second)
            exo_det_max_leadlag_block[block_eq] = make_pair(exo_det_max_leadlag_block[block_eq].first, lag);
          tmp_derivative = derivative_exo_det[block_eq];
          {
            map< int, map<int, int> >::const_iterator it = block_det_exo_index.find(block_eq);
            if (it == block_det_exo_index.end())
              block_det_exo_index[block_eq][var] = 0;
            else
              {
                map<int, int>::const_iterator it1 = it->second.find(var);
                if (it1 == it->second.end())
                  {
                    int size = block_det_exo_index[block_eq].size();
                    block_det_exo_index[block_eq][var] = size;
                  }
              }
          }
          tmp_derivative[make_pair(lag, make_pair(eq, var))] = first_derivatives[make_pair(eq, getDerivID(symbol_table.getID(eExogenous, var), lag))];
          derivative_exo_det[block_eq] = tmp_derivative;
          lag_var = exo_det_block[block_eq];
          if (lag_var.find(lag) == lag_var.end())
            lag_var[lag].clear();
          lag_var[lag].insert(var);
          exo_det_block[block_eq] = lag_var;
          break;
        default:
          break;
        }
      if (lag < 0 && lag < -max_leadlag_block[block_eq].first)
        max_leadlag_block[block_eq] = make_pair(-lag, max_leadlag_block[block_eq].second);
      if (lag > 0 && lag > max_leadlag_block[block_eq].second)
        max_leadlag_block[block_eq] = make_pair(max_leadlag_block[block_eq].first, lag);
    }

}

void
DynamicModel::collectBlockVariables()
{
  for (unsigned int block = 0; block < getNbBlocks(); block++)
    {
      int prev_var = -1;
      int prev_lag = -999999999;
      int count_col_exo = 0;
      var_t tmp_var_exo;
      for (lag_var_t::const_iterator it = exo_block[block].begin(); it != exo_block[block].end(); it++)
        {
          int lag = it->first;
          for (var_t::const_iterator it2 = it->second.begin(); it2 != it->second.end(); it2++)
            {
              int var = *it2;
              tmp_var_exo.insert(var);
              if (prev_var != var || prev_lag != lag)
                {
                  prev_var = var;
                  prev_lag = lag;
                  count_col_exo++;
                }
            }
        }
      block_var_exo.push_back(make_pair(tmp_var_exo, count_col_exo));
    }
}

void
DynamicModel::writeDynamicFile(const string &basename, bool block, bool bytecode, bool use_dll, int order, bool julia) const
{
  int r;
  string t_basename = basename + "_dynamic";
  if (block && bytecode)
    writeModelEquationsCode_Block(t_basename, basename, map_idx);
  else if (!block && bytecode)
    writeModelEquationsCode(t_basename, basename, map_idx);
  else if (block && !bytecode)
    {
#ifdef _WIN32
      r = mkdir(basename.c_str());
#else
      r = mkdir(basename.c_str(), 0777);
#endif
      if (r < 0 && errno != EEXIST)
        {
          perror("ERROR");
          exit(EXIT_FAILURE);
        }
      writeSparseDynamicMFile(t_basename, basename);
    }
  else if (use_dll)
    writeDynamicCFile(t_basename, order);
  else if (julia)
    writeDynamicJuliaFile(basename);
  else
    {
      writeDynamicMFile(t_basename);
      writeSetAuxiliaryVariables(t_basename, julia);
    }
}

void
DynamicModel::writeSetAuxiliaryVariables(const string &basename, const bool julia) const
{
  ostringstream output_func_body;
  writeAuxVarRecursiveDefinitions(output_func_body, oMatlabDseries);

  if (output_func_body.str().empty())
    return;

  string func_name = basename + "_set_auxiliary_series";
  string filename = julia ? func_name + ".jl" : func_name + ".m";
  string comment = julia ? "#" : "%";

  ofstream output;
  output.open(filename.c_str(), ios::out | ios::binary);
  if (!output.is_open())
    {
      cerr << "ERROR: Can't open file " << filename << " for writing" << endl;
      exit(EXIT_FAILURE);
    }

  output << "function ds = " << func_name + "(ds, params)" << endl
         << comment << endl
         << comment << " Status : Computes Auxiliary variables of the dynamic model and returns a dseries" << endl
         << comment << endl
         << comment << " Warning : this file is generated automatically by Dynare" << endl
         << comment << "           from model file (.mod)" << endl << endl
         << output_func_body.str();

  output.close();
}

void
DynamicModel::writeAuxVarRecursiveDefinitions(ostream &output, ExprNodeOutputType output_type) const
{
  deriv_node_temp_terms_t tef_terms;
  temporary_terms_t temporary_terms;
  for (int i = 0; i < (int) aux_equations.size(); i++)
    if (dynamic_cast<ExprNode *>(aux_equations[i])->containsExternalFunction())
      dynamic_cast<ExprNode *>(aux_equations[i])->writeExternalFunctionOutput(output, output_type,
                                                                              temporary_terms, tef_terms);
  for (int i = 0; i < (int) aux_equations.size(); i++)
    {
      dynamic_cast<ExprNode *>(aux_equations[i])->writeOutput(output, output_type, temporary_terms, tef_terms);
      output << ";" << endl;
    }
}

void
DynamicModel::updateAfterVariableChange(DynamicModel &dm)
{
  variable_node_map.clear();
  unary_op_node_map.clear();
  binary_op_node_map.clear();
  trinary_op_node_map.clear();
  external_function_node_map.clear();
  first_deriv_external_function_node_map.clear();
  second_deriv_external_function_node_map.clear();

  cloneDynamic(dm);
  dm.replaceMyEquations(*this);
}

void
DynamicModel::cloneDynamic(DynamicModel &dynamic_model) const
{
  /* Ensure that we are using the same symbol table, because at many places we manipulate
     symbol IDs rather than strings */
  assert(&symbol_table == &dynamic_model.symbol_table);

  // Convert model local variables (need to be done first)
  for (vector<int>::const_iterator it = local_variables_vector.begin();
       it != local_variables_vector.end(); it++)
    dynamic_model.AddLocalVariable(*it, local_variables_table.find(*it)->second->cloneDynamic(dynamic_model));

  // Convert equations
  for (size_t i = 0; i < equations.size(); i++)
    {
      vector<pair<string, string> > eq_tags;
      for (vector<pair<int, pair<string, string> > >::const_iterator it = equation_tags.begin();
           it != equation_tags.end(); ++it)
        if (it->first == (int)i)
          eq_tags.push_back(it->second);
      dynamic_model.addEquation(equations[i]->cloneDynamic(dynamic_model), equations_lineno[i], eq_tags);
    }

  // Convert auxiliary equations
  for (deque<BinaryOpNode *>::const_iterator it = aux_equations.begin();
       it != aux_equations.end(); it++)
    dynamic_model.addAuxEquation((*it)->cloneDynamic(dynamic_model));

  // Convert static_only equations
  for (size_t i = 0; i < static_only_equations.size(); i++)
    dynamic_model.addStaticOnlyEquation(static_only_equations[i]->cloneDynamic(dynamic_model),
                                        static_only_equations_lineno[i],
                                        static_only_equations_equation_tags[i]);

  dynamic_model.setLeadsLagsOrig();
}

void
DynamicModel::replaceMyEquations(DynamicModel &dynamic_model) const
{
  dynamic_model.equations.clear();
  for (size_t i = 0; i < equations.size(); i++)
    dynamic_model.addEquation(equations[i]->cloneDynamic(dynamic_model),
                              equations_lineno[i]);
}

void
DynamicModel::computeRamseyPolicyFOCs(const StaticModel &static_model, const bool nopreprocessoroutput)
{
  // Add aux LM to constraints in equations
  // equation[i]->lhs = rhs becomes equation[i]->MULT_(i+1)*(lhs-rhs) = 0
  int i;
  for (i = 0; i < (int) equations.size(); i++)
    {
      BinaryOpNode *substeq = dynamic_cast<BinaryOpNode *>(equations[i]->addMultipliersToConstraints(i));
      assert(substeq != NULL);
      equations[i] = substeq;
    }
  if (!nopreprocessoroutput)
    cout << "Ramsey Problem: added " << i << " Multipliers." << endl;

  // Add Planner Objective to equations to include in computeDerivIDs
  assert(static_model.equations.size() == 1);
  addEquation(static_model.equations[0]->cloneDynamic(*this), static_model.equations_lineno[0]);

  // Get max endo lead and max endo lag
  set<pair<int, int> > dynvars;
  int max_eq_lead = 0;
  int max_eq_lag = 0;
  for (int i = 0; i < (int) equations.size(); i++)
    equations[i]->collectDynamicVariables(eEndogenous, dynvars);

  for (set<pair<int, int> >::const_iterator it = dynvars.begin();
       it != dynvars.end(); it++)
    {
      int lag = it->second;
      if (max_eq_lead < lag)
        max_eq_lead = lag;
      else if (-max_eq_lag > lag)
        max_eq_lag = -lag;
    }

  // Get Discount Factor
  assert(symbol_table.exists("optimal_policy_discount_factor"));
  int symb_id = symbol_table.getID("optimal_policy_discount_factor");
  assert(symbol_table.getType(symb_id) == eParameter);
  expr_t discount_factor_node = AddVariable(symb_id, 0);

  // Create (modified) Lagrangian (so that we can take the derivative once at time t)
  expr_t lagrangian = Zero;
  for (i = 0; i < (int) equations.size(); i++)
    for (int lag = -max_eq_lag; lag <= max_eq_lead; lag++)
      {
        expr_t dfpower = NULL;
        std::stringstream lagstream;
        lagstream << abs(lag);
        if (lag < 0)
          dfpower = AddNonNegativeConstant(lagstream.str());
        else if (lag == 0)
          dfpower = Zero;
        else
          dfpower = AddMinus(Zero, AddNonNegativeConstant(lagstream.str()));

        lagrangian = AddPlus(AddTimes(AddPower(discount_factor_node, dfpower),
                                      equations[i]->getNonZeroPartofEquation()->decreaseLeadsLags(lag)), lagrangian);
      }

  equations.clear();
  addEquation(AddEqual(lagrangian, Zero), -1);
  computeDerivIDs();

  //Compute derivatives and overwrite equations
  vector<expr_t> neweqs;
  for (deriv_id_table_t::const_iterator it = deriv_id_table.begin();
       it != deriv_id_table.end(); it++)
    // For all endogenous variables with zero lag
    if (symbol_table.getType(it->first.first)  == eEndogenous && it->first.second == 0)
      neweqs.push_back(AddEqual(equations[0]->getNonZeroPartofEquation()->getDerivative(it->second), Zero));

  // Add new equations
  equations.clear();
  for (int i = 0; i < (int) neweqs.size(); i++)
    addEquation(neweqs[i], -1);
}

void
DynamicModel::toStatic(StaticModel &static_model) const
{
  /* Ensure that we are using the same symbol table, because at many places we manipulate
     symbol IDs rather than strings */
  assert(&symbol_table == &static_model.symbol_table);

  // Convert model local variables (need to be done first)
  for (vector<int>::const_iterator it = local_variables_vector.begin();
       it != local_variables_vector.end(); it++)
    static_model.AddLocalVariable(*it, local_variables_table.find(*it)->second->toStatic(static_model));

  // Convert equations
  int static_only_index = 0;
  for (int i = 0; i < (int) equations.size(); i++)
    {
      // Detect if equation is marked [dynamic]
      bool is_dynamic_only = false;
      vector<pair<string, string> > eq_tags;
      for (vector<pair<int, pair<string, string> > >::const_iterator it = equation_tags.begin();
           it != equation_tags.end(); ++it)
        if (it->first == i)
          {
            eq_tags.push_back(it->second);
            if (it->second.first == "dynamic")
              is_dynamic_only = true;
          }

      try
        {
          // If yes, replace it by an equation marked [static]
          if (is_dynamic_only)
            {
              static_model.addEquation(static_only_equations[static_only_index]->toStatic(static_model), static_only_equations_lineno[static_only_index], static_only_equations_equation_tags[static_only_index]);
              static_only_index++;
            }
          else
            static_model.addEquation(equations[i]->toStatic(static_model), equations_lineno[i], eq_tags);
        }
      catch (DataTree::DivisionByZeroException)
        {
          cerr << "...division by zero error encountred when converting equation " << i << " to static" << endl;
          exit(EXIT_FAILURE);
        }
    }

  // Convert auxiliary equations
  for (deque<BinaryOpNode *>::const_iterator it = aux_equations.begin();
       it != aux_equations.end(); it++)
    static_model.addAuxEquation((*it)->toStatic(static_model));
}

bool
DynamicModel::ParamUsedWithLeadLag() const
{
  return ParamUsedWithLeadLagInternal();
}

set<int>
DynamicModel::findUnusedEndogenous()
{
  set<int> usedEndo, unusedEndo;
  for (int i = 0; i < (int) equations.size(); i++)
    equations[i]->collectVariables(eEndogenous, usedEndo);
  set<int> allEndo = symbol_table.getEndogenous();
  set_difference(allEndo.begin(), allEndo.end(),
                 usedEndo.begin(), usedEndo.end(),
                 inserter(unusedEndo, unusedEndo.begin()));
  return unusedEndo;
}

set<int>
DynamicModel::findUnusedExogenous()
{
  set<int> usedExo, unusedExo, unobservedExo;
  for (int i = 0; i < (int) equations.size(); i++)
    equations[i]->collectVariables(eExogenous, usedExo);
  set<int> observedExo = symbol_table.getObservedExogenous();
  set<int> allExo = symbol_table.getExogenous();
  set_difference(allExo.begin(), allExo.end(),
                 observedExo.begin(), observedExo.end(),
                 inserter(unobservedExo, unobservedExo.begin()));
  set_difference(unobservedExo.begin(), unobservedExo.end(),
                 usedExo.begin(), usedExo.end(),
                 inserter(unusedExo, unusedExo.begin()));
  return unusedExo;
}

void
DynamicModel::setLeadsLagsOrig()
{
  set<pair<int, int> > dynvars;

  for (int i = 0; i < (int) equations.size(); i++)
    {
      equations[i]->collectDynamicVariables(eEndogenous, dynvars);
      equations[i]->collectDynamicVariables(eExogenous, dynvars);
      equations[i]->collectDynamicVariables(eExogenousDet, dynvars);
    }

    for (set<pair<int, int> >::const_iterator it = dynvars.begin();
         it != dynvars.end(); it++)
    {
      int lag = it->second;
      SymbolType type = symbol_table.getType(it->first);

      if (max_lead_orig < lag)
        max_lead_orig= lag;
      else if (-max_lag_orig > lag)
        max_lag_orig = -lag;

      switch (type)
        {
        case eEndogenous:
          if (max_endo_lead_orig < lag)
            max_endo_lead_orig = lag;
          else if (-max_endo_lag_orig > lag)
            max_endo_lag_orig = -lag;
          break;
        case eExogenous:
          if (max_exo_lead_orig < lag)
            max_exo_lead_orig = lag;
          else if (-max_exo_lag_orig > lag)
            max_exo_lag_orig = -lag;
          break;
        case eExogenousDet:
          if (max_exo_det_lead_orig < lag)
            max_exo_det_lead_orig = lag;
          else if (-max_exo_det_lag_orig > lag)
            max_exo_det_lag_orig = -lag;
          break;
        default:
          break;
        }
    }
}

void
DynamicModel::computeDerivIDs()
{
  set<pair<int, int> > dynvars;

  for (int i = 0; i < (int) equations.size(); i++)
    equations[i]->collectDynamicVariables(eEndogenous, dynvars);

  dynJacobianColsNbr = dynvars.size();

  for (int i = 0; i < (int) equations.size(); i++)
    {
      equations[i]->collectDynamicVariables(eExogenous, dynvars);
      equations[i]->collectDynamicVariables(eExogenousDet, dynvars);
      equations[i]->collectDynamicVariables(eParameter, dynvars);
      equations[i]->collectDynamicVariables(eTrend, dynvars);
      equations[i]->collectDynamicVariables(eLogTrend, dynvars);
    }

  for (set<pair<int, int> >::const_iterator it = dynvars.begin();
       it != dynvars.end(); it++)
    {
      int lag = it->second;
      SymbolType type = symbol_table.getType(it->first);

      /* Setting maximum and minimum lags.

         We don't want these to be affected by lead/lags on parameters: they
         are accepted for facilitating variable flipping, but are simply
         ignored. */
      if (max_lead < lag && type != eParameter)
        max_lead = lag;
      else if (-max_lag > lag && type != eParameter)
        max_lag = -lag;

      switch (type)
        {
        case eEndogenous:
          if (max_endo_lead < lag)
            max_endo_lead = lag;
          else if (-max_endo_lag > lag)
            max_endo_lag = -lag;
          break;
        case eExogenous:
          if (max_exo_lead < lag)
            max_exo_lead = lag;
          else if (-max_exo_lag > lag)
            max_exo_lag = -lag;
          break;
        case eExogenousDet:
          if (max_exo_det_lead < lag)
            max_exo_det_lead = lag;
          else if (-max_exo_det_lag > lag)
            max_exo_det_lag = -lag;
          break;
        default:
          break;
        }

      // Create a new deriv_id
      int deriv_id = deriv_id_table.size();

      deriv_id_table[*it] = deriv_id;
      inv_deriv_id_table.push_back(*it);
    }
}

SymbolType
DynamicModel::getTypeByDerivID(int deriv_id) const throw (UnknownDerivIDException)
{
  return symbol_table.getType(getSymbIDByDerivID(deriv_id));
}

int
DynamicModel::getLagByDerivID(int deriv_id) const throw (UnknownDerivIDException)
{
  if (deriv_id < 0 || deriv_id >= (int) inv_deriv_id_table.size())
    throw UnknownDerivIDException();

  return inv_deriv_id_table[deriv_id].second;
}

int
DynamicModel::getSymbIDByDerivID(int deriv_id) const throw (UnknownDerivIDException)
{
  if (deriv_id < 0 || deriv_id >= (int) inv_deriv_id_table.size())
    throw UnknownDerivIDException();

  return inv_deriv_id_table[deriv_id].first;
}

int
DynamicModel::getDerivID(int symb_id, int lag) const throw (UnknownDerivIDException)
{
  deriv_id_table_t::const_iterator it = deriv_id_table.find(make_pair(symb_id, lag));
  if (it == deriv_id_table.end())
    throw UnknownDerivIDException();
  else
    return it->second;
}

void
DynamicModel::addAllParamDerivId(set<int> &deriv_id_set)
{
  for (size_t i = 0; i < inv_deriv_id_table.size(); i++)
    if (symbol_table.getType(inv_deriv_id_table[i].first) == eParameter)
      deriv_id_set.insert(i);
}

void
DynamicModel::computeDynJacobianCols(bool jacobianExo)
{
  /* Sort the dynamic endogenous variables by lexicographic order over (lag, type_specific_symbol_id)
     and fill the dynamic columns for exogenous and exogenous deterministic */
  map<pair<int, int>, int> ordered_dyn_endo;

  for (deriv_id_table_t::const_iterator it = deriv_id_table.begin();
       it != deriv_id_table.end(); it++)
    {
      const int &symb_id = it->first.first;
      const int &lag = it->first.second;
      const int &deriv_id = it->second;
      SymbolType type = symbol_table.getType(symb_id);
      int tsid = symbol_table.getTypeSpecificID(symb_id);

      switch (type)
        {
        case eEndogenous:
          ordered_dyn_endo[make_pair(lag, tsid)] = deriv_id;
          break;
        case eExogenous:
          // At this point, dynJacobianColsNbr contains the number of dynamic endogenous
          if (jacobianExo)
            dyn_jacobian_cols_table[deriv_id] = dynJacobianColsNbr + tsid;
          break;
        case eExogenousDet:
          // At this point, dynJacobianColsNbr contains the number of dynamic endogenous
          if (jacobianExo)
            dyn_jacobian_cols_table[deriv_id] = dynJacobianColsNbr + symbol_table.exo_nbr() + tsid;
          break;
        case eParameter:
        case eTrend:
        case eLogTrend:
          // We don't assign a dynamic jacobian column to parameters or trend variables
          break;
        default:
          // Shut up GCC
          cerr << "DynamicModel::computeDynJacobianCols: impossible case" << endl;
          exit(EXIT_FAILURE);
        }
    }

  // Fill in dynamic jacobian columns for endogenous
  int sorted_id = 0;
  for (map<pair<int, int>, int>::const_iterator it = ordered_dyn_endo.begin();
       it != ordered_dyn_endo.end(); it++)
    dyn_jacobian_cols_table[it->second] = sorted_id++;

  // Set final value for dynJacobianColsNbr
  if (jacobianExo)
    dynJacobianColsNbr += symbol_table.exo_nbr() + symbol_table.exo_det_nbr();
}

int
DynamicModel::getDynJacobianCol(int deriv_id) const throw (UnknownDerivIDException)
{
  map<int, int>::const_iterator it = dyn_jacobian_cols_table.find(deriv_id);
  if (it == dyn_jacobian_cols_table.end())
    throw UnknownDerivIDException();
  else
    return it->second;
}

void
DynamicModel::testTrendDerivativesEqualToZero(const eval_context_t &eval_context)
{
  for (deriv_id_table_t::const_iterator it = deriv_id_table.begin();
       it != deriv_id_table.end(); it++)
    if (symbol_table.getType(it->first.first) == eTrend
        || symbol_table.getType(it->first.first) == eLogTrend)
      for (int eq = 0; eq < (int) equations.size(); eq++)
        {
          expr_t homogeneq = AddMinus(equations[eq]->get_arg1(),
                                      equations[eq]->get_arg2());

          // Do not run the test if the term inside the log is zero
          if (fabs(homogeneq->eval(eval_context)) > ZERO_BAND)
            {
              expr_t testeq = AddLog(homogeneq); // F = log(lhs-rhs)
              testeq = testeq->getDerivative(it->second); // d F / d Trend
              for (deriv_id_table_t::const_iterator endogit = deriv_id_table.begin();
                   endogit != deriv_id_table.end(); endogit++)
                if (symbol_table.getType(endogit->first.first) == eEndogenous)
                  {
                    double nearZero = testeq->getDerivative(endogit->second)->eval(eval_context); // eval d F / d Trend d Endog
                    if (fabs(nearZero) > ZERO_BAND)
                      {
                        cerr << "WARNING: trends not compatible with balanced growth path; the second-order cross partial of equation " << eq + 1 << " (line "
                             << equations_lineno[eq] << ") w.r.t. trend variable "
                             << symbol_table.getName(it->first.first) << " and endogenous variable "
                             << symbol_table.getName(endogit->first.first) << " is not null. " << endl;
                        // Changed to warning. See discussion in #1389
                      }
                  }
            }
        }
}

void
DynamicModel::writeParamsDerivativesFile(const string &basename, bool julia) const
{
  if (!residuals_params_derivatives.size()
      && !residuals_params_second_derivatives.size()
      && !jacobian_params_derivatives.size()
      && !jacobian_params_second_derivatives.size()
      && !hessian_params_derivatives.size())
    return;

  ExprNodeOutputType output_type = (julia ? oJuliaDynamicModel : oMatlabDynamicModel);
  ostringstream model_local_vars_output;   // Used for storing model local vars
  ostringstream model_output;              // Used for storing model temp vars and equations
  ostringstream jacobian_output;           // Used for storing jacobian equations
  ostringstream hessian_output;            // Used for storing Hessian equations
  ostringstream hessian1_output;           // Used for storing Hessian equations
  ostringstream third_derivs_output;       // Used for storing third order derivatives equations
  ostringstream third_derivs1_output;      // Used for storing third order derivatives equations

  deriv_node_temp_terms_t tef_terms;
  writeModelLocalVariables(model_local_vars_output, output_type, tef_terms);

  temporary_terms_t temp_terms_empty;
  writeTemporaryTerms(params_derivs_temporary_terms, temp_terms_empty, model_output, output_type, tef_terms);

  for (first_derivatives_t::const_iterator it = residuals_params_derivatives.begin();
       it != residuals_params_derivatives.end(); it++)
    {
      int eq = it->first.first;
      int param = it->first.second;
      expr_t d1 = it->second;

      int param_col = symbol_table.getTypeSpecificID(getSymbIDByDerivID(param)) + 1;

      jacobian_output << "rp" << LEFT_ARRAY_SUBSCRIPT(output_type) << eq+1 << ", " << param_col
                      << RIGHT_ARRAY_SUBSCRIPT(output_type) << " = ";
      d1->writeOutput(jacobian_output, output_type, params_derivs_temporary_terms, tef_terms);
      jacobian_output << ";" << endl;
    }

  for (second_derivatives_t::const_iterator it = jacobian_params_derivatives.begin();
       it != jacobian_params_derivatives.end(); it++)
    {
      int eq = it->first.first;
      int var = it->first.second.first;
      int param = it->first.second.second;
      expr_t d2 = it->second;

      int var_col = getDynJacobianCol(var) + 1;
      int param_col = symbol_table.getTypeSpecificID(getSymbIDByDerivID(param)) + 1;

      hessian_output << "gp" << LEFT_ARRAY_SUBSCRIPT(output_type) << eq+1 << ", " << var_col
                     << ", " << param_col << RIGHT_ARRAY_SUBSCRIPT(output_type) << " = ";
      d2->writeOutput(hessian_output, output_type, params_derivs_temporary_terms, tef_terms);
      hessian_output << ";" << endl;
    }

  int i = 1;
  for (second_derivatives_t::const_iterator it = residuals_params_second_derivatives.begin();
       it != residuals_params_second_derivatives.end(); ++it, i++)
    {
      int eq = it->first.first;
      int param1 = it->first.second.first;
      int param2 = it->first.second.second;
      expr_t d2 = it->second;

      int param1_col = symbol_table.getTypeSpecificID(getSymbIDByDerivID(param1)) + 1;
      int param2_col = symbol_table.getTypeSpecificID(getSymbIDByDerivID(param2)) + 1;

      hessian1_output << "rpp" << LEFT_ARRAY_SUBSCRIPT(output_type) << i << ",1"
                      << RIGHT_ARRAY_SUBSCRIPT(output_type) << "=" << eq+1 << ";" << endl
                      << "rpp" << LEFT_ARRAY_SUBSCRIPT(output_type) << i << ",2"
                      << RIGHT_ARRAY_SUBSCRIPT(output_type) << "=" << param1_col << ";" << endl
                      << "rpp" << LEFT_ARRAY_SUBSCRIPT(output_type) << i << ",3"
                      << RIGHT_ARRAY_SUBSCRIPT(output_type) << "=" << param2_col << ";" << endl
                      << "rpp" << LEFT_ARRAY_SUBSCRIPT(output_type) << i << ",4"
                      << RIGHT_ARRAY_SUBSCRIPT(output_type) << "=";
      d2->writeOutput(hessian1_output, output_type, params_derivs_temporary_terms, tef_terms);
      hessian1_output << ";" << endl;
    }

  i = 1;
  for (third_derivatives_t::const_iterator it = jacobian_params_second_derivatives.begin();
       it != jacobian_params_second_derivatives.end(); ++it, i++)
    {
      int eq = it->first.first;
      int var = it->first.second.first;
      int param1 = it->first.second.second.first;
      int param2 = it->first.second.second.second;
      expr_t d2 = it->second;

      int var_col = getDynJacobianCol(var) + 1;
      int param1_col = symbol_table.getTypeSpecificID(getSymbIDByDerivID(param1)) + 1;
      int param2_col = symbol_table.getTypeSpecificID(getSymbIDByDerivID(param2)) + 1;

      third_derivs_output << "gpp" << LEFT_ARRAY_SUBSCRIPT(output_type) << i << ",1"
                          << RIGHT_ARRAY_SUBSCRIPT(output_type) << "=" << eq+1 << ";" << endl
                          << "gpp" << LEFT_ARRAY_SUBSCRIPT(output_type) << i << ",2"
                          << RIGHT_ARRAY_SUBSCRIPT(output_type) << "=" << var_col << ";" << endl
                          << "gpp" << LEFT_ARRAY_SUBSCRIPT(output_type) << i << ",3"
                          << RIGHT_ARRAY_SUBSCRIPT(output_type) << "=" << param1_col << ";" << endl
                          << "gpp" << LEFT_ARRAY_SUBSCRIPT(output_type) << i << ",4"
                          << RIGHT_ARRAY_SUBSCRIPT(output_type) << "=" << param2_col << ";" << endl
                          << "gpp" << LEFT_ARRAY_SUBSCRIPT(output_type) << i << ",5"
                          << RIGHT_ARRAY_SUBSCRIPT(output_type) << "=";
      d2->writeOutput(third_derivs_output, output_type, params_derivs_temporary_terms, tef_terms);
      third_derivs_output << ";" << endl;
    }

  i = 1;
  for (third_derivatives_t::const_iterator it = hessian_params_derivatives.begin();
       it != hessian_params_derivatives.end(); ++it, i++)
    {
      int eq = it->first.first;
      int var1 = it->first.second.first;
      int var2 = it->first.second.second.first;
      int param = it->first.second.second.second;
      expr_t d2 = it->second;

      int var1_col = getDynJacobianCol(var1) + 1;
      int var2_col = getDynJacobianCol(var2) + 1;
      int param_col = symbol_table.getTypeSpecificID(getSymbIDByDerivID(param)) + 1;

      third_derivs1_output << "hp" << LEFT_ARRAY_SUBSCRIPT(output_type) << i << ",1"
                           << RIGHT_ARRAY_SUBSCRIPT(output_type) << "=" << eq+1 << ";" << endl
                           << "hp" << LEFT_ARRAY_SUBSCRIPT(output_type) << i << ",2"
                           << RIGHT_ARRAY_SUBSCRIPT(output_type) << "=" << var1_col << ";" << endl
                           << "hp" << LEFT_ARRAY_SUBSCRIPT(output_type) << i << ",3"
                           << RIGHT_ARRAY_SUBSCRIPT(output_type) << "=" << var2_col << ";" << endl
                           << "hp" << LEFT_ARRAY_SUBSCRIPT(output_type) << i << ",4"
                           << RIGHT_ARRAY_SUBSCRIPT(output_type) << "=" << param_col << ";" << endl
                           << "hp" << LEFT_ARRAY_SUBSCRIPT(output_type) << i << ",5"
                           << RIGHT_ARRAY_SUBSCRIPT(output_type) << "=";
      d2->writeOutput(third_derivs1_output, output_type, params_derivs_temporary_terms, tef_terms);
      third_derivs1_output << ";" << endl;
    }

  string filename = julia ? basename + "DynamicParamsDerivs.jl" : basename + "_params_derivs.m";
  ofstream paramsDerivsFile;
  paramsDerivsFile.open(filename.c_str(), ios::out | ios::binary);
  if (!paramsDerivsFile.is_open())
    {
      cerr << "ERROR: Can't open file " << filename << " for writing" << endl;
      exit(EXIT_FAILURE);
    }

  if (!julia)
    {
      // Check that we don't have more than 32 nested parenthesis because Matlab does not suppor this. See Issue #1201
      map<string, string> tmp_paren_vars;
      bool message_printed = false;
      fixNestedParenthesis(model_output, tmp_paren_vars, message_printed);
      fixNestedParenthesis(model_local_vars_output, tmp_paren_vars, message_printed);
      fixNestedParenthesis(jacobian_output, tmp_paren_vars, message_printed);
      fixNestedParenthesis(hessian_output, tmp_paren_vars, message_printed);
      fixNestedParenthesis(hessian1_output, tmp_paren_vars, message_printed);
      fixNestedParenthesis(third_derivs_output, tmp_paren_vars, message_printed);
      fixNestedParenthesis(third_derivs1_output, tmp_paren_vars, message_printed);
      paramsDerivsFile << "function [rp, gp, rpp, gpp, hp] = " << basename << "_params_derivs(y, x, params, steady_state, it_, ss_param_deriv, ss_param_2nd_deriv)" << endl
                       << "%" << endl
                       << "% Compute the derivatives of the dynamic model with respect to the parameters" << endl
                       << "% Inputs :" << endl
                       << "%   y         [#dynamic variables by 1] double    vector of endogenous variables in the order stored" << endl
                       << "%                                                 in M_.lead_lag_incidence; see the Manual" << endl
                       << "%   x         [nperiods by M_.exo_nbr] double     matrix of exogenous variables (in declaration order)" << endl
                       << "%                                                 for all simulation periods" << endl
                       << "%   params    [M_.param_nbr by 1] double          vector of parameter values in declaration order" << endl
                       << "%   steady_state  [M_.endo_nbr by 1] double       vector of steady state values" << endl
                       << "%   it_       scalar double                       time period for exogenous variables for which to evaluate the model" << endl
                       << "%   ss_param_deriv     [M_.eq_nbr by #params]     Jacobian matrix of the steady states values with respect to the parameters" << endl
                       << "%   ss_param_2nd_deriv [M_.eq_nbr by #params by #params] Hessian matrix of the steady states values with respect to the parameters" << endl
                       << "%" << endl
                       << "% Outputs:" << endl
                       << "%   rp        [M_.eq_nbr by #params] double    Jacobian matrix of dynamic model equations with respect to parameters " << endl
                       << "%                                              Dynare may prepend or append auxiliary equations, see M_.aux_vars" << endl
                       << "%   gp        [M_.endo_nbr by #dynamic variables by #params] double    Derivative of the Jacobian matrix of the dynamic model equations with respect to the parameters" << endl
                       << "%                                                           rows: equations in order of declaration" << endl
                       << "%                                                           columns: variables in order stored in M_.lead_lag_incidence" << endl
                       << "%   rpp       [#second_order_residual_terms by 4] double   Hessian matrix of second derivatives of residuals with respect to parameters;" << endl
                       << "%                                                              rows: respective derivative term" << endl
                       << "%                                                              1st column: equation number of the term appearing" << endl
                       << "%                                                              2nd column: number of the first parameter in derivative" << endl
                       << "%                                                              3rd column: number of the second parameter in derivative" << endl
                       << "%                                                              4th column: value of the Hessian term" << endl
                       << "%   gpp      [#second_order_Jacobian_terms by 5] double   Hessian matrix of second derivatives of the Jacobian with respect to the parameters;" << endl
                       << "%                                                              rows: respective derivative term" << endl
                       << "%                                                              1st column: equation number of the term appearing" << endl
                       << "%                                                              2nd column: column number of variable in Jacobian of the dynamic model" << endl
                       << "%                                                              3rd column: number of the first parameter in derivative" << endl
                       << "%                                                              4th column: number of the second parameter in derivative" << endl
                       << "%                                                              5th column: value of the Hessian term" << endl
                       << "%   hp      [#first_order_Hessian_terms by 5] double   Jacobian matrix of derivatives of the dynamic Hessian with respect to the parameters;" << endl
                       << "%                                                              rows: respective derivative term" << endl
                       << "%                                                              1st column: equation number of the term appearing" << endl
                       << "%                                                              2nd column: column number of first variable in Hessian of the dynamic model" << endl
                       << "%                                                              3rd column: column number of second variable in Hessian of the dynamic model" << endl
                       << "%                                                              4th column: number of the parameter in derivative" << endl
                       << "%                                                              5th column: value of the Hessian term" << endl
                       << "%" << endl
                       << "%" << endl
                       << "% Warning : this file is generated automatically by Dynare" << endl
                       << "%           from model file (.mod)" << endl << endl
                       << model_local_vars_output.str()
                       << model_output.str()
                       << "rp = zeros(" << equations.size() << ", "
                       << symbol_table.param_nbr() << ");" << endl
                       << jacobian_output.str()
                       << "gp = zeros(" << equations.size() << ", " << dynJacobianColsNbr << ", " << symbol_table.param_nbr() << ");" << endl
                       << hessian_output.str()
                       << "if nargout >= 3" << endl
                       << "rpp = zeros(" << residuals_params_second_derivatives.size() << ",4);" << endl
                       << hessian1_output.str()
                       << "gpp = zeros(" << jacobian_params_second_derivatives.size() << ",5);" << endl
                       << third_derivs_output.str()
                       << "end" << endl
                       << "if nargout >= 5" << endl
                       << "hp = zeros(" << hessian_params_derivatives.size() << ",5);" << endl
                       << third_derivs1_output.str()
                       << "end" << endl
                       << "end" << endl;
    }
  else
    paramsDerivsFile << "module " << basename << "DynamicParamsDerivs" << endl
                     << "#" << endl
                     << "# NB: this file was automatically generated by Dynare" << endl
                     << "#     from " << basename << ".mod" << endl
                     << "#" << endl
                     << "export params_derivs" << endl << endl
                     << "function params_derivs(y, x, paramssteady_state, it_, "
                     << "ss_param_deriv, ss_param_2nd_deriv)" << endl
                     << model_local_vars_output.str()
                     << model_output.str()
                     << "rp = zeros(" << equations.size() << ", "
                     << symbol_table.param_nbr() << ");" << endl
                     << jacobian_output.str()
                     << "gp = zeros(" << equations.size() << ", " << dynJacobianColsNbr << ", " << symbol_table.param_nbr() << ");" << endl
                     << hessian_output.str()
                     << "rpp = zeros(" << residuals_params_second_derivatives.size() << ",4);" << endl
                     << hessian1_output.str()
                     << "gpp = zeros(" << jacobian_params_second_derivatives.size() << ",5);" << endl
                     << third_derivs_output.str()
                     << "hp = zeros(" << hessian_params_derivatives.size() << ",5);" << endl
                     << third_derivs1_output.str()
                     << "(rp, gp, rpp, gpp, hp)" << endl
                     << "end" << endl
                     << "end" << endl;

  paramsDerivsFile.close();
}

void
DynamicModel::writeChainRuleDerivative(ostream &output, int eqr, int varr, int lag,
                                       ExprNodeOutputType output_type,
                                       const temporary_terms_t &temporary_terms) const
{
  map<pair<int, pair<int, int> >, expr_t>::const_iterator it = first_chain_rule_derivatives.find(make_pair(eqr, make_pair(varr, lag)));
  if (it != first_chain_rule_derivatives.end())
    (it->second)->writeOutput(output, output_type, temporary_terms);
  else
    output << 0;
}

void
DynamicModel::writeLatexFile(const string &basename, const bool write_equation_tags) const
{
  writeLatexModelFile(basename + "_dynamic", oLatexDynamicModel, write_equation_tags);
}

void
DynamicModel::writeLatexOriginalFile(const string &basename, const bool write_equation_tags) const
{
  writeLatexModelFile(basename + "_original", oLatexDynamicModel, write_equation_tags);
}

void
DynamicModel::substituteEndoLeadGreaterThanTwo(bool deterministic_model)
{
  substituteLeadLagInternal(avEndoLead, deterministic_model, vector<string>());
}

void
DynamicModel::substituteEndoLagGreaterThanTwo(bool deterministic_model)
{
  substituteLeadLagInternal(avEndoLag, deterministic_model, vector<string>());
}

void
DynamicModel::substituteExoLead(bool deterministic_model)
{
  substituteLeadLagInternal(avExoLead, deterministic_model, vector<string>());
}

void
DynamicModel::substituteExoLag(bool deterministic_model)
{
  substituteLeadLagInternal(avExoLag, deterministic_model, vector<string>());
}

void
DynamicModel::substituteLeadLagInternal(aux_var_t type, bool deterministic_model, const vector<string> &subset)
{
  ExprNode::subst_table_t subst_table;
  vector<BinaryOpNode *> neweqs;

  // Substitute in used model local variables
  set<int> used_local_vars;
  for (size_t i = 0; i < equations.size(); i++)
    equations[i]->collectVariables(eModelLocalVariable, used_local_vars);

  for (set<int>::const_iterator it = used_local_vars.begin();
       it != used_local_vars.end(); ++it)
    {
      const expr_t value = local_variables_table.find(*it)->second;
      expr_t subst;
      switch (type)
        {
        case avEndoLead:
          subst = value->substituteEndoLeadGreaterThanTwo(subst_table, neweqs, deterministic_model);
          break;
        case avEndoLag:
          subst = value->substituteEndoLagGreaterThanTwo(subst_table, neweqs);
          break;
        case avExoLead:
          subst = value->substituteExoLead(subst_table, neweqs, deterministic_model);
          break;
        case avExoLag:
          subst = value->substituteExoLag(subst_table, neweqs);
          break;
        case avDiffForward:
          subst = value->differentiateForwardVars(subset, subst_table, neweqs);
          break;
        default:
          cerr << "DynamicModel::substituteLeadLagInternal: impossible case" << endl;
          exit(EXIT_FAILURE);
        }
      local_variables_table[*it] = subst;
    }

  // Substitute in equations
  for (int i = 0; i < (int) equations.size(); i++)
    {
      expr_t subst;
      switch (type)
        {
        case avEndoLead:
          subst = equations[i]->substituteEndoLeadGreaterThanTwo(subst_table, neweqs, deterministic_model);
          break;
        case avEndoLag:
          subst = equations[i]->substituteEndoLagGreaterThanTwo(subst_table, neweqs);
          break;
        case avExoLead:
          subst = equations[i]->substituteExoLead(subst_table, neweqs, deterministic_model);
          break;
        case avExoLag:
          subst = equations[i]->substituteExoLag(subst_table, neweqs);
          break;
        case avDiffForward:
          subst = equations[i]->differentiateForwardVars(subset, subst_table, neweqs);
          break;
        default:
          cerr << "DynamicModel::substituteLeadLagInternal: impossible case" << endl;
          exit(EXIT_FAILURE);
        }
      BinaryOpNode *substeq = dynamic_cast<BinaryOpNode *>(subst);
      assert(substeq != NULL);
      equations[i] = substeq;
    }

  // Substitute in aux_equations
  // Without this loop, the auxiliary equations in equations
  // will diverge from those in aux_equations
  for (int i = 0; i < (int) aux_equations.size(); i++)
    {
      expr_t subst;
      switch (type)
        {
        case avEndoLead:
          subst = aux_equations[i]->substituteEndoLeadGreaterThanTwo(subst_table,
                                                                     neweqs, deterministic_model);
          break;
        case avEndoLag:
          subst = aux_equations[i]->substituteEndoLagGreaterThanTwo(subst_table, neweqs);
          break;
        case avExoLead:
          subst = aux_equations[i]->substituteExoLead(subst_table, neweqs, deterministic_model);
          break;
        case avExoLag:
          subst = aux_equations[i]->substituteExoLag(subst_table, neweqs);
          break;
        case avDiffForward:
          subst = aux_equations[i]->differentiateForwardVars(subset, subst_table, neweqs);
          break;
        default:
          cerr << "DynamicModel::substituteLeadLagInternal: impossible case" << endl;
          exit(EXIT_FAILURE);
        }
      BinaryOpNode *substeq = dynamic_cast<BinaryOpNode *>(subst);
      assert(substeq != NULL);
      aux_equations[i] = substeq;
    }

 // Substitute in diff_aux_equations
  // Without this loop, the auxiliary equations in equations
  // will diverge from those in diff_aux_equations
  for (int i = 0; i < (int) diff_aux_equations.size(); i++)
    {
      expr_t subst;
      switch (type)
        {
        case avEndoLead:
          subst = diff_aux_equations[i]->substituteEndoLeadGreaterThanTwo(subst_table,
                                                                     neweqs, deterministic_model);
          break;
        case avEndoLag:
          subst = diff_aux_equations[i]->substituteEndoLagGreaterThanTwo(subst_table, neweqs);
          break;
        case avExoLead:
          subst = diff_aux_equations[i]->substituteExoLead(subst_table, neweqs, deterministic_model);
          break;
        case avExoLag:
          subst = diff_aux_equations[i]->substituteExoLag(subst_table, neweqs);
          break;
        case avDiffForward:
          subst = diff_aux_equations[i]->differentiateForwardVars(subset, subst_table, neweqs);
          break;
        default:
          cerr << "DynamicModel::substituteLeadLagInternal: impossible case" << endl;
          exit(EXIT_FAILURE);
        }
      BinaryOpNode *substeq = dynamic_cast<BinaryOpNode *>(subst);
      assert(substeq != NULL);
      diff_aux_equations[i] = substeq;
    }

  // Add new equations
  for (int i = 0; i < (int) neweqs.size(); i++)
    addEquation(neweqs[i], -1);

  // Order of auxiliary variable definition equations:
  //  - expectation (entered before this function is called)
  //  - lead variables from lower lead to higher lead
  //  - lag variables from lower lag to higher lag
  copy(neweqs.begin(), neweqs.end(), back_inserter(aux_equations));

  if (neweqs.size() > 0)
    {
      cout << "Substitution of ";
      switch (type)
        {
        case avEndoLead:
          cout << "endo leads >= 2";
          break;
        case avEndoLag:
          cout << "endo lags >= 2";
          break;
        case avExoLead:
          cout << "exo leads";
          break;
        case avExoLag:
          cout << "exo lags";
          break;
        case avExpectation:
          cout << "expectation";
          break;
        case avDiffForward:
          cout << "forward vars";
          break;
        default:
          cerr << "DynamicModel::substituteLeadLagInternal: impossible case" << endl;
          exit(EXIT_FAILURE);
        }
      cout << ": added " << neweqs.size() << " auxiliary variables and equations." << endl;
    }
}

void
DynamicModel::substituteAdl()
{
  for (int i = 0; i < (int) equations.size(); i++)
    equations[i] = dynamic_cast<BinaryOpNode *>(equations[i]->substituteAdl());
}

void
DynamicModel::substituteDiff(StaticModel &static_model, ExprNode::subst_table_t &diff_subst_table)
{
  // Find diff Nodes
  diff_table_t diff_table;
  for (map<int, expr_t>::iterator it = local_variables_table.begin();
       it != local_variables_table.end(); it++)
    it->second->findDiffNodes(static_model, diff_table);

  for (int i = 0; i < (int) equations.size(); i++)
    equations[i]->findDiffNodes(static_model, diff_table);

  // Substitute in model local variables
  vector<BinaryOpNode *> neweqs;
  for (map<int, expr_t>::iterator it = local_variables_table.begin();
       it != local_variables_table.end(); it++)
    it->second = it->second->substituteDiff(static_model, diff_table, diff_subst_table, neweqs);

  // Substitute in equations
  for (int i = 0; i < (int) equations.size(); i++)
    {
      BinaryOpNode *substeq = dynamic_cast<BinaryOpNode *>(equations[i]->
                                                           substituteDiff(static_model, diff_table, diff_subst_table, neweqs));
      assert(substeq != NULL);
      equations[i] = substeq;
    }

  // Add new equations
  for (int i = 0; i < (int) neweqs.size(); i++)
    addEquation(neweqs[i], -1);

  copy(neweqs.begin(), neweqs.end(), back_inserter(diff_aux_equations));

  if (diff_subst_table.size() > 0)
    cout << "Substitution of Diff operator: added " << neweqs.size() << " auxiliary variables and equations." << endl;
}

void
DynamicModel::substituteDiffUnaryOps(StaticModel &static_model)
{
  // Find diff Nodes
  set<UnaryOpNode *> nodes;
  for (map<int, expr_t>::iterator it = local_variables_table.begin();
       it != local_variables_table.end(); it++)
    it->second->findDiffUnaryOpNodes(static_model, nodes);

  for (int i = 0; i < (int) equations.size(); i++)
    equations[i]->findDiffUnaryOpNodes(static_model, nodes);

  // Substitute in model local variables
  ExprNode::subst_table_t subst_table;
  vector<BinaryOpNode *> neweqs;
  for (map<int, expr_t>::iterator it = local_variables_table.begin();
       it != local_variables_table.end(); it++)
    it->second = it->second->substituteDiffUnaryOpNodes(static_model, nodes, subst_table, neweqs);

  // Substitute in equations
  for (int i = 0; i < (int) equations.size(); i++)
    {
      BinaryOpNode *substeq = dynamic_cast<BinaryOpNode *>(equations[i]->
                                                           substituteDiffUnaryOpNodes(static_model, nodes, subst_table, neweqs));
      assert(substeq != NULL);
      equations[i] = substeq;
    }

  // Add new equations
  for (int i = 0; i < (int) neweqs.size(); i++)
    addEquation(neweqs[i], -1);

  copy(neweqs.begin(), neweqs.end(), back_inserter(diff_aux_equations));

  if (subst_table.size() > 0)
    cout << "Substitution of Unary Ops in Diff operator: added " << neweqs.size() << " auxiliary variables and equations." << endl;
}

void
DynamicModel::combineDiffAuxEquations()
{
  copy(diff_aux_equations.begin(), diff_aux_equations.end(), back_inserter(aux_equations));
}

void
DynamicModel::substituteExpectation(bool partial_information_model)
{
  ExprNode::subst_table_t subst_table;
  vector<BinaryOpNode *> neweqs;

  // Substitute in model local variables
  for (map<int, expr_t>::iterator it = local_variables_table.begin();
       it != local_variables_table.end(); it++)
    it->second = it->second->substituteExpectation(subst_table, neweqs, partial_information_model);

  // Substitute in equations
  for (int i = 0; i < (int) equations.size(); i++)
    {
      BinaryOpNode *substeq = dynamic_cast<BinaryOpNode *>(equations[i]->substituteExpectation(subst_table, neweqs, partial_information_model));
      assert(substeq != NULL);
      equations[i] = substeq;
    }

  // Add new equations
  for (int i = 0; i < (int) neweqs.size(); i++)
    addEquation(neweqs[i], -1);

  // Add the new set of equations at the *beginning* of aux_equations
  copy(neweqs.rbegin(), neweqs.rend(), front_inserter(aux_equations));

  if (subst_table.size() > 0)
    {
      if (partial_information_model)
        cout << "Substitution of Expectation operator: added " << subst_table.size() << " auxiliary variables and " << neweqs.size() << " auxiliary equations." << endl;
      else
        cout << "Substitution of Expectation operator: added " << neweqs.size() << " auxiliary variables and equations." << endl;
    }
}

void
DynamicModel::transformPredeterminedVariables()
{
  for (map<int, expr_t>::iterator it = local_variables_table.begin();
       it != local_variables_table.end(); it++)
    it->second = it->second->decreaseLeadsLagsPredeterminedVariables();

  for (int i = 0; i < (int) equations.size(); i++)
    {
      BinaryOpNode *substeq = dynamic_cast<BinaryOpNode *>(equations[i]->decreaseLeadsLagsPredeterminedVariables());
      assert(substeq != NULL);
      equations[i] = substeq;
    }
}

void
DynamicModel::detrendEquations()
{
  // We go backwards in the list of trend_vars, to deal correctly with I(2) processes
  for (nonstationary_symbols_map_t::const_reverse_iterator it = nonstationary_symbols_map.rbegin();
       it != nonstationary_symbols_map.rend(); ++it)
    for (int i = 0; i < (int) equations.size(); i++)
      {
        BinaryOpNode *substeq = dynamic_cast<BinaryOpNode *>(equations[i]->detrend(it->first, it->second.first, it->second.second));
        assert(substeq != NULL);
        equations[i] = dynamic_cast<BinaryOpNode *>(substeq);
      }

  for (int i = 0; i < (int) equations.size(); i++)
    {
      BinaryOpNode *substeq = dynamic_cast<BinaryOpNode *>(equations[i]->removeTrendLeadLag(trend_symbols_map));
      assert(substeq != NULL);
      equations[i] = dynamic_cast<BinaryOpNode *>(substeq);
    }
}

void
DynamicModel::removeTrendVariableFromEquations()
{
  for (int i = 0; i < (int) equations.size(); i++)
    {
      BinaryOpNode *substeq = dynamic_cast<BinaryOpNode *>(equations[i]->replaceTrendVar());
      assert(substeq != NULL);
      equations[i] = dynamic_cast<BinaryOpNode *>(substeq);
    }
}

void
DynamicModel::differentiateForwardVars(const vector<string> &subset)
{
  substituteLeadLagInternal(avDiffForward, true, subset);
}

void
DynamicModel::fillEvalContext(eval_context_t &eval_context) const
{
  // First, auxiliary variables
  for (deque<BinaryOpNode *>::const_iterator it = aux_equations.begin();
       it != aux_equations.end(); it++)
    {
      assert((*it)->get_op_code() == oEqual);
      VariableNode *auxvar = dynamic_cast<VariableNode *>((*it)->get_arg1());
      assert(auxvar != NULL);
      try
        {
          double val = (*it)->get_arg2()->eval(eval_context);
          eval_context[auxvar->get_symb_id()] = val;
        }
      catch (ExprNode::EvalException &e)
        {
          // Do nothing
        }
    }

  // Second, model local variables
  for (map<int, expr_t>::const_iterator it = local_variables_table.begin();
       it != local_variables_table.end(); it++)
    {
      try
        {
          const expr_t expression = it->second;
          double val = expression->eval(eval_context);
          eval_context[it->first] = val;
        }
      catch (ExprNode::EvalException &e)
        {
          // Do nothing
        }
    }

  //Third, trend variables
  vector <int> trendVars = symbol_table.getTrendVarIds();
  for (vector <int>::const_iterator it = trendVars.begin();
       it != trendVars.end(); it++)
    eval_context[*it] = 2;                               //not <= 0 bc of log, not 1 bc of powers
}

bool
DynamicModel::isModelLocalVariableUsed() const
{
  set<int> used_local_vars;
  size_t i = 0;
  while (i < equations.size() && used_local_vars.size() == 0)
    {
      equations[i]->collectVariables(eModelLocalVariable, used_local_vars);
      i++;
    }
  return used_local_vars.size() > 0;
}

void
DynamicModel::addStaticOnlyEquation(expr_t eq, int lineno, const vector<pair<string, string> > &eq_tags)
{
  BinaryOpNode *beq = dynamic_cast<BinaryOpNode *>(eq);
  assert(beq != NULL && beq->get_op_code() == oEqual);

  vector<pair<string, string> > soe_eq_tags;
  for (size_t i = 0; i < eq_tags.size(); i++)
    soe_eq_tags.push_back(eq_tags[i]);

  static_only_equations.push_back(beq);
  static_only_equations_lineno.push_back(lineno);
  static_only_equations_equation_tags.push_back(soe_eq_tags);
}

size_t
DynamicModel::staticOnlyEquationsNbr() const
{
  return static_only_equations.size();
}

size_t
DynamicModel::dynamicOnlyEquationsNbr() const
{
  set<int> eqs;

  for (vector<pair<int, pair<string, string> > >::const_iterator it = equation_tags.begin();
       it != equation_tags.end(); ++it)
    if (it->second.first == "dynamic")
      eqs.insert(it->first);

  return eqs.size();
}

#ifndef PRIVATE_BUFFER_SIZE
# define PRIVATE_BUFFER_SIZE 1024
#endif

bool
DynamicModel::isChecksumMatching(const string &basename) const
{
  boost::crc_32_type result;

  std::stringstream buffer;

  // Write equation tags
  for (size_t i = 0; i < equation_tags.size(); i++)
    buffer << "  " << equation_tags[i].first + 1
           << equation_tags[i].second.first
           << equation_tags[i].second.second;

  ExprNodeOutputType buffer_type = oCDynamicModel;

  for (int eq = 0; eq < (int) equations.size(); eq++)
    {
      BinaryOpNode *eq_node = equations[eq];
      expr_t lhs = eq_node->get_arg1();
      expr_t rhs = eq_node->get_arg2();

      // Test if the right hand side of the equation is empty.
      double vrhs = 1.0;
      try
        {
          vrhs = rhs->eval(eval_context_t());
        }
      catch (ExprNode::EvalException &e)
        {
        }

      if (vrhs != 0) // The right hand side of the equation is not empty ==> residual=lhs-rhs;
        {
          buffer << "lhs =";
          lhs->writeOutput(buffer, buffer_type, temporary_terms);
          buffer << ";" << endl;

          buffer << "rhs =";
          rhs->writeOutput(buffer, buffer_type, temporary_terms);
          buffer << ";" << endl;

          buffer << "residual" << LEFT_ARRAY_SUBSCRIPT(buffer_type)
                 << eq + ARRAY_SUBSCRIPT_OFFSET(buffer_type)
                 << RIGHT_ARRAY_SUBSCRIPT(buffer_type)
                 << "= lhs-rhs;" << endl;
        }
      else // The right hand side of the equation is empty ==> residual=lhs;
        {
          buffer << "residual" << LEFT_ARRAY_SUBSCRIPT(buffer_type)
                 << eq + ARRAY_SUBSCRIPT_OFFSET(buffer_type)
                 << RIGHT_ARRAY_SUBSCRIPT(buffer_type)
                 << " = ";
          lhs->writeOutput(buffer, buffer_type, temporary_terms);
          buffer << ";" << endl;
        }
    }

  char private_buffer[PRIVATE_BUFFER_SIZE];
  while (buffer)
    {
      buffer.get(private_buffer, PRIVATE_BUFFER_SIZE);
      result.process_bytes(private_buffer, strlen(private_buffer));
    }

  bool basename_dir_exists = false;
#ifdef _WIN32
  int r = mkdir(basename.c_str());
#else
  int r = mkdir(basename.c_str(), 0777);
#endif
  if (r < 0)
    if (errno != EEXIST)
      {
        perror("ERROR");
        exit(EXIT_FAILURE);
      }
    else
      basename_dir_exists = true;

  // check whether basename directory exist. If not, create it.
  // If it does, read old checksum if it exist
  fstream checksum_file;
  string filename = basename + "/checksum";
  unsigned int old_checksum = 0;
  // read old checksum if it exists
  if (basename_dir_exists)
    {
      checksum_file.open(filename.c_str(), ios::in | ios::binary);
      if (checksum_file.is_open())
        {
          checksum_file >> old_checksum;
          checksum_file.close();
        }
    }
  // write new checksum file if none or different from old checksum
  if (old_checksum != result.checksum())
    {
      checksum_file.open(filename.c_str(), ios::out | ios::binary);
      if (!checksum_file.is_open())
        {
          cerr << "ERROR: Can't open file " << filename << endl;
          exit(EXIT_FAILURE);
        }
      checksum_file << result.checksum();
      checksum_file.close();
      return false;
    }

  return true;
}

void
DynamicModel::writeCOutput(ostream &output, const string &basename, bool block_decomposition, bool byte_code, bool use_dll, int order, bool estimation_present) const
{
  int lag_presence[3];
  // Loop on endogenous variables
  vector<int> zeta_back, zeta_mixed, zeta_fwrd, zeta_static;
  for (int endoID = 0; endoID < symbol_table.endo_nbr(); endoID++)
    {
      // Loop on periods
      for (int lag = 0; lag <= 2; lag++)
        {
          lag_presence[lag] = 1;
          try
            {
              getDerivID(symbol_table.getID(eEndogenous, endoID), lag-1);
            }
          catch (UnknownDerivIDException &e)
            {
              lag_presence[lag] = 0;
            }
        }
      if (lag_presence[0] == 1)
        if (lag_presence[2] == 1)
          zeta_mixed.push_back(endoID);
        else
          zeta_back.push_back(endoID);
      else if (lag_presence[2] == 1)
        zeta_fwrd.push_back(endoID);
      else
        zeta_static.push_back(endoID);

    }
  output << "size_t nstatic = " << zeta_static.size() << ";" << endl
         << "size_t nfwrd   = " << zeta_fwrd.size() << ";" << endl
         << "size_t nback   = " << zeta_back.size() << ";" << endl
         << "size_t nmixed  = " << zeta_mixed.size() << ";" << endl;
  output << "size_t zeta_static[" << zeta_static.size() << "] = {";
  for (vector<int>::iterator i = zeta_static.begin(); i != zeta_static.end(); ++i)
    {
      if (i != zeta_static.begin())
        output << ",";
      output << *i;
    }
  output << "};" << endl;

  output << "size_t zeta_back[" << zeta_back.size() << "] = {";
  for (vector<int>::iterator i = zeta_back.begin(); i != zeta_back.end(); ++i)
    {
      if (i != zeta_back.begin())
        output << ",";
      output << *i;
    }
  output << "};" << endl;

  output << "size_t zeta_fwrd[" << zeta_fwrd.size() << "] = {";
  for (vector<int>::iterator i = zeta_fwrd.begin(); i != zeta_fwrd.end(); ++i)
    {
      if (i != zeta_fwrd.begin())
        output << ",";
      output << *i;
    }
  output << "};" << endl;

  output << "size_t zeta_mixed[" << zeta_mixed.size() << "] = {";
  for (vector<int>::iterator i = zeta_mixed.begin(); i != zeta_mixed.end(); ++i)
    {
      if (i != zeta_mixed.begin())
        output << ",";
      output << *i;
    }
  output << "};" << endl;

  // Write number of non-zero derivatives
  // Use -1 if the derivatives have not been computed
  output << "int *NNZDerivatives[3] = {";
  switch (order)
    {
    case 0:
      output << NNZDerivatives[0] << ",-1,-1};" << endl;
      break;
    case 1:
      output << NNZDerivatives[0] << "," << NNZDerivatives[1] << ",-1};" << endl;
      break;
    case 2:
      output << NNZDerivatives[0] << "," << NNZDerivatives[1] << "," << NNZDerivatives[2] << "};" << endl;
      break;
    default:
      cerr << "Order larger than 3 not implemented" << endl;
      exit(EXIT_FAILURE);
    }
}

void
DynamicModel::writeResidualsC(const string &basename, bool cuda) const
{
  string filename = basename + "_residuals.c";
  ofstream mDynamicModelFile, mDynamicMexFile;

  mDynamicModelFile.open(filename.c_str(), ios::out | ios::binary);
  if (!mDynamicModelFile.is_open())
    {
      cerr << "Error: Can't open file " << filename << " for writing" << endl;
      exit(EXIT_FAILURE);
    }
  mDynamicModelFile << "/*" << endl
                    << " * " << filename << " : Computes residuals of the model for Dynare" << endl
                    << " *" << endl
                    << " * Warning : this file is generated automatically by Dynare" << endl
                    << " *           from model " << basename << "(.mod)" << endl
                    << " */" << endl
#if defined(_WIN32) || defined(__CYGWIN32__) || defined(__MINGW32__)
                    << "#ifdef _MSC_VER" << endl
                    << "#define _USE_MATH_DEFINES" << endl
                    << "#endif" << endl
#endif
                    << "#include <math.h>" << endl;

  mDynamicModelFile << "#include <stdlib.h>" << endl;

  mDynamicModelFile << "#define max(a, b) (((a) > (b)) ? (a) : (b))" << endl
                    << "#define min(a, b) (((a) > (b)) ? (b) : (a))" << endl;

  // Write function definition if oPowerDeriv is used
  // even for residuals if doing Ramsey
  writePowerDerivCHeader(mDynamicModelFile);
  writeNormcdfCHeader(mDynamicModelFile);

  mDynamicModelFile << "void Residuals(const double *y, double *x, int nb_row_x, double *params, double *steady_state, int it_, double *residual)" << endl
                    << "{" << endl;

  // this is always empty here, but needed by d1->writeOutput
  deriv_node_temp_terms_t tef_terms;

  ostringstream model_output;    // Used for storing model equations
  writeModelEquations(model_output, oCDynamic2Model);

  mDynamicModelFile << "  double lhs, rhs;" << endl
                    << endl
                    << "  /* Residual equations */" << endl
                    << model_output.str()
                    << "}" << endl;

  writePowerDeriv(mDynamicModelFile);
  writeNormcdf(mDynamicModelFile);
  mDynamicModelFile.close();

}

void
DynamicModel::writeFirstDerivativesC(const string &basename, bool cuda) const
{
  string filename = basename + "_first_derivatives.c";
  ofstream mDynamicModelFile, mDynamicMexFile;

  mDynamicModelFile.open(filename.c_str(), ios::out | ios::binary);
  if (!mDynamicModelFile.is_open())
    {
      cerr << "Error: Can't open file " << filename << " for writing" << endl;
      exit(EXIT_FAILURE);
    }
  mDynamicModelFile << "/*" << endl
                    << " * " << filename << " : Computes first order derivatives of the model for Dynare" << endl
                    << " *" << endl
                    << " * Warning : this file is generated automatically by Dynare" << endl
                    << " *           from model " << basename << "(.mod)" << endl
                    << " */" << endl
#if defined(_WIN32) || defined(__CYGWIN32__) || defined(__MINGW32__)
                    << "#ifdef _MSC_VER" << endl
                    << "#define _USE_MATH_DEFINES" << endl
                    << "#endif" << endl
#endif
                    << "#include <math.h>" << endl;

  mDynamicModelFile << "#include <stdlib.h>" << endl;

  mDynamicModelFile << "#define max(a, b) (((a) > (b)) ? (a) : (b))" << endl
                    << "#define min(a, b) (((a) > (b)) ? (b) : (a))" << endl;

  // Write function definition if oPowerDeriv is used
  writePowerDerivCHeader(mDynamicModelFile);
  writeNormcdfCHeader(mDynamicModelFile);

  mDynamicModelFile << "void FirstDerivatives(const double *y, double *x, int nb_row_x, double *params, double *steady_state, int it_, double *residual, double *g1, double *v2, double *v3)" << endl
                    << "{" << endl;

  // this is always empty here, but needed by d1->writeOutput
  deriv_node_temp_terms_t tef_terms;

  // Writing Jacobian
  for (first_derivatives_t::const_iterator it = first_derivatives.begin();
       it != first_derivatives.end(); it++)
    {
      int eq = it->first.first;
      int var = it->first.second;
      expr_t d1 = it->second;

      jacobianHelper(mDynamicModelFile, eq, getDynJacobianCol(var), oCDynamicModel);
      mDynamicModelFile << "=";
      // oCStaticModel makes reference to the static variables
      // oCDynamicModel makes reference to the dynamic variables
      d1->writeOutput(mDynamicModelFile, oCDynamicModel, temporary_terms, tef_terms);
      mDynamicModelFile << ";" << endl;
    }

  mDynamicModelFile << "}" << endl;

  mDynamicModelFile.close();

}

// using compressed sparse row format (CSR)
void
DynamicModel::writeFirstDerivativesC_csr(const string &basename, bool cuda) const
{
  string filename = basename + "_first_derivatives.c";
  ofstream mDynamicModelFile, mDynamicMexFile;

  mDynamicModelFile.open(filename.c_str(), ios::out | ios::binary);
  if (!mDynamicModelFile.is_open())
    {
      cerr << "Error: Can't open file " << filename << " for writing" << endl;
      exit(EXIT_FAILURE);
    }
  mDynamicModelFile << "/*" << endl
                    << " * " << filename << " : Computes first order derivatives of the model for Dynare" << endl
                    << " *" << endl
                    << " * Warning : this file is generated automatically by Dynare" << endl
                    << " *           from model " << basename << "(.mod)" << endl
                    << " */" << endl
#if defined(_WIN32) || defined(__CYGWIN32__) || defined(__MINGW32__)
                    << "#ifdef _MSC_VER" << endl
                    << "#define _USE_MATH_DEFINES" << endl
                    << "#endif" << endl
#endif
                    << "#include <math.h>" << endl;

  mDynamicModelFile << "#include <stdlib.h>" << endl;

  mDynamicModelFile << "#define max(a, b) (((a) > (b)) ? (a) : (b))" << endl
                    << "#define min(a, b) (((a) > (b)) ? (b) : (a))" << endl;

  // Write function definition if oPowerDeriv is used
  writePowerDerivCHeader(mDynamicModelFile);
  writeNormcdfCHeader(mDynamicModelFile);

  mDynamicModelFile << "void FirstDerivatives(const double *y, double *x, int nb_row_x, double *params, double *steady_state, int it_, double *residual, int *row_ptr, int *col_ptr, double *value)" << endl
                    << "{" << endl;

  int cols_nbr = 3*symbol_table.endo_nbr() + symbol_table.exo_nbr() + symbol_table.exo_det_nbr();
  // this is always empty here, but needed by d1->writeOutput
  deriv_node_temp_terms_t tef_terms;

  // Indexing derivatives in column order
  vector<derivative> D;
  for (first_derivatives_t::const_iterator it = first_derivatives.begin();
       it != first_derivatives.end(); it++)
    {
      int eq = it->first.first;
      int dynvar = it->first.second;
      int lag = getLagByDerivID(dynvar);
      int symb_id = getSymbIDByDerivID(dynvar);
      SymbolType type = getTypeByDerivID(dynvar);
      int tsid = symbol_table.getTypeSpecificID(symb_id);
      int col_id;
      switch (type)
        {
        case eEndogenous:
          col_id = tsid+(lag+1)*symbol_table.endo_nbr();
          break;
        case eExogenous:
          col_id = tsid+3*symbol_table.endo_nbr();
          break;
        case eExogenousDet:
          col_id = tsid+3*symbol_table.endo_nbr()+symbol_table.exo_nbr();
          break;
        default:
          std::cerr << "This case shouldn't happen" << std::endl;
          exit(EXIT_FAILURE);
        }
      derivative deriv(col_id + eq *cols_nbr, col_id, eq, it->second);
      D.push_back(deriv);
    }
  sort(D.begin(), D.end(), derivative_less_than());

  // writing sparse Jacobian
  vector<int> row_ptr(equations.size());
  fill(row_ptr.begin(), row_ptr.end(), 0.0);
  int k = 0;
  for (vector<derivative>::const_iterator it = D.begin(); it != D.end(); ++it)
    {
      row_ptr[it->row_nbr]++;
      mDynamicModelFile << "col_ptr[" << k << "] "
                        << "=" << it->col_nbr << ";" << endl;
      mDynamicModelFile << "value[" << k << "] = ";
      // oCstaticModel makes reference to the static variables
      it->value->writeOutput(mDynamicModelFile, oCDynamic2Model, temporary_terms, tef_terms);
      mDynamicModelFile << ";" << endl;
      k++;
    }

  // row_ptr must point to the relative address of the first element of the row
  int cumsum = 0;
  mDynamicModelFile << "int row_ptr_data[" <<  row_ptr.size() + 1 << "] = { 0";
  for (vector<int>::iterator it = row_ptr.begin(); it != row_ptr.end(); ++it)
    {
      cumsum += *it;
      mDynamicModelFile << ", " << cumsum;
    }
  mDynamicModelFile << "};" << endl
                    << "int i;" << endl
                    << "for (i=0; i < " << row_ptr.size() + 1 << "; i++) row_ptr[i] = row_ptr_data[i];" << endl;
  mDynamicModelFile << "}" << endl;

  mDynamicModelFile.close();

}
void
DynamicModel::writeSecondDerivativesC_csr(const string &basename, bool cuda) const
{

  string filename = basename + "_second_derivatives.c";
  ofstream mDynamicModelFile, mDynamicMexFile;

  mDynamicModelFile.open(filename.c_str(), ios::out | ios::binary);
  if (!mDynamicModelFile.is_open())
    {
      cerr << "Error: Can't open file " << filename << " for writing" << endl;
      exit(EXIT_FAILURE);
    }
  mDynamicModelFile << "/*" << endl
                    << " * " << filename << " : Computes second order derivatives of the model for Dynare" << endl
                    << " *" << endl
                    << " * Warning : this file is generated automatically by Dynare" << endl
                    << " *           from model " << basename << "(.mod)" << endl
                    << " */" << endl
#if defined(_WIN32) || defined(__CYGWIN32__) || defined(__MINGW32__)
                    << "#ifdef _MSC_VER" << endl
                    << "#define _USE_MATH_DEFINES" << endl
                    << "#endif" << endl
#endif
                    << "#include <math.h>" << endl;

  mDynamicModelFile << "#include <stdlib.h>" << endl;

  mDynamicModelFile << "#define max(a, b) (((a) > (b)) ? (a) : (b))" << endl
                    << "#define min(a, b) (((a) > (b)) ? (b) : (a))" << endl;

  // write function definition if oPowerDeriv is used
  writePowerDerivCHeader(mDynamicModelFile);
  writeNormcdfCHeader(mDynamicModelFile);

  mDynamicModelFile << "void SecondDerivatives(const double *y, double *x, int nb_row_x, double *params, double *steady_state, int it_, double *residual, int *row_ptr, int *col_ptr, double *value)" << endl
                    << "{" << endl;

  // this is always empty here, but needed by d1->writeOutput
  deriv_node_temp_terms_t tef_terms;

  // Indexing derivatives in column order
  vector<derivative> D;
  int hessianColsNbr = dynJacobianColsNbr*dynJacobianColsNbr;
  for (second_derivatives_t::const_iterator it = second_derivatives.begin();
       it != second_derivatives.end(); it++)
    {
      int eq = it->first.first;
      int var1 = it->first.second.first;
      int var2 = it->first.second.second;

      int id1 = getDynJacobianCol(var1);
      int id2 = getDynJacobianCol(var2);

      int col_nb = id1 * dynJacobianColsNbr + id2;

      derivative deriv(col_nb + eq *hessianColsNbr, col_nb, eq, it->second);
      D.push_back(deriv);
      if (id1 != id2)
        {
          col_nb = id2 * dynJacobianColsNbr + id1;
          derivative deriv(col_nb + eq *hessianColsNbr, col_nb, eq, it->second);
          D.push_back(deriv);
        }
    }
  sort(D.begin(), D.end(), derivative_less_than());

  // Writing Hessian
  vector<int> row_ptr(equations.size());
  fill(row_ptr.begin(), row_ptr.end(), 0.0);
  int k = 0;
  for (vector<derivative>::const_iterator it = D.begin(); it != D.end(); ++it)
    {
      row_ptr[it->row_nbr]++;
      mDynamicModelFile << "col_ptr[" << k << "] "
                        << "=" << it->col_nbr << ";" << endl;
      mDynamicModelFile << "value[" << k << "] = ";
      // oCstaticModel makes reference to the static variables
      it->value->writeOutput(mDynamicModelFile, oCStaticModel, temporary_terms, tef_terms);
      mDynamicModelFile << ";" << endl;
      k++;
    }

  // row_ptr must point to the relative address of the first element of the row
  int cumsum = 0;
  mDynamicModelFile << "row_ptr = [ 0";
  for (vector<int>::iterator it = row_ptr.begin(); it != row_ptr.end(); ++it)
    {
      cumsum += *it;
      mDynamicModelFile << ", " << cumsum;
    }
  mDynamicModelFile << "];" << endl;

  mDynamicModelFile << "}" << endl;

  writePowerDeriv(mDynamicModelFile);
  writeNormcdf(mDynamicModelFile);
  mDynamicModelFile.close();
}

void
DynamicModel::writeThirdDerivativesC_csr(const string &basename, bool cuda) const
{
  string filename = basename + "_third_derivatives.c";
  ofstream mDynamicModelFile, mDynamicMexFile;

  mDynamicModelFile.open(filename.c_str(), ios::out | ios::binary);
  if (!mDynamicModelFile.is_open())
    {
      cerr << "Error: Can't open file " << filename << " for writing" << endl;
      exit(EXIT_FAILURE);
    }
  mDynamicModelFile << "/*" << endl
                    << " * " << filename << " : Computes third order derivatives of the model for Dynare" << endl
                    << " *" << endl
                    << " * Warning : this file is generated automatically by Dynare" << endl
                    << " *           from model " << basename << "(.mod)" << endl
                    << " */" << endl
#if defined(_WIN32) || defined(__CYGWIN32__) || defined(__MINGW32__)
                    << "#ifdef _MSC_VER" << endl
                    << "#define _USE_MATH_DEFINES" << endl
                    << "#endif" << endl
#endif
                    << "#include <math.h>" << endl;

  mDynamicModelFile << "#include <stdlib.h>" << endl;

  mDynamicModelFile << "#define max(a, b) (((a) > (b)) ? (a) : (b))" << endl
                    << "#define min(a, b) (((a) > (b)) ? (b) : (a))" << endl;

  // Write function definition if oPowerDeriv is used
  writePowerDerivCHeader(mDynamicModelFile);
  writeNormcdfCHeader(mDynamicModelFile);

  mDynamicModelFile << "void ThirdDerivatives(const double *y, double *x, int nb_row_x, double *params, double *steady_state, int it_, double *residual, double *g1, double *v2, double *v3)" << endl
                    << "{" << endl;

  // this is always empty here, but needed by d1->writeOutput
  deriv_node_temp_terms_t tef_terms;

  vector<derivative> D;
  int hessianColsNbr = dynJacobianColsNbr*dynJacobianColsNbr;
  int thirdDerivativesColsNbr = hessianColsNbr*dynJacobianColsNbr;
  for (third_derivatives_t::const_iterator it = third_derivatives.begin();
       it != third_derivatives.end(); it++)
    {
      int eq = it->first.first;
      int var1 = it->first.second.first;
      int var2 = it->first.second.second.first;
      int var3 = it->first.second.second.second;

      int id1 = getDynJacobianCol(var1);
      int id2 = getDynJacobianCol(var2);
      int id3 = getDynJacobianCol(var3);

      // Reference column number for the g3 matrix (with symmetrical derivatives)
      vector<long unsigned int>  cols;
      long unsigned int col_nb = id1 * hessianColsNbr + id2 * dynJacobianColsNbr + id3;
      int thirdDColsNbr = hessianColsNbr*dynJacobianColsNbr;
      derivative deriv(col_nb + eq *thirdDColsNbr, col_nb, eq, it->second);
      D.push_back(deriv);
      cols.push_back(col_nb);
      col_nb = id1 * hessianColsNbr + id3 * dynJacobianColsNbr + id2;
      if (find(cols.begin(), cols.end(), col_nb) == cols.end())
        {
          derivative deriv(col_nb + eq *thirdDerivativesColsNbr, col_nb, eq, it->second);
          D.push_back(deriv);
          cols.push_back(col_nb);
        }
      col_nb = id2 * hessianColsNbr + id1 * dynJacobianColsNbr + id3;
      if (find(cols.begin(), cols.end(), col_nb) == cols.end())
        {
          derivative deriv(col_nb + eq *thirdDerivativesColsNbr, col_nb, eq, it->second);
          D.push_back(deriv);
          cols.push_back(col_nb);
        }
      col_nb = id2 * hessianColsNbr + id3 * dynJacobianColsNbr + id1;
      if (find(cols.begin(), cols.end(), col_nb) == cols.end())
        {
          derivative deriv(col_nb + eq *thirdDerivativesColsNbr, col_nb, eq, it->second);
          D.push_back(deriv);
          cols.push_back(col_nb);
        }
      col_nb = id3 * hessianColsNbr + id1 * dynJacobianColsNbr + id2;
      if (find(cols.begin(), cols.end(), col_nb) == cols.end())
        {
          derivative deriv(col_nb + eq *thirdDerivativesColsNbr, col_nb, eq, it->second);
          D.push_back(deriv);
          cols.push_back(col_nb);
        }
      col_nb = id3 * hessianColsNbr + id2 * dynJacobianColsNbr + id1;
      if (find(cols.begin(), cols.end(), col_nb) == cols.end())
        {
          derivative deriv(col_nb + eq *thirdDerivativesColsNbr, col_nb, eq, it->second);
          D.push_back(deriv);
        }
    }

  sort(D.begin(), D.end(), derivative_less_than());

  vector<int> row_ptr(equations.size());
  fill(row_ptr.begin(), row_ptr.end(), 0.0);
  int k = 0;
  for (vector<derivative>::const_iterator it = D.begin(); it != D.end(); ++it)
    {
      row_ptr[it->row_nbr]++;
      mDynamicModelFile << "col_ptr[" << k << "] "
                        << "=" << it->col_nbr << ";" << endl;
      mDynamicModelFile << "value[" << k << "] = ";
      // oCstaticModel makes reference to the static variables
      it->value->writeOutput(mDynamicModelFile, oCStaticModel, temporary_terms, tef_terms);
      mDynamicModelFile << ";" << endl;
      k++;
    }

  // row_ptr must point to the relative address of the first element of the row
  int cumsum = 0;
  mDynamicModelFile << "row_ptr = [ 0";
  for (vector<int>::iterator it = row_ptr.begin(); it != row_ptr.end(); ++it)
    {
      cumsum += *it;
      mDynamicModelFile << ", " << cumsum;
    }
  mDynamicModelFile << "];" << endl;

  mDynamicModelFile << "}" << endl;

  writePowerDeriv(mDynamicModelFile);
  writeNormcdf(mDynamicModelFile);
  mDynamicModelFile.close();

}

void
DynamicModel::writeCCOutput(ostream &output, const string &basename, bool block_decomposition, bool byte_code, bool use_dll, int order, bool estimation_present) const
{
  int lag_presence[3];
  // Loop on endogenous variables
  for (int endoID = 0; endoID < symbol_table.endo_nbr(); endoID++)
    {
      // Loop on periods
      for (int lag = 0; lag <= 2; lag++)
        {
          lag_presence[lag] = 1;
          try
            {
              getDerivID(symbol_table.getID(eEndogenous, endoID), lag-1);
            }
          catch (UnknownDerivIDException &e)
            {
              lag_presence[lag] = 0;
            }
        }
      if (lag_presence[0] == 1)
        if (lag_presence[2] == 1)
          output << "zeta_mixed.push_back(" << endoID << ");" << endl;
        else
          output << "zeta_back.push_back(" << endoID << ");" << endl;
      else if (lag_presence[2] == 1)
        output << "zeta_fwrd.push_back(" << endoID << ");" << endl;
      else
        output << "zeta_static.push_back(" << endoID << ");" << endl;

    }
  output << "nstatic = zeta_static.size();" << endl
         << "nfwrd   = zeta_fwrd.size();" << endl
         << "nback   = zeta_back.size();" << endl
         << "nmixed  = zeta_mixed.size();" << endl;

  // Write number of non-zero derivatives
  // Use -1 if the derivatives have not been computed
  output << endl
         << "NNZDerivatives.push_back(" << NNZDerivatives[0] << ");" << endl;
  if (order > 1)
    {
      output << "NNZDerivatives.push_back(" << NNZDerivatives[1] << ");" << endl;
      if (order > 2)
        output << "NNZDerivatives.push_back(" << NNZDerivatives[2] << ");" << endl;
      else
        output << "NNZDerivatives.push_back(-1);" << endl;
    }
  else
    output << "NNZDerivatives.push_back(-1);" << endl
           << "NNZDerivatives.push_back(-1);" << endl;
}

void
DynamicModel::writeJsonOutput(ostream &output) const
{
  writeJsonModelEquations(output, false);
  output << ", ";
  writeJsonXrefs(output);
}

void
DynamicModel::writeJsonXrefsHelper(ostream &output, const map<pair<int, int>, set<int> > &xrefs) const
{
  for (map<pair<int, int>, set<int> >::const_iterator it = xrefs.begin();
       it != xrefs.end(); it++)
    {
      if (it != xrefs.begin())
        output << ", ";
      output << "{\"name\": \"" << symbol_table.getName(it->first.first) << "\""
             << ", \"shift\": " << it->first.second
             << ", \"equations\": [";
      for (set<int>::const_iterator it1 = it->second.begin();
           it1 != it->second.end(); it1++)
        {
          if (it1 != it->second.begin())
            output << ", ";
          output << *it1 + 1;
        }
      output << "]}";
    }
}

void
DynamicModel::writeJsonXrefs(ostream &output) const
{
  output << "\"xrefs\": {"
         << "\"parameters\": [";
  writeJsonXrefsHelper(output, xref_param);
  output << "]"
         << ", \"endogenous\": [";
  writeJsonXrefsHelper(output, xref_endo);
  output << "]"
         << ", \"exogenous\": [";
    writeJsonXrefsHelper(output, xref_exo);
  output << "]"
         << ", \"exogenous_deterministic\": [";
  writeJsonXrefsHelper(output, xref_exo_det);
  output << "]}" << endl;
}

void
DynamicModel::writeJsonOriginalModelOutput(ostream &output) const
{
  writeJsonModelEquations(output, false);
}

void
DynamicModel::writeJsonDynamicModelInfo(ostream &output) const
{
  output << "\"model_info\": {"
         << "\"lead_lag_incidence\": [";
  // Loop on endogenous variables
  int nstatic = 0,
    nfwrd   = 0,
    npred   = 0,
    nboth   = 0;
  for (int endoID = 0; endoID < symbol_table.endo_nbr(); endoID++)
    {
      if (endoID != 0)
        output << ",";
      output << "[";
      int sstatic = 1,
        sfwrd   = 0,
        spred   = 0,
        sboth   = 0;
      // Loop on periods
      for (int lag = -max_endo_lag; lag <= max_endo_lead; lag++)
        {
          // Print variableID if exists with current period, otherwise print 0
          try
            {
              if (lag != -max_endo_lag)
                output << ",";
              int varID = getDerivID(symbol_table.getID(eEndogenous, endoID), lag);
              output << " " << getDynJacobianCol(varID) + 1;
              if (lag == -1)
                {
                  sstatic = 0;
                  spred = 1;
                }
              else if (lag == 1)
                {
                  if (spred == 1)
                    {
                      sboth = 1;
                      spred = 0;
                    }
                  else
                    {
                      sstatic = 0;
                      sfwrd = 1;
                    }
                }
            }
          catch (UnknownDerivIDException &e)
            {
              output << " 0";
            }
        }
      nstatic += sstatic;
      nfwrd   += sfwrd;
      npred   += spred;
      nboth   += sboth;
      output << "]";
    }
  output << "], "
         << "\"nstatic\": " << nstatic << ", "
         << "\"nfwrd\": " << nfwrd << ", "
         << "\"npred\": " << npred << ", "
         << "\"nboth\": " << nboth << ", "
         << "\"nsfwrd\": " << nfwrd+nboth << ", "
         << "\"nspred\": " << npred+nboth << ", "
         << "\"ndynamic\": " << npred+nboth+nfwrd << endl;
  output << "}";
}

void
DynamicModel::writeJsonComputingPassOutput(ostream &output, bool writeDetails) const
{
  ostringstream model_local_vars_output;  // Used for storing model local vars
  ostringstream model_output;             // Used for storing model temp vars and equations
  ostringstream jacobian_output;          // Used for storing jacobian equations
  ostringstream hessian_output;           // Used for storing Hessian equations
  ostringstream third_derivatives_output; // Used for storing third order derivatives equations

  deriv_node_temp_terms_t tef_terms;
  temporary_terms_t temp_term_empty;
  temporary_terms_t temp_term_union = temporary_terms_res;
  temporary_terms_t temp_term_union_m_1;

  string concat = "";
  int hessianColsNbr = dynJacobianColsNbr * dynJacobianColsNbr;

  writeJsonModelLocalVariables(model_local_vars_output, tef_terms);

  writeJsonTemporaryTerms(temporary_terms_res, temp_term_union_m_1, model_output, tef_terms, concat);
  model_output << ", ";
  writeJsonModelEquations(model_output, true);

  // Writing Jacobian
  temp_term_union_m_1 = temp_term_union;
  temp_term_union.insert(temporary_terms_g1.begin(), temporary_terms_g1.end());
  concat = "jacobian";
  writeJsonTemporaryTerms(temp_term_union, temp_term_union_m_1, jacobian_output, tef_terms, concat);
  jacobian_output << ", \"jacobian\": {"
                  << "  \"nrows\": " << equations.size()
                  << ", \"ncols\": " << dynJacobianColsNbr
                  << ", \"entries\": [";
  for (first_derivatives_t::const_iterator it = first_derivatives.begin();
       it != first_derivatives.end(); it++)
    {
      if (it != first_derivatives.begin())
        jacobian_output << ", ";

      int eq = it->first.first;
      int var = it->first.second;
      int col =  getDynJacobianCol(var);
      expr_t d1 = it->second;

      if (writeDetails)
        jacobian_output << "{\"eq\": " << eq + 1;
      else
        jacobian_output << "{\"row\": " << eq + 1;

      jacobian_output << ", \"col\": " << col + 1;

      if (writeDetails)
        jacobian_output << ", \"var\": \"" << symbol_table.getName(getSymbIDByDerivID(var)) << "\""
                        << ", \"shift\": " << getLagByDerivID(var);

      jacobian_output << ", \"val\": \"";
      d1->writeJsonOutput(jacobian_output, temp_term_union, tef_terms);
      jacobian_output << "\"}" << endl;
    }
  jacobian_output << "]}";

  // Writing Hessian
  temp_term_union_m_1 = temp_term_union;
  temp_term_union.insert(temporary_terms_g2.begin(), temporary_terms_g2.end());
  concat = "hessian";
  writeJsonTemporaryTerms(temp_term_union, temp_term_union_m_1, hessian_output, tef_terms, concat);
  hessian_output << ", \"hessian\": {"
                 << "  \"nrows\": " << equations.size()
                 << ", \"ncols\": " << hessianColsNbr
                 << ", \"entries\": [";
  for (second_derivatives_t::const_iterator it = second_derivatives.begin();
       it != second_derivatives.end(); it++)
    {
      if (it != second_derivatives.begin())
        hessian_output << ", ";

      int eq = it->first.first;
      int var1 = it->first.second.first;
      int var2 = it->first.second.second;
      expr_t d2 = it->second;
      int id1 = getDynJacobianCol(var1);
      int id2 = getDynJacobianCol(var2);
      int col_nb = id1 * dynJacobianColsNbr + id2;
      int col_nb_sym = id2 * dynJacobianColsNbr + id1;

      if (writeDetails)
        hessian_output << "{\"eq\": " << eq + 1;
      else
        hessian_output << "{\"row\": " << eq + 1;

      hessian_output << ", \"col\": [" << col_nb + 1;
      if (id1 != id2)
        hessian_output << ", " << col_nb_sym + 1;
      hessian_output << "]";

      if (writeDetails)
        hessian_output << ", \"var1\": \"" << symbol_table.getName(getSymbIDByDerivID(var1)) << "\""
                       << ", \"shift1\": " << getLagByDerivID(var1)
                       << ", \"var2\": \"" << symbol_table.getName(getSymbIDByDerivID(var2)) << "\""
                       << ", \"shift2\": " << getLagByDerivID(var2);

      hessian_output << ", \"val\": \"";
      d2->writeJsonOutput(hessian_output, temp_term_union, tef_terms);
      hessian_output << "\"}" << endl;
    }
  hessian_output << "]}";

  // Writing third derivatives
  temp_term_union_m_1 = temp_term_union;
  temp_term_union.insert(temporary_terms_g3.begin(), temporary_terms_g3.end());
  concat = "third_derivatives";
  writeJsonTemporaryTerms(temp_term_union, temp_term_union_m_1, third_derivatives_output, tef_terms, concat);
  third_derivatives_output << ", \"third_derivative\": {"
                           << "  \"nrows\": " << equations.size()
                           << ", \"ncols\": " << hessianColsNbr * dynJacobianColsNbr
                           << ", \"entries\": [";
  for (third_derivatives_t::const_iterator it = third_derivatives.begin();
       it != third_derivatives.end(); it++)
    {
      if (it != third_derivatives.begin())
        third_derivatives_output << ", ";

      int eq = it->first.first;
      int var1 = it->first.second.first;
      int var2 = it->first.second.second.first;
      int var3 = it->first.second.second.second;
      expr_t d3 = it->second;

      if (writeDetails)
        third_derivatives_output << "{\"eq\": " << eq + 1;
      else
        third_derivatives_output << "{\"row\": " << eq + 1;

      int id1 = getDynJacobianCol(var1);
      int id2 = getDynJacobianCol(var2);
      int id3 = getDynJacobianCol(var3);
      set<int> cols;
      cols.insert(id1 * hessianColsNbr + id2 * dynJacobianColsNbr + id3);
      cols.insert(id1 * hessianColsNbr + id3 * dynJacobianColsNbr + id2);
      cols.insert(id2 * hessianColsNbr + id1 * dynJacobianColsNbr + id3);
      cols.insert(id2 * hessianColsNbr + id3 * dynJacobianColsNbr + id1);
      cols.insert(id3 * hessianColsNbr + id1 * dynJacobianColsNbr + id2);
      cols.insert(id3 * hessianColsNbr + id2 * dynJacobianColsNbr + id1);

      third_derivatives_output << ", \"col\": [";
      for (set<int>::iterator it2 = cols.begin(); it2 != cols.end(); it2++)
        {
          if (it2 != cols.begin())
            third_derivatives_output << ", ";
          third_derivatives_output << *it2 + 1;
        }
      third_derivatives_output << "]";

      if (writeDetails)
        third_derivatives_output << ", \"var1\": \"" << symbol_table.getName(getSymbIDByDerivID(var1)) << "\""
                                 << ", \"shift1\": " << getLagByDerivID(var1)
                                 << ", \"var2\": \"" << symbol_table.getName(getSymbIDByDerivID(var2)) << "\""
                                 << ", \"shift2\": " << getLagByDerivID(var2)
                                 << ", \"var3\": \"" << symbol_table.getName(getSymbIDByDerivID(var3)) << "\""
                                 << ", \"shift3\": " << getLagByDerivID(var3);

      third_derivatives_output << ", \"val\": \"";
      d3->writeJsonOutput(third_derivatives_output, temp_term_union, tef_terms);
      third_derivatives_output << "\"}" << endl;
    }
  third_derivatives_output << "]}";

  if (writeDetails)
    output << "\"dynamic_model\": {";
  else
    output << "\"dynamic_model_simple\": {";
  output << model_local_vars_output.str()
         << ", " << model_output.str()
         << ", " << jacobian_output.str()
         << ", " << hessian_output.str()
         << ", " << third_derivatives_output.str()
         << "}";
}

void
DynamicModel::writeJsonParamsDerivativesFile(ostream &output, bool writeDetails) const
{
  if (!residuals_params_derivatives.size()
      && !residuals_params_second_derivatives.size()
      && !jacobian_params_derivatives.size()
      && !jacobian_params_second_derivatives.size()
      && !hessian_params_derivatives.size())
    return;

  ostringstream model_local_vars_output;   // Used for storing model local vars
  ostringstream model_output;              // Used for storing model temp vars and equations
  ostringstream jacobian_output;           // Used for storing jacobian equations
  ostringstream hessian_output;            // Used for storing Hessian equations
  ostringstream hessian1_output;           // Used for storing Hessian equations
  ostringstream third_derivs_output;       // Used for storing third order derivatives equations
  ostringstream third_derivs1_output;      // Used for storing third order derivatives equations

  deriv_node_temp_terms_t tef_terms;
  writeJsonModelLocalVariables(model_local_vars_output, tef_terms);

  temporary_terms_t temp_terms_empty;
  string concat = "all";
  writeJsonTemporaryTerms(params_derivs_temporary_terms, temp_terms_empty, model_output, tef_terms, concat);
  jacobian_output << "\"deriv_wrt_params\": {"
                  << "  \"neqs\": " << equations.size()
                  << ", \"nparamcols\": " << symbol_table.param_nbr()
                  << ", \"entries\": [";
  for (first_derivatives_t::const_iterator it = residuals_params_derivatives.begin();
       it != residuals_params_derivatives.end(); it++)
    {
      if (it != residuals_params_derivatives.begin())
        jacobian_output << ", ";

      int eq = it->first.first;
      int param = it->first.second;
      expr_t d1 = it->second;

      int param_col = symbol_table.getTypeSpecificID(getSymbIDByDerivID(param)) + 1;

      if (writeDetails)
        jacobian_output << "{\"eq\": " << eq + 1;
      else
        jacobian_output << "{\"row\": " << eq + 1;

      jacobian_output << ", \"param_col\": " << param_col + 1;

      if (writeDetails)
        jacobian_output << ", \"param\": \"" << symbol_table.getName(getSymbIDByDerivID(param)) << "\"";

      jacobian_output << ", \"val\": \"";
      d1->writeJsonOutput(jacobian_output, params_derivs_temporary_terms, tef_terms);
      jacobian_output << "\"}" << endl;
    }
  jacobian_output << "]}";
  hessian_output << "\"deriv_jacobian_wrt_params\": {"
                 << "  \"neqs\": " << equations.size()
                 << ", \"nvarcols\": " << dynJacobianColsNbr
                 << ", \"nparamcols\": " << symbol_table.param_nbr()
                 << ", \"entries\": [";
  for (second_derivatives_t::const_iterator it = jacobian_params_derivatives.begin();
       it != jacobian_params_derivatives.end(); it++)
    {
      if (it != jacobian_params_derivatives.begin())
        hessian_output << ", ";

      int eq = it->first.first;
      int var = it->first.second.first;
      int param = it->first.second.second;
      expr_t d2 = it->second;

      int var_col = getDynJacobianCol(var) + 1;
      int param_col = symbol_table.getTypeSpecificID(getSymbIDByDerivID(param)) + 1;

      if (writeDetails)
        hessian_output << "{\"eq\": " << eq + 1;
      else
        hessian_output << "{\"row\": " << eq + 1;

      hessian_output << ", \"var_col\": " << var_col + 1
                     << ", \"param_col\": " << param_col + 1;

      if (writeDetails)
      hessian_output << ", \"var\": \"" << symbol_table.getName(getSymbIDByDerivID(var)) << "\""
                     << ", \"lag\": " << getLagByDerivID(var)
                     << ", \"param\": \"" << symbol_table.getName(getSymbIDByDerivID(param)) << "\"";

      hessian_output << ", \"val\": \"";
      d2->writeJsonOutput(hessian_output, params_derivs_temporary_terms, tef_terms);
      hessian_output << "\"}" << endl;
    }
  hessian_output << "]}";

  hessian1_output << "\"second_deriv_residuals_wrt_params\": {"
                  << "  \"nrows\": " << equations.size()
                  << ", \"nparam1cols\": " << symbol_table.param_nbr()
                  << ", \"nparam2cols\": " << symbol_table.param_nbr()
                  << ", \"entries\": [";
  for (second_derivatives_t::const_iterator it = residuals_params_second_derivatives.begin();
       it != residuals_params_second_derivatives.end(); ++it)
    {
      if (it != residuals_params_second_derivatives.begin())
        hessian1_output << ", ";

      int eq = it->first.first;
      int param1 = it->first.second.first;
      int param2 = it->first.second.second;
      expr_t d2 = it->second;

      int param1_col = symbol_table.getTypeSpecificID(getSymbIDByDerivID(param1)) + 1;
      int param2_col = symbol_table.getTypeSpecificID(getSymbIDByDerivID(param2)) + 1;

      if (writeDetails)
        hessian1_output << "{\"eq\": " << eq + 1;
      else
        hessian1_output << "{\"row\": " << eq + 1;
      hessian1_output << ", \"param1_col\": " << param1_col + 1
                      << ", \"param2_col\": " << param2_col + 1;

      if (writeDetails)
        hessian1_output << ", \"param1\": \"" << symbol_table.getName(getSymbIDByDerivID(param1)) << "\""
                        << ", \"param2\": \"" << symbol_table.getName(getSymbIDByDerivID(param2)) << "\"";

      hessian1_output << ", \"val\": \"";
      d2->writeJsonOutput(hessian1_output, params_derivs_temporary_terms, tef_terms);
      hessian1_output << "\"}" << endl;
    }
  hessian1_output << "]}";
  third_derivs_output << "\"second_deriv_jacobian_wrt_params\": {"
                      << "  \"neqs\": " << equations.size()
                      << ", \"nvarcols\": " << dynJacobianColsNbr
                      << ", \"nparam1cols\": " << symbol_table.param_nbr()
                      << ", \"nparam2cols\": " << symbol_table.param_nbr()
                      << ", \"entries\": [";
  for (third_derivatives_t::const_iterator it = jacobian_params_second_derivatives.begin();
       it != jacobian_params_second_derivatives.end(); ++it)
    {
      if (it != jacobian_params_second_derivatives.begin())
        third_derivs_output << ", ";

      int eq = it->first.first;
      int var = it->first.second.first;
      int param1 = it->first.second.second.first;
      int param2 = it->first.second.second.second;
      expr_t d2 = it->second;

      int var_col = getDynJacobianCol(var) + 1;
      int param1_col = symbol_table.getTypeSpecificID(getSymbIDByDerivID(param1)) + 1;
      int param2_col = symbol_table.getTypeSpecificID(getSymbIDByDerivID(param2)) + 1;

      if (writeDetails)
        third_derivs_output << "{\"eq\": " << eq + 1;
      else
        third_derivs_output << "{\"row\": " << eq + 1;

      third_derivs_output << ", \"var_col\": " << var_col + 1
                          << ", \"param1_col\": " << param1_col + 1
                          << ", \"param2_col\": " << param2_col + 1;

      if (writeDetails)
        third_derivs_output << ", \"var\": \"" << symbol_table.getName(var) << "\""
                            << ", \"lag\": " << getLagByDerivID(var)
                            << ", \"param1\": \"" << symbol_table.getName(getSymbIDByDerivID(param1)) << "\""
                            << ", \"param2\": \"" << symbol_table.getName(getSymbIDByDerivID(param2)) << "\"";

      third_derivs_output << ", \"val\": \"";
      d2->writeJsonOutput(third_derivs_output, params_derivs_temporary_terms, tef_terms);
      third_derivs_output << "\"}" << endl;
    }
  third_derivs_output << "]}" << endl;

  third_derivs1_output << "\"derivative_hessian_wrt_params\": {"
                       << "  \"neqs\": " << equations.size()
                       << ", \"nvar1cols\": " << dynJacobianColsNbr
                       << ", \"nvar2cols\": " << dynJacobianColsNbr
                       << ", \"nparamcols\": " << symbol_table.param_nbr()
                       << ", \"entries\": [";
  for (third_derivatives_t::const_iterator it = hessian_params_derivatives.begin();
       it != hessian_params_derivatives.end(); ++it)
    {
      if (it != hessian_params_derivatives.begin())
        third_derivs1_output << ", ";

      int eq = it->first.first;
      int var1 = it->first.second.first;
      int var2 = it->first.second.second.first;
      int param = it->first.second.second.second;
      expr_t d2 = it->second;

      int var1_col = getDynJacobianCol(var1) + 1;
      int var2_col = getDynJacobianCol(var2) + 1;
      int param_col = symbol_table.getTypeSpecificID(getSymbIDByDerivID(param)) + 1;

      if (writeDetails)
        third_derivs1_output << "{\"eq\": " << eq + 1;
      else
        third_derivs1_output << "{\"row\": " << eq + 1;

      third_derivs1_output << ", \"var1_col\": " << var1_col + 1
                           << ", \"var2_col\": " << var2_col + 1
                           << ", \"param_col\": " << param_col + 1;

      if (writeDetails)
        third_derivs1_output << ", \"var1\": \"" << symbol_table.getName(getSymbIDByDerivID(var1)) << "\""
                             << ", \"lag1\": " << getLagByDerivID(var1)
                             << ", \"var2\": \"" << symbol_table.getName(getSymbIDByDerivID(var2)) << "\""
                             << ", \"lag2\": " << getLagByDerivID(var2)
                             << ", \"param\": \"" << symbol_table.getName(getSymbIDByDerivID(param)) << "\"";

      third_derivs1_output << ", \"val\": \"";
      d2->writeJsonOutput(third_derivs1_output, params_derivs_temporary_terms, tef_terms);
      third_derivs1_output << "\"}" << endl;
    }
  third_derivs1_output << "]}" << endl;

  if (writeDetails)
    output << "\"dynamic_model_params_derivative\": {";
  else
    output << "\"dynamic_model_params_derivatives_simple\": {";
  output << model_local_vars_output.str()
         << ", " << model_output.str()
         << ", " << jacobian_output.str()
         << ", " << hessian_output.str()
         << ", " << hessian1_output.str()
         << ", " << third_derivs_output.str()
         << ", " << third_derivs1_output.str()
         << "}";
}

void
DynamicModel::writeEquations() const
{
  cout << endl << endl;
  for (int i = 0; i < (int) equations.size(); i++)
    {
      cout << "EQUATION #" << i << ":" << endl;
      equations[i]->write();
      cout << endl << "--------------------------------------" << endl;
    }
}
