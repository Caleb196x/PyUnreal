import sys
import gc

from typing import List
from pyunreal.UE import Vector2D, MyObject, MyEnum, Array, test_func
import faulthandler
faulthandler.enable()

if __name__ == "__main__":
    vector = Vector2D(1.0, 2.0)
    print(vector.X)
    print(vector.Y)
    vector.X = 6.0
    vector.Y = 4.0
    print(vector.X)
    print(vector.Y)

    obj = MyObject()
    value = obj.add(3.0, 4.0)
    vec_value = obj.TestVector(vector)
    a = obj.TestEnum(MyEnum.TEST3)

    # print(vector)
    # print(f"Original ref count of value: {sys.getrefcount(value)}")
    # print(f"Original ref count of vector: {sys.getrefcount(vector)}")
    # print(type(value), value[0])
    print(type(vec_value), vec_value[0])
    print("obj type: ", type(MyObject))
    print("obj address: ", obj)

    a = []
    a.append(MyObject())
    a.append(1)
    a.append(2)

    for i in a:
        print(i)

    numbers: List[int] = [1, "2", 3]
    print(numbers)

    arr : Array[int] = Array[int](int, 10)
    arr[0] = 1
    arr[1] = 2
    print(arr[1])


