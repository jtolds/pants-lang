#include "compile.h"
#include "assets.h"
#include "wrap.h"

using namespace cirth::cps;

#define MIN_RIGHT_ARG_HIGHWATER 10
#define MIN_LEFT_ARG_HIGHWATER 2

class VariableContext {
public:
  VariableContext(unsigned int free_id, unsigned int frame_id)
    : m_freeID(free_id), m_frameID(frame_id) {}
  VariableContext(unsigned int free_id, unsigned int frame_id,
      const std::set<Name>& active_frame_names)
    : m_freeID(free_id), m_frameID(frame_id),
      m_activeFrameNames(active_frame_names) {}
  std::string varAccess(const Name& name) {
    std::ostringstream os;
    os << "((struct nameset_";
    if(m_activeFrameNames.find(name) != m_activeFrameNames.end()) {
      os << m_frameID << "*)frame)->";
    } else {
      os << m_freeID << "*)env)->";
    }
    os << name.c_name();
    return os.str();
  }
  std::string valAccess(const Name& name) {
    // TODO: dereference cell if necessary
    return varAccess(name);
  }
  void localDefinition(const Name& name) { m_activeFrameNames.insert(name); }
  bool activeInFrame(const Name& name)
    { m_activeFrameNames.find(name) != m_activeFrameNames.end(); }
  unsigned int frameID() { return m_frameID; }
  unsigned int freeID() { return m_freeID; }
private:
  unsigned int m_freeID;
  unsigned int m_frameID;
  std::set<Name> m_activeFrameNames;
};

class NameSetManager {
public:
  typedef std::map<std::string, std::pair<std::set<Name>, unsigned int> >
          NameSetContainer;
public:
  void addSet(const std::set<Name>& names) { getID(names); }

  unsigned int getID(const std::set<Name>& names) {
    std::string key(namesetKey(names));
    NameSetContainer::iterator it(m_namesets.find(key));
    if(it != m_namesets.end()) return it->second.second;
    m_namesets[key].first = names;
    m_namesets[key].second = m_namesets.size();
    return m_namesets.size();
  }

  void writeStructs(std::ostream& os) {
    for(NameSetContainer::iterator it1(m_namesets.begin());
        it1 != m_namesets.end(); ++it1) {
      os << "struct nameset_" << it1->second.second << " {\n";
      for(std::set<Name>::iterator it2(it1->second.first.begin());
          it2 != it1->second.first.end(); ++it2) {
        os << "  union Value " << it2->c_name() << ";\n";
      }
      os << "};\n\n";
    }
  }

private:
  std::string namesetKey(const std::set<Name>& names) {
    std::ostringstream os;
    for(std::set<Name>::const_iterator it(names.begin()); it != names.end();
        ++it) {
      // yay sets being in sorted order always
      os << it->c_name() << ", ";
    }
    return os.str();
  }

private:
  NameSetContainer m_namesets;

};

static std::string to_bytestring(const std::string& data) {
  std::ostringstream os;
  os << "\"" << std::hex;
  for(unsigned int i = 0; i < data.size(); ++i) os << "\\x" << (int)data[i];
  os << "\\x00\"";
  return os.str();
}

static void inline write_expression(PTR<Expression> cps, std::ostream& os,
    VariableContext& context, NameSetManager& namesets);

class ValueWriter : public ValueVisitor {
  public:
    ValueWriter(std::ostream* os, VariableContext* context,
        NameSetManager* namesets)
      : m_os(os), m_context(context), m_namesets(namesets) {}
    void visit(Field* field) {
      *m_os << "  dest = " << m_context->valAccess(field->object) << ";\n"
               "  switch(dest.t) {\n"
               "    default:\n"
               "      THROW_ERROR("
            << m_context->valAccess(HIDDEN_OBJECT) <<
               ", make_c_string(\"TODO: fields\"));\n"
               "    case OBJECT:\n"
               "      if(!get_field(dest.object.data, "
            << to_bytestring(field->field.c_name()) << ", "
            << field->field.c_name().size() << ", &dest)) {\n"
               "        THROW_ERROR("
            << m_context->valAccess(HIDDEN_OBJECT) <<
               ", make_c_string(\"field %s not found!\", "
            << to_bytestring(field->field.c_name()) << "));\n"
               "      }\n"
               "      break;\n"
               "  }\n";
      m_lastval = "dest";
    }
    void visit(Variable* var) {
      m_lastval = m_context->valAccess(var->variable);
    }
    void visit(Integer* integer) {
      *m_os << "  dest.t = INTEGER;\n"
               "  dest.integer.value = " << integer->value << ";\n";
      m_lastval = "dest";
    }
    void visit(String* str) {
      *m_os << "  dest.t = STRING;\n"
               "  dest.string.byte_oriented = "
            << (str->byte_oriented ? "true" : "false") << ";\n"
               "  dest.string.value = " << to_bytestring(str->value) << ";\n"
               "  dest.string.value_size = " << str->value.size() << ";\n";
      m_lastval = "dest";
    }
    void visit(Float* floating) {
      *m_os << "  dest.t = FLOAT;\n"
               "  dest.floating.value = " << floating->value << ";\n";
      m_lastval = "dest";
    }
    void visit(Callable* func) {
      std::set<Name> free_names;
      func->free_names(free_names);
      unsigned int free_id(m_namesets->getID(free_names));
      *m_os << "  dest.t = CLOSURE;\n"
               "  dest.closure.func = &&" << func->c_name() << ";\n";
      if(func->function) {
        *m_os << "  dest.closure.frame = NULL;\n"
                 "  dest.closure.env = GC_MALLOC(sizeof(struct nameset_"
              << free_id << "));\n";
        for(std::set<Name>::const_iterator it(free_names.begin());
            it != free_names.end(); ++it) {
          *m_os << "  ((struct nameset_" << free_id << "*)dest.closure.env)->"
                << it->c_name() << " = " << m_context->varAccess(*it) << ";\n";
        }
      } else {
        *m_os << "  dest.closure.frame = frame;\n"
                 "  dest.closure.env = env;\n";
      }
      m_lastval = "dest";
    }
    std::string lastval() const { return m_lastval; }
  private:
    std::ostream* m_os;
    VariableContext* m_context;
    NameSetManager* m_namesets;
    std::string m_lastval;
};

static void write_callable(std::ostream& os, Callable* func,
    VariableContext* context, NameSetManager* namesets) {
  os << "\n" << func->c_name() << ":\n"
           "  MIN_RIGHT_ARGS(" << func->right_positional_args.size()
        << ")\n"
           "  MIN_LEFT_ARGS(" << func->left_positional_args.size()
        << ")\n";
  if(func->function) {
    bool is_mutated(false); // TODO
    context->localDefinition(CONTINUATION);
    context->localDefinition(HIDDEN_OBJECT);
    os << "  frame = GC_MALLOC(sizeof(struct nameset_" << context->frameID()
       << "));\n"
          "  " << context->varAccess(CONTINUATION) << " = "
       << (is_mutated ? "make_cell(continuation);\n" : "continuation;\n")
       << "  " << context->varAccess(HIDDEN_OBJECT) << " = "
       << (is_mutated ? "make_cell(hidden_object);\n" : "hidden_object;\n");
  }
  for(unsigned int i = 0; i < func->right_positional_args.size(); ++i) {
    bool is_mutated(false); // TODO
    context->localDefinition(func->right_positional_args[i]);
    os << "  " << context->varAccess(func->right_positional_args[i]) << " = ";
    if(is_mutated) os << "make_cell(";
    os << "right_positional_args[" << i << "]";
    if(is_mutated) os << ")";
    os << ";\n";
  }
  for(unsigned int i = 0; i < func->left_positional_args.size(); ++i) {
    bool is_mutated(false); // TODO
    context->localDefinition(func->left_positional_args[i]);
    os << "  " << context->varAccess(func->left_positional_args[i]) << " = ";
    if(is_mutated) os << "make_cell(";
    os << "left_positional_args[" << i << "]";
    if(is_mutated) os << ")";
    os << ";\n";
  }
  write_expression(func->expression, os, *context, *namesets);
}

class ExpressionWriter : public ExpressionVisitor {
  public:
    ExpressionWriter(std::ostream* os, VariableContext* context,
        NameSetManager* namesets)
      : m_os(os), m_context(context), m_namesets(namesets) {}
    void visit(Call* call) {
      ValueWriter writer(m_os, m_context, m_namesets);

      if(call->continuation.get()) {
        call->continuation->accept(&writer);
        *m_os << "  continuation = " << writer.lastval() << ";\n";
      } else {
        *m_os << "  continuation.t = NIL;\n";
      }

      if(call->right_positional_args.size() > MIN_RIGHT_ARG_HIGHWATER) {
        *m_os << "  if(" << call->right_positional_args.size()
              << " > right_positional_args_highwater) {\n"
                 "    right_positional_args_highwater = "
              << call->right_positional_args.size() << ";\n"
                 "    right_positional_args = GC_MALLOC(sizeof(union Value) * "
              << call->right_positional_args.size() << ");\n"
                 "  }\n";
      }
      *m_os << "  right_positional_args_size = "
            << call->right_positional_args.size() << ";\n";

      for(unsigned int i = 0; i < call->right_positional_args.size(); ++i) {
        *m_os << "  right_positional_args[" << i << "] = "
              << m_context->valAccess(call->right_positional_args[i]) << ";\n";
      }

      if(call->left_positional_args.size() > MIN_LEFT_ARG_HIGHWATER) {
        *m_os << "  if(" << call->left_positional_args.size()
              << " > left_positional_args_highwater) {\n"
                 "    left_positional_args_highwater = "
              << call->left_positional_args.size() << ";\n"
                 "    left_positional_args = GC_MALLOC(sizeof(union Value) * "
              << call->left_positional_args.size() << ");\n"
                 "  }\n";
      }
      *m_os << "  left_positional_args_size = "
            << call->left_positional_args.size() << ";\n";

      for(unsigned int i = 0; i < call->left_positional_args.size(); ++i) {
        *m_os << "  left_positional_args[" << i << "] = "
              << m_context->valAccess(call->left_positional_args[i]) << ";\n";
      }

      if(call->hidden_object_optional_args.size() > 0) {
        *m_os << "  copy_object(&" << m_context->valAccess(HIDDEN_OBJECT)
              << ", &hidden_object);\n";
        for(unsigned int i = 0; i < call->hidden_object_optional_args.size();
            ++i) {
          *m_os << "  set_field(hidden_object.object.data, "
                << to_bytestring(
                    call->hidden_object_optional_args[i].key.c_name())
                << ", "
                << call->hidden_object_optional_args[i].key.c_name().size()
                << ", &"
                << m_context->valAccess(
                    call->hidden_object_optional_args[i].value)
                << ");\n";
        }
        *m_os << "  seal_object(hidden_object.object.data);\n";
      } else {
        *m_os << "  hidden_object = " << m_context->valAccess(HIDDEN_OBJECT)
              << ";\n";
      }
      *m_os << "  dest = " << m_context->valAccess(call->callable) << ";\n"
               "  if(dest.t != CLOSURE) {\n"
               "    THROW_ERROR(" << m_context->valAccess(HIDDEN_OBJECT)
            << ", make_c_string(\"cannot call a non-function!\"));\n"
               "  }\n"
               "  CALL_FUNC(dest)\n";

      if(call->continuation.get())
        write_callable(*m_os, call->continuation.get(), m_context, m_namesets);
    }
    void visit(Assignment* assignment) {
      ValueWriter writer(m_os, m_context, m_namesets);
      assignment->value->accept(&writer);
      bool written = false;
      if(assignment->local) {
        bool active_in_frame(m_context->activeInFrame(assignment->assignee));
        bool is_mutated(false); // TODO
        m_context->localDefinition(assignment->assignee);
        if(!active_in_frame && is_mutated) {
          *m_os << "  " << m_context->varAccess(assignment->assignee)
                << " = make_cell(" << writer.lastval() << ");\n";
          written = true;
        }
      }
      if(!written) {
        *m_os << "  " << m_context->valAccess(assignment->assignee) << " = "
              << writer.lastval() << ";\n";
      }
      assignment->next_expression->accept(this);
    }
    void visit(ObjectMutation* mut) {
      *m_os << "  dest = " << m_context->valAccess(mut->object) << ";\n"
               "  switch(dest.t) {\n"
               "    default:\n"
               "      THROW_ERROR(" << m_context->valAccess(HIDDEN_OBJECT)
            << ", make_c_string(\"not an object!\"));\n"
               "    case OBJECT:\n"
               "      if(!set_field(dest.object.data, "
            << to_bytestring(mut->field.c_name()) << ", "
            << mut->field.c_name().size() << ", &"
            << m_context->valAccess(mut->value) << ")) {\n"
               "        THROW_ERROR(" << m_context->valAccess(HIDDEN_OBJECT)
            << ", make_c_string(\"object %s sealed!\", "
            << to_bytestring(mut->object.c_name()) << "));\n"
               "      }\n"
               "      break;\n"
               "  }\n";
      mut->next_expression->accept(this);
    }
  private:
    std::ostream* m_os;
    VariableContext* m_context;
    NameSetManager* m_namesets;
};

static void inline write_expression(PTR<Expression> cps, std::ostream& os,
    VariableContext& context, NameSetManager& namesets) {
  ExpressionWriter writer(&os, &context, &namesets);
  cps->accept(&writer);
}

void cirth::compile::compile(PTR<Expression> cps, std::ostream& os) {

  std::vector<PTR<cps::Callable> > callables;
  std::set<Name> free_names;
  cps->callables(callables);
  cps->free_names(free_names);
  cirth::wrap::remove_provided_names(free_names);

  if(free_names.size() > 0) {
    std::ostringstream os;
    os << "unbound variable: " << free_names.begin()->format(0);
    throw expectation_failure(os.str());
  }

  os << cirth::assets::HEADER_C << "\n";
  os << cirth::assets::DATA_STRUCTURES_C << "\n";
  os << cirth::assets::BUILTINS_C << "\n";

  NameSetManager namesets;
  std::set<Name> names;
  // handle globals specially
  cps->free_names(names);
  cps->frame_names(names);
  namesets.addSet(names);
  VariableContext root_context(0, namesets.getID(names), names);

  for(unsigned int i = 0; i < callables.size(); ++i) {
    if(!callables[i]->function) continue;
    names.clear();
    callables[i]->free_names(names);
    namesets.addSet(names);
    names.clear();
    callables[i]->frame_names(names);
    namesets.addSet(names);
  }

  namesets.writeStructs(os);

  os << cirth::assets::START_MAIN_C;

  write_expression(cps, os, root_context, namesets);

  for(unsigned int i = 0; i < callables.size(); ++i) {
    if(callables[i]->function) {
      std::set<Name> free_names;
      callables[i]->free_names(free_names);
      std::set<Name> frame_names;
      callables[i]->frame_names(frame_names);
      VariableContext new_context(namesets.getID(free_names),
          namesets.getID(frame_names));
      write_callable(os, callables[i].get(), &new_context, &namesets);
    }
  }

  os << cirth::assets::END_MAIN_C;

}
