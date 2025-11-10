/*
 * This file is part of PowerDNS or dnsdist.
 * Copyright -- PowerDNS.COM B.V. and its contributors
 */
#pragma once
#include <map>
#include <set>
#include <string>
#include <vector>
#include <fstream>
#include <iostream>

#include "misc.hh"
#include "pdnsexception.hh"

#ifndef _WIN32
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#endif

#include "namespaces.hh"
#include "logging.hh"

using ArgException = PDNSException;

class ArgvMap
{
public:
  ArgvMap();
  void parse(int& argc, char** argv, bool lax = false);
  void laxParse(int& argc, char** argv)
  {
    parse(argc, argv, true);
  }
  void preParse(int& argc, char** argv, const string& arg);
  bool preParseFile(const string& fname, const string& arg, const string& theDefault = "");

  bool file(const string& fname, bool lax = false);
  bool file(const string& fname, bool lax, bool included);
  bool laxFile(const string& fname)
  {
    return file(fname, true);
  }
  bool parseFile(const string& fname, const string& arg, bool lax);
  bool parmIsset(const string& var);
  bool mustDo(const string& var);
  int asNum(const string& arg, int def = 0);
  mode_t asMode(const string& arg);
  uid_t asUid(const string& arg);
  gid_t asGid(const string& arg);
  double asDouble(const string& arg);
  string& set(const string&);
  string& set(const string&, const string&);
  void setCmd(const string&, const string&);
  string& setSwitch(const string&, const string&);
  string helpstring(string prefix = "");
  string configstring(bool running, bool full);
  bool contains(const string& var, const string& val);
  bool isEmpty(const string& arg);
  void setDefault(const string& var, const string& value);
  void setDefaults();

  using param_t = map<string, string>;
  param_t::const_iterator begin();
  param_t::const_iterator end();
  const string& operator[](const string&);
  const vector<string>& getCommands();
  void gatherIncludes(const std::string& dir, const std::string& suffix, std::vector<std::string>& extraConfigs);
  void warnIfDeprecated(const string& var) const;
  [[nodiscard]] static string isDeprecated(const string& var);
#ifdef RECURSOR
  void setSLog(Logr::log_t log)
  {
    d_log = log;
  }
#endif
private:
  void parseOne(const string& arg, const string& parseOnly = "", bool lax = false);
  static string formatOne(bool running, bool full, const string& var, const string& help, const string& theDefault, const string& current);
  map<string, string> d_params;
  map<string, string> d_unknownParams;
  map<string, string> helpmap;
  map<string, string> defaultmap;
  map<string, string> d_typeMap;
  vector<string> d_cmds;
  std::set<string> d_cleared;
#ifdef RECURSOR
  std::shared_ptr<Logr::Logger> d_log;
#endif
};

extern ArgvMap& arg();












