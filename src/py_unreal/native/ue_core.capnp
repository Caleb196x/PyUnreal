@0xa43f02d9830ffb2e;

interface UnrealCore
{
    struct Object
    {
        name @0 :Text;
        address @1 :UInt64;
    }

    struct Class
    {
        typeName @0 :Text;
    }

    struct Argument
    {
        class @0 :Class;
        name @1 :Text;
        union {
            boolValue @2 :Bool;
            uintValue @3 :UInt64;
            intValue @4 :Int64;
            strValue @5 :Text;
            floatValue @6 :Float64;
            object @7 :Object;
            enumValue @8 :Int64;
        }
    }

    struct Property
    {
        class @0 :Class;
        name @1 :Text;
        union {
            boolValue @2 :Bool;
            uintValue @3 :UInt64;
            intValue @4 :Int64;
            strValue @5 :Text;
            floatValue @6 :Float64;
            object @7 :Object;
            enumValue @8 :Int64;
        }
    }

    struct Method
    {
        name @0 :Text;
    }

    interface MethodCallable
    {
        invoke @0 (method :Method) -> ();
    }

    interface DelegateCallback 
    {
        onCall @0 (object :Object, params :List(Argument)) -> ();
    }

    newObject @0 (outer :Object, class :Class, objName :Text, flags :UInt64, constructArgs :List(Argument)) -> (object :Object);
    destroyObject @1 (outer :Object) -> (result :Bool);

    callFunction @2 (outer :Object, callObject :Object, class :Class, funcName :Text, params :List(Argument)) -> (return :Argument, outParams :List(Argument));
    callStaticFunction @3 (class :Class, funcName :Text, params :List(Argument)) -> (return :Argument, outParams :List(Argument));

    findClass @4 (class :Class) -> (object :Object);
    loadClass @5 (class :Class) -> (object :Object);
    staticClass @6 (object :Object) -> (class :Class);

    bindDelegate @7 (object :Object, callback :DelegateCallback) -> ();
    unbindDelegate @8 (object :Object) -> ();
    addMultiDelegate @9 (object :Object, callback :DelegateCallback) -> (delegateObject :Object);
    removeMultiDelegate @10 (object :Object, delegateObject :Object) -> ();

    registerOverrideClass @11 (class :Class, parent :Class, methods :List(Method)) -> ();
    unregisterOverrideClass @12 (class :Class, parent :Class) -> ();

    setProperty @13 (class :Class, owner :Object, property :Property) -> ();
    getProperty @14 (class :Class, owner :Object, propertyName :Text) -> (property :Property);
}