#ifndef __PRE_CPS_IR_H__
#define __PRE_CPS_IR_H__

#include "common.h"
#include "ast.h"

namespace cirth {
namespace pre_cps_ir {

  struct Expression {
    virtual ~Expression() {}
    virtual std::string format() const = 0;
    protected: Expression() {} };

  struct Term : public Expression {
    virtual ~Term() {}
    protected: Term() {} };

  struct Value : public Term {
    virtual ~Value() {}
    protected: Value() {} };

  struct Assignee {
    virtual ~Assignee() {}
    virtual std::string format() const = 0;
    protected: Assignee() {} };

  struct InArgument {
    virtual ~InArgument() {}
    virtual std::string format() const = 0;
    protected: InArgument() {} };

  struct OutArgument {
    virtual ~OutArgument() {}
    virtual std::string format() const = 0;
    protected: OutArgument() {} };

  struct Name {
    Name(const std::string& name_, bool user_provided_)
      : name(name_), user_provided(user_provided_) {}
    Name(const cirth::ast::Variable& var)
      : name(var.name), user_provided(var.user_provided) {}
    std::string format() const;
    std::string name;
    bool user_provided;
  };

  struct Assignment : public Expression {
    Assignment(PTR<Assignee> assignee_, PTR<Term> term_, bool mutation_)
      : assignee(assignee_), term(term_), mutation(mutation_) {}
    PTR<Assignee> assignee;
    PTR<Term> term;
    std::string format() const;
    bool mutation;
  };

  struct SingleAssignee : public Assignee {
    SingleAssignee(const Name& variable_) : variable(variable_) {}
    std::string format() const;
    Name variable;
  };

  struct IndexAssignee : public Assignee {
    IndexAssignee(const PTR<Value>& array_, const PTR<Value>& index_)
      : array(array_), index(index_) {}
    std::string format() const;
    PTR<Value> array;
    PTR<Value> index;
  };

  struct FieldAssignee : public Assignee {
    FieldAssignee(const PTR<Value>& object_, const Name& field_)
      : object(object_), field(field_) {}
    std::string format() const;
    PTR<Value> object;
    Name field;
  };

  struct Index : public Term {
    Index(const PTR<Value>& array_, const PTR<Value>& index_)
      : array(array_), index(index_) {}
    std::string format() const;
    PTR<Value> array;
    PTR<Value> index;
  };

  struct Field : public Term {
    Field(const PTR<Value>& object_, const Name& field_)
      : object(object_), field(field_) {}
    std::string format() const;
    PTR<Value> object;
    Name field;
  };

  struct Variable : public Value {
    Variable(const Name& variable_) : variable(variable_) {}
    Name variable;
    std::string format() const;
  };

  struct Integer : public Value {
    Integer(const long long& value_) : value(value_) {}
    long long value;
    std::string format() const;
  };

  struct CharString : public Value {
    CharString(const std::string& value_) : value(value_) {}
    std::string value;
    std::string format() const;
  };

  struct ByteString : public Value {
    ByteString(const std::string& value_) : value(value_) {}
    std::string value;
    std::string format() const;
  };

  struct Float : public Value {
    Float(const double& value_) : value(value_) {}
    double value;
    std::string format() const;
  };

  struct PositionalOutArgument : public OutArgument {
    PositionalOutArgument(const PTR<Value>& variable_) : variable(variable_) {}
    PTR<Value> variable;
    std::string format() const;
  };

  struct OptionalOutArgument : public OutArgument {
    OptionalOutArgument(const Name& key_, const PTR<Value>& variable_)
      : key(key_), variable(variable_) {}
    Name key;
    PTR<Value> variable;
    std::string format() const;
  };

  struct ArbitraryOutArgument : public OutArgument {
    ArbitraryOutArgument(const PTR<Value>& variable_) : variable(variable_) {}
    PTR<Value> variable;
    std::string format() const;
  };

  struct KeywordOutArgument : public OutArgument {
    KeywordOutArgument(const PTR<Value>& variable_) : variable(variable_) {}
    PTR<Value> variable;
    std::string format() const;
  };

  struct Call : public Term {
    PTR<Value> function;
    std::vector<PositionalOutArgument> left_positional_args;
    boost::optional<ArbitraryOutArgument> left_arbitrary_arg;
    std::vector<PositionalOutArgument> right_positional_args;
    std::vector<OptionalOutArgument> right_optional_args;
    boost::optional<ArbitraryOutArgument> right_arbitrary_arg;
    boost::optional<KeywordOutArgument> right_keyword_arg;
    std::vector<OptionalOutArgument> scoped_optional_args;
    boost::optional<KeywordOutArgument> scoped_keyword_arg;
    std::string format() const;
  };

  struct DictDefinition {
    DictDefinition(const PTR<Value>& key_, const PTR<Value>& value_)
      : key(key_), value(value_) {}
    PTR<Value> key;
    PTR<Value> value;
    std::string format() const;
  };

  struct Dictionary : public Value {
    std::vector<DictDefinition> definitions;
    std::string format() const;
  };

  struct Array : public Value {
    std::vector<PTR<Value> > values;
    std::string format() const;
  };

  struct PositionalInArgument : public InArgument {
    PositionalInArgument(const Name& variable_) : variable(variable_) {}
    Name variable;
    std::string format() const;
  };

  struct OptionalInArgument : public InArgument {
    OptionalInArgument(const Name& variable_, const PTR<Value>& defaultval_)
      : variable(variable_), defaultval(defaultval_) {}
    Name variable;
    PTR<Value> defaultval;
    std::string format() const;
  };

  struct ArbitraryInArgument : public InArgument {
    ArbitraryInArgument(const Name& variable_) : variable(variable_) {}
    Name variable;
    std::string format() const;
  };

  struct KeywordInArgument : public InArgument {
    KeywordInArgument(const Name& variable_) : variable(variable_) {}
    Name variable;
    std::string format() const;
  };

  struct Function : public Value {
    Function(bool full_function_) : full_function(full_function_) {}
    std::vector<PositionalInArgument> left_positional_args;
    boost::optional<ArbitraryInArgument> left_arbitrary_arg;
    std::vector<PositionalInArgument> right_positional_args;
    std::vector<OptionalInArgument> right_optional_args;
    boost::optional<ArbitraryInArgument> right_arbitrary_arg;
    boost::optional<KeywordInArgument> right_keyword_arg;
    std::vector<PTR<Expression> > expressions;
    bool full_function;
    std::string format() const;
  };

  void convert(const std::vector<PTR<cirth::ast::Expression> >& exps,
      std::vector<PTR<cirth::pre_cps_ir::Expression> >& out);

}}

#endif
