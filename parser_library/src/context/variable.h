#ifndef CONTEXT_VARIABLES_H
#define CONTEXT_VARIABLES_H

#include "common_types.h"
#include "macro_param_data.h"
#include "id_storage.h"
#include "../../include/shared/range.h"

#include <memory>
#include <unordered_map>

namespace hlasm_plugin {
namespace parser_library {
namespace context {

class set_symbol_base;
class macro_param_base;
class variable_symbol;

using var_sym_ptr = std::shared_ptr<variable_symbol>;
using set_sym_ptr = std::shared_ptr<set_symbol_base>;

//base for variable symbols
class variable_symbol
{
public:
	//name of the symbol
	const id_index id;

	//returns kind of variable symbol
	virtual variable_kind var_kind() const;

	//casts this to set_symbol_base
	set_symbol_base* access_set_symbol_base();

	//casts this to macro_param
	macro_param_base* access_macro_param_base();

	virtual ~variable_symbol();
protected:
	variable_symbol(id_index name);
};





//*********************set symbols*********************//





template<typename T> class set_symbol;

//base class for set_symbols
class set_symbol_base : public variable_symbol
{
public:
	//describes whether set symbol is scalar
	//when scalar, sublist notation is not allowed
	const bool is_scalar;

	//returns type of set symbol
	virtual SET_t_enum type() const;

	//returns kind of set symbol
	virtual variable_kind var_kind() const override;

	//casts this to specialized set symbol
	template <typename T>
	set_symbol<T>* access_set_symbol()
	{
		return dynamic_cast<set_symbol<T>*>(this);
	}

protected:
	set_symbol_base(id_index name, bool is_scalar);
};

//specialized set symbol holding data T (int = A_t, bool = B_t, std::string=C_t)
template<typename T>
class set_symbol : public set_symbol_base
{
	static_assert(object_traits<T>::type_enum != SET_t_enum::UNDEF_TYPE, "Not a SET variable type.");

	//data holding this set_symbol 
	//can be scalar or only array of scallars - no other nesting allowed
	std::unordered_map<size_t, T> data;
public:

	set_symbol(id_index name, bool is_scalar) : set_symbol_base(name, is_scalar) {}

	//returns type of set symbol
	SET_t_enum type() const override { return object_traits<T>::type_enum; }

	//gets value from non scalar set symbol
	//if data at idx is not set or it does not exists, default is returned
	const T& get_value(size_t idx) const
	{
		if (is_scalar)
			return object_traits<T>::default_v();

		auto tmp = data.find(idx);
		if (tmp == data.end())
			return object_traits<T>::default_v();
		return tmp->second;
	}

	//gets value from scalar set symbol
	const T& get_value() const
	{
		if (!is_scalar)
			return object_traits<T>::default_v();

		auto tmp = data.find(0);
		if (tmp == data.end())
			return object_traits<T>::default_v();
		return tmp->second;
	}

	//sets value to scalar set symbol
	void set_value(T value)
	{
		data.insert_or_assign(0, std::move(value));
	}

	//sets value to non scalar set symbol
	//any index can be accessed
	void set_value(T value, size_t idx)
	{
		if (is_scalar) 
			data.insert_or_assign(0, std::move(value));
		else
			data.insert_or_assign(idx, std::move(value));
	}
};





//*********************macro parameters*********************//





class keyword_param;
class positional_param;

//class wrapping macro parameters data
class macro_param_base : public variable_symbol
{
public:
	macro_data_shared_ptr data;

	//returns kind of macro_param
	virtual variable_kind var_kind() const override;
	//returns type of macro parameter
	virtual macro_param_type param_type() const;

	keyword_param* access_keyword_param();
	positional_param* access_positional_param();

	//gets value of data where parameter is list of nested data offsets
	virtual const C_t& get_value(const std::vector<size_t>& offset) const = 0;
	//gets value of data where parameter is offset to data field
	virtual const C_t& get_value(size_t idx) const = 0;
	//gets value of whole macro parameter
	virtual const C_t& get_value() const = 0;

protected:
	macro_param_base(id_index name);
};

using macro_param_ptr = std::shared_ptr<macro_param_base>;

//represent macro param with stated position and name, positional param
class keyword_param : public macro_param_base
{
	macro_data_shared_ptr default_data_;
public:
	//returns type of macro parameter
	virtual macro_param_type param_type() const override;

	//gets value of data where parameter is list of nested data offsets
	virtual const C_t& get_value(const std::vector<size_t>& offset) const override;
	//gets value of data where parameter is offset to data field
	virtual const C_t& get_value(size_t idx) const override;
	//gets value of whole macro parameter
	virtual const C_t& get_value() const override;

	keyword_param(id_index name, macro_data_ptr default_value);
};

//represents macro param with default value, name and no position, keyword param
class positional_param : public macro_param_base
{
public:
	const size_t position;

	//returns type of macro parameter
	virtual macro_param_type param_type() const override;

	//gets value of data where parameter is list of nested data offsets
	virtual const C_t& get_value(const std::vector<size_t>& offset) const override;
	//gets value of data where parameter is offset to data field
	virtual const C_t& get_value(size_t idx) const override;
	//gets value of whole macro parameter
	virtual const C_t& get_value() const override;

	positional_param(id_index name, size_t position);
};





//*********************sequence symbol*********************//
struct sequence_symbol;
struct opencode_sequence_symbol;
struct macro_sequence_symbol;
using sequence_symbol_ptr = std::unique_ptr<sequence_symbol>;

enum class sequence_symbol_kind
{
	OPENCODE,MACRO
};

//structure representing sequence symbol
struct sequence_symbol
{
	id_index name;
	location symbol_location; //TODO for lsp
	sequence_symbol_kind kind;

	const macro_sequence_symbol* access_macro_symbol() const;
	const opencode_sequence_symbol* access_opencode_symbol() const;

	virtual ~sequence_symbol() = default;

protected:
	sequence_symbol(id_index name, const sequence_symbol_kind kind, location symbol_location);
};

struct opencode_sequence_symbol : public sequence_symbol
{
	struct opencode_position {
		opencode_position(size_t file_line=0, size_t file_offset=0) :file_line(file_line), file_offset(file_offset) {}
		size_t file_line; size_t file_offset; 
		bool operator==(const opencode_position& oth) const { return file_line == oth.file_line && file_offset == oth.file_offset; }
	};

	struct copy_frame { 
		id_index copy_member; size_t statement_offset;
		bool operator==(const copy_frame& oth) const { return copy_member == oth.copy_member && statement_offset == oth.statement_offset; }
	};

	opencode_position statement_position;
	std::vector<copy_frame> copy_stack;

	opencode_sequence_symbol(id_index name, location symbol_location, opencode_position statement_position, std::vector<copy_frame> copy_stack = {});

	bool operator==(const opencode_sequence_symbol& oth) const;
};

struct macro_sequence_symbol : public sequence_symbol
{
	size_t statement_offset;

	macro_sequence_symbol(id_index name, location symbol_location, size_t statement_offset);
};

}
}
}
#endif
