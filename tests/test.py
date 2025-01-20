import unreal_core 
from unreal_core import UnrealObject, ClassProp, Argument
import sys
import faulthandler
faulthandler.enable()

class MyObject:
    def __init__(self) -> None:
        self._ue_class = ClassProp("MyObject")
        self._ue_obj = unreal_core.new_object(self, self._ue_class, "test", 0, [])
        # print(self._ue_obj)

    def __del__(self):
        print("destory object: ", hex(id(self)))
        unreal_core.destory_object(self)

    @property
    def ue_obj(self):
        return self._ue_obj
    
    def add(self, a, b):
        arg1 = Argument("a", self._ue_class, a)
        arg2 = Argument("b", self._ue_class, b)
        return unreal_core.call_function(self, self._ue_obj, self._ue_class, "Add", [arg1, arg2])
    
if __name__ == "__main__":
    obj = MyObject()
    print(obj)
    print("finished")
    print(obj.add(5, 2))
    # del obj
