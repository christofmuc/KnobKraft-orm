from typing import List
import functools

def list_compare(list1: List, list2: List) -> bool:
    if len(list1) != len(list2):
        print(f"\nlist1: {list1}")
        print(f"list2: {list2}")
        print(f"Lists differ in lengths: {len(list1)} != {len(list2)}")
        return False
    result = True
    for i in range(len(list1)):
        if list1[i] != list2[i]:
            print(f"\nlist1: {list1}")
            print(f"list2: {list2}")
            print(f"Result differ at position {i}: {list1[i]} vs {list2[i]}")
            result = False
    return result


