#include "parser.h"
#include "pegtl.hpp"

#include <iostream>
#include <cassert>

using namespace tao::pegtl;

/*** Convenience types ***/
template <typename... Ts>
struct sseq : seq<pad<Ts, space>...> {};
template <typename... Ts>
struct sstar : star<pad<Ts, space>...> {};
template <typename... Ts>
struct sopt : opt<pad<Ts, space>...> {};
template <typename T>
struct cslist : opt<list<T, pad<one<','>, space>>> {};
struct spaces : plus<space> {};

/*** Values ***/
struct val_bool : sor<TAO_PEGTL_STRING("true"), TAO_PEGTL_STRING("false")> {};
struct val_float : seq<opt<one<'-'>>, plus<digit>, opt<one<'.'>, plus<digit>>> {};
struct val_int : seq<opt<one<'-'>>, plus<digit>> {};

/*** Keywords ***/
struct kw_trait : TAO_PEGTL_STRING("trait") {};
struct kw_properties : TAO_PEGTL_STRING("properties") {};
struct kw_always : TAO_PEGTL_STRING("always") {};

/*** Types ***/
struct ty_bool : TAO_PEGTL_STRING("bool") {};
struct ty_float : TAO_PEGTL_STRING("float") {};
struct ty_int : sseq<TAO_PEGTL_STRING("int"), sopt<one<'<'>, val_int, one<','>, val_int, one<'>'>>> {};
struct type : sor<ty_bool, ty_float, ty_int> {};

struct variable_decl : sseq<identifier, one<':'>, type> {};
struct properties : sseq<kw_properties, one<'{'>, cslist<variable_decl>, one<'}'>> {};
struct always : sseq<kw_always, one<'{'>, one<'}'>> {};
struct trait : sseq<kw_trait, one<'{'>, properties, always, one<'}'>> {};

struct program : sstar<trait> {};

void parser::run() {
	assert(analyze<program>() == 0);

	auto const root = parse_tree::parse<program>(string_input(input_file, ""));
	parse_tree::print_dot(std::cout, *root);

	return;
}