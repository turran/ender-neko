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

/*============================================================================*
 *                                  Local                                     *
 *============================================================================*/
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

static value ender_neko_obj_new(Ender_Item *i, void *o)
{
	Ender_Neko_Object *no;
	value intptr;
	value ret;

	no = calloc(1, sizeof(Ender_Neko_Object));
	no->i = ender_item_ref(i);
	no->o = o;

	intptr = alloc_abstract(k_obj, no);
	val_gc(intptr, ender_neko_obj_finalize);

	ret = alloc_object(NULL);
	alloc_field(ret, val_id("__intptr"), intptr);

	return ret;
}

/*----------------------------------------------------------------------------*
 *                                 Items                                      *
 *----------------------------------------------------------------------------*/
static void ender_neko_item_finalize(value v)
{
	ender_item_unref(val_data(v));
}

static void ender_neko_item_initialize(value v, Ender_Item *i)
{
	value intptr;

	intptr = alloc_abstract(k_item, i);
	val_gc(intptr, ender_neko_item_finalize);
	alloc_field(v, val_id("__intptr"), intptr);
}
/*----------------------------------------------------------------------------*
 *                                  Attrs                                      *
 *----------------------------------------------------------------------------*/
static value ender_neko_attr_set(value *args, int nargs)
{
	printf("setting attr %d\n", nargs);
	return val_true;
}

static value ender_neko_attr_get(value *args, int nargs)
{
	printf("getting attr %d\n", nargs);
	return val_true;
}
/*----------------------------------------------------------------------------*
 *                                 Structs                                    *
 *----------------------------------------------------------------------------*/
static value ender_neko_struct_new(void)
{
	Ender_Item *i;
	Ender_Item *f;
	Eina_List *fields;
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
	fields = ender_item_struct_fields_get(i);
	EINA_LIST_FREE(fields, f)
	{
		Ender_Item *type;
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

		setter = alloc_function(ender_neko_attr_set, VAR_ARGS, "ener_neko_attr_set");
		//ender_neko_item_initialize(setter, ender_item_ref(f));
		getter = alloc_function(ender_neko_attr_get, VAR_ARGS, "ener_neko_attr_get");
		//ender_neko_item_initialize(getter, ender_item_ref(f));
		alloc_field(ret, val_id(get_name), getter);
		alloc_field(ret, val_id(set_name), setter);
		ender_item_unref(f);
	}
	/* TODO iterate over the functions */
	return ret;
}

static value ender_neko_struct_generate_class(Ender_Item *i)
{
	Eina_List *items;
	value ret;
	value intptr;
	value f;

	ret = alloc_object(NULL);
	ender_neko_item_initialize(ret, i);

	/* ctor */
	f = alloc_function(ender_neko_struct_new, 0, "new");
	alloc_field(ret, val_id("new"), f);

	return ret;	
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
	value ns;

	if (!val_is_string(api))
		return val_null;
	lib = ender_lib_find(val_string(api));
	if (!lib)
		return val_null;

	/* create a lib object */
	ret = alloc_object(NULL);
	intptr = alloc_abstract(k_lib, (void *)lib);
	alloc_field(ret, val_id("__intptr"), intptr);

	/* create all the possible objects,structs,enums as classes */
	/* for cases like foo.bar.s, we need to create intermediary empty objects */
	items = ender_lib_item_list(lib, ENDER_ITEM_TYPE_STRUCT);
	EINA_LIST_FREE(items, i)
	{
		value s;
		const char *orig;
		char *name, *token, *str, *current, *rname = NULL, *saveptr = NULL;
		int j;

		orig = ender_item_name_get(i);
		name = strdup(orig);
		rname = name;
		current = calloc(strlen(name), 1);
		
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
				printf("TODO create intermediary namespaces %s\n", current);
			}
		}
		/* finally generate the value */
		s = ender_neko_struct_generate_class(ender_item_ref(i));
		alloc_field(ret, val_id(rname), s);
		ender_item_unref(i);

		free(current);
		free(name);

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
	kind_share(&k_obj, "ender_neko_object");
	kind_share(&k_item, "ender_item");
	kind_share(&k_lib, "ender_lib");
	ender_init();
}

#if 0
/*============================================================================*
 *                                  Local                                     *
 *============================================================================*/
static void _add_properties(Ender_Property *prop, void *data)
{
	value getter;
	value setter;
	value element;

	element = data;
	//getter = alloc_function(point_to_string,0,"point_to_string")
	//setter = alloc_function(point_to_string,0,"point_to_string")
	printf("adding property %s\n", ender_property_name_get(prop));
}

static Ender_Element *  _validate_element(value element)
{
	Ender_Element *e;
	value intptr;

	if (!val_is_object(element))
		return NULL;
	intptr = val_field(element, val_id("__intptr")); 
	val_check_kind(intptr, k_element);
	e = val_data(intptr);

	return e;
}

static value value_get(value name)
{
	value element;
	value ret = val_false;
	Ender_Property *prop;
	Ender_Element *e;
	char *c_name;

	element = val_this();
	e = _validate_element(element);
	if (!e)
		return val_null;
	if (!val_is_string(name))
		return val_null;

	c_name = val_string(name);
	prop = ender_element_property_get(e, c_name);
	printf("trying to get a property from %p\n", prop);
	switch (ender_property_type_get(prop))
	{
		case ENDER_INT32:
		case ENDER_UINT32:
		{
			uint32_t v;

			ender_element_value_get(e, c_name, &v, NULL);
			printf("value is %d\n", v);
			ret = alloc_int(v);
		}

		break;
		case ENDER_DOUBLE:
		{
			double v;

			ender_element_value_get(e, c_name, &v, NULL);
			printf("value is %g\n", v);
			ret = alloc_float((float)v);
		}
		case ENDER_COLOR:
		break;

		default:
		printf("value not supported\n");
		break;
	}
	return ret;
}

static value value_set(value name, value v)
{
	value element;
	Ender_Element *e;
	Ender_Value *c_value;
	char *c_name;

	element = val_this();
	e = _validate_element(element);
	if (!e)
		return val_false;

	if (!val_is_string(name))
		return val_false;
	c_name = val_string(name);
	switch (val_type(v))
	{
		case VAL_NULL:
		return val_false;
		break;

		case VAL_INT:
		c_value = ender_value_basic_new(ENDER_INT32);
		ender_value_int32_set(c_value, val_int(v));
		break;

		case VAL_STRING:
		c_value = ender_value_basic_new(ENDER_STRING);
		ender_value_string_set(c_value, val_string(v));
		break;

		case VAL_OBJECT:
		break;
	}
	ender_element_value_set_simple(e, c_name, c_value);
	ender_value_free(c_value);

	return val_true;
}

static value element_initialize(value intptr)
{
	Ender_Element *e;
	value ret;
	value gen_setter;
	value gen_getter;

	val_check_kind(intptr, k_element);
	e = val_data(intptr);
	/* define the object */
	ret = alloc_object(NULL);
	alloc_field(ret, val_id("__intptr"), intptr);
	/* add common ender functions */
	gen_getter = alloc_function(value_get, 1, "value_get");
	alloc_field(ret, val_id("get"), gen_getter);
	/* get all the properties */
	ender_element_property_list(e, _add_properties, ret);
	gen_setter = alloc_function(value_set, 2, "value_set");
	alloc_field(ret, val_id("set"), gen_setter);

	return ret;
}
/*----------------------------------------------------------------------------*
 *                  The Neko FFI C interface functions                        *
 *----------------------------------------------------------------------------*/
static void element_delete(value v)
{
	Ender_Element *e;

	e = val_data(v);
	ender_element_delete(e);
}

static value element_new(value name)
{
	Ender_Element *e;
	value intptr;
	value ret;

	if (!val_is_string(name))
		return val_null;

	e = ender_element_new(val_string(name));
	if (!e) return val_null;

	intptr = alloc_abstract(k_element, e);
	ret = element_initialize(intptr);
	val_gc(intptr, element_delete);

	return ret;
}

static value element_name_get(value element)
{
	Ender_Element *e;
	value ret;
	const char *name;

	val_check_kind(element, k_element);
	e = val_data(element);
	name = ender_element_name_get(e);
	ret = alloc_string(name);

	return ret;
}

static value element_value_get(value element, value name)
{
	Ender_Element *e;
	char *c_name;

	val_check_kind(element, k_element);
	if (!val_is_string(name))
		return val_null;

	c_name = val_string(name);
}

static value element_value_add(value element, value name, value v)
{
	Ender_Element *e;
	char *c_name;

	val_check_kind(element, k_element);
	if (!val_is_string(name))
		return val_false;

	/* TOOD */
	return val_false;
}

static value element_value_remove(value element, value name, value v)
{
	Ender_Element *e;
	char *c_name;

	val_check_kind(element, k_element);
	if (!val_is_string(name))
		return val_false;

	/* TODO */
	return val_false;
}

static value element_value_clear(value element, value name)
{
	Ender_Element *e;
	char *c_name;

	val_check_kind(element, k_element);
	if (!val_is_string(name))
		return val_null;

	c_name = val_string(name);
	ender_element_value_clear(e, c_name);
}

DEFINE_PRIM(element_new, 1);
DEFINE_PRIM(element_initialize, 1);
DEFINE_PRIM(element_value_add, 3);
DEFINE_PRIM(element_value_remove, 3);
DEFINE_PRIM(element_value_clear, 2);
#endif
