#ifndef CONTEXT_MACRO_H
#define CONTEXT_MACRO_H

#include <string>
#include "variable.h"
#include "common_types.h"
#include "antlr4-runtime.h"
#include <unordered_map>
#include "../semantics/statement_fields.h"
#include "../diagnosable_impl.h"

namespace hlasm_plugin {
namespace parser_library {
namespace context {

//struct wrapping macro call args
struct macro_arg
{
	id_index id;
	macro_data_ptr data;

	macro_arg(macro_data_ptr data) : id(nullptr), data(std::move(data)) {}
	macro_arg(macro_data_ptr data, id_index name) : id(name), data(std::move(data)) {}
};

struct macro_invocation;
using macro_invo_ptr = std::shared_ptr<macro_invocation>;

//class representing macro definition
//contains info about keyword, positional parameters of HLASM mascro as well as derivation tree of the actual code
//has methods call to represent macro instruction call
//serves as prototype for creating macro_invocation objects
class macro_definition
{
	using macro_param_ptr = std::shared_ptr<macro_param_base>;

	std::vector<std::shared_ptr<positional_param>> positional_params_;
	std::unordered_map<id_index, macro_param_ptr> named_params_;
	const id_index label_param_name_;
	
public:

	//identifier of macro
	const id_index id;
	//params of macro
	const std::unordered_map<id_index, macro_param_ptr>& named_params() const;
	//tree representing macro definition
	const semantics::statement_block definition;
	//location of the macro definition in code
	const hlasm_plugin::parser_library::location location;
	//file, in which the macro was defined
	const std::string file_name;
	//initializes macro with its name and params - positional or keyword
	macro_definition(id_index name, id_index label_param_name, std::vector<macro_arg> params, semantics::statement_block definition, std::string file_name, hlasm_plugin::parser_library::location location);

	//returns object with parameters' data set to actual parameters in macro call
	macro_invo_ptr call(macro_data_ptr label_param_data, std::vector<macro_arg> actual_params) const;

	//satifying unordered_map needs 
	bool operator=(const macro_definition& m);

};

//represent macro instruction call 
//contains parameters with set values provided with the call 
struct macro_invocation
{
private:
	std::vector<macro_data_shared_ptr> syslist_;
public:
	//identifier of macro
	const id_index id;
	//params of macro
	const std::unordered_map<id_index, macro_param_ptr> named_params;
	//tree representing macro definition
	const semantics::statement_block* definition;
	//location of the macro definition in code
	const hlasm_plugin::parser_library::location location;
	//index to definition vector
	size_t current_statement;

	macro_invocation(id_index name, const semantics::statement_block* definition, std::unordered_map<id_index, macro_param_ptr> named_params, std::vector<macro_data_shared_ptr> syslist, hlasm_plugin::parser_library::location location);

	//gets syslist value
	const C_t& SYSLIST(size_t idx) const;

	//gets syslist sublisted value
	const C_t& SYSLIST(const std::vector<size_t>& offset) const;
};

}
}
}
#endif