int gc_main(int argc, char **argv) {
  struct nameset_1 globals;
  void* env = NULL;
  void* frame = &globals;
  void* raw_swap = NULL;
  union Value dest;
  unsigned int i, j;
  struct Array right_positional_args;
  struct Array left_positional_args;
  union Value continuation;
  union Value dynamic_vars;
  struct ObjectData dynamic_vars_data;
  struct ObjectData keyword_args;
  struct ObjectIterator it;

  // This strategy imposes an argument limit of 64
  unsigned long long named_slots[2] = {0, 0};
  unsigned long long dynamic_var_counter = 0;

  EXTERNAL_FUNCTION_LABEL = &&c_external__function__call;
  ARRAY_CONSTRUCTOR_LABEL = &&c_Array;

  initialize_array(&right_positional_args);
  initialize_array(&left_positional_args);
  initialize_object(&keyword_args);

  globals.c_continuation.t = CLOSURE;
  globals.c_continuation.closure.func = &&c_halt;
  globals.c_continuation.closure.env = NULL;
  globals.c_continuation.closure.frame = NULL;

  globals.c_null.t = NIL;
  globals.c_true.t = BOOLEAN;
  globals.c_true.boolean.value = true;
  globals.c_false.t = BOOLEAN;
  globals.c_false.boolean.value = false;

  globals.c_dynamic__vars.t = OBJECT;
  globals.c_dynamic__vars.object.data = &dynamic_vars_data;
  initialize_object(&dynamic_vars_data);

#define DEFINE_BUILTIN(name) \
  globals.c_##name.t = CLOSURE; \
  globals.c_##name.closure.func = &&c_##name; \
  globals.c_##name.closure.env = NULL; \
  globals.c_##name.closure.frame = NULL;

  DEFINE_BUILTIN(print)
  DEFINE_BUILTIN(println)
  DEFINE_BUILTIN(readln)
  DEFINE_BUILTIN(if)
  DEFINE_BUILTIN(lessthan)
  DEFINE_BUILTIN(equals)
  DEFINE_BUILTIN(add)
  DEFINE_BUILTIN(subtract)
  DEFINE_BUILTIN(divide)
  DEFINE_BUILTIN(multiply)
  DEFINE_BUILTIN(modulo)
  DEFINE_BUILTIN(new__object)
  DEFINE_BUILTIN(seal__object)
  DEFINE_BUILTIN(Array)
  DEFINE_BUILTIN(DynamicVar)
  DEFINE_BUILTIN(register__main)
  DEFINE_BUILTIN(type)
  DEFINE_BUILTIN(Integer)
  DEFINE_BUILTIN(Float)
  DEFINE_BUILTIN(ByteString)
  DEFINE_BUILTIN(CharString)
  DEFINE_BUILTIN(Boolean)
  DEFINE_BUILTIN(Null)
  DEFINE_BUILTIN(Function)

#undef DEFINE_BUILTIN

  continuation.t = CLOSURE;
  continuation.closure.env = env;
  continuation.closure.frame = frame;
  continuation.closure.func = &&finish_setup;
  dynamic_vars.t = NIL;

  goto c_DynamicVar;

finish_setup:
  globals.c_throw__dynamic__var = right_positional_args.data[0];
  right_positional_args.size = 0;
  dynamic_vars = globals.c_dynamic__vars;
  dest.t = CLOSURE;
  dest.closure.env = NULL;
  dest.closure.frame = NULL;
  dest.closure.func = &&ho_throw;
  set_field(dynamic_vars.object.data, *((
      struct ByteArray*)globals.c_throw__dynamic__var.object.data->env), dest);
  seal_object(dynamic_vars.object.data);
  dest.t = NIL;

  goto start;

#define FATAL_ERROR(msg, val) \
  printf("fatal error: %s\n", msg); \
  dump_value(val); \
  return 1;
#define CALL_FUNC(callable) \
  env = callable.closure.env; \
  frame = callable.closure.frame; \
  goto *callable.closure.func;
#define THROW_ERROR(current_dynamic_vars, val) \
  right_positional_args.size = 1; \
  right_positional_args.data[0] = val; \
  initialize_object(&keyword_args); \
  if(!get_field(current_dynamic_vars.object.data, \
      *((struct ByteArray*)globals.c_throw__dynamic__var.object.data->env), \
      &dest)) { \
    FATAL_ERROR("no throw method registered!", globals.c_null); \
  } \
  if(dest.t != CLOSURE) { \
    FATAL_ERROR("throw is not a method!", dest); \
  } \
  left_positional_args.size = 0; \
  CALL_FUNC(dest);
#define MIN_RIGHT_ARGS(count) \
  if(right_positional_args.size < count) { \
    dest = make_c_string("function takes at least %d right arguments, %d " \
        "given.", count, right_positional_args.size); \
    THROW_ERROR(dynamic_vars, dest); \
  }
#define MAX_RIGHT_ARGS(count) \
  if(right_positional_args.size > count) { \
    dest = make_c_string("function takes at most %d right arguments, %d " \
        "given.", count, right_positional_args.size); \
    THROW_ERROR(dynamic_vars, dest); \
  }
#define MIN_LEFT_ARGS(count) \
  if(left_positional_args.size < count) { \
    dest = make_c_string("function takes at least %d left arguments, %d " \
        "given.", count, left_positional_args.size); \
    THROW_ERROR(dynamic_vars, dest); \
  }
#define MAX_LEFT_ARGS(count) \
  if(left_positional_args.size > count) { \
    dest = make_c_string("function takes at most %d left arguments, %d " \
        "given.", count, left_positional_args.size); \
    THROW_ERROR(dynamic_vars, dest); \
  }
#define REQUIRED_FUNCTION(func) \
  if(func.t != CLOSURE) { \
    dest = make_c_string("cannot call a non-function!"); \
    THROW_ERROR(dynamic_vars, dest); \
  }
#define NO_KEYWORD_ARGUMENTS \
  if(keyword_args.tree != NULL) { \
    dest = make_c_string("no keyword arguments supported for this builtin!"); \
    THROW_ERROR(dynamic_vars, dest); \
  }

c_print:
  REQUIRED_FUNCTION(continuation)
  MAX_LEFT_ARGS(0)
  NO_KEYWORD_ARGUMENTS
  dest.t = NIL;
  for(i = 0; i < right_positional_args.size; ++i) {
    if(i > 0) printf(" ");
    builtin_print(&right_positional_args.data[i], &dest);
    if(dest.t != NIL) { THROW_ERROR(dynamic_vars, dest); }
  }
  if(right_positional_args.size > 0) {
    dest = right_positional_args.data[0];
  } else {
    dest.t = NIL;
  }
  right_positional_args.size = 1;
  right_positional_args.data[0] = dest;
  dest = continuation;
  continuation.t = NIL;
  CALL_FUNC(dest)

c_println:
  REQUIRED_FUNCTION(continuation)
  MAX_LEFT_ARGS(0)
  NO_KEYWORD_ARGUMENTS
  dest.t = NIL;
  for(i = 0; i < right_positional_args.size; ++i) {
    if(i > 0) printf(" ");
    builtin_print(&right_positional_args.data[i], &dest);
    if(dest.t != NIL) { THROW_ERROR(dynamic_vars, dest); }
  }
  printf("\n");
  if(right_positional_args.size > 0) {
    dest = right_positional_args.data[0];
  } else {
    dest.t = NIL;
  }
  right_positional_args.size = 1;
  right_positional_args.data[0] = dest;
  dest = continuation;
  continuation.t = NIL;
  CALL_FUNC(dest)

c_readln:
  REQUIRED_FUNCTION(continuation)
  MAX_LEFT_ARGS(0)
  MAX_RIGHT_ARGS(0)
  NO_KEYWORD_ARGUMENTS
  dest.t = NIL;
  builtin_readln(&right_positional_args.data[i], &dest);
  if(dest.t != NIL) { THROW_ERROR(dynamic_vars, dest); }
  right_positional_args.size = 1;
  dest = continuation;
  continuation.t = NIL;
  CALL_FUNC(dest)

c_if:
  REQUIRED_FUNCTION(continuation)
  NO_KEYWORD_ARGUMENTS
  dest.t = NIL;
  if(left_positional_args.size == 1 && right_positional_args.size == 1) {
    if(builtin_istrue(&right_positional_args.data[0], &dest)) {
      REQUIRED_FUNCTION(left_positional_args.data[0])
      right_positional_args.size = 0;
      left_positional_args.size = 0;
      CALL_FUNC(left_positional_args.data[0]);
    }
    if(dest.t != NIL) { THROW_ERROR(dynamic_vars, dest); }
    left_positional_args.size = 0;
    right_positional_args.data[0].t = NIL;
    dest = continuation;
    continuation.t = NIL;
    CALL_FUNC(dest)
  }
  MAX_LEFT_ARGS(0)
  MIN_RIGHT_ARGS(2)
  MAX_RIGHT_ARGS(3)
  if(builtin_istrue(&right_positional_args.data[0], &dest)) {
    REQUIRED_FUNCTION(right_positional_args.data[1])
    right_positional_args.size = 0;
    CALL_FUNC(right_positional_args.data[1])
  }
  if(dest.t != NIL) { THROW_ERROR(dynamic_vars, dest); }
  if(right_positional_args.size == 3) {
    REQUIRED_FUNCTION(right_positional_args.data[2])
    right_positional_args.size = 0;
    CALL_FUNC(right_positional_args.data[2])
  }
  right_positional_args.data[0].t = NIL;
  right_positional_args.size = 1;
  dest = continuation;
  continuation.t = NIL;
  CALL_FUNC(dest)

c_lessthan:
  REQUIRED_FUNCTION(continuation)
  NO_KEYWORD_ARGUMENTS
  dest.t = NIL;
  if(left_positional_args.size == 1 && right_positional_args.size == 1) {
    right_positional_args.data[0].boolean.value = builtin_less_than(
        &left_positional_args.data[0], &right_positional_args.data[0], &dest);
  } else {
    MAX_LEFT_ARGS(0)
    MIN_RIGHT_ARGS(2)
    MAX_RIGHT_ARGS(2)
    right_positional_args.data[0].boolean.value = builtin_less_than(
        &right_positional_args.data[0], &right_positional_args.data[1], &dest);
  }
  if(dest.t != NIL) { THROW_ERROR(dynamic_vars, dest); }
  right_positional_args.data[0].t = BOOLEAN;
  left_positional_args.size = 0;
  right_positional_args.size = 1;
  dest = continuation;
  continuation.t = NIL;
  CALL_FUNC(dest)

c_equals:
  REQUIRED_FUNCTION(continuation)
  NO_KEYWORD_ARGUMENTS
  dest.t = NIL;
  if(left_positional_args.size == 1 && right_positional_args.size == 1) {
    right_positional_args.data[0].boolean.value = builtin_equals(
        &left_positional_args.data[0], &right_positional_args.data[0], &dest);
  } else {
    MAX_LEFT_ARGS(0)
    MIN_RIGHT_ARGS(2)
    right_positional_args.data[0].boolean.value = builtin_equals(
        &right_positional_args.data[0], &right_positional_args.data[1], &dest);
    for(i = 2; i < right_positional_args.size &&
        right_positional_args.data[0].boolean.value; ++i) {
      if(dest.t != NIL) { THROW_ERROR(dynamic_vars, dest); }
      right_positional_args.data[0].boolean.value = builtin_equals(
          &right_positional_args.data[1], &right_positional_args.data[i],
          &dest);
    }
  }
  if(dest.t != NIL) { THROW_ERROR(dynamic_vars, dest); }
  right_positional_args.data[0].t = BOOLEAN;
  left_positional_args.size = 0;
  right_positional_args.size = 1;
  dest = continuation;
  continuation.t = NIL;
  CALL_FUNC(dest)

c_add:
  REQUIRED_FUNCTION(continuation)
  NO_KEYWORD_ARGUMENTS
  if(left_positional_args.size + right_positional_args.size == 0) {
    MIN_RIGHT_ARGS(1)
  }
  dest.t = NIL;
  if(left_positional_args.size > 0) {
    for(i = 1; i < left_positional_args.size; ++i) {
      builtin_add(&left_positional_args.data[0], &left_positional_args.data[i],
          &left_positional_args.data[0], &dest);
      if(dest.t != NIL) { THROW_ERROR(dynamic_vars, dest); }
    }
    for(i = 0; i < right_positional_args.size; ++i) {
      builtin_add(&left_positional_args.data[0], &right_positional_args.data[i],
          &left_positional_args.data[0], &dest);
      if(dest.t != NIL) { THROW_ERROR(dynamic_vars, dest); }
    }
    right_positional_args.data[0] = left_positional_args.data[0];
  } else {
    for(i = 1; i < right_positional_args.size; ++i) {
      builtin_add(&right_positional_args.data[0],
          &right_positional_args.data[i], &right_positional_args.data[0],
          &dest);
      if(dest.t != NIL) { THROW_ERROR(dynamic_vars, dest); }
    }
  }
  left_positional_args.size = 0;
  right_positional_args.size = 1;
  dest = continuation;
  continuation.t = NIL;
  CALL_FUNC(dest)

c_multiply:
  REQUIRED_FUNCTION(continuation)
  NO_KEYWORD_ARGUMENTS
  if(left_positional_args.size + right_positional_args.size == 0) {
    MIN_RIGHT_ARGS(1)
  }
  dest.t = NIL;
  if(left_positional_args.size > 0) {
    for(i = 1; i < left_positional_args.size; ++i) {
      builtin_multiply(&left_positional_args.data[0],
          &left_positional_args.data[i], &left_positional_args.data[0], &dest);
      if(dest.t != NIL) { THROW_ERROR(dynamic_vars, dest); }
    }
    for(i = 0; i < right_positional_args.size; ++i) {
      builtin_multiply(&left_positional_args.data[0],
          &right_positional_args.data[i], &left_positional_args.data[0], &dest);
      if(dest.t != NIL) { THROW_ERROR(dynamic_vars, dest); }
    }
    right_positional_args.data[0] = left_positional_args.data[0];
  } else {
    for(i = 1; i < right_positional_args.size; ++i) {
      builtin_multiply(&right_positional_args.data[0],
          &right_positional_args.data[i], &right_positional_args.data[0],
          &dest);
      if(dest.t != NIL) { THROW_ERROR(dynamic_vars, dest); }
    }
  }
  left_positional_args.size = 0;
  right_positional_args.size = 1;
  dest = continuation;
  continuation.t = NIL;
  CALL_FUNC(dest)

c_subtract:
  REQUIRED_FUNCTION(continuation)
  NO_KEYWORD_ARGUMENTS
  dest.t = NIL;
  if(right_positional_args.size == 1) {
    MAX_LEFT_ARGS(1)
    if(left_positional_args.size == 0) {
      left_positional_args.data[0].t = INTEGER;
      left_positional_args.data[0].integer.value = 0;
    }
    builtin_subtract(&left_positional_args.data[0],
        &right_positional_args.data[0], &right_positional_args.data[0], &dest);
  } else {
    MAX_LEFT_ARGS(0)
    MIN_RIGHT_ARGS(2)
    MAX_RIGHT_ARGS(2)
    builtin_subtract(&right_positional_args.data[0],
        &right_positional_args.data[1], &right_positional_args.data[0], &dest);
  }
  if(dest.t != NIL) { THROW_ERROR(dynamic_vars, dest); }
  left_positional_args.size = 0;
  right_positional_args.size = 1;
  dest = continuation;
  continuation.t = NIL;
  CALL_FUNC(dest)

c_divide:
  REQUIRED_FUNCTION(continuation)
  NO_KEYWORD_ARGUMENTS
  dest.t = NIL;
  if(left_positional_args.size == 1 && right_positional_args.size == 1) {
    builtin_divide(&left_positional_args.data[0],
        &right_positional_args.data[0], &right_positional_args.data[0], &dest);
  } else {
    MAX_LEFT_ARGS(0)
    MIN_RIGHT_ARGS(2)
    MAX_RIGHT_ARGS(2)
    builtin_divide(&right_positional_args.data[0],
        &right_positional_args.data[1], &right_positional_args.data[0], &dest);
  }
  if(dest.t != NIL) { THROW_ERROR(dynamic_vars, dest); }
  left_positional_args.size = 0;
  right_positional_args.size = 1;
  dest = continuation;
  continuation.t = NIL;
  CALL_FUNC(dest)

c_modulo:
  REQUIRED_FUNCTION(continuation)
  NO_KEYWORD_ARGUMENTS
  dest.t = NIL;
  if(left_positional_args.size == 1 && right_positional_args.size == 1) {
    builtin_modulo(&left_positional_args.data[0],
        &right_positional_args.data[0], &right_positional_args.data[0], &dest);
  } else {
    MAX_LEFT_ARGS(0)
    MIN_RIGHT_ARGS(2)
    MAX_RIGHT_ARGS(2)
    builtin_modulo(&right_positional_args.data[0],
        &right_positional_args.data[1], &right_positional_args.data[0], &dest);
  }
  if(dest.t != NIL) { THROW_ERROR(dynamic_vars, dest); }
  left_positional_args.size = 0;
  right_positional_args.size = 1;
  dest = continuation;
  continuation.t = NIL;
  CALL_FUNC(dest)

c_new__object:
  REQUIRED_FUNCTION(continuation)
  NO_KEYWORD_ARGUMENTS
  MAX_LEFT_ARGS(0)
  MAX_RIGHT_ARGS(0)
  right_positional_args.size = 1;
  make_object(&right_positional_args.data[0]);
  dest = continuation;
  continuation.t = NIL;
  CALL_FUNC(dest);

c_seal__object:
  REQUIRED_FUNCTION(continuation)
  NO_KEYWORD_ARGUMENTS
  MAX_LEFT_ARGS(0)
  MIN_RIGHT_ARGS(1)
  MAX_RIGHT_ARGS(1)
  switch(right_positional_args.data[0].t) {
    default:
      dest = make_c_string("cannot seal non-object!");
      THROW_ERROR(dynamic_vars, dest);
    case OBJECT:
      seal_object(right_positional_args.data[0].object.data);
  }
  dest = continuation;
  continuation.t = NIL;
  CALL_FUNC(dest);

ho_throw:
  MAX_LEFT_ARGS(0)
  NO_KEYWORD_ARGUMENTS
  MAX_RIGHT_ARGS(1)
  MIN_RIGHT_ARGS(1)
  printf("Exception thrown!\n => ");
  dest.t = NIL;
  builtin_print(&right_positional_args.data[0], &dest);
  if(dest.t != NIL) { FATAL_ERROR("unable to print!", dest); }
  printf("\n");
  return 1;

c_halt:
  MAX_LEFT_ARGS(0)
  NO_KEYWORD_ARGUMENTS
  MAX_RIGHT_ARGS(1)
  MIN_RIGHT_ARGS(1)
  switch(right_positional_args.data[0].t) {
    case INTEGER:
      return right_positional_args.data[0].integer.value;
    case NIL:
      return 0;
    default:
      dest.t = NIL;
      if(builtin_istrue(&right_positional_args.data[0], &dest))
        return 0;
      if(dest.t != NIL) { FATAL_ERROR("failed exiting with value", dest); }
      return 1;
  }

c_Array:
  MAX_LEFT_ARGS(0)
  NO_KEYWORD_ARGUMENTS
  REQUIRED_FUNCTION(continuation)
  make_array_object(&dest, (struct Array**)&env);
  append_values(env, right_positional_args.data, right_positional_args.size);
  right_positional_args.data[0] = dest;
  left_positional_args.size = 0;
  right_positional_args.size = 1;
  dest = continuation;
  continuation.t = NIL;
  CALL_FUNC(dest);

c_DynamicVar:
  MAX_LEFT_ARGS(0)
  MAX_RIGHT_ARGS(0)
  NO_KEYWORD_ARGUMENTS
  REQUIRED_FUNCTION(continuation)
  right_positional_args.size = 1;
  make_object(&right_positional_args.data[0]);
  dest.t = CLOSURE;
  dest.closure.frame = NULL;
  dest.closure.env = make_key(dynamic_var_counter++);
  dest.closure.func = &&c_DynamicVar_call;
  set_field(right_positional_args.data[0].object.data,
      (struct ByteArray){"u_call", 6}, dest);
  dest.closure.func = &&c_DynamicVar_get;
  set_field(right_positional_args.data[0].object.data,
      (struct ByteArray){"u_get", 5}, dest);
  right_positional_args.data[0].object.data->env = dest.closure.env;
  dest.closure.env = NULL;
  dest.closure.func = &&c_DynamicVar;
  set_field(right_positional_args.data[0].object.data,
      (struct ByteArray){"u__7etype", 9}, dest);
  seal_object(right_positional_args.data[0].object.data);
  dest = continuation;
  continuation.t = NIL;
  CALL_FUNC(dest);

c_DynamicVar_get:
  MAX_LEFT_ARGS(0)
  MAX_RIGHT_ARGS(0)
  NO_KEYWORD_ARGUMENTS
  REQUIRED_FUNCTION(continuation)
  right_positional_args.size = 1;
  if(!(get_field(dynamic_vars.object.data, *((struct ByteArray*)env),
      &right_positional_args.data[0]))) {
    dest = make_c_string("dynamic variable missing!");
    THROW_ERROR(dynamic_vars, dest);
  }
  dest = continuation;
  continuation.t = NIL;
  CALL_FUNC(dest)

c_DynamicVar_call:
  MAX_LEFT_ARGS(0)
  MIN_RIGHT_ARGS(2)
  MAX_RIGHT_ARGS(2)
  NO_KEYWORD_ARGUMENTS
  REQUIRED_FUNCTION(continuation)
  copy_object(&dynamic_vars, &dest);
  set_field(dest.object.data, *((struct ByteArray*)env),
      right_positional_args.data[0]);
  seal_object(dest.object.data);
  dynamic_vars = dest;
  dest = right_positional_args.data[1];
  right_positional_args.size = 0;
  CALL_FUNC(dest)

c_external__function__call:
  REQUIRED_FUNCTION(continuation)
  NO_KEYWORD_ARGUMENTS
  if(!((ExternalFunction)frame)(env, &right_positional_args,
      &left_positional_args, &dest)) {
    THROW_ERROR(dynamic_vars, dest);
  }
  right_positional_args.data[0] = dest;
  right_positional_args.size = 1;
  left_positional_args.size = 0;
  dest = continuation;
  continuation.t = NIL;
  CALL_FUNC(dest);

c_register__main:
  MAX_LEFT_ARGS(0)
  MIN_RIGHT_ARGS(1)
  MAX_RIGHT_ARGS(1)
  REQUIRED_FUNCTION(right_positional_args.data[0])
  NO_KEYWORD_ARGUMENTS
  continuation = globals.c_continuation;
  dest = right_positional_args.data[0];
  right_positional_args.size = 0;
  reserve_space(&right_positional_args, argc);
  right_positional_args.size = argc;
  for(i = 0; i < argc; ++i) {
    right_positional_args.data[i] = make_byte_string(argv[i], strlen(argv[i]));
  }
  CALL_FUNC(dest);

c_type:
  MAX_LEFT_ARGS(0) MIN_RIGHT_ARGS(1) MAX_RIGHT_ARGS(1)
  REQUIRED_FUNCTION(continuation) NO_KEYWORD_ARGUMENTS
  switch(right_positional_args.data[0].t) {
    case INTEGER:
      right_positional_args.data[0] = globals.c_Integer; break;
    case FLOAT:
      right_positional_args.data[0] = globals.c_Float; break;
    case STRING:
      if(right_positional_args.data[0].string.byte_oriented) {
        right_positional_args.data[0] = globals.c_ByteString;
      } else {
        right_positional_args.data[0] = globals.c_CharString;
      }
      break;
    case BOOLEAN:
      right_positional_args.data[0] = globals.c_Boolean; break;
    case NIL:
      right_positional_args.data[0] = globals.c_Null; break;
    case CLOSURE:
      right_positional_args.data[0] = globals.c_Function; break;
    case OBJECT:
      if(get_field(right_positional_args.data[0].object.data,
          (struct ByteArray){"u__7etype", 9}, &dest)) {
        right_positional_args.data[0] = dest;
        break;
      } else {
        // fall through the switch statement
      }
    default:
      dest = make_c_string("unknown value type");
      THROW_ERROR(dynamic_vars, dest)
  }
  dest = continuation;
  continuation.t = NIL;
  CALL_FUNC(dest)

c_Integer:
  dest = make_c_string("TODO: constructor unimplemented!");
  THROW_ERROR(dynamic_vars, dest)
c_Float:
  dest = make_c_string("TODO: constructor unimplemented!");
  THROW_ERROR(dynamic_vars, dest)
c_ByteString:
  dest = make_c_string("TODO: constructor unimplemented!");
  THROW_ERROR(dynamic_vars, dest)
c_CharString:
  dest = make_c_string("TODO: constructor unimplemented!");
  THROW_ERROR(dynamic_vars, dest)
c_Boolean:
  MAX_LEFT_ARGS(0) MIN_RIGHT_ARGS(1) MAX_RIGHT_ARGS(1)
  REQUIRED_FUNCTION(continuation) NO_KEYWORD_ARGUMENTS
  dest.t = NIL;
  right_positional_args.data[0].boolean.value = builtin_istrue(
      &right_positional_args.data[0], &dest);
  right_positional_args.data[0].t = BOOLEAN;
  if(dest.t != NIL) { THROW_ERROR(dynamic_vars, dest); }
  dest = continuation;
  continuation.t = NIL;
  CALL_FUNC(dest)
c_Null:
  MAX_LEFT_ARGS(0) MAX_RIGHT_ARGS(0)
  REQUIRED_FUNCTION(continuation) NO_KEYWORD_ARGUMENTS
  right_positional_args.data[0].t = NIL;
  dest = continuation;
  continuation.t = NIL;
  CALL_FUNC(dest)
c_Function:
  dest = make_c_string("Cannot call function constructor directly!");
  THROW_ERROR(dynamic_vars, dest)

start:
