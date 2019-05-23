from time import localtime
from time import strftime
def stamp_to_localtime(timeStamp):
    timeArray = localtime(timeStamp)
    otherStyleTime = strftime("%Y/%m/%d %H:%M:%S", timeArray)
    return otherStyleTime