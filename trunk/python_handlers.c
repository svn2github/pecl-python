/*
 * Python in PHP - Embedded Python Extension
 *
 * Copyright (c) 2003,2004,2005,2006 Jon Parise <jon@php.net>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_python_internal.h"

/* {{{ efree_function(zend_internal_function *func)
   Frees the memory allocated to a zend_internal_function structure. */
static void
efree_function(zend_internal_function *func)
{
	int i;

	/* Free the estrdup'ed function name. */
	efree(func->function_name);

	/* Free the argument information. */
	for (i = 0; i < func->num_args; i++) {
		if (func->arg_info[i].name) {
			efree(func->arg_info[i].name);
		}
		if (func->arg_info[i].class_name) {
			efree(func->arg_info[i].class_name);
		}
	}
	efree(func->arg_info);

	/* Free the function structure itself. */
	efree(func);
}
/* }}} */

/* {{{ python_read_property(zval *object, zval *member, int type TSRMLS_DC)
 */
static zval *
python_read_property(zval *object, zval *member, int type TSRMLS_DC)
{
	PHP_PYTHON_FETCH(pip, object);
	zval *return_value = NULL;
	PyObject *attr;

	convert_to_string_ex(&member);

	if (attr = PyObject_GetAttrString(pip->object, Z_STRVAL_P(member))) {
		return_value = pip_pyobject_to_zval(attr TSRMLS_CC);
		Py_DECREF(attr);
	}

	/* TODO: Do something with the 'type' parameter? */

	return return_value;
}
/* }}} */
/* {{{ python_write_property(zval *object, zval *member, zval *value TSRMLS_DC)
 */
static void
python_write_property(zval *object, zval *member, zval *value TSRMLS_DC)
{
	PHP_PYTHON_FETCH(pip, object);
	PyObject *val = pip_zval_to_pyobject(&value TSRMLS_CC);

	if (val) {
		convert_to_string_ex(&member);
		PyObject_SetAttrString(pip->object, Z_STRVAL_P(member), val);
	}
}
/* }}} */
/* {{{ python_read_dimension(zval *object, zval *offset, int type TSRMLS_DC)
 */
static zval *
python_read_dimension(zval *object, zval *offset, int type TSRMLS_DC)
{
	PHP_PYTHON_FETCH(pip, object);
	zval *return_value = NULL;
	PyObject *item = NULL;

	/*
	 * If we've been given a numeric value, start by attempting to use the
	 * sequence protocol.  The value may be a valid index.
	 */
	if (Z_TYPE_P(offset) == IS_LONG) {
		item = PySequence_GetItem(pip->object, Z_LVAL_P(offset));
	}

	/*
	 * Otherwise, if this object provides the mapping protocol, our offset's
	 * string representation may be a key value.
	 */
	if (!item && PyMapping_Check(pip->object)) {
		convert_to_string_ex(&offset);
		item = PyMapping_GetItemString(pip->object, Z_STRVAL_P(offset));
	}

	/* If we successfully fetched an item, return its PHP representation. */
	if (item) {
		return_value = pip_pyobject_to_zval(item TSRMLS_CC);
		Py_DECREF(item);
	}

	/* TODO: Do something with the 'type' parameter? */

	return return_value;
}
/* }}} */
/* {{{ python_write_dimension(zval *object, zval *offset, zval *value TSRMLS_DC)
 */
static void
python_write_dimension(zval *object, zval *offset, zval *value TSRMLS_DC)
{
	PHP_PYTHON_FETCH(pip, object);
	PyObject *val = pip_zval_to_pyobject(&value TSRMLS_CC);

	/*
	 * If this offset is a numeric value, we'll start by attempting to use
	 * the sequence protocol to set this item.
	 */
	if (Z_TYPE_P(offset) == IS_LONG && PySequence_Check(pip->object)) {
		if (PySequence_SetItem(pip->object, Z_LVAL_P(offset), val) == -1)
			zend_error(E_ERROR, "Failed to set sequence item %d", Z_LVAL_P(offset));
	}

	/*
	 * Otherwise, if this object supports the mapping protocol, use the string
	 * representation of the offset as the key value.
	 */
	else if (PyMapping_Check(pip->object)) {
		convert_to_string_ex(&offset);
		if (PyMapping_SetItemString(pip->object, Z_STRVAL_P(offset), val) == -1)
			zend_error(E_ERROR, "Failed to set mapping item '%s'", Z_STRVAL_P(offset));
	}

	if (val) Py_DECREF(val);
}
/* }}} */
/* {{{ python_property_exists(zval *object, zval *member, int check_empty TSRMLS_DC)
 */
static int
python_property_exists(zval *object, zval *member, int check_empty TSRMLS_DC)
{
	PHP_PYTHON_FETCH(pip, object);

	/* We're only concerned with the string representation of this value. */
	convert_to_string_ex(&member);

	/*
	 * If check_empty is specified, we need to check whether or not the
	 * attribute exists *and* is considered "true".  This requires to fetch
	 * and inspect its value.  Like python_dimension_exists() below, we apply
	 * Python's notion of truth here.
	 */
	if (check_empty) {
		PyObject *a = PyObject_GetAttrString(pip->object, Z_STRVAL_P(member));
		if (a) {
			int ret = (PyObject_IsTrue(a) == 1);
			Py_DECREF(a);
			return ret;
		}
	}

	/* Otherwise, just check for the existence of the attribute. */
	return PyObject_HasAttrString(pip->object, Z_STRVAL_P(member)) ? 1 : 0;
}
/* }}} */
/* {{{ python_dimension_exists(zval *object, zval *member, int check_empty TSRMLS_DC)
 */
static int
python_dimension_exists(zval *object, zval *member, int check_empty TSRMLS_DC)
{
	PHP_PYTHON_FETCH(pip, object);
	PyObject *item = NULL;
	int ret = 0;

	/*
	 * If we've been handed an integer value, check if this is a valid item
	 * index.  PySequence_GetItem() will perform a PySequence_Check() test.
	 */
	if (Z_TYPE_P(member) == IS_LONG) {
		item = PySequence_GetItem(pip->object, Z_LVAL_P(member));
	}

	/*
	 * Otherwise, check if this object provides the mapping protocol.  If it
	 * does, check if the string representation of our value is a valid key.
	 */
	if (!item && PyMapping_Check(pip->object)) {
		convert_to_string_ex(&member);
		item = PyMapping_GetItemString(pip->object, Z_STRVAL_P(member));
	}

	/*
	 * If we have a valid item at this point, we have a chance at success.  The
	 * last thing we need to consider is whehter or not the item's value is
	 * considered "true" if check_empty has been specified.  We use Python's
	 * notion of truth here for consistency, although it may be more correct to
	 * use PHP's notion of truth (as determined by zend_is_true()) should we
	 * encountered problems with this in the future.
	 */
	if (item) {
		ret = (check_empty) ? (PyObject_IsTrue(item) == 1) : 1;
		Py_DECREF(item);
	}

	return ret;
}
/* }}} */
/* {{{ python_property_delete(zval *object, zval *member TSRMLS_DC)
 */
static void
python_property_delete(zval *object, zval *member TSRMLS_DC)
{
	PHP_PYTHON_FETCH(pip, object);

	convert_to_string_ex(&member);
	if (PyObject_DelAttrString(pip->object, Z_STRVAL_P(member)) == -1)
		zend_error(E_ERROR, "Failed to delete attribute '%s'", Z_STRVAL_P(member));
}
/* }}} */
/* {{{ python_dimension_delete(zval *object, zval *offset TSRMLS_DC)
 */
static void
python_dimension_delete(zval *object, zval *offset TSRMLS_DC)
{
	PHP_PYTHON_FETCH(pip, object);
	int deleted = 0;

	/*
	 * If we've been given a numeric offset and this object provides the
	 * sequence protocol, attempt to delete the item using a sequence index.
	 */
	if (Z_TYPE_P(offset) == IS_LONG && PySequence_Check(pip->object)) {
		deleted = PySequence_DelItem(pip->object, Z_LVAL_P(offset)) != -1;
	}

	/*
	 * If we failed to delete the item using the sequence protocol, use the
	 * offset's string representation and try the mapping protocol.
	 */
	if (PyMapping_Check(pip->object)) {
		convert_to_string_ex(&offset);
		deleted = PyMapping_DelItemString(pip->object,Z_STRVAL_P(offset)) != -1;
	}

	/* If we still haven't deleted the requested item, trigger an error. */
	if (!deleted)
		zend_error(E_ERROR, "Failed to delete item");
}
/* }}} */
/* {{{ python_get_properties(zval *object TSRMLS_DC)
 */
static HashTable *
python_get_properties(zval *object TSRMLS_DC)
{
	PHP_PYTHON_FETCH(pip, object);
	PyObject *dict;
	HashTable *ht;

	/* Initialize the hashtable which will hold the list of properties. */
	ALLOC_HASHTABLE(ht);
	if (zend_hash_init(ht, 0, NULL, ZVAL_PTR_DTOR, 0) != SUCCESS) {
		return NULL;
	}

	/*
	 * Fetch the dictionary containing all of this object's attributes
	 * (__dict__) and store its contents in our PHP hashtable.
	 */
	dict = PyObject_GetAttrString(pip->object, "__dict__");
	if (dict) {
		pip_mapping_to_hash(dict, ht TSRMLS_CC);
		Py_DECREF(dict);
	}

    return ht;
}
/* }}} */
/* {{{ python_get_method(zval **object_ptr, char *method, int method_len TSRMLS_DC)
 */
static union _zend_function *
python_get_method(zval **object_ptr, char *method, int method_len TSRMLS_DC)
{
	PHP_PYTHON_FETCH(pip, *object_ptr);
	zend_internal_function *f;
	PyObject *func;

	/* Quickly check if this object has a method with the requested name. */
	if (PyObject_HasAttrString(pip->object, method) == 0) {
		return NULL;
	}

	/* Attempt to fetch the requested method and verify that it's callable. */
	func = PyObject_GetAttrString(pip->object, method);
	if (!func || PyMethod_Check(func) == 0 || PyCallable_Check(func) == 0) {
		if (func) Py_DECREF(func);
		return NULL;
	}

	/*
	 * Set up the function call structure for this method invokation.  We
	 * allocate a bit of memory here which will be released later on in
	 * python_call_method().
	 */
	f = emalloc(sizeof(zend_internal_function));
	f->type = ZEND_OVERLOADED_FUNCTION_TEMPORARY;
	f->function_name = estrndup(method, method_len);
	f->scope = pip->ce;
	f->fn_flags = 0;
	f->prototype = NULL;
	f->pass_rest_by_reference = 0;
	f->num_args = python_get_arg_info(func, &(f->arg_info));

	Py_DECREF(func);

	return (union _zend_function *)f;
}
/* }}} */
/* {{{ python_call_method(char *method, INTERNAL_FUNCTION_PARAMETERS)
 */
static int
python_call_method(char *method_name, INTERNAL_FUNCTION_PARAMETERS)
{
	PHP_PYTHON_FETCH(pip, getThis());
	PyObject *method;
	int ret = FAILURE;

	/* Get a pointer to the requested method from this object. */
	method = PyObject_GetAttrString(pip->object, method_name);

	/* If the method exists and is callable ... */
	if (method && PyMethod_Check(method) && PyCallable_Check(method)) {
		PyObject *args, *result;

		/* Convert all of our PHP arguments into a Python-digestable tuple. */
		args = pip_args_to_tuple(ZEND_NUM_ARGS(), 0 TSRMLS_CC);

		/*
		 * Invoke the requested method and store the resul. If we have a tuple
		 * of arguments, remember to free (decref) it.
		 */
		result = PyObject_CallObject(method, args); 
		Py_DECREF(method);
		if (args) Py_DECREF(args);

		if (result) {
			/* Convert the Python result value to its PHP equivalent. */
			zval *retval = pip_pyobject_to_zval(result TSRMLS_CC);
			Py_DECREF(result);

			/* If we have a return value, set it and signal success. */
			if (retval) {
				RETVAL_ZVAL(retval, 1, 0);
				ret = SUCCESS;
			}
		}
	}

	/* Release the memory that we allocated for this function in method_get. */
	efree_function((zend_internal_function *)EG(function_state_ptr)->function);

	return ret;
}
/* }}} */
/* {{{ python_constructor_get(zval *object TSRMLS_DC)
 */
static union _zend_function *
python_constructor_get(zval *object TSRMLS_DC)
{
	PHP_PYTHON_FETCH(pip, object);
	return pip->ce->constructor;
}
/* }}} */
/* {{{ python_get_class_entry(zval *object TSRMLS_DC)
 */
static zend_class_entry *
python_get_class_entry(zval *object TSRMLS_DC)
{
	PHP_PYTHON_FETCH(pip, object);
	return pip->ce;
}
/* }}} */
/* {{{ python_get_class_name(zval *object, char **class_name, zend_uint *class_name_len, int parent TSRMLS_DC)
 */
static int
python_get_class_name(zval *object, char **class_name,
					  zend_uint *class_name_len, int parent TSRMLS_DC)
{
	PHP_PYTHON_FETCH(pip, object);
	char *key = (parent) ? "__module__" : "__class__";
	PyObject *attr, *str;

	/* Start off with some safe initial values. */
	*class_name = NULL;
	*class_name_len = 0;

	/*
	 * Attempt to use the Python object instance's special read-only attributes
	 * to determine object's class name.  We use __class__ unless we've been
	 * asked for the name of our parent, in which case we use __module__.  We
	 * prefix the class name with "Python." to avoid confusion with native PHP
	 * classes.
	 */
	if (attr = PyObject_GetAttrString(pip->object, key)) {
		if (str = PyObject_Str(attr)) {
			*class_name_len = sizeof("Python.") + PyString_Size(str);
			*class_name = (char *)emalloc(sizeof(char *) * *class_name_len);
			zend_sprintf(*class_name, "Python.%s", PyString_AsString(str));
			Py_DECREF(str);
		}
		Py_DECREF(attr);
	}

	/* If we still don't have a string, use the PHP class entry's name. */
	if (*class_name_len == 0) {
		*class_name = estrndup(pip->ce->name, pip->ce->name_length);
		*class_name_len = pip->ce->name_length;
	}

	return SUCCESS;
}
/* }}} */
/* {{{ python_compare(zval *object1, zval *object2 TSRMLS_DC)
 */
static int
python_compare(zval *object1, zval *object2 TSRMLS_DC)
{
	PHP_PYTHON_FETCH(a, object1);
	PHP_PYTHON_FETCH(b, object2);

	return PyObject_Compare(a->object, b->object);
}
/* }}} */
/* {{{ python_cast(zval *readobj, zval *writeobj, int type TSRMLS_DC)
 */
static int
python_cast(zval *readobj, zval *writeobj, int type TSRMLS_DC)
{
	PHP_PYTHON_FETCH(pip, readobj);
	PyObject *val = NULL;

	switch (type) {
		case IS_STRING:
			val = PyObject_Str(pip->object);
			break;
		default:
			return FAILURE;
	}

	if (val) {
		writeobj = pip_pyobject_to_zval(val TSRMLS_CC);
		Py_DECREF(val);
	}

	return SUCCESS;
}
/* }}} */
/* {{{ python_count_elements(zval *object, long *count TSRMLS_DC)
   Updates count with the number of elements present in the object and returns
   SUCCESS.  If the object has no sense of overloaded dimensions, FAILURE is
   returned. */
static int
python_count_elements(zval *object, long *count TSRMLS_DC)
{
	PHP_PYTHON_FETCH(pip, object);
	int len = PyObject_Length(pip->object);

	if (len != -1) {
		*count = len;
		return SUCCESS;
	}

	return FAILURE;
}
/* }}} */

/* {{{ python_object_handlers[]
 */
zend_object_handlers python_object_handlers = {
	ZEND_OBJECTS_STORE_HANDLERS,
    python_read_property,
    python_write_property,
    python_read_dimension,
    python_write_dimension,
    NULL,
    NULL,
    NULL,
    python_property_exists,
    python_property_delete,
    python_dimension_exists,
    python_dimension_delete,
    python_get_properties,
    python_get_method,
    python_call_method,
    python_constructor_get,
    python_get_class_entry,
    python_get_class_name,
    python_compare,
    python_cast,
	python_count_elements
};
/* }}} */

/*
 * Local variables:
 * c-basic-offset: 4
 * tab-width: 4
 * End:
 * vim600: fdm=marker
 * vim: sw=4 ts=4 noet
 */
