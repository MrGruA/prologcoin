#include "builtins.hpp"
#include "interpreter.hpp"

namespace prologcoin { namespace interp {

    using namespace prologcoin::common;

    //
    // Profiling
    //

    bool builtins::profile_0(interpreter &interp, term &caller)
    {
	interp.print_profile();
	return true;
    }

    //
    // Simple
    //

    bool builtins::true_0(interpreter &interp, term &caller)
    {
	return true;
    }

    //
    // Control flow
    //

    bool builtins::operator_comma(interpreter &interp, term &caller)
    {
        term arg0 = interp.env().arg(caller, 0);
	term arg1 = interp.env().arg(caller, 1);
	interp.set_continuation_point(arg1);
        interp.allocate_environment();
	interp.set_continuation_point(arg0);
	return true;
    }

    bool builtins::operator_cut(interpreter &interp, term &caller)
    {
        if (interp.has_late_choice_point()) {
	    interp.reset_choice_point();
	    interp.tidy_trail();
	}
        return true;
    }

    bool builtins::operator_cut_if(interpreter &interp, term &caller)
    {
        if (interp.has_late_choice_point()) {
	    interp.reset_choice_point();
	    interp.tidy_trail();
	    auto *ch = interp.get_last_choice_point();
	    ch->bp = 2; // Don't back track to false clause
	}
        return true;
    }

    bool builtins::operator_disjunction(interpreter &interp, term &caller)
    {
	static con_cell arrow("->", 2);

	// Check if this is an if-then-else
	term arg0 = interp.env().arg(caller, 0);
	if (interp.env().functor(arg0) == arrow) {
	    return operator_if_then_else(interp, caller);
	}

        auto *ch = interp.allocate_choice_point(0);
        ch->bp = 1; // Index 0 and clause 1 (use query to get clause 1)
	interp.set_continuation_point(arg0);
	return true;
    }

    bool builtins::operator_if_then(interpreter &interp, term &caller)
    {
	static con_cell cut_op("!",0);

	auto *ch = interp.allocate_choice_point(0);
	ch->bp = 2;
	interp.move_cut_point_to_last_choice_point();
	term cut = interp.env().to_term(cut_op);
	term arg0 = interp.env().arg(caller, 0);
	term arg1 = interp.env().arg(caller, 1);
	interp.set_continuation_point(arg1);
	interp.allocate_environment();
	interp.set_continuation_point(cut);
	interp.allocate_environment();
	interp.set_continuation_point(arg0);
        return true;
    }

    bool builtins::operator_if_then_else(interpreter &interp, term &caller)
    {
	static con_cell cut_op_if("_!",0);

	term lhs = interp.env().arg(caller, 0);

	term cond = interp.env().arg(lhs, 0);
	term iftrue = interp.env().arg(lhs, 1);
	term cut_if = interp.env().to_term(cut_op_if);

	auto *ch = interp.allocate_choice_point(0);
	ch->bp = 1; // Go to 'C' the false clause if ((A->B) ; C) fails
	interp.move_cut_point_to_last_choice_point();
	interp.set_continuation_point(iftrue);
	interp.allocate_environment();
	interp.set_continuation_point(cut_if);
	interp.allocate_environment();
	interp.set_continuation_point(cond);
        return true;
    }

    //
    // Standard order, eqaulity and unification
    //

    bool builtins::operator_at_less_than(interpreter &interp, term &caller)
    {
        return interp.env().standard_order(interp.env().arg(caller, 0),
				           interp.env().arg(caller, 1)) < 0;
    }

    bool builtins::operator_at_equals_less_than(interpreter &interp, term &caller)
    {
        return interp.env().standard_order(interp.env().arg(caller, 0),
				           interp.env().arg(caller, 1)) <= 0;
    }

    bool builtins::operator_at_greater_than(interpreter &interp, term &caller)
    {
        return interp.env().standard_order(interp.env().arg(caller, 0),
				           interp.env().arg(caller, 1)) > 0;
    }

    bool builtins::operator_at_greater_than_equals(interpreter &interp, term &caller)
    {
        return interp.env().standard_order(interp.env().arg(caller, 0),
				           interp.env().arg(caller, 1)) >= 0;
    }

    bool builtins::operator_equals(interpreter &interp, term &caller)
    {
        return interp.env().standard_order(interp.env().arg(caller, 0),
					   interp.env().arg(caller, 1)) == 0;
    }

    bool builtins::operator_not_equals(interpreter &interp, term &caller)
    {
        return interp.env().standard_order(interp.env().arg(caller, 0),
					   interp.env().arg(caller, 1)) != 0;
    }

    bool builtins::compare_3(interpreter &interp, term &caller)
    {
        term order = interp.env().arg(caller, 0);

	int c = interp.env().standard_order(interp.env().arg(caller, 1),
					    interp.env().arg(caller, 2));
	if (c < 0) {
	    term lt = interp.env().to_term(con_cell("<",0));
	    return interp.unify(order, lt);
	} else if (c > 0) {
	    term gt = interp.env().to_term(con_cell(">",0));
	    return interp.unify(order, gt);
	} else {
	    term eq = interp.env().to_term(con_cell("=",0));
	    return interp.unify(order, eq);
	}
    }

    bool builtins::operator_unification(interpreter &interp, term &caller)
    {
	term arg0 = interp.env().arg(caller, 0);
	term arg1 = interp.env().arg(caller, 1);
	bool r = interp.unify(arg0, arg1);
	return r;
    }

    bool builtins::operator_cannot_unify(interpreter &interp, term &caller)
    {
	term lhs = interp.env().arg(caller, 0);
	term rhs = interp.env().arg(caller, 1);
	size_t current_tr = interp.get_trail_pointer();
	bool r = interp.unify(lhs, rhs);
	if (r) {
	    interp.unwind(current_tr);
	}
	return !r;
    }

    //
    // Type tests
    //
    
    bool builtins::var_1(interpreter &interp, term &caller)
    {
	return interp.env().arg(caller, 0).tag() == tag_t::REF;
    }

    bool builtins::nonvar_1(interpreter &interp, term &caller)
    {
	return !var_1(interp, caller);
    }

    bool builtins::integer_1(interpreter &interp, term &caller)
    {
	return interp.env().arg(caller, 0).tag() == tag_t::INT;
    }

    bool builtins::number_1(interpreter &interp, term &caller)
    {
	return integer_1(interp, caller);
    }

    bool builtins::atom_1(interpreter &interp, term &caller)
    {
	term arg = interp.env().arg(caller, 0);
	
	switch (arg.tag()) {
	case tag_t::CON: return true;
	case tag_t::STR: {
	    con_cell f = interp.env().functor(arg);
	    return f.arity() == 0;
	}
	default: return false;
	}
    }

    bool builtins::compound_1(interpreter &interp, term &caller)
    {
	term arg = interp.env().arg(caller, 0);
	if (arg.tag() != tag_t::STR) {
	    return false;
	}
	return interp.env().functor(arg).arity() > 0;
    }

    bool builtins::callable_1(interpreter &interp, term &caller)
    {
	return atom_1(interp, caller) || compound_1(interp, caller);
    }

    bool builtins::atomic_1(interpreter &interp, term &caller)
    {
	return atom_1(interp, caller);
    }

    bool builtins::ground_1(interpreter &interp, term &caller)
    {
	term arg = interp.env().arg(caller, 0);
	return interp.env().is_ground(arg);
    }

    // TODO: cyclic_term/1 and acyclic_term/1

    //
    // Arithmetics
    //

    bool builtins::is_2(interpreter &interp, term &caller)
    {
	term lhs = interp.env().arg(caller, 0);
	term rhs = interp.env().arg(caller, 1);
	term result = interp.arith().eval(rhs, "is/2");
	return interp.unify(lhs, result);
    }	

    //
    // Analyzing & constructing terms
    //

    bool builtins::copy_term_2(interpreter &interp, term &caller)
    {
	term arg1 = interp.env().arg(caller, 0);
	term arg2 = interp.env().arg(caller, 1);
	term copy_arg1 = interp.copy(arg1);
	return interp.unify(arg2, copy_arg1);
    }

    bool builtins::functor_3(interpreter &interp, term &caller)
    {
	term t = interp.env().arg(caller, 0);
	term f = interp.env().arg(caller, 1);
	term a = interp.env().arg(caller, 2);

	switch (t.tag()) {
  	  case tag_t::REF:
            interp.abort(interpreter_exception_not_sufficiently_instantiated("funtor/3: Arguments are not sufficiently instantiated"));
	    return false;
	  case tag_t::INT:
 	  case tag_t::BIG: {
	    term zero = interp.env().to_term(int_cell(0));
	    return interp.unify(f, t) && interp.unify(a, zero);
	    }
	  case tag_t::STR:
  	  case tag_t::CON: {
	      con_cell tf = interp.env().functor(t);
	      term fun = interp.env().to_term(tf);
	      term arity = interp.env().to_term(int_cell(tf.arity()));
	      return interp.unify(f, fun) && interp.unify(a, arity);
	    }
	}

        return false;
    }

   term builtins::deconstruct_write_list(interpreter &interp,
					 term &t, size_t index)
    {
	term empty_lst = interp.env().empty_list();
	term lst = empty_lst;
	con_cell f = interp.env().functor(t);
	size_t n = f.arity();
	term tail = lst;
	while (index < n) {
	    term arg = interp.env().arg(t, index);
	    term elem = interp.new_dotted_pair(arg, empty_lst);
	    if (interp.env().is_empty_list(tail)) {
		lst = elem;
		tail = elem;
	    } else {
		interp.env().set_arg(tail, 1, elem);
		tail = elem;
	    }
	    index++;
	}
	return lst;
    }

    bool builtins::deconstruct_read_list(interpreter &interp,
					 term lst,
					 term &t, size_t index)
    {
	con_cell f = interp.env().functor(t);
	size_t n = f.arity();
	while (index < n) {
	    if (lst.tag() == tag_t::REF) {
		term tail = deconstruct_write_list(interp, t, index);
		return interp.unify(lst, tail);
	    }
	    if (!interp.env().is_dotted_pair(lst)) {
		return false;
	    }
	    term elem = interp.env().arg(lst, 0);
	    term arg = interp.env().arg(t, index);
	    if (!interp.unify(elem, arg)) {
		return false;
	    }
	    lst = interp.env().arg(lst, 1);
	    index++;
	}
	return true;
    }

    bool builtins::operator_deconstruct(interpreter &interp, term &caller)
    {
	auto lhs = interp.env().arg(caller, 0);
	auto rhs = interp.env().arg(caller, 1);

	// To make deconstruction more efficient, let's handle the
	// common scenarios first.

	if (lhs.tag() == tag_t::REF && rhs.tag() == tag_t::REF) {
		interp.abort(interpreter_exception_not_sufficiently_instantiated("=../2: Arguments are not sufficiently instantiated"));
	}

	if (lhs.tag() == tag_t::REF) {
	    if (!interp.env().is_list(rhs)) {
		interp.abort(interpreter_exception_not_list("=../2: Second argument is not a list; found " + interp.env().to_string(rhs)));
	    }
	    size_t lst_len = interp.env().list_length(rhs);
	    if (lst_len == 0) {
		interp.abort(interpreter_exception_not_list("=../2: Second argument must be non-empty; found " + interp.env().to_string(rhs)));
	    }
	    term first_elem = interp.env().arg(rhs,0);
	    if (first_elem.tag() == tag_t::REF) {
		interp.abort(interpreter_exception_not_sufficiently_instantiated("=../2: Arguments are not sufficiently instantiated"));
	    }
	    if (first_elem.tag() == tag_t::INT && lst_len == 1) {
		return interp.unify(lhs, first_elem);
	    }
	    if (!interp.env().is_functor(first_elem)) {
		return false;
	    }
	    con_cell f = interp.env().functor(first_elem);
	    size_t num_args = lst_len - 1;
	    term t = interp.new_term(interp.env().to_functor(f, num_args));
	    term lst = interp.env().arg(rhs, 1);
	    for (size_t i = 0; i < num_args; i++) {
		if (lst.tag() == tag_t::REF) {
		    term tail = deconstruct_write_list(interp, lhs, i);
		    return interp.unify(lst, tail);
		}
		term arg = interp.env().arg(lst, 0);
		interp.env().set_arg(t, i, arg);

		lst = interp.env().arg(lst, 1);
	    }
	    return interp.unify(lhs, t);
	}
	if (lhs.tag() == tag_t::INT) {
	    term empty = interp.env().empty_list();
	    term lst = interp.new_dotted_pair(lhs,empty);
	    return interp.unify(rhs, lst);
	}
	if (lhs.tag() != tag_t::STR && lhs.tag() != tag_t::CON) {
	    return false;
	}
	con_cell f = interp.env().to_atom(interp.env().functor(lhs));
	term elem = interp.env().to_term(f);
	term lst = deconstruct_write_list(interp, lhs, 0);
	lst = interp.new_dotted_pair(elem, lst);
	return interp.unify(rhs, lst);
    }

    //
    // Meta
    //

    bool builtins::operator_disprove(interpreter &interp, term &caller)
    {
	term arg = interp.env().arg(caller, 0);
	interp.new_meta_context<meta_context>(&operator_disprove_post);

	interp.set_top_e();
	interp.allocate_environment();
	interp.set_top_b();
	interp.set_current_query(arg);
	interp.set_continuation_point(arg);

	return true;
    }

    void builtins::operator_disprove_post(interpreter &interp,
					  meta_context *context)
    {
        bool failed = interp.is_top_fail();
	if (!failed) {
	    interp.unwind_to_top_choice_point();
	}
	interp.release_last_meta_context();
	// Note that this is "disprove," so its success is the reverse of
	// the underlying expression to succeed.
	interp.set_top_fail(!failed);
    }

}}