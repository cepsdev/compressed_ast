/*
Copyright 2021 Tomas Prerovsky (cepsdev@hotmail.com).

Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/


#include<iostream>
#include "ceps_all.hh"


#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <vector>
#include <mutex>
#include <map>
#include <thread>
#include <sstream>
#include <fstream>

#define VERSION_CEPSVALIDATE_MAJOR 0
#define VERSION_CEPSVALIDATE_MINOR 5

using namespace std;
using namespace ceps::ast;

int main(int argc, char** argv){
    std::vector<std::string> grammar_files {"prelude/base"};
	ceps::Ceps_Environment ceps_env{""};
	Nodeset universe;
	for(std::string const & filename : grammar_files)
	{
		ifstream in{filename};
		if (!in)
		{
			std::cerr << "\n***Error: Couldn't open file '" << filename << "' " << std::endl;
			return EXIT_FAILURE;
		}

		try{
			Ceps_parser_driver driver(ceps_env.get_global_symboltable(),in);
			ceps::Cepsparser parser(driver);
			if (parser.parse() != 0)
				continue;
			if (driver.errors_occured())
				continue;
			auto root = ceps::ast::nlf_ptr(driver.parsetree().get_root());
			char buffer[PATH_MAX] = {};
			if ( buffer != realpath(filename.c_str(),buffer) ){
				std::cerr << "\n***Error: realpath() failed for '" << filename << "' " << std::endl;
				return EXIT_FAILURE;
			}

			root->children().insert(root->children().begin(),new ceps::ast::Struct("@@file",new ceps::ast::String(std::string{buffer}),nullptr,nullptr));
			std::vector<ceps::ast::Nodebase_ptr> generated_nodes;
			ceps::interpreter::evaluate(universe,
						                    driver.parsetree().get_root(),
						                    ceps_env.get_global_symboltable(),
						                    ceps_env.interpreter_env(),&generated_nodes);
			auto p = new Root();
			p->children().insert(p->children().end(), generated_nodes.begin(), generated_nodes.end());
		} catch (ceps::interpreter::semantic_exception & se)
		{
			std::cerr << "[ERROR][Interpreter]:"<< se.what() << std::endl;
		}
		catch (std::runtime_error & re)
		{
			std::cerr << "[ERROR][System]:"<< re.what() << std::endl;
		}
	}//for
 return 0;
}