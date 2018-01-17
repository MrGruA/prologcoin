#pragma once

#ifndef _interp_wam_interpreter_hpp
#define _interp_wam_interpreter_hpp

#include <istream>
#include <vector>
#include <iomanip>
#include "interpreter_base.hpp"

namespace prologcoin { namespace interp {

class test_wam_interpreter;
class test_wam_compiler;

struct static_ 
{
  template<typename T> static_ (T once) { once(); }
  ~static_ () {}
};

enum wam_instruction_type {
  PUT_VARIABLE_X,
  PUT_VARIABLE_Y,
  PUT_VALUE_X,
  PUT_VALUE_Y,
  PUT_UNSAFE_VALUE_Y,
  PUT_STRUCTURE_A,
  PUT_STRUCTURE_X,
  PUT_STRUCTURE_Y,
  PUT_LIST,
  PUT_CONSTANT,
  
  GET_VARIABLE_X,
  GET_VARIABLE_Y,
  GET_VALUE_X,
  GET_VALUE_Y,
  GET_STRUCTURE_A,
  GET_STRUCTURE_X,
  GET_STRUCTURE_Y,
  GET_LIST,
  GET_CONSTANT,

  SET_VARIABLE_A,
  SET_VARIABLE_X,
  SET_VARIABLE_Y,
  SET_VALUE_A,
  SET_VALUE_X,
  SET_VALUE_Y,
  SET_LOCAL_VALUE_X,
  SET_LOCAL_VALUE_Y,
  SET_CONSTANT,
  SET_VOID,

  UNIFY_VARIABLE_A,
  UNIFY_VARIABLE_X,
  UNIFY_VARIABLE_Y,
  UNIFY_VALUE_A,
  UNIFY_VALUE_X,
  UNIFY_VALUE_Y,
  UNIFY_LOCAL_VALUE_X,
  UNIFY_LOCAL_VALUE_Y,
  UNIFY_CONSTANT,
  UNIFY_VOID,

  ALLOCATE,
  DEALLOCATE,
  CALL,
  EXECUTE,
  PROCEED,
  BUILTIN,
 
  TRY_ME_ELSE,
  RETRY_ME_ELSE,
  TRUST_ME,
  TRY,
  RETRY,
  TRUST,

  SWITCH_ON_TERM,
  SWITCH_ON_CONSTANT,
  SWITCH_ON_STRUCTURE,
 
  NECK_CUT,
  GET_LEVEL,
  CUT,

  LAST
};

class wam_interpreter;

typedef uint64_t code_t;

class wam_instruction_base;

typedef std::unordered_map<common::term, code_point> wam_hash_map;

template<wam_instruction_type I> class wam_instruction;

class wam_instruction_base
{
protected:
    typedef void (*fn_type)(wam_interpreter &interp, wam_instruction_base *self);
    inline wam_instruction_base(fn_type fn, uint64_t sz_bytes, wam_instruction_type t)
      : fn_(fn), type_(t), size_((sz_bytes+sizeof(code_t)-1)/sizeof(code_t)) { }

public:
    inline void invoke(wam_interpreter &interp) {
        fn_(interp, this);
    }

    inline fn_type fn() const { return fn_; }
    inline wam_instruction_type type() const { return type_; }
    inline size_t size() const {return size_; }
    inline size_t size_in_bytes() const { return size() * sizeof(code_t); }

    inline void set_type(fn_type fn, wam_instruction_type t) { fn_ = fn; type_ = t; }

    template<wam_instruction_type I> inline void set_type();

protected:
    inline void update_ptr(code_point &p, code_t *old_base, code_t *new_base)
    {
	if (p.wam_code() == nullptr) {
	    return;
	}
	p.set_wam_code(reinterpret_cast<wam_instruction_base *>(reinterpret_cast<code_t *>(p.wam_code()) - old_base + new_base));
    }

    inline void update(code_t *old_base, code_t *new_base)
    {
	auto it = updater_fns_.find(fn());
	if (it != updater_fns_.end()) {
	    auto updater_fn = it->second;
	    updater_fn(this, old_base, new_base);
	}
    }

private:
    fn_type fn_;
    wam_instruction_type type_;
    uint32_t size_;

    typedef void (*print_fn_type)(std::ostream &out, wam_interpreter &interp, wam_instruction_base *self);

    typedef void (*updater_fn_type)(wam_instruction_base *self, code_t *old_base, code_t *new_base);

public:
    static void register_printer(fn_type fn, print_fn_type print_fn)
    {
        print_fns_[fn] = print_fn;
    }

    static void register_updater(fn_type fn, updater_fn_type updater_fn)
    {
	updater_fns_[fn] = updater_fn;
    }

    void print(std::ostream &out, wam_interpreter &interp)
    {
        print_fn_type pfn = print_fns_[fn_];
	if (pfn == nullptr) {
	    std::cout << "???";
	} else {
	    pfn(out, interp, this);
	}
    }

private:
    static std::unordered_map<fn_type, print_fn_type> print_fns_;
    static std::unordered_map<fn_type, updater_fn_type> updater_fns_;

    friend class wam_code;
};

template<wam_instruction_type I> class wam_instruction : protected wam_instruction_base
{
public:
    inline wam_instruction(fn_type fn, size_t sz_bytes)
	: wam_instruction_base(fn, sz_bytes, I) { }
};

class wam_instruction_unary_reg : public wam_instruction_base
{
public:
    inline wam_instruction_unary_reg(fn_type fn, uint64_t sz_bytes, wam_instruction_type t, size_t reg)
          : wam_instruction_base(fn, sz_bytes, t),
	    data_(static_cast<uint32_t>(reg))
    {
    }

    inline size_t reg() const {
	return data_;
    }

    inline void set_reg(size_t t) {
	data_ = static_cast<uint32_t>(t);
    }

private:
    uint32_t data_;
};

class wam_instruction_binary_reg : public wam_instruction_base
{
public:
    inline wam_instruction_binary_reg(fn_type fn, uint64_t sz_bytes, wam_instruction_type t, uint32_t reg_1, uint32_t reg_2)
        : wam_instruction_base(fn, sz_bytes, t),
	  data_1_(reg_1),
	  data_2_(reg_2)
    {
    }

    inline size_t reg_1() const {
	return data_1_;
    }

    inline size_t reg_2() const {
	return data_2_;
    }

    inline void set_reg_1(size_t r) {
	data_1_ = r;
    }

    inline void set_reg_2(size_t r) {
	data_2_ = r;
    }

private:
    uint32_t data_1_;
    uint32_t data_2_;
};

class wam_instruction_con_reg : public wam_instruction_base
{
public:
    inline wam_instruction_con_reg(fn_type fn, uint64_t sz_bytes, wam_instruction_type t, common::con_cell c, uint32_t r)
        : wam_instruction_base(fn, sz_bytes, t),
	  con_(c),
	  reg_(r)
    {
    }

    inline common::con_cell con() const {
	return con_;
    }

    inline uint32_t reg() const {
	return reg_;
    }

    inline void set_con(common::con_cell c) {
	con_ = c;
    }

    inline void set_reg(uint32_t r) {
	reg_ = r;
    }

private:
    common::con_cell con_;
    uint32_t reg_;
};

class wam_instruction_term : public wam_instruction_base
{
public:
    inline wam_instruction_term(fn_type fn, uint64_t sz_bytes, wam_instruction_type t, common::term te)
        : wam_instruction_base(fn, sz_bytes, t),
	  term_(te)
    {
    }

    inline common::term get_term() const {
	return term_;
    }

    inline void set_term(common::term t) {
	term_ = t;
    }

private:
    common::term term_;
};

class wam_instruction_term_reg : public wam_instruction_term
{
public:
    inline wam_instruction_term_reg(fn_type fn, uint64_t sz_bytes, wam_instruction_type t, common::term te, uint32_t r)
        : wam_instruction_term(fn, sz_bytes, t, te), reg_(r)
    {
    }

    inline uint32_t reg() const {
	return reg_;
    }

    inline void set_reg(uint32_t r) {
	reg_ = r;
    }

private:
    uint32_t reg_;
};

class wam_instruction_code_point : public wam_instruction_base
{
public:
    inline wam_instruction_code_point(fn_type fn, uint64_t sz_bytes, wam_instruction_type t, const code_point &cp)
        : wam_instruction_base(fn, sz_bytes, t),
	  cp_(cp)
    {
    }

    inline const code_point & cp() const {
	return cp_;
    }

    inline code_point & cp() {
	return cp_;
    }

    inline void set_cp(const code_point &cp) {
	cp_ = cp;
    }

protected:
    static void updater(wam_instruction_base *self, code_t *old_base, code_t *new_base)
    {
	auto self1 = reinterpret_cast<wam_instruction_code_point *>(self);
	self1->update(old_base, new_base);
    }

private:
    code_point cp_;
};

class wam_instruction_code_point_reg : public wam_instruction_code_point
{
public:
    inline wam_instruction_code_point_reg(fn_type fn, uint64_t sz_bytes, wam_instruction_type t, const code_point &cp, uint32_t r)
	: wam_instruction_code_point(fn, sz_bytes, t, cp), reg_(r) { }

    inline uint32_t reg() const { return reg_; }
    void set_reg(uint32_t r) { reg_ = r; }

private:
    uint32_t reg_;
};

class wam_instruction_hash_map : public wam_instruction_base
{
public:
    inline wam_instruction_hash_map(fn_type fn, uint64_t sz_bytes, wam_instruction_type t, wam_hash_map *map)
	: wam_instruction_base(fn, sz_bytes, t), map_(map) { }

    inline wam_hash_map & map() const { return *map_; }

    inline void update(code_t *old_base, code_t *new_base)
    {
	for (auto &v : map()) {
	    update_ptr(v.second, old_base, new_base);
	}
    }

protected:
    static void updater(wam_instruction_base *self, code_t *old_base, code_t *new_base)
    {
	auto self1 = reinterpret_cast<wam_instruction_hash_map *>(self);
	self1->update(old_base, new_base);
    }

private:
    wam_hash_map *map_;
};

class wam_code
{
public:
    wam_code(wam_interpreter &interp,
  			     size_t initial_capacity = 1024)
	: interp_(interp), instrs_size_(0), instrs_capacity_(initial_capacity)
    { instrs_ = new code_t[instrs_capacity_]; }

    inline size_t next_offset() const
    {
	return instrs_size_;
    }
    
    inline size_t to_code_addr(code_t *p) const
    {
	return static_cast<size_t>(p - instrs_);
    }

    inline size_t to_code_addr(wam_instruction_base *p) const
    {
	return static_cast<size_t>(reinterpret_cast<code_t *>(p) - instrs_);
    }

    inline wam_instruction_base * to_code(size_t addr) const
    {
	return reinterpret_cast<wam_instruction_base *>(&instrs_[addr]);
    }

    size_t add(const wam_instruction_base &i);

    void print_code(std::ostream &out);

protected:
    void set_predicate(common::con_cell predicate_name,
		       wam_instruction_base *instr,
		       size_t environment_size)
    {
	size_t predicate_offset = to_code_addr(instr);
	predicate_map_[predicate_name] = predicate_offset;
	predicate_rev_map_[predicate_offset] = predicate_name;

	auto &offsets = calls_[predicate_name];
	for (auto offset : offsets) {
	    auto *cp_instr = reinterpret_cast<wam_instruction_code_point *>(to_code(offset));
	    cp_instr->cp().set_wam_code(instr);
	}
    }

    wam_instruction_base * resolve_predicate(common::con_cell predicate_name)
    {
	if (predicate_map_.count(predicate_name)) {
	    return to_code(predicate_map_[predicate_name]);
	} else {
	    return nullptr;
	}
    }

private:
    code_t * ensure_fit(size_t sz)
    {
        ensure_capacity(instrs_size_ + sz);
	code_t *data = &instrs_[instrs_size_];
	instrs_size_ += sz;
	return data;
    }

    void ensure_capacity(size_t cap)
    {
        if (cap > instrs_capacity_) {
   	    size_t new_cap = 2*std::max(cap, instrs_size_);
	    code_t *new_instrs = new code_t[new_cap];
	    memcpy(new_instrs, instrs_, sizeof(code_t)*instrs_size_);
	    instrs_capacity_ = new_cap;

	    update(instrs_, new_instrs);

	    delete instrs_;
	    instrs_ = new_instrs;
        }
    }

    inline void update(code_t *old_base, code_t *new_base);

    wam_interpreter &interp_;
    size_t instrs_size_;
    size_t instrs_capacity_;
    code_t *instrs_;

    std::unordered_map<common::con_cell, size_t> predicate_map_;
    std::unordered_map<size_t, common::con_cell> predicate_rev_map_;
    std::unordered_map<common::con_cell, std::vector<size_t> > calls_;
};

template<> class wam_instruction<CALL> : public wam_instruction_code_point_reg {
public:
    inline wam_instruction(common::con_cell l, uint32_t num_y) :
       wam_instruction_code_point_reg(&invoke, sizeof(*this), CALL, l, num_y) {
       init();
    }

    inline static void init() {
	static bool init_ = [] {
	    register_printer(&invoke, &print);
	    register_updater(&invoke, &updater);
	    return true; } ();
	static_cast<void>(init_);
    }

    inline void update(code_t *old_base, code_t *new_base)
    {
	update_ptr(p(), old_base, new_base);
    }

    inline const code_point & p() const { return cp(); }
    inline code_point & p() { return cp(); }

    inline common::con_cell pn() const
    { auto c = p().term_code();
      common::con_cell &cc = static_cast<common::con_cell &>(c);
      return cc;
    }

    inline size_t arity() const { return pn().arity(); }
    inline size_t num_y() const { return reg(); }

    inline void set_num_y(size_t n) { set_reg(n); }

    static void invoke(wam_interpreter &interp, wam_instruction_base *self);

    static void print(std::ostream &out, wam_interpreter &interp, wam_instruction_base *self);

    static void updater(wam_instruction_base *self, code_t *old_base, code_t *new_base);
};

template<> class wam_instruction<BUILTIN> : public wam_instruction_base {
public:
    inline wam_instruction(common::con_cell f, builtin b) :
      wam_instruction_base(&invoke, sizeof(*this), BUILTIN),
      f_(f), bn_(b) {
       init();
    }

    inline static void init() {
	static bool init_ = [] {
	    register_printer(&invoke, &print);
	    return true; } ();
	static_cast<void>(init_);
    }

    inline common::con_cell f() const { return f_; }
    inline builtin bn() const { return bn_; }
    inline size_t arity() const { return f().arity(); }

    static void invoke(wam_interpreter &interp, wam_instruction_base *self);

    static void print(std::ostream &out, wam_interpreter &interp, wam_instruction_base *self);
private:
    common::con_cell f_;
    builtin bn_;
};

class wam_interpreter : public interpreter_base, public wam_code
{
public:
    wam_interpreter();
    ~wam_interpreter();

    typedef common::term term;

    inline wam_hash_map * new_hash_map()
    {
	auto *map = new wam_hash_map();
	hash_maps_.push_back(map);
	return map;
    }

    inline std::string to_string(const term t,
				 common::term_emitter::style style
			  	    = common::term_emitter::STYLE_TERM) const
    {
	return interpreter_base::to_string(t, style);
    }

    inline wam_instruction_base * p()
    {
        return register_p_.wam_code();
    }

    inline void set_p(wam_instruction_base *instr)
    {
        register_p_.set_wam_code(instr);
    }

    inline bool execute_wam()
    {
        allocate_environment(true);
	return cont_wam();
    }

protected:
    inline void backtrack_wam()
    {
	backtrack();
	if (!is_top_fail()) {
	    set_e(b()->ce);
	}
    }

    inline bool cont_wam()
    {
        while (e_is_wam() && !is_top_fail()) {
	    if (auto instr = p()) {
		if (is_debug()) {
		    std::cout << "[WAM debug]: [" << std::setw(5)
			      << to_code_addr(instr) << "]: ";
		    instr->print(std::cout, *this);
		    std::cout << "\n";
		}
		instr->invoke(*this);
	    } else {
		if (is_debug()) {
		    std::cout << "[WAM debug]: success\n";
		}
		// We assume that WAM always created an environment
		// frame before execution.
		deallocate_environment();
		return true;
	    }
	}
	std::cout << "[WAM debug]: fail\n";
	if (b() != nullptr) {
	    reset_to_choice_point(b());
	}
	return false;
    }

private:

    inline std::string to_string(const code_point &cp) const
    {
	if (cp.wam_code() != nullptr) {
	    size_t offset = to_code_addr(cp.wam_code());
	    return "[" + boost::lexical_cast<std::string>(offset) + "]";
	} else {
	    std::string s("L:");
	    s += to_string(cp.term_code());
	    return s;
	}
    }

    template<wam_instruction_type I> friend class wam_instruction;

    static inline size_t num_y(environment_base_t *e)
    {
        auto after_call = e->cp.wam_code();
        if (after_call == nullptr) {
	    // TODO: Fix proper value here...
	    size_t n = 10;
	    return n;
        } else {
	    auto at_call = reinterpret_cast<wam_instruction<CALL> *>(
		 reinterpret_cast<code_t *>(after_call) -
		 sizeof(wam_instruction<CALL>)/sizeof(code_t));

	    (void)at_call;
	    size_t n = 10; // at_call->num_y();
	    return n;
	}
    }

    inline term & x(size_t i)
    {
        return register_xn_[i];
    }

    inline term & y(size_t i)
    {
        return e()->yn[i];
    }

    inline void backtrack()
    {
        if (b() == top_b()) {
	    set_top_fail(true);
	    if (is_debug()) {
	        std::cout << "[WAM debug]: backtrack(): Top fail\n";
	    }
        } else {
	    if (is_debug()) {
	        std::cout << "[WAM debug]: backtrack(): fail\n";
	    }
	    set_b0(b()->b0);
	    register_p_ = b()->bp;
	}
    }

    inline void tidy_trail()
    {
        size_t i = b()->tr;
	size_t tr = trail_size();
	size_t bb = to_stack_addr(base(b()));
	while (i < tr) {
	    if (trail_get(i) < get_register_hb() ||
		(heap_size() < trail_get(i) && trail_get(i) < bb)) {
		i++;
	    } else {
		trail_set(i, trail_get(tr-1));
		tr--;
	    }
	}
    }

    inline void unwind_trail(size_t a1, size_t a2)
    {
	for (size_t i = a1; i < a2; i++) {
	    size_t index = trail_get(i);
	    if (is_stack(index)) {
		*reinterpret_cast<term *>(to_stack(index))
		    = common::ref_cell(index);
	    } else {
		heap_set(index, common::ref_cell(index));
	    }
	}
    }

    inline void trail(size_t a)
    {
        size_t bb = to_stack_addr(base(b()));
	if (a < get_register_hb() || (is_stack(a) && a < bb)) {
	    push_trail(a);
	}
    }

    enum mode_t { READ, WRITE } mode_;

    code_point register_p_;
    size_t register_s_;

    std::vector<wam_hash_map *> hash_maps_;

    term register_xn_[1024];

  public:
    inline void next_instruction(code_point &p)
    {
        p.set_wam_code(reinterpret_cast<wam_instruction_base *>(reinterpret_cast<code_t *>(p.wam_code()) + p.wam_code()->size()));
    }

    inline wam_instruction_base *  next_instruction(wam_instruction_base *p)
    {
        return reinterpret_cast<wam_instruction_base *>(reinterpret_cast<code_t *>(p) + p->size());
    }


  private:

    inline void goto_next_instruction()
    {
        next_instruction(register_p_);
    }

    inline term deref_stack(common::ref_cell ref)
    {
        term t1 = to_stack(ref)->term;
        while (t1.tag() == common::tag_t::REF) {
  	    auto &ref1 = static_cast<common::ref_cell &>(t1);
	    if (!is_stack(ref)) {
	        return term_env::deref(t1);
	    }
	    term t2 = heap_get(ref1.index());
	    if (t1 == t2) {
	        return t1;
	    }
        }
	return t1;
    }

    inline term deref(term t)
    {
        if (t.tag() == common::tag_t::REF) {
  	    auto ref = static_cast<common::ref_cell &>(t);
	    if (is_stack(ref)) {
	        return deref_stack(ref);
	    } else {
	        return term_env::deref(t);
	    }
        } else {
	    return t;
	}
    }

    inline void bind(common::ref_cell &ref, term t)
    {
        if (is_stack(ref)) {
	    to_stack(ref)->term = t;
	    trail(ref.index());
        } else {
	    term_env::bind(ref, t);
	}
    }

    inline void put_variable_x(uint32_t xn, uint32_t ai)
    {
        term ref = new_ref();
	x(xn) = ref;
	a(ai) = ref;
	goto_next_instruction();
    }

    inline void put_variable_y(uint32_t yn, uint32_t ai)
    {
        term ref = new_ref();
	y(yn) = ref;
	a(ai) = ref;
	goto_next_instruction();
    }

    inline void put_value_x(uint32_t xn, uint32_t ai)
    {
        a(ai) = x(xn);
	goto_next_instruction();
    }

    inline void put_value_y(uint32_t yn, uint32_t ai)
    {
        a(ai) = y(yn);
	goto_next_instruction();
    }

    inline void put_unsafe_value_y(uint32_t yn, uint32_t ai)
    {
        term t = deref(y(yn));
	if (t.tag() != common::tag_t::REF) {
	    a(ai) = t;
	    goto_next_instruction();
	    return;
	}
	auto ref = static_cast<common::ref_cell &>(t);
	if (!is_stack(ref) || to_stack(ref) < base(e())) {
	    a(ai) = t;
        } else {
	    t = new_ref();
	    a(ai) = t;
	    bind(ref, t);
	}
	goto_next_instruction();
    }

    inline void put_structure_a(common::con_cell f, uint32_t ai)
    {
        a(ai) = new_term_con(f);
	goto_next_instruction();
    }

    inline void put_structure_x(common::con_cell f, uint32_t xn)
    {
        x(xn) = new_term_con(f);
	goto_next_instruction();
    }

    inline void put_structure_y(common::con_cell f, uint32_t yn)
    {
        y(yn) = new_term_con(f);
	goto_next_instruction();
    }
  
    inline void put_list(uint32_t ai)
    {
        a(ai) = new_dotted_pair();
	goto_next_instruction();
    }

    inline void put_constant(term c, uint32_t ai)
    {
        a(ai) = c;
	goto_next_instruction();
    }

    inline void get_variable_x(uint32_t xn, uint32_t ai)
    {
        x(xn) = a(ai);
	goto_next_instruction();
    }

    inline void get_variable_y(uint32_t yn, uint32_t ai)
    {
        y(yn) = a(ai);
	goto_next_instruction();
    }

    inline void get_value_x(uint32_t xn, uint32_t ai)
    {
        if (!unify(x(xn), a(ai))) {
	    backtrack();
        } else {
	    goto_next_instruction();
	}
    }

    inline void get_value_y(uint32_t yn, uint32_t ai)
    {
        if (!unify(y(yn), a(ai))) {
 	    backtrack();
        } else {
	    goto_next_instruction();
	}
    }

    inline void get_structure_a(common::con_cell f, uint32_t ai)
    {
        term t = deref(a(ai));

	return get_structure(f, t);
    }

    inline void get_structure_x(common::con_cell f, uint32_t xn)
    {
        term t = deref(x(xn));

	return get_structure(f, t);
    }

    inline void get_structure_y(common::con_cell f, uint32_t yn)
    {
        term t = deref(y(yn));
	return get_structure(f, t);
    }

    inline void get_structure(common::con_cell f, common::term t)
    {
        bool fail = false;
	switch (t.tag()) {
	case common::tag_t::REF: {
	    term s = new_term_str(f);
	    auto ref = static_cast<common::ref_cell &>(t);
	    bind(ref, s);
	    mode_ = WRITE;
	    break;
	  }
	case common::tag_t::STR: {
  	    auto str = static_cast<common::str_cell &>(t);
	    common::con_cell c = functor(str);
	    if (c == f) {
	        register_s_ = str.index() + 1;
		mode_ = READ;
	    } else {
	        fail = true;
	    }
	    break;
	 }
	default:
	  fail = true;
	  break;
	}
	if (fail) {
	    backtrack();
	} else {
	    goto_next_instruction();
	}
    }

    inline void get_list(uint32_t ai)
    {
        get_structure_a(empty_list_con(), ai);
    }

    inline void get_constant(common::term c, uint32_t ai)
    {
        bool fail = false;
        term t = deref(a(ai));
	switch (t.tag()) {
	case common::tag_t::REF: {
	  auto ref = static_cast<common::ref_cell &>(t);
	  bind(ref, c);
	  break;
  	  }
	case common::tag_t::CON: case common::tag_t::INT: {
	  auto c1 = static_cast<common::con_cell &>(t);
	  fail = c != c1;
	  break;
  	  }
	default:
	  fail = true;
	  break;
	}
	if (fail) {
	    backtrack();
	} else {
	    goto_next_instruction();
	}
    }

    inline void set_variable_a(uint32_t ai)
    {
        term t = new_ref();
	a(ai) = t;
	goto_next_instruction();
    }

    inline void set_variable_x(uint32_t xn)
    {
        term t = new_ref();
	x(xn) = t;
	goto_next_instruction();
    }

    inline void set_variable_y(uint32_t yn)
    {
        term t = new_ref();
	y(yn) = t;
	goto_next_instruction();
    }

    inline void set_value_a(uint32_t ai)
    {
        new_term_copy_cell(a(ai));
	goto_next_instruction();
    }

    inline void set_value_x(uint32_t xn)
    {
        new_term_copy_cell(x(xn));
	goto_next_instruction();
    }

    inline void set_value_y(uint32_t yn)
    {
        new_term_copy_cell(y(yn));
	goto_next_instruction();
    }

    inline void set_local_value_x(uint32_t xn)
    {
        term t = deref(x(xn));
	if (t.tag() == common::tag_t::REF) {
	    auto ref = static_cast<common::ref_cell &>(t);
	    if (is_stack(ref)) {
	        term h = new_ref();
		bind(ref, h);
	    } else {
  	        new_term_copy_cell(t);
	    }
	} else {
	    new_term_copy_cell(t);
	}
	goto_next_instruction();
    }

    inline void set_local_value_y(uint32_t yn)
    {
        term t = deref(y(yn));
	if (t.tag() == common::tag_t::REF) {
	    auto ref = static_cast<common::ref_cell &>(t);
	    if (is_stack(ref)) {
	        term h = new_ref();
		bind(ref, h);
	    } else {
  	        new_term_copy_cell(t);
	    }
	} else {
	    new_term_copy_cell(t);
	}
	goto_next_instruction();
    }

    inline void set_constant(common::term c)
    {
        new_term_copy_cell(c);
	goto_next_instruction();
    }

    inline void set_void(uint32_t n)
    {
        new_ref(n);
	goto_next_instruction();
    }

    inline void unify_variable_a(uint32_t ai)
    {
        switch (mode_) {
	case READ: a(ai) = heap_get(register_s_); break;
	case WRITE: a(ai) = new_ref(); break;
        }
	register_s_++;
	goto_next_instruction();
    }

    inline void unify_variable_x(uint32_t xn)
    {
        switch (mode_) {
	case READ: x(xn) = heap_get(register_s_); break;
	case WRITE: x(xn) = new_ref(); break;
        }
	register_s_++;
	goto_next_instruction();
    }

    inline void unify_variable_y(uint32_t yn)
    {
        switch (mode_) {
	case READ: y(yn) = heap_get(register_s_); break;
	case WRITE: y(yn) = new_ref(); break;
        }
	register_s_++;
	goto_next_instruction();
    }

    inline void unify_value_a(uint32_t ai)
    {
        bool fail = false;
        switch (mode_) {
	case READ: fail = !unify(a(ai), heap_get(register_s_)); break;
	case WRITE: new_term_copy_cell(a(ai)); break;
        }
	register_s_++;
	if (fail) {
	    backtrack();
	} else {
	    goto_next_instruction();
	}
    }

    inline void unify_value_x(uint32_t xn)
    {
        bool fail = false;
        switch (mode_) {
	case READ: fail = !unify(x(xn), heap_get(register_s_)); break;
	case WRITE: new_term_copy_cell(x(xn)); break;
        }
	register_s_++;
	if (fail) {
	    backtrack();
	} else {
	    goto_next_instruction();
	}
    }

    inline void unify_value_y(uint32_t yn)
    {
        bool fail = false;
        switch (mode_) {
	case READ: fail = !unify(y(yn), heap_get(register_s_)); break;
	case WRITE: new_term_copy_cell(y(yn)); break;
        }
	register_s_++;
	if (fail) {
	    backtrack();
	} else {
	    goto_next_instruction();
	}
    }

    inline void unify_local_value_x(uint32_t xn)
    {
        bool fail = false;
	switch (mode_) {
  	case READ: fail = !unify(x(xn), heap_get(register_s_)); break;
	case WRITE: {
	  term t = deref(x(xn));
	  if (t.tag() == common::tag_t::REF) {
	      auto ref = static_cast<common::ref_cell &>(t);
	      if (is_stack(ref)) {
		  auto h = new_ref();
		  bind(ref, h);
	      } else {
	  	  new_term_copy_cell(t);
	      }
	  } else {
  	      new_term_copy_cell(t);
	  }
	  break;
      	  }
	}
	register_s_++;
	if (fail) {
	    backtrack();
	} else {
	    goto_next_instruction();
	}
    }

    inline void unify_local_value_y(uint32_t xn)
    {
        bool fail = false;
	switch (mode_) {
  	case READ: fail = !unify(x(xn), heap_get(register_s_)); break;
	case WRITE: {
	  term t = deref(x(xn));
	  if (t.tag() == common::tag_t::REF) {
	      auto ref = static_cast<common::ref_cell &>(t);
	      if (is_stack(ref)) {
		  auto h = new_ref();
		  bind(ref, h);
	      } else {
	  	  new_term_copy_cell(t);
	      }
	  } else {
  	      new_term_copy_cell(t);
	  }
	  break;
      	  }
	}
	register_s_++;
	if (fail) {
	    backtrack();
	} else {
	    goto_next_instruction();
	}
    }

    inline void unify_constant(common::term c)
    {
        bool fail = false;
        switch (mode_) {
	case READ: {
	    term t = deref(heap_get(register_s_));
	    switch (t.tag()) {
	    case common::tag_t::REF: {
	        auto ref = static_cast<common::ref_cell &>(t);
		bind(ref, c);
		break;
	      }
	    case common::tag_t::CON: case common::tag_t::INT: {
	        auto c1 = static_cast<common::term &>(t);
		fail = c != c1;
		break;
	      }
	    default:
	      fail = true;
	      break;
	    }
          }
	case WRITE: {
	    new_term_copy_cell(c);
	    break;
	  }
	}
	if (fail) {
	    backtrack();
	} else {
	    goto_next_instruction();
	}
    }

    inline void unify_void(uint32_t n)
    {
        switch (mode_) {
	case READ: register_s_ += n; break;
	case WRITE: new_ref(n); break;
        }
	goto_next_instruction();
    }

    inline void allocate()
    {
        allocate_environment(true);
	goto_next_instruction();
    }

    inline void deallocate()
    {
        deallocate_environment();
	goto_next_instruction();
    }

    inline void call(code_point &p, size_t arity, uint32_t num_stack)
    {
        cp().set_wam_code(next_instruction(register_p_.wam_code()));
	set_num_of_args(arity);
	set_b0(b());
	register_p_ = p.wam_code();
    }

    inline void execute(code_point &p, size_t arity)
    {
        set_num_of_args(arity);
	set_b0(b());
	register_p_ = p.wam_code();
    }

    inline void proceed()
    {
        register_p_ = cp().wam_code();
    }

    inline bool builtin(wam_instruction_base *p)
    {
        auto b = reinterpret_cast<wam_instruction<BUILTIN> *>(p);
	size_t num_args = b->arity();
	set_num_of_args(num_args);
	goto_next_instruction();
	common::term args[num_args];
	for (size_t i = 0; i < num_args; i++) {
	    args[i] = a(i);
	}
	bool r = (b->bn())(*this, b->arity(), args);
	if (!r) {
	    backtrack();
	}
	return r;
    }

    void retry_choice_point(code_point &p_else)
    {
        size_t n = b()->arity;
	for (size_t i = 0; i < n; i++) {
  	    a(i) = b()->ai[i];
	}
	set_e(b()->ce);
	set_cp(b()->cp);
	b()->bp = p_else;
	unwind_trail(b()->tr, trail_size());
	trim_trail(b()->tr);
	trim_heap(b()->h);
	set_register_hb(heap_size());
    }

    void trust_choice_point()
    {
        size_t n = b()->arity;
	for (size_t i = 0; i < n; i++) {
	    a(i) = b()->ai[i];
	}
	set_e(b()->ce);
	set_cp(b()->cp);
	unwind_trail(b()->tr, trail_size());
	trim_trail(b()->tr);
	trim_heap(b()->h);
	set_b(b()->b);
	if (b() != nullptr) {
	    set_register_hb(b()->h);
	}
    }

    inline void try_me_else(code_point &L)
    {
	allocate_choice_point(L);
	goto_next_instruction();
    }

    inline void retry_me_else(code_point &L)
    {
	retry_choice_point(L);
	goto_next_instruction();
    }

    inline void trust_me()
    {
	trust_choice_point();
	goto_next_instruction();
    }

    inline void try_(code_point &L)
    {
        auto p = register_p_;
	next_instruction(p);
	allocate_choice_point(p);
	register_p_ = L;
    }

    inline void retry(code_point &L)
    {
        auto p = register_p_;
	next_instruction(p);
	retry_choice_point(p);
	register_p_ = L;
    }

    inline void trust(code_point &L)
    {
	trust_choice_point();
	register_p_ = L;
    }

    inline void switch_on_term(const code_point &pv,
			       const code_point &pc,
			       const code_point &pl,
			       const code_point &ps)
    {
	term t = deref(a(0));

	switch (t.tag()) {
	case common::tag_t::CON: case common::tag_t::INT:
	    if (pc.is_fail()) {
		backtrack();
	    } else {
		register_p_ = pc;
	    }
	    break;
	case common::tag_t::STR: {
	    if (is_dotted_pair(t)) {
	        if (pl.is_fail()) {
		    backtrack();
		} else {
		    register_p_ = pl;
		}
	    } else {
	        if (ps.is_fail()) {
		    backtrack();
		} else {
		    register_p_ = ps;
		}
	    }
	    break;
	}
	case common::tag_t::REF:
	default: {
	    if (pv.is_fail()) {
		backtrack();
	    } else {
		register_p_ = pv;
	    }
	    break;
	}
	}
    }

    inline void switch_on_constant(wam_hash_map &map)
    {
	term t = deref(a(0));
	auto it = map.find(t);
	if (it == map.end()) {
	    backtrack();
	} else {
	    register_p_ = it->second;
	}
    }

    inline void switch_on_structure(wam_hash_map &map)
    {
	term t = deref(a(0));
	auto it = map.find(t);
	if (it == map.end()) {
	    backtrack();
	} else {
	    register_p_ = it->second;
	}
    }

    inline void neck_cut()
    {
        if (b() > b0()) {
	    set_b(b0());
	    tidy_trail();
	}
	goto_next_instruction();
    }

    inline void get_level(uint32_t yn)
    {
        y(yn) = common::int_cell(to_stack_addr(base(b0())));
	goto_next_instruction();
    }

    inline void cut(uint32_t yn)
    {
        auto b0 = reinterpret_cast<choice_point_t *>(
	     to_stack(static_cast<common::int_cell &>(y(yn)).value()));
	if (b() > b0) {
	    set_b(b0);
	}
    }

    friend class test_wam_interpreter;
};

inline void wam_code::update(code_t *old_base, code_t *new_base)
{
    wam_instruction_base *instr = reinterpret_cast<wam_instruction_base *>(new_base);
    for (size_t i = 0; i < instrs_size_;) {
	instr->update(old_base, new_base);
	instr = interp_.next_instruction(instr);
	i = static_cast<size_t>(reinterpret_cast<code_t *>(instr) - new_base);
    }
}

template<> class wam_instruction<PUT_VARIABLE_X> : public wam_instruction_binary_reg {
public:
    inline wam_instruction(uint32_t xn, uint32_t ai) :
	wam_instruction_binary_reg(&invoke, sizeof(*this), PUT_VARIABLE_X,
				   xn, ai) {
        init();
    }

    inline uint32_t xn() const { return reg_1(); }
    inline uint32_t ai() const { return reg_2(); }
    inline void set_xn(size_t n) { set_reg_1(n); }

    inline static void init() {
	static bool init_ = [] {
	    register_printer(&invoke, &print);
	    return true; } ();
	static_cast<void>(init_);
    }

    static void invoke(wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<PUT_VARIABLE_X> *>(self);
        interp.put_variable_x(self1->xn(), self1->ai());
    }

    static void print(std::ostream &out, wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<PUT_VARIABLE_X> *>(self);
        out << "put_variable x" << self1->xn() << ", a" << self1->ai();
    }
};

template<> class wam_instruction<PUT_VARIABLE_Y> : public wam_instruction_binary_reg {
public:
    inline wam_instruction(uint32_t yn, uint32_t ai) :
	wam_instruction_binary_reg(&invoke, sizeof(*this), PUT_VARIABLE_Y,
				   yn, ai) {
        init();
    }

    static inline void init() {
	static bool init_ = [] {
	    register_printer(&invoke, &print);
	    return true; } ();
	static_cast<void>(init_);
    }

    inline uint32_t yn() const { return reg_1(); }
    inline uint32_t ai() const { return reg_2(); }

    static void invoke(wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<PUT_VARIABLE_Y> *>(self);
        interp.put_variable_y(self1->yn(), self1->ai());
    }

    static void print(std::ostream &out, wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<PUT_VARIABLE_Y> *>(self);
        out << "put_variable y" << self1->yn() << ", a" << self1->ai();
    }
};

template<> class wam_instruction<PUT_VALUE_X> : public wam_instruction_binary_reg {
public:
    inline wam_instruction(uint32_t xn, uint32_t ai) :
	wam_instruction_binary_reg(&invoke, sizeof(*this), PUT_VALUE_X,xn,ai) {
        init();
    }

    static inline void init() {
	static bool init_ = [] {
	    register_printer(&invoke, &print);
	    return true; } ();
	static_cast<void>(init_);
    }

    inline uint32_t xn() const { return reg_1(); }
    inline uint32_t ai() const { return reg_2(); }

    inline void set_xn(uint32_t n) { set_reg_1(n); }

    static void invoke(wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<PUT_VALUE_X> *>(self);
        interp.put_value_x(self1->xn(), self1->ai());
    }

    static void print(std::ostream &out, wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<PUT_VALUE_X> *>(self);
        out << "put_value x" << self1->xn() << ", a" << self1->ai();
    }
};

template<> class wam_instruction<PUT_VALUE_Y> : public wam_instruction_binary_reg {
public:
    inline wam_instruction(uint32_t yn, uint32_t ai) :
	wam_instruction_binary_reg(&invoke, sizeof(*this),
				   PUT_VALUE_Y, yn, ai) {
        init();
    }

    static inline void init() {
	static bool init_ = [] {
	    register_printer(&invoke, &print);
	    return true; } ();
	static_cast<void>(init_);
    }

    inline uint32_t yn() const { return reg_1(); }
    inline uint32_t ai() const { return reg_2(); }

    static void invoke(wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<PUT_VALUE_Y> *>(self);
        interp.put_value_y(self1->yn(), self1->ai());
    }

    static void print(std::ostream &out, wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<PUT_VALUE_Y> *>(self);
        out << "put_value y" << self1->yn() << ", a" << self1->ai();
    }
};

template<> class wam_instruction<PUT_UNSAFE_VALUE_Y> : public wam_instruction_binary_reg {
public:
    inline wam_instruction(uint32_t yn, uint32_t ai) :
	wam_instruction_binary_reg(&invoke, sizeof(*this),
				   PUT_UNSAFE_VALUE_Y,yn,ai) {
        init();
    }

    static inline void init() {
	static bool init_ = [] {
	    register_printer(&invoke, &print);
	    return true; } ();
	static_cast<void>(init_);
    }

    inline uint32_t yn() const { return reg_1(); }
    inline uint32_t ai() const { return reg_2(); }

    static void invoke(wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<PUT_UNSAFE_VALUE_Y> *>(self);
        interp.put_unsafe_value_y(self1->yn(), self1->ai());
    }

    static void print(std::ostream &out, wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<PUT_UNSAFE_VALUE_Y> *>(self);
        out << "put_unsafe_value y" << self1->yn() << ", a" << self1->ai();
    }
};

template<> class wam_instruction<PUT_STRUCTURE_A> : public wam_instruction_con_reg {
public:
    inline wam_instruction(common::con_cell f, uint32_t ai) :
	wam_instruction_con_reg(&invoke, sizeof(*this), PUT_STRUCTURE_A,
				f,ai) {
        init();
    }

    static inline void init() {
	static bool init_ = [] {
	    register_printer(&invoke, &print);
	    return true; } ();
	static_cast<void>(init_);
    }

    inline common::con_cell f() const { return con(); }
    inline size_t ai() const { return reg(); }

    static void invoke(wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<PUT_STRUCTURE_A> *>(self);
        interp.put_structure_a(self1->f(), self1->ai());
    }

    static void print(std::ostream &out, wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<PUT_STRUCTURE_A> *>(self);
        out << "put_structure " << interp.to_string(self1->f()) << "/" << self1->f().arity() << ", a" << self1->ai();
    }
};

template<> class wam_instruction<PUT_STRUCTURE_X> : public wam_instruction_con_reg {
public:
    inline wam_instruction(common::con_cell f, uint32_t xn) :
	wam_instruction_con_reg(&invoke, sizeof(*this), PUT_STRUCTURE_X,
				f, xn) {
        init();
    }

    static inline void init() {
	static bool init_ = [] {
	    register_printer(&invoke, &print);
	    return true; } ();
	static_cast<void>(init_);
    }

    inline common::con_cell f() const { return con(); }
    inline size_t xn() const { return reg(); }

    inline void set_xn(uint32_t n) { set_reg(n); }

    static void invoke(wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<PUT_STRUCTURE_X> *>(self);
        interp.put_structure_x(self1->f(), self1->xn());
    }

    static void print(std::ostream &out, wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<PUT_STRUCTURE_X> *>(self);
        out << "put_structure " << interp.to_string(self1->f()) << "/" << self1->f().arity() << ", x" << self1->xn();
    }
};

template<> class wam_instruction<PUT_STRUCTURE_Y> : public wam_instruction_con_reg {
public:
    inline wam_instruction(common::con_cell f, uint32_t yn) :
	wam_instruction_con_reg(&invoke, sizeof(*this), PUT_STRUCTURE_Y,f,yn) {
        init();
    }

    static inline void init() {
	static bool init_ = [] {
	    register_printer(&invoke, &print);
	    return true; } ();
	static_cast<void>(init_);
    }

    inline common::con_cell f() const { return con(); }
    inline size_t yn() const { return reg(); }

    static void invoke(wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<PUT_STRUCTURE_Y> *>(self);
        interp.put_structure_y(self1->f(), self1->yn());
    }

    static void print(std::ostream &out, wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<PUT_STRUCTURE_Y> *>(self);
        out << "put_structure " << interp.to_string(self1->f()) << "/" << self1->f().arity() << ", y" << self1->yn();
    }
};

template<> class wam_instruction<PUT_LIST> : public wam_instruction_unary_reg {
public:
    inline wam_instruction(uint32_t ai) :
	wam_instruction_unary_reg(&invoke, sizeof(*this), PUT_LIST, ai) {
        init();
    }

    static inline void init() {
	static bool init_ = [] {
	    register_printer(&invoke, &print);
	    return true; } ();
	static_cast<void>(init_);
    }

    inline uint32_t ai() const { return reg(); }

    static void invoke(wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<PUT_LIST> *>(self);
        interp.put_list(self1->ai());
    }

    static void print(std::ostream &out, wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<PUT_LIST> *>(self);
        out << "put_list " << "a" << self1->ai();
    }
};

template<> class wam_instruction<PUT_CONSTANT> : public wam_instruction_term_reg {
public:
    inline wam_instruction(common::term c, uint32_t ai) :
	wam_instruction_term_reg(&invoke, sizeof(*this), PUT_CONSTANT,c,ai) {
        init();
    }

    static inline void init() {
	static bool init_ = [] {
	    register_printer(&invoke, &print);
	    return true; } ();
	static_cast<void>(init_);
    }
    
    inline common::term c() const { return get_term(); }
    inline size_t ai() const { return reg(); }

    static void invoke(wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<PUT_CONSTANT> *>(self);
        interp.put_constant(self1->c(), self1->ai());
    }

    static void print(std::ostream &out, wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<PUT_CONSTANT> *>(self);
        out << "put_constant " << interp.to_string(self1->c()) << ", a" << self1->ai();
    }
};

template<> class wam_instruction<GET_VARIABLE_X> : public wam_instruction_binary_reg {
public:
    inline wam_instruction(uint32_t xn, uint32_t ai) :
	wam_instruction_binary_reg(&invoke, sizeof(*this),
				   GET_VARIABLE_X,xn,ai) {
        init();
    }

    static inline void init() {
	static bool init_ = [] {
	    register_printer(&invoke, &print);
	    return true; } ();
	static_cast<void>(init_);
    }

    inline uint32_t xn() const { return reg_1(); }
    inline uint32_t ai() const { return reg_2(); }
    inline void set_xn(uint32_t n) { set_reg_1(n); }

    static void invoke(wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<GET_VARIABLE_X> *>(self);
        interp.get_variable_x(self1->xn(), self1->ai());
    }

    static void print(std::ostream &out, wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<GET_VARIABLE_X> *>(self);
        out << "get_variable x" << self1->xn() << ", a" << self1->ai();
    }
};

template<> class wam_instruction<GET_VARIABLE_Y> : public wam_instruction_binary_reg {
public:
    inline wam_instruction(uint32_t yn, uint32_t ai) :
	wam_instruction_binary_reg(&invoke, sizeof(*this), GET_VARIABLE_Y,
				   yn, ai) {
        init();
    }

    static inline void init() {
	static bool init_ = [] {
	    register_printer(&invoke, &print);
	    return true; } ();
	static_cast<void>(init_);
    }

    inline uint32_t yn() const { return reg_1(); }
    inline uint32_t ai() const { return reg_2(); }

    static void invoke(wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<GET_VARIABLE_Y> *>(self);
        interp.get_variable_y(self1->yn(), self1->ai());
    }

    static void print(std::ostream &out, wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<GET_VARIABLE_Y> *>(self);
        out << "get_variable y" << self1->yn() << ", a" << self1->ai();
    }
};

template<> class wam_instruction<GET_VALUE_X> : public wam_instruction_binary_reg {
public:
    inline wam_instruction(uint32_t xn, uint32_t ai) :
	wam_instruction_binary_reg(&invoke, sizeof(*this), GET_VALUE_X,
				   xn,ai) {
        init();
    }

    static inline void init() {
	static bool init_ = [] {
	    register_printer(&invoke, &print);
	    return true; } ();
	static_cast<void>(init_);
    }

    inline uint32_t xn() const { return reg_1(); }
    inline uint32_t ai() const { return reg_2(); }
    inline void set_xn(uint32_t n) { set_reg_1(n); }

    static void invoke(wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<GET_VALUE_X> *>(self);
        interp.get_value_x(self1->xn(), self1->ai());
    }

    static void print(std::ostream &out, wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<GET_VALUE_X> *>(self);
        out << "get_value x" << self1->xn() << ", a" << self1->ai();
    }
};

template<> class wam_instruction<GET_VALUE_Y> : public wam_instruction_binary_reg {
public:
    inline wam_instruction(uint32_t yn, uint32_t ai) :
	wam_instruction_binary_reg(&invoke, sizeof(*this), GET_VALUE_Y,yn,ai) {
        init();
    }

    static inline void init() {
	static bool init_ = [] {
	    register_printer(&invoke, &print);
	    return true; } ();
	static_cast<void>(init_);
    }

    inline uint32_t yn() const { return reg_1(); }
    inline uint32_t ai() const { return reg_2(); }

    static void invoke(wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<GET_VALUE_Y> *>(self);
        interp.get_value_y(self1->yn(), self1->ai());
    }

    static void print(std::ostream &out, wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<GET_VALUE_Y> *>(self);
        out << "get_value y" << self1->yn() << ", a" << self1->ai();
    }
};

template<> class wam_instruction<GET_STRUCTURE_A> : public wam_instruction_con_reg {
public:
    inline wam_instruction(common::con_cell f, uint32_t ai) :
	wam_instruction_con_reg(&invoke, sizeof(*this), GET_STRUCTURE_A,f,ai) {
        init();
    }

    static inline void init() {
	static bool init_ = [] {
	    register_printer(&invoke, &print);
	    return true; } ();
	static_cast<void>(init_);
    }

    inline common::con_cell f() const { return con(); }
    inline size_t ai() const { return reg(); }

    static void invoke(wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<GET_STRUCTURE_A> *>(self);
        interp.get_structure_a(self1->f(), self1->ai());
    }

    static void print(std::ostream &out, wam_interpreter &interp, wam_instruction_base *self)
    {
      auto self1 = reinterpret_cast<wam_instruction<GET_STRUCTURE_A> *>(self);
        out << "get_structure " << interp.to_string(self1->f()) << "/" << self1->f().arity() << ", a" << self1->ai();
    }
};

template<> class wam_instruction<GET_STRUCTURE_X> : public wam_instruction_con_reg {
public:
    inline wam_instruction(common::con_cell f, uint32_t xn) :
	wam_instruction_con_reg(&invoke, sizeof(*this), GET_STRUCTURE_X,
				f,xn) {
        init();
    }

    static inline void init() {
	static bool init_ = [] {
	    register_printer(&invoke, &print);
	    return true; } ();
	static_cast<void>(init_);
    }

    inline common::con_cell f() const { return con(); }
    inline size_t xn() const { return reg(); }
    inline void set_xn(uint32_t n) { set_reg(n); }

    static void invoke(wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<GET_STRUCTURE_X> *>(self);
        interp.get_structure_x(self1->f(), self1->xn());
    }

    static void print(std::ostream &out, wam_interpreter &interp, wam_instruction_base *self)
    {
      auto self1 = reinterpret_cast<wam_instruction<GET_STRUCTURE_X> *>(self);
        out << "get_structure " << interp.to_string(self1->f()) << "/" << self1->f().arity() << ", x" << self1->xn();
    }
};

template<> class wam_instruction<GET_STRUCTURE_Y> : public wam_instruction_con_reg {
public:
    inline wam_instruction(common::con_cell f, uint32_t yn) :
	wam_instruction_con_reg(&invoke, sizeof(*this), GET_STRUCTURE_Y,f,yn) {
        init();
    }

    static inline void init() {
	static bool init_ = [] {
	    register_printer(&invoke, &print);
	    return true; } ();
	static_cast<void>(init_);
    }

    inline common::con_cell f() const { return con(); }
    inline uint32_t yn() const { return reg(); }

    static void invoke(wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<GET_STRUCTURE_Y> *>(self);
        interp.get_structure_y(self1->f(), self1->yn());
    }

    static void print(std::ostream &out, wam_interpreter &interp, wam_instruction_base *self)
    {
      auto self1 = reinterpret_cast<wam_instruction<GET_STRUCTURE_Y> *>(self);
        out << "get_structure " << interp.to_string(self1->f()) << "/" << self1->f().arity() << ", y" << self1->yn();
    }
};

template<> class wam_instruction<GET_LIST> : public wam_instruction_unary_reg {
public:
    inline wam_instruction(uint32_t ai) :
	wam_instruction_unary_reg(&invoke, sizeof(*this), GET_LIST,ai) {
        init();
    }

    static inline void init() {
	static bool init_ = [] {
	    register_printer(&invoke, &print);
	    return true; } ();
	static_cast<void>(init_);
    }

    inline uint32_t ai() const { return reg(); }

    static void invoke(wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<GET_LIST> *>(self);
        interp.get_list(self1->ai());
    }

    static void print(std::ostream &out, wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<GET_LIST> *>(self);
        out << "get_list " << "a" << self1->ai();
    }
};

template<> class wam_instruction<GET_CONSTANT> : public wam_instruction_term_reg {
public:
    inline wam_instruction(common::term c, uint32_t ai) :
	wam_instruction_term_reg(&invoke, sizeof(*this), GET_CONSTANT,c,ai) {
        init();
    }

    static inline void init() {
	static bool init_ = [] {
	    register_printer(&invoke, &print);
	    return true; } ();
	static_cast<void>(init_);
    }
    
    inline common::term c() const { return get_term(); }
    inline size_t ai() const { return reg(); }

    static void invoke(wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<GET_CONSTANT> *>(self);
        interp.get_constant(self1->c(), self1->ai());
    }

    static void print(std::ostream &out, wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<GET_CONSTANT> *>(self);
        out << "get_constant " << interp.to_string(self1->c()) << ", a" << self1->ai();
    }
};

template<> class wam_instruction<SET_VARIABLE_A> : public wam_instruction_unary_reg {
public:
    inline wam_instruction(uint32_t ai) :
	wam_instruction_unary_reg(&invoke, sizeof(*this), SET_VARIABLE_A,ai) {
        init();
    }

    static inline void init() {
	static bool init_ = [] {
	    register_printer(&invoke, &print);
	    return true; } ();
	static_cast<void>(init_);
    }

    inline uint32_t ai() const { return reg(); }

    static void invoke(wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<SET_VARIABLE_A> *>(self);
        interp.set_variable_a(self1->ai());
    }

    static void print(std::ostream &out, wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<SET_VARIABLE_A> *>(self);
        out << "set_variable a" << self1->ai();
    }
};

template<> class wam_instruction<SET_VARIABLE_X> : public wam_instruction_unary_reg {
public:
    inline wam_instruction(uint32_t xn) :
	wam_instruction_unary_reg(&invoke, sizeof(*this), SET_VARIABLE_X,xn) {
        init();
    }

    static inline void init() {
	static bool init_ = [] {
	    register_printer(&invoke, &print);
	    return true; } ();
	static_cast<void>(init_);
    }

    inline uint32_t xn() const { return reg(); }
    inline void set_xn(uint32_t n) { set_reg(n); }

    static void invoke(wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<SET_VARIABLE_X> *>(self);
        interp.set_variable_x(self1->xn());
    }

    static void print(std::ostream &out, wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<SET_VARIABLE_X> *>(self);
        out << "set_variable x" << self1->xn();
    }
};

template<> class wam_instruction<SET_VARIABLE_Y> : public wam_instruction_unary_reg {
public:
    inline wam_instruction(uint32_t yn) :
	wam_instruction_unary_reg(&invoke, sizeof(*this), SET_VARIABLE_Y,yn) {
        init();
    }

    static inline void init() {
	static bool init_ = [] {
	    register_printer(&invoke, &print);
	    return true; } ();
	static_cast<void>(init_);
    }

    inline uint32_t yn() const { return reg(); }

    static void invoke(wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<SET_VARIABLE_Y> *>(self);
        interp.set_variable_y(self1->yn());
    }

    static void print(std::ostream &out, wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<SET_VARIABLE_Y> *>(self);
        out << "set_variable y" << self1->yn();
    }
};

template<> class wam_instruction<SET_VALUE_A> : public wam_instruction_unary_reg {
public:
    inline wam_instruction(uint32_t ai) :
	wam_instruction_unary_reg(&invoke, sizeof(*this), SET_VALUE_A, ai) {
        init();
    }

    static inline void init() {
	static bool init_ = [] {
	    register_printer(&invoke, &print);
	    return true; } ();
	static_cast<void>(init_);
    }

    inline uint32_t ai() const { return reg(); }

    static void invoke(wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<SET_VALUE_A> *>(self);
        interp.set_value_a(self1->ai());
    }

    static void print(std::ostream &out, wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<SET_VALUE_A> *>(self);
        out << "set_value a" << self1->ai();
    }
};

template<> class wam_instruction<SET_VALUE_X> : public wam_instruction_unary_reg {
public:
    inline wam_instruction(uint32_t xn) :
	wam_instruction_unary_reg(&invoke, sizeof(*this), SET_VALUE_X,xn) {
        init();
    }

    static inline void init() {
	static bool init_ = [] {
	    register_printer(&invoke, &print);
	    return true; } ();
	static_cast<void>(init_);
    }

    inline uint32_t xn() const { return reg(); }
    inline void set_xn(uint32_t n) { set_reg(n); }

    static void invoke(wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<SET_VALUE_X> *>(self);
        interp.set_value_x(self1->xn());
    }

    static void print(std::ostream &out, wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<SET_VALUE_X> *>(self);
        out << "set_value x" << self1->xn();
    }
};

template<> class wam_instruction<SET_VALUE_Y> : public wam_instruction_unary_reg {
public:
    inline wam_instruction(uint32_t yn) :
	wam_instruction_unary_reg(&invoke, sizeof(*this), SET_VALUE_Y, yn) {
        init();
    }

    static inline void init() {
	static bool init_ = [] {
	    register_printer(&invoke, &print);
	    return true; } ();
	static_cast<void>(init_);
    }

    inline uint32_t yn() const { return reg(); }

    static void invoke(wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<SET_VALUE_Y> *>(self);
        interp.set_value_y(self1->yn());
    }

    static void print(std::ostream &out, wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<SET_VALUE_Y> *>(self);
        out << "set_value y" << self1->yn();
    }
};

template<> class wam_instruction<SET_LOCAL_VALUE_X> : public wam_instruction_unary_reg {
public:
    inline wam_instruction(uint32_t xn) :
      wam_instruction_unary_reg(&invoke, sizeof(*this), SET_LOCAL_VALUE_X,xn) {
        init();
    }

    static inline void init() {
	static bool init_ = [] {
	    register_printer(&invoke, &print);
	    return true; } ();
	static_cast<void>(init_);
    }

    inline uint32_t xn() const { return reg(); }
    inline void set_xn(uint32_t n) { set_reg(n); }

    static void invoke(wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<SET_LOCAL_VALUE_X> *>(self);
        interp.set_local_value_x(self1->xn());
    }

    static void print(std::ostream &out, wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<SET_LOCAL_VALUE_X> *>(self);
        out << "set_local_value x" << self1->xn();
    }
};

template<> class wam_instruction<SET_LOCAL_VALUE_Y> : public wam_instruction_unary_reg {
public:
    inline wam_instruction(uint32_t yn) :
      wam_instruction_unary_reg(&invoke, sizeof(*this), SET_LOCAL_VALUE_Y,yn) {
        init();
    }

    static inline void init() {
	static bool init_ = [] {
	    register_printer(&invoke, &print);
	    return true; } ();
	static_cast<void>(init_);
    }

    inline uint32_t yn() const { return reg(); }

    static void invoke(wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<SET_LOCAL_VALUE_Y> *>(self);
        interp.set_local_value_y(self1->yn());
    }

    static void print(std::ostream &out, wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<SET_LOCAL_VALUE_Y> *>(self);
        out << "set_local_value y" << self1->yn();
    }
};

template<> class wam_instruction<SET_CONSTANT> : public wam_instruction_term {
public:
    inline wam_instruction(common::term t) :
	wam_instruction_term(&invoke, sizeof(*this), SET_CONSTANT,t) {
        init();
    }
    
    static inline void init() {
	static bool init_ = [] {
	    register_printer(&invoke, &print);
	    return true; } ();
	static_cast<void>(init_);
    }

    inline common::term c() const { return get_term(); }

    static void invoke(wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<SET_CONSTANT> *>(self);
        interp.set_constant(self1->c());
    }

    static void print(std::ostream &out, wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<SET_CONSTANT> *>(self);
        out << "set_constant " << interp.to_string(self1->c());
    }
};

template<> class wam_instruction<SET_VOID> : public wam_instruction_unary_reg {
public:
    inline wam_instruction(uint32_t n) :
	wam_instruction_unary_reg(&invoke, sizeof(*this), SET_VOID, n) {
        init();
    }

    static inline void init() {
	static bool init_ = [] {
	    register_printer(&invoke, &print);
	    return true; } ();
	static_cast<void>(init_);
    }

    inline uint32_t n() const { return reg(); }

    static void invoke(wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<SET_VOID> *>(self);
        interp.set_void(self1->n());
    }

    static void print(std::ostream &out, wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<SET_VOID> *>(self);
        out << "set_void " << self1->n();
    }
};

template<> class wam_instruction<UNIFY_VARIABLE_A> : public wam_instruction_unary_reg {
public:
    inline wam_instruction(uint32_t ai) :
      wam_instruction_unary_reg(&invoke, sizeof(*this), UNIFY_VARIABLE_A, ai) {
        init();
    }

    static inline void init() {
	static bool init_ = [] {
	    register_printer(&invoke, &print);
	    return true; } ();
	static_cast<void>(init_);
    }

    inline uint32_t ai() const { return reg(); }

    static void invoke(wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<UNIFY_VARIABLE_A> *>(self);
        interp.unify_variable_a(self1->ai());
    }

    static void print(std::ostream &out, wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<UNIFY_VARIABLE_A> *>(self);
        out << "unify_variable a" << self1->ai();
    }
};

template<> class wam_instruction<UNIFY_VARIABLE_X> : public wam_instruction_unary_reg {
public:
    inline wam_instruction(uint32_t xn) :
       wam_instruction_unary_reg(&invoke, sizeof(*this), UNIFY_VARIABLE_X,xn) {
        init();
    }

    static inline void init() {
	static bool init_ = [] {
	    register_printer(&invoke, &print);
	    return true; } ();
	static_cast<void>(init_);
    }

    inline uint32_t xn() const { return reg(); }
    inline void set_xn(uint32_t n) { set_reg(n); }

    static void invoke(wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<UNIFY_VARIABLE_X> *>(self);
        interp.unify_variable_x(self1->xn());
    }

    static void print(std::ostream &out, wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<UNIFY_VARIABLE_X> *>(self);
        out << "unify_variable x" << self1->xn();
    }
};

template<> class wam_instruction<UNIFY_VARIABLE_Y> : public wam_instruction_unary_reg {
public:
    inline wam_instruction(uint32_t yn) :
      wam_instruction_unary_reg(&invoke, sizeof(*this), UNIFY_VARIABLE_Y,yn) {
        init();
    }

    static inline void init() {
	static bool init_ = [] {
	    register_printer(&invoke, &print);
	    return true; } ();
	static_cast<void>(init_);
    }

    inline uint32_t yn() const { return reg(); }

    static void invoke(wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<UNIFY_VARIABLE_Y> *>(self);
        interp.unify_variable_y(self1->yn());
    }

    static void print(std::ostream &out, wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<UNIFY_VARIABLE_Y> *>(self);
        out << "unify_variable y" << self1->yn();
    }
};

template<> class wam_instruction<UNIFY_VALUE_A> : public wam_instruction_unary_reg {
public:
    inline wam_instruction(uint32_t ai) :
	wam_instruction_unary_reg(&invoke, sizeof(*this), UNIFY_VALUE_A,ai) {
        init();
    }

    static inline void init() {
	static bool init_ = [] {
	    register_printer(&invoke, &print);
	    return true; } ();
	static_cast<void>(init_);
    }

    inline uint32_t ai() const { return reg(); }

    static void invoke(wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<UNIFY_VALUE_A> *>(self);
        interp.unify_value_a(self1->ai());
    }

    static void print(std::ostream &out, wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<UNIFY_VALUE_A> *>(self);
        out << "unify_value a" << self1->ai();
    }
};

template<> class wam_instruction<UNIFY_VALUE_X> : public wam_instruction_unary_reg {
public:
    inline wam_instruction(uint32_t xn) :
	wam_instruction_unary_reg(&invoke, sizeof(*this), UNIFY_VALUE_X,xn) {
        init();
    }

    static inline void init() {
	static bool init_ = [] {
	    register_printer(&invoke, &print);
	    return true; } ();
	static_cast<void>(init_);
    }

    inline uint32_t xn() const { return reg(); }
    inline void set_xn(uint32_t n) { set_reg(n); }

    static void invoke(wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<UNIFY_VALUE_X> *>(self);
        interp.unify_value_x(self1->xn());
    }

    static void print(std::ostream &out, wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<UNIFY_VALUE_X> *>(self);
        out << "unify_value x" << self1->xn();
    }
};

template<> class wam_instruction<UNIFY_VALUE_Y> : public wam_instruction_unary_reg {
public:
    inline wam_instruction(uint32_t yn) :
	wam_instruction_unary_reg(&invoke, sizeof(*this), UNIFY_VALUE_Y,yn) {
        init();
    }

    static inline void init() {
	static bool init_ = [] {
	    register_printer(&invoke, &print);
	    return true; } ();
	static_cast<void>(init_);
    }

    inline uint32_t yn() const { return reg(); }

    static void invoke(wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<UNIFY_VALUE_Y> *>(self);
        interp.unify_value_y(self1->yn());
    }

    static void print(std::ostream &out, wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<UNIFY_VALUE_Y> *>(self);
        out << "unify_value y" << self1->yn();
    }
};

template<> class wam_instruction<UNIFY_LOCAL_VALUE_X> : public wam_instruction_unary_reg {
public:
    inline wam_instruction(uint32_t xn) :
     wam_instruction_unary_reg(&invoke, sizeof(*this), UNIFY_LOCAL_VALUE_X,xn){
        init();
    }

    static inline void init() {
	static bool init_ = [] {
	    register_printer(&invoke, &print);
	    return true; } ();
	static_cast<void>(init_);
    }

    inline uint32_t xn() const { return reg(); }
    inline void set_xn(uint32_t n) { set_reg(n); }

    static void invoke(wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<UNIFY_LOCAL_VALUE_X> *>(self);
        interp.unify_local_value_x(self1->xn());
    }

    static void print(std::ostream &out, wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<UNIFY_LOCAL_VALUE_X> *>(self);
        out << "unify_local_value x" << self1->xn();
    }
};

template<> class wam_instruction<UNIFY_LOCAL_VALUE_Y> : public wam_instruction_unary_reg {
public:
    inline wam_instruction(uint32_t yn) :
    wam_instruction_unary_reg(&invoke, sizeof(*this), UNIFY_LOCAL_VALUE_Y,yn){
        init();
    }

    static inline void init() {
	static bool init_ = [] {
	    register_printer(&invoke, &print);
	    return true; } ();
	static_cast<void>(init_);
    }

    inline uint32_t yn() const { return reg(); }

    static void invoke(wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<UNIFY_LOCAL_VALUE_Y> *>(self);
        interp.unify_local_value_y(self1->yn());
    }

    static void print(std::ostream &out, wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<UNIFY_LOCAL_VALUE_Y> *>(self);
        out << "unify_local_value y" << self1->yn();
    }
};

template<> class wam_instruction<UNIFY_CONSTANT> : public wam_instruction_term {
public:
    inline wam_instruction(common::term t) :
	wam_instruction_term(&invoke, sizeof(*this), UNIFY_CONSTANT, t) {
        init();
    }

    static inline void init() {
	static bool init_ = [] {
	    register_printer(&invoke, &print);
	    return true; } ();
	static_cast<void>(init_);
    }
    
    inline common::term c() const { return get_term(); }

    static void invoke(wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<UNIFY_CONSTANT> *>(self);
        interp.unify_constant(self1->c());
    }

    static void print(std::ostream &out, wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<UNIFY_CONSTANT> *>(self);
        out << "unify_constant " << interp.to_string(self1->c());
    }
};

template<> class wam_instruction<UNIFY_VOID> : public wam_instruction_unary_reg {
public:
    inline wam_instruction(uint32_t n) :
	wam_instruction_unary_reg(&invoke, sizeof(*this), UNIFY_VOID, n) {
        init();
    }

    static inline void init() {
	static bool init_ = [] {
	    register_printer(&invoke, &print);
	    return true; } ();
	static_cast<void>(init_);
    }

    inline uint32_t n() const { return reg(); }

    static void invoke(wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<UNIFY_VOID> *>(self);
        interp.unify_void(self1->n());
    }

    static void print(std::ostream &out, wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<UNIFY_VOID> *>(self);
        out << "unify_void " << self1->n();
    }
};

template<> class wam_instruction<ALLOCATE> : public wam_instruction_base {
public:
    inline wam_instruction() :
      wam_instruction_base(&invoke, sizeof(*this), ALLOCATE) {
      init();
    }

    static inline void init() {
	static bool init_ = [] {
	    register_printer(&invoke, &print);
	    return true; } ();
	static_cast<void>(init_);
    }

    static void invoke(wam_interpreter &interp, wam_instruction_base *self)
    {
	static_cast<void>(self);
        interp.allocate();
    }

    static void print(std::ostream &out, wam_interpreter &interp, wam_instruction_base *self)
    {
	static_cast<void>(interp);
	static_cast<void>(self);
        out << "allocate";
    }
};

template<> class wam_instruction<DEALLOCATE> : public wam_instruction_base {
public:
    inline wam_instruction() :
      wam_instruction_base(&invoke, sizeof(*this), DEALLOCATE) {
      init();
    }

    static inline void init() {
	static bool init_ = [] {
	    register_printer(&invoke, &print);
	    return true; } ();
	static_cast<void>(init_);
    }

    static void invoke(wam_interpreter &interp, wam_instruction_base *self)
    {
	static_cast<void>(self);
        interp.deallocate();
    }

    static void print(std::ostream &out, wam_interpreter &interp, wam_instruction_base *self)
    {
	static_cast<void>(interp);
	static_cast<void>(self);
        out << "deallocate";
    }
};

inline void wam_instruction<CALL>::invoke(wam_interpreter &interp, wam_instruction_base *self)
{
    auto self1 = reinterpret_cast<wam_instruction<CALL> *>(self);
    interp.call(self1->p(), self1->arity(), self1->num_y());
}

inline void wam_instruction<CALL>::print(std::ostream &out, wam_interpreter &interp, wam_instruction_base *self)
{
    auto self1 = reinterpret_cast<wam_instruction<CALL> *>(self);
    out << "call " << interp.to_string(self1->pn()) << "/" << self1->arity();
    if (self1->p().wam_code() != nullptr) {
	out << " " << interp.to_string(self1->p());
    }
    out << ", " << self1->num_y();
}

inline void wam_instruction<CALL>::updater(wam_instruction_base *self, code_t *old_base, code_t *new_base)
{
    auto self1 = reinterpret_cast<wam_instruction<CALL> *>(self);
    self1->update(old_base, new_base);
}

template<> class wam_instruction<EXECUTE> : public wam_instruction_code_point {
public:
    inline wam_instruction(code_point p) :
        wam_instruction_code_point(&invoke, sizeof(*this), EXECUTE, p) {
        init();
    }

    static inline void init() {
	static bool init_ = [] {
	    register_printer(&invoke, &print);
	    return true; } ();
	static_cast<void>(init_);
    }

    inline const code_point & p() const { return cp(); }
    inline code_point & p() { return cp(); }

    inline common::con_cell pn() const
    { auto c = p().term_code();
      common::con_cell &cc = static_cast<common::con_cell &>(c);
      return cc;
    }

    inline size_t arity() const { return pn().arity(); }

    static void invoke(wam_interpreter &interp, wam_instruction_base *self)
    {
	auto self1 = reinterpret_cast<wam_instruction<EXECUTE> *>(self);
	interp.execute(self1->p(), self1->arity());
    }

    static void print(std::ostream &out, wam_interpreter &interp, wam_instruction_base *self)
    {
	auto self1 = reinterpret_cast<wam_instruction<EXECUTE> *>(self);
	out << "execute " << interp.to_string(self1->pn()) << "/" << self1->arity();
	if (self1->p().wam_code() != nullptr) {
	    out << " " << interp.to_string(self1->p());
	}
    }
};

template<> class wam_instruction<PROCEED> : public wam_instruction_base {
public:
    inline wam_instruction() :
      wam_instruction_base(&invoke, sizeof(*this), PROCEED) {
      init();
    }

    static inline void init() {
	static bool init_ = [] {
	    register_printer(&invoke, &print);
	    return true; } ();
	static_cast<void>(init_);
    }

    static void invoke(wam_interpreter &interp, wam_instruction_base *self)
    {
	static_cast<void>(self);
        interp.proceed();
    }

    static void print(std::ostream &out, wam_interpreter &interp, wam_instruction_base *self)
    {
	static_cast<void>(interp);
	static_cast<void>(self);
        out << "proceed";
    }
};

inline void wam_instruction<BUILTIN>::invoke(wam_interpreter &interp, wam_instruction_base *self)
{
    auto self1 = reinterpret_cast<wam_instruction<BUILTIN> *>(self);
    interp.builtin(self1);
}

inline void wam_instruction<BUILTIN>::print(std::ostream &out, wam_interpreter &interp, wam_instruction_base *self)
{
    auto self1 = reinterpret_cast<wam_instruction<BUILTIN> *>(self);
    out << "builtin " << interp.to_string(self1->f()) << "/" << self1->arity();
}

template<> class wam_instruction<TRY_ME_ELSE> : public wam_instruction_code_point {
public:
    inline wam_instruction(code_point p) :
	  wam_instruction_code_point(&invoke, sizeof(*this), TRY_ME_ELSE, p) {
        init();
    }

    static inline void init() {
	static bool init = [] {
 	    register_printer(&invoke, &print);
 	    register_updater(&invoke, &updater);
	    return true; } ();
	static_cast<void>(init);
    }

    inline const code_point & p() const { return cp(); }
    inline code_point & p() { return cp(); }

    inline void update(code_t *old_base, code_t *new_base)
    {
	update_ptr(p(), old_base, new_base);
    }

    static void invoke(wam_interpreter &interp, wam_instruction_base *self)
    {
	auto self1 = reinterpret_cast<wam_instruction<TRY_ME_ELSE> *>(self);
	interp.try_me_else(self1->p());
    }

    static void print(std::ostream &out, wam_interpreter &interp, wam_instruction_base *self)
    {
	auto self1 = reinterpret_cast<wam_instruction<TRY_ME_ELSE> *>(self);
	out << "try_me_else " << interp.to_string(self1->p());
    }

    static void updater(wam_instruction_base *self, code_t *old_base, code_t *new_base)
    {
	auto self1 = reinterpret_cast<wam_instruction<TRY_ME_ELSE> *>(self);
	self1->update(old_base, new_base);
    }
};

template<> class wam_instruction<RETRY_ME_ELSE> : public wam_instruction_code_point {
public:
    inline wam_instruction(code_point p) :
	 wam_instruction_code_point(&invoke, sizeof(*this), RETRY_ME_ELSE, p) {
        init();
    }

    static inline void init() {
	static bool init = [] {
 	    register_printer(&invoke, &print);
 	    register_updater(&invoke, &updater);
	    return true; } ();
	static_cast<void>(init);
    }

    inline const code_point & p() const { return cp(); }
    inline code_point & p() { return cp(); }

    inline void update(code_t *old_base, code_t *new_base)
    {
        update_ptr(p(), old_base, new_base);
    }

    static void invoke(wam_interpreter &interp, wam_instruction_base *self)
    {
	auto self1 = reinterpret_cast<wam_instruction<RETRY_ME_ELSE> *>(self);
	interp.retry_me_else(self1->p());
    }

    static void print(std::ostream &out, wam_interpreter &interp, wam_instruction_base *self)
    {
	auto self1 = reinterpret_cast<wam_instruction<RETRY_ME_ELSE> *>(self);
	out << "retry_me_else " << interp.to_string(self1->p());
    }

    static void updater(wam_instruction_base *self, code_t *old_base, code_t *new_base)
    {
	auto self1 = reinterpret_cast<wam_instruction<RETRY_ME_ELSE> *>(self);
	self1->update(old_base, new_base);
    }
};

template<> class wam_instruction<TRUST_ME> : public wam_instruction_base {
public:
    inline wam_instruction() :
      wam_instruction_base(&invoke, sizeof(*this), TRUST_ME) {
      init();
    }

    static inline void init() {
	static bool init = [] {
 	    register_printer(&invoke, &print);
	    return true; } ();
	static_cast<void>(init);
    }

    static void invoke(wam_interpreter &interp, wam_instruction_base *self)
    {
	static_cast<void>(self);
        interp.trust_me();
    }

    static void print(std::ostream &out, wam_interpreter &interp, wam_instruction_base *self)
    {
	static_cast<void>(interp);
	static_cast<void>(self);
        out << "trust_me";
    }
};

template<> class wam_instruction<TRY> : public wam_instruction_code_point {
public:
    inline wam_instruction(code_point p) :
	  wam_instruction_code_point(&invoke, sizeof(*this), TRY, p) {
        init();
    }

    static inline void init() {
	static bool init = [] {
 	    register_printer(&invoke, &print);
 	    register_updater(&invoke, &updater);
	    return true; } ();
	static_cast<void>(init);
    }

    inline const code_point & p() const { return cp(); }
    inline code_point & p() { return cp(); }

    inline void update(code_t *old_base, code_t *new_base)
    {
	update_ptr(p(), old_base, new_base);
    }

    static void invoke(wam_interpreter &interp, wam_instruction_base *self)
    {
	auto self1 = reinterpret_cast<wam_instruction<TRY> *>(self);
	interp.try_(self1->p());
    }

    static void print(std::ostream &out, wam_interpreter &interp, wam_instruction_base *self)
    {
	auto self1 = reinterpret_cast<wam_instruction<TRY> *>(self);
	out << "try " << interp.to_string(self1->p());
    }

    static void updater(wam_instruction_base *self, code_t *old_base, code_t *new_base)
    {
	auto self1 = reinterpret_cast<wam_instruction<TRY> *>(self);
	self1->update(old_base, new_base);
    }
};

template<> class wam_instruction<RETRY> : public wam_instruction_code_point {
public:
    inline wam_instruction(code_point p) :
	  wam_instruction_code_point(&invoke, sizeof(*this), RETRY, p) {
        init();
    }

    static inline void init() {
	static bool init = [] {
 	    register_printer(&invoke, &print);
 	    register_updater(&invoke, &updater);
	    return true; } ();
	static_cast<void>(init);
    }

    inline const code_point & p() const { return cp(); }
    inline code_point & p() { return cp(); }

    inline void update(code_t *old_base, code_t *new_base)
    {
	update_ptr(p(), old_base, new_base);
    }

    static void invoke(wam_interpreter &interp, wam_instruction_base *self)
    {
	auto self1 = reinterpret_cast<wam_instruction<RETRY> *>(self);
	interp.retry(self1->p());
    }

    static void print(std::ostream &out, wam_interpreter &interp, wam_instruction_base *self)
    {
	auto self1 = reinterpret_cast<wam_instruction<RETRY> *>(self);
	out << "retry " << interp.to_string(self1->p());
    }

    static void updater(wam_instruction_base *self, code_t *old_base, code_t *new_base)
    {
	auto self1 = reinterpret_cast<wam_instruction<RETRY> *>(self);
	self1->update(old_base, new_base);
    }
};

template<> class wam_instruction<TRUST> : public wam_instruction_code_point {
public:
    inline wam_instruction(code_point p) :
	wam_instruction_code_point(&invoke, sizeof(*this), TRUST, p) {
        init();
    }

    static inline void init() {
	static bool init = [] {
 	    register_printer(&invoke, &print);
 	    register_updater(&invoke, &updater);
	    return true; } ();
	static_cast<void>(init);
    }

    inline const code_point & p() const { return cp(); }
    inline code_point & p() { return cp(); }

    inline void update(code_t *old_base, code_t *new_base)
    {
	update_ptr(p(), old_base, new_base);
    }

    static void invoke(wam_interpreter &interp, wam_instruction_base *self)
    {
	auto self1 = reinterpret_cast<wam_instruction<TRUST> *>(self);
	interp.trust(self1->p());
    }

    static void print(std::ostream &out, wam_interpreter &interp, wam_instruction_base *self)
    {
	auto self1 = reinterpret_cast<wam_instruction<TRUST> *>(self);
	out << "trust " << interp.to_string(self1->p());
    }

    static void updater(wam_instruction_base *self, code_t *old_base, code_t *new_base)
    {
	auto self1 = reinterpret_cast<wam_instruction<TRUST> *>(self);
	self1->update(old_base, new_base);
    }
};

template<> class wam_instruction<SWITCH_ON_TERM> : public wam_instruction_base {
public:
      inline wam_instruction(code_point pv,
			     code_point pc,
			     code_point pl,
			     code_point ps) :
      wam_instruction_base(&invoke, sizeof(*this), SWITCH_ON_TERM),
      pv_(pv), pc_(pc), pl_(pl), ps_(ps) {
      init();
    }

    static inline void init() {
	static bool init = [] {
 	    register_printer(&invoke, &print);
 	    register_updater(&invoke, &updater);
	    return true; } ();
	static_cast<void>(init);
    }

    inline code_point & pv() { return pv_; }
    inline code_point & pc() { return pc_; }
    inline code_point & pl() { return pl_; }
    inline code_point & ps() { return ps_; }

    inline void update(code_t *old_base, code_t *new_base)
    {
        update_ptr(pv_, old_base, new_base);
	update_ptr(pc_, old_base, new_base);
	update_ptr(pl_, old_base, new_base);
	update_ptr(ps_, old_base, new_base);
    }

    static void invoke(wam_interpreter &interp, wam_instruction_base *self)
    {
	auto self1 = reinterpret_cast<wam_instruction<SWITCH_ON_TERM> *>(self);
	interp.switch_on_term(self1->pv(), self1->pc(), self1->pl(), self1->ps());
    }

    static void print(std::ostream &out, wam_interpreter &interp, wam_instruction_base *self)
    {
	auto self1 = reinterpret_cast<wam_instruction<SWITCH_ON_TERM> *>(self);
	out << "switch_on_term ";
        if (self1->pv().is_fail()) {
	    out << "V->fail";
	} else {
	    out << "V->" << interp.to_string(self1->pv());
	}
	out << ", ";
	if (self1->pc().is_fail()) {
	    out << "C->fail";
	} else {
	    out << "C->" << interp.to_string(self1->pc());
	}
	out << ", ";
	if (self1->pl().is_fail()) {
	    out << "L->fail";
	} else {
  	    out << "L->" << interp.to_string(self1->pl());
	}
	out << ", ";
	if (self1->ps().is_fail()) {
	    out << "S->fail";
	} else {
	    out << "S->" << interp.to_string(self1->ps());
	}
    }

    static void updater(wam_instruction_base *self, code_t *old_base, code_t *new_base)
    {
	auto self1 = reinterpret_cast<wam_instruction<SWITCH_ON_TERM> *>(self);
	self1->update(old_base, new_base);
    }

    code_point pv_;
    code_point pc_;
    code_point pl_;
    code_point ps_;
};

template<> class wam_instruction<SWITCH_ON_CONSTANT> : public wam_instruction_hash_map {
public:
    inline wam_instruction(wam_hash_map *map) :
      wam_instruction_hash_map(&invoke, sizeof(*this), SWITCH_ON_CONSTANT,map){
        init();
    }

    static inline void init() {
	static bool init = [] {
 	    register_printer(&invoke, &print);
 	    register_updater(&invoke, &updater);
	    return true; } ();
	static_cast<void>(init);
    }

    static void invoke(wam_interpreter &interp, wam_instruction_base *self)
    {
	auto self1 = reinterpret_cast<wam_instruction<SWITCH_ON_CONSTANT> *>(self);
	interp.switch_on_constant(self1->map());
    }

    static void print(std::ostream &out, wam_interpreter &interp, wam_instruction_base *self)
    {
	auto self1 = reinterpret_cast<wam_instruction<SWITCH_ON_CONSTANT> *>(self);
	out << "switch_on_constant ";
	bool first = true;
	for (auto &v : self1->map()) {
	    if (!first) out << ", ";
	    out << interp.to_string(v.first) << "->" << interp.to_string(v.second);
	    first = false;
	}
    }

    wam_hash_map *map_;
};

template<> class wam_instruction<SWITCH_ON_STRUCTURE> : public wam_instruction_hash_map {
public:
    inline wam_instruction(wam_hash_map *map) :
        wam_instruction_hash_map(&invoke, sizeof(*this), SWITCH_ON_STRUCTURE,
				 map) {
        init();
    }

    static inline void init() {
	static bool init = [] {
 	    register_printer(&invoke, &print);
 	    register_updater(&invoke, &updater);
	    return true; } ();
	static_cast<void>(init);
    }

    static void invoke(wam_interpreter &interp, wam_instruction_base *self)
    {
	auto self1 = reinterpret_cast<wam_instruction<SWITCH_ON_STRUCTURE> *>(self);
	interp.switch_on_structure(self1->map());
    }

    static void print(std::ostream &out, wam_interpreter &interp, wam_instruction_base *self)
    {
	auto self1 = reinterpret_cast<wam_instruction<SWITCH_ON_STRUCTURE> *>(self);
	out << "switch_on_structure ";
	bool first = true;
	for (auto &v : self1->map()) {
	    if (!first) out << ", ";
	    auto str = static_cast<const common::str_cell &>(v.first);
	    auto f = interp.functor(str);
	    out << interp.to_string(v.first) << "/" << f.arity() << "->" << interp.to_string(v.second);
	    first = false;
	}
    }

    static void updater(wam_instruction_base *self, code_t *old_base, code_t *new_base)
    {
	auto self1 = reinterpret_cast<wam_instruction<SWITCH_ON_STRUCTURE> *>(self);
	self1->update(old_base, new_base);
    }
};

template<> class wam_instruction<NECK_CUT> : public wam_instruction_base {
public:
    inline wam_instruction() :
      wam_instruction_base(&invoke, sizeof(*this), NECK_CUT) {
      init();
    }

    static inline void init() {
	static bool init = [] {
 	    register_printer(&invoke, &print);
	    return true; } ();
	static_cast<void>(init);
    }

    static void invoke(wam_interpreter &interp, wam_instruction_base *self)
    {
	static_cast<void>(self);
        interp.neck_cut();
    }

    static void print(std::ostream &out, wam_interpreter &interp, wam_instruction_base *self)
    {
	static_cast<void>(interp);
	static_cast<void>(self);
        out << "neck_cut";
    }
};

template<> class wam_instruction<GET_LEVEL> : public wam_instruction_unary_reg {
public:
    inline wam_instruction(uint32_t yn) :
	wam_instruction_unary_reg(&invoke, sizeof(*this), GET_LEVEL, yn) {
        init();
    }

    static inline void init() {
	static bool init = [] {
 	    register_printer(&invoke, &print);
	    return true; } ();
	static_cast<void>(init);
    }

    inline uint32_t yn() const { return reg(); }

    static void invoke(wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<GET_LEVEL> *>(self);
        interp.get_level(self1->yn());
    }

    static void print(std::ostream &out, wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<GET_LEVEL> *>(self);
        out << "get_level y" << self1->yn();
    }
};

template<> class wam_instruction<CUT> : public wam_instruction_unary_reg {
public:
    inline wam_instruction(uint32_t yn) :
	wam_instruction_unary_reg(&invoke, sizeof(*this), CUT, yn) {
        init();
    }

    inline static void init() {
	static bool init = [] {
	    register_printer(&invoke, &print); return true; } ();
	static_cast<void>(init);
    }

    inline uint32_t yn() const { return reg(); }

    static void invoke(wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<CUT> *>(self);
        interp.cut(self1->yn());
    }

    static void print(std::ostream &out, wam_interpreter &interp, wam_instruction_base *self)
    {
        auto self1 = reinterpret_cast<wam_instruction<CUT> *>(self);
        out << "cut y" << self1->yn();
    }
};

template<wam_instruction_type I> inline void wam_instruction_base::set_type()
{
    wam_instruction<I>::init();
    set_type(&wam_instruction<I>::invoke, I);
}

}}

#endif
