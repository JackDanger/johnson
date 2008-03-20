#include "immutable_node.h"

static VALUE cNode;

static void deallocate(ImmutableNodeContext* context)
{
  if (context->pc)
  {
    js_FinishParseContext(context->js, context->pc);
    free(context->pc);
  }

  JS_DestroyContext(context->js);
  JS_DestroyRuntime(context->runtime);
  free(context);
}

static VALUE allocate(VALUE klass)
{
  ImmutableNodeContext * context = calloc(1, sizeof(ImmutableNodeContext));

  VALUE self = Data_Wrap_Struct(klass, 0, deallocate, context);

  assert(context->runtime = JS_NewRuntime(0x100000));
  assert(context->js = JS_NewContext(context->runtime, 8192));

  return self;
}

static VALUE parse_io(VALUE klass, VALUE stream) {
  VALUE self = allocate(klass);

  ImmutableNodeContext* context;
  Data_Get_Struct(self, ImmutableNodeContext, context);

  assert(context->pc = calloc(1, sizeof(JSParseContext)));
  
  VALUE file_contents = rb_funcall(stream, rb_intern("read"), 0);
  size_t length = NUM2INT(rb_funcall(file_contents, rb_intern("length"), 0));

  jschar* chars;
  assert(chars = js_InflateString(context->js, StringValuePtr(file_contents), &length));

  // FIXME: Ask +stream+ for its path as the filename
  assert(js_InitParseContext(context->js, context->pc, 
      NULL,
      chars,
      length,
      NULL, "boner", 0));

  context->node = js_ParseScript(context->js, 
      JS_NewObject(context->js, NULL, NULL, NULL),
      context->pc);

  return self;
}

static VALUE /* line */
line(VALUE self) {
  ImmutableNodeContext * ctx;

  Data_Get_Struct(self, ImmutableNodeContext, ctx);
  return INT2NUM(ctx->node->pn_pos.begin.lineno);
}

static VALUE /* index */
begin_index(VALUE self) {
  ImmutableNodeContext * ctx;

  Data_Get_Struct(self, ImmutableNodeContext, ctx);
  return INT2NUM(ctx->node->pn_pos.begin.index);
}

static VALUE /* pn_arity */
pn_arity(VALUE self) {
  ImmutableNodeContext * ctx;

  Data_Get_Struct(self, ImmutableNodeContext, ctx);
  switch(ctx->node->pn_arity) {
    case PN_FUNC:
      return ID2SYM(rb_intern("pn_func"));
    case PN_LIST:
      return ID2SYM(rb_intern("pn_list"));
    case PN_TERNARY:
      return ID2SYM(rb_intern("pn_ternary"));
    case PN_BINARY:
      return ID2SYM(rb_intern("pn_binary"));
    case PN_UNARY:
      return ID2SYM(rb_intern("pn_unary"));
    case PN_NAME:
      return ID2SYM(rb_intern("pn_name"));
    case PN_NULLARY:
      return ID2SYM(rb_intern("pn_nullary"));
  }
  return Qnil;
}

static VALUE /* pn_type */
pn_type(VALUE self) {
  ImmutableNodeContext * ctx;

  Data_Get_Struct(self, ImmutableNodeContext, ctx);
    /*
  switch(ctx->node->pn_type) {
    <% tokens.each do |token| %>
    case <%= token %>: return ID2SYM(rb_intern("<%= token.downcase %>"));
    <% end %>
  }
    */
  return INT2NUM(ctx->node->pn_type);
}

static VALUE /* pn_expr */
data_pn_expr(VALUE self) {
  ImmutableNodeContext * ctx;

  Data_Get_Struct(self, ImmutableNodeContext, ctx);
  if(ctx->node->pn_expr) {
    ImmutableNodeContext *roc;
    VALUE node = Data_Make_Struct(cNode, ImmutableNodeContext, NULL, NULL, roc);
    roc->js = ctx->js;
    roc->node = ctx->node->pn_expr;
    return node;
  }
  return Qnil;
}

static VALUE /* pn_kid */
data_pn_kid(VALUE self) {
  ImmutableNodeContext * ctx;

  Data_Get_Struct(self, ImmutableNodeContext, ctx);
  if(ctx->node->pn_kid) {
    ImmutableNodeContext *roc;
    VALUE node = Data_Make_Struct(cNode, ImmutableNodeContext, NULL, NULL, roc);
    roc->js = ctx->js;
    roc->node = ctx->node->pn_kid;
    return node;
  }
  return Qnil;
}

static VALUE /* pn_dval */
data_pn_dval(VALUE self) {
  ImmutableNodeContext * ctx;

  Data_Get_Struct(self, ImmutableNodeContext, ctx);
  if(ATOM_IS_DOUBLE(ctx->node->pn_atom)) {
    return rb_float_new(ctx->node->pn_dval);
  } else {
    return INT2NUM((int)(ctx->node->pn_dval));
  }
}

static VALUE /* pn_op */
data_pn_op(VALUE self) {
  ImmutableNodeContext * ctx;

  Data_Get_Struct(self, ImmutableNodeContext, ctx);
  switch(ctx->node->pn_op) {
    /*
    <% jsops.each do |jsop| %>
    case <%= jsop %>: return ID2SYM(rb_intern("<%= jsop.downcase %>"));
    <% end %>
    */
  }
  return INT2NUM(ctx->node->pn_op);
}

static VALUE /* pn_left */
data_pn_left(VALUE self) {
  ImmutableNodeContext * ctx;
  JSFunction * f;
  JSObject * object;

  Data_Get_Struct(self, ImmutableNodeContext, ctx);

  if(ctx->node->pn_left) {
    ImmutableNodeContext *roc;
    VALUE node = Data_Make_Struct(cNode, ImmutableNodeContext, NULL, NULL, roc);
    roc->js = ctx->js;
    roc->node = ctx->node->pn_left;
    return node;
  }
  return Qnil;
}

static VALUE /* name */
name(VALUE self) {
  ImmutableNodeContext * ctx;

  Data_Get_Struct(self, ImmutableNodeContext, ctx);
  return rb_str_new2(JS_GetStringBytes(ATOM_TO_STRING(ctx->node->pn_atom)));
}

static VALUE /* regexp */
regexp(VALUE self) {
  ImmutableNodeContext * ctx;
  jsval result;

  Data_Get_Struct(self, ImmutableNodeContext, ctx);
  js_regexp_toString(ctx->js, ctx->node->pn_pob->object, &result);
  return rb_str_new2(JS_GetStringBytes(JSVAL_TO_STRING(result)));
}

static VALUE /* function_name */
function_name(VALUE self) {
  ImmutableNodeContext * ctx;
  JSFunction * f;
  JSObject * object;

  Data_Get_Struct(self, ImmutableNodeContext, ctx);
  object = ctx->node->pn_funpob->object;
  f = (JSFunction *)JS_GetPrivate(ctx->js, ctx->node->pn_funpob->object);

  if(f->atom) {
    return rb_str_new2(JS_GetStringBytes(ATOM_TO_STRING(f->atom)));
  } else {
    return Qnil;
  }
}

static VALUE /* function_args */
function_args(VALUE self) {
  ImmutableNodeContext * ctx;
  JSFunction * f;
  JSObject * object;
  JSAtom ** names;
  VALUE func_args;
  int i;

  Data_Get_Struct(self, ImmutableNodeContext, ctx);
  object = ctx->node->pn_funpob->object;
  f = (JSFunction *)JS_GetPrivate(ctx->js, ctx->node->pn_funpob->object);

  func_args = rb_ary_new2(f->nargs);
  if(f->nargs > 0) {
    names = js_GetLocalNames(ctx->js, f, &ctx->js->tempPool, NULL);
    for(i = 0; i < f->nargs; i++) {
      rb_ary_push(func_args,
          rb_str_new2(JS_GetStringBytes(ATOM_TO_STRING(names[i])))
          );
    }
  }
  return func_args;
}

static VALUE /* function_body */
function_body(VALUE self) {
  ImmutableNodeContext * ctx;
  JSFunction * f;
  JSObject * object;

  Data_Get_Struct(self, ImmutableNodeContext, ctx);
  object = ctx->node->pn_funpob->object;
  f = (JSFunction *)JS_GetPrivate(ctx->js, ctx->node->pn_funpob->object);

  if(ctx->node->pn_body) {
    ImmutableNodeContext *roc;
    VALUE node = Data_Make_Struct(cNode, ImmutableNodeContext, NULL, NULL, roc);
    roc->js = ctx->js;
    roc->node = ctx->node->pn_body;
    return node;
  }
  return Qnil;
}

static VALUE /* pn_right */
data_pn_right(VALUE self)
{
  ImmutableNodeContext * ctx;
  JSFunction * f;
  JSObject * object;

  Data_Get_Struct(self, ImmutableNodeContext, ctx);

  if(ctx->node->pn_right) {
    ImmutableNodeContext *roc;
    VALUE node = Data_Make_Struct(cNode, ImmutableNodeContext, NULL, NULL, roc);
    roc->js = ctx->js;
    roc->node = ctx->node->pn_right;
    return node;
  }
  return Qnil;
}

static VALUE /* children */
children(VALUE self) {
  ImmutableNodeContext * ctx;
  JSParseNode * p;
  VALUE children;

  Data_Get_Struct(self, ImmutableNodeContext, ctx);
  children = rb_ary_new();
  for(p = ctx->node->pn_head; p != NULL; p = p->pn_next) {
    ImmutableNodeContext *roc;
    VALUE node = Data_Make_Struct(cNode, ImmutableNodeContext, NULL, NULL, roc);
    roc->js = ctx->js;
    roc->node = p;

    rb_ary_push(children, node);
  }

  return children;
}

void init_Johnson_SpiderMonkey_Immutable_Node(VALUE spidermonkey)
{
  cNode = rb_define_class_under(spidermonkey, "ImmutableNode", rb_cObject);

  rb_define_alloc_func(cNode, allocate);
  rb_define_singleton_method(cNode, "parse_io", parse_io, 1);
  rb_define_method(cNode, "line", line, 0);
  rb_define_method(cNode, "index", begin_index, 0);
  rb_define_method(cNode, "pn_arity", pn_arity, 0);
  rb_define_method(cNode, "pn_type", pn_type, 0);
  rb_define_method(cNode, "pn_expr", data_pn_expr, 0);
  rb_define_method(cNode, "pn_kid", data_pn_kid, 0);
  rb_define_method(cNode, "pn_dval", data_pn_dval, 0);
  rb_define_method(cNode, "pn_op", data_pn_op, 0);
  rb_define_method(cNode, "pn_left", data_pn_left, 0);
  rb_define_method(cNode, "name", name, 0);
  rb_define_method(cNode, "regexp", regexp, 0);
  rb_define_method(cNode, "function_name", function_name, 0);
  rb_define_method(cNode, "function_args", function_args, 0);
  rb_define_method(cNode, "function_body", function_body, 0);
  rb_define_method(cNode, "pn_right", data_pn_right, 0);
  rb_define_method(cNode, "children", children, 0);
}
