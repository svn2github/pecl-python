/*
 * Python in PHP - Embedded Python Extension
 *
 * Copyright (c) 2003,2004,2005,2006,2007,2008 Jon Parise <jon@php.net>
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

#ifndef PHP_PYTHON_H
#define PHP_PYTHON_H

extern zend_module_entry python_module_entry;
#define phpext_python_ptr &python_module_entry

#ifdef PHP_WIN32
#define PHP_PYTHON_API __declspec(dllexport)
#else
#define PHP_PYTHON_API
#endif

#ifdef ZTS
#include "TSRM.h"
#endif

PHP_MINIT_FUNCTION(python);
PHP_MSHUTDOWN_FUNCTION(python);
PHP_RINIT_FUNCTION(python);
PHP_RSHUTDOWN_FUNCTION(python);
PHP_MINFO_FUNCTION(python);

PHP_FUNCTION(python_version);

PHP_FUNCTION(python_construct);
PHP_FUNCTION(python_eval);
PHP_FUNCTION(python_exec);
PHP_FUNCTION(python_call);

ZEND_BEGIN_MODULE_GLOBALS(python)
	zend_bool dummy;
ZEND_END_MODULE_GLOBALS(python)

#ifdef ZTS
#define PYG(v) TSRMG(python_globals_id, zend_python_globals *, v)
#else
#define PYG(v) (python_globals.v)
#endif

#endif /* PHP_PYTHON_H */
