#include <Python.h>
#include "ue_core.capnp.h"
#include <kj/async-io.h>
#include <capnp/rpc-twoparty.h>


#define CHECK_CLIENT_AND_RECREATE_IT() \
    if (ue_core_client == NULL) { \
        ue_core_client = create_ue_core_client(); \
        if (ue_core_client == NULL) { \
            PyErr_SetString(PyExc_RuntimeError, "unreal engine rpc server is not connected"); \
            return NULL; \
        } \
    } \

#define CATCH_EXCEPTION_FOR_RPC_CALL(code) \
    try { \
        code \
    } catch (kj::Exception& e) { \
        const char* err_file = e.getFile(); \
        const int err_line = e.getLine(); \
        const char* err_msg = e.getDescription().cStr(); \
        int err_size = strlen(err_file) + strlen(err_msg) + 5; \
        char* err_str = (char*)malloc(err_size); \
        sprintf(err_str, "[%s]:[%d]: %s", err_file, err_line, err_msg); \
        PyErr_SetString(PyExc_RuntimeError, err_str); \
        free(err_str); \
        return NULL; \
    } \

#define PYTHON_MODULE_NAME "py_unreal"
#define UNREAD_OBJECT_PROPERTY_NAME "unreal_object"

/**
 * capnp client
 */
typedef struct {
    UnrealCore::Client client;
    const char* name;
} CapnpClient;

static CapnpClient* ue_core_client = NULL;
static kj::AsyncIoContext io_context = kj::setupAsyncIo();
static uint16_t server_port = 0;

static CapnpClient* create_ue_core_client()
{
    CapnpClient* rpc_client = (CapnpClient*)malloc(sizeof(CapnpClient));
    if (rpc_client == NULL) {
        return NULL;
    }

    rpc_client->name = "unreal_core_client";
    kj::Network& network = io_context.provider->getNetwork();
    auto& wait_scope = io_context.waitScope;
    uint16_t start_port = 60001;
    uint16_t end_port = 60010;
    for (uint16_t port = start_port; port <= end_port; ++port) {
        char ip_addr[20];
        sprintf(ip_addr, "127.0.0.1:%d", port);

        try{
            kj::Own<kj::NetworkAddress> address = network.parseAddress(ip_addr).wait(wait_scope);
            kj::Own<kj::AsyncIoStream> conn = address->connect().wait(wait_scope);
            capnp::TwoPartyClient client(*conn);
            rpc_client->client = client.bootstrap().castAs<UnrealCore>();
            server_port = port;
            break;
        } catch (kj::Exception& e) {
            printf("connect to %s failed, try next port\n", ip_addr);
        }
    }

    if (server_port == 0) {
        printf("connect to unreal engine rpc server failed\n");
        free(rpc_client);
        rpc_client = NULL;
        return NULL;
    }

    return rpc_client;
}

/**
 * Unreal Object
 */
typedef struct {
    PyObject_HEAD
    const char* name;
    uint64_t address;
} UnrealObject;

static PyObject* UnrealObject_repr(UnrealObject* self)
{
    if (self->name == NULL) {
        return PyUnicode_FromFormat("UnrealObject(name=NULL)");
    }
    return PyUnicode_FromFormat("UnrealObject(name=%s)", self->name);
}

static int UnrealObject_init(UnrealObject* self, PyObject* args)
{
    char* name = NULL;
    uint64_t address = 0;

    if (!PyArg_ParseTuple(args, "K|s", &address, &name)) {
        return -1;
    }
    self->name = name;
    self->address = address;

    return 0;
}

static PyTypeObject UnrealObject_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "unreal_core.Object",           /* tp_name */
    sizeof(UnrealObject),                 /* tp_basicsize */
    0,                             /* tp_itemsize */
    0,                             /* tp_dealloc */
    0,                             /* tp_print */
    0,                             /* tp_getattr */
    0,                             /* tp_setattr */
    0,                             /* tp_reserved */
    (reprfunc)UnrealObject_repr,         /* tp_repr */
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
    (initproc)UnrealObject_init,         /* tp_init */
};

/*
 * Class
 */
typedef struct {
    PyObject_HEAD
    const char* type_name;
} ClassProp;

static PyObject* ClassProp_repr(ClassProp* self)
{
    if (self->type_name == NULL) {
        return PyUnicode_FromFormat("Class(type_name=NULL)");
    }

    return PyUnicode_FromFormat("Class(type_name=%s)", self->type_name);
}

static int ClassProp_init(ClassProp* self, PyObject* args, PyObject* kwargs)
{
    char* type_name = NULL;

    if (!PyArg_ParseTuple(args, "s", &type_name)) {
        return -1;
    }

    self->type_name = type_name;
    return 0;
}

static PyTypeObject ClassProp_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "unreal_core.Class",           /* tp_name */
    sizeof(ClassProp),                 /* tp_basicsize */
    0,                             /* tp_itemsize */
    0,                             /* tp_dealloc */
    0,                             /* tp_print */
    0,                             /* tp_getattr */
    0,                             /* tp_setattr */
    0,                             /* tp_reserved */
    (reprfunc)ClassProp_repr,         /* tp_repr */
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
    "Unreal Engine Class",        /* tp_doc */
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
    (initproc)ClassProp_init,         /* tp_init */
};

/*
 * Property
 */
typedef enum {
    ARGUMENT_TYPE_BOOL = 0,
    ARGUMENT_TYPE_UINT = 1,
    ARGUMENT_TYPE_FLOAT = 2,
    ARGUMENT_TYPE_STRING = 3,
    ARGUMENT_TYPE_ENUM = 4,
    ARGUMENT_TYPE_OBJECT = 5,
} ArgumentType;

typedef struct {
    PyObject_HEAD
    ClassProp* ue_class;
    const char* name;
    ArgumentType value_type;
    union {
        bool bool_value;
        uint64_t uint_value;
        float float_value;
        const char* str_value;
        int64_t enum_value;
        PyObject* object;
    };
} Argument;

static PyObject* Argument_repr(Argument* self)
{
    if (self->name == NULL) {
        return PyUnicode_FromFormat("Argument(name=NULL)");
    }
    return PyUnicode_FromFormat("Argument(name=%s)", self->name);
}

static int Argument_init(Argument* self, PyObject* args, PyObject* kwargs)
{
    char* name = NULL;    
    ClassProp* ue_class = NULL;
    PyObject* value = NULL;
    if (!PyArg_ParseTuple(args, "sO!O", &name, &ClassProp_Type, &ue_class, &value)) {
        return -1;
    }
    if (ue_class == NULL) {
        PyErr_SetString(PyExc_TypeError, "ue_class is required");
        return -1;
    }

    if (ue_class->type_name == NULL) {
        PyErr_SetString(PyExc_TypeError, "ue_class is not initialized");
        return -1;
    }

    self->name = name;
    self->ue_class = ue_class;
    
    if (value != NULL && PyBool_Check(value)) {
        self->value_type = ARGUMENT_TYPE_BOOL;
        self->bool_value = PyObject_IsTrue(value);
    }
    else if (value != NULL && PyLong_Check(value)) {
        self->value_type = ARGUMENT_TYPE_UINT;
        self->uint_value = PyLong_AsUnsignedLongLong(value);
    }
    else if (value != NULL && PyFloat_Check(value)) {
        self->value_type = ARGUMENT_TYPE_FLOAT;
        self->float_value = PyFloat_AsDouble(value);
    }
    else if (value != NULL && PyUnicode_Check(value)) {
        self->value_type = ARGUMENT_TYPE_STRING;
        self->str_value = PyUnicode_AsUTF8(value);
    }
    else if (value != NULL && PyLong_Check(value)) {
        self->value_type = ARGUMENT_TYPE_ENUM;
        self->enum_value = PyLong_AsLongLong(value);
    }
    else if (value != NULL) {
        self->value_type = ARGUMENT_TYPE_OBJECT;
        self->object = value;
    }
    else {
        PyErr_SetString(PyExc_TypeError, "Invalid argument type");
        return -1;
    }

    return 0;
}

static PyTypeObject Argument_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "unreal_core.Argument",           /* tp_name */
    sizeof(Argument),                 /* tp_basicsize */
    0,                             /* tp_itemsize */
    0,                             /* tp_dealloc */
    0,                             /* tp_print */
    0,                             /* tp_getattr */
    0,                             /* tp_setattr */
    0,                             /* tp_reserved */
    (reprfunc)Argument_repr,         /* tp_repr */
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
    "Unreal Engine Argument",        /* tp_doc */
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
    (initproc)Argument_init,         /* tp_init */
};

typedef struct {
    PyObject_HEAD
    const char* name;
} Method;

static PyObject* Method_repr(Method* self)
{
    if (self->name == NULL) {
        return PyUnicode_FromFormat("Method(name=NULL)");
    }
    return PyUnicode_FromFormat("Method(name=%s)", self->name);
}

static int Method_init(Method* self, PyObject* args, PyObject* kwargs)
{
    char* name = NULL;
    if (!PyArg_ParseTuple(args, "s", &name)) {
        return -1;
    }
    self->name = name;
    return 0;
}

static PyTypeObject Method_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "unreal_core.Method",           /* tp_name */
    sizeof(Method),                 /* tp_basicsize */
    0,                             /* tp_itemsize */
    0,                             /* tp_dealloc */
    0,                             /* tp_print */
    0,                             /* tp_getattr */
    0,                             /* tp_setattr */
    0,                             /* tp_reserved */
    (reprfunc)Method_repr,         /* tp_repr */
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
    "Unreal Engine Method",        /* tp_doc */ 
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
    (initproc)Method_init,         /* tp_init */
};

/**
 * Utils functions
 */

static bool SetupArguments(PyObject* src_args, capnp::List<UnrealCore::Argument, capnp::Kind::STRUCT>::Builder& dest_args, Py_ssize_t list_size)
{

    for (Py_ssize_t i = 0; i < list_size; i++) {
        PyObject* item = PyList_GetItem(src_args, i);
        if (item == NULL) {
            return false;
        }

        if (PyObject_TypeCheck(item, &Argument_Type)) {
            Argument* argument = (Argument*)item;
            dest_args[i].setName(argument->name);
            dest_args[i].initUeClass().setTypeName(argument->ue_class->type_name);

            switch (argument->value_type) {
                case ARGUMENT_TYPE_BOOL:
                    dest_args[i].setBoolValue(argument->bool_value);
                    break;
                case ARGUMENT_TYPE_UINT:
                    dest_args[i].setUintValue(argument->uint_value);
                    break;
                case ARGUMENT_TYPE_FLOAT:
                    dest_args[i].setFloatValue(argument->float_value);
                    break;
                case ARGUMENT_TYPE_STRING:
                    dest_args[i].setStrValue(argument->str_value);
                    break;
                case ARGUMENT_TYPE_ENUM:
                    dest_args[i].setEnumValue(argument->enum_value);
                    break;
                case ARGUMENT_TYPE_OBJECT:
                {
                    dest_args[i].initObject().setAddress(reinterpret_cast<uint64_t>(argument->object));
                    break;
                }
                default:
                {
                    PyErr_SetString(PyExc_TypeError, "Invalid argument type");
                    return false;
                }
            }
        }
        else {
            return false;
        }
        
        return true;
    }
}

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
    CHECK_CLIENT_AND_RECREATE_IT()

    PyObject* object = NULL;
    ClassProp* ue_class = NULL;
    char* object_name = NULL;
    uint32_t flags = 0;
    PyObject* construct_args = NULL;
    
    if (!PyArg_ParseTuple(args, "OO!sIO!", &object, &ClassProp_Type, &ue_class, &object_name, &flags, &PyList_Type, &construct_args))
    {
        return NULL;
    }

    capnp::Request<UnrealCore::NewObjectParams, UnrealCore::NewObjectResults> new_object_request = ue_core_client->client.newObjectRequest();
    new_object_request.getUeClass().setTypeName(ue_class->type_name);
    new_object_request.setFlags(flags);
    new_object_request.setObjName(object_name);
    new_object_request.getOwn().setAddress(reinterpret_cast<uint64_t>(object));
    new_object_request.getOwn().setName(object_name);

    // handle construct_args
    // get list size
    Py_ssize_t list_size = PyList_Size(construct_args);
    capnp::List<UnrealCore::Argument, capnp::Kind::STRUCT>::Builder new_object_args = new_object_request.initConstructArgs(list_size);

    if (!SetupArguments(construct_args, new_object_args, list_size))
    {
        PyErr_SetString(PyExc_TypeError, "Parse construct args failed");
        return NULL;
    }

    kj::WaitScope& wait_scope = io_context.waitScope;
    CATCH_EXCEPTION_FOR_RPC_CALL({
        capnp::Response<UnrealCore::NewObjectResults> result = new_object_request.send().wait(wait_scope);
        
        UnrealObject* unreal_object = (UnrealObject*)PyObject_New(UnrealObject, &UnrealObject_Type);
        unreal_object->name = result.getObject().getName().cStr();
        unreal_object->address = result.getObject().getAddress();

        return (PyObject*)unreal_object;
    })
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
    CHECK_CLIENT_AND_RECREATE_IT()

    PyObject* object = NULL;
    if (!PyArg_ParseTuple(args, "O", &object)) {
        return NULL;
    }

    auto destory_object_request = ue_core_client->client.destroyObjectRequest();
    destory_object_request.initOwn().setAddress(reinterpret_cast<uint64_t>(object));

    kj::WaitScope& wait_scope = io_context.waitScope;
    CATCH_EXCEPTION_FOR_RPC_CALL({
        capnp::Response<UnrealCore::DestroyObjectResults> result = destory_object_request.send().wait(wait_scope);
        if (result.getResult()) {
            return Py_True;
        }
        else {
            return Py_False;
        }
    })
}

static PyObject* CreateObjectFromSpecifiedClass(const char* class_type_name)
{
    // PyObject* module_name = PyUnicode_FromString();
    PyObject* py_module = PyImport_ImportModule(PYTHON_MODULE_NAME); // todo: Compatible with other Python versions
    if (py_module == NULL) {
        PyErr_SetString(PyExc_ImportError, "Failed to import unreal module");
        return NULL;
    }

    PyObject* py_class = PyObject_GetAttrString(py_module, class_type_name);
    if (py_class == NULL || !PyType_Check(py_class)) {
        char error_msg[128];
        snprintf(error_msg, sizeof(error_msg), "Failed to find unreal class: %s", class_type_name);
        PyErr_SetString(PyExc_ImportError, error_msg);
        Py_DECREF(py_module);
        return NULL;
    }

    PyObject* py_object = PyObject_New(PyObject, (PyTypeObject*)py_class);
    if (py_object == NULL) {
        PyErr_SetString(PyExc_MemoryError, "Failed to allocate memory for unreal object");
        Py_DECREF(py_module);
        Py_INCREF(py_class);
        return NULL;
    }

    Py_INCREF(py_module);
    Py_INCREF(py_class);

    return py_object;
}

static PyObject* SendPyObjectToUnrealEngine(PyObject* py_object, UnrealObject* unreal_object, const char* class_type_name)
{
    auto create_py_object_request = ue_core_client->client.registerCreatedPyObjectRequest();
    create_py_object_request.initPyObject().setAddress(reinterpret_cast<uint64_t>(py_object));
    create_py_object_request.initUnrealObject().setAddress(unreal_object->address);
    create_py_object_request.initUeClass().setTypeName(class_type_name);

    kj::WaitScope& wait_scope = io_context.waitScope;
    CATCH_EXCEPTION_FOR_RPC_CALL({
        create_py_object_request.send().wait(wait_scope);
        return Py_True;
    })
}

static PyObject* ParseValueFromFunctionReturn(const capnp::Response<UnrealCore::CallFunctionResults>& result)
{
    // Initialize the object's fields
    auto return_value = result.getReturn();
    const char* class_type_name = return_value.getUeClass().getTypeName().cStr();

    // maybe unuse
    const char* name = return_value.getName().cStr();

    // Set the value based on the return type
    switch (return_value.which()) {
        case UnrealCore::Argument::BOOL_VALUE:
        {
            bool value = return_value.getBoolValue();
            if (PyBool_Check(value)) {
                return Py_True;
            }
            else {
                return Py_False;
            }
        }
        case UnrealCore::Argument::UINT_VALUE:
            return PyLong_FromUnsignedLongLong(return_value.getUintValue());
        case UnrealCore::Argument::FLOAT_VALUE:
            return PyFloat_FromDouble(return_value.getFloatValue());
        case UnrealCore::Argument::STR_VALUE:
            return PyUnicode_FromString(return_value.getStrValue().cStr());
        case UnrealCore::Argument::ENUM_VALUE:
            return PyLong_FromLongLong(return_value.getEnumValue());
        case UnrealCore::Argument::OBJECT:
        {
            // Create a new UnrealObject for object returns
            UnrealObject* obj = (UnrealObject*)PyObject_New(UnrealObject, &UnrealObject_Type);
            if (obj == NULL) {
                obj = (UnrealObject*)PyObject_New(UnrealObject, &UnrealObject_Type); // try again
            }
            obj->address = return_value.getObject().getAddress();
            obj->name = return_value.getObject().getName().cStr();
            PyObject* py_object = CreateObjectFromSpecifiedClass(class_type_name);
            if (py_object == NULL) {
                return NULL;
            }
            // set unreal object to the py object's property, property name is UNREAD_OBJECT_PROPERTY_NAME
            if (PyObject_SetAttrString(py_object, UNREAD_OBJECT_PROPERTY_NAME, (PyObject*)obj) != 0) {
                PyErr_SetString(PyExc_AttributeError, "Failed to set unreal object to the py object's unreal objectproperty");
                Py_DECREF(py_object);
                return NULL;
            }

            SendPyObjectToUnrealEngine(py_object, obj, class_type_name);

            return py_object;
        }
        default:
        {
            PyErr_SetString(PyExc_ValueError, "Unsupported return value type");
            return NULL;
        }

    }

    return NULL;
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
    CHECK_CLIENT_AND_RECREATE_IT()

    PyObject* object = NULL;
    UnrealObject* unreal_object = NULL;
    ClassProp* ue_class = NULL;
    char* function_name = NULL;
    PyObject* params = NULL;

    if (!PyArg_ParseTuple(args, "OO!O!sO!", &object, &UnrealObject_Type, &unreal_object,
                         &ClassProp_Type, &ue_class, &function_name, &PyList_Type, &params)) {
        return NULL;
    }

    auto call_function_request = ue_core_client->client.callFunctionRequest();
    call_function_request.initOwn().setAddress(reinterpret_cast<uint64_t>(object));
    call_function_request.initUeClass().setTypeName(ue_class->type_name);

    auto call_object = call_function_request.initCallObject();
    call_object.setName(unreal_object->name);
    call_object.setAddress(unreal_object->address);

    call_function_request.setFuncName(function_name);

    // handle params
    Py_ssize_t list_size = PyList_Size(params);
    capnp::List<UnrealCore::Argument, capnp::Kind::STRUCT>::Builder call_function_args = call_function_request.initParams(list_size);

    if (!SetupArguments(params, call_function_args, list_size))
    {
        PyErr_SetString(PyExc_TypeError, "Parse params failed");
        return NULL;
    }

    kj::WaitScope& wait_scope = io_context.waitScope;
    CATCH_EXCEPTION_FOR_RPC_CALL({
        capnp::Response<UnrealCore::CallFunctionResults> result = call_function_request.send().wait(wait_scope);
        
        // 对于返回值是unreal object的情况的处理：
        // 1. 根据类名创建一个对应的pyobject
        // 2. 将获取到的unreal object设置到对应的property中
        // 3. 返回创建的pyobject
        PyObject* return_value = ParseValueFromFunctionReturn(result);
        if (return_value == NULL) {
            return NULL;
        }

        return return_value; // todo: tuple for other output params
    })
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

static PyMethodDef unreal_core_methods[] = {
    {"new_object", unreal_core_new_object, METH_VARARGS, "Create a new unreal object"},
    {"destory_object", unreal_core_destory_object, METH_VARARGS, "Destory a unreal object"},
    {"call_function", unreal_core_call_function, METH_VARARGS, "Call a function"},
    {"call_static_function", unreal_core_call_static_function, METH_VARARGS, "Call a static function"},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef unreal_core_module = {
    PyModuleDef_HEAD_INIT,
    "unreal_core",
    "unreal engine core rpc framework apis",
    -1,
    unreal_core_methods,
};

PyMODINIT_FUNC PyInit_unreal_core(void)
{
    PyObject* m;
    if (PyType_Ready(&ClassProp_Type) < 0) {
        return NULL;
    }
    if (PyType_Ready(&Argument_Type) < 0) {
        return NULL;
    }
    if (PyType_Ready(&Method_Type) < 0) {
        return NULL;
    }

    ue_core_client = create_ue_core_client();
    if (ue_core_client == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "connect to unreal engine rpc server failed, run unreal engine at first.");
        return NULL;
    }

    m = PyModule_Create(&unreal_core_module);
    if (m == NULL) {
        return NULL;
    }

    Py_INCREF(&ClassProp_Type);
    Py_INCREF(&Argument_Type);
    Py_INCREF(&Method_Type);

    PyModule_AddObject(m, "Class", (PyObject*)&ClassProp_Type);
    PyModule_AddObject(m, "Argument", (PyObject*)&Argument_Type);
    PyModule_AddObject(m, "Method", (PyObject*)&Method_Type);

    return m;
}
