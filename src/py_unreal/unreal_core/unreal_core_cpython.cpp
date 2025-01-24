#include <Python.h>
#include "ue_core.capnp.h"
#include <kj/async-io.h>
#include <capnp/rpc-twoparty.h>
#include <windows.h>
#include <exception>
#include <cstring>
#include <string>

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
        size_t err_size = strlen(err_file) + strlen(err_msg) + 5; \
        char* err_str = (char*)malloc(err_size); \
        sprintf(err_str, "[%s]:[%d]: %s", err_file, err_line, err_msg); \
        PyErr_SetString(PyExc_RuntimeError, err_str); \
        free(err_str); \
        return NULL; \
    } \

#define PYTHON_MODULE_NAME "py_unreal"
#define UNREAD_OBJECT_PROPERTY_NAME "unreal_object"


/**
 * Utils functions
 */
static inline char* deep_copy_str(const char* src)
{
    char* dest = (char*)malloc(strlen(src));
    strcpy(dest, src);
    return dest;
}

int is_enum_type(PyObject* obj) {
    PyObject* enum_module = PyImport_ImportModule("enum");
    if (!enum_module) return 0;

    PyObject* enum_class = PyObject_GetAttrString(enum_module, "Enum");
    if (!enum_class) {
        Py_DECREF(enum_module);
        return 0;
    }

    int result = PyObject_IsInstance(obj, enum_class);

    Py_DECREF(enum_class);
    Py_DECREF(enum_module);
    return result;
}

/**
 * capnp client
 */
typedef struct {
    const char* name;
    kj::Own<kj::AsyncIoStream> connection;
    kj::Own<capnp::TwoPartyClient> client;
} CapnpClient;

static PyTypeObject CapnpClient_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
};

static CapnpClient* ue_core_client = NULL;
static kj::AsyncIoContext io_context = kj::setupAsyncIo();
static uint16_t server_port = 0;

static const char* format_win_characters(const char* message)
{
#ifdef _WIN32
                // On Windows, convert to wide string and back to handle encoding
                int wide_size = MultiByteToWideChar(CP_UTF8, 0, message, -1, NULL, 0);
                wchar_t* wide_str = new wchar_t[wide_size];
                MultiByteToWideChar(CP_UTF8, 0, message, -1, wide_str, wide_size);
                
                int ansi_size = WideCharToMultiByte(CP_ACP, 0, wide_str, -1, NULL, 0, NULL, NULL);
                char* ansi_str = new char[ansi_size];
                WideCharToMultiByte(CP_ACP, 0, wide_str, -1, ansi_str, ansi_size, NULL, NULL);
                
                delete[] wide_str;
                message = ansi_str;
#endif
    return message;
}

static CapnpClient* create_ue_core_client()
{
    CapnpClient* rpc_client = (CapnpClient*)malloc(sizeof(CapnpClient));
    if (rpc_client == NULL) {
        return NULL;
    }

    std::memset(rpc_client, 0, sizeof(CapnpClient));

    rpc_client->name = "unreal_core_client";
    kj::Network& network = io_context.provider->getNetwork();
    auto& wait_scope = io_context.waitScope;
    uint16_t start_port = 60001;
    uint16_t end_port = 60005;

    // find the right port from start_port to end_port
    for (uint16_t port = start_port; port <= end_port; ++port) {
        char ip_addr[20];
        sprintf(ip_addr, "127.0.0.1:%d", port);

        try{
            kj::Own<kj::NetworkAddress> address = network.parseAddress(ip_addr).wait(wait_scope);
            kj::Own<kj::AsyncIoStream> conn = address->connect().wait(wait_scope);
            if (conn.get() == nullptr) {
                continue;
            }

            // save connection
            rpc_client->connection = kj::mv(conn);

            // create and save rpc client
            rpc_client->client = kj::heap<capnp::TwoPartyClient>(*rpc_client->connection);

            server_port = port;

            printf("connect to unreal rpc server: %s success\n", ip_addr);

            break;
            
        } catch (kj::Exception& e) {
            const char* err_desc = e.getDescription().cStr();
#ifdef _WIN32
                // On Windows, convert to wide string and back to handle encoding
                const char* formatted_desc = format_win_characters(err_desc);
#endif
            printf("connect to unreal rpc server %s failed, error: %s, try next port\n", ip_addr, formatted_desc);
            delete[] formatted_desc;
            
            continue;

        } catch (std::exception& e) {
            printf("connect to unreal rpc server %s failed, error: %s\n", ip_addr, e.what());

            break;
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
    char* name;
    uint64_t address;
} UnrealObject;

static PyObject* UnrealObject_repr(UnrealObject* self)
{
    if (self->name == NULL) {
        return PyUnicode_FromFormat("UnrealObject(name=NULL)");
    }
    return PyUnicode_FromFormat("UnrealObject(name=%s, address=0x%016llx)", self->name, self->address);
}

static int UnrealObject_init(UnrealObject* self, PyObject* args)
{
    const char* name = NULL;
    uint64_t address = 0;

    if (!PyArg_ParseTuple(args, "K|s", &address, &name)) {
        return -1;
    }
    
    if (name != NULL) {
        self->name = deep_copy_str(name);
    }

    self->address = address;
    return 0;
}

static PyObject* UnrealObject_new(PyTypeObject* type, PyObject* args, PyObject* kwds)
{
    UnrealObject* self = (UnrealObject*)type->tp_alloc(type, 0);
    if (self != NULL) {
        self->name = "";
        self->address = 0;
    }
    return (PyObject*)self;
}

static void UnrealObject_dealloc(UnrealObject* self)
{
    if (self->name != NULL) free(self->name);
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyTypeObject UnrealObject_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "unreal_core.UnrealObject",        /* tp_name */
    sizeof(UnrealObject),              /* tp_basicsize */
    0,                                 /* tp_itemsize */
    (destructor)UnrealObject_dealloc,  /* tp_dealloc */
    0,                                 /* tp_print */
    0,                                 /* tp_getattr */
    0,                                 /* tp_setattr */
    0,                                 /* tp_reserved */
    (reprfunc)UnrealObject_repr,       /* tp_repr */
    0,                                 /* tp_as_number */
    0,                                 /* tp_as_sequence */
    0,                                 /* tp_as_mapping */
    0,                                 /* tp_hash */
    0,                                 /* tp_call */
    0,                                 /* tp_str */
    0,                                 /* tp_getattro */
    0,                                 /* tp_setattro */
    0,                                 /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,                 /* tp_flags */
    "Unreal Engine Object",            /* tp_doc */
    0,                                 /* tp_traverse */
    0,                                 /* tp_clear */
    0,                                 /* tp_richcompare */
    0,                                 /* tp_weaklistoffset */
    0,                                 /* tp_iter */
    0,                                 /* tp_iternext */
    0,                                 /* tp_methods */
    0,                                 /* tp_members */
    0,                                 /* tp_getset */
    0,                                 /* tp_base */
    0,                                 /* tp_dict */
    0,                                 /* tp_descr_get */
    0,                                 /* tp_descr_set */
    0,                                 /* tp_dictoffset */
    (initproc)UnrealObject_init,       /* tp_init */
    0,                                 /* tp_alloc */
    UnrealObject_new,                  /* tp_new */
};

/*
 * Class
 */
typedef struct {
    PyObject_HEAD
    std::string type_name;
} ClassProp;

static PyObject* ClassProp_repr(ClassProp* self)
{
    if (self->type_name.empty()) {
        return PyUnicode_FromFormat("Class(type_name=NULL)");
    }

    return PyUnicode_FromFormat("Class(type_name=%s)", self->type_name);
}

static int ClassProp_init(ClassProp* self, PyObject* args, PyObject* kwargs)
{
    const char* type_name = NULL;

    if (!PyArg_ParseTuple(args, "s", &type_name)) {
        return -1;
    }

    self->type_name = std::move(std::string(type_name));
    return 0;
}

static PyObject* ClassProp_new(PyTypeObject* type, PyObject* args, PyObject* kwds) 
{
    ClassProp* self = (ClassProp*)type->tp_alloc(type, 0);
    return (PyObject*)self;
}

static void ClassProp_dealloc(ClassProp* self) 
{
    self->type_name.~basic_string();
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyTypeObject ClassProp_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "unreal_core.ClassProp",           /* tp_name */
    sizeof(ClassProp),                 /* tp_basicsize */
    0,                             /* tp_itemsize */
    (destructor)ClassProp_dealloc,     /* tp_dealloc */
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
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,  /* tp_flags */
    "Unreal Engine Class Property",    /* tp_doc */
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
    0,                             /* tp_alloc */
    ClassProp_new,                 /* tp_new */
};

/*
 * Property
 */
typedef enum {
    ARGUMENT_TYPE_BOOL = 0,
    ARGUMENT_TYPE_UINT = 1,
    ARGUMENT_TYPE_INT = 2,
    ARGUMENT_TYPE_FLOAT = 3,
    ARGUMENT_TYPE_STRING = 4,
    ARGUMENT_TYPE_ENUM = 5,
    ARGUMENT_TYPE_OBJECT = 6,
} ArgumentType;

typedef struct {
    PyObject_HEAD
    ClassProp* ue_class;
    std::string name;
    ArgumentType value_type;
    union {
        bool bool_value;
        uint64_t uint_value;
        int64_t int_value;
        double float_value;
        const char* str_value;
        int64_t enum_value;
        PyObject* object;
    };
} Argument;

static PyObject* Argument_repr(Argument* self)
{
    switch (self->value_type) {
        case ARGUMENT_TYPE_BOOL:
            return PyUnicode_FromFormat("Argument(name=%s, ue_class=%s, value_type=%d, bool_value=%d)", 
                self->name, self->ue_class->type_name.c_str(), self->value_type, self->bool_value);
        case ARGUMENT_TYPE_UINT:
            return PyUnicode_FromFormat("Argument(name=%s, ue_class=%s, value_type=%d, uint_value=%lld)", 
                self->name, self->ue_class->type_name.c_str(), self->value_type, self->uint_value);
        case ARGUMENT_TYPE_INT:
            return PyUnicode_FromFormat("Argument(name=%s, ue_class=%s, value_type=%d, int_value=%lld)", 
                self->name, self->ue_class->type_name.c_str(), self->value_type, self->int_value);
        case ARGUMENT_TYPE_FLOAT:
            return PyUnicode_FromFormat("Argument(name=%s, ue_class=%s, value_type=%d, float_value=%f)", 
                self->name, self->ue_class->type_name.c_str(), self->value_type, self->float_value);
        case ARGUMENT_TYPE_STRING:
            return PyUnicode_FromFormat("Argument(name=%s, ue_class=%s, value_type=%d, str_value=%s)", 
                self->name, self->ue_class->type_name.c_str(), self->value_type, self->str_value);
        case ARGUMENT_TYPE_ENUM:
            return PyUnicode_FromFormat("Argument(name=%s, ue_class=%s, value_type=%d, enum_value=%lld)", 
                self->name, self->ue_class->type_name.c_str(), self->value_type, self->enum_value);
        case ARGUMENT_TYPE_OBJECT:
            return PyUnicode_FromFormat("Argument(name=%s, ue_class=%s, value_type=%d, object=0x%llx)", 
                self->name, self->ue_class->type_name.c_str(), self->value_type, self->object); 
    }
}

static int Argument_init(Argument* self, PyObject* args)
{
    char* name = NULL;    
    ClassProp* ue_class = NULL;
    PyObject* value = NULL;
    char* value_type = NULL;
    if (!PyArg_ParseTuple(args, "sO!O|s", &name, &ClassProp_Type, &ue_class, &value, &value_type)) {
        return -1;
    }
    if (ue_class == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "ue_class is required");
        return -1;
    }

    if (ue_class->type_name.empty()) {
        PyErr_SetString(PyExc_RuntimeError, "ue_class is not initialized");
        return -1;
    }

    self->name = std::move(std::string(name));
    self->ue_class = ue_class;

    if (value == NULL) {
        return 0;
    }

    if (PyBool_Check(value))
    {
        self->value_type = ARGUMENT_TYPE_BOOL;
        self->bool_value = PyObject_IsTrue(value);
    }
    else if (PyLong_Check(value)) {

        // special case: enum
        if (value_type != NULL && strcmp(value_type, "enum") == 0) {
            self->value_type = ARGUMENT_TYPE_ENUM;     
            self->enum_value = PyLong_AsLongLong(value); 
        }
        else {
            self->value_type = ARGUMENT_TYPE_INT;
            self->int_value = PyLong_AsLongLong(value);    
        }
    }
    else if (PyFloat_Check(value)) {

        // special case: int
        if (value_type != NULL && strcmp(value_type, "int") == 0) {
            self->value_type = ARGUMENT_TYPE_INT;
            self->int_value = PyLong_AsLong(PyNumber_Long(value)); // convert float to int
        }
        else {
            self->value_type = ARGUMENT_TYPE_FLOAT;
            self->float_value = PyFloat_AsDouble(value);
        }
    }
    else if (PyUnicode_Check(value)) {
        self->value_type = ARGUMENT_TYPE_STRING;
        self->str_value = PyUnicode_AsUTF8(value);
    }
    else if (is_enum_type(value)) {
        printf("enum type\n");
        PyObject* enum_value = PyObject_GetAttrString(value, "value");
        if (!value) {
            PyErr_SetString(PyExc_RuntimeError, "Can not read the value of enum object");
            return -1;
        }
        self->value_type = ARGUMENT_TYPE_ENUM;
        self->enum_value = PyLong_AsLongLong(enum_value);
        printf("enum value: %lld\n", self->enum_value);
    }
    else {
        // todo@Caleb196x: implement special type such as list, set or map
        self->value_type = ARGUMENT_TYPE_OBJECT;
        self->object = value;
        Py_INCREF(self->object);
    }

    return 0;
}

static PyObject* Argument_new(PyTypeObject* type, PyObject* args, PyObject* kwds)
{
    Argument* self;
    self = (Argument*)type->tp_alloc(type, 0);
    if (self != NULL) {
        self->name = "";
        self->ue_class = NULL;
        self->value_type = ARGUMENT_TYPE_BOOL;
        self->bool_value = false;
        self->uint_value = 0;
        self->float_value = 0.0;
        self->str_value = NULL;
        self->enum_value = 0;
        self->object = NULL;
    }
    return (PyObject*)self;
}

static void Argument_dealloc(Argument* self)
{
    if (self->value_type == ARGUMENT_TYPE_OBJECT)
    {
        Py_DECREF(self->object);
    }

    Py_TYPE(self)->tp_free((PyObject*)self);
}


static PyTypeObject Argument_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "unreal_core.Argument",           /* tp_name */
    sizeof(Argument),                 /* tp_basicsize */
    0,                             /* tp_itemsize */
    (destructor)Argument_dealloc,  /* tp_dealloc */
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
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,           /* tp_flags */
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
    0,
    Argument_new
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

static bool create_unreal_rpc_argument(PyObject* py_argument, UnrealCore::Argument::Builder& unreal_core_argument)
{
    if (!PyObject_TypeCheck(py_argument, &Argument_Type)) {
        return false;
    }

    Argument* argument = (Argument*)py_argument;
    unreal_core_argument.setName(argument->name);
    unreal_core_argument.initUeClass().setTypeName(argument->ue_class->type_name);

    switch (argument->value_type) {
        case ARGUMENT_TYPE_BOOL:
            unreal_core_argument.setBoolValue(argument->bool_value);
            break;
        case ARGUMENT_TYPE_INT:
            unreal_core_argument.setIntValue(argument->int_value);
            break;
        case ARGUMENT_TYPE_UINT:
            unreal_core_argument.setUintValue(argument->uint_value);
            break;
        case ARGUMENT_TYPE_FLOAT:
            unreal_core_argument.setFloatValue(argument->float_value);
            break;
        case ARGUMENT_TYPE_STRING:
            unreal_core_argument.setStrValue(argument->str_value);
            break;
        case ARGUMENT_TYPE_ENUM:
            unreal_core_argument.setEnumValue(argument->enum_value);
            break;
        case ARGUMENT_TYPE_OBJECT:
        {
            unreal_core_argument.initObject().setAddress(reinterpret_cast<uint64_t>(argument->object));
            break;
        }
        default:
        {
            return false;
        }
    }
    return true;
}


static bool setup_unreal_rpc_arguments_from_list(PyObject* src_args, capnp::List<UnrealCore::Argument, capnp::Kind::STRUCT>::Builder& dest_args, Py_ssize_t list_size)
{
    for (uint32_t i = 0; i < list_size; i++) {
        PyObject* item = PyList_GetItem(src_args, i);
        if (item == NULL) {
            PyErr_SetString(PyExc_RuntimeError, "Failed to get a parameter from the parameter list.");
            return false;
        }

        if (PyObject_TypeCheck(item, &Argument_Type)) {
            Argument* argument = (Argument*)item;
            dest_args[i].setName(argument->name);
            dest_args[i].initUeClass().setTypeName(argument->ue_class->type_name);

            if (!create_unreal_rpc_argument(item, dest_args[i])) {
                PyErr_SetString(PyExc_RuntimeError, "Failure to create parameters needed for unreal rpc call.");
                return false;
            }
        }
        else {
            PyErr_SetString(PyExc_RuntimeError, "The type of the argument passed into the function must be the 'Argument' type.");
            return false;
        }
    }

    return true;
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

    // if (object == Py_None)
    // {
    //     PyErr_SetString(PyExc_RuntimeError, "object can not be None.");
    //     return NULL;
    // }

    UnrealCore::Client client = ue_core_client->client->bootstrap().castAs<UnrealCore>();

    capnp::Request<UnrealCore::NewObjectParams, UnrealCore::NewObjectResults> new_object_request = client.newObjectRequest();
    new_object_request.getUeClass().setTypeName(ue_class->type_name);
    new_object_request.setFlags(flags);
    new_object_request.setObjName(object_name);
    new_object_request.getOwn().setAddress(reinterpret_cast<uint64_t>(object));
    new_object_request.getOwn().setName(object_name);

    // handle construct_args
    // get list size
    Py_ssize_t list_size = PyList_Size(construct_args);
    capnp::List<UnrealCore::Argument, capnp::Kind::STRUCT>::Builder new_object_args = new_object_request.initConstructArgs(list_size);

    if (!setup_unreal_rpc_arguments_from_list(construct_args, new_object_args, list_size))
    {
        return NULL;
    }

    kj::WaitScope& wait_scope = io_context.waitScope;
    CATCH_EXCEPTION_FOR_RPC_CALL({
        capnp::Response<UnrealCore::NewObjectResults> result = new_object_request.send().catch_([](kj::Exception& e){
            switch (e.getType()) {
                case kj::Exception::Type::FAILED:
                    PyErr_SetString(PyExc_RuntimeError, e.getDescription().cStr());
                    break;
                case kj::Exception::Type::DISCONNECTED:
                    PyErr_SetString(PyExc_RuntimeError, "connection to unreal engine is lost, error: ");
                    break;
                default:
                    PyErr_SetString(PyExc_RuntimeError, e.getDescription().cStr());
                    break;
            }
            
        }).wait(wait_scope);
        
        UnrealObject* unreal_object = (UnrealObject*)PyObject_New(UnrealObject, &UnrealObject_Type);
        unreal_object->address = result.getObject().getAddress();
        unreal_object->name = deep_copy_str(result.getObject().getName().cStr());

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

    UnrealCore::Client client = ue_core_client->client->bootstrap().castAs<UnrealCore>();
    auto destory_object_request = client.destroyObjectRequest();
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

static PyObject* create_object_from_specified_class(const char* class_type_name)
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
        Py_DECREF(py_class);
        return NULL;
    }

    Py_DECREF(py_module);
    Py_DECREF(py_class);

    return py_object;
}

static PyObject* send_pyobject_to_unreal_engine(PyObject* py_object, UnrealObject* unreal_object, const char* class_type_name)
{
    UnrealCore::Client client = ue_core_client->client->bootstrap().castAs<UnrealCore>();
    auto create_py_object_request = client.registerCreatedPyObjectRequest();
    create_py_object_request.initPyObject().setAddress(reinterpret_cast<uint64_t>(py_object));
    create_py_object_request.initUnrealObject().setAddress(unreal_object->address);
    create_py_object_request.initUeClass().setTypeName(class_type_name);

    kj::WaitScope& wait_scope = io_context.waitScope;
    CATCH_EXCEPTION_FOR_RPC_CALL({
        create_py_object_request.send().wait(wait_scope);
        return Py_True;
    })
}

static PyObject* parse_value_from_function_return(const  UnrealCore::Argument::Reader& return_value, bool is_retrun_value)
{
    // Initialize the object's fields
    const char* class_type_name = return_value.getUeClass().getTypeName().cStr();

    if (class_type_name != NULL && strcmp(class_type_name, "void") == 0) {
        return Py_None;
    }

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
        case UnrealCore::Argument::INT_VALUE:
            return PyLong_FromLongLong(return_value.getIntValue());
        case UnrealCore::Argument::FLOAT_VALUE:
            return PyFloat_FromDouble(return_value.getFloatValue());
        case UnrealCore::Argument::STR_VALUE:
            return PyUnicode_FromString(return_value.getStrValue().cStr());
        case UnrealCore::Argument::ENUM_VALUE:
            return PyLong_FromLongLong(return_value.getEnumValue());
        case UnrealCore::Argument::OBJECT:
        {
            if (!is_retrun_value) {
                uint64_t address = return_value.getObject().getAddress();
                void* object = (void*)address;
                return (PyObject*)object;
            }

            // Handle the case where return value is an unreal object:
            // 1. Create a corresponding pyobject based on the class name
            // 2. Set the obtained unreal object to the corresponding property
            // 3. Return the created pyobject
            // Create a new UnrealObject for object returns
            UnrealObject* obj = (UnrealObject*)PyObject_New(UnrealObject, &UnrealObject_Type);
            if (obj == NULL) {
                obj = (UnrealObject*)PyObject_New(UnrealObject, &UnrealObject_Type); // try again
            }
            obj->address = return_value.getObject().getAddress();
            obj->name = deep_copy_str(return_value.getObject().getName().cStr());
            PyObject* py_object = create_object_from_specified_class(class_type_name);
            if (py_object == NULL) {
                return NULL;
            }
            // set unreal object to the py object's property, property name is UNREAD_OBJECT_PROPERTY_NAME
            if (PyObject_SetAttrString(py_object, UNREAD_OBJECT_PROPERTY_NAME, (PyObject*)obj) != 0) {
                PyErr_SetString(PyExc_AttributeError, "Failed to set unreal object to the py object's unreal objectproperty");
                Py_DECREF(py_object);
                return NULL;
            }

            // register the py object to unreal engine
            send_pyobject_to_unreal_engine(py_object, obj, class_type_name);

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

    UnrealCore::Client client = ue_core_client->client->bootstrap().castAs<UnrealCore>();
    auto call_function_request = client.callFunctionRequest();
    call_function_request.initOwn().setAddress(reinterpret_cast<uint64_t>(object));
    call_function_request.initUeClass().setTypeName(ue_class->type_name);

    auto call_object = call_function_request.initCallObject();
    call_object.setName(unreal_object->name);
    call_object.setAddress(unreal_object->address);

    call_function_request.setFuncName(function_name);

    // handle params
    Py_ssize_t list_size = PyList_Size(params);
    capnp::List<UnrealCore::Argument, capnp::Kind::STRUCT>::Builder call_function_args = call_function_request.initParams((uint32_t)list_size);

    if (!setup_unreal_rpc_arguments_from_list(params, call_function_args, list_size))
    {
        return NULL;
    }

    kj::WaitScope& wait_scope = io_context.waitScope;
    CATCH_EXCEPTION_FOR_RPC_CALL({
        capnp::Response<UnrealCore::CallFunctionResults> result = call_function_request.send().wait(wait_scope);
        
        PyObject* return_value = parse_value_from_function_return(result.getReturn(), true);
        if (return_value == NULL) {
            return NULL; 
        }

        if (Py_None == return_value) {
            return PyTuple_New(0);
        }

        // handle out params
        auto out_params = result.getOutParams();
        auto out_params_size = out_params.size();

        PyObject* tuple = PyTuple_New(out_params_size + 1); // +1 for return value
        PyTuple_SetItem(tuple, 0, return_value);

        for (Py_ssize_t i = 0; i < out_params_size; ++i) {
            // fixme: do not create new py object when out params is unreal object, directly return the passed in py object
            PyObject* out_param = parse_value_from_function_return(out_params[i], false);
            if (out_param == NULL) {
                Py_DECREF(tuple);
                Py_DECREF(return_value);
                return NULL;
            }

            PyTuple_SetItem(tuple, i + 1, out_param);
        }

        return tuple; 
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
    CHECK_CLIENT_AND_RECREATE_IT()

    ClassProp* ue_class = NULL;
    char* function_name = NULL;
    PyObject* params = NULL;

    if (!PyArg_ParseTuple(args, "O!sO!", &ClassProp_Type, &ue_class, &function_name, &PyList_Type, &params)) {
        return NULL;
    }

    UnrealCore::Client client = ue_core_client->client->bootstrap().castAs<UnrealCore>();
    auto call_static_function_request = client.callStaticFunctionRequest();
    call_static_function_request.initUeClass().setTypeName(ue_class->type_name);

    call_static_function_request.setFuncName(function_name);

    // handle params
    Py_ssize_t list_size = PyList_Size(params);
    capnp::List<UnrealCore::Argument, capnp::Kind::STRUCT>::Builder call_function_args = call_static_function_request.initParams(list_size);

    if (!setup_unreal_rpc_arguments_from_list(params, call_function_args, list_size))
    {
        return NULL;
    }

    kj::WaitScope& wait_scope = io_context.waitScope;
    CATCH_EXCEPTION_FOR_RPC_CALL({
        capnp::Response<UnrealCore::CallStaticFunctionResults> result = call_static_function_request.send().wait(wait_scope);
        
        PyObject* return_value = parse_value_from_function_return(result.getReturn(), true);
        if (return_value == NULL) {
            return NULL; 
        }

        // handle out params
        auto out_params = result.getOutParams();
        auto out_params_size = out_params.size();

        PyObject* tuple = PyTuple_New(out_params_size + 1); // +1 for return value
        PyTuple_SetItem(tuple, 0, return_value);

        for (Py_ssize_t i = 0; i < out_params_size; ++i) {
            // fixme: do not create new py object when out params is unreal object, directly return the passed in py object
            PyObject* out_param = parse_value_from_function_return(out_params[i], false); 
            PyTuple_SetItem(tuple, i + 1, out_param);
        }

        return tuple; 
    })

}

/**
 * unreal_core.get_property
 * call rpc function (getProperty) to get a property
 * 
 * args:
 *   ue_class: ue class name
 *   object: pyobject
 *   property_name: Property name
 * 
 */
static PyObject* unreal_core_get_property(PyObject* self, PyObject* args)
{
    CHECK_CLIENT_AND_RECREATE_IT()

    ClassProp* ue_class = NULL;
    PyObject* object = NULL;
    char* property_name = NULL;

    if (!PyArg_ParseTuple(args, "OO!s", &object, &ClassProp_Type, &ue_class, &property_name)) {
        return NULL;
    }

    UnrealCore::Client client = ue_core_client->client->bootstrap().castAs<UnrealCore>();
    auto get_property_request = client.getPropertyRequest();
    get_property_request.initUeClass().setTypeName(ue_class->type_name);
    get_property_request.initOwner().setAddress(reinterpret_cast<uint64_t>(object));
    get_property_request.setPropertyName(property_name);

    kj::WaitScope& wait_scope = io_context.waitScope;
    CATCH_EXCEPTION_FOR_RPC_CALL({
        capnp::Response<UnrealCore::GetPropertyResults> result = get_property_request.send().wait(wait_scope);
        return parse_value_from_function_return(result.getProperty(), false);
    })
}

/**
 * unreal_core.set_property
 * call rpc function (setProperty) to set a property
 * 
 * args:
 *   ue_class: ue class name
 *   object: pyobject
 *   property: struct Argument
 */
static PyObject* unreal_core_set_property(PyObject* self, PyObject* args)
{
    CHECK_CLIENT_AND_RECREATE_IT()

    ClassProp* ue_class = NULL;
    PyObject* object = NULL;
    PyObject* property_value = NULL;

    if (!PyArg_ParseTuple(args, "OO!O!", &object, &ClassProp_Type, &ue_class, &Argument_Type, &property_value)) {
        return NULL;
    }

    UnrealCore::Client client = ue_core_client->client->bootstrap().castAs<UnrealCore>();
    auto set_property_request = client.setPropertyRequest();
    set_property_request.initUeClass().setTypeName(ue_class->type_name);
    set_property_request.initOwner().setAddress(reinterpret_cast<uint64_t>(object));
    auto unreal_core_argument = set_property_request.initProperty();
    if (!create_unreal_rpc_argument(property_value, unreal_core_argument)) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to create unreal core argument the property should be argument type");
        return NULL;
    }

    kj::WaitScope& wait_scope = io_context.waitScope;
    CATCH_EXCEPTION_FOR_RPC_CALL({
        set_property_request.send().wait(wait_scope);
    })
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
    {"get_property", unreal_core_get_property, METH_VARARGS, "Get a property"},
    {"set_property", unreal_core_set_property, METH_VARARGS, "Set a property"},
    {NULL, NULL, 0, NULL}
};

static void clean_ue_core_client_inner()
{
    if (ue_core_client != NULL) {
        kj::Own<capnp::TwoPartyClient> TMP_cleint = kj::mv(ue_core_client->client);
        kj::Own<kj::AsyncIoStream> TMP_connection = kj::mv(ue_core_client->connection);
        free(ue_core_client);
        ue_core_client = NULL;
    }

}

static int clean_ue_core_client(PyObject* module)
{
    clean_ue_core_client_inner();

    return 0;
}

static struct PyModuleDef unreal_core_module = {
    PyModuleDef_HEAD_INIT,
    "unreal_core",
    "unreal engine core rpc framework apis",
    -1,
    unreal_core_methods,
    0,
    0,
    clean_ue_core_client,
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
    if (PyType_Ready(&UnrealObject_Type) < 0) {
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

    // Py_INCREF(&UnrealObject_Type);
    Py_INCREF(&ClassProp_Type);
    Py_INCREF(&Argument_Type);
    Py_INCREF(&Method_Type);

    if (PyModule_AddObject(m, "ClassProp", (PyObject*)&ClassProp_Type) < 0) {
        Py_DECREF(&ClassProp_Type);
        Py_DECREF(m);
        return NULL;
    }

    if (PyModule_AddObject(m, "Argument", (PyObject*)&Argument_Type) < 0) {
        Py_DECREF(&Argument_Type);
        Py_DECREF(m);
        return NULL;
    }

    if (PyModule_AddObject(m, "Method", (PyObject*)&Method_Type) < 0) {
        Py_DECREF(&Argument_Type);
        Py_DECREF(m);
        return NULL;
    }
    
    if (PyModule_AddObject(m, "UnrealObject", (PyObject*)&UnrealObject_Type) < 0) {
        Py_DECREF(&Argument_Type);
        Py_DECREF(m);
        return NULL;
    }

    Py_AtExit(clean_ue_core_client_inner);
    
    return m;
}
