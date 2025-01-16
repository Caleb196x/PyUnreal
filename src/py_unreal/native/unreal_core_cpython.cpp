#include <Python.h>
#include "ue_core.capnp.h"

typedef struct {
    PyObject_HEAD
    const char* name;
    uint64_t address;
} Object;

static PyObject* Object_repr(Object* self)
{
    if (self->name == NULL) {
        return PyUnicode_FromFormat("Object(name=NULL, address=%llx)", self->address);
    }
    return PyUnicode_FromFormat("Object(name=%s, address=%llx)", self->name, self->address);
}

static int Object_init(Object* self, PyObject* args, PyObject* kwargs)
{
    const char* name = NULL;
    uint64_t address = 0;

    if (!PyArg_ParseTuple(args, "sK", &name, &address)) {
        return -1;
    }

    self->name = name;
    self->address = address;
    
    return 0;
}

static PyTypeObject Object_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "unreal_core.Object",           /* tp_name */
    sizeof(Object),                 /* tp_basicsize */
    0,                             /* tp_itemsize */
    0,                             /* tp_dealloc */
    0,                             /* tp_print */
    0,                             /* tp_getattr */
    0,                             /* tp_setattr */
    0,                             /* tp_reserved */
    (reprfunc)Object_repr,         /* tp_repr */
    0,                             /* tp_as_number */
    0,                             /* tp_as_sequence */
    0,                             /* tp_as_mapping */
    0,                             /* tp_hash */
    0,                             /* tp_call */
    0,                             /* tp_str */
    0,                             /* tp_getattro */
    0,                             /* tp_setattro */
    0,                             /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,           /* tp_flags */
    "Unreal Engine Object",        /* tp_doc */
    0,                             /* tp_traverse */
    0,                             /* tp_clear */
    0,                             /* tp_richcompare */
    0,                             /* tp_weaklistoffset */
    0,                             /* tp_iter */
    0,                             /* tp_iternext */
    0,                             /* tp_methods */
    0,                             /* tp_members */
    0,                             /* tp_getset */
    0,                             /* tp_base */
    0,                             /* tp_dict */
    0,                             /* tp_descr_get */
    0,                             /* tp_descr_set */
    0,                             /* tp_dictoffset */
    (initproc)Object_init,         /* tp_init */
};

typedef struct {
    PyObject_HEAD
    const char* type_name;
} Class;

typedef struct {
    PyObject_HEAD
    Class* ue_class;
    const char* name;
    union {
        bool bool_value;
        uint64_t uint_value;
        float float_value;
        const char* str_value;
        int64_t enum_value;
        Object* object;
    };
} Argument;

typedef struct {
    PyObject_HEAD
    const char* name;
} Method;

/**
 * unreal_core.new_object
 * call rpc function (newObject) to create a new ue object
 * 
 * args:
 *   object: pyobject
 *   ue_class: ue class name
 *   flags: ue object flags
 *   construct_args: list of struct Argument
 * 
 * return:
 *   ue object address
 */
static PyObject* unreal_core_new_object(PyObject* self, PyObject* args)
{
    PyObject* object = NULL;
    Py_RETURN_NONE;
}

/**
 * unreal_core.destory_object
 * call rpc function (destoryObject) to destory a ue object
 * 
 * args:
 *   object: pyobject
 * 
 * return:
 *   bool
 */
static PyObject* unreal_core_destory_object(PyObject* self, PyObject* args)
{
    Py_RETURN_NONE;
}

/**
 * unreal_core.call_function
 * call rpc function (callFunction) to call a function
 * 
 * args:
 *   object: pyobject
 *   ue_class: ue class name
 *   function_name: str
 *   params: list of struct Argument
 * 
 * returns:
 *   return value: struct Argument
 *   out param value: list of struct Argument
 */
static PyObject* unreal_core_call_function(PyObject* self, PyObject* args)
{
    Py_RETURN_NONE;
}

/**
 * unreal_core.call_static_function
 * call rpc function (callStaticFunction) to call a static function
 * 
 * args:
 *   ue_class: ue class name
 *   function_name: str
 *   params: list of struct Argument
 * 
 * returns:
 *   return value: struct Argument
 *   out param value: list of struct Argument
 */
static PyObject* unreal_core_call_static_function(PyObject* self, PyObject* args)
{
    Py_RETURN_NONE;
}

/**
 * unreal_core.get_property
 * call rpc function (getProperty) to get a property
 * 
 * args:
 *   ue_class: ue class name
 *   object: pyobject
 *   property: struct Property
 * 
 */
static PyObject* unreal_core_get_property(PyObject* self, PyObject* args)
{
    Py_RETURN_NONE;
}

/**
 * unreal_core.set_property
 * call rpc function (setProperty) to set a property
 * 
 * args:
 *   ue_class: ue class name
 *   object: pyobject
 *   property_name: str
 */
static PyObject* unreal_core_set_property(PyObject* self, PyObject* args)
{
    Py_RETURN_NONE;
}

static PyObject* unreal_core_find_class(PyObject* self, PyObject* args)
{
    Py_RETURN_NONE;
}

static PyObject* unreal_core_load_class(PyObject* self, PyObject* args)
{
    Py_RETURN_NONE;
}

static PyObject* unreal_core_load_object(PyObject* self, PyObject* args)
{
    Py_RETURN_NONE;
}

static PyObject* unreal_core_bind_delegate(PyObject* self, PyObject* args)
{
    Py_RETURN_NONE;
}

static PyObject* unreal_core_unbind_delegate(PyObject* self, PyObject* args)
{
    Py_RETURN_NONE;
}

static PyObject* unreal_core_add_multicast_delegate(PyObject* self, PyObject* args)
{
    Py_RETURN_NONE;
}

static PyObject* unreal_core_remove_multicast_delegate(PyObject* self, PyObject* args) 
{
    Py_RETURN_NONE;
}

static PyObject* unreal_core_register_overrided_class(PyObject* self, PyObject* args)
{
    Py_RETURN_NONE;
}

static PyObject* unreal_core_unregister_overrided_class(PyObject* self, PyObject* args)
{
    Py_RETURN_NONE;
}



