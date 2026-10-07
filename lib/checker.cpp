#include "checker.hpp"

#include <cstddef>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "Absyn.H"
#include "Printer.H"

namespace stella {
namespace detail {
struct Ty;
using T = std::shared_ptr<const Ty>;
struct Ty {
    enum Kind { Bool, Nat, Unit, Fun, Tuple, Record, Sum, Variant, List } kind;
    std::vector<T> parts;
    std::map<std::string, T> fields;
};
using Env = std::map<std::string, T>;
T make(Ty::Kind k, std::vector<T> parts = {}, std::map<std::string, T> fields = {}) {
    return std::make_shared<Ty>(Ty{k, std::move(parts), std::move(fields)});
}
const T boolean = make(Ty::Bool), natural = make(Ty::Nat), unit = make(Ty::Unit);
std::string show(T t) {
    if (t->kind == Ty::Bool)
        return "Bool";
    if (t->kind == Ty::Nat)
        return "Nat";
    if (t->kind == Ty::Unit)
        return "Unit";
    if (t->kind == Ty::Fun)
        return "fn(" + show(t->parts[0]) + ") -> " + show(t->parts[1]);
    if (t->kind == Ty::Sum)
        return "(" + show(t->parts[0]) + " + " + show(t->parts[1]) + ")";
    if (t->kind == Ty::List)
        return "[" + show(t->parts[0]) + "]";
    std::string result = t->kind == Ty::Variant ? "<|" : "{";
    bool first = true;
    for (auto p : t->parts) {
        if (!first)
            result += ", ";
        first = false;
        result += show(p);
    }
    for (auto &[name, p] : t->fields) {
        if (!first)
            result += ", ";
        first = false;
        result += name + ": " + show(p);
    }
    return result + (t->kind == Ty::Variant ? "|>" : "}");
}
bool equal(T a, T b) {
    if (a->kind != b->kind || a->parts.size() != b->parts.size() ||
        a->fields.size() != b->fields.size())
        return false;
    for (std::size_t i = 0; i < a->parts.size(); ++i)
        if (!equal(a->parts[i], b->parts[i]))
            return false;
    for (auto &[name, t] : a->fields) {
        auto it = b->fields.find(name);
        if (it == b->fields.end() || !equal(t, it->second))
            return false;
    }
    return true;
}
[[noreturn]] void fail(const std::string &code, const std::string &message, Expr *e = nullptr) {
    std::string text = "ERROR_" + code + ": " + message;
    if (e) {
        PrintAbsyn printer;
        text += "\nExpression: ";
        text += printer.print(e);
    }
    throw std::runtime_error(text);
}
void same(T actual, T expected, Expr *e) {
    if (!equal(actual, expected))
        fail("UNEXPECTED_TYPE_FOR_EXPRESSION",
             "Expected " + show(expected) + ", got " + show(actual), e);
}
T type(Type *t) {
    if (dynamic_cast<TypeBool *>(t))
        return boolean;
    if (dynamic_cast<TypeNat *>(t))
        return natural;
    if (dynamic_cast<TypeUnit *>(t))
        return unit;
    if (auto p = dynamic_cast<TypeFun *>(t))
        return make(Ty::Fun, {type(p->type_1), type(p->type_2)});
    if (auto p = dynamic_cast<TypeSum *>(t))
        return make(Ty::Sum, {type(p->type_1), type(p->type_2)});
    if (auto p = dynamic_cast<TypeList *>(t))
        return make(Ty::List, {type(p->type_)});
    if (auto p = dynamic_cast<TypeTuple *>(t)) {
        std::vector<T> parts;
        for (auto x : *p->listtype_)
            parts.push_back(type(x));
        return make(Ty::Tuple, parts);
    }
    auto record = dynamic_cast<TypeRecord *>(t);
    auto variant = dynamic_cast<TypeVariant *>(t);
    auto fields = record ? record->listfieldtype_ : variant->listfieldtype_;
    std::map<std::string, T> result;
    for (auto x : *fields) {
        auto p = static_cast<AFieldType *>(x);
        if (result.contains(p->name_))
            fail(record ? "DUPLICATE_RECORD_TYPE_FIELDS" : "DUPLICATE_VARIANT_TYPE_FIELDS",
                 "Repeated field " + p->name_);
        result[p->name_] = type(p->type_);
    }
    return make(record ? Ty::Record : Ty::Variant, {}, result);
}
T expr(Expr *, Env, T = nullptr);
void require(T t, Ty::Kind k, const std::string &code, Expr *e) {
    if (t->kind != k)
        fail(code, "Invalid type " + show(t), e);
}
// Bind a pattern and report which outer constructor it covers.
std::string pattern(Pattern *p, T t, Env &env) {
    if (auto x = dynamic_cast<PatternVar *>(p)) {
        env[x->name_] = t;
        return "*";
    }
    if (auto x = dynamic_cast<PatternInl *>(p)) {
        if (t->kind != Ty::Sum)
            fail("UNEXPECTED_PATTERN_FOR_TYPE", "inl pattern requires a sum");
        pattern(x->pattern_, t->parts[0], env);
        return "inl";
    }
    if (auto x = dynamic_cast<PatternInr *>(p)) {
        if (t->kind != Ty::Sum)
            fail("UNEXPECTED_PATTERN_FOR_TYPE", "inr pattern requires a sum");
        pattern(x->pattern_, t->parts[1], env);
        return "inr";
    }
    auto x = static_cast<PatternVariant *>(p);
    if (t->kind != Ty::Variant || !t->fields.contains(x->name_))
        fail("UNEXPECTED_PATTERN_FOR_TYPE", "Variant pattern does not belong to " + show(t));
    pattern(x->pattern_, t->fields.at(x->name_), env);
    return x->name_;
}
T expr(Expr *e, Env env, T expected) {
    auto finish = [&](T t) {
        if (expected)
            same(t, expected, e);
        return t;
    };
    if (dynamic_cast<ConstTrue *>(e) || dynamic_cast<ConstFalse *>(e))
        return finish(boolean);
    if (dynamic_cast<ConstUnit *>(e))
        return finish(unit);
    if (dynamic_cast<ConstInt *>(e))
        return finish(natural);
    if (auto p = dynamic_cast<Var *>(e)) {
        if (!env.contains(p->name_))
            fail("UNDEFINED_VARIABLE", "Undefined variable " + p->name_, e);
        return finish(env.at(p->name_));
    }
    if (auto p = dynamic_cast<If *>(e)) {
        expr(p->expr_1, env, boolean);
        T t = expr(p->expr_2, env, expected);
        expr(p->expr_3, env, t);
        return t;
    }
    if (auto p = dynamic_cast<Let *>(e)) {
        for (auto b : *p->listbinding_) {
            auto x = static_cast<ABinding *>(b);
            T t = expr(x->expr_, env);
            pattern(x->pattern_, t, env);
        }
        return expr(p->expr_, env, expected);
    }
    if (auto p = dynamic_cast<TypeAsc *>(e)) {
        T t = type(p->type_);
        expr(p->expr_, env, t);
        return finish(t);
    }
    if (auto p = dynamic_cast<Abstraction *>(e)) {
        if (expected)
            require(expected, Ty::Fun, "UNEXPECTED_LAMBDA", e);
        auto param = static_cast<AParam *>(p->param_);
        T arg = type(param->type_);
        if (expected && !equal(arg, expected->parts[0]))
            fail("UNEXPECTED_TYPE_FOR_PARAMETER",
                 "Expected " + show(expected->parts[0]) + ", got " + show(arg), e);
        env[param->name_] = arg;
        T result = expr(p->expr_, env, expected ? expected->parts[1] : nullptr);
        return make(Ty::Fun, {arg, result});
    }
    if (auto p = dynamic_cast<Application *>(e)) {
        T f = expr(p->expr_1, env);
        require(f, Ty::Fun, "NOT_A_FUNCTION", p->expr_1);
        expr(p->expr_2, env, f->parts[0]);
        return finish(f->parts[1]);
    }
    if (auto p = dynamic_cast<Tuple *>(e)) {
        if (expected) {
            require(expected, Ty::Tuple, "UNEXPECTED_TUPLE", e);
            if (expected->parts.size() != p->listexpr_->size())
                fail("UNEXPECTED_TUPLE_LENGTH", "Tuple length differs from " + show(expected), e);
        }
        std::vector<T> parts;
        for (std::size_t i = 0; i < p->listexpr_->size(); ++i)
            parts.push_back(expr((*p->listexpr_)[i], env, expected ? expected->parts[i] : nullptr));
        return make(Ty::Tuple, parts);
    }
    if (auto p = dynamic_cast<Record *>(e)) {
        if (expected)
            require(expected, Ty::Record, "UNEXPECTED_RECORD", e);
        std::set<std::string> names;
        for (auto f : *p->listfield_) {
            auto x = static_cast<AField *>(f);
            if (!names.insert(x->name_).second)
                fail("DUPLICATE_RECORD_FIELDS", "Repeated field " + x->name_, e);
        }
        if (expected) {
            for (auto &name : names)
                if (!expected->fields.contains(name))
                    fail("UNEXPECTED_RECORD_FIELDS", "Unexpected field " + name, e);
            for (auto &[name, t] : expected->fields)
                if (!names.contains(name))
                    fail("MISSING_RECORD_FIELDS", "Missing field " + name, e);
        }
        std::map<std::string, T> fields;
        for (auto f : *p->listfield_) {
            auto x = static_cast<AField *>(f);
            fields[x->name_] =
                expr(x->expr_, env, expected ? expected->fields.at(x->name_) : nullptr);
        }
        return make(Ty::Record, {}, fields);
    }
    if (auto p = dynamic_cast<DotTuple *>(e)) {
        T t = expr(p->expr_, env);
        require(t, Ty::Tuple, "NOT_A_TUPLE", p->expr_);
        if (p->integer_ < 1 || static_cast<std::size_t>(p->integer_) > t->parts.size())
            fail("TUPLE_INDEX_OUT_OF_BOUNDS", "Invalid tuple index " + std::to_string(p->integer_),
                 e);
        return finish(t->parts[p->integer_ - 1]);
    }
    if (auto p = dynamic_cast<DotRecord *>(e)) {
        T t = expr(p->expr_, env);
        require(t, Ty::Record, "NOT_A_RECORD", p->expr_);
        if (!t->fields.contains(p->name_))
            fail("UNEXPECTED_FIELD_ACCESS", "Missing field " + p->name_, e);
        return finish(t->fields.at(p->name_));
    }
    if (auto p = dynamic_cast<Inl *>(e)) {
        if (!expected)
            fail("AMBIGUOUS_SUM_TYPE", "Cannot infer the other summand", e);
        require(expected, Ty::Sum, "UNEXPECTED_INJECTION", e);
        expr(p->expr_, env, expected->parts[0]);
        return expected;
    }
    if (auto p = dynamic_cast<Inr *>(e)) {
        if (!expected)
            fail("AMBIGUOUS_SUM_TYPE", "Cannot infer the other summand", e);
        require(expected, Ty::Sum, "UNEXPECTED_INJECTION", e);
        expr(p->expr_, env, expected->parts[1]);
        return expected;
    }
    if (auto p = dynamic_cast<Variant *>(e)) {
        if (!expected)
            fail("AMBIGUOUS_VARIANT_TYPE", "Variant needs an expected type", e);
        require(expected, Ty::Variant, "UNEXPECTED_VARIANT", e);
        if (!expected->fields.contains(p->name_))
            fail("UNEXPECTED_VARIANT_LABEL", "Unknown label " + p->name_, e);
        expr(p->expr_, env, expected->fields.at(p->name_));
        return expected;
    }
    if (auto p = dynamic_cast<List *>(e)) {
        if (expected)
            require(expected, Ty::List, "UNEXPECTED_LIST", e);
        T element = expected ? expected->parts[0] : nullptr;
        if (!element && p->listexpr_->empty())
            fail("AMBIGUOUS_LIST", "Empty list needs an expected type", e);
        for (auto x : *p->listexpr_)
            element = expr(x, env, element);
        return make(Ty::List, {element});
    }
    if (auto p = dynamic_cast<ConsList *>(e)) {
        if (expected)
            require(expected, Ty::List, "UNEXPECTED_LIST", e);
        T element = expr(p->expr_1, env, expected ? expected->parts[0] : nullptr);
        T list = make(Ty::List, {element});
        expr(p->expr_2, env, list);
        return list;
    }
    Expr *listArg = nullptr;
    if (auto p = dynamic_cast<Head *>(e))
        listArg = p->expr_;
    if (auto p = dynamic_cast<Tail *>(e))
        listArg = p->expr_;
    if (auto p = dynamic_cast<IsEmpty *>(e))
        listArg = p->expr_;
    if (listArg) {
        T t = expr(listArg, env);
        require(t, Ty::List, "NOT_A_LIST", listArg);
        return finish(dynamic_cast<Head *>(e)   ? t->parts[0]
                      : dynamic_cast<Tail *>(e) ? t
                                                : boolean);
    }
    Expr *natArg = nullptr;
    if (auto p = dynamic_cast<Succ *>(e))
        natArg = p->expr_;
    if (auto p = dynamic_cast<Pred *>(e))
        natArg = p->expr_;
    if (auto p = dynamic_cast<IsZero *>(e))
        natArg = p->expr_;
    if (natArg) {
        expr(natArg, env, natural);
        return finish(dynamic_cast<IsZero *>(e) ? boolean : natural);
    }
    if (auto p = dynamic_cast<NatRec *>(e)) {
        expr(p->expr_1, env, natural);
        T t = expr(p->expr_2, env, expected);
        expr(p->expr_3, env, make(Ty::Fun, {natural, make(Ty::Fun, {t, t})}));
        return t;
    }
    if (auto p = dynamic_cast<Fix *>(e)) {
        T t = expr(p->expr_, env);
        require(t, Ty::Fun, "NOT_A_FUNCTION", p->expr_);
        same(t->parts[1], t->parts[0], e);
        return finish(t->parts[0]);
    }
    if (auto p = dynamic_cast<Match *>(e)) {
        if (p->listcase_->empty())
            fail("ILLEGAL_EMPTY_MATCHING", "Match has no cases", e);
        T input = expr(p->expr_, env), result = expected;
        std::set<std::string> covered;
        for (auto c : *p->listcase_) {
            auto x = static_cast<ACase *>(c);
            Env local = env;
            covered.insert(pattern(x->pattern_, input, local));
            result = expr(x->expr_, local, result);
        }
        bool exhaustive = covered.contains("*");
        if (input->kind == Ty::Sum)
            exhaustive |= covered.contains("inl") && covered.contains("inr");
        if (input->kind == Ty::Variant) {
            bool all = true;
            for (auto &[name, t] : input->fields)
                all &= covered.contains(name);
            exhaustive |= all;
        }
        if (!exhaustive)
            fail("NONEXHAUSTIVE_MATCH_PATTERNS", "Match does not cover " + show(input), e);
        return result;
    }
    throw std::runtime_error("Unsupported expression");
}
} // namespace detail

void check(AProgram *p) {
    using namespace detail;
    Env env;
    for (auto d : *p->listdecl_) {
        auto f = static_cast<DeclFun *>(d);
        auto arg = static_cast<AParam *>(f->param_);
        env[f->name_] = make(Ty::Fun, {type(arg->type_), type(f->type_)});
    }
    if (!env.contains("main"))
        fail("MISSING_MAIN", "Program has no main function");
    for (auto d : *p->listdecl_) {
        auto f = static_cast<DeclFun *>(d);
        auto arg = static_cast<AParam *>(f->param_);
        Env local = env;
        local[arg->name_] = type(arg->type_);
        expr(f->expr_, local, type(f->type_));
    }
}
} // namespace stella
