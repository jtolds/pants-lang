#include "ir.h"

using namespace pants;

class ConversionVisitor : public ast::AstVisitor {
public:
  ConversionVisitor(std::vector<PTR<ir::Expression> >* ir,
      ir::Name* lastval, unsigned long long* varcount)
    : m_ir(ir), m_lastval(lastval), m_varcount(varcount)
  {}

  void visit(const std::vector<PTR<ast::Expression> >& exps) {
    for(unsigned int i = 0; i < exps.size(); ++i)
      exps[i]->accept(this);
  }

  void visit(ast::Application* app) {
    if(app->terms.size() <= 0)
      throw expectation_failure("application has 0 terms?!");
    if(app->terms.size() > 1) {
      // this is a multi-term application. find the call term
      unsigned int opencall_idx = 0;
      bool found = false;
      for(unsigned int i = 0; i < app->terms.size(); ++i) {
        ast::Term *term = app->terms[i].get();
        if(term->trailers.size() > 0 && dynamic_cast<ast::OpenCall*>(
            term->trailers[term->trailers.size() - 1].get())) {
          if(found)
            throw expectation_failure(
                "application has more than 1 open calls!");
          opencall_idx = i;
          found = true;
        }
      }
      // okay, call term found, process

      PTR<ir::Call> call(new ir::Call(gensym()));

      call->left_positional_args.reserve(opencall_idx);
      for(unsigned int i = 0; i < opencall_idx; ++i) {
        app->terms[i]->accept(this);
        call->left_positional_args.push_back(ir::PositionalOutArgument(
            *m_lastval));
      }

      ast::Term term(*app->terms[opencall_idx]);
      if(found) term.trailers.pop_back();
      visit(&term);
      call->callable = *m_lastval;

      call->right_positional_args.reserve(app->terms.size() - opencall_idx - 1);
      for(unsigned int i = opencall_idx + 1; i < app->terms.size(); ++i) {
        app->terms[i]->accept(this);
        call->right_positional_args.push_back(ir::PositionalOutArgument(
            *m_lastval));
      }

      ir::Name name(gensym());
      m_ir->push_back(PTR<ir::Expression>(new ir::ReturnValue(
          name, call)));
      *m_lastval = name;

      return;
    }
    // at this point all applications have just a single term.
    visit(app->terms[0].get());
  }

  void visit(ast::Term* term) {
    // if we got here, this is the term from a single-term application
    // process the term from the inside out, value first, trailers next.
    term->value->accept(this);
    for(unsigned int i = 0; i < term->trailers.size(); ++i)
      term->trailers[i]->accept(this);
  }

  void visit(ast::OpenCall*) {
    // if we got an open call at this point, then it was preempted by some
    // subsequent trailer. treat it as an argumentless closed call
    ast::ClosedCall call;
    visit(&call);
  }

  void visit(ast::Field* field) {
    ir::Name val(gensym());
    m_ir->push_back(PTR<ir::Expression>(new ir::Assignment(val,
        PTR<ir::Value>(new ir::Field(*m_lastval, ir::Name(
        field->variable))), true)));
    *m_lastval = val;
  }

  void visit(ast::Index* index) {
    ir::Name array(*m_lastval);
    ast::SubExpression subexp(index->expressions);
    visit(&subexp);
    ir::Name loc(*m_lastval);
    ir::Name name(gensym());
    ir::Name callable(gensym());
    PTR<ir::Call> call(new ir::Call(gensym()));
    m_ir->push_back(PTR<ir::Expression>(new ir::Assignment(call->callable,
        PTR<ir::Value>(new ir::Field(array, LOOKUP_FIELD)), true)));
    call->right_positional_args.push_back(ir::PositionalOutArgument(loc));
    m_ir->push_back(PTR<ir::Expression>(new ir::ReturnValue(name, call)));
    *m_lastval = name;
  }

  void visit(ast::Variable* var) {
    *m_lastval = ir::Name(*var);
  }

  void visit(ast::SubExpression* subexp) {
    PTR<ir::Function> scope(new ir::Function(NULL_VALUE));
    ConversionVisitor subvisitor(&scope->expressions, &scope->lastval,
        m_varcount);
    subvisitor.visit(subexp->expressions);
    PTR<ir::Call> call(new ir::Call(gensym()));
    m_ir->push_back(PTR<ir::Expression>(new ir::Assignment(call->callable,
        scope, true)));
    ir::Name name(gensym());
    m_ir->push_back(PTR<ir::Expression>(new ir::ReturnValue(
        name, call)));
    *m_lastval = name;
  }

  void visit(ast::Integer* int_) {
    ir::Name val(gensym());
    m_ir->push_back(PTR<ir::Expression>(new ir::Assignment(val,
        PTR<ir::Value>(new ir::Integer(int_->value)), true)));
    *m_lastval = val;
  }

  void visit(ast::CharString* char_) {
    ir::Name val(gensym());
    m_ir->push_back(PTR<ir::Expression>(new ir::Assignment(val,
        PTR<ir::Value>(new ir::CharString(char_->value)), true)));
    *m_lastval = val;
  }

  void visit(ast::ByteString* byte) {
    ir::Name val(gensym());
    m_ir->push_back(PTR<ir::Expression>(new ir::Assignment(val,
        PTR<ir::Value>(new ir::ByteString(byte->value)), true)));
    *m_lastval = val;
  }

  void visit(ast::Float* float_) {
    ir::Name val(gensym());
    m_ir->push_back(PTR<ir::Expression>(new ir::Assignment(val,
        PTR<ir::Value>(new ir::Float(float_->value)), true)));
    *m_lastval = val;
  }

  void visit(ast::Dictionary* in_dict) {
    ir::Name out_dict(gensym());
    PTR<ir::Call> constructor(new ir::Call(DICT_CONSTRUCTOR));
    m_ir->push_back(PTR<ir::Expression>(new ir::ReturnValue(out_dict,
        constructor)));
    for(unsigned int i = 0; i < in_dict->values.size(); ++i) {
      in_dict->values[i].key->accept(this);
      ir::Name key(*m_lastval);
      in_dict->values[i].value->accept(this);
      ir::Name value(*m_lastval);
      PTR<ir::Call> update(new ir::Call(gensym()));
      m_ir->push_back(PTR<ir::Expression>(new ir::Assignment(update->callable,
          PTR<ir::Value>(new ir::Field(out_dict, UPDATE_FIELD)), true)));
      update->right_positional_args.push_back(ir::PositionalOutArgument(key));
      update->right_positional_args.push_back(ir::PositionalOutArgument(value));
      m_ir->push_back(PTR<ir::Expression>(new ir::ReturnValue(gensym(),
          update)));
    }
    *m_lastval = out_dict;
  }

  void visit(ast::Array* in_array) {
    ir::Name out_array(gensym());
    PTR<ir::Call> constructor(new ir::Call(ARRAY_CONSTRUCTOR));
    constructor->right_positional_args.reserve(in_array->values.size());
    for(unsigned int i = 0; i < in_array->values.size(); ++i) {
      in_array->values[i]->accept(this);
      constructor->right_positional_args.push_back(ir::PositionalOutArgument(
          *m_lastval));
    }
    m_ir->push_back(PTR<ir::Expression>(new ir::ReturnValue(out_array,
        constructor)));
    *m_lastval = out_array;
  }

  void visit(ast::Definition* assignment)
    { visit_assignment(assignment, true); }

  void visit(ast::Mutation* assignment)
    { visit_assignment(assignment, false); }

  void visit_assignment(ast::Assignment* assignment, bool definition) {
    PTR<ast::Term> term(assignment->assignee->term);

    if(term->trailers.size() == 0) {
      ast::Variable* var = dynamic_cast<ast::Variable*>(term->value.get());
      if(var) {
        if(definition) {
          m_ir->push_back(PTR<ir::Expression>(new ir::Assignment(
              ir::Name(*var), PTR<ir::Value>(new ir::Variable(NULL_VALUE)),
              true)));

          assignment->exp->accept(this);

          m_ir->push_back(PTR<ir::Expression>(new ir::Assignment(
              ir::Name(*var), PTR<ir::Value>(new ir::Variable(*m_lastval)),
              false)));
          return;
        } else {
          assignment->exp->accept(this);
          m_ir->push_back(PTR<ir::Expression>(new ir::Assignment(
              ir::Name(*var), PTR<ir::Value>(new ir::Variable(*m_lastval)),
              false)));
          return;
        }
      }
      throw expectation_failure("left-hand side of an assignment must be a "
          "variable, field, or index");
    }

    assignment->exp->accept(this);
    ir::Name rhs(*m_lastval);

    term->value->accept(this);
    for(unsigned int i = 0; i < term->trailers.size() - 1; ++i)
      term->trailers[i]->accept(this);

    ast::Field* field(dynamic_cast<ast::Field*>(term->trailers.back().get()));
    if(field) {
      m_ir->push_back(PTR<ir::Expression>(new ir::ObjectMutation(*m_lastval,
          ir::Name(field->variable), rhs)));
      *m_lastval = rhs;
      return;
    }

    ast::Index* index(dynamic_cast<ast::Index*>(term->trailers.back().get()));
    if(index) {
      ir::Name array(*m_lastval);
      ast::SubExpression subexp(index->expressions);
      visit(&subexp);
      ir::Name loc(*m_lastval);
      PTR<ir::Call> call(new ir::Call(gensym()));
      m_ir->push_back(PTR<ir::Expression>(new ir::Assignment(call->callable,
          PTR<ir::Value>(new ir::Field(array, UPDATE_FIELD)), true)));
      call->right_positional_args.push_back(ir::PositionalOutArgument(loc));
      call->right_positional_args.push_back(ir::PositionalOutArgument(rhs));
      m_ir->push_back(PTR<ir::Expression>(new ir::ReturnValue(gensym(), call)));
      *m_lastval = rhs;
      return;
    }

    throw expectation_failure("left-hand side of an assignment must be a "
        "variable, field, or index");
  }

  void visit(ast::Function* infunc) {
    PTR<ir::Function> outfunc(new ir::Function(NULL_VALUE));
    outfunc->left_positional_args.reserve(infunc->left_required_args.size());
    for(unsigned int i = 0; i < infunc->left_required_args.size(); ++i) {
      outfunc->left_positional_args.push_back(ir::PositionalInArgument(
          ir::Name(infunc->left_required_args[i].name)));
    }
    outfunc->left_optional_args.reserve(infunc->left_optional_args.size());
    for(unsigned int i = 0; i < infunc->left_optional_args.size(); ++i) {
      infunc->left_optional_args[i].application->accept(this);
      outfunc->left_optional_args.push_back(ir::OptionalInArgument(
          ir::Name(infunc->left_optional_args[i].name), *m_lastval));
    }
    if(!!infunc->left_arbitrary_arg)
      outfunc->left_arbitrary_arg = ir::ArbitraryInArgument(
          ir::Name(infunc->left_arbitrary_arg->name));
    outfunc->right_positional_args.reserve(infunc->right_required_args.size());
    for(unsigned int i = 0; i < infunc->right_required_args.size(); ++i) {
      outfunc->right_positional_args.push_back(ir::PositionalInArgument(
          ir::Name(infunc->right_required_args[i].name)));
    }
    outfunc->right_optional_args.reserve(infunc->right_optional_args.size());
    for(unsigned int i = 0; i < infunc->right_optional_args.size(); ++i) {
      infunc->right_optional_args[i].application->accept(this);
      outfunc->right_optional_args.push_back(ir::OptionalInArgument(
          ir::Name(infunc->right_optional_args[i].name), *m_lastval));
    }
    if(!!infunc->right_arbitrary_arg)
      outfunc->right_arbitrary_arg = ir::ArbitraryInArgument(
          ir::Name(infunc->right_arbitrary_arg->name));
    if(!!infunc->right_keyword_arg)
      outfunc->right_keyword_arg = ir::KeywordInArgument(
          ir::Name(infunc->right_keyword_arg->name));

    outfunc->expressions.push_back(PTR<ir::Expression>(new ir::Assignment(
        RETURN, PTR<ir::Value>(new ir::Variable(CONTINUATION)), true)));
    ConversionVisitor subvisitor(&outfunc->expressions, &outfunc->lastval,
        m_varcount);
    subvisitor.visit(infunc->expressions);

    ir::Name val(gensym());
    m_ir->push_back(PTR<ir::Expression>(new ir::Assignment(val, outfunc,
        true)));
    *m_lastval = val;
  }

  void visit(ast::ClosedCall* incall) {
    PTR<ir::Call> outcall(new ir::Call(*m_lastval));
    if(!!incall->left_arbitrary_arg) {
      ast::SubExpression subexp(incall->left_arbitrary_arg.get().array);
      visit(&subexp);
      outcall->left_arbitrary_arg = ir::ArbitraryOutArgument(
          *m_lastval);
    }
    outcall->left_positional_args.reserve(incall->left_required_args.size());
    for(unsigned int i = 0; i < incall->left_required_args.size(); ++i) {
      incall->left_required_args[i].application->accept(this);
      outcall->left_positional_args.push_back(
          ir::PositionalOutArgument(*m_lastval));
    }
    outcall->right_positional_args.reserve(incall->right_required_args.size());
    for(unsigned int i = 0; i < incall->right_required_args.size(); ++i) {
      incall->right_required_args[i].application->accept(this);
      outcall->right_positional_args.push_back(
          ir::PositionalOutArgument(*m_lastval));
    }
    outcall->right_optional_args.reserve(incall->right_optional_args.size());
    for(unsigned int i = 0; i < incall->right_optional_args.size(); ++i) {
      incall->right_optional_args[i].application->accept(this);
      outcall->right_optional_args.push_back(
          ir::OptionalOutArgument(ir::Name(
          incall->right_optional_args[i].name), *m_lastval));
    }
    if(!!incall->right_arbitrary_arg) {
      ast::SubExpression subexp(incall->right_arbitrary_arg.get().array);
      visit(&subexp);
      outcall->right_arbitrary_arg = ir::ArbitraryOutArgument(
          *m_lastval);
    }
    if(!!incall->right_keyword_arg) {
      ast::SubExpression subexp(incall->right_keyword_arg.get().object);
      visit(&subexp);
      outcall->right_keyword_arg = ir::KeywordOutArgument(*m_lastval);
    }
    ir::Name name(gensym());
    m_ir->push_back(PTR<ir::Expression>(new ir::ReturnValue(
        name, outcall)));
    *m_lastval = name;
  }

private:
  ir::Name gensym() {
    std::ostringstream os;
    os << GENSYM_PREFIX << ++(*m_varcount);
    return ir::Name(os.str(), false);
  }

private:
  std::vector<PTR<ir::Expression> >* m_ir;
  ir::Name* m_lastval;
  unsigned long long* m_varcount;
};

void ir::convert(const std::vector<PTR<ast::Expression> >& ast,
    std::vector<PTR<ir::Expression> >& ir,
    ir::Name& lastval) {
  unsigned long long varcount = 0;
  ConversionVisitor visitor(&ir, &lastval, &varcount);
  visitor.visit(ast);
}

std::string pants::ir::Name::format(unsigned int) const {
  std::ostringstream os;
  os << (user_provided ? "u" : "c")
     << "_"
     << name;
  return os.str();
}

std::string pants::ir::Name::c_name() const {
  std::ostringstream os;
  os << std::hex;
  os << (user_provided ? "u_" : "c_");
  for(unsigned int i = 0; i < name.size(); ++i) {
    if(isalnum(name[i])) {
      os << name[i];
    } else if(name[i] == '_') {
      os << "__";
    } else {
      os << "_" << ((int)name[i]);
    }
  }
  return os.str();
}

std::string pants::ir::ReturnValue::format(unsigned int indent_level) const {
  std::ostringstream os;
  os << "ReturnValue(" << assignee.format(indent_level+1) << ", "
     << term->format(indent_level+1) << ")";
  return os.str();
}

std::string pants::ir::Assignment::format(unsigned int indent_level) const {
  std::ostringstream os;
  os << "Assignment(" << (local ? "local, " : "nonlocal, ")
     << assignee.format(indent_level+1) << ", " << value->format(indent_level+1)
     << ")";
  return os.str();
}

std::string pants::ir::ObjectMutation::format(unsigned int indent_level) const {
  std::ostringstream os;
  os << "ObjectMutation(" << object.format(indent_level+1) << ", "
     << field.format(indent_level+1) << ", " << value.format(indent_level+1)
     << ")";
  return os.str();
}

std::string pants::ir::Field::format(unsigned int indent_level) const {
  std::ostringstream os;
  os << "Field(" << object.format(indent_level+1) << ", "
     << field.format(indent_level+1) << ")";
  return os.str();
}

std::string pants::ir::Variable::format(unsigned int indent_level) const {
  std::ostringstream os;
  os << "Variable(" << variable.format(indent_level+1) << ")";
  return os.str();
}

std::string pants::ir::Integer::format(unsigned int indent_level) const {
  std::ostringstream os;
  os << "Integer(" << value << ")";
  return os.str();
}

std::string pants::ir::CharString::format(unsigned int indent_level) const {
  std::ostringstream os;
  os << "CharString(" << value << ")";
  return os.str();
}

std::string pants::ir::ByteString::format(unsigned int indent_level) const {
  std::ostringstream os;
  os << "ByteString(" << value << ")";
  return os.str();
}

std::string pants::ir::Float::format(unsigned int indent_level) const {
  std::ostringstream os;
  os << "Float(" << value << ")";
  return os.str();
}

std::string pants::ir::PositionalOutArgument::format(unsigned int indent_level)
    const {
  std::ostringstream os;
  os << "PositionalOutArgument(" << variable.format(indent_level+1) << ")";
  return os.str();
}

std::string pants::ir::OptionalOutArgument::format(unsigned int indent_level)
    const {
  std::ostringstream os;
  os << "OptionalOutArgument(" << key.format(indent_level+1) << ", "
     << variable.format(indent_level+1) << ")";
  return os.str();
}

std::string pants::ir::ArbitraryOutArgument::format(unsigned int indent_level)
    const {
  std::ostringstream os;
  os << "ArbitraryOutArgument(" << variable.format(indent_level+1) << ")";
  return os.str();
}

std::string pants::ir::KeywordOutArgument::format(unsigned int indent_level)
    const {
  std::ostringstream os;
  os << "KeywordOutArgument(" << variable.format(indent_level+1) << ")";
  return os.str();
}

std::string pants::ir::PositionalInArgument::format(unsigned int indent_level)
    const {
  std::ostringstream os;
  os << "PositionalInArgument(" << variable.format(indent_level+1) << ")";
  return os.str();
}

std::string pants::ir::OptionalInArgument::format(unsigned int indent_level)
    const {
  std::ostringstream os;
  os << "OptionalInArgument(" << variable.format(indent_level+1) << ", "
     << defaultval.format(indent_level+1) << ")";
  return os.str();
}

std::string pants::ir::ArbitraryInArgument::format(unsigned int indent_level)
    const {
  std::ostringstream os;
  os << "ArbitraryInArgument(" << variable.format(indent_level+1) << ")";
  return os.str();
}

std::string pants::ir::KeywordInArgument::format(unsigned int indent_level)
    const {
  std::ostringstream os;
  os << "KeywordInArgument(" << variable.format(indent_level+1) << ")";
  return os.str();
}

std::string pants::ir::Call::format(unsigned int indent_level) const {
  std::ostringstream os;
  os << "Call(" << callable.format(indent_level+1) << ", Left(";
  bool comma_needed = false;
  for(unsigned int i = 0; i < left_positional_args.size(); ++i) {
    if(comma_needed) os << ", ";
    os << left_positional_args[i].format(indent_level+2);
    comma_needed = true;
  }
  if(!!left_arbitrary_arg) {
    if(comma_needed) os << ", ";
    os << left_arbitrary_arg.get().format(indent_level+2);
  }
  comma_needed = false;
  os << "), Right(";
  for(unsigned int i = 0; i < right_positional_args.size(); ++i) {
    if(comma_needed) os << ", ";
    os << right_positional_args[i].format(indent_level+2);
    comma_needed = true;
  }
  for(unsigned int i = 0; i < right_optional_args.size(); ++i) {
    if(comma_needed) os << ", ";
    os << right_optional_args[i].format(indent_level+2);
    comma_needed = true;
  }
  if(!!right_arbitrary_arg) {
    if(comma_needed) os << ", ";
    os << right_arbitrary_arg.get().format(indent_level+2);
    comma_needed = true;
  }
  if(!!right_keyword_arg) {
    if(comma_needed) os << ", ";
    os << right_keyword_arg.get().format(indent_level+2);
  }
  os << "))";
  return os.str();
}

std::string pants::ir::Function::format(unsigned int indent_level) const {
  std::ostringstream os;
  os << "Function(Left(";
  bool comma_needed = false;
  for(unsigned int i = 0; i < left_positional_args.size(); ++i) {
    if(comma_needed) os << ", ";
    os << left_positional_args[i].format(indent_level+2);
    comma_needed = true;
  }
  if(!!left_arbitrary_arg) {
    if(comma_needed) os << ", ";
    os << left_arbitrary_arg.get().format(indent_level+2);
  }
  os << "), Right(";
  comma_needed = false;
  for(unsigned int i = 0; i < right_positional_args.size(); ++i) {
    if(comma_needed) os << ", ";
    os << right_positional_args[i].format(indent_level+2);
    comma_needed = true;
  }
  for(unsigned int i = 0; i < right_optional_args.size(); ++i) {
    if(comma_needed) os << ", ";
    os << right_optional_args[i].format(indent_level+2);
    comma_needed = true;
  }
  if(!!right_arbitrary_arg) {
    if(comma_needed) os << ", ";
    os << right_arbitrary_arg.get().format(indent_level+2);
    comma_needed = true;
  }
  if(!!right_keyword_arg) {
    if(comma_needed) os << ", ";
    os << right_keyword_arg.get().format(indent_level+2);
  }
  os << "), Expressions(";
  for(unsigned int i = 0; i < expressions.size(); ++i) {
    if(i > 0) os << ", ";
    os << expressions[i]->format(indent_level+2);
  }
  os << "), LastVal(" << lastval.format(indent_level+2) << "))";
  return os.str();
}
