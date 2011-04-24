struct ObjectTree {
  char* key;
  unsigned int key_size;
  union Value value;
  struct ObjectTree* left;
  struct ObjectTree* right;
};

struct ObjectData {
  bool sealed;
  struct ObjectTree* tree;
};

static inline union Value make_object() {
  union Value v;
  v.t = OBJECT;
  v.object.data = GC_MALLOC(sizeof(struct ObjectData));
  v.object.data->sealed = false;
  v.object.data->tree = NULL;
  return v;
}

static inline struct ObjectTree* new_tree_node(char* key, unsigned int key_size,
    union Value* value) {
  struct ObjectTree* t;
  t = GC_MALLOC(sizeof(struct ObjectTree));
  t->key = key;
  t->key_size = key_size;
  t->value = *value;
  t->left = NULL;
  t->right = NULL;
}

static void _copy_object(struct ObjectTree* t1, struct ObjectTree** t2) {
  if(t1 == NULL) {
    *t2 = NULL;
    return;
  }
  *t2 = new_tree_node(t1->key, t1->key_size, &t1->value);
  _copy_object(t1->right, &(*t2)->left);
  _copy_object(t1->right, &(*t2)->right);
}

static inline void copy_object(struct ObjectData* d1, struct ObjectData* d2) {
  d2->sealed = false;
  _copy_object(d1->tree, &d2->tree);
}

static inline void seal_object(struct ObjectData* data) {
  data->sealed = true;
}

static bool _set_field(struct ObjectTree** tree, char* key,
    unsigned int key_size, union Value* value, bool sealed) {
  if(*tree == NULL) {
    if(sealed) return false;
    *tree = new_tree_node(key, key_size, value);
    return true;
  }
  switch (safe_strcmp(key, key_size, (*tree)->key, (*tree)->key_size)) {
    case 0:
      (*tree)->value = *value;
      return true;
    case -1:
      return _set_field(&(*tree)->left, key, key_size, value, sealed);
    default:
      return _set_field(&(*tree)->right, key, key_size, value, sealed);
  }
}

static inline bool set_field(struct ObjectData* data, char* key,
    unsigned int key_size, union Value* value) {
  _set_field(&data->tree, key, key_size, value, data->sealed);
}

static bool _get_field(struct ObjectTree* tree, char* key,
    unsigned int key_size, union Value* value) {
  if(tree == NULL) return false;
  switch (safe_strcmp(key, key_size, tree->key, tree->key_size)) {
    case 0:
      *value = tree->value;
      return true;
    case -1:
      return _get_field(tree->left, key, key_size, value);
    default:
      return _get_field(tree->right, key, key_size, value);
  }
}

static inline bool get_field(struct ObjectData* data, char* key,
    unsigned int key_size, union Value* value) {
  return _get_field(data->tree, key, key_size, value);
}
