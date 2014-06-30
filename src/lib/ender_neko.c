/* ESCEN - Ender's based scene loader
 * Copyright (C) 2010 Jorge Luis Zapata
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.
 * If not, see <http://www.gnu.org/licenses/>.
 */
#include "Ender.h"
#include <neko.h>
#include <neko_vm.h>
#include <ctype.h>
/*============================================================================*
 *                                  Local                                     *
 *============================================================================*/
/* We need to get the environment when a function is called. For that we need
 * create this macro given that the vm struct is not exported on neko
 */
#define NEKOVM_ENV(v) *((value *)((char *)(v) + sizeof(void *) + sizeof(void *)))

/* TODO Add a helper macro to get the item related to a function call */
/* TODO Add a helper macro to set the item related to a function call */

#define ERR(...) EINA_LOG_DOM_ERR(_log, __VA_ARGS__)
#define WRN(...) EINA_LOG_DOM_WARN(_log, __VA_ARGS__)
#define INF(...) EINA_LOG_DOM_INFO(_log, __VA_ARGS__)
#define DBG(...) EINA_LOG_DOM_DBG(_log, __VA_ARGS__)
#define CRI(...) EINA_LOG_DOM_CRIT(_log, __VA_ARGS__)

typedef struct _Ender_Neko_Object
{
	/* the item associated with the data */
	Ender_Item *i;
	/* the real allocated object */
	void *o;
} Ender_Neko_Object;

DEFINE_KIND(k_obj);
DEFINE_KIND(k_item);
DEFINE_KIND(k_lib);
DEFINE_ENTRY_POINT(ender_neko_init)

static int _log = -1;
static Eina_Hash *_protos;

/* Forward declarations */
static value ender_neko_object_new(Ender_Item *i, void *o);
static value ender_neko_def_new(Ender_Item *i, Ender_Value *v);


static char * ender_neko_toupper(const char *str)
{
	char *ret;
	char *tmp;
	size_t len;

	len = strlen(str);
	ret = tmp = malloc(len + 1);
	while (*str)
	{
		*tmp = toupper(*str);
		tmp++;
		str++;
	}
	ret[len] = '\0';
	return ret;
}
/*----------------------------------------------------------------------------*
 *                            Ender Neko objects                              *
 *----------------------------------------------------------------------------*/
static void ender_neko_obj_finalize(value v)
{
	Ender_Neko_Object *no = val_data(v);

	ender_item_unref(no->i);
	/* TODO we might need to call the dtor */
	free(no->o);
	free(no);
}

static value ender_neko_obj_to_string(void)
{
	Ender_Neko_Object *no;
	value o;
	value ret;
	value intptr;
	buffer b;

	o = val_this();
	intptr = val_field(o, val_id("__intptr")); 
	val_check_kind(intptr, k_obj);
	no = val_data(intptr);
	b = alloc_buffer(ender_item_name_get(no->i));

	return buffer_to_string(b);
}

static value ender_neko_obj_new(Ender_Item *i, void *o)
{
	Ender_Neko_Object *no;
	value intptr;
	value ret;
	value f;

	no = calloc(1, sizeof(Ender_Neko_Object));
	no->i = i;
	no->o = o;

	intptr = alloc_abstract(k_obj, no);
	val_gc(intptr, ender_neko_obj_finalize);

	ret = alloc_object(NULL);
	alloc_field(ret, val_id("__intptr"), intptr);

	/* set the to_string function */
	f = alloc_function(ender_neko_obj_to_string, 0, "ender_neko_obj_to_string");
	alloc_field(ret, val_id("__string"), f);

	DBG("New object for item '%s'", ender_item_name_get(i));
	return ret;
}

/*----------------------------------------------------------------------------*
 *                                Values                                      *
 *----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*
 *                                 Items                                      *
 *----------------------------------------------------------------------------*/
static void ender_neko_item_finalize(value v)
{
	ender_item_unref(val_data(v));
}

static value ender_neko_item_new(Ender_Item *i)
{
	value ret;

	ret = alloc_abstract(k_item, i);
	val_gc(ret, ender_neko_item_finalize);
	return ret;
}

static value ender_neko_item_to_string(void)
{
	Ender_Item *i;
	value o;
	value ret;
	value intptr;
	buffer b;

	o = val_this();
	intptr = val_field(o, val_id("__intptr")); 
	val_check_kind(intptr, k_item);
	i = val_data(intptr);
	b = alloc_buffer(ender_item_name_get(i));

	return buffer_to_string(b);
}

static void ender_neko_item_initialize(value v, Ender_Item *i)
{
	value intptr;
	value f;

	intptr = ender_neko_item_new(i);
	/* set the abstract */
	alloc_field(v, val_id("__intptr"), intptr);
	/* set the to_string function */
	f = alloc_function(ender_neko_item_to_string, 0, "ender_neko_item_to_string");
	alloc_field(v, val_id("__string"), f);
}

/*----------------------------------------------------------------------------*
 *                                  Basic                                     *
 *----------------------------------------------------------------------------*/
static value ender_neko_basic_new(Ender_Item *i, Ender_Value *v)
{
	value ret = val_null;

	switch (ender_item_basic_value_type_get(i))
	{
		case ENDER_VALUE_TYPE_BOOL:
		ret = alloc_bool(v->b);
		break;

		case ENDER_VALUE_TYPE_UINT32:
		ret = alloc_best_int(v->u32);
		break;

		case ENDER_VALUE_TYPE_INT32:
		ret = alloc_best_int(v->u32);
		break;

		case ENDER_VALUE_TYPE_DOUBLE:
		ret = alloc_float(v->d);
		break;

		case ENDER_VALUE_TYPE_STRING:
		ret = alloc_string(v->ptr);
		break;

		case ENDER_VALUE_TYPE_POINTER:
		case ENDER_VALUE_TYPE_UINT64:
		case ENDER_VALUE_TYPE_INT64:
		default:
		failure("Unsupported value type");
		break;
	}
	return ret;
}

static Eina_Bool ender_neko_basic_from_val(Ender_Item *i, Ender_Value *v, value val)
{
	Ender_Value_Type vtype;

	vtype = ender_item_basic_value_type_get(i);
	switch (vtype)
	{
		case ENDER_VALUE_TYPE_DOUBLE:
		{
			switch (val_type(val))
			{
				case VAL_INT:
				v->d = val_int(val);
				break;

				case VAL_FLOAT:
				v->d = val_float(val);
				break;

				default:
				failure("Unsupported neko value");
				break;
			}
		}
		break;

		case ENDER_VALUE_TYPE_INT8:
		{
			switch (val_type(val))
			{
				case VAL_INT:
				v->i8 = val_int(val);
				break;

				default:
				failure("Unsupported neko value");
				break;
			}
		}
		break;

		case ENDER_VALUE_TYPE_UINT8:
		{
			switch (val_type(val))
			{
				case VAL_INT:
				v->u8 = val_int(val);
				break;

				default:
				failure("Unsupported neko value");
				break;
			}
		}
		break;

		case ENDER_VALUE_TYPE_INT32:
		{
			switch (val_type(val))
			{
				case VAL_INT:
				v->i32 = val_int(val);
				break;

				case VAL_FLOAT:
				v->i32 = val_float(val);
				break;

				default:
				failure("Unsupported neko value");
				break;
			}
		}
		break;

		case ENDER_VALUE_TYPE_UINT32:
		{
			switch (val_type(val))
			{
				case VAL_INT:
				v->u32 = val_int(val);
				break;

				case VAL_FLOAT:
				v->u32 = val_float(val);
				break;

				default:
				failure("Unsupported neko value");
				break;
			}
		}
		break;

		case ENDER_VALUE_TYPE_POINTER:
		{
			switch (val_type(val))
			{
				case VAL_NULL:
				v->ptr = NULL;
				break;

				default:
				failure("Unsupported neko value");
				break;
			}
		}
		break;

		case ENDER_VALUE_TYPE_STRING:
		{
			switch (val_type(val))
			{
				case VAL_NULL:
				v->ptr = NULL;
				break;

				case VAL_STRING:
				v->ptr = val_string(val);
				break;

				default:
				failure("Unsupported neko value");
				break;
			}
		}
		break;

		default:
		CRI("Unsupported value type '%d'", vtype);
		failure("Unsupported value type");
		break;
	}
	return EINA_TRUE;
}

static Eina_Bool ender_neko_basic_to_val(Ender_Item *i, Ender_Value *v, value *val)
{
	Ender_Value_Type vtype;

	vtype = ender_item_basic_value_type_get(i);
	switch (vtype)
	{
		case ENDER_VALUE_TYPE_DOUBLE:
		*val = alloc_float(v->d);
		break;

		case ENDER_VALUE_TYPE_INT32:
		*val = alloc_int(v->i32);
		break;

		case ENDER_VALUE_TYPE_BOOL:
		*val = alloc_bool(v->b);
		break;

		default:
		CRI("Unsupported value type '%d'", vtype);
		failure("Unsupported value type");
	}
	return EINA_TRUE;
}
/*----------------------------------------------------------------------------*
 *                                  Arg                                       *
 *----------------------------------------------------------------------------*/
static Eina_Bool ender_neko_arg_from_val_full(Ender_Item *type,
		Ender_Item_Arg_Direction dir, Ender_Item_Arg_Transfer xfer,
		Ender_Value *v, value val)
{
	Eina_Bool ret = EINA_FALSE;

	/* handle the in/out direction */
	if (dir == ENDER_ITEM_ARG_DIRECTION_IN)
	{
		Ender_Item_Type it;

		it = ender_item_type_get(type);
		switch (it)
		{
			case ENDER_ITEM_TYPE_BASIC:
			ret = ender_neko_basic_from_val(type, v, val);
			break;

			case ENDER_ITEM_TYPE_DEF:
			{
				Ender_Item *other;

				other = ender_item_def_type_get(type);
				ret = ender_neko_arg_from_val_full(other, dir, xfer, v, val);
			}
			break;

			case ENDER_ITEM_TYPE_ENUM:
			v->i32 = val_int(val);
			ret = EINA_TRUE;
			break;

			case ENDER_ITEM_TYPE_OBJECT:
			case ENDER_ITEM_TYPE_STRUCT:
			{
				if (val_is_object(val))
				{
					value intptr;
					intptr = val_field(val, val_id("__intptr"));
					if (val_is_kind(intptr, k_obj))
					{
						Ender_Neko_Object *obj;
						obj = val_data(intptr);
						v->ptr = obj->o;
						ret = EINA_TRUE;
						/* in case of a transfer full, ref it again */
						if (it == ENDER_ITEM_TYPE_OBJECT && xfer == ENDER_ITEM_ARG_TRANSFER_FULL)
						{
							WRN("Transfer full");
						}
					}
				}
				else if (val_is_null(val))
				{
					v->ptr = NULL;
					ret = EINA_TRUE;
				}
				else
				{
					failure("Wrong data");
				}
			}
			break;

			default:
			ERR("Type %s", ender_item_type_name_get(it));
			failure("Unsupported ender value");
			break;
		}
	}
	else
	{
		/* for in/inout directions we always pass a pointer */
		ERR("Unsupported direction %d", dir);
		failure("Unsupported direction");
	}
	ender_item_unref(type);
	return ret;
}

/* convert the neko arg into an ender value */
static Eina_Bool ender_neko_arg_from_val(Ender_Item *i, Ender_Value *v, value val)
{
	Ender_Item *type;
	Ender_Item_Arg_Direction dir;
	Ender_Item_Arg_Transfer xfer;
	Eina_Bool ret = EINA_FALSE;

	type = ender_item_arg_type_get(i);

	if (!type)
	{
		ERR("No type found");
		v->ptr = NULL;
		return EINA_FALSE;
	}

	dir = ender_item_arg_direction_get(i);
	xfer = ender_item_arg_transfer_get(i);

	ret = ender_neko_arg_from_val_full(type, dir, xfer, v, val);
	return ret;
}

static Eina_Bool ender_neko_arg_to_val(Ender_Item *i, Ender_Value *v, Eina_Bool is_ret, value *val)
{
	Ender_Item *type;
	Ender_Item_Arg_Direction dir;
	Eina_Bool ret = EINA_FALSE;

	type = ender_item_arg_type_get(i);
	dir = ender_item_arg_direction_get(i);

	/* TODO handle the transfer */
	/* handle the in/out direction */
	if (dir == ENDER_ITEM_ARG_DIRECTION_IN)
	{
		Ender_Item_Type it;

		it = ender_item_type_get(type);
		switch (it)
		{
			case ENDER_ITEM_TYPE_BASIC:
			ret = ender_neko_basic_to_val(type, v, val);
			break;

			case ENDER_ITEM_TYPE_OBJECT:
			*val = ender_neko_object_new(ender_item_ref(type), v->ptr);
			ret = EINA_TRUE;
			break;

			case ENDER_ITEM_TYPE_DEF:
			*val = ender_neko_def_new(ender_item_ref(type), v);
			ret = EINA_TRUE;
			break;

			default:
			ERR("Type %s", ender_item_type_name_get(it));
			failure("Unsupported ender value");
			break;
		}
	}
	else
	{
		ERR("Unsupported direction %d", dir);
		failure("Unsupported direction");
	}
	ender_item_unref(type);
	return ret;
}
/*----------------------------------------------------------------------------*
 *                               Functions                                    *
 *----------------------------------------------------------------------------*/
static value ender_neko_function_object_call(Ender_Item *i, Ender_Neko_Object *obj,
		value *args, int nargs)
{
	Ender_Item *type;
	Ender_Item *i_ret;
	Ender_Value ret_val;
	Ender_Value *passed_args = NULL;
	value ret = val_true;
	int nnargs;
	int arg = 0;
	int neko_arg = 0;
	int flags;

	/* check the number of arguments */
	nnargs = ender_item_function_args_count(i);
	flags = ender_item_function_flags_get(i);

	DBG("Calling function '%s' with flags: '%08x', nargs: %d", ender_item_name_get(i), flags, nargs);
	/* we increment by one nargs because for methods the first argument
	 * is the object itself
	 */
	if (flags & ENDER_ITEM_FUNCTION_FLAG_IS_METHOD)
	{
		/* we must have a self */
		if (!obj)
			failure("Wrong self object");
		nargs++;
	}

	if (nnargs != nargs)
		failure("Invalid number of arguments");

	/* setup the args */
	if (nnargs)
	{
		Eina_List *info_args;
		Ender_Item *a;

		passed_args = calloc(nnargs, sizeof(Ender_Value));

		/* set the args */
		info_args = ender_item_function_args_get(i);
		/* set self */
		if (flags & ENDER_ITEM_FUNCTION_FLAG_IS_METHOD)
		{
			passed_args[arg].ptr = obj->o;
			arg++;
			ender_item_unref(info_args->data);
			info_args = eina_list_remove_list(info_args, info_args);
		}

		EINA_LIST_FREE(info_args, a)
		{
			ender_neko_arg_from_val(a, &passed_args[arg], args[neko_arg]);
			neko_arg++;
			arg++;
			ender_item_unref(a);
		}
	}
	ender_item_function_call(i, passed_args, &ret_val);
	i_ret = ender_item_function_ret_get(i);
	if (i_ret)
	{
		ender_neko_arg_to_val(i_ret, &ret_val, EINA_TRUE, &ret);
		ender_item_unref(i_ret);
	}
	free(passed_args);

	return ret;
}

static value ender_neko_function_call(value *args, int nargs)
{
	Ender_Neko_Object *obj = NULL;
	Ender_Item *i;
	value intptr;
	int flags;

	/* get our function environment */
	intptr = NEKOVM_ENV(neko_vm_current());
	i = val_data(intptr);

	flags = ender_item_function_flags_get(i);
	if (flags & ENDER_ITEM_FUNCTION_FLAG_IS_METHOD)
	{
		value self;

		/* check that we have a valid object */
		self = val_this();
		intptr = val_field(self, val_id("__intptr")); 
		val_check_kind(intptr, k_obj);
		obj = val_data(intptr);
	}

	return ender_neko_function_object_call(i, obj, args, nargs);
}

static value ender_neko_function_new(Ender_Item *f)
{
	value ret;
	value intptr;

	ret = alloc_function(ender_neko_function_call, VAR_ARGS,
			ender_item_name_get(f));
	intptr = ender_neko_item_new(f);
	((vfunction *)ret)->env = intptr;

	return ret;
}
/*----------------------------------------------------------------------------*
 *                                  Attrs                                      *
 *----------------------------------------------------------------------------*/
static value ender_neko_attr_set(value *args, int nargs)
{
	Ender_Neko_Object *obj;
	Ender_Item *i;
	Ender_Item *type;
	Ender_Item_Type itype;
	Ender_Item_Arg_Direction dir;
	Ender_Item_Arg_Transfer xfer;
	Eina_Bool valid;
	Ender_Value v;
	value intptr;
	value self;
	
	/* get our function environment */
	intptr = NEKOVM_ENV(neko_vm_current());
	i = val_data(intptr);

	/* check that we have a valid object */
	self = val_this();
	intptr = val_field(self, val_id("__intptr")); 
	val_check_kind(intptr, k_obj);
	obj = val_data(intptr);

	if (nargs != 1)
		failure("Invalid number of arguments");

	/* finally set the value */
	type = ender_item_attr_type_get(i);
	itype = ender_item_type_get(type);

	/* TODO add a way to get the direction/transfer from the getter/setter */
	dir = ENDER_ITEM_ARG_DIRECTION_IN;
	xfer = ENDER_ITEM_ARG_TRANSFER_FULL;

	valid = ender_neko_arg_from_val_full(type, dir, xfer, &v, args[0]);
	if (valid)
	{
		/* TODO handle exceptions */
		valid = ender_item_attr_value_set(i, obj->o, &v, NULL);
	}
	ender_item_unref(type);

	if (valid)
		return val_true;
	else
		return val_false;
}

static value ender_neko_attr_get(value *args, int nargs)
{
	Ender_Neko_Object *obj;
	Ender_Item *i;
	Ender_Item *type;
	Ender_Value v;
	value self;
	value intptr;
	value ret = val_null;

	/* get our function environment */
	intptr = NEKOVM_ENV(neko_vm_current());
	i = val_data(intptr);

	/* check that we have a valid object */
	self = val_this();
	intptr = val_field(self, val_id("__intptr")); 
	val_check_kind(intptr, k_obj);
	obj = val_data(intptr);

	if (nargs != 0)
		failure("Invalid number of arguments");

	/* finally get the value */
	ender_item_attr_value_get(i, obj->o, &v, NULL);
	type = ender_item_attr_type_get(i);
	switch (ender_item_type_get(type))
	{
		case ENDER_ITEM_TYPE_BASIC:
		ret = ender_neko_basic_new(type, &v);
		break;

		default:
		failure("Unsupported type");
		break;
	}
	ender_item_unref(type);

	return ret;
}
/*----------------------------------------------------------------------------*
 *                                    Defs                                    *
 *----------------------------------------------------------------------------*/
static value ender_neko_def_new(Ender_Item *i, Ender_Value *v)
{
	Ender_Item *it;
	value ret;

	DBG("Creating def for '%s'", ender_item_name_get(i));
	/* create the def */
	it = ender_item_def_type_get(i);
	switch (ender_item_type_get(it))
	{
		case ENDER_ITEM_TYPE_BASIC:
		ret = ender_neko_basic_new(it, v);
		break;

		default:
		failure("Unsupported type");
		break;
	}
	ender_item_unref(it);
	ender_item_unref(i);

	return ret;
}

static value ender_neko_def_ctor(void)
{
	Ender_Item *i;
	Ender_Value val = { 0 };
	value v = val_this();
	value intptr;

	intptr = val_field(v, val_id("__intptr"));
	val_check_kind(intptr, k_item);

	i = val_data(intptr);
	DBG("Def constructor for '%s'", ender_item_name_get(i));
	return ender_neko_def_new(ender_item_ref(i), &val);

}

static value ender_neko_def_generate_class(Ender_Item *i)
{
	Ender_Item *it;
	value ret;
	value f;
	Eina_List *items;

	ret = alloc_object(NULL);
	ender_neko_item_initialize(ret, i);

	/* ctor */
	f = alloc_function(ender_neko_def_ctor, 0, "new");
	alloc_field(ret, val_id("new"), f);

	/* functions  */
	items = ender_item_def_functions_get(i);
	EINA_LIST_FREE(items, it)
	{
		/* only add the method functions */
		f = ender_neko_function_new(ender_item_ref(it));
		alloc_field(ret, val_id(ender_item_name_get(it)), f);
		ender_item_unref(it);
	}

	return ret;	
}
/*----------------------------------------------------------------------------*
 *                                 Structs                                    *
 *----------------------------------------------------------------------------*/
static value ender_neko_struct_new(void)
{
	Ender_Item *i;
	Ender_Item *f;
	Eina_List *items;
	value v = val_this();
	value intptr;
	value ret;
	void *o;
	size_t size;

	intptr = val_field(v, val_id("__intptr"));
	val_check_kind(intptr, k_item);

	i = val_data(intptr);

	/* create the struct */
	size = ender_item_struct_size_get(i);
	o = calloc(size, 1);
	ret = ender_neko_obj_new(ender_item_ref(i), o);

	/* iterate over the fields */
	items = ender_item_struct_fields_get(i);
	EINA_LIST_FREE(items, f)
	{
		value getter;
		value setter;
		char *set_name;
		char *get_name;

		/* given that neko does not support generic setters/getters
		 * we need to create the functions ourselves
		 */
		if (asprintf(&set_name, "set_%s", ender_item_name_get(f)) < 0)
			break;
		if (asprintf(&get_name, "get_%s", ender_item_name_get(f)) < 0)
			break;

		setter = alloc_function(ender_neko_attr_set, VAR_ARGS, set_name);
		intptr = ender_neko_item_new(ender_item_ref(f));
		((vfunction *)setter)->env = intptr;

		getter = alloc_function(ender_neko_attr_get, VAR_ARGS, get_name);
		intptr = ender_neko_item_new(ender_item_ref(f));
		((vfunction *)getter)->env = intptr;

		alloc_field(ret, val_id(get_name), getter);
		alloc_field(ret, val_id(set_name), setter);
		ender_item_unref(f);
	}
	/* iterate over the functions */
	items = ender_item_struct_functions_get(i);
	EINA_LIST_FREE(items, f)
	{
		value function;

		/* only add the method functions */
		if (!(ender_item_function_flags_get(f) & ENDER_ITEM_FUNCTION_FLAG_IS_METHOD))
		{
			ender_item_unref(f);
			continue;
		}
		function = ender_neko_function_new(ender_item_ref(f));
		alloc_field(ret, val_id(ender_item_name_get(f)), function);
		ender_item_unref(f);
	}
	return ret;
}

static value ender_neko_struct_generate_class(Ender_Item *i)
{
	value ret;
	value f;

	ret = alloc_object(NULL);
	ender_neko_item_initialize(ret, i);

	/* ctor */
	f = alloc_function(ender_neko_struct_new, 0, "new");
	alloc_field(ret, val_id("new"), f);

	return ret;	
}
/*----------------------------------------------------------------------------*
 *                                 Enums                                      *
 *----------------------------------------------------------------------------*/
static value ender_neko_enum_generate_class(Ender_Item *i)
{
	Ender_Item *item;
	Eina_List *items;
	value ret;

	ret = alloc_object(NULL);
	ender_neko_item_initialize(ret, i);

	/* generate the values */
	items = ender_item_enum_values_get(i);
	EINA_LIST_FREE(items, item)
	{
		Ender_Value v;
		value f;
		char *name;

		ender_item_constant_value_get(item, &v);
		/* make the names be capitalized */
		name = ender_neko_toupper(ender_item_name_get(item));
		f = alloc_int(v.i32);
		alloc_field(ret, val_id(name), f);
		free(name);
		ender_item_unref(item);
	}
	return ret;
}
/*----------------------------------------------------------------------------*
 *                                Objects                                     *
 *----------------------------------------------------------------------------*/
static value ender_neko_object_generate_proto(Ender_Item *i)
{
	Ender_Item *f;
	Ender_Item *inherit;
	Eina_List *items;
	value proto;
	value parent_proto = val_null;

	proto = eina_hash_find(_protos, ender_item_name_get(i));
	if (proto) return proto;

	DBG("Creating proto for '%s'", ender_item_name_get(i));
	/* get the inheritance */
	inherit = ender_item_object_inherit_get(i);
	if (inherit)
	{
		parent_proto = ender_neko_object_generate_proto(inherit);
		ender_item_unref(inherit);
	}
	proto = alloc_object(NULL);

	/* iterate over the properties */
	items = ender_item_object_props_get(i);
	EINA_LIST_FREE(items, f)
	{
		value intptr;
		value getter;
		value setter;
		char *get_name;
		char *set_name;


		/* given that neko does not support generic setters/getters
		 * we need to create the functions ourselves
		 */
		if (asprintf(&get_name, "get_%s", ender_item_name_get(f)) < 0)
			break;
		if (asprintf(&set_name, "set_%s", ender_item_name_get(f)) < 0)
			break;

		getter = alloc_function(ender_neko_attr_get, VAR_ARGS, get_name);
		intptr = ender_neko_item_new(ender_item_ref(f));
		((vfunction *)getter)->env = intptr;

		setter = alloc_function(ender_neko_attr_set, VAR_ARGS, set_name);
		intptr = ender_neko_item_new(ender_item_ref(f));
		((vfunction *)setter)->env = intptr;

		alloc_field(proto, val_id(set_name), setter);
		alloc_field(proto, val_id(get_name), getter);
		ender_item_unref(f);
	}

	/* iterate over the functions */
	items = ender_item_object_functions_get(i);
	EINA_LIST_FREE(items, f)
	{
		value function;

		/* only add the method functions */
		if (!(ender_item_function_flags_get(f) & ENDER_ITEM_FUNCTION_FLAG_IS_METHOD))
		{
			ender_item_unref(f);
			continue;
		}
		function = ender_neko_function_new(ender_item_ref(f));
		alloc_field(proto, val_id(ender_item_name_get(f)), function);
		ender_item_unref(f);
	}
	eina_hash_add(_protos, ender_item_name_get(i), proto);
	((vobject *)proto)->proto = (vobject *)parent_proto;

	return proto;
}

static value ender_neko_object_new(Ender_Item *i, void *o)
{
	value ret;
	value proto;

	DBG("Creating object for '%s'", ender_item_name_get(i));
	ret = ender_neko_obj_new(i, o);
	proto = ender_neko_object_generate_proto(i);
	((vobject *)ret)->proto = (vobject *)proto;

	return ret;
}

static value ender_neko_object_ctor(value *args, int nargs)
{
	Ender_Item *i;
	Ender_Item *item;
	Eina_List *items;
	value intptr;
	value self;
	value ret = val_null;

	/* get the class that invokd new() */
	intptr = val_field(val_this(), val_id("__intptr")); 
	val_check_kind(intptr, k_item);
	i = val_data(intptr);

	DBG("Object constructor for '%s'", ender_item_name_get(i));
	/* TODO find the ctor that matches the type of args */
	items = ender_item_object_ctor_get(i);
	EINA_LIST_FREE(items, item)
	{
		int nnargs;

		nnargs = ender_item_function_args_count(item);
		if (nargs != nnargs)
		{
			ender_item_unref(item);
			continue;
		}
		/* so far we have the same number of arguments */
		DBG("Constructor '%s' found, calling it", ender_item_name_get(item));
		ret = ender_neko_function_object_call(item, NULL, args, nargs);
		ender_item_unref(item);
	}

	if (ret == val_null)
	{
		failure("Impossible to find a constructor, check your args");
	}

	return ret;
}

static value ender_neko_object_generate_class(Ender_Item *i)
{
	Eina_List *items;
	Ender_Item *item;
	Eina_Bool has_ctor = EINA_FALSE;
	value ret;

	ret = alloc_object(NULL);
	ender_neko_item_initialize(ret, i);

	/* ctor */
	items = ender_item_object_functions_get(i);
	EINA_LIST_FREE(items, item)
	{
		int mask;

		mask = ender_item_function_flags_get(item);
		if (mask & ENDER_ITEM_FUNCTION_FLAG_IS_METHOD)
		{
			ender_item_unref(item);
			continue;
		}
		/* the ctor is a special case given that we need
		 * to find the best ctor function
		 */
		if (mask & ENDER_ITEM_FUNCTION_FLAG_CTOR)
		{
			/* only register once the ctor */
			if (!has_ctor)
			{
				value f;

				f = alloc_function(ender_neko_object_ctor, VAR_ARGS, "new");
				alloc_field(ret, val_id("new"), f);
				has_ctor = EINA_TRUE;
			}
		}
		/* everything else, add it as a class function */
		else
		{
			value f;

			f = ender_neko_function_new(ender_item_ref(item));
			alloc_field(ret, val_id(ender_item_name_get(item)), f);
		}
		ender_item_unref(item);
	}

	return ret;	
}
/*----------------------------------------------------------------------------*
 *                               Namespaces                                   *
 *----------------------------------------------------------------------------*/
static value ender_neko_namespace_generate_empty_class(const char *name, value rel)
{
	value ret;

	DBG("Creating intermediary namespace %s", name);
	ret = alloc_object(NULL);
	alloc_field(rel, val_id(name), ret);
	return ret;
}

static value ender_neko_namespace_generate_class(const char *name, Ender_Item *i, value rel)
{
	value ret = val_null;

	/* TODO it might be possible that the object to create already exists */
	/* finally generate the value */
	switch (ender_item_type_get(i))
	{
		case ENDER_ITEM_TYPE_STRUCT:
		ret = ender_neko_struct_generate_class(ender_item_ref(i));
		alloc_field(rel, val_id(name), ret);
		break;

		case ENDER_ITEM_TYPE_OBJECT:
		ret = ender_neko_object_generate_class(ender_item_ref(i));
		alloc_field(rel, val_id(name), ret);
		break;

		case ENDER_ITEM_TYPE_DEF:
		ret = ender_neko_def_generate_class(ender_item_ref(i));
		alloc_field(rel, val_id(name), ret);
		break;

		case ENDER_ITEM_TYPE_FUNCTION:
		ret = ender_neko_function_new(ender_item_ref(i));
		alloc_field(rel, val_id(name), ret);
		break;

		case ENDER_ITEM_TYPE_ENUM:
		ret = ender_neko_enum_generate_class(ender_item_ref(i));
		alloc_field(rel, val_id(name), ret);
		break;

		default:
		CRI("Unsupported type");
		break;
	}
	ender_item_unref(i);

	return ret;
}

/* for cases like foo.bar.s, we need to create intermediary empty objects */
static void ender_neko_namespace_generate(const Ender_Lib *lib, Ender_Item *i, value rel)
{
	value ret;
	value tmp;
	const char *orig;
	char *name, *token, *str, *current, *rname = NULL, *saveptr = NULL;
	int j;

	orig = ender_item_name_get(i);
	name = strdup(orig);
	rname = name;
	current = calloc(strlen(name) + 1, 1);
	
	for (j = 0, str = name; ; j++, str = NULL)
	{
		token = strtok_r(str, ".", &saveptr);
		if (!token)
			break;
		rname = token;
		/* first time, in case the first prefix is different
		 * from the main lib name add a new namespace
		 */
		if (!j && !strcmp(token, ender_lib_name_get(lib)))
		{
			strcat(current, token);
			continue;
		}
		strcat(current, ".");
		strcat(current, token);
		/* we skip the last one */
		if (strcmp(orig, current))
		{
			/* check if we have a namespace already */
			tmp = val_field(rel, val_id(rname));
			if (val_is_null(tmp))
			{
				Ender_Item *other;

				other = ender_lib_item_find(lib, current);
				if (!other)
				{
					rel = ender_neko_namespace_generate_empty_class(token, rel);
				}
				else
				{
					rel = ender_neko_namespace_generate_class(rname, other, rel);
				}
			}
			else
			{
				/* all the fields must be relative to this object */
				rel = tmp;
			}
		}
	}
	/* check that the object does not exist already */
	tmp = val_field(rel, val_id(rname));
	if (val_is_null(tmp))
	{
		ender_neko_namespace_generate_class(rname, i, rel);
	}

	free(current);
	free(name);
}

/*----------------------------------------------------------------------------*
 *                               Primitives                                   *
 *----------------------------------------------------------------------------*/
static value load(value api)
{
	const Ender_Lib *lib;
	Ender_Item *i;
	Eina_List *items;
	value ret;
	value intptr;
	value rel;

	if (!val_is_string(api))
		return val_null;
	lib = ender_lib_find(val_string(api));
	if (!lib)
		return val_null;

	/* create a lib object */
	ret = rel = alloc_object(NULL);
	intptr = alloc_abstract(k_lib, (void *)lib);
	alloc_field(ret, val_id("__intptr"), intptr);

	/* create all the possible objects,structs,enums as classes */
	items = ender_lib_item_list(lib, ENDER_ITEM_TYPE_STRUCT);
	items = eina_list_merge(items, ender_lib_item_list(lib, ENDER_ITEM_TYPE_OBJECT));
	items = eina_list_merge(items, ender_lib_item_list(lib, ENDER_ITEM_TYPE_FUNCTION));
	items = eina_list_merge(items, ender_lib_item_list(lib, ENDER_ITEM_TYPE_ENUM));
	items = eina_list_merge(items, ender_lib_item_list(lib, ENDER_ITEM_TYPE_DEF));
	EINA_LIST_FREE(items, i)
	{
		ender_neko_namespace_generate(lib, i, rel);
	}

	return ret;
}

DEFINE_PRIM(load, 1);
/*============================================================================*
 *                                 Global                                     *
 *============================================================================*/
/*----------------------------------------------------------------------------*
 *                  The Neko FFI C interface functions                        *
 *----------------------------------------------------------------------------*/
void ender_neko_init(void)
{
	eina_init();
	_log = eina_log_domain_register("ender-neko", NULL);
	_protos = eina_hash_string_superfast_new(NULL);

	kind_share(&k_obj, "ender_neko_object");
	kind_share(&k_item, "ender_item");
	kind_share(&k_lib, "ender_lib");
	ender_init();
}
