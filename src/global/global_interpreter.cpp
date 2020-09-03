#include "global_interpreter.hpp"
#include "builtins.hpp"
#include "../common/checked_cast.hpp"
#include "../ec/builtins.hpp"
#include "../coin/builtins.hpp"
#include "global.hpp"

using namespace prologcoin::common;
using namespace prologcoin::interp;

namespace prologcoin { namespace global {

global_interpreter::global_interpreter(global &g)
    : global_(g),
      current_block_index_(static_cast<size_t>(-2)),
      current_block_(nullptr),
      block_flusher_(),
      block_cache_(global::BLOCK_CACHE_SIZE / heap_block::MAX_SIZE / sizeof(cell), block_flusher_)
{
    init();
}

void global_interpreter::init()
{
    heap_setup_get_block_function( call_get_heap_block, this );

    init_from_heap_db();
    init_from_symbols_db();
    init_from_program_db();

    setup_updated_predicate_function(updated_predicate);
    heap_setup_new_atom_function( call_new_atom, this );
    heap_setup_modified_block_function( call_modified_heap_block, this );

    setup_standard_lib();
    set_retain_state_between_queries(true);

    // TODO: Remove this
    load_builtins_file_io();

    setup_consensus_lib(*this);

    setup_builtins();
}

void global_interpreter::total_reset()
{
    block_cache_.clear();

    naming_ = false;
    name_to_term_.clear();
    current_block_index_ = static_cast<size_t>(-2);
    current_block_ = nullptr;
    new_atoms_.clear();
    modified_blocks_.clear();
    updated_predicates_.clear();
    modified_closures_.clear();

    get_global().erase_db();
    interpreter::total_reset();
    init();
}

void global_interpreter::updated_predicate(interpreter_base &interp, const interp::qname &qn) {
    auto &gi = reinterpret_cast<global_interpreter &>(interp);
    gi.updated_predicate(qn);
}

void global_interpreter::setup_consensus_lib(interpreter &interp) {
  ec::builtins::load_consensus(interp);
  coin::builtins::load_consensus(interp);
  builtins::load(interp); // Overrides reward_2 from coin
    
  std::string lib = R"PROG(

%
% Transaction predicates
%

%
% tx/5
%

tx(CoinIn, Hash, Script, Args, CoinOut) :-
    functor(CoinIn, Functor, Arity),
    arg(1, CoinIn, V),
    ground(V),
    arg(2, CoinIn, X),
    var(X),
    cmove(CoinIn, CoinX),
    freeze(Hash,
           (call(Script, Hash, Args),
            CoinOut = CoinX)).

tx1(Hash,args(Signature,PubKey,PubKeyAddr)) :-
    ec:address(PubKey,PubKeyAddr),
    ec:validate(PubKey,Hash,Signature).

reward(PubKeyAddr) :-
    reward(_, Coin),
    tx(Coin, _, tx1, args(_,_,PubKeyAddr), _),
    increment_height.

)PROG";

    auto &tx_5 = interp.get_predicate(con_cell("user",0), con_cell("tx",5));
    std::cout << "global_interpreter: setup..." << std::endl;
    if (tx_5.empty()) {
	std::cout << "global_interpreter: load program..." << std::endl;
        // Nope, so load it
        interp.load_program(lib);
	auto &tx_5_verify = interp.get_predicate(con_cell("user",0), con_cell("tx",5));
	assert(!tx_5_verify.empty());
    }
    interp.compile();
}
  
bool global_interpreter::execute_goal(term t, bool and_undo) {
    if (and_undo) {
        allocate_choice_point(code_point::fail());
    }  
    bool r = execute(t);
    if (and_undo) unwind_to_top_choice_point();
    return r;
}

bool global_interpreter::execute_goal(buffer_t &serialized, bool and_undo)
{
    term_serializer ser(*this);

    size_t old_height = get_global().current_height();
    
    try {
	if (and_undo) {
	    allocate_choice_point(code_point::fail());
	}
	
        term goal = ser.read(serialized);

	if (naming_) {
	    std::unordered_set<std::string> seen;
	    // Scan all vars in goal, and set initial bindings
	    std::for_each( begin(goal),
		   end(goal),
		   [&](term t) {
		     if (t.tag().is_ref()) {
		           ref_cell r = static_cast<ref_cell &>(t).unwatch();
			   const std::string name = to_string(r);
			   if (!seen.count(name)) {
			       seen.insert(name);
			       if (name_to_term_.count(name)) {
				   unify(t, name_to_term_[name]);
			       } else {
				   name_to_term_[name] = t;
			       }
			   }
		       }
		   } );
	}

	if (!execute(goal)) {
	    get_global().set_height(old_height);
	    if (and_undo) {
	        reset();
	    }
	    return false;
	}
	serialized.clear();
	ser.write(serialized, goal);
	if (and_undo) {
	    get_global().set_height(old_height);	
	    reset();
	}
	return true;
    } catch (serializer_exception &ex) {
	get_global().set_height(old_height);
        throw ex;
    } catch (interpreter_exception &ex) {
	get_global().set_height(old_height);      
        throw ex;
    }
}

void global_interpreter::execute_cut() {
    set_b0(nullptr); // Set cut point to top level
    interpreter_base::cut();
    interpreter_base::clear_trail();
}
    
bool global_builtins::operator_clause_2(interpreter_base &interp0, size_t arity, term args[] )
{
    auto &interp = to_global(interp0);

    term head = args[0];
    term body = args[1];

    if (head.tag() != tag_t::STR || interp.functor(head) != con_cell("p",1)) {
        throw interpreter_exception_wrong_arg_type(":-/2: Head of clause must be 'p(Hash)'; was " + interp.to_string(head));
    }

    // Setup new environment and where to continue
    interp.allocate_environment<ENV_NAIVE>();
	
    interp.set_p(body);
    interp.set_cp(interpreter_base::EMPTY_LIST);

    return true;
}

void global_interpreter::setup_builtins()
{
    load_builtin(con_cell(":-",2), &global_builtins::operator_clause_2);
}

db::triedb_leaf * global_interpreter::find_block_db(size_t block_index)

{
    return get_global().heap_db().find(get_global().current_height(), block_index);
}

term global_interpreter::get_frozen_closure(size_t addr)
{
    auto height = get_global().current_height();
    auto it = modified_closures_.find(addr);
    if (it != modified_closures_.end()) {
	return it->second;
    }
    auto *leaf = get_global().closures_db().find(height, addr);
    if (leaf == nullptr) {
	return EMPTY_LIST;
    }
    assert(leaf->custom_data_size() == sizeof(uint64_t));
    return cell(db::read_uint64(leaf->custom_data()));
}

void global_interpreter::clear_frozen_closure(size_t addr)
{
    internal_clear_frozen_closure(addr);

    // Check if frozen closure is in modified list, then remove it
    auto it = modified_closures_.find(addr);
    if (it != modified_closures_.end()) {
	modified_closures_.erase(addr);
	return;
    }
    // We need to remove it from the database, so we annotate for it
    modified_closures_[addr] = term();
}

void global_interpreter::set_frozen_closure(size_t addr, term closure)
{
    internal_set_frozen_closure(addr, closure);
    modified_closures_[addr] = closure;
}

void global_interpreter::get_frozen_closures(size_t from_addr,
					     size_t to_addr,
					     size_t max_closures,
			     std::vector<std::pair<size_t,term> > &closures)
{
    size_t height = get_global().current_height();
    size_t k = max_closures;

    bool reversed = get_global().interp().heap_size() == from_addr &&
	            to_addr <= from_addr;

    const auto none = term();

    auto &closures_db = get_global().closures_db();
    
    auto it1 = reversed ? closures_db.end(height) : closures_db.begin(height, from_addr);
    if (reversed) --it1;
    size_t prev_last_addr = from_addr;
    size_t last_addr = from_addr;
    while (!it1.at_end() && k > 0) {
	auto &leaf = *it1;
	if (leaf.key() >= to_addr) {
	    break;
	}
	prev_last_addr = last_addr;
	last_addr = leaf.key();

	// Process modified closures in between
	if (reversed) {
	    auto it2 = modified_closures_.lower_bound(last_addr+1);
	    while (it2 != modified_closures_.end()) {
		if (it2->first >= prev_last_addr) {
		    break;
		}
		if (it2->second != none) {
		    closures.push_back(*it2);
		}
		++it2;
	    }
	} else {
	    auto it2 = modified_closures_.lower_bound(prev_last_addr+1);
	    while (it2 != modified_closures_.end()) {
		if (it2->first >= last_addr) {
		    break;
		}
		if (it2->second != none) {
		    closures.push_back(*it2);
		}
		++it2;
	    }
	}

	auto mod = modified_closures_.find(leaf.key());
	if (mod != modified_closures_.end()) {
	    ++it1;
	    if (mod->second == none) {
		continue;
	    }
	    k--;
	    closures.push_back(*mod);
	    continue;
	}

	assert(it1->custom_data_size() == sizeof(uint64_t));
	cell cl(db::read_uint64(it1->custom_data()));
	closures.push_back(std::make_pair(leaf.key(), cl));
	k--;
	if (reversed) --it1; else ++it1;
    }

    if (!reversed) {
	// At this point we check modified closures from last_addr
	auto mod = modified_closures_.lower_bound(last_addr);
	while (mod != modified_closures_.end() && k > 0) {
	    if (mod->second != none) {
		closures.push_back(*mod);
		k--;
	    }
	    ++mod;
	}
    } else if (last_addr > 0) {
	auto mod0 = modified_closures_.lower_bound(last_addr-1);
	std::map<size_t, term>::reverse_iterator mod(mod0);
	while (mod != modified_closures_.rend() && k > 0) {
	    if (mod->first < last_addr) {
		if (mod->second != none) {
		    closures.push_back(*mod);
		    k--;
		}
	    }
	    ++mod;
	}
    }
}

void global_interpreter::commit_symbols()
{
    size_t current_height = get_global().current_height();

    if (new_atoms_.empty()) {
         get_global().symbols_db().no_change(current_height);
	 return;
    }
    
    std::vector<uint8_t> buffer;

    auto buffer_ensure = [&](size_t n) {
         size_t p = buffer.size();
         buffer.resize(p+n);
	 return &buffer[p];
    };
    
    for (auto &a : new_atoms_) {
        const std::string &atom_name = a.first;
	size_t atom_index = a.second;
	auto *p = buffer_ensure(sizeof(uint32_t));
	// Store atom index for sanity check only (it's not really needed)
	db::write_uint32(p, checked_cast<uint32_t>(atom_index));
	// Store atom name length
	p = buffer_ensure(sizeof(uint32_t));
	db::write_uint32(p, checked_cast<uint32_t>(atom_name.size()));
	p = buffer_ensure(atom_name.size());
	memcpy(p, &atom_name[0], atom_name.size());
    }

    get_global().symbols_db().update(current_height, current_height,
				     &buffer[0], buffer.size());
    new_atoms_.clear();
}

void global_interpreter::commit_closures()
{
    size_t current_height = get_global().current_height();

    auto &closures_db = get_global().closures_db();

    if (modified_closures_.empty()) {
         closures_db.no_change(current_height);
	 return;
    }
    
    uint8_t buffer[sizeof(uint64_t)];
    const auto none = term();
    for (auto &cl : modified_closures_) {
	if (cl.second == none) {
	    closures_db.remove(current_height, cl.first);
	} else {
	    std::cout << "Writing " << cl.first << std::endl;
	    db::write_uint64(buffer, cl.second.raw_value());
	    closures_db.update(current_height, cl.first, buffer, sizeof(buffer));
	}
    }
    modified_closures_.clear();
}
    
void global_interpreter::commit_heap()
{
    std::vector<heap_block*> blocks;
    for (auto &e : modified_blocks_) {
        auto *block = e.second;
	blocks.push_back(block);
    }
    std::sort(blocks.begin(), blocks.end(),
	   [](heap_block *a, heap_block *b) { return a->index() < b->index();});
    for (auto *block : blocks) {
	heap_block_as_custom_data view(*block);
	view.write();
	// Write on disk and move block to block cache
	get_global().heap_db().update(get_global().current_height(), block->index(),
				      view.custom_data(), view.custom_data_size());
	block->clear_changed();
	block_cache_.insert(block->index(), block);
    }
    modified_blocks_.clear();
}

void global_interpreter::init_from_heap_db()
{
    auto &hdb = get_global().heap_db();
    if (hdb.is_empty()) {
        return;
    }
    size_t current_height = get_global().current_height();
    auto it = hdb.end(current_height);
    --it;
    assert(!it.at_end());
    auto &leaf = *it;
    auto block_index = leaf.key();
    auto &block = get_heap_block(block_index);
    size_t heap_size = block_index * heap_block::MAX_SIZE + block.size();
    heap_set_size(heap_size);
    set_head_block(&block);
}

void global_interpreter::init_from_symbols_db()
{
    auto &sdb = get_global().symbols_db();
    if (sdb.is_empty()) {
        return;
    }
    size_t current_height = get_global().current_height();
    auto it = sdb.begin(current_height);
    auto it_end = sdb.end(current_height);
    for (; it != it_end; ++it) {
        auto *data = it->custom_data();
	auto data_size = it->custom_data_size();
	for (size_t i = 0; i < data_size; ) {
  	    auto atom_index = static_cast<size_t>(db::read_uint32(&data[i]));
	    i += sizeof(uint32_t);
	    auto atom_name_size =static_cast<size_t>(db::read_uint32(&data[i]));
	    i += sizeof(uint32_t);
	    std::string atom_name(atom_name_size, ' ');
	    memcpy(&atom_name[0], &data[i], atom_name_size);
	    i += atom_name_size;
	    size_t resolved_index = resolve_atom_index(atom_name);
	    assert(atom_index == resolved_index);
	}
    }
}
    
void global_interpreter::init_from_program_db()
{
    // Load all from latest height

    auto &pdb = get_global().program_db();
    if (pdb.is_empty()) {
        return;
    }
    size_t current_height = get_global().current_height();
    auto it = pdb.begin(current_height);
    auto it_end = pdb.end(current_height);
    for (; it != it_end; ++it) {
        auto *data = it->custom_data();
	auto data_size = it->custom_data_size();
	for (size_t i = 0; i < data_size; ) {
	    auto raw_clause = db::read_uint64(&data[i]); i += sizeof(uint64_t);
	    cell clause(raw_clause);
	    load_clause(clause, interp::LAST_CLAUSE);
	}
    }
}
    
void global_interpreter::commit_program()

{    size_t current_height = get_global().current_height();

    if (updated_predicates_.empty()) {
         get_global().program_db().no_change(current_height);
	 return;
    }

    std::vector<uint8_t> buffer;

    auto buffer_ensure = [&](size_t n) {
         size_t p = buffer.size();
         buffer.resize(p+n);
	 return &buffer[p];
    };
    
    for (auto &qn : updated_predicates_) {
        auto &pred = get_predicate(qn);
	// First write predicate id (module and name)
	// Then number of clauses
	// Followed by the clauses (one cell for each clause)

	auto &clauses = pred.get_clauses();
	for (auto &mclause : clauses) {
	    auto t = mclause.clause();
	    auto *p = buffer_ensure(sizeof(uint64_t));
	    db::write_uint64(p, t.raw_value());
	}
    }

    get_global().program_db().update(current_height, current_height,
				     &buffer[0], buffer.size());
    updated_predicates_.clear();
}

    
    
}}
