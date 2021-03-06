/*
 * The MIT License
 *
 * Copyright (c) 2010 - 2011 Shuhei Tanuma
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "php_git.h"
#include <spl/spl_array.h>
#include <zend_interfaces.h>
#include <string.h>
#include <time.h>

PHPAPI zend_class_entry *git_index_class_entry;

static void php_git_index_free_storage(php_git_index_t *obj TSRMLS_DC)
{
    zend_object_std_dtor(&obj->zo TSRMLS_CC);
    if(obj->index){
        //なぜかセグフォる
        //git_index_free(obj->index);
    }
    obj->repository = NULL;
    efree(obj);
}

zend_object_value php_git_index_new(zend_class_entry *ce TSRMLS_DC)
{
	zend_object_value retval;
	php_git_index_t *obj;
	zval *tmp;

	obj = ecalloc(1, sizeof(*obj));
	zend_object_std_init( &obj->zo, ce TSRMLS_CC );
	zend_hash_copy(obj->zo.properties, &ce->default_properties, (copy_ctor_func_t) zval_add_ref, (void *) &tmp, sizeof(zval *));

	retval.handle = zend_objects_store_put(obj, 
        (zend_objects_store_dtor_t)zend_objects_destroy_object,
        (zend_objects_free_object_storage_t)php_git_index_free_storage,
        NULL TSRMLS_CC);
	retval.handlers = zend_get_std_object_handlers();
	return retval;
}

// GitIndex implements Iterator
PHP_METHOD(git_index, current)
{
    git_index *index;
    zval *entry_count;
    php_git_index_t *myobj = (php_git_index_t *) zend_object_store_get_object(getThis() TSRMLS_CC);
    index = myobj->index;

    zval *offset;
    //FIXME: ほぼGit::getIndexのコピペ。
    long z_offset;
    git_index_entry *entry;
    zval *git_index_entry;
    char oid[GIT_OID_HEXSZ];

    offset = zend_read_property(git_index_class_entry,getThis(),"offset",6,0 TSRMLS_DC);
    z_offset = Z_LVAL_P(offset);
    entry = git_index_get(index,z_offset);
    if(entry == NULL){
        return;
    }

    git_oid_fmt(oid,&entry->oid);
    MAKE_STD_ZVAL(git_index_entry);
    object_init_ex(git_index_entry,git_index_entry_class_entry);

    add_property_string_ex(git_index_entry,"path", 5, entry->path, 1 TSRMLS_CC);
    add_property_string_ex(git_index_entry,"oid",4,oid, 1 TSRMLS_CC);
    add_property_long(git_index_entry,"dev",entry->dev);
    add_property_long(git_index_entry,"ino",entry->ino);
    add_property_long(git_index_entry,"mode",entry->mode);
    add_property_long(git_index_entry,"uid",entry->uid);
    add_property_long(git_index_entry,"gid",entry->gid);
    add_property_long(git_index_entry,"file_size",entry->file_size);
    add_property_long(git_index_entry,"flags",entry->flags);
    add_property_long(git_index_entry,"flags_extended",entry->flags_extended);
    add_property_long(git_index_entry,"ctime",time(&entry->ctime.seconds));
    add_property_long(git_index_entry,"mtime",time(&entry->mtime.seconds));
    
    RETURN_ZVAL(git_index_entry,1, 0);

}

PHP_METHOD(git_index, key)
{
    git_index *index;
    git_index_entry *entry;
    zval *entry_count;
    zval *offset;

    char oid[GIT_OID_HEXSZ];
    php_git_index_t *myobj = (php_git_index_t *) zend_object_store_get_object(getThis() TSRMLS_CC);
    index = myobj->index;

    offset = zend_read_property(git_index_class_entry,getThis(),"offset",6,0 TSRMLS_DC);
    entry = git_index_get(index,Z_LVAL_P(offset));
    if(entry == NULL){
        return;
    }
    git_oid_fmt(oid,&entry->oid);
    
    RETURN_STRINGL(oid,GIT_OID_HEXSZ,1);
}

PHP_METHOD(git_index, next)
{
    zval *offset;
    offset = zend_read_property(git_index_class_entry,getThis(),"offset",6,0 TSRMLS_DC);
    zend_update_property_long(git_index_class_entry, getThis(), "offset",6, Z_LVAL_P(offset)+1 TSRMLS_DC);

    RETURN_TRUE;
}

PHP_METHOD(git_index, rewind)
{
    add_property_long(getThis(), "offset", 0);
}

PHP_METHOD(git_index, valid)
{
    zval *entry_count;
    zval *offset;
    long z_entry_count;
    long z_offset;

    entry_count = zend_read_property(git_index_class_entry,getThis(),"entry_count",11,0 TSRMLS_DC);
    offset = zend_read_property(git_index_class_entry,getThis(),"offset",6,0 TSRMLS_DC);
    z_entry_count = Z_LVAL_P(entry_count);
    z_offset = Z_LVAL_P(offset);
    
    if(z_offset < z_entry_count && z_offset >= 0){
        RETURN_TRUE;
    }else{
        RETURN_FALSE;
    }
}


// GitIndex implements Countable
PHP_METHOD(git_index, count)
{
    git_index *index;
    zval *entry_count;
    php_git_index_t *myobj = (php_git_index_t *) zend_object_store_get_object(getThis() TSRMLS_CC);
    index = myobj->index;

    entry_count = zend_read_property(git_index_class_entry,getThis(),"entry_count",11,0 TSRMLS_DC);

    long count = Z_LVAL_P(entry_count);

    RETVAL_LONG(count);
}


ZEND_BEGIN_ARG_INFO_EX(arginfo_git_index_find, 0, 0, 1)
    ZEND_ARG_INFO(0, path)
ZEND_END_ARG_INFO()

PHP_METHOD(git_index, find)
{
    int offset = 0;
    char *path;
    int path_len = 0;
    git_index *index = NULL;

    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,
        "s", &path, &path_len) == FAILURE){
        return;
    }

    php_git_index_t *myobj = (php_git_index_t *) zend_object_store_get_object(getThis() TSRMLS_CC);
    index = myobj->index;

    offset = git_index_find(index,path);

    if(offset >= 0){
        RETURN_LONG(offset);
    }
}

PHP_METHOD(git_index, refresh)
{
    git_index *index = NULL;
    php_git_index_t *myobj = (php_git_index_t *) zend_object_store_get_object(getThis() TSRMLS_CC);
    index = myobj->index;

    git_index_read(index);
}


ZEND_BEGIN_ARG_INFO_EX(arginfo_git_index_get_entry, 0, 0, 1)
    ZEND_ARG_INFO(0, offset)
ZEND_END_ARG_INFO()

PHP_METHOD(git_index, getEntry)
{
    int offset = 0;
    git_index *index = NULL;
    git_index_entry *entry;
    zval *git_index_entry;
    char oid[GIT_OID_HEXSZ];

    php_git_index_t *myobj = (php_git_index_t *) zend_object_store_get_object(getThis() TSRMLS_CC);
    index = myobj->index;
    
    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,
            "l", &offset) == FAILURE){
        return;
    }

    entry = git_index_get(index,offset);

    git_oid_fmt(oid,&entry->oid);

    MAKE_STD_ZVAL(git_index_entry);
    object_init_ex(git_index_entry,git_index_entry_class_entry);

    add_property_string_ex(git_index_entry,"path", 5, entry->path, 1 TSRMLS_CC);
    add_property_string_ex(git_index_entry,"oid",4,oid, 1 TSRMLS_CC);
    add_property_long(git_index_entry,"dev",entry->dev);
    add_property_long(git_index_entry,"ino",entry->ino);
    add_property_long(git_index_entry,"mode",entry->mode);
    add_property_long(git_index_entry,"uid",entry->uid);
    add_property_long(git_index_entry,"gid",entry->gid);
    add_property_long(git_index_entry,"file_size",entry->file_size);
    add_property_long(git_index_entry,"flags",entry->flags);
    add_property_long(git_index_entry,"flags_extended",entry->flags_extended);
    add_property_long(git_index_entry,"ctime",time(&entry->ctime.seconds));
    add_property_long(git_index_entry,"mtime",time(&entry->mtime.seconds));
    
    RETURN_ZVAL(git_index_entry,1, 0);
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_git_index_construct, 0, 0, 1)
  ZEND_ARG_INFO(0, repository_path)
ZEND_END_ARG_INFO()

PHP_METHOD(git_index, __construct)
{
    const char *repository_path = NULL;
    int ret = 0;
    int arg_len = 0;
    git_index *index;
    zval *object = getThis();
    object_init_ex(object, git_index_class_entry);

    if(!object){
        php_error_docref(NULL TSRMLS_CC, E_WARNING,
            "Constructor called statically!");
        RETURN_FALSE;
    }

    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,
                            "s", &repository_path, &arg_len) == FAILURE){
        return;
    }

    ret = git_index_open_bare(&index,repository_path);
    if(ret != GIT_SUCCESS){
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "Git repository not found.");
        RETURN_FALSE;
    }
    git_index_read(index);

    php_git_index_t *myobj = (php_git_index_t *) zend_object_store_get_object(object TSRMLS_CC);
    myobj->index = index;

    add_property_long(object, "offset", 0);
    add_property_string_ex(object, "path",5,repository_path, 1 TSRMLS_CC);
    add_property_long(object, "entry_count",git_index_entrycount(index));
}


ZEND_BEGIN_ARG_INFO_EX(arginfo_git_index_add, 0, 0, 1)
    ZEND_ARG_INFO(0, path)
ZEND_END_ARG_INFO()

PHP_METHOD(git_index, add)
{
    int offset = 0;
    char *path;
    int path_len = 0;
    git_index *index = NULL;

    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,
        "s", &path, &path_len) == FAILURE){
        return;
    }

    php_git_index_t *myobj = (php_git_index_t *) zend_object_store_get_object(getThis() TSRMLS_CC);
    index = myobj->index;

    //FIXME: stage の値の意味を調べる
    // 0 => new file
    // 1 => deleted ?
    // 2 => ?
    int success = git_index_add(index,path,0);
    if(success != GIT_SUCCESS){
        //FIXME
        php_printf("can't add index.\n");
        RETURN_FALSE;
    }

    git_index_read(index);
    RETURN_TRUE;
}


PHP_METHOD(git_index, write)
{
    git_index *index = NULL;

    php_git_index_t *myobj = (php_git_index_t *) zend_object_store_get_object(getThis() TSRMLS_CC);
    index = myobj->index;

    int success = git_index_write(index);
    if(success != GIT_SUCCESS){
        //FIXME
        php_printf("can't write index.\n");
        RETURN_FALSE;
    }

    git_index_read(index);
    RETURN_TRUE;
}





// GitIndex
PHPAPI function_entry php_git_index_methods[] = {
    PHP_ME(git_index, __construct, arginfo_git_index_construct, ZEND_ACC_PUBLIC)
    PHP_ME(git_index, getEntry, arginfo_git_index_get_entry, ZEND_ACC_PUBLIC)
    PHP_ME(git_index, refresh, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(git_index, find, arginfo_git_index_find, ZEND_ACC_PUBLIC)
    PHP_ME(git_index, add, arginfo_git_index_add, ZEND_ACC_PUBLIC)
    PHP_ME(git_index, write, NULL, ZEND_ACC_PUBLIC)
    // Countable
    PHP_ME(git_index, count, NULL, ZEND_ACC_PUBLIC)
    // Iterator
    PHP_ME(git_index, current, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(git_index, key, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(git_index, next, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(git_index, rewind, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(git_index, valid, NULL, ZEND_ACC_PUBLIC)
    {NULL, NULL, NULL}
};

void git_index_init(TSRMLS_D)
{
    zend_class_entry git_index_ce;
    INIT_NS_CLASS_ENTRY(git_index_ce, PHP_GIT_NS,"Index", php_git_index_methods);

    git_index_class_entry = zend_register_internal_class(&git_index_ce TSRMLS_CC);
	git_index_class_entry->create_object = php_git_index_new;
    zend_class_implements(git_index_class_entry TSRMLS_CC, 2, spl_ce_Countable, spl_ce_Iterator);

    //zend_declare_property_null(classentry, "name", sizeof("name")-1, ZEND_ACC_PUBLIC TSRMLS_CC);
}
