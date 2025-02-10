import unreal_core 
from unreal_core import ClassProp, Argument, UnrealObject
from enum import Enum
from typing import TypeVar, MutableSequence

_ElemType = TypeVar('_ElemType')
_KeyType = TypeVar('_KeyType')
_ValueType = TypeVar('_ValueType')

class Array(MutableSequence[_ElemType]):
    def __init__(self, element_type: type, size: int = 1) -> None:
        self._size = size
        self._data = [0] * size
        if not isinstance(element_type, type):
            raise ValueError("element_type must be a type")
        print("element_type: ", element_type.__name__)
        print(size)
        self._container_type = ClassProp("Array")
        self._ue_value_type = ClassProp(element_type.__name__)
        self._ue_key_type = ClassProp("")
        self._ue_container = unreal_core.new_container(self, self._container_type, self._ue_value_type, self._ue_key_type)
        print("ue_container: ", self._ue_container)
    
    def __setitem__(self, index: int, value: _ElemType) -> None:
        print("setitem: ", index, value)
        arg1 = Argument("Index", self._container_type, index, "int")
        arg2 = Argument("Value", self._container_type, value, _ElemType.__name__)
        unreal_core.call_function(self, self._ue_container, self._container_type, "Set", [arg1, arg2])

    def __getitem__(self, index: int) -> _ElemType:
        print("getitem: ", index)
        arg1 = Argument("Index", self._container_type, index, "int")
        val = unreal_core.call_function(self, self._ue_container, self._container_type, "Get", [arg1])
        return val[0]
    
    def __len__(self) -> int:
        return len(self._data)
    
    def __del__(self) -> None:
        unreal_core.destroy_container(self)
        
    def __repr__(self) -> str:
        return f"Array({self._size})"
    
    def __delitem__(self, index: int) -> None:
        pass

    def insert(self, index: int, value: _ElemType) -> None:
        self._data.insert(index, value)

    def append(self, value: _ElemType) -> None:
        self._data.append(value)

class MyObject:
    def __init__(self) -> None:
        self._ue_class = ClassProp("MyObject")
        # fixme@Caleb196x: object name must be unique
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
        return unreal_core.call_function(self, self._ue_obj, self._ue_class, "Add", [arg1, arg2])
    
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
    """
    Test enum
    """
    TEST = 0
    TEST2 = 1
    TEST3 = 2

def test_func():
    """
    Test function
    """
    print("test_func")