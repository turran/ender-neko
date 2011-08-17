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

DEFINE_KIND(k_element);
DEFINE_ENTRY_POINT(neko_ender_init);

void neko_ender_init(void)
{
	kind_share(&k_element, "ender_element");
	ender_init(NULL, NULL);
}

#if 0
/* the descriptor interface */
typedef struct _Ender_Descriptor Ender_Descriptor;

typedef void (*Ender_List_Callback)(const char *name, void *data);
typedef void (*Ender_Property_List_Callback)(Ender_Descriptor *e, const char *name, void *data);

EAPI Ender_Descriptor * ender_descriptor_find(const char *name);
EAPI void ender_descriptor_property_list(Ender_Descriptor *ed, Ender_Property_List_Callback cb, void *data);
EAPI Ender_Property * ender_descriptor_property_get(Ender_Descriptor *ed, const char *name);
EAPI void ender_descriptor_list(Ender_List_Callback cb, void *data);
EAPI Eina_Bool ender_descriptor_exists(const char *name);
EAPI Ender_Type ender_descriptor_type(Ender_Descriptor *ed);
EAPI const char * ender_descriptor_name_get(Ender_Descriptor *ed);
EAPI Ender_Descriptor * ender_descriptor_parent(Ender_Descriptor *ed);
#endif
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

#if 0
EAPI Ender_Descriptor * ender_element_descriptor_get(Ender_Element *e);
EAPI Ender_Property * ender_element_property_get(Ender_Element *e, const char *name);
EAPI void ender_element_property_value_set_valist(Ender_Element *e, Ender_Property *prop, va_list va_args);
EAPI void ender_element_property_value_set(Ender_Element *e, Ender_Property *prop, ...);
EAPI void ender_element_property_value_set_simple(Ender_Element *e, Ender_Property *prop, Ender_Value *value);

EAPI void ender_element_value_get(Ender_Element *e, const char *name, ...);
EAPI void ender_element_value_get_valist(Ender_Element *e, const char *name, va_list var_args);
EAPI void ender_element_value_get_simple(Ender_Element *e, const char *name, Ender_Value *value);

EAPI void ender_element_value_set(Ender_Element *e, const char *name, ...);
EAPI void ender_element_value_set_valist(Ender_Element *e, const char *name, va_list var_args);
EAPI void ender_element_value_set_simple(Ender_Element *e, const char *name, Ender_Value *value);

EAPI void ender_element_value_add(Ender_Element *e, const char *name, ...);
EAPI void ender_element_value_add_valist(Ender_Element *e, const char *name, va_list var_args);
EAPI void ender_element_value_add_simple(Ender_Element *e, const char *name, Ender_Value *value);

EAPI void ender_element_value_remove(Ender_Element *e, const char *name, ...);
EAPI void ender_element_value_remove_valist(Ender_Element *e, const char *name, va_list var_args);
EAPI void ender_element_value_remove_simple(Ender_Element *e, const char *name, Ender_Value *value);

EAPI Enesim_Renderer * ender_element_renderer_get(Ender_Element *e);
EAPI Ender_Element * ender_element_renderer_from(Enesim_Renderer *r);

EAPI Ender_Element * ender_element_parent_get(Ender_Element *e);
#endif

DEFINE_PRIM(element_new, 1);
DEFINE_PRIM(element_initialize, 1);
DEFINE_PRIM(element_value_add, 3);
DEFINE_PRIM(element_value_remove, 3);
DEFINE_PRIM(element_value_clear, 2);
