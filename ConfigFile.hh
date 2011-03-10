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

#ifndef _CONFIG_FILE_HH
#define _CONFIG_FILE_HH

#include <map>
#include <vector>

using namespace std;

typedef map<string, double *> member_nodes_t;


class SlaveNode
{
  friend class ConfigFile;
public:
  SlaveNode(string &computerName_arg, int minCpuNbr_arg, int maxCpuNbr_arg, string &userName_arg,
            string &password_arg, string &remoteDrive_arg, string &remoteDirectory_arg,
            string &dynarePath_arg, string &matlabOctavePath_arg, bool singleCompThread_arg,
            string &operatingSystem_arg);
  ~SlaveNode();

protected:
  const string computerName;
  int minCpuNbr;
  int maxCpuNbr;
  const string userName;
  const string password;
  const string remoteDrive;
  const string remoteDirectory;
  const string dynarePath;
  const string matlabOctavePath;
  const bool singleCompThread;
  const string operatingSystem;
};

class Cluster
{
  friend class ConfigFile;
public:
  Cluster(member_nodes_t member_nodes_arg);
  ~Cluster();

protected:
  const member_nodes_t member_nodes;
};

//! The abstract representation of a "config" file
class ConfigFile
{
public:
  ConfigFile(bool parallel_arg, bool parallel_test_arg, bool parallel_slave_open_mode_arg, const string &cluster_name);
  ~ConfigFile();

private:
  const bool parallel;
  const bool parallel_test;
  const bool parallel_slave_open_mode;
  const string cluster_name;
  string firstClusterName;
  //! Cluster Table
  map<string, Cluster *> clusters;
  //! Node Map
  map<string, SlaveNode *> slave_nodes;
  //! Add a SlaveNode or a Cluster object
  void addConfFileElement(bool inNode, bool inCluster, member_nodes_t member_nodes, string &name,
                          string &computerName, int minCpuNbr, int maxCpuNbr, string &userName,
                          string &password, string &remoteDrive, string &remoteDirectory,
                          string &dynarePath, string &matlabOctavePath, bool singleCompThread,
                          string &operatingSystem);
public:
  //! Parse config file
  void getConfigFileInfo(const string &parallel_config_file);
  //! Check Pass
  void checkPass() const;
  //! Check Pass
  void transformPass();
  //! Create options_.parallel structure, write options
  void writeCluster(ostream &output) const;
  //! Close slave nodes if needed
  void writeEndParallel(ostream &output) const;
};

#endif // ! CONFIG_FILE_HH
