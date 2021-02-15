#pragma once

#ifndef _node_local_interpreter_hpp
#define _node_local_interpreter_hpp

#include "../interp/interpreter.hpp"
#include "../common/term_serializer.hpp"
#include "../global/global.hpp"
#include <boost/filesystem.hpp>

namespace prologcoin { namespace node {

class self_node;
class local_interpreter;
class in_session_state;

class me_builtins {
public:
    using interpreter_base = interp::interpreter_base;
    using meta_context = interp::meta_context;
    using meta_reason_t = interp::meta_reason_t;
    using buffer_t = common::term_serializer::buffer_t;

    using term = common::term;

    static local_interpreter & to_local(interpreter_base &interp)
    { return reinterpret_cast<local_interpreter &>(interp); }

    static bool list_load_2(interpreter_base &interp, size_t arity, term args[] );

    static bool operator_clause_2(interpreter_base &interp, size_t arity, term args[]);

    static bool operator_at_impl(interpreter_base &interp, size_t arity, term args[], const std::string &name, interp::remote_execute_mode mode);
    static bool operator_at_2(interpreter_base &interp, size_t arity, term args[]);
    static bool operator_at_silent_2(interpreter_base &interp, size_t arity, term args[]);
    static bool operator_at_parallel_2(interpreter_base &interp, size_t arity, term args[]);    
    static bool operator_at_2_meta(interpreter_base &interp, const meta_reason_t &reason);

    // Version & name...
    static bool id_1(interpreter_base &interp, size_t arity, term args[]);
    static bool name_1(interpreter_base &interp, size_t arity, term args[]);
    static bool heartbeat_0(interpreter_base &interp, size_t arity, term args[]);
    static bool version_1(interpreter_base &interp, size_t arity, term args[]);
    static bool comment_1(interpreter_base &interp, size_t arity, term args[]);

    // Get data directory
    static bool datadir_1(interpreter_base &interp, size_t arity, term args[]);

    // Addresses & connections
    static bool peers_2(interpreter_base &interp, size_t arity, term args[]);

    static bool add_address_2(interpreter_base &interp, size_t arity, term args[]);
    static bool connections_0(interpreter_base &interp, size_t arity, term args[]);

    // Mailboxes
    static bool mailbox_1(interpreter_base &interp, size_t arity, term args[]);
    static bool send_2(interpreter_base &interp, size_t arity, term args[]);
    static bool check_mail_0(interpreter_base &interp, size_t arity, term args[]);

    // Funding
    static bool initial_funds_1(interpreter_base &interp, size_t arity, term args[]);
    static bool maximum_funds_1(interpreter_base &interp, size_t arity, term args[]);
    static bool new_funds_per_second_1(interpreter_base &interp, size_t arity, term args[]);
    static bool funds_1(interpreter_base &interp, size_t arity, term args[]);
    static bool nolimit_0(interpreter_base &interp, size_t arity, term args[]);

    // Commit to global state
    static std::set<global::meta_id> get_meta_ids(interpreter_base &interp, const std::string &name, term prefix_id);
    static global::meta_id get_meta_id(interpreter_base &interp, const std::string &name, term prefix_id);
    static bool drop_global_0(interpreter_base &interp, size_t arity, term args[]);
    static bool gstat_1(interpreter_base &interp, size_t arity, term args[]);
    static bool chain_0(interpreter_base &interp, size_t arity, term args[]);
    static bool chain_2(interpreter_base &interp, size_t arity, term args[]);
    static bool chain_3(interpreter_base &interp, size_t arity, term args[]);
    static bool advance_0(interpreter_base &interp, size_t arity, term args[]);
    static bool discard_0(interpreter_base &interp, size_t arity, term args[]);
    static bool switch_1(interpreter_base &interp, size_t arity, term args[]);
    static bool height_1(interpreter_base &interp, size_t arity, term args[]);
    static bool goals_2(interpreter_base &interp, size_t arity, term args[]);
    static bool meta_2(interpreter_base &interp, size_t arity, term args[]);
    static bool current_1(interpreter_base &interp, size_t arity, term args[]);

    static bool setup_commit_1(interpreter_base &interp, size_t arity, term args[]);
    static bool commit(local_interpreter &interp, buffer_t &buf, term t, bool naming);
    static bool commit_2(interpreter_base &interp, size_t arity, term args[]);
    static bool global_impl(interpreter_base &interp, size_t arity, term args[], bool silent);
    static bool global_1(interpreter_base &interp, size_t arity, term args[]);
    static bool global_silent_1(interpreter_base &interp, size_t arity, term args[]);

    // Fast sync primitives
    static bool fastsync_1(interpreter_base &interp, size_t arity, term args[]);

    static term build_leaf_term(interpreter_base &interp0, const db::merkle_leaf &br);
    static term build_tree_term(interpreter_base &interp0, const db::merkle_branch &br);
    static term db_get(interpreter_base &interp, const std::string &name,
		       size_t arity, term args[], bool compute_size_only);
    static bool db_get_5(interpreter_base &interp, size_t arity, term args[]);
    static bool db_size_5(interpreter_base &interp, size_t arity, term args[]);
    static bool db_key_5(interpreter_base &interp, size_t arity, term args[]);

    static std::pair<db::triedb *, db::root_id> get_db_root(interpreter_base &interp, term name, const global::meta_id &id);
    static bool set_hash(interpreter_base &interp0, term hash_term, db::merkle_node &mnode);
    static bool set_position(interpreter_base &interp0, term hash_term, db::merkle_node &mnode);
    static bool build_merkle_tree(interpreter_base &interp0, term t, db::merkle_branch &br);
    static bool build_merkle_tree(interpreter_base &interp0, term t, db::merkle_leaf &lf);
    static bool db_put_3(interpreter_base &interp, size_t arity, term args[]);
};

class local_interpreter_exception : public interp::interpreter_exception {
public:
    local_interpreter_exception(const std::string &msg) :
	interpreter_exception(msg) { }
};

class interpreter_exception_file_not_found : public local_interpreter_exception {
public:
    interpreter_exception_file_not_found(const std::string &msg) :
	local_interpreter_exception(msg) { }
};

class interpreter_exception_syntax_error : public local_interpreter_exception {
public:
    interpreter_exception_syntax_error(const std::string &msg) :
	local_interpreter_exception(msg) { }
};

class interpreter_exception_security : public local_interpreter_exception {
public:
    interpreter_exception_security(const std::string &msg) :
        local_interpreter_exception(msg) { }
};

class local_interpreter : public interp::interpreter {
public:
    using interperter_base = interp::interpreter_base;
    using term = common::term;

    local_interpreter(in_session_state &session);

    void ensure_initialized();

    bool reset();
    void local_reset();

    void load_file(const std::string &filename);
    void startup_file();

    inline in_session_state & session() { return session_; }

    inline const std::string & get_text_out() { return text_out_; }
    inline void reset_text_out() { text_out_.clear(); }

    inline void flush_standard_output()
    {
	add_text(standard_output_.str());
	standard_output_.str("");
    }

    inline std::ostream & out() {
	return standard_output_;
    }

    inline void add_text(const std::string &str)
    { if (!ignore_text()) { text_out_ += str; } }

    inline void set_ignore_text(bool ignore) { ignore_text_ = ignore; }
    inline bool ignore_text() const { return ignore_text_; }

    static const common::con_cell ME;
    static const common::con_cell COLON;
    static const common::con_cell COMMA;

private:
    self_node & self();
    bool is_root();
    void root_check(const std::string &name, size_t arity);

    friend class me_builtins;

    void setup_local_builtins();

    in_session_state &session_;
    bool initialized_;
    std::string text_out_;
    bool ignore_text_;
    std::stringstream standard_output_;
};

}}

#endif
