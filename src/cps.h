#ifndef __CPS_TRANSFORM_H__
#define __CPS_TRANSFORM_H__

#include "common.h"
#include "ir.h"

namespace cirth {
namespace cps {

  typedef cirth::ir::Name Name;
  struct Callable; struct Field; struct Variable; struct Integer;
  struct String; struct Float; struct Callable; struct Call;
  struct VariableMutation; struct ObjectMutation;

  struct ValueVisitor {
    virtual void visit(Field*) = 0;
    virtual void visit(Variable*) = 0;
    virtual void visit(Integer*) = 0;
    virtual void visit(String*) = 0;
    virtual void visit(Float*) = 0;
    virtual void visit(Callable*) = 0;
  };

  struct ExpressionVisitor {
    virtual void visit(Call*) = 0;
    virtual void visit(VariableMutation*) = 0;
    virtual void visit(ObjectMutation*) = 0;
  };

  struct Value {
    virtual ~Value() {}
    virtual std::string format(unsigned int indent_level) const = 0;
    virtual void accept(ValueVisitor*) = 0;
    virtual void free_names(std::set<Name>& names) = 0;
    protected: Value() {} };

  struct Field : public Value {
    Field(const Name& object_, const Name& field_)
      : object(object_), field(field_) {}
    std::string format(unsigned int indent_level) const;
    void accept(ValueVisitor* v) { v->visit(this); }
    void free_names(std::set<Name>& names) { names.insert(object); }
    Name object;
    Name field;
  };

  struct Variable : public Value {
    Variable(const Name& variable_) : variable(variable_) {}
    Name variable;
    std::string format(unsigned int indent_level) const;
    void accept(ValueVisitor* v) { v->visit(this); }
    void free_names(std::set<Name>& names) { names.insert(variable); }
  };

  struct Integer : public Value {
    Integer(const long long& value_) : value(value_) {}
    long long value;
    std::string format(unsigned int indent_level) const;
    void accept(ValueVisitor* v) { v->visit(this); }
    void free_names(std::set<Name>& names) {}
  };

  struct String : public Value {
    String(const std::string& value_, bool byte_oriented_)
      : value(value_), byte_oriented(byte_oriented_) {}
    std::string value;
    bool byte_oriented;
    std::string format(unsigned int indent_level) const;
    void accept(ValueVisitor* v) { v->visit(this); }
    void free_names(std::set<Name>& names) {}
  };

  struct Float : public Value {
    Float(const double& value_) : value(value_) {}
    double value;
    std::string format(unsigned int indent_level) const;
    void accept(ValueVisitor* v) { v->visit(this); }
    void free_names(std::set<Name>& names) {}
  };

  struct Definition {
    Definition(const Name& key_, const Name& value_)
      : key(key_), value(value_) {}
    Name key;
    Name value;
    std::string format(unsigned int indent_level) const;
  };

  struct Expression {
    virtual ~Expression() {}
    virtual std::string format(unsigned int indent_level) const = 0;
    virtual void callables(std::vector<std::pair<PTR<Callable>, bool> >&) = 0;
    virtual void free_names(std::set<Name>& names) = 0;
    virtual void accept(ExpressionVisitor*) = 0;
    protected: Expression() {} };

  struct Call : public Expression {
    PTR<Value> callable;
    std::vector<Name> left_positional_args;
    boost::optional<Name> left_arbitrary_arg;
    std::vector<PTR<Value> > right_positional_args;
    std::vector<Definition> right_optional_args;
    boost::optional<Name> right_arbitrary_arg;
    boost::optional<Name> right_keyword_arg;
    std::vector<Definition> hidden_object_optional_args;
    PTR<Value> continuation;
    void callables(std::vector<std::pair<PTR<Callable>, bool> >& callables);
    void free_names(std::set<Name>& names);
    std::string format(unsigned int indent_level) const;
    void accept(ExpressionVisitor* v) { v->visit(this); }
  };

  struct VariableMutation : public Expression {
    VariableMutation(const Name& assignee_, Name value_,
        PTR<Expression> next_expression_)
      : assignee(assignee_), value(value_),
        next_expression(next_expression_)
    { assignee.set_mutated(); }
    Name assignee;
    Name value;
    PTR<Expression> next_expression;
    void callables(std::vector<std::pair<PTR<Callable>, bool> >& callables)
      { next_expression->callables(callables); }
    void free_names(std::set<Name>& names) {
      next_expression->free_names(names);
      names.insert(assignee);
      names.insert(value);
    }
    std::string format(unsigned int indent_level) const;
    void accept(ExpressionVisitor* v) { v->visit(this); }
  };

  struct ObjectMutation : public Expression {
    ObjectMutation(const Name& object_, const Name& field_, const Name& value_,
        PTR<Expression> next_expression_)
      : object(object_), field(field_), value(value_),
        next_expression(next_expression_) {}
    Name object;
    Name field;
    Name value;
    PTR<Expression> next_expression;
    void callables(std::vector<std::pair<PTR<Callable>, bool> >& callables)
      { next_expression->callables(callables); }
    void free_names(std::set<Name>& names) {
      next_expression->free_names(names);
      names.insert(object);
      names.insert(value);
    }
    std::string format(unsigned int indent_level) const;
    void accept(ExpressionVisitor* v) { v->visit(this); }
  };

  struct Callable : public Value {
    Callable(bool function_) : varid(m_varcount++), function(function_) {}
    PTR<Expression> expression;
    std::vector<Name> left_positional_args;
    boost::optional<Name> left_arbitrary_arg;
    std::vector<Name> right_positional_args;
    std::vector<Definition> right_optional_args;
    boost::optional<Name> right_arbitrary_arg;
    boost::optional<Name> right_keyword_arg;
    unsigned int varid;
    bool function;
    std::string c_name() const {
      std::ostringstream os;
      os << "f_" << varid;
      return os.str();
    }
    std::string format(unsigned int indent_level) const;
    void accept(ValueVisitor* v) { v->visit(this); }
    void arg_names(std::set<Name>& names);
    void free_names(std::set<Name>& names);
    private: static unsigned int m_varcount;
  };

  void transform(const std::vector<PTR<cirth::ir::Expression> >& in_ir,
      const ir::Name& in_lastval, PTR<Expression>& out_ir);

}}

#endif
