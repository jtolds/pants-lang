#ifndef __IR_H__
#define __IR_H__

#include "common.h"
#include "ast.h"

#define UPDATE_FIELD      ::pants::ir::Name("~update", true)
#define LOOKUP_FIELD      ::pants::ir::Name("~index", true)
#define DICT_CONSTRUCTOR  ::pants::ir::Name("Dictionary", true)
#define ARRAY_CONSTRUCTOR ::pants::ir::Name("Array", true)
#define NULL_VALUE        ::pants::ir::Name("null", false)
#define CONTINUATION      ::pants::ir::Name("continuation", false)
#define RETURN            ::pants::ir::Name("cont", true)
#define THROW             ::pants::ir::Name("throw", true)
#define DYNAMIC_VARS      ::pants::ir::Name("dynamic_vars", false)
#define GENSYM_PREFIX     "ir_"

namespace pants {
namespace ir {

  struct Assignment; struct ObjectMutation; struct ReturnValue;

  struct ExpressionVisitor {
    virtual void visit(Assignment*) = 0;
    virtual void visit(ObjectMutation*) = 0;
    virtual void visit(ReturnValue*) = 0;
  };

  struct Field; struct Variable; struct Integer; struct CharString;
  struct ByteString; struct Float; struct Function; struct Scope;

  struct ValueVisitor {
    virtual void visit(Field*) = 0;
    virtual void visit(Variable*) = 0;
    virtual void visit(Integer*) = 0;
    virtual void visit(CharString*) = 0;
    virtual void visit(ByteString*) = 0;
    virtual void visit(Float*) = 0;
    virtual void visit(Function*) = 0;
  };

  struct Expression {
    virtual ~Expression() {}
    virtual std::string format(unsigned int indent_level) const = 0;
    virtual void accept(ExpressionVisitor* visitor) = 0;
    protected: Expression() {} };

  struct Value {
    virtual ~Value() {}
    virtual std::string format(unsigned int indent_level) const = 0;
    virtual void accept(ValueVisitor* visitor) = 0;
    protected: Value() {} };

  struct Name {
    Name(const std::string& name_, bool user_provided_)
      : name(name_), user_provided(user_provided_) {}
    Name(const pants::ast::Variable& var)
      : name(var.name), user_provided(var.user_provided)
      { if(!name.size()) throw expectation_failure("expected variable name"); }
    std::string format(unsigned int indent_level = 0) const;
    std::string c_name() const;
    std::string name;
    bool user_provided;
    bool operator<(const Name& rhs) const {
      if (name < rhs.name) return true;
      if (rhs.name < name) return false;
      return user_provided < rhs.user_provided;
    }
    bool operator==(const Name& rhs) const
      { return name == rhs.name && user_provided == rhs.user_provided; }
  };

  struct Assignment : public Expression {
    Assignment(const Name& assignee_, PTR<Value> value_, bool local_)
      : assignee(assignee_), value(value_), local(local_) {}
    Name assignee;
    PTR<Value> value;
    bool local;
    std::string format(unsigned int indent_level) const;
    void accept(ExpressionVisitor* visitor) { visitor->visit(this); }
  };

  struct ObjectMutation : public Expression {
    ObjectMutation(const Name& object_, const Name& field_, const Name& value_)
      : object(object_), field(field_), value(value_) {}
    Name object;
    Name field;
    Name value;
    std::string format(unsigned int indent_level) const;
    void accept(ExpressionVisitor* visitor) { visitor->visit(this); }
  };

  struct Field : public Value {
    Field(const Name& object_, const Name& field_)
      : object(object_), field(field_) {}
    std::string format(unsigned int indent_level) const;
    Name object;
    Name field;
    void accept(ValueVisitor* visitor) { visitor->visit(this); }
  };

  struct Variable : public Value {
    Variable(const Name& variable_) : variable(variable_) {}
    Name variable;
    std::string format(unsigned int indent_level) const;
    void accept(ValueVisitor* visitor) { visitor->visit(this); }
  };

  struct Integer : public Value {
    Integer(const long long& value_) : value(value_) {}
    long long value;
    std::string format(unsigned int indent_level) const;
    void accept(ValueVisitor* visitor) { visitor->visit(this); }
  };

  struct CharString : public Value {
    CharString(const std::string& value_) : value(value_) {}
    std::string value;
    std::string format(unsigned int indent_level) const;
    void accept(ValueVisitor* visitor) { visitor->visit(this); }
  };

  struct ByteString : public Value {
    ByteString(const std::string& value_) : value(value_) {}
    std::string value;
    std::string format(unsigned int indent_level) const;
    void accept(ValueVisitor* visitor) { visitor->visit(this); }
  };

  struct Float : public Value {
    Float(const double& value_) : value(value_) {}
    double value;
    std::string format(unsigned int indent_level) const;
    void accept(ValueVisitor* visitor) { visitor->visit(this); }
  };

  struct PositionalOutArgument {
    PositionalOutArgument(const Name& variable_) : variable(variable_) {}
    Name variable;
    std::string format(unsigned int indent_level) const;
  };

  struct OptionalOutArgument {
    OptionalOutArgument(const Name& key_, const Name& variable_)
      : key(key_), variable(variable_) {}
    Name key;
    Name variable;
    std::string format(unsigned int indent_level) const;
  };

  struct ArbitraryOutArgument {
    ArbitraryOutArgument(const Name& variable_) : variable(variable_) {}
    Name variable;
    std::string format(unsigned int indent_level) const;
  };

  struct KeywordOutArgument {
    KeywordOutArgument(const Name& variable_) : variable(variable_) {}
    Name variable;
    std::string format(unsigned int indent_level) const;
  };

  struct Call {
    Call(const Name& callable_) : callable(callable_) {}
    Name callable;
    std::vector<PositionalOutArgument> left_positional_args;
    boost::optional<ArbitraryOutArgument> left_arbitrary_arg;
    std::vector<PositionalOutArgument> right_positional_args;
    std::vector<OptionalOutArgument> right_optional_args;
    boost::optional<ArbitraryOutArgument> right_arbitrary_arg;
    boost::optional<KeywordOutArgument> right_keyword_arg;
    std::string format(unsigned int indent_level) const;
  };

  struct ReturnValue : public Expression {
    ReturnValue(const Name& assignee_, PTR<Call> term_)
      : assignee(assignee_), term(term_) {}
    Name assignee;
    PTR<Call> term;
    std::string format(unsigned int indent_level) const;
    void accept(ExpressionVisitor* visitor) { visitor->visit(this); }
  };

  struct PositionalInArgument {
    PositionalInArgument(const Name& variable_) : variable(variable_) {}
    Name variable;
    std::string format(unsigned int indent_level) const;
  };

  struct OptionalInArgument {
    OptionalInArgument(const Name& variable_, const Name& defaultval_)
      : variable(variable_), defaultval(defaultval_) {}
    Name variable;
    Name defaultval;
    std::string format(unsigned int indent_level) const;
  };

  struct ArbitraryInArgument {
    ArbitraryInArgument(const Name& variable_) : variable(variable_) {}
    Name variable;
    std::string format(unsigned int indent_level) const;
  };

  struct KeywordInArgument {
    KeywordInArgument(const Name& variable_) : variable(variable_) {}
    Name variable;
    std::string format(unsigned int indent_level) const;
  };

  struct Function : public Value {
    Function(const Name& lastval_) : lastval(lastval_) {}
    std::vector<PTR<Expression> > expressions;
    Name lastval;
    std::vector<PositionalInArgument> left_positional_args;
    std::vector<OptionalInArgument> left_optional_args;
    boost::optional<ArbitraryInArgument> left_arbitrary_arg;
    std::vector<PositionalInArgument> right_positional_args;
    std::vector<OptionalInArgument> right_optional_args;
    boost::optional<ArbitraryInArgument> right_arbitrary_arg;
    boost::optional<KeywordInArgument> right_keyword_arg;
    std::string format(unsigned int indent_level) const;
    void accept(ValueVisitor* visitor) { visitor->visit(this); }
  };

  void convert(const std::vector<PTR<pants::ast::Expression> >& exps,
      std::vector<PTR<pants::ir::Expression> >& out,
      pants::ir::Name& lastval);

}}

#endif
