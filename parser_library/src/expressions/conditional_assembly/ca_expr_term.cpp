/*
 * Copyright (c) 2019 Broadcom.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 *
 * Contributors:
 *   Broadcom, Inc. - initial API and implementation
 */

#include "ca_expr_term.h"

#include "ca_expr_policy.h"
#include "ca_operator_binary.h"
#include "ca_operator_unary.h"
#include "semantics/concatenation_term.h"

namespace hlasm_plugin {
namespace parser_library {
namespace expressions {

ca_expr_list::ca_expr_list(std::vector<ca_expr_ptr> expr_list, range expr_range)
    : ca_expression(context::SET_t_enum::UNDEF_TYPE, std::move(expr_range))
    , expr_list(std::move(expr_list))
{}

undef_sym_set ca_expr_list::get_undefined_attributed_symbols(const context::dependency_solver& solver) const
{
    undef_sym_set tmp;
    for (auto&& expr : expr_list)
        tmp.merge(expr->get_undefined_attributed_symbols(solver));
    return tmp;
}

bool is_symbol(const ca_expr_ptr& expr) { return static_cast<const ca_symbol*>(expr.get()) != nullptr; }

const std::string& get_symbol(const ca_expr_ptr& expr) { return *static_cast<const ca_symbol*>(expr.get())->symbol; }

void ca_expr_list::resolve_expression_tree(context::SET_t_enum kind)
{
    expr_kind = kind;

    if (kind == context::SET_t_enum::A_TYPE)
        resolve<context::A_t>();
    else if (kind == context::SET_t_enum::B_TYPE)
        resolve<context::B_t>();
    else if (kind == context::SET_t_enum::C_TYPE)
        resolve<context::C_t>();
    else
        assert(false);
}

void ca_expr_list::collect_diags() const
{
    for (auto&& expr : expr_list)
        collect_diags_from_child(*expr);
}

bool ca_expr_list::is_character_expression() const { return false; }

template<typename T> void ca_expr_list::resolve()
{
    if (expr_list.empty())
    {
        add_diagnostic(diagnostic_op::error_CE003(expr_range));
        return;
    }

    size_t it = 0;
    bool err = false;

    ca_expr_ptr final_expr = retrieve_term<ca_expr_traits<T>::policy_t>(it, 0);
    err |= final_expr == nullptr;

    while (it == expr_list.size() && !err)
    {
        auto op_range = expr_list[it]->expr_range;

        auto [prio, op_type] = retrieve_binary_operator<ca_expr_traits<T>::policy_t>(it, err);

        auto r_expr = retrieve_term<ca_expr_traits<T>::policy_t>(++it, prio);
        err |= r_expr == nullptr;

        final_expr = std::make_unique<ca_function_binary_operator>(
            std::move(final_expr), std::move(r_expr), op_type, context::object_traits<T>::type_enum, op_range);
    }

    if (err)
    {
        expr_list.clear();
        expr_list.emplace_back(std::make_unique<ca_constant>(0, expr_range));
    }

    // resolve created tree
    final_expr->resolve_expression_tree(context::object_traits<T>::type_enum);

    // move resolved tree to the front of the array
    expr_list.clear();
    expr_list.emplace_back(std::move(final_expr));
}

template<typename EXPR_POLICY> ca_expr_ptr ca_expr_list::retrieve_term(size_t& it, int priority)
{
    // list is exhausted
    if (it == expr_list.size())
    {
        auto r = expr_list[it - 1]->expr_range;
        r.start = r.end;
        r.end.column++;
        add_diagnostic(diagnostic_op::error_CE003(r));
        return nullptr;
    }

    // first possible term
    auto& curr_expr = expr_list[it];

    // is unary op
    if (is_symbol(curr_expr))
    {
        if (auto op_type = EXPR_POLICY::get_operator(get_symbol(curr_expr)); EXPR_POLICY::is_unary(op_type))
        {
            auto new_expr = retrieve_term<EXPR_POLICY>(++it, EXPR_POLICY::get_priority(op_type));
            return std::make_unique<ca_function_unary_operator>(
                std::move(new_expr), op_type, EXPR_POLICY::set_type, curr_expr->expr_range);
        }
    }

    // is only term
    if (it + 1 == expr_list.size())
        return std::move(expr_list[it++]);

    // tries to get binary operator
    auto op_it = it + 1;
    auto op_range = expr_list[op_it]->expr_range;
    bool err = false;

    auto [op_prio, op_type] = retrieve_binary_operator<EXPR_POLICY>(op_it, err);
    if (err)
        return nullptr;

    // if operator is of lower priority than the calling operator, finish
    if (op_prio >= priority)
        return std::move(curr_expr);
    else
        it = op_it;

    auto right_expr = retrieve_term<EXPR_POLICY>(++it, op_prio);

    return std::make_unique<ca_function_binary_operator>(
        std::move(curr_expr), std::move(right_expr), op_type, EXPR_POLICY::set_type, op_range);
}

template<typename EXPR_POLICY> std::pair<int, ca_expr_ops> ca_expr_list::retrieve_binary_operator(size_t& it, bool& err)
{
    auto& op = expr_list[it];

    if (!is_symbol(op) || !EXPR_POLICY::is_operator(EXPR_POLICY::get_operator(get_symbol(op))))
    {
        add_diagnostic(diagnostic_op::error_CE001(expr_range));
        err = true;
    }

    ca_expr_ops op_type = EXPR_POLICY::get_operator(get_symbol(expr_list[it]));

    if (EXPR_POLICY::multiple_words(op_type))
    {
        if (is_symbol(expr_list[it + 1]) && get_symbol(expr_list[it + 1]) == "NOT")
        {
            op_type = EXPR_POLICY::get_operator(get_symbol(expr_list[it + 1]) + " NOT");
            ++it;
        }
    }

    auto op_prio = EXPR_POLICY::get_priority(op_type);

    return std::make_pair(op_prio, op_type);
}

ca_string::substring_t::substring_t()
    : start(nullptr)
    , count(nullptr)
    , to_end(false)
{}

ca_string::ca_string(
    semantics::concat_chain value, ca_expr_ptr duplication_factor, substring_t substring, range expr_range)
    : ca_expression(context::SET_t_enum::C_TYPE, std::move(expr_range))
    , value(std::move(value))
    , duplication_factor(std::move(duplication_factor))
    , substring(std::move(substring))
{}

undef_sym_set ca_string::get_undefined_attributed_symbols(const context::dependency_solver& solver) const
{
    undef_sym_set tmp;
    if (duplication_factor)
        tmp = duplication_factor->get_undefined_attributed_symbols(solver);
    if (substring.start)
        tmp.merge(substring.start->get_undefined_attributed_symbols(solver));
    if (substring.count)
        tmp.merge(substring.count->get_undefined_attributed_symbols(solver));
    return tmp;
}

void ca_string::resolve_expression_tree(context::SET_t_enum kind)
{
    if (expr_kind != kind)
        add_diagnostic(diagnostic_op::error_CE004(expr_range));
    else
    {
        if (duplication_factor)
            duplication_factor->resolve_expression_tree(context::SET_t_enum::A_TYPE);
        if (substring.start)
            substring.start->resolve_expression_tree(context::SET_t_enum::A_TYPE);
        if (substring.count)
            substring.count->resolve_expression_tree(context::SET_t_enum::A_TYPE);
    }
}

void ca_string::collect_diags() const
{
    if (duplication_factor)
        collect_diags_from_child(*duplication_factor);
    if (substring.start)
        collect_diags_from_child(*substring.start);
    if (substring.count)
        collect_diags_from_child(*substring.count);
}

bool ca_string::is_character_expression() const { return duplication_factor == nullptr; }

ca_var_sym::ca_var_sym(semantics::vs_ptr symbol, range expr_range)
    : ca_expression(context::SET_t_enum::A_TYPE, std::move(expr_range))
    , symbol(std::move(symbol))
{}

undef_sym_set get_undefined_attributed_symbols_vs(
    const semantics::vs_ptr& symbol, const context::dependency_solver& solver)
{
    undef_sym_set tmp;
    // for (auto&& expr : symbol->subscript)
    //    tmp.merge(expr->get_undefined_attributed_symbols_vs(solver));

    if (symbol->created)
    {
        auto created = symbol->access_created();
        for (auto&& point : created->created_name)
            if (point->type == semantics::concat_type::VAR)
                tmp.merge(get_undefined_attributed_symbols_vs(point->access_var()->symbol, solver));
    }
    return tmp;
}

undef_sym_set ca_var_sym::get_undefined_attributed_symbols(const context::dependency_solver& solver) const
{
    return get_undefined_attributed_symbols_vs(symbol, solver);
}

void ca_var_sym::resolve_expression_tree(context::SET_t_enum)
{
    // auto&& sym = std::get<semantics::vs_ptr>(symbol);
    // for (auto&& expr : sym->subscript)
    //    expr->resolve_expression_tree(context::SET_t_enum::SETA_type);
}

void ca_var_sym::collect_diags() const
{
    // for (auto&& expr : symbol->subscript)
    //    collect_diags_from_child(*expr);
}

bool ca_var_sym::is_character_expression() const { return false; }

ca_constant::ca_constant(context::A_t value, range expr_range)
    : ca_expression(context::SET_t_enum::A_TYPE, std::move(expr_range))
    , value(value)
{}

undef_sym_set ca_constant::get_undefined_attributed_symbols(const context::dependency_solver&) const
{
    return undef_sym_set();
}

void ca_constant::resolve_expression_tree(context::SET_t_enum kind)
{
    if (kind == context::SET_t_enum::C_TYPE)
        add_diagnostic(diagnostic_op::error_CE004(expr_range));
}

void ca_constant::collect_diags() const {}

bool ca_constant::is_character_expression() const { return false; }

ca_symbol::ca_symbol(context::id_index symbol, range expr_range)
    : ca_expression(context::SET_t_enum::A_TYPE, std::move(expr_range))
    , symbol(symbol)
{}

undef_sym_set ca_symbol::get_undefined_attributed_symbols(const context::dependency_solver&) const
{
    return undef_sym_set();
}

void ca_symbol::resolve_expression_tree(context::SET_t_enum kind)
{
    if (kind == context::SET_t_enum::C_TYPE)
        add_diagnostic(diagnostic_op::error_CE004(expr_range));
}

void ca_symbol::collect_diags() const {}

bool ca_symbol::is_character_expression() const { return false; }

ca_symbol_attribute::ca_symbol_attribute(context::id_index symbol, context::data_attr_kind attribute, range expr_range)
    : ca_expression(attribute == context::data_attr_kind::T ? context::SET_t_enum::C_TYPE : context::SET_t_enum::A_TYPE,
        std::move(expr_range))
    , attribute(attribute)
    , symbol(symbol)

{}

ca_symbol_attribute::ca_symbol_attribute(semantics::vs_ptr symbol, context::data_attr_kind attribute, range expr_range)
    : ca_expression(attribute == context::data_attr_kind::T ? context::SET_t_enum::C_TYPE : context::SET_t_enum::A_TYPE,
        std::move(expr_range))
    , attribute(attribute)
    , symbol(std::move(symbol))
{}

undef_sym_set ca_symbol_attribute::get_undefined_attributed_symbols(const context::dependency_solver& solver) const
{
    if (!context::symbol_attributes::ordinary_allowed(attribute))
        return undef_sym_set();

    if (std::holds_alternative<context::id_index>(symbol))
        return { std::get<context::id_index>(symbol) };
    else if (std::holds_alternative<semantics::vs_ptr>(symbol))
        return get_undefined_attributed_symbols_vs(std::get<semantics::vs_ptr>(symbol), solver);
    else
    {
        assert(false);
        return undef_sym_set();
    }
}

void ca_symbol_attribute::resolve_expression_tree(context::SET_t_enum kind)
{
    if (kind == context::SET_t_enum::C_TYPE && kind != expr_kind)
        add_diagnostic(diagnostic_op::error_CE004(expr_range));
    else if (std::holds_alternative<semantics::vs_ptr>(symbol))
    {
        // auto&& sym = std::get<semantics::vs_ptr>(symbol);
        // for (auto&& expr : sym->subscript)
        //    expr->resolve_expression_tree(context::SET_t_enum::SETA_type);
    }
}

void ca_symbol_attribute::collect_diags() const
{
    if (std::holds_alternative<semantics::vs_ptr>(symbol))
    {
        // auto&& sym = std::get<semantics::vs_ptr>(symbol);
        // for (auto&& expr : sym->subscript)
        //    collect_diags_from_child(*expr);
    }
}

bool ca_symbol_attribute::is_character_expression() const { return false; }

ca_function::ca_function(
    ca_expr_funcs function, std::vector<ca_expr_ptr> parameters, ca_expr_ptr duplication_factor, range expr_range)
    : ca_expression(ca_common_expr_policy::get_function_type(function), std::move(expr_range))
    , function(function)
    , parameters(std::move(parameters))
    , duplication_factor(std::move(duplication_factor))
{}

undef_sym_set ca_function::get_undefined_attributed_symbols(const context::dependency_solver& solver) const
{
    undef_sym_set ret;
    for (auto&& expr : parameters)
        ret.merge(expr->get_undefined_attributed_symbols(solver));
    if (duplication_factor)
        ret.merge(duplication_factor->get_undefined_attributed_symbols(solver));
    return ret;
}

void ca_function::resolve_expression_tree(context::SET_t_enum kind)
{
    if (kind != expr_kind)
        add_diagnostic(diagnostic_op::error_CE004(expr_range));
    else if (duplication_factor && expr_kind != context::SET_t_enum::C_TYPE)
        add_diagnostic(diagnostic_op::error_CE005(duplication_factor->expr_range));
    else
    {
        auto [param_size, param_kind] = ca_common_expr_policy::get_function_param_info(function, expr_kind);
        if (parameters.size() != param_size)
            add_diagnostic(diagnostic_op::error_CE006(expr_range));
        else
        {
            for (auto&& expr : parameters)
                expr->resolve_expression_tree(param_kind);
        }
    }
}

void ca_function::collect_diags() const {}

bool ca_function::is_character_expression() const { return false; }

} // namespace expressions
} // namespace parser_library
} // namespace hlasm_plugin