import unreal_core 
from unreal_core import ClassProp, Argument
from enum import Enum

class Array:
    def __init__(self, element_type: type, size: int = 1):
        self._size = size
        self._data = [0] * size
        if not isinstance(element_type, type):
            raise ValueError("element_type must be a type")
        print(element_type)
        print(size)

    def __getitem__(self, index: int):
        return self._data[index]
        
class MyObject:
    def __init__(self) -> None:
        self._ue_class = ClassProp("MyObject")
        self._ue_obj = unreal_core.new_object(self, self._ue_class, "test", 0, [])
        # print("MyObject object: ", hex(id(self)))

    def __del__(self):
        # print("destory object: ", hex(id(self)))
        unreal_core.destory_object(self)

    @property
    def ue_obj(self):
        return self._ue_obj
    
    def add(self, a:int, b:int):
        arg1 = Argument("a", self._ue_class, a, "int")
        arg2 = Argument("b", self._ue_class, b, "int")
        return unreal_core.call_function(self, self._ue_obj, self._ue_class, "Add2", [arg1, arg2])
    
    def TestVector(self, vector):
        print("TestVector: ", hex(id(vector)))
        arg = Argument("Vector", self._ue_class, vector)
        return unreal_core.call_function(self, self._ue_obj, self._ue_class, "TestVector", [arg])
    
    def TestEnum(self, enum):
        print("type enum: ", type(enum))
        arg = Argument("Enum", self._ue_class, enum, 'enum')
        return unreal_core.call_function(self, self._ue_obj, self._ue_class, "TestEnum", [arg])
    
class Vector2D:
    def __init__(self, X, Y):
        self._ue_class = ClassProp("Vector2D")
        arg1 = Argument("X", self._ue_class, X)
        arg2 = Argument("Y", self._ue_class, Y)
        self._ue_obj = unreal_core.new_object(self, self._ue_class, "test_vector", 0, [arg1, arg2])
        # print("Vector2D object: ", hex(id(self)))

    def __del__(self):
        # print("destory object: ", hex(id(self)))
        unreal_core.destory_object(self)

    @property
    def ue_obj(self):
        return self._ue_obj
    
    @property
    def X(self):
        return unreal_core.get_property(self, self._ue_class, "X")
    
    @property
    def Y(self):
        return unreal_core.get_property(self, self._ue_class, "Y")
    
    @X.setter
    def X(self, value):
        arg = Argument("X", self._ue_class, value)
        unreal_core.set_property(self, self._ue_class, arg)
    
    @Y.setter
    def Y(self, value):
        arg = Argument("Y", self._ue_class, value)
        unreal_core.set_property(self, self._ue_class, arg)

class MyEnum(Enum):
    TEST = 0
    TEST2 = 1
    TEST3 = 2