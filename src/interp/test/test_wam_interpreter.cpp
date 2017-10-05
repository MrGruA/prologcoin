#include <boost/algorithm/string.hpp>
#include "../../common/term_tools.hpp"
#include "../wam_interpreter.hpp"
#include "../wam_compiler.hpp"

using namespace prologcoin::common;
using namespace prologcoin::interp;

namespace prologcoin { namespace interp {

static void header( const std::string &str )
{
    std::cout << "\n";
    std::cout << "--- [" + str + "] " + std::string(60 - str.length(), '-') << "\n";
    std::cout << "\n";
}

static void check_gold(std::string actual, std::string expect)
{
    std::stringstream ss(actual);
    std::stringstream goldss(expect);

    while (!ss.eof()) {
        std::string actual;
        std::string expected;
	std::getline(ss, actual);
	std::getline(goldss, expected);
	std::cout << actual << std::endl;
	boost::trim(actual);
	boost::trim(expected);
	if (actual != expected) {
	    std::cout << "ACTUAL: " << actual << std::endl;
	    std::cout << "EXPECT: " << expected << std::endl;
	    assert(actual == expected);
	}
    }

}

class test_wam_compiler
{
public:
    test_wam_compiler() : interp_(), comp_(interp_) { }

    void test_flatten();
    void test_compile();
    void test_partition();

private:
    wam_interpreter interp_;
    wam_compiler comp_;
};

}}

void test_wam_compiler::test_flatten()
{
    term t = interp_.parse("p(f(X),h(Y,f(a)),Y).");
    auto fl = comp_.flatten(t, wam_compiler::COMPILE_PROGRAM, true);
    comp_.print_prims(fl);
}

static void test_flatten()
{
    header("test_flatten");

    test_wam_compiler test;
    test.test_flatten();
}

void test_wam_compiler::test_compile()
{
    term t = interp_.parse("p(Z,h(Z,W),f(W)).");

    std::cout << "Compile " << interp_.to_string(t) << " as QUERY:" << std::endl << std::endl;

    {
        wam_interim_code seq(interp_);
        comp_.compile_query_or_program(t,wam_compiler::COMPILE_QUERY,seq);
        seq.print(std::cout);
    }

    term t2 = interp_.parse("p(Z,h(Z,W),f(W)).");

    std::cout << std::endl << "Compile " << interp_.to_string(t2) << " as PROGRAM:" << std::endl << std::endl;

    {
        wam_interim_code seq(interp_);
	comp_.compile_query_or_program(t2,wam_compiler::COMPILE_PROGRAM,seq);
	seq.print(std::cout);
    }

    {
        std::string prog =
        R"PROG(
          [
            (append([X|Xs],Ys,[X|Zs]) :- append(Xs,Ys,Zs)),
             append([],Ys,Ys),
            (nrev([X|Xs],Ys) :- nrev(Xs,Ys0), append(Ys0,[X],Ys)),
             nrev([],[])
          ].)PROG";

	std::cout << std::endl << "Compile append & nrev" << std::endl << std::endl;

	try {
	    interp_.load_program(prog);
	} catch (std::runtime_error &err) {
	    std::cout << "Syntax error: " << err.what() << std::endl;
	    throw;
	}

	auto &clauses = interp_.get_predicate(con_cell("nrev",2));

	wam_interim_code seq(interp_);
	comp_.compile_clause(clauses[0], seq);

	seq.print(std::cout);
    }
}

void test_wam_compiler::test_partition()
{
    std::string prog =
        R"PROG(
          [
          (call(or(X,Y)) :- call(X)),
          (call(trace) :- trace),
          (call(or(X,Y)) :- call(Y)),
          (call(notrace) :- notrace),
          (call(nl) :- nl),
          (call(X) :- builtin(X)),
          (call(X) :- extern(X)),
          (call(call(X)) :- call(X)),
          call(repeat),
          (call(repeat) :- call(repeat)),
          call(true)
          ].

       )PROG";

    std::string gold =
        R"GOLD(Section 0:
   call(or(X, Y)) :- call(X)
   call(trace) :- trace
   call(or(X, Y)) :- call(Y)
   call(notrace) :- notrace
   call(nl) :- nl
Section 1:
   call(X) :- builtin(X)
Section 2:
   call(X) :- extern(X)
Section 3:
   call(call(X)) :- call(X)
   call(repeat)
   call(repeat) :- call(repeat)
   call(true)
)GOLD";

    std::string gold2 =
        R"GOLD(--- SECTION 0 ---------------
Section 0:
   call(or(X, Y)) :- call(X)
   call(or(X, Y)) :- call(Y)
Section 1:
   call(trace) :- trace
Section 2:
   call(notrace) :- notrace
Section 3:
   call(nl) :- nl
--- SECTION 1 ---------------
Section 0:
   call(X) :- builtin(X)
--- SECTION 2 ---------------
Section 0:
   call(X) :- extern(X)
--- SECTION 3 ---------------
Section 0:
   call(call(X)) :- call(X)
Section 1:
   call(repeat)
   call(repeat) :- call(repeat)
Section 2:
   call(true)
)GOLD";

    try {
        interp_.load_program(prog);
    } catch (syntax_exception &ex) {
      std::cout << "Syntax exception: " << ex.what() << ": " << interp_.to_string(ex.get_term()) << std::endl;
	throw ex;
    }

    auto &clauses = interp_.get_predicate(con_cell("call",1));
    
    auto p = comp_.partition_clauses_nonvar(clauses);

    std::stringstream ss;
    comp_.print_partition(ss, p);

    check_gold(ss.str(), gold);

    std::stringstream ss2;
    size_t i = 0;
    for (auto &cs : p) {
        ss2 << "--- SECTION " << i << " ---------------" << std::endl;
        auto p1 = comp_.partition_clauses_first_arg(cs);
	comp_.print_partition(ss2, p1);
	i++;
    }

    check_gold(ss2.str(), gold2);
}

static void test_compile()
{
    header("test_compile");

    test_wam_compiler test;
    test.test_compile();
}


static void test_instruction_sequence()
{
    header("test_instruction_sequence");

    wam_interpreter interp;
    interp.add(wam_instruction<PUT_VARIABLE_X>(1, 2));
    interp.add(wam_instruction<PUT_VARIABLE_X>(3, 4));
    interp.add(wam_instruction<PUT_VARIABLE_Y>(5, 6));
    interp.add(wam_instruction<PUT_VALUE_X>(6, 7));
    interp.add(wam_instruction<PUT_VALUE_Y>(8, 9));
    interp.add(wam_instruction<PUT_UNSAFE_VALUE_Y>(10, 11));
    interp.add(wam_instruction<PUT_STRUCTURE_A>(con_cell("f",2), 12));
    interp.add(wam_instruction<PUT_STRUCTURE_X>(con_cell("f",2), 11));
    interp.add(wam_instruction<PUT_STRUCTURE_Y>(con_cell("f",2), 10));
    interp.add(wam_instruction<PUT_LIST>(13));
    interp.add(wam_instruction<PUT_CONSTANT>(con_cell("foo",0), 14));
    interp.add(wam_instruction<PUT_CONSTANT>(int_cell(4711), 15));
    interp.add(wam_instruction<GET_VARIABLE_X>(16,17));
    interp.add(wam_instruction<GET_VARIABLE_Y>(16,17));
    interp.add(wam_instruction<GET_VALUE_X>(18,19));
    interp.add(wam_instruction<GET_VALUE_Y>(20,21));
    interp.add(wam_instruction<GET_STRUCTURE_A>(con_cell("ga",2), 22));
    interp.add(wam_instruction<GET_STRUCTURE_X>(con_cell("gx",2), 21));
    interp.add(wam_instruction<GET_STRUCTURE_Y>(con_cell("gy",2), 20));
    interp.add(wam_instruction<GET_LIST>(23));
    interp.add(wam_instruction<GET_CONSTANT>(con_cell("bar",0), 24));
    interp.add(wam_instruction<GET_CONSTANT>(int_cell(-123), 25));
    interp.add(wam_instruction<SET_VARIABLE_X>(26));
    interp.add(wam_instruction<SET_VARIABLE_Y>(27));
    interp.add(wam_instruction<SET_VALUE_X>(28));
    interp.add(wam_instruction<SET_VALUE_Y>(29));
    interp.add(wam_instruction<SET_LOCAL_VALUE_X>(30));
    interp.add(wam_instruction<SET_LOCAL_VALUE_Y>(31));
    interp.add(wam_instruction<SET_CONSTANT>(con_cell("xyz",0)));
    interp.add(wam_instruction<SET_CONSTANT>(int_cell(456)));
    interp.add(wam_instruction<SET_VOID>(8));
    interp.add(wam_instruction<UNIFY_VARIABLE_X>(32));
    interp.add(wam_instruction<UNIFY_VARIABLE_Y>(33));
    interp.add(wam_instruction<UNIFY_VALUE_X>(34));
    interp.add(wam_instruction<UNIFY_VALUE_Y>(35));
    interp.add(wam_instruction<UNIFY_LOCAL_VALUE_X>(36));
    interp.add(wam_instruction<UNIFY_LOCAL_VALUE_Y>(37));
    interp.add(wam_instruction<UNIFY_CONSTANT>(con_cell("abc",0)));
    interp.add(wam_instruction<UNIFY_CONSTANT>(int_cell(-789)));
    interp.add(wam_instruction<UNIFY_VOID>(9));
    interp.add(wam_instruction<ALLOCATE>());
    interp.add(wam_instruction<DEALLOCATE>());
    interp.add(wam_instruction<CALL>(con_cell("f",3), 10));
    interp.add(wam_instruction<EXECUTE>(con_cell("g",4)));
    interp.add(wam_instruction<PROCEED>());
    interp.add(wam_instruction<TRY_ME_ELSE>(int_cell(1)));
    interp.add(wam_instruction<RETRY_ME_ELSE>(int_cell(2)));
    interp.add(wam_instruction<TRUST_ME>());
    interp.add(wam_instruction<TRY>(int_cell(3)));
    interp.add(wam_instruction<RETRY>(int_cell(4)));
    interp.add(wam_instruction<TRUST>(int_cell(5)));
    interp.add(wam_instruction<SWITCH_ON_TERM>(int_cell(6),
					       int_cell(7),
					       code_point::fail(),
					       int_cell(8)));

    auto *hm1 = interp.new_hash_map();
    hm1->insert(std::make_pair(con_cell("f", 2), int_cell(126)));
    hm1->insert(std::make_pair(int_cell(1234), int_cell(207)));
    interp.add(wam_instruction<SWITCH_ON_CONSTANT>(hm1));

    auto *hm2 = interp.new_hash_map();
    hm2->insert(std::make_pair(con_cell("f", 2), int_cell(221)));
    hm2->insert(std::make_pair(con_cell("g", 3), int_cell(235)));
    interp.add(wam_instruction<SWITCH_ON_STRUCTURE>(hm2));

    interp.add(wam_instruction<NECK_CUT>());
    interp.add(wam_instruction<GET_LEVEL>(111));
    interp.add(wam_instruction<CUT>(112));

    interp.print_code(std::cout);
}

static void test_partition()
{
    header("test_partition");

    test_wam_compiler test;
    test.test_partition();
}

int main( int argc, char *argv[] )
{
    test_flatten();

    test_compile();

    test_instruction_sequence();

    test_partition();

    return 0;
}
