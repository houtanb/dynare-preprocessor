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

#ifndef _COMPUTINGTASKS_HH
#define _COMPUTINGTASKS_HH

#include <ostream>

#include "SymbolList.hh"
#include "SymbolTable.hh"
#include "Statement.hh"
#include "StaticModel.hh"
#include "DynamicModel.hh"
#include "SteadyStateModel.hh"

class SteadyStatement : public Statement
{
private:
  const OptionsList options_list;
public:
  SteadyStatement(const OptionsList &options_list_arg);
  virtual void checkPass(ModFileStructure &mod_file_struct, WarningConsolidation &warnings);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeJsonOutput(ostream &output) const;
};

class CheckStatement : public Statement
{
private:
  const OptionsList options_list;
public:
  CheckStatement(const OptionsList &options_list_arg);
  virtual void checkPass(ModFileStructure &mod_file_struct, WarningConsolidation &warnings);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeJsonOutput(ostream &output) const;
};

class SimulStatement : public Statement
{
private:
  const OptionsList options_list;
public:
  SimulStatement(const OptionsList &options_list_arg);
  virtual void checkPass(ModFileStructure &mod_file_struct, WarningConsolidation &warnings);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeJsonOutput(ostream &output) const;
};

class PerfectForesightSetupStatement : public Statement
{
private:
  const OptionsList options_list;
public:
  PerfectForesightSetupStatement(const OptionsList &options_list_arg);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeJsonOutput(ostream &output) const;
};

class PerfectForesightSolverStatement : public Statement
{
private:
  const OptionsList options_list;
public:
  PerfectForesightSolverStatement(const OptionsList &options_list_arg);
  virtual void checkPass(ModFileStructure &mod_file_struct, WarningConsolidation &warnings);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeJsonOutput(ostream &output) const;
};

class PriorPosteriorFunctionStatement : public Statement
{
private:
  const bool prior_func;
  const OptionsList options_list;
public:
  PriorPosteriorFunctionStatement(const bool prior_func_arg, const OptionsList &options_list_arg);
  virtual void checkPass(ModFileStructure &mod_file_struct, WarningConsolidation &warnings);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeJsonOutput(ostream &output) const;
};

class ModelInfoStatement : public Statement
{
private:
  const OptionsList options_list;
public:
  ModelInfoStatement(const OptionsList &options_list_arg);
  virtual void checkPass(ModFileStructure &mod_file_struct, WarningConsolidation &warnings);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeJsonOutput(ostream &output) const;
};

class StochSimulStatement : public Statement
{
private:
  const SymbolList symbol_list;
  const OptionsList options_list;
public:
  StochSimulStatement(const SymbolList &symbol_list_arg,
                      const OptionsList &options_list_arg);
  virtual void checkPass(ModFileStructure &mod_file_struct, WarningConsolidation &warnings);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeJsonOutput(ostream &output) const;
};

class PacModelStatement : public Statement
{
private:
  const string &name;
  const string &var_name;
  const string &discount;
  const string &growth;
  const map<string, int> undiff;
  const SymbolTable &symbol_table;
  vector<int> lhs;
public:
  PacModelStatement(const string &name_arg,
                    const string &var_name_arg,
                    const string &discount_arg,
                    const string &growth_arg,
                    const map<string, int> &undiff_arg,
                    const SymbolTable &symbol_table_arg);
  virtual void checkPass(ModFileStructure &mod_file_struct, WarningConsolidation &warnings);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeJsonOutput(ostream &output) const;
  void fillUndiffedLHS(vector<int> &lhs);
  void getPacModelInfoForPacExpectation(pair<string, pair<string, pair<string, pair<int, map<string, int> > > > > &pac_model_info) const;
};

class VarModelStatement : public Statement
{
private:
  const SymbolList symbol_list;
  const OptionsList options_list;
  const string &name;
  const SymbolTable &symbol_table;
  vector<int> eqnumber, lhs, orig_diff_var;
  vector<set<pair<int, int> > > rhs_by_eq; // rhs by equation
  vector<bool> nonstationary, diff;
  int max_lag;
public:
  VarModelStatement(const SymbolList &symbol_list_arg,
                    const OptionsList &options_list_arg,
                    const string &name_arg,
                    const SymbolTable &symbol_table_arg);
  void getVarModelInfo(string &var_model_name,
                       map<string, pair<SymbolList, int> > &var_model_info,
                       map<string, vector<string> > &var_model_eqtags) const;
  void fillVarModelInfoFromEquations(vector<int> &eqnumber_arg, vector<int> &lhs_arg,
                                     vector<set<pair<int, int> > > &rhs_arg,
                                     vector<bool> &nonstationary_arg,
                                     vector<bool> &diff_arg, vector<int> &orig_diff_var_arg,
                                     int max_lag_arg);
  virtual void checkPass(ModFileStructure &mod_file_struct, WarningConsolidation &warnings);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  void createVarModelMFunction(ostream &output, const map<string, set<int> > &var_expectation_functions_to_write) const;
};

class VarRestrictionsStatement : public Statement
{
private:
  typedef pair<pair<int, pair<int, int> >, expr_t> var_restriction_eq_crosseq_t;
  const string &var_model_name;
  const map<string, vector<string> > &var_map;
  const map<int, map<int, SymbolList> > exclusion_restrictions;
  typedef map<int, pair<pair<var_restriction_eq_crosseq_t, var_restriction_eq_crosseq_t>, double> > equation_restrictions_t;
  const equation_restrictions_t equation_restrictions;
  typedef vector<pair<pair<var_restriction_eq_crosseq_t, var_restriction_eq_crosseq_t>, double> > crossequation_restrictions_t;
  const crossequation_restrictions_t crossequation_restrictions;
  const map<pair<int, int>, double> covariance_number_restriction;
  const map<pair<int, int>, pair<int, int> > covariance_pair_restriction;
  const SymbolTable &symbol_table;
  int findIdxInVector(const vector<string> &vecvars, const string &var) const;
public:
  VarRestrictionsStatement(const string &var_model_name_arg,
                           const map<string, vector<string> > &var_map_arg,
                           const map<int, map<int, SymbolList> > &exclusion_restrictions_arg,
                           const equation_restrictions_t &equation_restrictions_arg,
                           const crossequation_restrictions_t &crossequation_restrictions_arg,
                           const map<pair<int, int>, double> &covariance_number_restriction_arg,
                           const map<pair<int, int>, pair<int, int> > &covariance_pair_restriction_arg,
                           const SymbolTable &symbol_table_arg);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
};

class VarEstimationStatement : public Statement
{
private:
  const OptionsList options_list;
public:
  VarEstimationStatement(const OptionsList &options_list_arg);
  virtual void checkPass(ModFileStructure &mod_file_struct, WarningConsolidation &warnings);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
};

class ForecastStatement : public Statement
{
private:
  const SymbolList symbol_list;
  const OptionsList options_list;
public:
  ForecastStatement(const SymbolList &symbol_list_arg,
                    const OptionsList &options_list_arg);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeJsonOutput(ostream &output) const;
};

class RamseyModelStatement : public Statement
{
private:
  const OptionsList options_list;
public:
  RamseyModelStatement(const OptionsList &options_list_arg);
  virtual void checkPass(ModFileStructure &mod_file_struct, WarningConsolidation &warnings);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeJsonOutput(ostream &output) const;
};

class RamseyConstraintsStatement : public Statement
{
public:
  struct Constraint
  {
    int endo;
    BinaryOpcode code;
    expr_t expression;
  };
  typedef vector<Constraint> constraints_t;
private:
  const SymbolTable &symbol_table;
  const constraints_t constraints;
public:
  RamseyConstraintsStatement(const SymbolTable &symbol_table_arg, const constraints_t &constraints_arg);
  virtual void checkPass(ModFileStructure &mod_file_struct, WarningConsolidation &warnings);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeJsonOutput(ostream &output) const;
};

class RamseyPolicyStatement : public Statement
{
private:
  const SymbolTable &symbol_table;
  const vector<string> ramsey_policy_list;
  const OptionsList options_list;
public:
  RamseyPolicyStatement(const SymbolTable &symbol_table_arg,
                        const vector<string> &ramsey_policy_list_arg,
                        const OptionsList &options_list_arg);
  virtual void checkPass(ModFileStructure &mod_file_struct, WarningConsolidation &warnings);
  void checkRamseyPolicyList();
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeJsonOutput(ostream &output) const;
};

class DiscretionaryPolicyStatement : public Statement
{
private:
  const SymbolList symbol_list;
  const OptionsList options_list;
public:
  DiscretionaryPolicyStatement(const SymbolList &symbol_list_arg,
                               const OptionsList &options_list_arg);
  virtual void checkPass(ModFileStructure &mod_file_struct, WarningConsolidation &warnings);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeJsonOutput(ostream &output) const;
};

class RplotStatement : public Statement
{
private:
  const SymbolList symbol_list;
public:
  RplotStatement(const SymbolList &symbol_list_arg);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeJsonOutput(ostream &output) const;
};

class UnitRootVarsStatement : public Statement
{
public:
  UnitRootVarsStatement(void);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeJsonOutput(ostream &output) const;
};

class PeriodsStatement : public Statement
{
private:
  const int periods;
public:
  PeriodsStatement(int periods_arg);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeJsonOutput(ostream &output) const;
};

class DsampleStatement : public Statement
{
private:
  const int val1, val2;
public:
  DsampleStatement(int val1_arg);
  DsampleStatement(int val1_arg, int val2_arg);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeJsonOutput(ostream &output) const;
};

class EstimationStatement : public Statement
{
private:
  const SymbolList symbol_list;
  const OptionsList options_list;
public:
  EstimationStatement(const SymbolList &symbol_list_arg,
                      const OptionsList &options_list_arg);
  virtual void checkPass(ModFileStructure &mod_file_struct, WarningConsolidation &warnings);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeJsonOutput(ostream &output) const;
};

class DynareSensitivityStatement : public Statement
{
private:
  const OptionsList options_list;
public:
  DynareSensitivityStatement(const OptionsList &options_list_arg);
  virtual void checkPass(ModFileStructure &mod_file_struct, WarningConsolidation &warnings);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeJsonOutput(ostream &output) const;
};

class ObservationTrendsStatement : public Statement
{
public:
  typedef map<string, expr_t> trend_elements_t;
private:
  const trend_elements_t trend_elements;
  const SymbolTable &symbol_table;
public:
  ObservationTrendsStatement(const trend_elements_t &trend_elements_arg,
                             const SymbolTable &symbol_table_arg);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeJsonOutput(ostream &output) const;
};

class OsrParamsStatement : public Statement
{
private:
  const SymbolList symbol_list;
  const SymbolTable &symbol_table;
public:
  OsrParamsStatement(const SymbolList &symbol_list_arg, const SymbolTable &symbol_table_arg);
  virtual void checkPass(ModFileStructure &mod_file_struct, WarningConsolidation &warnings);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeJsonOutput(ostream &output) const;
};

class OsrStatement : public Statement
{
private:
  const SymbolList symbol_list;
  const OptionsList options_list;
public:
  OsrStatement(const SymbolList &symbol_list_arg,
               const OptionsList &options_list_arg);
  virtual void checkPass(ModFileStructure &mod_file_struct, WarningConsolidation &warnings);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeJsonOutput(ostream &output) const;
};

//! Temporary structure used when parsing estimation_params* statements
class OsrParams
{
public:
  string name;
  expr_t low_bound, up_bound;

  void
  init(const DataTree &datatree)
  {
    name = "";
    low_bound = datatree.MinusInfinity;
    up_bound = datatree.Infinity;
  }
};

class OsrParamsBoundsStatement : public Statement
{
private:
  const vector<OsrParams> osr_params_list;
public:
  OsrParamsBoundsStatement(const vector<OsrParams> &osr_params_list_arg);
  virtual void checkPass(ModFileStructure &mod_file_struct, WarningConsolidation &warnings);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeJsonOutput(ostream &output) const;
};

class DynaTypeStatement : public Statement
{
private:
  const SymbolList symbol_list;
  const string filename;
public:
  DynaTypeStatement(const SymbolList &symbol_list_arg,
                    const string &filename_arg);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeJsonOutput(ostream &output) const;
};

class DynaSaveStatement : public Statement
{
private:
  const SymbolList symbol_list;
  const string filename;
public:
  DynaSaveStatement(const SymbolList &symbol_list_arg,
                    const string &filename_arg);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeJsonOutput(ostream &output) const;
};

class ModelComparisonStatement : public Statement
{
public:
  typedef vector<pair<string, string> > filename_list_t;
private:
  filename_list_t filename_list;
  OptionsList options_list;
public:
  ModelComparisonStatement(const filename_list_t &filename_list_arg,
                           const OptionsList &options_list_arg);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeJsonOutput(ostream &output) const;
};

//! Temporary structure used when parsing estimation_params* statements
class EstimationParams
{
public:
  int type;
  string name, name2;
  PriorDistributions prior;
  expr_t init_val, low_bound, up_bound, mean, std, p3, p4, jscale;

  void
  init(const DataTree &datatree)
  {
    type = 0;
    name = "";
    name2 = "";
    prior = eNoShape;
    init_val = datatree.NaN;
    low_bound = datatree.MinusInfinity;
    up_bound = datatree.Infinity;
    mean = datatree.NaN;
    std = datatree.NaN;
    p3 = datatree.NaN;
    p4 = datatree.NaN;
    jscale = datatree.NaN;
  }
};

class EstimatedParamsStatement : public Statement
{
private:
  const vector<EstimationParams> estim_params_list;
  const SymbolTable &symbol_table;
public:
  EstimatedParamsStatement(const vector<EstimationParams> &estim_params_list_arg,
                           const SymbolTable &symbol_table_arg);
  virtual void checkPass(ModFileStructure &mod_file_struct, WarningConsolidation &warnings);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeJsonOutput(ostream &output) const;
};

class EstimatedParamsInitStatement : public Statement
{
private:
  const vector<EstimationParams> estim_params_list;
  const SymbolTable &symbol_table;
  const bool use_calibration;
public:
  EstimatedParamsInitStatement(const vector<EstimationParams> &estim_params_list_arg,
                               const SymbolTable &symbol_table_arg,
                               const bool use_calibration_arg);
  virtual void checkPass(ModFileStructure &mod_file_struct, WarningConsolidation &warnings);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeJsonOutput(ostream &output) const;
};

class EstimatedParamsBoundsStatement : public Statement
{
private:
  const vector<EstimationParams> estim_params_list;
  const SymbolTable &symbol_table;
public:
  EstimatedParamsBoundsStatement(const vector<EstimationParams> &estim_params_list_arg,
                                 const SymbolTable &symbol_table_arg);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeJsonOutput(ostream &output) const;
};

class OptimWeightsStatement : public Statement
{
public:
  typedef map<string, expr_t> var_weights_t;
  typedef map<pair<string, string>, expr_t> covar_weights_t;
private:
  const var_weights_t var_weights;
  const covar_weights_t covar_weights;
  const SymbolTable &symbol_table;
public:
  OptimWeightsStatement(const var_weights_t &var_weights_arg,
                        const covar_weights_t &covar_weights_arg,
                        const SymbolTable &symbol_table_arg);
  virtual void checkPass(ModFileStructure &mod_file_struct, WarningConsolidation &warnings);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeJsonOutput(ostream &output) const;
};

/*! \todo Make model_tree a member instead of a pointer */
class PlannerObjectiveStatement : public Statement
{
private:
  StaticModel *model_tree;
  bool computing_pass_called;
public:
  //! Constructor
  /*! \param model_tree_arg the model tree used to store the objective function.
    It is owned by the PlannerObjectiveStatement, and will be deleted by its destructor */
  PlannerObjectiveStatement(StaticModel *model_tree_arg);
  virtual
  ~PlannerObjectiveStatement();
  /*! \todo check there are only endogenous variables at the current period in the objective
    (no exogenous, no lead/lag) */
  virtual void checkPass(ModFileStructure &mod_file_struct, WarningConsolidation &warnings);
  /*! \todo allow for the possibility of disabling temporary terms */
  virtual void computingPass();
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeJsonOutput(ostream &output) const;
  //! Return the Planner Objective
  StaticModel *getPlannerObjective() const;
};

class BVARDensityStatement : public Statement
{
private:
  const int maxnlags;
  const OptionsList options_list;
public:
  BVARDensityStatement(int maxnlags_arg, const OptionsList &options_list_arg);
  virtual void checkPass(ModFileStructure &mod_file_struct, WarningConsolidation &warnings);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeJsonOutput(ostream &output) const;
};

class BVARForecastStatement : public Statement
{
private:
  const int nlags;
  const OptionsList options_list;
public:
  BVARForecastStatement(int nlags_arg, const OptionsList &options_list_arg);
  virtual void checkPass(ModFileStructure &mod_file_struct, WarningConsolidation &warnings);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeJsonOutput(ostream &output) const;
};

class SBVARStatement : public Statement
{
private:
  const OptionsList options_list;
public:
  SBVARStatement(const OptionsList &options_list_arg);
  virtual void checkPass(ModFileStructure &mod_file_struct, WarningConsolidation &warnings);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeJsonOutput(ostream &output) const;
};

class MSSBVAREstimationStatement : public Statement
{
private:
  const OptionsList options_list;
public:
  MSSBVAREstimationStatement(const OptionsList &options_list_arg);
  virtual void checkPass(ModFileStructure &mod_file_struct, WarningConsolidation &warnings);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeJsonOutput(ostream &output) const;
};

class MSSBVARSimulationStatement : public Statement
{
private:
  const OptionsList options_list;
public:
  MSSBVARSimulationStatement(const OptionsList &options_list_arg);
  virtual void checkPass(ModFileStructure &mod_file_struct, WarningConsolidation &warnings);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeJsonOutput(ostream &output) const;
};

class MSSBVARComputeMDDStatement : public Statement
{
private:
  const OptionsList options_list;
public:
  MSSBVARComputeMDDStatement(const OptionsList &options_list_arg);
  virtual void checkPass(ModFileStructure &mod_file_struct, WarningConsolidation &warnings);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeJsonOutput(ostream &output) const;
};

class MSSBVARComputeProbabilitiesStatement : public Statement
{
private:
  const OptionsList options_list;
public:
  MSSBVARComputeProbabilitiesStatement(const OptionsList &options_list_arg);
  virtual void checkPass(ModFileStructure &mod_file_struct, WarningConsolidation &warnings);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeJsonOutput(ostream &output) const;
};

class MSSBVARIrfStatement : public Statement
{
private:
  const SymbolList symbol_list;
  const OptionsList options_list;
public:
  MSSBVARIrfStatement(const SymbolList &symbol_list_arg,
                      const OptionsList &options_list_arg);
  virtual void checkPass(ModFileStructure &mod_file_struct, WarningConsolidation &warnings);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeJsonOutput(ostream &output) const;
};

class MSSBVARForecastStatement : public Statement
{
private:
  const OptionsList options_list;
public:
  MSSBVARForecastStatement(const OptionsList &options_list_arg);
  virtual void checkPass(ModFileStructure &mod_file_struct, WarningConsolidation &warnings);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeJsonOutput(ostream &output) const;
};

class MSSBVARVarianceDecompositionStatement : public Statement
{
private:
  const OptionsList options_list;
public:
  MSSBVARVarianceDecompositionStatement(const OptionsList &options_list_arg);
  virtual void checkPass(ModFileStructure &mod_file_struct, WarningConsolidation &warnings);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeJsonOutput(ostream &output) const;
};

class IdentificationStatement : public Statement
{
private:
  OptionsList options_list;
public:
  IdentificationStatement(const OptionsList &options_list_arg);
  virtual void checkPass(ModFileStructure &mod_file_struct, WarningConsolidation &warnings);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeJsonOutput(ostream &output) const;
};

class WriteLatexDynamicModelStatement : public Statement
{
private:
  const DynamicModel &dynamic_model;
  const bool write_equation_tags;
public:
  WriteLatexDynamicModelStatement(const DynamicModel &dynamic_model_arg, bool write_equation_tags_arg);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeJsonOutput(ostream &output) const;
};

class WriteLatexStaticModelStatement : public Statement
{
private:
  const StaticModel &static_model;
  const bool write_equation_tags;
public:
  WriteLatexStaticModelStatement(const StaticModel &static_model_arg, bool write_equation_tags_arg);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeJsonOutput(ostream &output) const;
};

class WriteLatexOriginalModelStatement : public Statement
{
private:
  const DynamicModel &original_model;
  const bool write_equation_tags;
public:
  WriteLatexOriginalModelStatement(const DynamicModel &original_model_arg, bool write_equation_tags_arg);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeJsonOutput(ostream &output) const;
};

class WriteLatexSteadyStateModelStatement : public Statement
{
private:
  const SteadyStateModel &steady_state_model;
public:
  WriteLatexSteadyStateModelStatement(const SteadyStateModel &steady_state_model_arg);
  virtual void checkPass(ModFileStructure &mod_file_struct, WarningConsolidation &warnings);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeJsonOutput(ostream &output) const;
};

class ShockDecompositionStatement : public Statement
{
private:
  const SymbolList symbol_list;
  const OptionsList options_list;
public:
  ShockDecompositionStatement(const SymbolList &symbol_list_arg,
                              const OptionsList &options_list_arg);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeJsonOutput(ostream &output) const;
};

class RealtimeShockDecompositionStatement : public Statement
{
private:
  const SymbolList symbol_list;
  const OptionsList options_list;
public:
  RealtimeShockDecompositionStatement(const SymbolList &symbol_list_arg,
                                      const OptionsList &options_list_arg);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
};

class PlotShockDecompositionStatement : public Statement
{
private:
  const SymbolList symbol_list;
  const OptionsList options_list;
public:
  PlotShockDecompositionStatement(const SymbolList &symbol_list_arg,
                                  const OptionsList &options_list_arg);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
};

class InitialConditionDecompositionStatement : public Statement
{
private:
  const SymbolList symbol_list;
  const OptionsList options_list;
public:
  InitialConditionDecompositionStatement(const SymbolList &symbol_list_arg,
                                         const OptionsList &options_list_arg);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
};

class ConditionalForecastStatement : public Statement
{
private:
  const OptionsList options_list;
public:
  ConditionalForecastStatement(const OptionsList &options_list_arg);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeJsonOutput(ostream &output) const;
};

class PlotConditionalForecastStatement : public Statement
{
private:
  //! A value of -1 indicates that the user didn't specify a value
  const int periods;
  const SymbolList symbol_list;
public:
  PlotConditionalForecastStatement(int periods_arg, const SymbolList &symbol_list_arg);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeJsonOutput(ostream &output) const;
};

class CalibSmootherStatement : public Statement
{
private:
  const SymbolList symbol_list;
  const OptionsList options_list;
public:
  CalibSmootherStatement(const SymbolList &symbol_list_arg,
                         const OptionsList &options_list_arg);
  virtual void checkPass(ModFileStructure &mod_file_struct, WarningConsolidation &warnings);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeJsonOutput(ostream &output) const;
};

class ExtendedPathStatement : public Statement
{
private:
  const OptionsList options_list;
public:
  ExtendedPathStatement(const OptionsList &options_list_arg);
  virtual void checkPass(ModFileStructure &mod_file_struct, WarningConsolidation &warnings);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeJsonOutput(ostream &output) const;
};

class SvarIdentificationStatement : public Statement
{
public:
  //  typedef map<pair<int, int>, vector<int> > svar_identification_exclusion_t;
  struct svar_identification_restriction
  {
    int equation;
    int restriction_nbr;
    int lag;
    int variable;
    expr_t value;
  };

  typedef vector< svar_identification_restriction > svar_identification_restrictions_t;
private:
  const svar_identification_restrictions_t restrictions;
  const bool upper_cholesky_present;
  const bool lower_cholesky_present;
  const bool constants_exclusion_present;
  const SymbolTable &symbol_table;
  int getMaxLag() const;
public:
  SvarIdentificationStatement(const svar_identification_restrictions_t &restrictions_arg,
                              const bool &upper_cholesky_present_arg,
                              const bool &lower_cholesky_present_arg,
                              const bool &constants_exclusion_present_arg,
                              const SymbolTable &symbol_table_arg);
  virtual void checkPass(ModFileStructure &mod_file_struct, WarningConsolidation &warnings);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeJsonOutput(ostream &output) const;
};

class MarkovSwitchingStatement : public Statement
{
private:
  const OptionsList options_list;
  map <pair<int, int >, double > restriction_map;
public:
  MarkovSwitchingStatement(const OptionsList &options_list_arg);
  virtual void checkPass(ModFileStructure &mod_file_struct, WarningConsolidation &warnings);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeCOutput(ostream &output, const string &basename);
  virtual void writeJsonOutput(ostream &output) const;
};

class SvarStatement : public Statement
{
private:
  const OptionsList options_list;
public:
  SvarStatement(const OptionsList &options_list_arg);
  virtual void checkPass(ModFileStructure &mod_file_struct, WarningConsolidation &warnings);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeJsonOutput(ostream &output) const;
};

class SvarGlobalIdentificationCheckStatement : public Statement
{
public:
  SvarGlobalIdentificationCheckStatement();
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeJsonOutput(ostream &output) const;
};

class SetTimeStatement : public Statement
{
private:
  const OptionsList options_list;
public:
  SetTimeStatement(const OptionsList &options_list_arg);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeJsonOutput(ostream &output) const;
};

class EstimationDataStatement : public Statement
{
private:
  const OptionsList options_list;
public:
  EstimationDataStatement(const OptionsList &options_list_arg);
  virtual void checkPass(ModFileStructure &mod_file_struct, WarningConsolidation &warnings);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeJsonOutput(ostream &output) const;
};

class SubsamplesStatement : public Statement
{
public:
  //! Storage for declaring subsamples: map<subsample_name, <date1, date2 >
  typedef map<string, pair<string, string> > subsample_declaration_map_t;
private:
  const string name1;
  const string name2;
  const subsample_declaration_map_t subsample_declaration_map;
  const SymbolTable symbol_table;
public:
  SubsamplesStatement(const string &name1_arg,
                      const string &name2_arg,
                      const subsample_declaration_map_t subsample_declaration_map_arg,
                      const SymbolTable &symbol_table_arg);
  virtual void checkPass(ModFileStructure &mod_file_struct, WarningConsolidation &warnings);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeJsonOutput(ostream &output) const;
};

class SubsamplesEqualStatement : public Statement
{
private:
  const string to_name1;
  const string to_name2;
  const string from_name1;
  const string from_name2;
  const SymbolTable symbol_table;
public:
  SubsamplesEqualStatement(const string &to_name1_arg,
                           const string &to_name2_arg,
                           const string &from_name1_arg,
                           const string &from_name2_arg,
                           const SymbolTable &symbol_table_arg);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeJsonOutput(ostream &output) const;
};

class JointPriorStatement : public Statement
{
private:
  const vector<string> joint_parameters;
  const PriorDistributions prior_shape;
  const OptionsList options_list;
public:
  JointPriorStatement(const vector<string> joint_parameters_arg,
                      const PriorDistributions &prior_shape_arg,
                      const OptionsList &options_list_arg);
  virtual void checkPass(ModFileStructure &mod_file_struct, WarningConsolidation &warnings);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  void writeOutputHelper(ostream &output, const string &field, const string &lhs_field) const;
  virtual void writeJsonOutput(ostream &output) const;
};

class BasicPriorStatement : public Statement
{
public:
  virtual
  ~BasicPriorStatement();
protected:
  const string name;
  const string subsample_name;
  const PriorDistributions prior_shape;
  const expr_t variance;
  const OptionsList options_list;
  BasicPriorStatement(const string &name_arg,
                      const string &subsample_name_arg,
                      const PriorDistributions &prior_shape_arg,
                      const expr_t &variance_arg,
                      const OptionsList &options_list_arg);
  virtual void checkPass(ModFileStructure &mod_file_struct, WarningConsolidation &warnings);
  void get_base_name(const SymbolType symb_type, string &lhs_field) const;
  void writeCommonOutput(ostream &output, const string &lhs_field) const;
  void writeCommonOutputHelper(ostream &output, const string &field, const string &lhs_field) const;
  void writePriorOutput(ostream &output, string &lhs_field, const string &name2) const;
  bool is_structural_innovation(const SymbolType symb_type) const;
  void writePriorIndex(ostream &output, const string &lhs_field) const;
  void writeVarianceOption(ostream &output, const string &lhs_field) const;
  void writeOutputHelper(ostream &output, const string &field, const string &lhs_field) const;
  void writeShape(ostream &output, const string &lhs_field) const;
  void writeCOutputHelper(ostream &output, const string &field) const;
  void writeCShape(ostream &output) const;
  void writeCVarianceOption(ostream &output) const;
  void writeCDomain(ostream &output) const;
  void writeJsonShape(ostream &output) const;
  void writeJsonPriorOutput(ostream &output) const;
};

class PriorStatement : public BasicPriorStatement
{
public:
  PriorStatement(const string &name_arg,
                 const string &subsample_name_arg,
                 const PriorDistributions &prior_shape_arg,
                 const expr_t &variance_arg,
                 const OptionsList &options_list_arg);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeCOutput(ostream &output, const string &basename);
  virtual void writeJsonOutput(ostream &output) const;
};

class StdPriorStatement : public BasicPriorStatement
{
private:
  const SymbolTable symbol_table;
public:
  StdPriorStatement(const string &name_arg,
                    const string &subsample_name_arg,
                    const PriorDistributions &prior_shape_arg,
                    const expr_t &variance_arg,
                    const OptionsList &options_list_arg,
                    const SymbolTable &symbol_table_arg);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeCOutput(ostream &output, const string &basename);
  virtual void writeJsonOutput(ostream &output) const;
};

class CorrPriorStatement : public BasicPriorStatement
{
private:
  const string name1;
  const SymbolTable symbol_table;
public:
  CorrPriorStatement(const string &name_arg1,
                     const string &name_arg2,
                     const string &subsample_name_arg,
                     const PriorDistributions &prior_shape_arg,
                     const expr_t &variance_arg,
                     const OptionsList &options_list_arg,
                     const SymbolTable &symbol_table_arg);
  virtual void checkPass(ModFileStructure &mod_file_struct, WarningConsolidation &warnings);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeCOutput(ostream &output, const string &basename);
  virtual void writeJsonOutput(ostream &output) const;
};

class PriorEqualStatement : public Statement
{
private:
  const string to_declaration_type;
  const string to_name1;
  const string to_name2;
  const string to_subsample_name;
  const string from_declaration_type;
  const string from_name1;
  const string from_name2;
  const string from_subsample_name;
  const SymbolTable symbol_table;
public:
  PriorEqualStatement(const string &to_declaration_type_arg,
                      const string &to_name1_arg,
                      const string &to_name2_arg,
                      const string &to_subsample_name_arg,
                      const string &from_declaration_type_arg,
                      const string &from_name1_arg,
                      const string &from_name2_arg,
                      const string &from_subsample_name_arg,
                      const SymbolTable &symbol_table_arg);
  void get_base_name(const SymbolType symb_type, string &lhs_field) const;
  virtual void checkPass(ModFileStructure &mod_file_struct, WarningConsolidation &warnings);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeJsonOutput(ostream &output) const;
};

class BasicOptionsStatement : public Statement
{
public:
  virtual
  ~BasicOptionsStatement();
protected:
  const string name;
  const string subsample_name;
  const OptionsList options_list;
  BasicOptionsStatement(const string &name_arg,
                        const string &subsample_name_arg,
                        const OptionsList &options_list_arg);
  void get_base_name(const SymbolType symb_type, string &lhs_field) const;
  virtual void checkPass(ModFileStructure &mod_file_struct, WarningConsolidation &warnings);
  void writeOptionsOutput(ostream &output, string &lhs_field, const string &name2) const;
  void writeCommonOutput(ostream &output, const string &lhs_field) const;
  void writeCommonOutputHelper(ostream &output, const string &field, const string &lhs_field) const;
  bool is_structural_innovation(const SymbolType symb_type) const;
  void writeOptionsIndex(ostream &output, const string &lhs_field) const;
  void writeOutputHelper(ostream &output, const string &field, const string &lhs_field) const;
  void writeCOutputHelper(ostream &output, const string &field) const;
  void writeJsonOptionsOutput(ostream &output) const;
};

class OptionsStatement : public BasicOptionsStatement
{
public:
  OptionsStatement(const string &name_arg, const string &subsample_name_arg, const OptionsList &options_list_arg);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeCOutput(ostream &output, const string &basename);
  virtual void writeJsonOutput(ostream &output) const;
};

class StdOptionsStatement : public BasicOptionsStatement
{
private:
  const SymbolTable symbol_table;
public:
  StdOptionsStatement(const string &name_arg,
                      const string &subsample_name_arg,
                      const OptionsList &options_list_arg,
                      const SymbolTable &symbol_table_arg);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeCOutput(ostream &output, const string &basename);
  virtual void writeJsonOutput(ostream &output) const;
};

class CorrOptionsStatement : public BasicOptionsStatement
{
private:
  const string name1;
  const SymbolTable symbol_table;
public:
  CorrOptionsStatement(const string &name_arg1, const string &name_arg2,
                       const string &subsample_name_arg,
                       const OptionsList &options_list_arg,
                       const SymbolTable &symbol_table_arg);
  virtual void checkPass(ModFileStructure &mod_file_struct, WarningConsolidation &warnings);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeCOutput(ostream &output, const string &basename);
  virtual void writeJsonOutput(ostream &output) const;
};

class OptionsEqualStatement : public Statement
{
private:
  const string to_declaration_type;
  const string to_name1;
  const string to_name2;
  const string to_subsample_name;
  const string from_declaration_type;
  const string from_name1;
  const string from_name2;
  const string from_subsample_name;
  const SymbolTable symbol_table;
public:
  OptionsEqualStatement(const string &to_declaration_type_arg,
                        const string &to_name1_arg,
                        const string &to_name2_arg,
                        const string &to_subsample_name_arg,
                        const string &from_declaration_type_arg,
                        const string &from_name1_arg,
                        const string &from_name2_arg,
                        const string &from_subsample_name_arg,
                        const SymbolTable &symbol_table_arg);
  void get_base_name(const SymbolType symb_type, string &lhs_field) const;
  virtual void checkPass(ModFileStructure &mod_file_struct, WarningConsolidation &warnings);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeJsonOutput(ostream &output) const;
};

class ModelDiagnosticsStatement : public Statement
{
public:
  ModelDiagnosticsStatement();
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeJsonOutput(ostream &output) const;
};

class Smoother2histvalStatement : public Statement
{
private:
  const OptionsList options_list;
public:
  Smoother2histvalStatement(const OptionsList &options_list_arg);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeJsonOutput(ostream &output) const;
};

class GMMEstimationStatement : public Statement
{
private:
  const SymbolList symbol_list;
  const OptionsList options_list;
public:
  GMMEstimationStatement(const SymbolList &symbol_list_arg, const OptionsList &options_list_arg);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeJsonOutput(ostream &output) const;
};

class SMMEstimationStatement : public Statement
{
private:
  const SymbolList symbol_list;
  const OptionsList options_list;
public:
  SMMEstimationStatement(const SymbolList &symbol_list_arg, const OptionsList &options_list_arg);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeJsonOutput(ostream &output) const;
};

class GenerateIRFsStatement : public Statement
{
public:
private:
  const OptionsList options_list;
  const vector<string> generate_irf_names;
  const vector<map<string, double> > generate_irf_elements;
public:
  GenerateIRFsStatement(const OptionsList &options_list_arg,
                        const vector<string> &generate_irf_names_arg,
                        const vector<map<string, double> > &generate_irf_elements_arg);
  virtual void writeOutput(ostream &output, const string &basename, bool minimal_workspace) const;
  virtual void writeJsonOutput(ostream &output) const;
};

#endif