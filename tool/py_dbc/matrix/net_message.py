#!/usr/bin/env python
import struct
class net_message:
    def __init__(self):
        self.lenth = 0
        self.data=str("")
    def set_lenth(self, lenth):
        self.lenth = lenth
    def add_data(self, in_data):
        if len(in_data) < 1:
            # print("0")
            return
        self.data=in_data
