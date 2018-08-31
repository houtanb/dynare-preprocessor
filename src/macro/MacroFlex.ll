/* -*- C++ -*- */
/*
 * Copyright (C) 2008-2018 Dynare Team
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

%{
using namespace std;

#include <fstream>
#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/tokenizer.hpp>

#include "MacroDriver.hh"

// Announce to Flex the prototype we want for lexing function
#define YY_DECL                                              \
  Macro::parser::token_type                                  \
  MacroFlex::lex(Macro::parser::semantic_type *yylval,       \
                 Macro::parser::location_type *yylloc,       \
                 MacroDriver &driver)

// Shortcut to access tokens defined by Bison
using token = Macro::parser::token;

/* By default yylex returns int, we use token_type.
   Unfortunately yyterminate by default returns 0, which is
   not of token_type.  */
#define yyterminate() return Macro::parser::token_type (0);
%}

%option c++

%option prefix="Macro"

%option case-insensitive noyywrap batch debug never-interactive

%x STMT
%x EXPR
%x FOR_BODY
%x END_FOR_COMP
%x FOR_COMP_STORAGE
%x THEN_BODY
%x ELSE_BODY

%{
// Increments location counter for every token read
#define YY_USER_ACTION yylloc->columns(yyleng);
%}

SPC  [ \t]+
EOL  (\r)?\n
CONT \\\\

%%
 /* Code put at the beginning of yylex() */
%{
  // Reset location before reading token
  yylloc->step();
%}

 // Ignore inline comments
<STMT>\/{2}.*

<INITIAL>^{SPC}*@#{SPC}*includepath{SPC}+\"([^\"\r\n:;|<>]*){1}(:[^\"\r\n:;|<>]*)*\"{SPC}*{EOL} {
                              yylloc->lines(1);
                              yylloc->step();

                              // Get path
                              string *includepath = new string(yytext);
                              int dblq_idx1 = includepath->find('"');
                              int dblq_idx2 = includepath->find('"', dblq_idx1 + 1);
                              includepath->erase(dblq_idx2);
                              includepath->erase(0, dblq_idx1 + 1);

                              push_path(includepath, yylloc, driver);
                              BEGIN(INITIAL);
                            }

<INITIAL>^{SPC}*@#{SPC}*includepath{SPC}+[^\"\r\n]*{SPC}*{EOL} {
                              yylloc->lines(1);
                              yylloc->step();

                              // Get variable name
                              string pathvar = string(yytext);
                              int dblq_idx1 = pathvar.find("includepath");
                              pathvar.erase(0, dblq_idx1 + 11);
                              pathvar.erase(0, pathvar.find_first_not_of(" \t"));
                              size_t p = pathvar.find_last_not_of(" \t\n\r");
                              if (string::npos != p)
                                pathvar.erase(p+1);

                              string *includepath = nullptr;
                              try
                              {
                                includepath = new string(driver.get_variable(pathvar)->toString());
                              }
                              catch(MacroDriver::UnknownVariable(&e))
                              {
                                driver.error(*yylloc, "Unknown variable: " + pathvar);
                              }
                              push_path(includepath, yylloc, driver);
                              BEGIN(INITIAL);
                            }

<INITIAL>^{SPC}*@#{SPC}*include{SPC}+\"[^\"\r\n]*\"{SPC}*{EOL} {
                              yylloc->lines(1);
                              yylloc->step();

                              // Get filename
                              string *filename = new string(yytext);
                              int dblq_idx1 = filename->find('"');
                              int dblq_idx2 = filename->find('"', dblq_idx1 + 1);
                              filename->erase(dblq_idx2);
                              filename->erase(0, dblq_idx1 + 1);

                              create_include_context(filename, yylloc, driver);

                              BEGIN(INITIAL);
                            }

<INITIAL>^{SPC}*@#{SPC}*include{SPC}+[^\"\r\n]*{SPC}*{EOL} {
                              yylloc->lines(1);
                              yylloc->step();

                              // Get variable name
                              string modvarname = string(yytext);
                              int dblq_idx1 = modvarname.find("include");
                              modvarname.erase(0, dblq_idx1 + 7);
                              modvarname.erase(0, modvarname.find_first_not_of(" \t"));
                              size_t p = modvarname.find_last_not_of(" \t\n\r");
                              if (string::npos != p)
                                modvarname.erase(p+1);

                              string *filename = nullptr;
                              try
                              {
                                filename = new string(driver.get_variable(modvarname)->toString());
                              }
                              catch(MacroDriver::UnknownVariable(&e))
                              {
                                driver.error(*yylloc, "Unknown variable: " + modvarname);
                              }
                              create_include_context(filename, yylloc, driver);
                              BEGIN(INITIAL);
                            }

<INITIAL>^{SPC}*@#          { yylloc->step(); BEGIN(STMT); }
<INITIAL>@\{                { yylloc->step(); BEGIN(EXPR); }

<EXPR>\}                    { BEGIN(INITIAL); return token::EOL; }

<STMT>{CONT}{SPC}*{EOL}     { yylloc->lines(1); yylloc->step(); }
<STMT>{EOL}                 {
                              /* If parsing a @#for or an @#if, keep the location
                                 for reporting message in case of error */
                              if (reading_for_statement)
                                for_stmt_loc_tmp = *yylloc;
                              else if (reading_if_statement)
                                if_stmt_loc_tmp = *yylloc;

                              yylloc->lines(1);
                              yylloc->step();
                              if (reading_for_statement)
                                {
                                  reading_for_statement = false;
                                  for_body_tmp.erase();
                                  for_body_loc_tmp = *yylloc;
                                  nested_for_nb = 0;
                                  BEGIN(FOR_BODY);
                                }
                              else if (reading_if_statement)
                                {
                                  reading_if_statement = false;
                                  then_body_tmp.erase();
                                  then_body_loc_tmp = *yylloc;
                                  nested_if_nb = 0;
                                  BEGIN(THEN_BODY);
                                }
                              else
                                {
#if (YY_FLEX_MAJOR_VERSION > 2) || (YY_FLEX_MAJOR_VERSION == 2 && YY_FLEX_MINOR_VERSION >= 6)
                                  yyout << endl;
#else
                                  *yyout << endl;
#endif
                                  BEGIN(INITIAL);
                                }
                              return token::EOL;
                            }

<STMT,EXPR>{SPC}+           { yylloc->step(); }

<STMT,EXPR>[0-9]+           {
                              yylval->build<int>(atoi(yytext));
                              return token::INTEGER;
                            }
<STMT,EXPR>\(               { return token::LPAREN; }
<STMT,EXPR>\)               { return token::RPAREN; }
<STMT,EXPR>:                { return token::COLON; }
<STMT,EXPR>,                { return token::COMMA; }
<STMT,EXPR>=                { return token::EQUAL; }
<STMT,EXPR>[!]              { return token::EXCLAMATION; }
<STMT,EXPR>"||"             { return token::LOGICAL_OR; }
<STMT,EXPR>&&               { return token::LOGICAL_AND; }
<STMT,EXPR>"|"              { return token::UNION; }
<STMT,EXPR>"&"              { return token::INTERSECTION; }
<STMT,EXPR>"^"              { return token::POWER; }
<STMT,EXPR>"<="             { return token::LESS_EQUAL; }
<STMT,EXPR>">="             { return token::GREATER_EQUAL; }
<STMT,EXPR>"<"              { return token::LESS; }
<STMT,EXPR>">"              { return token::GREATER; }
<STMT,EXPR>"=="             { return token::EQUAL_EQUAL; }
<STMT,EXPR>"!="             { return token::EXCLAMATION_EQUAL; }
<STMT,EXPR>[+]              { return token::PLUS; }
<STMT,EXPR>[-]              { return token::MINUS; }
<STMT,EXPR>[*]              { return token::TIMES; }
<STMT,EXPR>[/]              { return token::DIVIDE; }
<STMT,EXPR>in               { return token::IN; }
<STMT,EXPR>length           { return token::LENGTH; }

<STMT,EXPR>\"[^\"]*\"       {
                              yylval->build<string>(yytext + 1).pop_back();
                              return token::STRING;
                            }

<STMT>line                  { return token::LINE; }
<STMT>define                { return token::DEFINE; }

<STMT>echomacrovars         { return token::ECHOMACROVARS; }
<STMT>save                  { return token::SAVE; }

<STMT,EXPR>\]{SPC}*(\/\/.*)?{EOL} {
                              unput('\n');
                              if (reading_for_comprehension)
                                BEGIN(END_FOR_COMP);
                              return token::RBRACKET;
                            }
<END_FOR_COMP>{EOL}         {
                              yylloc->step();
                              if (driver.iter_loop())
                                {
                                  // Save old buffer state and location
                                  save_context(yylloc);

                                  is_for_comp_context = true;
                                  for_body = for_body_tmp;
                                  for_body_loc = for_body_loc_tmp;

                                  new_loop_body_buffer(yylloc);
                                }
                              reading_for_comprehension = false;
                              unput('\n');
                              BEGIN(STMT);
                            }
<STMT,EXPR>\[               {
                              // Store everything that comes after a [
                              // if you hit EOL and haven't seen a `for`
                              // unput everything and reparse normally
                              for_stmt_loc_tmp = *yylloc;
                              //yylloc->lines(1);
                              //yylloc->step();
                              for_body_tmp.erase();
                              for_body_loc_tmp = *yylloc;
                              BEGIN(FOR_COMP_STORAGE);
                              return token::LBRACKET;
                            }
<FOR_COMP_STORAGE>for       {
                              reading_for_comprehension = true;
                              BEGIN(EXPR);
                              return token::FOR;
                            }
<FOR_COMP_STORAGE>{EOL}     {
                              is_for_comp_context = false;
                              char *uput = &(for_body_tmp.back());
                              unput('\n');
                              while (uput >= &(for_body_tmp.front()))
                                unput(*uput--);
                              BEGIN(STMT);
                            }
<FOR_COMP_STORAGE><<EOF>>   {
                              is_for_comp_context = false;
                              char *uput = &(for_body_tmp.back());
                              while (uput >= &(for_body_tmp.front()))
                                unput(*uput--);
                              BEGIN(STMT);
                            }
<FOR_COMP_STORAGE>.         { for_body_tmp.append(yytext); yylloc->step(); }


<STMT,EXPR>\]               { return token::RBRACKET; }
<STMT>for                   { reading_for_statement = true; return token::FOR; }
<STMT>endfor                { driver.error(*yylloc, "@#endfor is not matched by a @#for statement"); }

<STMT>ifdef                 { reading_if_statement = true; return token::IFDEF; }
<STMT>ifndef                { reading_if_statement = true; return token::IFNDEF; }

<STMT>if                    { reading_if_statement = true; return token::IF; }
<STMT>else                  { driver.error(*yylloc, "@#else is not matched by an @#if/@#ifdef/@#ifndef statement"); }
<STMT>endif                 { driver.error(*yylloc, "@#endif is not matched by an @#if/@#ifdef/@#ifndef statement"); }

<STMT>echo                  { return token::ECHO_DIR; }
<STMT>error                 { return token::ERROR; }

<STMT,EXPR>[A-Za-z_][A-Za-z0-9_]* {
                              yylval->build<string>(yytext);
                              cout << "MATCHED A NAME: " << yytext << endl;
                              return token::NAME;
                            }

<EXPR><<EOF>>               {
                              if (is_for_comp_context)
                                if (driver.iter_loop())
                                  new_loop_body_buffer(yylloc);
                                else
                                  restore_context(yylloc);
                              else
                                driver.error(*yylloc, "Unexpected end of file while parsing a macro expression");
                            }
<STMT><<EOF>>               { driver.error(*yylloc, "Unexpected end of file while parsing a macro statement"); }

<FOR_BODY>{EOL}             { yylloc->lines(1); yylloc->step(); for_body_tmp.append(yytext); }
<FOR_BODY>^{SPC}*@#{SPC}*for({SPC}|{CONT}) {
                              nested_for_nb++;
                              for_body_tmp.append(yytext);
                              yylloc->step();
                            }
<FOR_BODY>.                 { for_body_tmp.append(yytext); yylloc->step(); }
<FOR_BODY><<EOF>>           { driver.error(for_stmt_loc_tmp, "@#for loop not matched by an @#endfor or file does not end with a new line (unexpected end of file)"); }
<FOR_BODY>^{SPC}*@#{SPC}*endfor{SPC}*(\/\/.*)?{EOL} {
                              yylloc->lines(1);
                              yylloc->step();
                              if (nested_for_nb)
                                {
                                  /* This @#endfor is not the end of the loop body,
                                     but only that of a nested @#for loop */
                                  nested_for_nb--;
                                  for_body_tmp.append(yytext);
                                }
                              else
                                {
                                  // Switch to loop body context, except if iterating over an empty array
                                  if (driver.iter_loop())
                                    {
                                      // Save old buffer state and location
                                      save_context(yylloc);

                                      is_for_context = true;
                                      for_body = for_body_tmp;
                                      for_body_loc = for_body_loc_tmp;

                                      new_loop_body_buffer(yylloc);
                                    }

                                  BEGIN(INITIAL);
                                }
                            }

<THEN_BODY>{EOL}            { yylloc->lines(1); yylloc->step(); then_body_tmp.append(yytext); }
<THEN_BODY>^{SPC}*@#{SPC}*if({SPC}|{CONT}) {
                              nested_if_nb++;
                              then_body_tmp.append(yytext);
                              yylloc->step();
                            }
<THEN_BODY>^{SPC}*@#{SPC}*ifdef({SPC}|{CONT}) {
                              nested_if_nb++;
                              then_body_tmp.append(yytext);
                              yylloc->step();
                            }
<THEN_BODY>^{SPC}*@#{SPC}*ifndef({SPC}|{CONT}) {
                              nested_if_nb++;
                              then_body_tmp.append(yytext);
                              yylloc->step();
                            }
<THEN_BODY>.                { then_body_tmp.append(yytext); yylloc->step(); }
<THEN_BODY><<EOF>>          { driver.error(if_stmt_loc_tmp, "@#if/@#ifdef/@#ifndef not matched by an @#endif or file does not end with a new line (unexpected end of file)"); }
<THEN_BODY>^{SPC}*@#{SPC}*else{SPC}*(\/\/.*)?{EOL} {
                              yylloc->lines(1);
                              yylloc->step();
                              if (nested_if_nb)
                                then_body_tmp.append(yytext);
                              else
                                {
                                  else_body_tmp.erase();
                                  else_body_loc_tmp = *yylloc;
                                  BEGIN(ELSE_BODY);
                                }
                             }

<THEN_BODY>^{SPC}*@#{SPC}*endif{SPC}*(\/\/.*)?{EOL} {
                              yylloc->lines(1);
                              yylloc->step();
                              if (nested_if_nb)
                                {
                                  /* This @#endif is not the end of the @#if we're parsing,
                                     but only that of a nested @#if */
                                  nested_if_nb--;
                                  then_body_tmp.append(yytext);
                                }
                              else
                                {
                                  if (driver.last_if)
                                    create_then_context(yylloc);
                                  else
                                    output_line(yylloc);

                                  BEGIN(INITIAL);
                                }
                            }

<ELSE_BODY>{EOL}            { yylloc->lines(1); yylloc->step(); else_body_tmp.append(yytext); }
<ELSE_BODY>^{SPC}*@#{SPC}*if({SPC}|{CONT}) {
                              nested_if_nb++;
                              else_body_tmp.append(yytext);
                              yylloc->step();
                            }
<ELSE_BODY>.                { else_body_tmp.append(yytext); yylloc->step(); }
<ELSE_BODY><<EOF>>          { driver.error(if_stmt_loc_tmp, "@#if/@#ifdef/@#ifndef not matched by an @#endif or file does not end with a new line (unexpected end of file)"); }

<ELSE_BODY>^{SPC}*@#{SPC}*endif{SPC}*(\/\/.*)?{EOL} {
                              yylloc->lines(1);
                              yylloc->step();
                              if (nested_if_nb)
                                {
                                  /* This @#endif is not the end of the @#if we're parsing,
                                     but only that of a nested @#if */
                                  nested_if_nb--;
                                  else_body_tmp.append(yytext);
                                }
                              else
                                {
                                  if (driver.last_if)
                                    create_then_context(yylloc);
                                  else
                                    create_else_context(yylloc);

                                  BEGIN(INITIAL);
                                }
                            }

<INITIAL><<EOF>>            {
                              // Quit lexer if end of main file
                              if (context_stack.empty())
                                {
                                  yyterminate();
                                }

                              // Else clean current scanning context
                              yy_delete_buffer(YY_CURRENT_BUFFER);
                              delete input;
                              delete yylloc->begin.filename;

                              /* If we are not in a loop body, or if the loop has terminated,
                                 pop a context */
                              if ((is_for_context || is_for_comp_context) && driver.iter_loop())
                                new_loop_body_buffer(yylloc);
                              else
                                restore_context(yylloc);
                            }

 /* We don't use echo, because under Cygwin it will add an extra \r */
<INITIAL>{EOL}              { yylloc->lines(1); yylloc->step();
#if (YY_FLEX_MAJOR_VERSION > 2) || (YY_FLEX_MAJOR_VERSION == 2 && YY_FLEX_MINOR_VERSION >= 6)
                              yyout << endl;
#else
                              *yyout << endl;
#endif
                            }

<INITIAL>.                  {
                              if (is_for_comp_context)
                                {
                                  cout << "I am actually reading a for comprehension!" << endl;
                                  cout << yytext << endl << endl;
                                  yyless(0);
                                  BEGIN(EXPR);
                                }
                              else
                                {
                                  /* Copy everything else to output */
                                  yylloc->step();
                                  ECHO;
                                }
                            }

<*>.                        { driver.error(*yylloc, "Macro lexer error: '" + string(yytext) + "'"); }
%%

MacroFlex::MacroFlex(istream* in, ostream* out, bool no_line_macro_arg, vector<string> path_arg)
  : MacroFlexLexer(in, out), input(in), no_line_macro(no_line_macro_arg), path(path_arg),
    is_for_context(false), is_for_comp_context(false),
    reading_for_statement(false), reading_for_comprehension(false), reading_if_statement(false)
{
}

void
MacroFlex::output_line(Macro::parser::location_type *yylloc) const
{
  if (!no_line_macro)
#if (YY_FLEX_MAJOR_VERSION > 2) || (YY_FLEX_MAJOR_VERSION == 2 && YY_FLEX_MINOR_VERSION >= 6)
    const_cast<ostream&>(yyout)
#else
    *yyout
#endif
     << endl << "@#line \"" << *yylloc->begin.filename << "\" "
     << yylloc->begin.line << endl;
}

void
MacroFlex::save_context(Macro::parser::location_type *yylloc)
{
  context_stack.push(ScanContext(input, YY_CURRENT_BUFFER, *yylloc, is_for_context,
                                 is_for_comp_context, for_body, for_body_loc));
}

void
MacroFlex::restore_context(Macro::parser::location_type *yylloc)
{
  input = context_stack.top().input;
  yy_switch_to_buffer(context_stack.top().buffer);
  *yylloc = context_stack.top().yylloc;
  is_for_context = context_stack.top().is_for_context;
  is_for_comp_context = context_stack.top().is_for_comp_context;
  for_body = context_stack.top().for_body;
  for_body_loc = context_stack.top().for_body_loc;
  // Remove top of stack
  context_stack.pop();
  // Dump @#line instruction
  output_line(yylloc);
}

void
MacroFlex::push_path(string *includepath, Macro::parser::location_type *yylloc,
                                  MacroDriver &driver)
{
  using namespace boost;
  vector<string> tokenizedPath;
  split(tokenizedPath, *includepath, is_any_of(":"), token_compress_on);
  for (vector<string>::iterator it = tokenizedPath.begin();
       it != tokenizedPath.end(); it++ )
    if (!it->empty())
      {
        trim(*it);
        path.push_back(*it);
      }
}

void
MacroFlex::create_include_context(string *filename, Macro::parser::location_type *yylloc,
                                  MacroDriver &driver)
{
#ifdef _WIN32
  string FILESEP = "\\";
#else
  string FILESEP = "/";
#endif
  save_context(yylloc);
  // Open new file
  input = new ifstream(*filename, ios::binary);
  if (input->fail())
    {
      ostringstream dirs;
      dirs << "." << FILESEP << endl;
      for (vector<string>::const_iterator it = path.begin(); it != path.end(); it++)
        {
          string testfile = *it + FILESEP + *filename;
          input = new ifstream(testfile, ios::binary);
          if (input->good())
            break;
          dirs << *it << endl;
        }
      if (input->fail())
        driver.error(*yylloc, "Could not open " + *filename +
                     ". The following directories were searched:\n" + dirs.str());
    }

  // Reset location
  yylloc->begin.filename = yylloc->end.filename = filename;
  yylloc->begin.line = yylloc->end.line = 1;
  yylloc->begin.column = yylloc->end.column = 0;
  // We are not in a loop body
  is_for_context = false;
  is_for_comp_context = false;
  for_body.clear();
  // Output @#line information
  output_line(yylloc);
  // Switch to new buffer
  yy_switch_to_buffer(yy_create_buffer(input, YY_BUF_SIZE));
}

void
MacroFlex::create_then_context(Macro::parser::location_type *yylloc)
{
  save_context(yylloc);
  input = new stringstream(then_body_tmp);
  *yylloc = then_body_loc_tmp;
  yylloc->begin.filename = yylloc->end.filename = new string(*then_body_loc_tmp.begin.filename);
  is_for_context = false;
  is_for_comp_context = false;
  for_body.clear();
  output_line(yylloc);
  yy_switch_to_buffer(yy_create_buffer(input, YY_BUF_SIZE));
}

void
MacroFlex::create_else_context(Macro::parser::location_type *yylloc)
{
  save_context(yylloc);
  input = new stringstream(else_body_tmp);
  *yylloc = else_body_loc_tmp;
  yylloc->begin.filename = yylloc->end.filename = new string(*else_body_loc_tmp.begin.filename);
  is_for_context = false;
  is_for_comp_context = false;
  for_body.clear();
  output_line(yylloc);
  yy_switch_to_buffer(yy_create_buffer(input, YY_BUF_SIZE));
}

void
MacroFlex::new_loop_body_buffer(Macro::parser::location_type *yylloc)
{
  cout << "NEW LOOP BUFF: " << (reading_for_comprehension ? "1" : "0") << " "
       << "..." << for_body << "..." << endl;
  input = new stringstream(for_body);
  *yylloc = for_body_loc;
  yylloc->begin.filename = yylloc->end.filename = new string(*for_body_loc.begin.filename);
  output_line(yylloc);
  cout << "Switching buffers" << endl;
  yy_switch_to_buffer(yy_create_buffer(input, YY_BUF_SIZE));
}

/* This implementation of MacroFlexLexer::yylex() is required to fill the
 * vtable of the class MacroFlexLexer. We define the scanner's main yylex
 * function via YY_DECL to reside in the MacroFlex class instead. */

#ifdef yylex
# undef yylex
#endif

int
MacroFlexLexer::yylex()
{
  cerr << "MacroFlexLexer::yylex() has been called, that should never happen!" << endl;
  exit(EXIT_FAILURE);
}
