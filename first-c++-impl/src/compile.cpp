#include "compile.h"
#include "assets.h"
#include "wrap.h"

using namespace pants::cps;
using namespace pants::annotate;

#define MIN_RIGHT_ARG_HIGHWATER 10
#define MIN_LEFT_ARG_HIGHWATER 10

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
  std::string valAccess(const Name& name, bool is_mutated) {
    if(is_mutated) {
      std::ostringstream os;
      os << "(*" << varAccess(name) << ".cell.addr)";
      return os.str();
    } else {
      return varAccess(name);
    }
  }
  void localDefinition(const Name& name) { m_activeFrameNames.insert(name); }
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
    VariableContext& context, NameSetManager& namesets, DataStore& store);

class ValueWriter : public ValueVisitor {
  public:
    ValueWriter(std::ostream* os, VariableContext* context,
        NameSetManager* namesets, DataStore* store)
      : m_os(os), m_context(context), m_namesets(namesets), m_store(store) {}
    void visit(Field* field) {
      *m_os << "  dest = " << m_context->valAccess(field->object->name,
          m_store->isMutated(field->object->getVarid())) << ";\n"
               "  switch(dest.t) {\n"
               "    default:\n"
               "      THROW_ERROR("
            << m_context->valAccess(DYNAMIC_VARS, false) <<
               ", make_c_string(\"TODO: fields\"));\n"
               "    case OBJECT:\n"
               "      if(!get_field(dest.object.data, (struct ByteArray){"
            << to_bytestring(field->field.c_name()) << ", "
            << field->field.c_name().size() << "}, &dest)) {\n"
               "        THROW_ERROR("
            << m_context->valAccess(DYNAMIC_VARS, false) <<
               ", make_c_string(\"field %s not found!\", "
            << to_bytestring(field->field.c_name()) << "));\n"
               "      }\n"
               "      break;\n"
               "  }\n";
      m_lastval = "dest";
    }
    void visit(VariableValue* var) {
      m_lastval = m_context->valAccess(var->variable->name,
          m_store->isMutated(var->variable->getVarid()));
    }
    void visit(Integer* integer) {
      std::ostringstream os;
      os << "(union Value){.integer = (struct Integer){INTEGER, "
         << integer->value << "}}";
      m_lastval = os.str();
    }
    void visit(String* str) {
      *m_os << "  dest.t = STRING;\n"
               "  dest.string.byte_oriented = "
            << (str->byte_oriented ? "true" : "false") << ";\n"
               "  dest.string.value.data = " << to_bytestring(str->value)
            << ";\n"
               "  dest.string.value.size = " << str->value.size() << ";\n";
      m_lastval = "dest";
    }
    void visit(Float* floating) {
      std::ostringstream os;
      os << "(union Value){.floating = (struct Float){FLOAT, "
         << floating->value << "}}";
      m_lastval = os.str();
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
    DataStore* m_store;
};

static void write_callable(std::ostream& os, Callable* func,
    VariableContext* context, NameSetManager* namesets, DataStore* store) {
  // TODO: don't generate code we know we don't need!
  //   * don't deal with keyword arguments if none are passed in
  //   * don't require slot checking for arguments with default values.

  // set up callable's address
  os << "\n" << func->c_name() << ":\n";
  if(func->function) {
    // if it's actually a function, we want to save off the current
    // continuation, dynamic vars, and make a frame
    context->localDefinition(CONTINUATION);
    context->localDefinition(DYNAMIC_VARS);
    os << "  frame = GC_MALLOC(sizeof(struct nameset_" << context->frameID()
       << "));\n"
          "  " << context->varAccess(CONTINUATION) << " = continuation;\n"
          "  " << context->varAccess(DYNAMIC_VARS) << " = dynamic_vars;\n";
  }

  // were we given a right keyword argument? make space so we can add any
  // overflow keyword arguments if necessary
  if(!!func->right_keyword_arg) {
    bool is_mutated(store->isMutated(func->right_keyword_arg->getVarid()));
    context->localDefinition(func->right_keyword_arg->name);
    os << "  make_object(&dest);\n"
          "  " << context->varAccess(func->right_keyword_arg->name) << " = "
       << (is_mutated ? "make_cell(dest);\n" : "dest;\n");
  }

  // alright, go through and find all the names and positions of possible
  // keyword arguments.
  std::map<Name, std::pair<unsigned int, unsigned int> > argument_slots;
  for(unsigned int i = 0; i < func->right_positional_args.size(); ++i) {
    if(argument_slots.find(func->right_positional_args[i]->name) !=
        argument_slots.end())
      throw pants::expectation_failure("argument name collision");
    argument_slots[func->right_positional_args[i]->name].first = i;
    argument_slots[func->right_positional_args[i]->name].second = 1;
  }
  for(unsigned int i = 0; i < func->right_optional_args.size(); ++i) {
    if(argument_slots.find(func->right_optional_args[i].key->name) !=
        argument_slots.end())
      throw pants::expectation_failure("argument name collision");
    argument_slots[func->right_optional_args[i].key->name].first = i +
        func->right_positional_args.size();
    argument_slots[func->right_optional_args[i].key->name].second = 1;
  }
  for(unsigned int i = 0; i < func->left_positional_args.size(); ++i) {
    if(argument_slots.find(func->left_positional_args[i]->name) !=
        argument_slots.end())
      throw pants::expectation_failure("argument name collision");
    argument_slots[func->left_positional_args[i]->name].first =
        func->left_positional_args.size() - i - 1;
    argument_slots[func->left_positional_args[i]->name].second = 0;
  }
  for(unsigned int i = 0; i < func->left_optional_args.size(); ++i) {
    if(argument_slots.find(func->left_optional_args[i].key->name) !=
        argument_slots.end())
      throw pants::expectation_failure("argument name collision");
    argument_slots[func->left_optional_args[i].key->name].first =
        func->left_optional_args.size() - i - 1
        + func->left_positional_args.size();
    argument_slots[func->left_optional_args[i].key->name].second = 0;
  }
  unsigned int right_argument_slots =
      func->right_positional_args.size() + func->right_optional_args.size();
  unsigned int left_argument_slots =
      func->left_positional_args.size() + func->left_optional_args.size();

  // we're using a 64 bit unsigned integer to keep track of argument slots, so,
  // we can't go over 64. TODO: actually make the limit 64 and not 63.
  if(right_argument_slots >= 64)
    throw pants::expectation_failure("too many right arguments");
  if(left_argument_slots >= 64)
    throw pants::expectation_failure("too many left arguments");

  // we only handle named arguments if any of the arguments defined in the
  // function are user provided, there are default values, or someone is
  // accepting a keyword object
  bool handle_named_arguments = (func->left_optional_args.size() +
      func->right_optional_args.size()) > 0 || func->right_keyword_arg;
  if(!handle_named_arguments) {
    for(std::map<Name, std::pair<unsigned int, unsigned int> >::iterator it(
        argument_slots.begin()); it != argument_slots.end(); ++it) {
      if(it->first.user_provided) {
        handle_named_arguments = true;
        break;
      }
    }
  }

  if(handle_named_arguments) {
    // if we have any possible named arguments, let's deal with them
    if(right_argument_slots > 0) {
      os << "  if(right_positional_args.size > " << right_argument_slots
         << ") {\n"
            "    named_slots[1] = " << ((1 << right_argument_slots) - 1)
         << ";\n"
            "  } else {\n"
            "    named_slots[1] = (1 << right_positional_args.size) - 1;\n"
            "  }\n";
  //   It's the caller's responsibility to make sure there's enough space for all
  //   the right argument slots (named arguments can be assumed by the caller to
  //   be right arguments)
  //        "  reserve_space(&right_positional_args, " << right_argument_slots
  //     << ");\n"
    }
    if(left_argument_slots > 0) {
      os << "  if(left_positional_args.size > " << left_argument_slots
         << ") {\n"
            "    named_slots[0] = " << ((1 << left_argument_slots) - 1)
         << ";\n"
            "  } else {\n"
            "    named_slots[0] = (1 << left_positional_args.size) - 1;\n"
            "  }\n"
            "  reserve_space(&left_positional_args, " << left_argument_slots
         << ");\n";
    }
    os << "  for(initialize_object_iterator(&it, &keyword_args);\n"
          "      !object_iterator_complete(&it);\n"
          "      object_iterator_step(&it)) {\n"
          "    j = " << argument_slots.size() << ";\n"
          "    i = binary_search(object_iterator_current_node(&it)->key,\n"
          "        (struct ByteArray[]){";
    for(std::map<Name, std::pair<unsigned int, unsigned int> >::iterator it(
        argument_slots.begin()); it != argument_slots.end();
        ++it) {
      if(it != argument_slots.begin()) os << ", ";
      os << "(struct ByteArray){" << to_bytestring(it->first.c_name())
         << ", " << it->first.c_name().size() << "}";
    }
    os << "}, " << argument_slots.size() << ");\n"
          "    switch(i) {\n"
          "      default: break;\n";
    {
      unsigned int i = 0;
      for(std::map<Name, std::pair<unsigned int, unsigned int> >::iterator it(
          argument_slots.begin()); it != argument_slots.end();
          ++it, ++i) {
        os << "      case " << i << ": j = " << it->second.first << "; "
              "i = " << it->second.second << "; break;\n";
      }
    }
    os << "    }\n"
          "    if(j == " << argument_slots.size() << ") {\n";
    if(!!func->right_keyword_arg) {
      os << "      set_field("
         << context->valAccess(func->right_keyword_arg->name,
            store->isMutated(func->right_keyword_arg->getVarid()))
         << ".object.data, "
            "object_iterator_current_node(&it)->key, "
            "object_iterator_current_node(&it)->value);\n"
            "      continue;\n";
    } else {
      os << "      THROW_ERROR("
         << context->valAccess(DYNAMIC_VARS, false) <<
            ", make_c_string(\"argument %s unknown!\", "
            "object_iterator_current_node(&it)->key.data));\n";
    }
    os << "    }\n"
          "    if(named_slots[i] & (1 << j)) {\n"
          "      THROW_ERROR("
       << context->valAccess(DYNAMIC_VARS, false) <<
          ", make_c_string(\"argument %s already provided!\", "
          "object_iterator_current_node(&it)->key.data));\n"
          "    }\n"
          "    named_slots[i] |= (1 << j);\n";
    if(left_argument_slots > 0 && right_argument_slots > 0) {
      os << "    if(i == 0) {\n"
            "      left_positional_args.data[j] = "
            "object_iterator_current_node(&it)->value;\n"
            "    } else {\n"
            "      right_positional_args.data[j] = "
            "object_iterator_current_node(&it)->value;\n"
            "    }\n";
    } else {
      if(left_argument_slots > 0) {
        os << "    left_positional_args.data[j] = "
              "object_iterator_current_node(&it)->value;\n";
      } else {
        os << "    right_positional_args.data[j] = "
              "object_iterator_current_node(&it)->value;\n";
      }
    }
    os << "  }\n"
          "  initialize_object(&keyword_args);\n";

    // okay, now we have all of the provided and named arguments in slots, let's
    // throw in default values for anything still missing.
    for(unsigned int i = 0; i < func->right_optional_args.size(); ++i) {
      os << "  if(!(named_slots[1] & (1 << "
         << i + func->right_positional_args.size() << "))) {\n"
            "    right_positional_args.data["
         << i + func->right_positional_args.size() << "] = "
         << context->valAccess(func->right_optional_args[i].value->name,
            store->isMutated(func->right_optional_args[i].value->getVarid()))
         << ";\n"
         << "    named_slots[1] |= (1 << "
         << i + func->right_positional_args.size() << ");\n"
         << "  }\n";
    }
    for(unsigned int i = 0; i < func->left_optional_args.size(); ++i) {
      os << "  if(!(named_slots[0] & (1 << "
         << i + func->left_positional_args.size() << "))) {\n"
            "    left_positional_args.data["
         << i + func->left_positional_args.size() << "] = "
         << context->valAccess(func->left_optional_args[
            func->left_optional_args.size() - i - 1].value->name,
            store->isMutated(func->left_optional_args[
            func->left_optional_args.size() - i - 1].value->getVarid())) << ";\n"
         << "    named_slots[0] |= (1 << "
         << i + func->left_positional_args.size() << ");\n"
         << "  }\n";
    }

    // let's seal the keyword object
    if(!!func->right_keyword_arg) {
      os << "  seal_object(" << context->valAccess(func->right_keyword_arg->name,
            store->isMutated(func->right_keyword_arg->getVarid()))
         << ".object.data);\n";
    }

    // check and make sure we got everything.
    if(right_argument_slots > 0) {
      os << "  if((~(named_slots[1])) & ((1 << "
         << right_argument_slots << ") - 1)) {\n"
            "    THROW_ERROR(" << context->valAccess(DYNAMIC_VARS, false) << ", "
            "make_c_string(\"argument missing!\"));\n"
            "  }\n"
            "  if(right_positional_args.size < " << right_argument_slots
         << ") {\n"
            "    right_positional_args.size = " << right_argument_slots
         << ";\n"
            "  }\n";
    }
    if(left_argument_slots > 0) {
      os << "  if((~(named_slots[0])) & ((1 << "
         << left_argument_slots << ") - 1)) {\n"
            "    THROW_ERROR(" << context->valAccess(DYNAMIC_VARS, false) << ", "
            "make_c_string(\"argument missing!\"));\n"
            "  }\n"
            "  if(left_positional_args.size < " << left_argument_slots
         << ") {\n"
            "    left_positional_args.size = " << left_argument_slots
         << ";\n"
            "  }\n";
    }
  } else {
    // we didn't deal with named objects, so we just have to check if we got
    // everything
    os << "  MIN_LEFT_ARGS(" << left_argument_slots << ")\n"
          "  MIN_RIGHT_ARGS(" << right_argument_slots << ")\n";
  }

  // okay, let's actually take our slots and fill in the real arguments
  for(unsigned int i = 0; i < func->right_positional_args.size(); ++i) {
    bool is_mutated(store->isMutated(
        func->right_positional_args[i]->getVarid()));
    context->localDefinition(func->right_positional_args[i]->name);
    os << "  " << context->varAccess(func->right_positional_args[i]->name)
       << " = ";
    if(is_mutated) os << "make_cell(";
    os << "right_positional_args.data[" << i << "]";
    if(is_mutated) os << ")";
    os << ";\n";
  }
  for(unsigned int i = 0; i < func->right_optional_args.size(); ++i) {
    bool is_mutated(store->isMutated(
        func->right_optional_args[i].key->getVarid()));
    context->localDefinition(func->right_optional_args[i].key->name);
    os << "  " << context->varAccess(func->right_optional_args[i].key->name)
       << " = ";
    if(is_mutated) os << "make_cell(";
    os << "right_positional_args.data["
       << i + func->right_positional_args.size() << "]";
    if(is_mutated) os << ")";
    os << ";\n";
  }
  for(unsigned int i = 0; i < func->left_positional_args.size(); ++i) {
    bool is_mutated(store->isMutated(
        func->left_positional_args[i]->getVarid()));
    context->localDefinition(func->left_positional_args[i]->name);
    os << "  " << context->varAccess(func->left_positional_args[i]->name)
       << " = ";
    if(is_mutated) os << "make_cell(";
    os << "left_positional_args.data["
       << (func->left_positional_args.size() - i - 1) << "]";
    if(is_mutated) os << ")";
    os << ";\n";
  }
  for(unsigned int i = 0; i < func->left_optional_args.size(); ++i) {
    bool is_mutated(store->isMutated(
        func->left_optional_args[i].key->getVarid()));
    context->localDefinition(func->left_optional_args[i].key->name);
    os << "  " << context->varAccess(func->left_optional_args[i].key->name)
       << " = ";
    if(is_mutated) os << "make_cell(";
    os << "left_positional_args.data["
       << ((func->left_optional_args.size() - i - 1) +
           func->left_positional_args.size()) << "]";
    if(is_mutated) os << ")";
    os << ";\n";
  }

  // okay, any overflow arguments go into an array
  if(!!func->right_arbitrary_arg) {
    bool is_mutated(store->isMutated(func->right_arbitrary_arg->getVarid()));
    context->localDefinition(func->right_arbitrary_arg->name);
    os << "  make_array_object(&dest, (struct Array**)&raw_swap);\n"
          "  append_values(raw_swap, right_positional_args.data + "
       << right_argument_slots << ", right_positional_args.size - "
       << right_argument_slots << ");\n"
          "  " << context->varAccess(func->right_arbitrary_arg->name) << " = "
       << (is_mutated ? "make_cell(dest);\n" : "dest;\n");
  } else {
    os << "  MAX_RIGHT_ARGS(" << right_argument_slots << ")\n";
  }
  if(!!func->left_arbitrary_arg) {
    bool is_mutated(store->isMutated(func->left_arbitrary_arg->getVarid()));
    context->localDefinition(func->left_arbitrary_arg->name);
    os << "  make_array_object(&dest, (struct Array**)&raw_swap);\n"
          "  reserve_space(raw_swap, left_positional_args.size - "
       << left_argument_slots << ");\n"
          "  for(i = 0; i < left_positional_args.size - "
       << left_argument_slots << "; ++i) {\n"
          "    ((struct Array*)raw_swap)->data[i] = left_positional_args.data["
          "left_positional_args.size - i - 1];\n"
          "  }\n"
          "  ((struct Array*)raw_swap)->size = left_positional_args.size - "
       << left_argument_slots << ";\n"
          "  " << context->varAccess(func->left_arbitrary_arg->name) << " = "
       << (is_mutated ? "make_cell(dest);\n" : "dest;\n");
  } else {
    os << "  MAX_LEFT_ARGS(" << left_argument_slots << ")\n";
  }

  // k, we should be set, let's run the function
  write_expression(func->expression, os, *context, *namesets, *store);
}

class ExpressionWriter : public ExpressionVisitor {
  public:
    ExpressionWriter(std::ostream* os, VariableContext* context,
        NameSetManager* namesets, DataStore* store)
      : m_os(os), m_context(context), m_namesets(namesets), m_store(store) {}
    void visit(Call* call) {
      ValueWriter writer(m_os, m_context, m_namesets, m_store);

      if(call->continuation.get()) {
        call->continuation->accept(&writer);
        *m_os << "  continuation = " << writer.lastval() << ";\n";
      } else {
        *m_os << "  continuation.t = NIL;\n";
      }

      if(!!call->right_keyword_arg) {
        *m_os << "  for(initialize_object_iterator(&it, "
              << m_context->valAccess(call->right_keyword_arg->name,
                 m_store->isMutated(call->right_keyword_arg->getVarid()))
              << ".object.data);\n"
                 "      !object_iterator_complete(&it);\n"
                 "      object_iterator_step(&it)) {\n"
                 "    set_field(&keyword_args,\n"
                 "              object_iterator_current_node(&it)->key,\n"
                 "              object_iterator_current_node(&it)->value);\n"
                 "  }\n";
      }
      if(call->right_optional_args.size() > 0) {
        for(unsigned int i = 0; i < call->right_optional_args.size(); ++i) {
          *m_os << "  set_field(&keyword_args, (struct ByteArray){"
                << to_bytestring(
                    call->right_optional_args[i].key.c_name())
                << ", "
                << call->right_optional_args[i].key.c_name().size()
                << "}, "
                << m_context->valAccess(
                    call->right_optional_args[i].value->name,
                    m_store->isMutated(
                    call->right_optional_args[i].value->getVarid()))
                << ");\n";
        }
      }

      *m_os << "  i = 0;\n";
      if(call->right_positional_args.size() + call->right_optional_args.size() >
          MIN_RIGHT_ARG_HIGHWATER || !!call->right_arbitrary_arg ||
          !!call->right_keyword_arg) {
        if(!!call->right_arbitrary_arg) {
          *m_os << "  get_field("
                << m_context->valAccess(call->right_arbitrary_arg->name,
                   m_store->isMutated(call->right_arbitrary_arg->getVarid()))
                << ".object.data, (struct ByteArray){\"u_size\", 6}, &dest);\n";
          *m_os << "  i += ((struct Array*)dest.closure.env)->size;\n";
        }
        *m_os << "  right_positional_args.size = 0;\n"
                 "  reserve_space(&right_positional_args, "
              << call->right_positional_args.size() << " + i + "
                 "keyword_args.size);\n";
      }
      *m_os << "  right_positional_args.size = "
            << call->right_positional_args.size() << " + i;\n";

      for(unsigned int i = 0; i < call->right_positional_args.size(); ++i) {
        *m_os << "  right_positional_args.data[" << i << "] = "
              << m_context->valAccess(call->right_positional_args[i]->name,
                 m_store->isMutated(
                 call->right_positional_args[i]->getVarid()))
              << ";\n";
      }

      if(!!call->right_arbitrary_arg) {
        *m_os << "  for(j = 0; j < i; ++j) {\n"
                 "    right_positional_args.data["
              << call->right_positional_args.size()
              << " + j] = ((struct Array*)dest.closure.env)->data[j];\n"
                 "  }\n";
      }

      *m_os << "  i = 0;\n";
      if(call->left_positional_args.size() > MIN_LEFT_ARG_HIGHWATER ||
          !!call->left_arbitrary_arg) {
        if(!!call->left_arbitrary_arg) {
          *m_os << "  get_field("
                << m_context->valAccess(call->left_arbitrary_arg->name,
                   m_store->isMutated(call->left_arbitrary_arg->getVarid()))
                << ".object.data, (struct ByteArray){\"u_size\", 6}, &dest);\n";
          *m_os << "  i = ((struct Array*)dest.closure.env)->size;\n";
        }
        *m_os << "  left_positional_args.size = 0;\n"
                 "  reserve_space(&left_positional_args, "
              << call->left_positional_args.size() << " + i);\n";
      }
      *m_os << "  left_positional_args.size = "
            << call->left_positional_args.size() << " + i;\n";

      if(!!call->left_arbitrary_arg) {
        *m_os << "  for(j = 0; j < i; ++j) {\n"
                 "    left_positional_args.data["
                 "left_positional_args.size - j - 1] = "
                 "((struct Array*)dest.closure.env)->data[j];\n"
                 "  }\n";
      }

      for(unsigned int i = 0; i < call->left_positional_args.size(); ++i) {
        *m_os << "  left_positional_args.data[" << i << "] = "
              << m_context->valAccess(call->left_positional_args[
                 call->left_positional_args.size() - i - 1]->name,
                 m_store->isMutated(call->left_positional_args[
                 call->left_positional_args.size() - i - 1]->getVarid()))
              << ";\n";
      }

      *m_os << "  dynamic_vars = " << m_context->valAccess(DYNAMIC_VARS, false)
            << ";\n"
               "  dest = " << m_context->valAccess(call->callable->name,
               m_store->isMutated(call->callable->getVarid()))
            << ";\n"
               "  if(dest.t != CLOSURE) {\n"
               "    THROW_ERROR(" << m_context->valAccess(DYNAMIC_VARS, false)
            << ", make_c_string(\"cannot call a non-function!\"));\n"
               "  }\n"
               "  CALL_FUNC(dest)\n";

      if(call->continuation.get())
        write_callable(*m_os, call->continuation.get(), m_context, m_namesets,
            m_store);
    }
    void visit(Assignment* assignment) {
      ValueWriter writer(m_os, m_context, m_namesets, m_store);
      assignment->value->accept(&writer);
      bool written = false;
      if(assignment->local) {
        bool is_mutated(m_store->isMutated(assignment->assignee->getVarid()));
        m_context->localDefinition(assignment->assignee->name);
        if(is_mutated) {
          *m_os << "  " << m_context->varAccess(assignment->assignee->name)
                << " = make_cell(" << writer.lastval() << ");\n";
          written = true;
        }
      }
      if(!written) {
        *m_os << "  " << m_context->valAccess(assignment->assignee->name,
                 m_store->isMutated(assignment->assignee->getVarid()))
              << " = " << writer.lastval() << ";\n";
      }
      assignment->next_expression->accept(this);
    }
    void visit(ObjectMutation* mut) {
      *m_os << "  dest = " << m_context->valAccess(mut->object->name,
               m_store->isMutated(mut->object->getVarid())) << ";\n"
               "  switch(dest.t) {\n"
               "    default:\n"
               "      THROW_ERROR(" << m_context->valAccess(DYNAMIC_VARS, false)
            << ", make_c_string(\"not an object!\"));\n"
               "    case OBJECT:\n"
               "      if(!set_field(dest.object.data, (struct ByteArray){"
            << to_bytestring(mut->field.c_name()) << ", "
            << mut->field.c_name().size() << "}, "
            << m_context->valAccess(mut->value->name, m_store->isMutated(
               mut->value->getVarid())) << ")) {\n"
               "        THROW_ERROR(" << m_context->valAccess(DYNAMIC_VARS,
               false)
            << ", make_c_string(\"object %s sealed!\", "
            << to_bytestring(mut->object->name.c_name()) << "));\n"
               "      }\n"
               "      break;\n"
               "  }\n";
      mut->next_expression->accept(this);
    }
  private:
    std::ostream* m_os;
    VariableContext* m_context;
    NameSetManager* m_namesets;
    DataStore* m_store;
};

static void inline write_expression(PTR<Expression> cps, std::ostream& os,
    VariableContext& context, NameSetManager& namesets, DataStore& store) {
  ExpressionWriter writer(&os, &context, &namesets, &store);
  cps->accept(&writer);
}

void pants::compile::compile(PTR<Expression> cps, DataStore& store,
    std::ostream& os, bool use_gc) {

  std::vector<PTR<cps::Callable> > callables;
  std::set<Name> free_names;
  cps->callables(callables);
  cps->free_names(free_names);

  std::set<Name> provided_names;
  pants::wrap::provided_names(provided_names);

  for(std::set<Name>::iterator it(provided_names.begin());
      it != provided_names.end(); ++it)
    free_names.erase(*it);

  if(free_names.size() > 0) {
    std::ostringstream os;
    os << "unbound variable: " << free_names.begin()->format(0);
    throw expectation_failure(os.str());
  }

  if(use_gc) os << "#define __USE_PANTS_GC\n";
  os << pants::assets::HEADER_C << "\n";
  os << pants::assets::DATA_STRUCTURES_C << "\n";
  os << pants::assets::BUILTINS_C << "\n";

  NameSetManager namesets;
  std::set<Name> names;
  // handle globals specially
  cps->free_names(names);
  cps->frame_names(names);
  namesets.addSet(names);
  VariableContext root_context(0, namesets.getID(names), provided_names);

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

  os << pants::assets::START_MAIN_C;

  write_expression(cps, os, root_context, namesets, store);

  for(unsigned int i = 0; i < callables.size(); ++i) {
    if(callables[i]->function) {
      std::set<Name> free_names;
      callables[i]->free_names(free_names);
      std::set<Name> frame_names;
      callables[i]->frame_names(frame_names);
      VariableContext new_context(namesets.getID(free_names),
          namesets.getID(frame_names));
      write_callable(os, callables[i].get(), &new_context, &namesets, &store);
    }
  }

  os << pants::assets::END_MAIN_C;

}
